import 'jasmine';
import * as fs from 'node:fs';
import * as http from 'node:http';
import * as net from 'node:net';
import * as os from 'node:os';
import * as path from 'node:path';
import type { DebuggerServerInfo } from './server';
import { startDebuggerServer } from './server';

interface HttpResult {
  body: string;
  contentSecurityPolicy: string;
  contentType: string;
  statusCode: number;
  xFrameOptions: string;
}

interface HttpRequestOptions {
  method: string;
  headers: http.OutgoingHttpHeaders;
  body: string | undefined;
}

interface StreamingHttpRequest {
  request: http.ClientRequest;
  result: Promise<HttpResult>;
}

const GET_REQUEST_OPTIONS: HttpRequestOptions = {
  method: 'GET',
  headers: {},
  body: undefined,
};

async function getFreePort(): Promise<number> {
  return await new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once('error', reject);
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      if (typeof address !== 'object' || address === null) {
        server.close(() => reject(new Error('Could not allocate a TCP port.')));
        return;
      }
      server.close(error => (error ? reject(error) : resolve(address.port)));
    });
  });
}

async function listenOnPort(port: number): Promise<net.Server> {
  const server = net.createServer();
  return await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(port, '127.0.0.1', () => resolve(server));
  });
}

async function closeServer(server: { close: (callback: (error?: Error) => void) => void }): Promise<void> {
  await new Promise<void>((resolve, reject) => {
    server.close(error => (error ? reject(error) : resolve()));
  });
}

async function request(url: string, options: HttpRequestOptions): Promise<HttpResult> {
  return await new Promise((resolve, reject) => {
    const outgoingRequest = http.request(
      url,
      {
        method: options.method,
        headers: options.headers,
      },
      response => {
        let body = '';
        response.setEncoding('utf8');
        response.on('data', chunk => {
          body += chunk;
        });
        response.on('end', () => {
          resolve({
            body,
            contentSecurityPolicy: String(response.headers['content-security-policy'] ?? ''),
            contentType: String(response.headers['content-type'] ?? ''),
            statusCode: response.statusCode ?? 0,
            xFrameOptions: String(response.headers['x-frame-options'] ?? ''),
          });
        });
      },
    );
    outgoingRequest.once('error', reject);
    if (options.body !== undefined) {
      outgoingRequest.write(options.body);
    }
    outgoingRequest.end();
  });
}

function startStreamingRequest(
  url: string,
  method: string,
  headers: http.OutgoingHttpHeaders = {},
): StreamingHttpRequest {
  const outgoingRequest = http.request(url, { method, headers });
  const result = new Promise<HttpResult>((resolve, reject) => {
    outgoingRequest.once('response', response => {
      let body = '';
      response.setEncoding('utf8');
      response.on('data', chunk => {
        body += chunk;
      });
      response.on('end', () => {
        resolve({
          body,
          contentSecurityPolicy: String(response.headers['content-security-policy'] ?? ''),
          contentType: String(response.headers['content-type'] ?? ''),
          statusCode: response.statusCode ?? 0,
          xFrameOptions: String(response.headers['x-frame-options'] ?? ''),
        });
      });
    });
    outgoingRequest.once('error', reject);
  });
  return { request: outgoingRequest, result };
}

async function readSseEvents(
  url: string,
  eventName: string,
  count: number,
  onEvent: (payload: Record<string, unknown>, index: number) => void,
): Promise<Array<Record<string, unknown>>> {
  return await new Promise((resolve, reject) => {
    let settled = false;
    const payloads: Array<Record<string, unknown>> = [];
    const outgoingRequest = http.get(url, response => {
      let pending = '';
      response.setEncoding('utf8');
      response.on('data', chunk => {
        pending += chunk;
        let separatorIndex = pending.indexOf('\n\n');
        while (separatorIndex !== -1) {
          const eventBlock = pending.slice(0, separatorIndex);
          pending = pending.slice(separatorIndex + 2);
          const lines = eventBlock.split('\n');
          if (lines[0] !== `event: ${eventName}`) {
            separatorIndex = pending.indexOf('\n\n');
            continue;
          }
          const dataLine = lines.find(line => line.startsWith('data: '));
          if (dataLine && !settled) {
            const payload = JSON.parse(dataLine.slice(6)) as Record<string, unknown>;
            payloads.push(payload);
            onEvent(payload, payloads.length - 1);
            if (payloads.length === count) {
              settled = true;
              response.destroy();
              outgoingRequest.destroy();
              resolve(payloads);
              return;
            }
          }
          separatorIndex = pending.indexOf('\n\n');
        }
      });
    });
    outgoingRequest.once('error', error => {
      if (!settled) reject(error);
    });
  });
}

describe('debugger server', () => {
  let assetRoot: string;
  let debuggerServer: DebuggerServerInfo | undefined;
  let occupiedPortServer: net.Server | undefined;
  let originalHome: string | undefined;

  beforeEach(() => {
    originalHome = process.env['HOME'];
    assetRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-debugger-assets-'));
    fs.writeFileSync(path.join(assetRoot, 'index.html'), '<!doctype html><title>Valdi Debugger</title>', 'utf8');
  });

  afterEach(async () => {
    if (debuggerServer) {
      await debuggerServer.close();
      debuggerServer = undefined;
    }
    if (occupiedPortServer) {
      await closeServer(occupiedPortServer);
      occupiedPortServer = undefined;
    }
    if (originalHome === undefined) {
      delete process.env['HOME'];
    } else {
      process.env['HOME'] = originalHome;
    }
    fs.rmSync(assetRoot, { recursive: true, force: true });
  });

  it('serves the packaged debugger application', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(debuggerServer.url, GET_REQUEST_OPTIONS);

    expect(result.statusCode).toBe(200);
    expect(result.contentType).toBe('text/html; charset=utf-8');
    expect(result.body).toContain('Valdi Debugger');
    expect(result.contentSecurityPolicy).toContain("script-src 'self'");
    expect(result.contentSecurityPolicy).toContain("frame-ancestors 'none'");
    expect(result.xFrameOptions).toBe('DENY');
  });

  it('closes cleanly while an event stream is open', async () => {
    const serverToClose = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    debuggerServer = serverToClose;

    const response = await new Promise<http.IncomingMessage>((resolve, reject) => {
      const outgoingRequest = http.get(new URL('/api/debugger/events', serverToClose.url).toString(), resolve);
      outgoingRequest.once('error', reject);
    });
    response.resume();
    const responseEnded = new Promise<void>(resolve => response.once('end', resolve));
    debuggerServer = undefined;

    await serverToClose.close();
    await responseEnded;
  });

  it('selects the next port when the preferred port is occupied', async () => {
    const preferredPort = await getFreePort();
    occupiedPortServer = await listenOnPort(preferredPort);

    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: preferredPort,
      strictPort: false,
    });

    expect(debuggerServer.requestedPort).toBe(preferredPort);
    expect(debuggerServer.port).toBe(preferredPort + 1);
    expect(debuggerServer.portWasAutoSelected).toBeTrue();
  });

  it('fails when strict port selection encounters an occupied port', async () => {
    const preferredPort = await getFreePort();
    occupiedPortServer = await listenOnPort(preferredPort);

    await expectAsync(
      startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: preferredPort,
        strictPort: true,
      }),
    ).toBeRejectedWith(jasmine.objectContaining({ code: 'EADDRINUSE' }));
  });

  it('rejects non-loopback bind addresses', async () => {
    await expectAsync(
      startDebuggerServer({
        assetRoot,
        host: '0.0.0.0',
        port: await getFreePort(),
        strictPort: true,
      }),
    ).toBeRejectedWithError(/only binds to loopback hosts/);
  });

  it('rejects invalid debugger ports', async () => {
    await expectAsync(
      startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: 65_536,
        strictPort: true,
      }),
    ).toBeRejectedWithError(/between 1 and 65535/);
  });

  it('rejects non-loopback Host headers', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(debuggerServer.url, {
      method: 'GET',
      headers: { Host: `attacker.example:${debuggerServer.port}` },
      body: undefined,
    });

    expect(result.statusCode).toBe(403);
  });

  it('rejects requests from a non-loopback browser origin', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(new URL('/api/debugger/actions', debuggerServer.url).toString(), {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Origin: 'https://attacker.example',
      },
      body: JSON.stringify({ action: 'refreshSnapshot', params: {} }),
    });

    expect(result.statusCode).toBe(403);
  });

  it('rejects requests from a different loopback origin', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(new URL('/api/debugger/actions', debuggerServer.url).toString(), {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Origin: `http://127.0.0.2:${debuggerServer.port}`,
      },
      body: JSON.stringify({ action: 'refreshSnapshot', params: {} }),
    });

    expect(result.statusCode).toBe(403);
  });

  it('rejects cross-site API subresource requests without an Origin header', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(new URL('/api/status', debuggerServer.url).toString(), {
      method: 'GET',
      headers: { 'Sec-Fetch-Site': 'cross-site' },
      body: undefined,
    });

    expect(result.statusCode).toBe(403);
  });

  it('accepts same-origin browser actions', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(new URL('/api/debugger/actions', debuggerServer.url).toString(), {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Origin: new URL(debuggerServer.url).origin,
      },
      body: JSON.stringify({ action: 'setAutoRefresh', params: { enabled: true } }),
    });

    expect(result.statusCode).toBe(200);
    const responseBody = JSON.parse(result.body) as { state: { autoRefresh: boolean } };
    expect(responseBody.state.autoRefresh).toBeTrue();
  });

  it('rejects unknown debugger actions and invalid ports', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const actionsUrl = new URL('/api/debugger/actions', debuggerServer.url).toString();

    const unknown = await request(actionsUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'typoAction', params: {} }),
    });
    const invalidPort = await request(actionsUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'setPort', params: { port: 70_000 } }),
    });

    expect(unknown.statusCode).toBe(400);
    expect((JSON.parse(unknown.body) as { error: string }).error).toContain('Unknown debugger action');
    expect(invalidPort.statusCode).toBe(400);
    expect((JSON.parse(invalidPort.body) as { error: string }).error).toContain('between 1 and 65535');
  });

  it('decodes JSON only after joining split UTF-8 request bytes', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const body = Buffer.from(JSON.stringify({ action: 'refreshSnapshot', params: {}, source: 'café' }), 'utf8');
    const encodedCharacter = Buffer.from('é', 'utf8');
    const characterOffset = body.indexOf(encodedCharacter);
    expect(characterOffset).toBeGreaterThan(0);
    const streaming = startStreamingRequest(new URL('/api/debugger/actions', debuggerServer.url).toString(), 'POST', {
      'Content-Type': 'application/json',
    });

    streaming.request.write(body.subarray(0, characterOffset + 1));
    await new Promise<void>(resolve => setImmediate(resolve));
    streaming.request.end(body.subarray(characterOffset + 1));
    const result = await streaming.result;

    expect(result.statusCode).toBe(200);
    expect((JSON.parse(result.body) as { source: string }).source).toBe('café');
  });

  it('serializes concurrent CPU profile transitions before reading their bodies', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const profileUrl = new URL('/api/performance/profile/start', debuggerServer.url).toString();
    const first = startStreamingRequest(profileUrl, 'POST', { 'Content-Type': 'application/json' });
    first.request.write('{');
    await new Promise<void>(resolve => setTimeout(resolve, 20));

    const second = await request(profileUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{}',
    });
    first.request.end('invalid');
    await first.result;

    expect(second.statusCode).toBe(500);
    expect((JSON.parse(second.body) as { error: string }).error).toContain('transition is already in progress');
  });

  it('does not serve paths outside the debugger asset root', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(new URL('/%2e%2e%2foutside.txt', debuggerServer.url).toString(), GET_REQUEST_OPTIONS);

    expect(result.statusCode).toBe(403);
    expect(result.body).toBe('Forbidden');
  });

  it('returns JSON for unknown API routes', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(new URL('/api/unknown', debuggerServer.url).toString(), GET_REQUEST_OPTIONS);

    expect(result.statusCode).toBe(404);
    expect(result.contentType).toBe('application/json; charset=utf-8');
    expect(JSON.parse(result.body)).toEqual({ error: 'Unknown API route /api/unknown' });
  });

  it('selects the exact runtime log for an application and platform', async () => {
    const logsDirectory = path.join(assetRoot, 'logs');
    fs.mkdirSync(logsDirectory);
    fs.writeFileSync(
      path.join(logsDirectory, 'ios-example.log'),
      '2026-08-24 01:02:03 +0000 [info] [JS] exact application\n',
      'utf8',
    );
    fs.writeFileSync(
      path.join(logsDirectory, 'ios-example-preview.log'),
      '2026-08-24 01:02:04 +0000 [info] [JS] newer prefix collision\n',
      'utf8',
    );
    const newerTime = new Date(Date.now() + 10_000);
    fs.utimesSync(path.join(logsDirectory, 'ios-example-preview.log'), newerTime, newerTime);
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      logsDirectory,
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(
      new URL('/api/runtime-logs?applicationId=example&platform=ios', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    const responseBody = JSON.parse(result.body) as {
      logFile: string;
      logs: Array<{ message: string }>;
    };

    expect(result.statusCode).toBe(200);
    expect(responseBody.logFile).toBe('ios-example.log');
    expect(responseBody.logs.map(log => log.message)).toEqual(['exact application']);
  });

  it('reads the logs directory through the shared YAML config parser', async () => {
    const testHome = path.join(assetRoot, 'home');
    const logsDirectory = path.join(testHome, 'logs#debug');
    fs.mkdirSync(path.join(testHome, '.valdi'), { recursive: true });
    fs.mkdirSync(logsDirectory);
    fs.writeFileSync(
      path.join(testHome, '.valdi', 'config.yaml'),
      'logs_output_dir: "~/logs#debug" # inline comments are valid YAML\n',
      'utf8',
    );
    fs.writeFileSync(
      path.join(logsDirectory, 'ios-configured.log'),
      '2026-08-24 01:02:03 +0000 [info] [JS] configured directory\n',
      'utf8',
    );
    process.env['HOME'] = testHome;
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(
      new URL('/api/runtime-logs?applicationId=configured&platform=ios', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    const responseBody = JSON.parse(result.body) as { logFile: string; logs: Array<{ message: string }> };

    expect(result.statusCode).toBe(200);
    expect(responseBody.logFile).toBe('ios-configured.log');
    expect(responseBody.logs.map(log => log.message)).toEqual(['configured directory']);
  });

  it('preserves repeated runtime log entries as the stream window advances', async () => {
    const logsDirectory = path.join(assetRoot, 'logs');
    fs.mkdirSync(logsDirectory);
    const repeatedLine = '2026-08-24 01:02:03 +0000 [info] [JS] repeated\n';
    const logPath = path.join(logsDirectory, 'ios-example.log');
    fs.writeFileSync(logPath, repeatedLine.repeat(2), 'utf8');
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      logsDirectory,
      port: await getFreePort(),
      strictPort: true,
    });

    const payloads = await readSseEvents(
      new URL('/api/runtime-logs/stream?applicationId=example&platform=ios&limit=2', debuggerServer.url).toString(),
      'logs',
      2,
      (_payload, index) => {
        if (index === 0) fs.appendFileSync(logPath, repeatedLine, 'utf8');
      },
    );
    const initialLogs = payloads[0]?.['logs'] as Array<{ message: string }>;
    const appendedLogs = payloads[1]?.['logs'] as Array<{ message: string }>;

    expect(initialLogs.map(log => log.message)).toEqual(['repeated', 'repeated']);
    expect(appendedLogs.map(log => log.message)).toEqual(['repeated']);
  });
});
