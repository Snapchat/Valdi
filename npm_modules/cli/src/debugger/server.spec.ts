import 'jasmine';
import * as fs from 'node:fs';
import * as http from 'node:http';
import * as net from 'node:net';
import * as os from 'node:os';
import * as path from 'node:path';
import { valdiDebugger } from '../commands/debugger';
import { ArgumentsResolver } from '../utils/ArgumentsResolver';
import type { DebuggerServerInfo } from './server';
import {
  MAX_TRACE_EVENT_COUNT,
  MAX_TRACE_HTTP_RESPONSE_BYTES,
  PerformanceTraceAction,
  assertPerformanceTraceContext,
  buildPerfettoTracePayload,
  decorateTraceResult,
  normalizeTraceCaptureDurationMs,
  projectDebuggerTreeForJson,
  readRecordedTraces,
  runPerformanceTraceCapture,
  startDebuggerServer,
  summarizeTraces,
} from './server';

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
const WEB_PREVIEW_NONCE = 'server-devtools-nonce-123456';

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

  it('projects deep cyclic daemon and Owl hierarchies before the HTTP JSON boundary', () => {
    const root: Record<string, unknown> = { id: 0, tag: 'root' };
    let current = root;
    for (let index = 1; index <= 20_000; index += 1) {
      const child: Record<string, unknown> = { id: index, tag: 'node' };
      current['children'] = [child];
      current = child;
    }
    const cyclicMetadata: Record<string, unknown> = { title: 'cyclic' };
    cyclicMetadata['self'] = cyclicMetadata;
    current['metadata'] = cyclicMetadata;
    current['children'] = [root];

    const projection = projectDebuggerTreeForJson(root);

    expect(projection.nodeCount).toBe(20_001);
    expect(projection.complete).toBeTrue();
    expect(() => JSON.stringify({ tree: projection })).not.toThrow();
  });

  it('assigns shared hierarchy ownership to the first node reached in preorder and caps output', () => {
    const shared = { id: 'shared', tag: 'shared' };
    const first = { children: [shared], id: 'first', tag: 'first' };
    const root: Record<string, unknown> = { children: [first, shared], id: 'root', tag: 'root' };

    const projection = projectDebuggerTreeForJson(root);

    expect(projection.nodes.map(node => node.data['id'])).toEqual(['root', 'first', 'shared']);
    expect(projection.nodes[0]?.childIndexes).toEqual([1]);
    expect(projection.nodes[1]?.childIndexes).toEqual([2]);
    expect(projection.nodes[2]).toEqual(jasmine.objectContaining({ depth: 2, parentIndex: 1 }));

    root['children'] = Array.from({ length: 26_000 }, (_, index) => ({ id: index, tag: 'node' }));
    const capped = projectDebuggerTreeForJson(root);
    expect(capped.nodeCount).toBe(25_000);
    expect(capped.complete).toBeFalse();
    expect(() => JSON.stringify({ tree: capped })).not.toThrow();
  });

  it('bounds sparse metadata and child arrays without invoking accessors at the HTTP boundary', () => {
    const sparseMetadata: unknown[] = [];
    sparseMetadata[10_000_000] = 'far-value';
    let getterCalls = 0;
    Object.defineProperty(sparseMetadata, 'accessor', {
      enumerable: true,
      get: () => {
        getterCalls += 1;
        return 'unsafe';
      },
    });
    const sparseChildren: Array<Record<string, unknown>> = [];
    sparseChildren[10_000_000] = { id: 'distant', tag: 'child' };
    const root: Record<string, unknown> = {
      children: sparseChildren,
      id: 'root',
      metadata: sparseMetadata,
      oversized: 'x'.repeat(60_000),
      tag: 'root',
    };

    const startedAt = performance.now();
    const projection = projectDebuggerTreeForJson(root);
    const elapsedMilliseconds = performance.now() - startedAt;
    const metadata = projection.nodes[0]?.data['metadata'] as {
      $entries: Array<Record<string, unknown>>;
      $length: number;
      $truncated: string;
    };
    const serializedMetadata = JSON.stringify(metadata);

    expect(projection.complete).toBeFalse();
    expect(projection.nodeCount).toBe(2);
    expect(projection.nodes[1]?.sourceChildIndex).toBe(10_000_000);
    expect(metadata.$length).toBe(10_000_001);
    expect(metadata.$truncated).toBe('sparse-array');
    expect((projection.nodes[0]?.data['oversized'] as string).length).toBe(50_000);
    expect(metadata.$entries).toContain(jasmine.objectContaining({ $index: 10_000_000, value: 'far-value' }));
    expect(metadata.$entries).toContain(
      jasmine.objectContaining({
        $key: 'accessor',
        value: jasmine.objectContaining({ $truncated: 'accessor' }),
      }),
    );
    expect(getterCalls).toBe(0);
    expect(serializedMetadata.length).toBeLessThan(1500);
    expect(elapsedMilliseconds).toBeLessThan(1000);
  });

  it('preserves own prototype-named keys in projected values and tree-node data', () => {
    const metadata = JSON.parse('{"__proto__":{"metadataPolluted":true},"safe":1}') as Record<string, unknown>;
    Object.setPrototypeOf(metadata, { inheritedSentinel: 'must-not-project' });
    const root = JSON.parse('{"id":"prototype-node","tag":"view","__proto__":{"treePolluted":true}}') as Record<
      string,
      unknown
    >;
    Object.setPrototypeOf(root, { inheritedSentinel: 'must-not-project' });
    root['metadata'] = metadata;

    const projection = projectDebuggerTreeForJson(root);
    const data = projection.nodes[0]?.data;
    const projectedMetadata = data?.['metadata'] as Record<string, unknown>;
    const serialized = JSON.parse(JSON.stringify(projection)) as {
      nodes: Array<{ data: Record<string, unknown> }>;
    };

    expect(projection.complete).toBeTrue();
    expect(Object.getPrototypeOf(data)).toBeNull();
    expect(Object.getPrototypeOf(projectedMetadata)).toBeNull();
    expect(Object.prototype.hasOwnProperty.call(data, '__proto__')).toBeTrue();
    expect(Object.prototype.hasOwnProperty.call(projectedMetadata, '__proto__')).toBeTrue();
    expect(data?.['inheritedSentinel']).toBeUndefined();
    expect(projectedMetadata['inheritedSentinel']).toBeUndefined();
    expect(serialized.nodes[0]?.data['__proto__']).toEqual({ treePolluted: true });
    expect((serialized.nodes[0]?.data['metadata'] as Record<string, unknown>)['__proto__']).toEqual({
      metadataPolluted: true,
    });
  });

  it('marks a revoked proxy child incomplete without throwing or invoking its traps', () => {
    const revokedChild = Proxy.revocable({ id: 'revoked', tag: 'view' }, {});
    revokedChild.revoke();
    const root: Record<string, unknown> = {
      children: [revokedChild.proxy],
      id: 'root',
      tag: 'view',
    };

    const projection = projectDebuggerTreeForJson(root);

    expect(projection.complete).toBeFalse();
    expect(projection.nodeCount).toBe(1);
    expect(projection.truncations).toContain(
      jasmine.objectContaining({
        $at: '$.nodes[0].children[0]',
        $truncated: 'unavailable-child',
      }),
    );
    expect(() => JSON.stringify(projection)).not.toThrow();
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

  it('allows extension framing only for the dedicated DevTools panel route', async () => {
    fs.writeFileSync(path.join(assetRoot, 'devtools-panel.html'), '<!doctype html><title>Valdi DevTools</title>');
    fs.writeFileSync(path.join(assetRoot, 'devtools-panel.js'), 'void 0;');
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const panel = await request(new URL('/devtools-panel.html', debuggerServer.url).toString(), GET_REQUEST_OPTIONS);
    const standalone = await request(debuggerServer.url, GET_REQUEST_OPTIONS);
    const panelScript = await request(
      new URL('/devtools-panel.js', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );

    expect(panel.statusCode).toBe(200);
    expect(panel.contentSecurityPolicy).toContain('frame-ancestors chrome-extension://*');
    expect(panel.contentSecurityPolicy).not.toContain("frame-ancestors 'none'");
    expect(panel.xFrameOptions).toBe('');
    expect(standalone.contentSecurityPolicy).toContain("frame-ancestors 'none'");
    expect(standalone.xFrameOptions).toBe('DENY');
    expect(panelScript.contentSecurityPolicy).toContain("frame-ancestors 'none'");
    expect(panelScript.xFrameOptions).toBe('DENY');
  });

  it('resolves only the exact configured web preview page for integrated DevTools', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      webPreviewUrl: 'http://127.0.0.1:54321/index.html?tenant=alpha&mode=dev',
      chromiumDebuggingPort: 9333,
    });

    const matching = await request(
      new URL(
        `/api/devtools/target?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3FvaldiDebugger%3D1%26mode%3Ddev%26valdiDevTools%3D1%26tenant%3Dalpha&targetNonce=${WEB_PREVIEW_NONCE}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    const differentPath = await request(
      new URL(
        `/api/devtools/target?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Fother.html&targetNonce=${WEB_PREVIEW_NONCE}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    const differentQuery = await request(
      new URL(
        `/api/devtools/target?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3Fmode%3Ddev%26tenant%3Dbeta%26valdiDebugger%3D1%26valdiDevTools%3D1&targetNonce=${WEB_PREVIEW_NONCE}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );

    expect(matching.statusCode).toBe(200);
    expect(JSON.parse(matching.body)).toEqual({
      target: jasmine.objectContaining({
        applicationUrl: 'http://127.0.0.1:54321/index.html?tenant=alpha&mode=dev',
        debuggingPort: 9333,
        id: 'owl:web-preview',
        sessionId: 'web-preview',
      }),
    });
    expect(differentPath.statusCode).toBe(404);
    expect(JSON.parse(differentPath.body)).toEqual({
      error: 'The inspected page does not match the configured Valdi web preview target.',
    });
    expect(differentQuery.statusCode).toBe(404);
    expect(JSON.parse(differentQuery.body)).toEqual({
      error: 'The inspected page does not match the configured Valdi web preview target.',
    });
  });

  it('requires an inspected-tab nonce when resolving the integrated DevTools target', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      webPreviewUrl: 'http://127.0.0.1:54321/index.html',
    });

    const result = await request(
      new URL(
        '/api/devtools/target?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html',
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );

    expect(result.statusCode).toBe(400);
    expect(JSON.parse(result.body)).toEqual({
      error: 'DevTools target discovery requires a valid inspected-tab nonce.',
    });
  });

  it('requires JSON for executable integrated DevTools routes', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      webPreviewUrl: 'http://127.0.0.1:54321/index.html',
    });

    const plainText = await request(new URL('/api/devtools/evaluate', debuggerServer.url).toString(), {
      method: 'POST',
      headers: { 'Content-Type': 'text/plain' },
      body: '{"sessionId":"web-preview","expression":"1 + 1"}',
    });
    const json = await request(new URL('/api/devtools/evaluate', debuggerServer.url).toString(), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{"sessionId":"missing","expression":"1 + 1"}',
    });

    expect(plainText.statusCode).toBe(415);
    expect(JSON.parse(plainText.body)).toEqual({
      error: 'Valdi DevTools actions require an application/json request.',
    });
    expect(json.statusCode).toBe(404);
    expect(JSON.parse(json.body)).toEqual({
      error: 'The configured web preview debugger target is not available.',
    });
  });

  it('does not accept mutations on read-only integrated DevTools routes', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      webPreviewUrl: 'http://127.0.0.1:54321/index.html',
    });

    const result = await request(new URL('/api/devtools/target', debuggerServer.url).toString(), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{}',
    });

    expect(result.statusCode).toBe(405);
    expect(JSON.parse(result.body)).toEqual({ error: 'Valdi DevTools target discovery requires GET.' });
  });

  it('rejects remote web previews and invalid Chromium debugging ports', async () => {
    await expectAsync(
      startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: 'https://example.com/index.html',
      }),
    ).toBeRejectedWithError(/unauthenticated loopback HTTP URL/);
    await expectAsync(
      startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: 'http://127.0.0.1:54321/index.html',
        chromiumDebuggingPort: 0,
      }),
    ).toBeRejectedWithError(/Chromium debugging port must be an integer between 1 and 65535/);
  });

  it('restores the active web preview target when a configured server cannot bind', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const occupiedPort = await getFreePort();
    occupiedPortServer = await listenOnPort(occupiedPort);

    await expectAsync(
      startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: occupiedPort,
        strictPort: true,
        webPreviewUrl: 'http://127.0.0.1:54321/index.html',
      }),
    ).toBeRejected();

    const result = await request(
      new URL(
        '/api/devtools/target?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html',
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    expect(result.statusCode).toBe(404);
    expect(JSON.parse(result.body)).toEqual({
      error: 'Start valdi debugger with --web-preview-url before opening the DevTools panel.',
    });
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

  it('rejects non-integer input element identifiers before connecting to a daemon', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const inputUrl = new URL('/api/input', debuggerServer.url).toString();
    const invalidBodies = [
      JSON.stringify({ type: 'tap', elementId: '12' }),
      JSON.stringify({ type: 'tap', elementId: '12px' }),
      JSON.stringify({ type: 'tap', elementId: 12.5 }),
      JSON.stringify({ type: 'tap', elementId: null }),
      '{"type":"tap","elementId":1e400}',
    ];

    for (const body of invalidBodies) {
      const result = await request(inputUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body,
      });

      expect(result.statusCode).toBe(400);
      expect((JSON.parse(result.body) as { error: string }).error).toBe('elementId must be a finite integer.');
    }
  });

  it('rejects action-specific input fields and malformed ports before connecting to a daemon', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const baseUrl = new URL('/api/input', debuggerServer.url);
    const cases = [
      {
        url: baseUrl.toString(),
        body: { type: 'tap', elementId: 1, text: 'unexpected' },
        error: "Field 'text' is not supported for tap input.",
      },
      {
        url: baseUrl.toString(),
        body: { type: 'text', elementId: 1, text: 'a', value: 'b' },
        error: 'Use only one of text or value.',
      },
      {
        url: baseUrl.toString(),
        body: { type: 'key', elementId: 1, key: 'abc' },
        error: 'key must be Enter, Return, Escape, Backspace, Delete, or one printable grapheme.',
      },
      {
        url: new URL('/api/input?port=13591oops', debuggerServer.url).toString(),
        body: { type: 'capabilities' },
        error: 'Input port must be an integer between 1 and 65535.',
      },
    ];

    for (const inputCase of cases) {
      const result = await request(inputCase.url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(inputCase.body),
      });
      expect(result.statusCode).toBe(400);
      expect((JSON.parse(result.body) as { error: string }).error).toBe(inputCase.error);
    }
  });

  it('returns explicit client errors for unsupported input methods and malformed bodies', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const inputUrl = new URL('/api/input', debuggerServer.url).toString();

    const unsupportedMethod = await request(inputUrl, GET_REQUEST_OPTIONS);
    expect(unsupportedMethod.statusCode).toBe(405);
    expect((JSON.parse(unsupportedMethod.body) as { error: string }).error).toBe('Input dispatch requires POST.');

    const malformedJson = await request(inputUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{',
    });
    expect(malformedJson.statusCode).toBe(400);
    expect((JSON.parse(malformedJson.body) as { error: string }).error).toBe('Request body must contain valid JSON.');

    for (const body of ['null', '[]', '"input"']) {
      const malformedBody = await request(inputUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body,
      });
      expect(malformedBody.statusCode).toBe(400);
      expect((JSON.parse(malformedBody.body) as { error: string }).error).toBe('Request body must be a JSON object.');
    }

    const oversizedBody = await request(inputUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ padding: 'a'.repeat(1024 * 1024) }),
    });
    expect(oversizedBody.statusCode).toBe(400);
    expect((JSON.parse(oversizedBody.body) as { error: string }).error).toBe('Request body is too large.');
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

  it('rejects malformed UTF-8 split across executable JSON request chunks', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const streaming = startStreamingRequest(new URL('/api/debugger/actions', debuggerServer.url).toString(), 'POST', {
      'Content-Type': 'application/json',
    });
    streaming.request.write(Buffer.from('{"action":"refreshSnapshot","source":"', 'utf8'));
    streaming.request.write(Buffer.from([0xc3]));
    await new Promise<void>(resolve => setImmediate(resolve));
    streaming.request.end(Buffer.concat([Buffer.from([0x28]), Buffer.from('"}', 'utf8')]));
    const result = await streaming.result;

    expect(result.statusCode).toBe(400);
    expect(JSON.parse(result.body)).toEqual({ error: 'Request body must contain valid UTF-8.' });
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

  it('serializes concurrent renderer trace transitions before reading their bodies', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const traceUrl = new URL('/api/performance/trace/start', debuggerServer.url).toString();
    const first = startStreamingRequest(traceUrl, 'POST', { 'Content-Type': 'application/json' });
    first.request.write('{');
    await new Promise<void>(resolve => setTimeout(resolve, 20));

    const second = await request(traceUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{}',
    });
    first.request.end('invalid');
    await first.result;

    expect(second.statusCode).toBe(500);
    expect((JSON.parse(second.body) as { error: string }).error).toContain(
      'renderer trace transition is already in progress',
    );
  });

  it('enforces HTTP methods for renderer trace routes before contacting a target', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const status = await request(new URL('/api/performance/trace/status', debuggerServer.url).toString(), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{}',
    });
    const start = await request(
      new URL('/api/performance/trace/start', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );

    expect(status.statusCode).toBe(500);
    expect((JSON.parse(status.body) as { error: string }).error).toBe('Performance trace status requires GET.');
    expect(start.statusCode).toBe(500);
    expect((JSON.parse(start.body) as { error: string }).error).toBe('Performance trace start requires POST.');
  });

  it('validates renderer trace options before contacting a target', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(new URL('/api/performance/trace/start', debuggerServer.url).toString(), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ rendererTracing: 'true' }),
    });

    expect(result.statusCode).toBe(400);
    expect((JSON.parse(result.body) as { error: string }).error).toBe(
      'rendererTracing must be a boolean when provided.',
    );
  });

  it('converts bounded renderer traces into summaries and Perfetto events', () => {
    const traces = readRecordedTraces([
      { trace: 'Renderer.onRender.App', startMicros: 1000, endMicros: 4000, threadId: 7 },
      {
        trace: 'Renderer.viewModelChange.App.title',
        startMicros: 4000,
        endMicros: 4000,
        threadId: 7,
      },
    ]);
    const summary = summarizeTraces(traces);
    const perfetto = buildPerfettoTracePayload(traces, { name: 'Example', contextId: 'root' }, 0);
    const renderEvent = perfetto.traceEvents.find(event => event.name === 'Renderer.onRender.App');
    const triggerEvent = perfetto.traceEvents.find(event => event.name === 'Renderer.viewModelChange.App.title');

    expect(summary.traceCount).toBe(2);
    expect(summary.captureScope).toBe('process-wide');
    expect(summary.durationTraceCount).toBe(1);
    expect(summary.instantTraceCount).toBe(1);
    expect(summary.topComponents).toEqual([{ name: 'App', count: 1, durationMs: 3 }]);
    expect(summary.topViewModelTriggers).toEqual([{ name: 'App.title', count: 1 }]);
    expect(renderEvent).toEqual(jasmine.objectContaining({ ph: 'X', ts: 0, dur: 3000, tid: 7 }));
    expect(renderEvent?.args).toBeUndefined();
    expect(triggerEvent).toEqual(jasmine.objectContaining({ ph: 'i', ts: 3000, s: 't', tid: 7 }));
    expect(perfetto.metadata).toEqual({
      captureScope: 'process-wide',
      captureTargetContextId: 'root',
      captureTargetName: 'Example',
      droppedTraceEventCount: 0,
    });

    const fabricatedInstant = buildPerfettoTracePayload(
      readRecordedTraces([{ trace: 'Unrelated.trace', startMicros: 1, endMicros: 2, threadId: 1, type: 1 }]),
      {},
      0,
    ).traceEvents.find(event => event.name === 'Unrelated.trace');
    expect(fabricatedInstant).toEqual(jasmine.objectContaining({ ph: 'X', dur: 1 }));
  });

  it('drops malformed renderer trace events and caps conversion work', () => {
    const malformed = readRecordedTraces([
      { trace: 'valid', startMicros: 1, endMicros: 2, threadId: 1 },
      { trace: '', startMicros: 1, endMicros: 2, threadId: 1 },
      { trace: 'negative', startMicros: -1, endMicros: 2, threadId: 1 },
      { trace: 'backwards', startMicros: 2, endMicros: 1, threadId: 1 },
      { trace: 'unsafe', startMicros: 1, endMicros: 2, threadId: Number.MAX_SAFE_INTEGER + 1 },
      { trace: 'x'.repeat(2049), startMicros: 1, endMicros: 2, threadId: 1 },
      { trace: 'é'.repeat(1025), startMicros: 1, endMicros: 2, threadId: 1 },
      null,
    ]);
    const manyEvents = Array.from({ length: MAX_TRACE_EVENT_COUNT + 1 }, (_, index) => ({
      trace: `event-${index}`,
      startMicros: index,
      endMicros: index,
      threadId: 1,
    }));

    expect(malformed).toEqual([{ trace: 'valid', startMicros: 1, endMicros: 2, threadId: 1 }]);
    expect(readRecordedTraces(manyEvents).length).toBe(MAX_TRACE_EVENT_COUNT);
    expect(readRecordedTraces([{ trace: 'é'.repeat(1024), startMicros: 1, endMicros: 2, threadId: 1 }]).length).toBe(1);
  });

  it('derives trace truncation from dropped counts rather than an exactly-full result', () => {
    const trace = { trace: 'valid', startMicros: 1, endMicros: 2, threadId: 1 };
    const exactlyFull = decorateTraceResult(
      {
        traces: Array.from({ length: MAX_TRACE_EVENT_COUNT }, () => trace),
        droppedTraceEventCount: 0,
      },
      {},
    );
    const truncated = decorateTraceResult({ traces: [trace], droppedTraceEventCount: 2 }, {});
    const truncatedPerfettoMetadata = truncated['perfettoMetadata'] as { droppedTraceEventCount: number };

    expect(exactlyFull['droppedTraceEventCount']).toBe(0);
    expect(exactlyFull['traceEventLimitReached']).toBeFalse();
    expect(truncated['droppedTraceEventCount']).toBe(2);
    expect(truncated['traceEventLimitReached']).toBeTrue();
    expect(truncatedPerfettoMetadata.droppedTraceEventCount).toBe(2);
  });

  it('bounds the complete HTTP trace result without duplicating Perfetto events', () => {
    const escapedTraceName = '\0'.repeat(2048);
    const result = decorateTraceResult(
      {
        recording: false,
        contextId: '\0'.repeat(100_000),
        completedRecordingAvailable: false,
        completionError: '\0'.repeat(100_000),
        rendererTracingEnabled: false,
        tracingSupported: true,
        traces: Array.from({ length: 512 }, (_, index) => ({
          trace: escapedTraceName,
          startMicros: index,
          endMicros: index + 1,
          threadId: 1,
        })),
        droppedTraceEventCount: 0,
        timedOut: false,
      },
      { contextId: '\0'.repeat(100_000), name: '\0'.repeat(100_000), port: 13_591 },
    );
    const serialized = JSON.stringify(result);

    expect(Buffer.byteLength(serialized, 'utf8')).toBeLessThanOrEqual(MAX_TRACE_HTTP_RESPONSE_BYTES);
    expect(result['perfetto']).toBeUndefined();
    expect(Array.isArray(result['traces'])).toBeTrue();
    expect(result['traceCount'] as number).toBeLessThan(512);
    expect(result['traceEventLimitReached']).toBeTrue();
    expect((result['perfettoMetadata'] as { droppedTraceEventCount: number }).droppedTraceEventCount).toBe(
      result['droppedTraceEventCount'] as number,
    );
  });

  it('saturates local trace drops at Number.MAX_SAFE_INTEGER', () => {
    const result = decorateTraceResult(
      {
        traces: [
          { trace: 'valid', startMicros: 1, endMicros: 2, threadId: 1 },
          { trace: '', startMicros: 1, endMicros: 2, threadId: 1 },
        ],
        droppedTraceEventCount: Number.MAX_SAFE_INTEGER,
      },
      {},
    );

    expect(result['droppedTraceEventCount']).toBe(Number.MAX_SAFE_INTEGER);
  });

  it('clamps renderer trace capture duration and rejects invalid values', () => {
    expect(normalizeTraceCaptureDurationMs({})).toBe(5000);
    expect(normalizeTraceCaptureDurationMs({ durationMs: -1 })).toBe(100);
    expect(normalizeTraceCaptureDurationMs({ durationMs: 40_000 })).toBe(15_000);
    expect(normalizeTraceCaptureDurationMs({ durationMs: 123.6 })).toBe(124);
    expect(() => normalizeTraceCaptureDurationMs({ durationMs: '5000' })).toThrowError(
      Error,
      'durationMs must be a finite number when provided.',
    );
  });

  it('rejects one-shot capture without stopping an existing manual trace', async () => {
    const actions: PerformanceTraceAction[] = [];
    let waitCalled = false;

    await expectAsync(
      runPerformanceTraceCapture(
        { durationMs: 100, rendererTracing: true },
        {
          send: action => {
            actions.push(action);
            return Promise.resolve({
              recording: true,
              contextId: 'root',
              completedRecordingAvailable: false,
              tracingSupported: true,
            });
          },
          wait: () => {
            waitCalled = true;
            return Promise.resolve();
          },
        },
      ),
    ).toBeRejectedWithError(
      Error,
      'A renderer trace recording is already active or waiting to be retrieved for context root. Stop it before one-shot capture.',
    );

    expect(actions).toEqual([PerformanceTraceAction.Status]);
    expect(waitCalled).toBeFalse();
  });

  it('best-effort stops a one-shot trace when waiting fails', async () => {
    const actions: PerformanceTraceAction[] = [];

    await expectAsync(
      runPerformanceTraceCapture(
        { durationMs: 100, rendererTracing: true },
        {
          send: action => {
            actions.push(action);
            if (action === PerformanceTraceAction.Status) {
              return Promise.resolve({
                recording: false,
                completedRecordingAvailable: false,
                tracingSupported: true,
              });
            }
            return Promise.resolve({ recording: action === PerformanceTraceAction.Start });
          },
          wait: () => Promise.reject(new Error('wait failed')),
        },
      ),
    ).toBeRejectedWithError(Error, 'wait failed');

    expect(actions).toEqual([PerformanceTraceAction.Status, PerformanceTraceAction.Start, PerformanceTraceAction.Stop]);
  });

  it('retries a failed stop so a handler-retained timed-out result can be recovered', async () => {
    const actions: PerformanceTraceAction[] = [];
    let stopCount = 0;

    const result = await runPerformanceTraceCapture(
      { durationMs: 100, rendererTracing: true },
      {
        send: action => {
          actions.push(action);
          if (action === PerformanceTraceAction.Status) {
            return Promise.resolve({
              recording: false,
              completedRecordingAvailable: false,
              tracingSupported: true,
            });
          }
          if (action === PerformanceTraceAction.Stop && ++stopCount === 1) {
            return Promise.reject(new Error('transient stop failure'));
          }
          return Promise.resolve({ recording: action === PerformanceTraceAction.Start, traces: [] });
        },
        wait: () => Promise.resolve(),
      },
    );

    expect(result['traces']).toEqual([]);
    expect(actions).toEqual([
      PerformanceTraceAction.Status,
      PerformanceTraceAction.Start,
      PerformanceTraceAction.Stop,
      PerformanceTraceAction.Stop,
    ]);
  });

  it('does not start one-shot capture when runtime tracing support is absent', async () => {
    const actions: PerformanceTraceAction[] = [];

    await expectAsync(
      runPerformanceTraceCapture(
        { durationMs: 100, rendererTracing: true },
        {
          send: action => {
            actions.push(action);
            return Promise.resolve({
              recording: false,
              completedRecordingAvailable: false,
              tracingSupported: false,
            });
          },
          wait: () => Promise.resolve(),
        },
      ),
    ).toBeRejectedWithError(Error, 'This Valdi runtime does not support renderer trace capture.');

    expect(actions).toEqual([PerformanceTraceAction.Status]);
  });

  it('rejects trace output that does not match the selected context', () => {
    expect(() =>
      assertPerformanceTraceContext(PerformanceTraceAction.Stop, { contextId: 'context-a' }, 'context-b'),
    ).toThrowError(Error, 'The Valdi runtime returned trace data for context context-a, expected context-b.');
    expect(() =>
      assertPerformanceTraceContext(PerformanceTraceAction.Status, { contextId: 'context-a' }, 'context-b'),
    ).not.toThrow();
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

  it('does not expose absolute log paths in filesystem error responses', async () => {
    const logsDirectory = path.join(assetRoot, 'missing', 'logs');
    const consoleWarn = spyOn(console, 'warn');
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      logsDirectory,
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await request(new URL('/api/runtime-logs', debuggerServer.url).toString(), GET_REQUEST_OPTIONS);
    const responseBody = JSON.parse(result.body) as { error: string };

    expect(result.statusCode).toBe(500);
    expect(responseBody.error).toBe('A local filesystem operation failed. See the debugger server output for details.');
    expect(result.body).not.toContain(logsDirectory);
    expect(consoleWarn).toHaveBeenCalledWith(jasmine.stringContaining(logsDirectory));
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

  it('rejects blank and invalid command preview URLs before allocating server or extension resources', async () => {
    const extensionDirectoryPrefix = 'valdi-devtools-extension-';
    const extensionDirectoriesBefore = fs
      .readdirSync(os.tmpdir())
      .filter(name => name.startsWith(extensionDirectoryPrefix))
      .sort();
    const invalidValues = [
      { error: /must not be blank/, value: '   \t  ' },
      { error: /Invalid web preview URL/, value: 'not a URL' },
    ];

    for (const invalidValue of invalidValues) {
      const port = await getFreePort();
      await expectAsync(
        valdiDebugger(
          new ArgumentsResolver({
            chromiumDebuggingPort: 9222,
            host: '127.0.0.1',
            json: true,
            port,
            strictPort: true,
            webPreviewUrl: invalidValue.value,
          }),
        ),
      ).toBeRejectedWithError(invalidValue.error);
      const availableServer = await listenOnPort(port);
      await closeServer(availableServer);
    }

    expect(
      fs
        .readdirSync(os.tmpdir())
        .filter(name => name.startsWith(extensionDirectoryPrefix))
        .sort(),
    ).toEqual(extensionDirectoriesBefore);
  });

  it('closes the debugger and removes its temporary extension on SIGHUP', async () => {
    const consoleLog = spyOn(console, 'log');
    const port = await getFreePort();
    const previousSignalListeners = new Set(process.listeners('SIGHUP'));
    const operation = valdiDebugger(
      new ArgumentsResolver({
        chromiumDebuggingPort: 9222,
        host: '127.0.0.1',
        json: true,
        port,
        strictPort: true,
        webPreviewUrl: 'http://127.0.0.1:54321/index.html',
      }),
    );
    while (consoleLog.calls.count() === 0) await new Promise<void>(resolve => setImmediate(resolve));
    const startup = JSON.parse(String(consoleLog.calls.mostRecent().args[0])) as { extensionDirectory: string };
    expect(fs.existsSync(startup.extensionDirectory)).toBeTrue();

    const shutdownListener = process.listeners('SIGHUP').find(listener => !previousSignalListeners.has(listener));
    if (!shutdownListener) throw new Error('Expected the debugger command to install a SIGHUP listener.');
    shutdownListener('SIGHUP');
    await operation;

    expect(fs.existsSync(startup.extensionDirectory)).toBeFalse();
    const availableServer = await listenOnPort(port);
    await closeServer(availableServer);
  });
});
