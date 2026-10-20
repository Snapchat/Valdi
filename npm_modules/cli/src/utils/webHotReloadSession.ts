import crypto from 'node:crypto';
import fs from 'node:fs';
import http from 'node:http';
import type { Socket } from 'node:net';
import os from 'node:os';
import path from 'node:path';

const SESSION_VERSION = 1;
const PUBLISH_PATH = '/publish';

export interface WebHotReloadSessionDescriptor {
  applicationTarget: string;
  bazelArgs: string;
  hotreloadTarget: string;
  id: string;
  pid: number;
  publishUrl: string;
  startedAt: string;
  token: string;
  version: typeof SESSION_VERSION;
  workspaceRoot: string;
}

export interface WebHotReloadSessionRegistration {
  descriptor: WebHotReloadSessionDescriptor;
  close(): Promise<void>;
}

interface RegisterWebHotReloadSessionOptions {
  applicationTarget: string;
  bazelArgs: string;
  hotreloadTarget: string;
  onPublish: () => Promise<void>;
  registryDir?: string | undefined;
  workspaceRoot: string;
}

function defaultRegistryDir(): string {
  const userId = typeof process.getuid === 'function' ? String(process.getuid()) : os.userInfo().username;
  return path.join(os.tmpdir(), `valdi-web-sessions-${userId}`);
}

function ensureRegistryDir(registryDir: string): void {
  fs.mkdirSync(registryDir, { mode: 0o700, recursive: true });
  const stat = fs.lstatSync(registryDir);
  if (!stat.isDirectory() || stat.isSymbolicLink()) {
    throw new Error(`Web hot reload registry is not a directory: ${registryDir}`);
  }
  if (typeof process.getuid === 'function' && stat.uid !== process.getuid()) {
    throw new Error(`Web hot reload registry is not owned by the current user: ${registryDir}`);
  }
  try {
    fs.chmodSync(registryDir, 0o700);
  } catch {
    // Some platforms do not support POSIX permissions.
  }
}

function sessionFilePath(registryDir: string, id: string): string {
  return path.join(registryDir, `${id}.json`);
}

function isProcessRunning(pid: number): boolean {
  try {
    process.kill(pid, 0);
    return true;
  } catch (error) {
    return (error as NodeJS.ErrnoException).code === 'EPERM';
  }
}

function isSessionDescriptor(value: unknown): value is WebHotReloadSessionDescriptor {
  if (typeof value !== 'object' || value === null) {
    return false;
  }
  const descriptor = value as Partial<WebHotReloadSessionDescriptor>;
  return (
    descriptor.version === SESSION_VERSION &&
    typeof descriptor.id === 'string' &&
    Number.isInteger(descriptor.pid) &&
    typeof descriptor.pid === 'number' &&
    descriptor.pid > 0 &&
    typeof descriptor.workspaceRoot === 'string' &&
    typeof descriptor.applicationTarget === 'string' &&
    typeof descriptor.hotreloadTarget === 'string' &&
    typeof descriptor.bazelArgs === 'string' &&
    typeof descriptor.publishUrl === 'string' &&
    typeof descriptor.token === 'string' &&
    typeof descriptor.startedAt === 'string'
  );
}

function closeServer(server: http.Server, sockets: Set<Socket>): Promise<void> {
  return new Promise((resolve, reject) => {
    server.close(error => {
      if (error) {
        reject(error);
      } else {
        resolve();
      }
    });
    for (const socket of sockets) {
      socket.destroy();
    }
  });
}

export function inferWebHotReloadTarget(applicationTarget: string): string | undefined {
  const separatorIndex = applicationTarget.lastIndexOf(':');
  const targetNameStart = separatorIndex === -1 ? applicationTarget.lastIndexOf('/') + 1 : separatorIndex + 1;
  const targetName = applicationTarget.slice(targetNameStart);
  if (!targetName.endsWith('_web')) {
    return undefined;
  }
  return `${applicationTarget.slice(0, targetNameStart)}${targetName.slice(0, -'_web'.length)}_hotreload`;
}

export async function registerWebHotReloadSession(
  options: RegisterWebHotReloadSessionOptions,
): Promise<WebHotReloadSessionRegistration> {
  const registryDir = options.registryDir ?? defaultRegistryDir();
  ensureRegistryDir(registryDir);

  const token = crypto.randomBytes(32).toString('hex');
  let publishQueue = Promise.resolve();
  const server = http.createServer((request, response) => {
    const requestUrl = new URL(request.url ?? '/', 'http://127.0.0.1');
    if (request.method !== 'POST' || requestUrl.pathname !== PUBLISH_PATH) {
      response.writeHead(404).end('not found');
      return;
    }
    if (request.headers.authorization !== `Bearer ${token}`) {
      response.writeHead(401).end('unauthorized');
      return;
    }

    publishQueue = publishQueue.then(options.onPublish, options.onPublish);
    void publishQueue.then(
      () => response.writeHead(204).end(),
      () => response.writeHead(500).end('publication failed'),
    );
  });
  const sockets = new Set<Socket>();
  server.on('connection', socket => {
    sockets.add(socket);
    socket.once('close', () => sockets.delete(socket));
  });

  const port = await new Promise<number>((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', () => {
      server.off('error', reject);
      const address = server.address();
      if (address === null || typeof address === 'string') {
        reject(new Error('Could not resolve the web hot reload control port'));
        return;
      }
      resolve(address.port);
    });
  });

  const descriptor: WebHotReloadSessionDescriptor = {
    applicationTarget: options.applicationTarget,
    bazelArgs: options.bazelArgs,
    hotreloadTarget: options.hotreloadTarget,
    id: crypto.randomUUID(),
    pid: process.pid,
    publishUrl: `http://127.0.0.1:${port}${PUBLISH_PATH}`,
    startedAt: new Date().toISOString(),
    token,
    version: SESSION_VERSION,
    workspaceRoot: fs.realpathSync(options.workspaceRoot),
  };
  const descriptorPath = sessionFilePath(registryDir, descriptor.id);
  const temporaryPath = `${descriptorPath}.${process.pid}.tmp`;
  try {
    fs.writeFileSync(temporaryPath, JSON.stringify(descriptor), { flag: 'wx', mode: 0o600 });
    fs.renameSync(temporaryPath, descriptorPath);
  } catch (error) {
    fs.rmSync(temporaryPath, { force: true });
    await closeServer(server, sockets);
    throw error;
  }

  return {
    descriptor,
    close: async () => {
      fs.rmSync(descriptorPath, { force: true });
      await publishQueue.catch(() => {});
      await closeServer(server, sockets);
    },
  };
}

export function findWebHotReloadSessions(
  workspaceRoot: string,
  hotreloadTarget?: string,
  registryDir: string = defaultRegistryDir(),
): WebHotReloadSessionDescriptor[] {
  if (!fs.existsSync(registryDir)) {
    return [];
  }

  const realWorkspaceRoot = fs.realpathSync(workspaceRoot);
  const sessions: WebHotReloadSessionDescriptor[] = [];
  for (const entry of fs.readdirSync(registryDir, { withFileTypes: true })) {
    if (!entry.isFile() || !entry.name.endsWith('.json')) {
      continue;
    }
    const descriptorPath = path.join(registryDir, entry.name);
    let descriptor: WebHotReloadSessionDescriptor;
    try {
      const parsed: unknown = JSON.parse(fs.readFileSync(descriptorPath, 'utf8'));
      if (!isSessionDescriptor(parsed)) {
        continue;
      }
      descriptor = parsed;
    } catch {
      continue;
    }

    if (!isProcessRunning(descriptor.pid)) {
      fs.rmSync(descriptorPath, { force: true });
      continue;
    }
    if (
      descriptor.workspaceRoot === realWorkspaceRoot &&
      (hotreloadTarget === undefined || descriptor.hotreloadTarget === hotreloadTarget)
    ) {
      sessions.push(descriptor);
    }
  }
  return sessions.sort((left, right) => left.startedAt.localeCompare(right.startedAt));
}

export function publishWebHotReloadSession(session: WebHotReloadSessionDescriptor): Promise<void> {
  const publishUrl = new URL(session.publishUrl);
  if (publishUrl.protocol !== 'http:' || publishUrl.hostname !== '127.0.0.1' || publishUrl.pathname !== PUBLISH_PATH) {
    return Promise.reject(new Error(`Refusing non-loopback web hot reload endpoint: ${session.publishUrl}`));
  }

  return new Promise((resolve, reject) => {
    const request = http.request(
      publishUrl,
      {
        headers: { authorization: `Bearer ${session.token}` },
        method: 'POST',
      },
      response => {
        response.resume();
        response.once('end', () => {
          if (response.statusCode === 204) {
            resolve();
          } else {
            reject(new Error(`Web hot reload publication failed with HTTP ${String(response.statusCode)}`));
          }
        });
      },
    );
    request.setTimeout(60_000, () => request.destroy(new Error('Web hot reload publication timed out')));
    request.once('error', reject);
    request.end();
  });
}
