import { spawn } from 'child_process';
import fs from 'fs';
import http from 'http';
import type { Socket } from 'net';
import path from 'path';

export interface StaticWebServerOptions {
  host: string;
  liveReload?: boolean;
  port: number;
}

export interface StaticWebServer {
  url: string;
  close: () => Promise<void>;
  reload: () => void;
  setRootDir: (rootDir: string) => void;
}

export const WEB_LIVE_RELOAD_PATH = '/__valdi_livereload';

const LIVE_RELOAD_SCRIPT = `<script>
(() => {
  const events = new EventSource('${WEB_LIVE_RELOAD_PATH}');
  events.addEventListener('reload', () => window.location.reload());
})();
</script>`;

function contentType(filePath: string): string {
  if (filePath.endsWith('.html')) {
    return 'text/html; charset=utf-8';
  }
  if (filePath.endsWith('.js')) {
    return 'text/javascript; charset=utf-8';
  }
  if (filePath.endsWith('.json')) {
    return 'application/json; charset=utf-8';
  }
  if (filePath.endsWith('.png')) {
    return 'image/png';
  }
  if (filePath.endsWith('.jpg') || filePath.endsWith('.jpeg')) {
    return 'image/jpeg';
  }
  if (filePath.endsWith('.svg')) {
    return 'image/svg+xml';
  }
  if (filePath.endsWith('.webp')) {
    return 'image/webp';
  }
  return 'application/octet-stream';
}

function sendText(response: http.ServerResponse, statusCode: number, body: string): void {
  response.writeHead(statusCode, { 'content-type': 'text/plain; charset=utf-8' });
  response.end(body);
}

function injectHtml(html: string, injectedHtml: string): string {
  const headEnd = html.lastIndexOf('</head>');
  if (headEnd !== -1) {
    return `${html.slice(0, headEnd)}${injectedHtml}\n${html.slice(headEnd)}`;
  }
  return `${injectedHtml}\n${html}`;
}

function serveFile(
  rootDir: string,
  request: http.IncomingMessage,
  response: http.ServerResponse,
  injectedHtml: string,
  liveReload: boolean,
): void {
  if (request.method !== 'GET' && request.method !== 'HEAD') {
    sendText(response, 405, 'method not allowed');
    return;
  }

  const requestUrl = new URL(request.url ?? '/', `http://${request.headers.host ?? 'localhost'}`);
  const decodedPath = decodeURIComponent(requestUrl.pathname === '/' ? '/index.html' : requestUrl.pathname);
  if (decodedPath.split('/').includes('..')) {
    sendText(response, 403, 'forbidden');
    return;
  }

  const root = path.resolve(rootDir);
  const relativePath = decodedPath.startsWith('/') ? decodedPath.slice(1) : decodedPath;
  let filePath = path.resolve(root, relativePath);
  if (!(filePath === root || filePath.startsWith(`${root}${path.sep}`))) {
    sendText(response, 403, 'forbidden');
    return;
  }

  if (fs.existsSync(filePath) && fs.statSync(filePath).isDirectory()) {
    filePath = path.join(filePath, 'index.html');
  }

  if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
    sendText(response, 404, 'not found');
    return;
  }

  response.writeHead(200, {
    'cache-control': liveReload || injectedHtml !== '' ? 'no-store' : 'no-cache',
    'content-type': contentType(filePath),
  });
  if (request.method === 'HEAD') {
    response.end();
    return;
  }
  if (injectedHtml !== '' && filePath.endsWith('.html')) {
    response.end(injectHtml(fs.readFileSync(filePath, 'utf8'), injectedHtml));
    return;
  }
  fs.createReadStream(filePath).pipe(response);
}

function serveLiveReload(
  request: http.IncomingMessage,
  response: http.ServerResponse,
  clients: Set<http.ServerResponse>,
): void {
  if (request.method !== 'GET') {
    sendText(response, 405, 'method not allowed');
    return;
  }
  response.writeHead(200, {
    'cache-control': 'no-cache, no-transform',
    connection: 'keep-alive',
    'content-type': 'text/event-stream',
  });
  response.write('retry: 1000\n\n');
  clients.add(response);
  request.once('close', () => {
    clients.delete(response);
  });
}

function listen(server: http.Server, host: string, port: number): Promise<number> {
  return new Promise((resolve, reject) => {
    const onError = (error: Error) => {
      reject(error);
    };
    server.once('error', onError);
    server.listen(port, host, () => {
      server.off('error', onError);
      const address = server.address();
      if (address === null || typeof address === 'string') {
        reject(new Error(`Could not resolve listening port for ${host}:${port}`));
        return;
      }
      resolve(address.port);
    });
  });
}

function trackConnections(server: http.Server): Set<Socket> {
  const sockets = new Set<Socket>();
  server.on('connection', socket => {
    sockets.add(socket);
    socket.on('close', () => {
      sockets.delete(socket);
    });
  });
  return sockets;
}

function close(server: http.Server, sockets: Set<Socket>): Promise<void> {
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

export async function startStaticWebServer(rootDir: string, options: StaticWebServerOptions): Promise<StaticWebServer> {
  let activeRootDir = path.resolve(rootDir);
  const liveReloadClients = new Set<http.ServerResponse>();
  const injectedHtml = options.liveReload ? LIVE_RELOAD_SCRIPT : '';
  const server = http.createServer((request, response) => {
    const requestUrl = new URL(request.url ?? '/', `http://${request.headers.host ?? 'localhost'}`);
    if (options.liveReload && requestUrl.pathname === WEB_LIVE_RELOAD_PATH) {
      serveLiveReload(request, response, liveReloadClients);
      return;
    }
    serveFile(activeRootDir, request, response, injectedHtml, options.liveReload ?? false);
  });
  const sockets = trackConnections(server);
  const actualPort = await listen(server, options.host, options.port);
  return {
    url: `http://${options.host}:${actualPort}/index.html`,
    close: async () => {
      for (const client of liveReloadClients) {
        client.end();
      }
      liveReloadClients.clear();
      await close(server, sockets);
    },
    reload: () => {
      for (const client of liveReloadClients) {
        client.write('event: reload\ndata: {}\n\n');
      }
    },
    setRootDir: nextRootDir => {
      activeRootDir = path.resolve(nextRootDir);
    },
  };
}

export function openUrlInDefaultBrowser(url: string): void {
  const platform = process.platform;
  const command = platform === 'darwin' ? 'open' : platform === 'win32' ? 'cmd' : 'xdg-open';
  const args = platform === 'win32' ? ['/c', 'start', '', url] : [url];
  const child = spawn(command, args, {
    detached: true,
    stdio: 'ignore',
  });
  child.on('error', error => {
    console.warn(`Could not open browser: ${error.message}`);
  });
  child.unref();
}
