import 'jasmine';
import crypto from 'node:crypto';
import * as fs from 'node:fs';
import * as http from 'node:http';
import * as net from 'node:net';
import * as os from 'node:os';
import * as path from 'node:path';
import type { CpuProfile } from '../utils/hermesClient';
import { valdiDebugger } from '../commands/debugger';
import { ArgumentsResolver } from '../utils/ArgumentsResolver';
import type { DebuggerServerInfo } from './server';
import {
  MAX_TRACE_EVENT_COUNT,
  MAX_TRACE_HTTP_RESPONSE_BYTES,
  PerformanceTraceAction,
  PerformanceTraceCoordinator,
  assertPerformanceTraceContext,
  buildPerfettoTracePayload,
  createInterruptibleOperation,
  decorateTraceResult,
  normalizeTraceCaptureDurationMs,
  projectDebuggerTreeForJson,
  readRecordedTraces,
  resolveDebuggerServicePort,
  resolveDebuggerUiPort,
  runPerformanceTraceCapture,
  startDebuggerServer,
  summarizeCpuProfile,
  summarizeTraces,
} from './server';

interface HttpResult {
  body: string;
  contentSecurityPolicy: string;
  contentType: string;
  statusCode: number;
  referrerPolicy: string;
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

interface MockDaemon {
  close: () => Promise<void>;
  port: number;
  requests: Array<Record<string, unknown>>;
  setApplicationId(applicationId: string): void;
  setContexts(contexts: Array<{ id: string; rootComponentName: string }>): void;
}

interface MockChromiumConsoleServer {
  close: () => Promise<void>;
  debuggerSockets: Set<net.Socket>;
  expressions: string[];
  methods: string[];
  port: number;
  runtimeEnableReceived: Promise<void>;
  releaseRuntimeEnable(): void;
  setInspectedUrl(inspectedUrl: string): void;
  setTargetNonce(targetNonce: string): void;
}

interface MockChromiumConsoleServerOptions {
  closeOnTracingEnd?: boolean;
  componentPropertyEditProtocolVersion?: number | null;
  componentPropertyValue?: string;
  dropTracingStartResponse?: boolean;
  holdRuntimeEnable: boolean;
  rejectIdentityAfterTracingStart?: boolean;
  rejectTracingStart?: boolean;
  componentPropertyEditingAvailable?: boolean;
  componentPropertyEditResult?: boolean;
}

function encodeChromiumServerMessage(payload: Record<string, unknown>): Buffer {
  const body = Buffer.from(JSON.stringify(payload), 'utf8');
  const header = body.length < 126 ? Buffer.alloc(2) : body.length <= 0xff_ff ? Buffer.alloc(4) : Buffer.alloc(10);
  header[0] = 0x81;
  if (body.length < 126) {
    header[1] = body.length;
  } else if (body.length <= 0xff_ff) {
    header[1] = 126;
    header.writeUInt16BE(body.length, 2);
  } else {
    header[1] = 127;
    header.writeBigUInt64BE(BigInt(body.length), 2);
  }
  return Buffer.concat([header, body]);
}

function readChromiumClientFrame(buffer: Buffer): { consumed: number; payload: Record<string, unknown> } | null {
  if (buffer.length < 2) return null;
  let offset = 2;
  const secondByte = buffer.readUInt8(1);
  let payloadLength = secondByte & 0x7f;
  if (payloadLength === 126) {
    if (buffer.length < 4) return null;
    payloadLength = buffer.readUInt16BE(2);
    offset = 4;
  } else if (payloadLength === 127) {
    if (buffer.length < 10) return null;
    payloadLength = Number(buffer.readBigUInt64BE(2));
    offset = 10;
  }
  const masked = (secondByte & 0x80) !== 0;
  if (!masked || buffer.length < offset + 4 + payloadLength) return null;
  const mask = buffer.subarray(offset, offset + 4);
  offset += 4;
  const encoded = buffer.subarray(offset, offset + payloadLength);
  const decoded = Buffer.from(encoded.map((value, index) => value ^ (mask[index % mask.length] ?? 0)));
  return {
    consumed: offset + payloadLength,
    payload: JSON.parse(decoded.toString('utf8')) as Record<string, unknown>,
  };
}

async function startMockChromiumConsoleServer(
  applicationUrl: string,
  targetNonce: string,
  options: MockChromiumConsoleServerOptions,
): Promise<MockChromiumConsoleServer> {
  const debuggerSockets = new Set<net.Socket>();
  const pendingRuntimeEnableResponses: Array<() => void> = [];
  const methods: string[] = [];
  const expressions: string[] = [];
  let currentInspectedUrl = `${applicationUrl}${applicationUrl.includes('?') ? '&' : '?'}valdiDevTools=1`;
  let currentTargetNonce = targetNonce;
  let resolveRuntimeEnableReceived: (() => void) | null = null;
  const runtimeEnableReceived = new Promise<void>(resolve => {
    resolveRuntimeEnableReceived = resolve;
  });
  const server = http.createServer((request, response) => {
    if (request.url !== '/json/list') {
      response.writeHead(404).end();
      return;
    }
    const address = server.address();
    if (typeof address !== 'object' || address === null) {
      response.writeHead(500).end();
      return;
    }
    response.writeHead(200, { 'Content-Type': 'application/json' });
    response.end(
      JSON.stringify([
        {
          id: 'selected-page',
          title: 'Selected Valdi page',
          type: 'page',
          url: currentInspectedUrl,
          webSocketDebuggerUrl: `ws://127.0.0.1:${address.port}/devtools/page/selected-page`,
        },
      ]),
    );
  });
  server.on('upgrade', (request, socket: net.Socket) => {
    const key = request.headers['sec-websocket-key'];
    if (request.url !== '/devtools/page/selected-page' || typeof key !== 'string') {
      socket.destroy();
      return;
    }
    const accept = crypto.createHash('sha1').update(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`).digest('base64');
    socket.write(
      `HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\n\r\n`,
    );
    debuggerSockets.add(socket);
    socket.once('end', () => socket.destroy());
    socket.once('close', () => debuggerSockets.delete(socket));
    let metricRequestCount = 0;
    let tracingStarted = false;
    let buffered = Buffer.alloc(0);
    socket.on('data', chunk => {
      buffered = Buffer.concat([buffered, chunk]);
      let frame = readChromiumClientFrame(buffered);
      while (frame) {
        buffered = buffered.subarray(frame.consumed);
        const id = frame.payload['id'];
        const method = frame.payload['method'];
        const params = frame.payload['params'] as Record<string, unknown> | undefined;
        if (typeof id !== 'number' || typeof method !== 'string') {
          socket.destroy();
          return;
        }
        methods.push(method);
        switch (method) {
          case 'Runtime.evaluate': {
            const expression = typeof params?.['expression'] === 'string' ? params['expression'] : '';
            expressions.push(expression);
            const guarded = expression.includes('__valdiDevToolsTargetMatched');
            const matched =
              guarded &&
              expression.includes(applicationUrl) &&
              expression.includes(currentTargetNonce) &&
              !(options.rejectIdentityAfterTracingStart === true && tracingStarted);
            let value: unknown;
            if (guarded) {
              const componentPropertyValue = options.componentPropertyValue;
              const componentPropertyEditProtocolVersion = options.componentPropertyEditProtocolVersion;
              const guardedValue = expression.includes('__VALDI_WEB_DEBUGGER__?.getSnapshot()')
                ? {
                    channel: 'valdi-web-debugger',
                    componentPropertyEditingAvailable: options.componentPropertyEditingAvailable !== false,
                    ...(componentPropertyEditProtocolVersion === null
                      ? {}
                      : {
                          componentPropertyEditProtocolVersion: componentPropertyEditProtocolVersion ?? 1,
                        }),
                    selectedNodeId: 'web-root',
                    snapshot: {
                      tree:
                        componentPropertyValue === undefined
                          ? { children: [], id: 'web-root', tag: 'WebRoot' }
                          : {
                              children: [],
                              component: {
                                key: 'root',
                                name: 'WebRoot',
                                properties: { title: componentPropertyValue },
                                propertyEdits: {
                                  title: { componentToken: 'a'.repeat(32), snapshotRevision: 3 },
                                },
                              },
                              id: 'component:[null,"root"]',
                              tag: 'WebRoot',
                            },
                      viewport: { height: 800, width: 1200 },
                    },
                    type: 'snapshot',
                  }
                : expression.includes('__VALDI_WEB_DEBUGGER__?.editComponentProperty?.')
                  ? options.componentPropertyEditResult !== false
                  : true;
              value = { __valdiDevToolsTargetMatched: matched, ...(matched ? { value: guardedValue } : {}) };
            } else if (expression === 'String(globalThis.location.href)') {
              value = currentInspectedUrl;
            } else if (expression.includes("getEntriesByType('resource')")) {
              value = {
                navigation: { domContentLoadedMs: 30, loadMs: 50 },
                paints: [{ name: 'first-contentful-paint', startTime: 25 }],
                rendererTracingEnabled: false,
                resourceCount: 4,
                transferSize: 2048,
                uptimeMs: 100,
              };
            } else {
              value = true;
            }
            socket.write(
              encodeChromiumServerMessage({
                id,
                result: {
                  result: {
                    type: typeof value,
                    value,
                  },
                },
              }),
            );
            break;
          }
          case 'Performance.getMetrics': {
            metricRequestCount++;
            socket.write(
              encodeChromiumServerMessage({
                id,
                result: {
                  metrics: [
                    { name: 'TaskDuration', value: metricRequestCount * 0.012 },
                    { name: 'ScriptDuration', value: metricRequestCount * 0.004 },
                    { name: 'LayoutDuration', value: metricRequestCount * 0.002 },
                    { name: 'LayoutCount', value: metricRequestCount * 2 },
                    { name: 'RecalcStyleCount', value: metricRequestCount },
                    { name: 'JSHeapUsedSize', value: 1024 },
                    { name: 'JSHeapTotalSize', value: 2048 },
                  ],
                },
              }),
            );
            break;
          }
          case 'Tracing.end': {
            if (options.closeOnTracingEnd) {
              socket.destroy();
              break;
            }
            socket.write(
              Buffer.concat([
                encodeChromiumServerMessage({ id, result: {} }),
                encodeChromiumServerMessage({
                  method: 'Tracing.dataCollected',
                  params: {
                    value: [
                      { name: 'Valdi.Renderer.onRender.Example', ph: 'X', ts: 1000, dur: 300, tid: 7 },
                      { name: 'Layout', ph: 'X', ts: 1400, dur: 500, tid: 7 },
                      { name: 'RunTask', ph: 'X', ts: 2000, dur: 75_000, tid: 7 },
                      { name: 'Unrelated', ph: 'X', ts: 3000, dur: 400, tid: 7 },
                    ],
                  },
                }),
                encodeChromiumServerMessage({
                  method: 'Tracing.tracingComplete',
                  params: { dataLossOccurred: false },
                }),
              ]),
            );
            break;
          }
          default: {
            if (method === 'Tracing.start' && options.rejectTracingStart) {
              socket.write(
                encodeChromiumServerMessage({
                  error: { message: 'Tracing is already started by another client.' },
                  id,
                }),
              );
              break;
            }
            if (method === 'Tracing.start') tracingStarted = true;
            if (method === 'Tracing.start' && options.dropTracingStartResponse) {
              socket.destroy();
              break;
            }
            const sendResponse = () => {
              if (socket.destroyed) return;
              socket.write(encodeChromiumServerMessage({ id, result: {} }));
              if (method === 'Runtime.enable') {
                socket.write(
                  encodeChromiumServerMessage({
                    method: 'Runtime.consoleAPICalled',
                    params: {
                      args: [{ type: 'string', value: 'Synthetic <renderer> output' }],
                      timestamp: 101,
                      type: 'warning',
                    },
                  }),
                );
              }
              if (method === 'Log.enable') {
                socket.write(
                  encodeChromiumServerMessage({
                    method: 'Log.entryAdded',
                    params: {
                      entry: {
                        level: 'error',
                        text: 'authorization: Bearer synthetic-private-token',
                        timestamp: 102,
                      },
                    },
                  }),
                );
              }
            };
            if (method === 'Runtime.enable') {
              resolveRuntimeEnableReceived?.();
              if (options.holdRuntimeEnable) {
                pendingRuntimeEnableResponses.push(sendResponse);
              } else {
                sendResponse();
              }
            } else {
              sendResponse();
            }
            break;
          }
        }
        frame = readChromiumClientFrame(buffered);
      }
    });
  });

  await new Promise<void>((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  const address = server.address();
  if (typeof address !== 'object' || address === null) throw new Error('Mock Chromium server did not bind.');
  return {
    close: async () => {
      for (const socket of debuggerSockets) socket.destroy();
      await closeServer(server);
    },
    debuggerSockets,
    expressions,
    methods,
    port: address.port,
    releaseRuntimeEnable(): void {
      for (const sendResponse of pendingRuntimeEnableResponses.splice(0)) sendResponse();
    },
    runtimeEnableReceived,
    setInspectedUrl(nextInspectedUrl: string): void {
      currentInspectedUrl = nextInspectedUrl;
    },
    setTargetNonce(nextTargetNonce: string): void {
      currentTargetNonce = nextTargetNonce;
    },
  };
}

const TEST_PACKET_MAGIC = Buffer.from([0x33, 0xc6, 0x00, 0x01]);

function encodeDaemonPacket(payload: object): Buffer {
  const body = Buffer.from(JSON.stringify(payload), 'utf8');
  const header = Buffer.alloc(8);
  TEST_PACKET_MAGIC.copy(header, 0);
  header.writeUInt32LE(body.length, 4);
  return Buffer.concat([header, body]);
}

async function startMockDaemon(customResponseBody?: unknown): Promise<MockDaemon> {
  const sockets = new Set<net.Socket>();
  const requests: Array<Record<string, unknown>> = [];
  let applicationId = 'mock.app';
  let contexts = [{ id: 'mock-context', rootComponentName: 'Mock App' }];
  let responseId = 0;
  const server = net.createServer(socket => {
    sockets.add(socket);
    socket.once('close', () => sockets.delete(socket));
    let buffered = Buffer.alloc(0);
    socket.on('data', chunk => {
      buffered = Buffer.concat([buffered, chunk]);
      while (buffered.length >= 8) {
        const bodyLength = buffered.readUInt32LE(4);
        if (buffered.length < bodyLength + 8) return;
        const packet = JSON.parse(buffered.subarray(8, bodyLength + 8).toString('utf8')) as Record<string, unknown>;
        buffered = buffered.subarray(bodyLength + 8);
        const event = packet['event'] as Record<string, unknown> | undefined;
        const payload = event?.['payload_from_client'] as Record<string, unknown> | undefined;
        if (!payload) continue;
        const requestBody = JSON.parse(String(payload['payload_string'])) as Record<string, unknown>;
        requests.push(requestBody);
        const type = requestBody['type'];
        let body: unknown;
        let responseType: number;
        if (type === 2) {
          responseType = -2;
          body = contexts;
        } else if (type === 3) {
          responseType = -3;
          const requestedContext = (requestBody['body'] as Record<string, unknown>)['id'];
          body = {
            children: [],
            id: `tree-${String(requestedContext)}`,
            tag: 'MockRoot',
          };
        } else {
          responseType = -1000;
          const custom = requestBody['body'] as Record<string, unknown>;
          const data = custom['data'] as Record<string, unknown>;
          if (customResponseBody !== undefined) {
            body = customResponseBody;
          } else if (data['action'] === 'list') {
            body = {
              handled: true,
              data: {
                contractVersion: 1,
                providers: [{ available: true, id: 'independent', kind: 'storage', label: 'Independent' }],
                revision: 1,
              },
            };
          } else {
            body = {
              handled: true,
              data: {
                contractVersion: 1,
                data: { request: data['request'] },
                registrationToken: 7,
                revision: 1,
              },
            };
          }
        }
        socket.write(
          encodeDaemonPacket({
            request: {
              forward_client_payload: {
                client_id: 1,
                payload_string: JSON.stringify({
                  body,
                  requestId: requestBody['requestId'],
                  type: responseType,
                }),
              },
              request_id: `mock-${(++responseId).toString()}`,
            },
          }),
        );
      }
    });
    socket.write(
      encodeDaemonPacket({
        request: {
          configure: { application_id: applicationId, platform: 'android' },
          request_id: 'configure-1',
        },
      }),
    );
  });
  await new Promise<void>((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  const address = server.address();
  if (typeof address !== 'object' || address === null) throw new Error('Mock daemon did not bind a TCP port.');
  return {
    close: async () => {
      sockets.forEach(socket => socket.destroy());
      await closeServer(server);
    },
    port: address.port,
    requests,
    setApplicationId(nextApplicationId: string): void {
      applicationId = nextApplicationId;
    },
    setContexts(nextContexts: Array<{ id: string; rootComponentName: string }>): void {
      contexts = nextContexts;
    },
  };
}

function targetDiscoveryFor(endpoints: () => Array<{ deviceId: string; port: number }>): {
  defaultDaemonEndpoints: [];
  discoverAndroidDaemonEndpoints(): Promise<{
    endpoints: Array<{ deviceId: string; port: number }>;
    error: null;
  }>;
  discoverDebuggerProxyTargets(): Promise<[]>;
  probeDebuggerProxy(): Promise<false>;
} {
  return {
    defaultDaemonEndpoints: [],
    discoverAndroidDaemonEndpoints: () => Promise.resolve({ endpoints: endpoints(), error: null }),
    discoverDebuggerProxyTargets: () => Promise.resolve([]),
    probeDebuggerProxy: () => Promise.resolve(false),
  };
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
            referrerPolicy: String(response.headers['referrer-policy'] ?? ''),
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
          referrerPolicy: String(response.headers['referrer-policy'] ?? ''),
          xFrameOptions: String(response.headers['x-frame-options'] ?? ''),
        });
      });
    });
    outgoingRequest.once('error', reject);
  });
  return { request: outgoingRequest, result };
}

function authenticatedOptions(server: DebuggerServerInfo, options: HttpRequestOptions): HttpRequestOptions {
  return {
    ...options,
    headers: {
      ...options.headers,
      [server.apiTokenHeader]: server.apiToken,
    },
  };
}

async function requestApi(server: DebuggerServerInfo, route: string, options: HttpRequestOptions): Promise<HttpResult> {
  return await request(new URL(route, server.url).toString(), authenticatedOptions(server, options));
}

function eventSourceUrl(server: DebuggerServerInfo, route: string): string {
  const url = new URL(route, server.url);
  url.searchParams.set('valdiDebuggerToken', server.apiToken);
  return url.toString();
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
          const lines = eventBlock.trimStart().split('\n');
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
              response.socket.destroy();
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
  let mockDaemon: MockDaemon | undefined;
  let originalHome: string | undefined;
  let originalServicePort: string | undefined;
  let originalUiPort: string | undefined;

  beforeEach(() => {
    originalHome = process.env['HOME'];
    originalServicePort = process.env['VALDI_DEBUGGER_SERVICE_PORT'];
    originalUiPort = process.env['VALDI_DEBUGGER_UI_PORT'];
    delete process.env['VALDI_DEBUGGER_SERVICE_PORT'];
    delete process.env['VALDI_DEBUGGER_UI_PORT'];
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
    if (mockDaemon) {
      await mockDaemon.close();
      mockDaemon = undefined;
    }
    if (originalHome === undefined) {
      delete process.env['HOME'];
    } else {
      process.env['HOME'] = originalHome;
    }
    if (originalServicePort === undefined) {
      delete process.env['VALDI_DEBUGGER_SERVICE_PORT'];
    } else {
      process.env['VALDI_DEBUGGER_SERVICE_PORT'] = originalServicePort;
    }
    if (originalUiPort === undefined) {
      delete process.env['VALDI_DEBUGGER_UI_PORT'];
    } else {
      process.env['VALDI_DEBUGGER_UI_PORT'] = originalUiPort;
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
      oversized: 'x'.repeat(70_000),
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
    expect((projection.nodes[0]?.data['oversized'] as string).length).toBe(65_536);
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
    expect(debuggerServer.apiToken).toMatch(/^[A-Za-z0-9_-]{43}$/);
    expect(debuggerServer.url).not.toContain(debuggerServer.apiToken);
    expect(result.body).toContain(`name="valdi-debugger-api-token" content="${debuggerServer.apiToken}"`);
    expect(result.contentSecurityPolicy).toContain("script-src 'self'");
    expect(result.contentSecurityPolicy).toContain("frame-ancestors 'none'");
    expect(result.referrerPolicy).toBe('no-referrer');
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
    expect(panel.body).toContain(`name="valdi-debugger-api-token" content="${debuggerServer.apiToken}"`);
    expect(panel.referrerPolicy).toBe('no-referrer');
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

    const matching = await requestApi(
      debuggerServer,
      `/api/devtools/target?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3FvaldiDebugger%3D1%26mode%3Ddev%26valdiDevTools%3D1%26tenant%3Dalpha%26valdiTrace%3Dchrome&targetNonce=${WEB_PREVIEW_NONCE}`,
      GET_REQUEST_OPTIONS,
    );
    const differentPath = await requestApi(
      debuggerServer,
      `/api/devtools/target?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Fother.html&targetNonce=${WEB_PREVIEW_NONCE}`,
      GET_REQUEST_OPTIONS,
    );
    const differentQuery = await requestApi(
      debuggerServer,
      `/api/devtools/target?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3Fmode%3Ddev%26tenant%3Dbeta%26valdiDebugger%3D1%26valdiDevTools%3D1&targetNonce=${WEB_PREVIEW_NONCE}`,
      GET_REQUEST_OPTIONS,
    );

    expect(matching.statusCode).toBe(200);
    expect(JSON.parse(matching.body)).toEqual({
      target: jasmine.objectContaining({
        applicationUrl: 'http://127.0.0.1:54321/index.html?tenant=alpha&mode=dev',
        capabilities: ['components', 'component-properties', 'snapshot', 'highlight', 'console', 'performance'],
        debuggingPort: 9333,
        id: 'owl:web-preview',
        identityMode: 'inspected-page',
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

    const result = await requestApi(
      debuggerServer,
      '/api/devtools/target?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html',
      GET_REQUEST_OPTIONS,
    );

    expect(result.statusCode).toBe(400);
    expect(JSON.parse(result.body)).toEqual({
      error: 'DevTools target discovery requires a valid inspected-tab nonce.',
    });
  });

  it('bounds configured web preview URLs and derived target names at startup', async () => {
    const urlPrefix = 'http://127.0.0.1:54321/?padding=';
    const maximumUrl = `${urlPrefix}${'x'.repeat(4096 - Buffer.byteLength(urlPrefix, 'utf8'))}`;
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      webPreviewUrl: maximumUrl,
    });
    await debuggerServer.close();
    debuggerServer = undefined;

    await expectAsync(
      startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: `${maximumUrl}x`,
      }),
    ).toBeRejectedWithError('The integrated DevTools web preview URL cannot exceed 4096 bytes.');

    const maximumName = 'n'.repeat(256);
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      webPreviewUrl: `http://127.0.0.1:54321/${maximumName}`,
    });
    await debuggerServer.close();
    debuggerServer = undefined;

    await expectAsync(
      startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: `http://127.0.0.1:54321/${maximumName}x`,
      }),
    ).toBeRejectedWithError('The integrated DevTools web preview target name cannot exceed 256 bytes.');
  });

  it('discovers and exact-resolves opaque native targets alongside the explicit web preview', async () => {
    mockDaemon = await startMockDaemon();
    mockDaemon.setContexts([
      { id: 'first-context', rootComponentName: 'First' },
      { id: 'second-context', rootComponentName: 'Second' },
    ]);
    let endpoints = [{ deviceId: 'emulator-5554', port: mockDaemon.port }];
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      targetDiscovery: targetDiscoveryFor(() => endpoints),
      webPreviewUrl: 'http://127.0.0.1:54321/index.html',
    });

    const registry = await requestApi(
      debuggerServer,
      new URL('/api/devtools/targets', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    const registryBody = JSON.parse(registry.body) as { targets: Array<Record<string, unknown>> };
    const nativeTargets = registryBody.targets.filter(target => target['transport'] === 'valdi-daemon');
    const webTargets = registryBody.targets.filter(target => target['id'] === 'owl:web-preview');
    const second = nativeTargets.find(target => target['contextId'] === 'second-context');
    if (!second) throw new Error('Expected the second native debugger context.');
    const targetId = String(second['id']);

    expect(registry.statusCode).toBe(200);
    expect(nativeTargets.length).toBe(2);
    expect(webTargets.length).toBe(1);
    expect(targetId).toMatch(/^vdt_[\w-]{32}$/);
    expect(targetId).not.toContain(mockDaemon.port.toString());
    expect(targetId).not.toContain('second-context');
    expect(second['capabilities']).toEqual(['components', 'snapshot']);
    expect(second['identityMode']).toBe('target-id');
    expect(webTargets[0]?.['identityMode']).toBe('inspected-page');
    expect(webTargets[0]?.['capabilities']).toEqual([
      'components',
      'component-properties',
      'snapshot',
      'highlight',
      'console',
      'performance',
    ]);

    const resolved = await requestApi(
      debuggerServer,
      new URL(`/api/devtools/target?targetId=${encodeURIComponent(targetId)}`, debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    const snapshot = await requestApi(
      debuggerServer,
      new URL(`/api/devtools/snapshot?targetId=${encodeURIComponent(targetId)}`, debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    const snapshotBody = JSON.parse(snapshot.body) as {
      target: Record<string, unknown>;
      tree: { nodes: Array<{ data: Record<string, unknown> }> };
    };

    expect(resolved.statusCode).toBe(200);
    expect((JSON.parse(resolved.body) as { target: Record<string, unknown> }).target['id']).toBe(targetId);
    expect(snapshot.statusCode).toBe(200);
    expect(snapshotBody.target['id']).toBe(targetId);
    expect(snapshotBody.tree.nodes[0]?.data['id']).toBe('tree-second-context');

    const duplicate = await requestApi(
      debuggerServer,
      new URL(
        `/api/devtools/target?targetId=${encodeURIComponent(targetId)}&targetId=${encodeURIComponent(targetId)}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    const mixed = await requestApi(
      debuggerServer,
      new URL(
        `/api/devtools/snapshot?targetId=${encodeURIComponent(targetId)}&port=${mockDaemon.port.toString()}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    const unknown = await requestApi(
      debuggerServer,
      new URL('/api/devtools/target?targetId=vdt_AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );

    expect(duplicate.statusCode).toBe(400);
    expect(mixed.statusCode).toBe(400);
    expect(unknown.statusCode).toBe(404);

    mockDaemon.setApplicationId('mock.replacement');
    const replaced = await requestApi(
      debuggerServer,
      new URL(`/api/devtools/target?targetId=${encodeURIComponent(targetId)}`, debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    expect(replaced.statusCode).toBe(404);

    const replacementRegistry = await requestApi(
      debuggerServer,
      new URL('/api/devtools/targets', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    const replacementTargets = (JSON.parse(replacementRegistry.body) as { targets: Array<Record<string, unknown>> })
      .targets;
    expect(replacementTargets.some(target => target['id'] === targetId)).toBeFalse();
    const replacementTarget = replacementTargets.find(target => target['contextId'] === 'second-context');
    if (!replacementTarget) throw new Error('Expected the replacement native debugger context.');
    const replacementTargetId = String(replacementTarget['id']);
    expect(replacementTargetId).not.toBe(targetId);

    endpoints = [{ deviceId: 'replacement-device', port: mockDaemon.port }];
    const samePortReplacement = await requestApi(
      debuggerServer,
      new URL(
        `/api/devtools/target?targetId=${encodeURIComponent(replacementTargetId)}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    expect(samePortReplacement.statusCode).toBe(404);

    const samePortReplacementRegistry = await requestApi(
      debuggerServer,
      new URL('/api/devtools/targets', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    const samePortReplacementTargets = (
      JSON.parse(samePortReplacementRegistry.body) as { targets: Array<Record<string, unknown>> }
    ).targets;
    const samePortReplacementTarget = samePortReplacementTargets.find(
      target => target['contextId'] === 'second-context',
    );
    if (!samePortReplacementTarget) throw new Error('Expected the same-port replacement debugger context.');
    const samePortReplacementTargetId = String(samePortReplacementTarget['id']);
    expect(samePortReplacementTargetId).not.toBe(replacementTargetId);

    endpoints = [];
    const removed = await requestApi(
      debuggerServer,
      new URL(
        `/api/devtools/target?targetId=${encodeURIComponent(samePortReplacementTargetId)}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    expect(removed.statusCode).toBe(404);
  });

  it('treats every configured target-discovery endpoint as read-only', async () => {
    mockDaemon = await startMockDaemon();
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      targetDiscovery: {
        defaultDaemonEndpoints: [
          {
            autoForward: true,
            deviceId: 'unsafe;device',
            port: mockDaemon.port,
          },
        ],
        discoverAndroidDaemonEndpoints: () => Promise.resolve({ endpoints: [], error: null }),
        discoverDebuggerProxyTargets: () => Promise.resolve([]),
        probeDebuggerProxy: () => Promise.resolve(false),
      },
    });

    const registry = await requestApi(
      debuggerServer,
      new URL('/api/devtools/targets', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    const targets = (JSON.parse(registry.body) as { targets: Array<Record<string, unknown>> }).targets;

    expect(registry.statusCode).toBe(200);
    expect(targets.some(target => target['transport'] === 'valdi-daemon')).toBeTrue();
  });

  it('preserves the legacy status shape and explicit-port path without registry discovery', async () => {
    mockDaemon = await startMockDaemon();
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      targetDiscovery: {
        defaultDaemonEndpoints: [],
        discoverAndroidDaemonEndpoints: () => Promise.reject(new Error('Status must not discover ADB endpoints.')),
        discoverDebuggerProxyTargets: () => Promise.reject(new Error('Status must not discover proxy targets.')),
        probeDebuggerProxy: () => Promise.reject(new Error('Status must not use registry proxy discovery.')),
      },
      webPreviewUrl: 'http://127.0.0.1:54321/index.html',
    });

    const originalPath = process.env['PATH'];
    process.env['PATH'] = '/nonexistent';
    let status: HttpResult;
    try {
      status = await requestApi(
        debuggerServer,
        new URL(`/api/status?port=${mockDaemon.port.toString()}`, debuggerServer.url).toString(),
        GET_REQUEST_OPTIONS,
      );
    } finally {
      if (originalPath === undefined) delete process.env['PATH'];
      else process.env['PATH'] = originalPath;
    }
    const payload = JSON.parse(status.body) as Record<string, unknown>;
    const webPreviewTarget = payload['webPreviewTarget'] as Record<string, unknown>;

    expect(status.statusCode).toBe(200);
    expect(Object.keys(payload).sort()).toEqual(['defaultPort', 'hotReloadProxy', 'ports', 'webPreviewTarget']);
    expect(payload['ports']).toEqual([
      jasmine.objectContaining({
        connected: true,
        port: mockDaemon.port,
      }),
    ]);
    expect(Object.keys(webPreviewTarget).sort()).toEqual([
      'applicationId',
      'applicationUrl',
      'debuggingPort',
      'id',
      'name',
      'owlTarget',
      'platform',
      'sessionId',
      'state',
      'transport',
    ]);
  });

  it('rejects a final serialized target registry response larger than 512 KiB', async () => {
    let proxyTargetCount = 1;
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      targetDiscovery: {
        defaultDaemonEndpoints: [],
        discoverAndroidDaemonEndpoints: () => Promise.resolve({ endpoints: [], error: null }),
        discoverDebuggerProxyTargets: () =>
          Promise.resolve(
            Array.from({ length: proxyTargetCount }, (_, index) => {
              const suffix = index.toString();
              const deviceId = `device-${suffix}-${'d'.repeat(900)}`;
              return {
                adapterType: `_android_${deviceId}`,
                appId: `application-${suffix}-${'a'.repeat(980)}`,
                id: `proxy-${suffix}`,
                metadata: { deviceId, deviceName: `Device ${suffix} ${'n'.repeat(980)}` },
                title: `Runtime ${suffix} ${'t'.repeat(980)}`,
                webSocketDebuggerUrl: `ws://127.0.0.1:9010/${suffix}/${'w'.repeat(3900)}`,
              };
            }),
          ),
        probeDebuggerProxy: () => Promise.resolve(true),
      },
    });

    const bounded = await requestApi(
      debuggerServer,
      new URL('/api/devtools/targets', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    proxyTargetCount = 80;
    const oversized = await requestApi(
      debuggerServer,
      new URL('/api/devtools/targets', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );

    expect(bounded.statusCode).toBe(200);
    expect(oversized.statusCode).toBe(500);
    expect(JSON.parse(oversized.body)).toEqual({
      error: 'Valdi DevTools target registry response exceeded 524288 bytes.',
    });
  });

  it('rejects mixed web-preview and target-ID resolver modes without falling back', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      targetDiscovery: targetDiscoveryFor(() => []),
      webPreviewUrl: 'http://127.0.0.1:54321/index.html',
    });
    const mixed = await requestApi(
      debuggerServer,
      new URL(
        `/api/devtools/target?targetId=owl%3Aweb-preview&inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html&targetNonce=${WEB_PREVIEW_NONCE}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    const duplicateNonce = await requestApi(
      debuggerServer,
      new URL(
        `/api/devtools/target?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html&targetNonce=${WEB_PREVIEW_NONCE}&targetNonce=${WEB_PREVIEW_NONCE}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    const webByIdTarget = await requestApi(
      debuggerServer,
      new URL('/api/devtools/target?targetId=owl%3Aweb-preview', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    const webByIdSnapshot = await requestApi(
      debuggerServer,
      new URL('/api/devtools/snapshot?targetId=owl%3Aweb-preview', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );

    expect(mixed.statusCode).toBe(400);
    expect(duplicateNonce.statusCode).toBe(400);
    expect(webByIdTarget.statusCode).toBe(400);
    expect(webByIdSnapshot.statusCode).toBe(400);
    expect(JSON.parse(webByIdTarget.body)).toEqual({
      error: 'The selected debugger target cannot be attached by target ID.',
    });
    expect(JSON.parse(webByIdSnapshot.body)).toEqual({
      error: 'The selected debugger target cannot be attached by target ID.',
    });
  });

  it('resolves and snapshots the web preview only through its inspected-page identity', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const inspectedUrl = `${applicationUrl}?valdiDevTools=1`;
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: false,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        chromiumDebuggingPort: chromium.port,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        targetDiscovery: targetDiscoveryFor(() => []),
        webPreviewUrl: applicationUrl,
      });
      const targetUrl = new URL('/api/devtools/target', debuggerServer.url);
      targetUrl.searchParams.set('inspectedUrl', inspectedUrl);
      targetUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);
      const snapshotUrl = new URL('/api/devtools/snapshot', debuggerServer.url);
      snapshotUrl.searchParams.set('inspectedUrl', inspectedUrl);
      snapshotUrl.searchParams.set('sessionId', 'web-preview');
      snapshotUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);

      const target = await requestApi(debuggerServer, targetUrl.toString(), GET_REQUEST_OPTIONS);
      const snapshot = await requestApi(debuggerServer, snapshotUrl.toString(), GET_REQUEST_OPTIONS);
      const snapshotBody = JSON.parse(snapshot.body) as {
        target: Record<string, unknown>;
        tree: { nodes: Array<{ data: Record<string, unknown> }> };
      };

      expect(target.statusCode).toBe(200);
      expect((JSON.parse(target.body) as { target: Record<string, unknown> }).target).toEqual(
        jasmine.objectContaining({
          id: 'owl:web-preview',
          identityMode: 'inspected-page',
          transport: 'chromium-cdp',
        }),
      );
      expect((JSON.parse(target.body) as { target: { capabilities: string[] } }).target.capabilities).not.toContain(
        'component-property-edit',
      );
      expect(snapshot.statusCode).withContext(snapshot.body).toBe(200);
      expect(snapshotBody.target['identityMode']).toBe('inspected-page');
      expect(snapshotBody.target['capabilities']).toContain('component-property-edit');
      expect(snapshotBody.tree.nodes[0]?.data['id']).toBe('web-root');
    } finally {
      await chromium.close();
    }
  });

  it('downgrades snapshot capabilities when the web bridge cannot issue secure edit tokens', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      componentPropertyEditingAvailable: false,
      holdRuntimeEnable: false,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        chromiumDebuggingPort: chromium.port,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
      });
      const snapshotUrl = new URL('/api/devtools/snapshot', debuggerServer.url);
      snapshotUrl.searchParams.set('inspectedUrl', `${applicationUrl}?valdiDevTools=1`);
      snapshotUrl.searchParams.set('sessionId', 'web-preview');
      snapshotUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);

      const snapshot = await requestApi(debuggerServer, snapshotUrl.toString(), GET_REQUEST_OPTIONS);
      const target = (JSON.parse(snapshot.body) as { target: { capabilities: string[] } }).target;

      expect(snapshot.statusCode).toBe(200);
      expect(target.capabilities).toContain('component-properties');
      expect(target.capabilities).not.toContain('component-property-edit');
    } finally {
      await chromium.close();
    }
  });

  it('downgrades snapshot capabilities for missing or unsupported bridge edit protocols', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    for (const componentPropertyEditProtocolVersion of [null, 2]) {
      const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
        componentPropertyEditProtocolVersion,
        holdRuntimeEnable: false,
      });
      try {
        debuggerServer = await startDebuggerServer({
          assetRoot,
          chromiumDebuggingPort: chromium.port,
          host: '127.0.0.1',
          port: await getFreePort(),
          strictPort: true,
          webPreviewUrl: applicationUrl,
        });
        const snapshotUrl = new URL('/api/devtools/snapshot', debuggerServer.url);
        snapshotUrl.searchParams.set('inspectedUrl', `${applicationUrl}?valdiDevTools=1`);
        snapshotUrl.searchParams.set('sessionId', 'web-preview');
        snapshotUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);

        const snapshot = await requestApi(debuggerServer, snapshotUrl.toString(), GET_REQUEST_OPTIONS);
        const target = (JSON.parse(snapshot.body) as { target: { capabilities: string[] } }).target;

        expect(snapshot.statusCode).toBe(200);
        expect(target.capabilities).toContain('component-properties');
        expect(target.capabilities).not.toContain('component-property-edit');
      } finally {
        if (debuggerServer) {
          await debuggerServer.close();
          debuggerServer = undefined;
        }
        await chromium.close();
      }
    }
  });

  it('preserves an editable scalar exactly through projection before a no-op update', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const inspectedUrl = `${applicationUrl}?valdiDevTools=1`;
    const originalValue = 'x'.repeat(60_000);
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      componentPropertyValue: originalValue,
      holdRuntimeEnable: false,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        chromiumDebuggingPort: chromium.port,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
      });
      const snapshotUrl = new URL('/api/devtools/snapshot', debuggerServer.url);
      snapshotUrl.searchParams.set('inspectedUrl', inspectedUrl);
      snapshotUrl.searchParams.set('sessionId', 'web-preview');
      snapshotUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);

      const snapshot = await requestApi(debuggerServer, snapshotUrl.toString(), GET_REQUEST_OPTIONS);
      const snapshotBody = JSON.parse(snapshot.body) as {
        tree: {
          nodes: Array<{
            data: {
              component?: {
                properties?: { title?: unknown };
                propertyEdits?: {
                  title?: { componentToken: unknown; snapshotRevision: unknown };
                };
              };
            };
          }>;
        };
      };
      const component = snapshotBody.tree.nodes[0]?.data.component;
      const projectedValue = component?.properties?.title;
      const metadata = component?.propertyEdits?.title;
      if (
        typeof projectedValue !== 'string' ||
        typeof metadata?.componentToken !== 'string' ||
        typeof metadata.snapshotRevision !== 'number'
      ) {
        throw new TypeError('Expected exact projected edit metadata.');
      }

      expect(snapshot.statusCode).withContext(snapshot.body).toBe(200);
      expect(projectedValue).toBe(originalValue);
      const editBody = {
        componentId: 'component:[null,"root"]',
        componentToken: metadata.componentToken,
        inspectedUrl,
        propertyName: 'title',
        protocolVersion: 1,
        sessionId: 'web-preview',
        snapshotRevision: metadata.snapshotRevision,
        targetNonce: WEB_PREVIEW_NONCE,
        value: projectedValue,
      };
      const edit = await requestApi(
        debuggerServer,
        new URL('/api/devtools/component-property', debuggerServer.url).toString(),
        {
          body: JSON.stringify(editBody),
          headers: { 'Content-Type': 'application/json' },
          method: 'POST',
        },
      );
      const editExpression = chromium.expressions.find(expression => expression.includes('editComponentProperty'));

      expect(edit.statusCode).withContext(edit.body).toBe(200);
      expect(editExpression).toContain(
        `globalThis.__VALDI_WEB_DEBUGGER__?.editComponentProperty?.(${JSON.stringify({
          componentId: editBody.componentId,
          componentToken: editBody.componentToken,
          propertyName: editBody.propertyName,
          protocolVersion: editBody.protocolVersion,
          snapshotRevision: editBody.snapshotRevision,
          value: originalValue,
        })})`,
      );
      expect(editExpression).not.toContain('…[truncated]');
    } finally {
      await chromium.close();
    }
  });

  it('accepts only the exact web-preview component-property edit tuple', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const inspectedUrl = `${applicationUrl}?valdiDevTools=1`;
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: false,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        chromiumDebuggingPort: chromium.port,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
      });
      const exactBody = {
        componentId: 'component:[null,"root"]',
        componentToken: 'a'.repeat(32),
        inspectedUrl,
        propertyName: 'title',
        protocolVersion: 1,
        sessionId: 'web-preview',
        snapshotRevision: 3,
        targetNonce: WEB_PREVIEW_NONCE,
        value: 'updated',
      };
      const post = (body: unknown, contentType = 'application/json') =>
        requestApi(debuggerServer, new URL('/api/devtools/component-property', debuggerServer?.url).toString(), {
          body: JSON.stringify(body),
          headers: { 'Content-Type': contentType },
          method: 'POST',
        });

      const success = await post(exactBody);
      expect(success.statusCode).withContext(success.body).toBe(200);
      expect(JSON.parse(success.body)).toEqual({ updated: true });
      const editExpression = chromium.expressions.find(expression => expression.includes('editComponentProperty'));
      expect(editExpression).toContain(
        `globalThis.__VALDI_WEB_DEBUGGER__?.editComponentProperty?.(${JSON.stringify({
          componentId: exactBody.componentId,
          componentToken: exactBody.componentToken,
          propertyName: exactBody.propertyName,
          protocolVersion: exactBody.protocolVersion,
          snapshotRevision: exactBody.snapshotRevision,
          value: exactBody.value,
        })})`,
      );

      for (const body of [
        { ...exactBody, extra: true },
        Object.fromEntries(Object.entries(exactBody).filter(([key]) => key !== 'componentToken')),
        Object.fromEntries(Object.entries(exactBody).filter(([key]) => key !== 'protocolVersion')),
        { ...exactBody, componentToken: 'A'.repeat(32) },
        { ...exactBody, protocolVersion: 2 },
        { ...exactBody, snapshotRevision: 0 },
        { ...exactBody, propertyName: '   ' },
        { ...exactBody, propertyName: 'children' },
        { ...exactBody, value: Number.POSITIVE_INFINITY },
        { ...exactBody, targetId: 'native-target' },
      ]) {
        const result = await post(body);
        expect(result.statusCode).withContext(JSON.stringify(body)).toBe(400);
      }
      const negativeZeroBody = JSON.stringify({ ...exactBody, value: 'NEGATIVE_ZERO' }).replace(
        '"NEGATIVE_ZERO"',
        '-0',
      );
      const negativeZero = await requestApi(
        debuggerServer,
        new URL('/api/devtools/component-property', debuggerServer.url).toString(),
        {
          body: negativeZeroBody,
          headers: { 'Content-Type': 'application/json' },
          method: 'POST',
        },
      );
      const wrongMethod = await requestApi(
        debuggerServer,
        new URL('/api/devtools/component-property', debuggerServer.url).toString(),
        GET_REQUEST_OPTIONS,
      );
      const wrongContentType = await post(exactBody, 'text/plain');
      const queryIdentityUrl = new URL('/api/devtools/component-property', debuggerServer.url);
      queryIdentityUrl.searchParams.set('targetId', 'native-target');
      const queryIdentity = await requestApi(debuggerServer, queryIdentityUrl.toString(), {
        body: JSON.stringify(exactBody),
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
      });
      expect(wrongMethod.statusCode).toBe(405);
      expect(wrongContentType.statusCode).toBe(415);
      expect(negativeZero.statusCode).toBe(400);
      expect(queryIdentity.statusCode).toBe(400);
    } finally {
      await chromium.close();
    }
  });

  it('returns only a generic conflict when the exact bridge mutation is rejected', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const inspectedUrl = `${applicationUrl}?valdiDevTools=1`;
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      componentPropertyEditResult: false,
      holdRuntimeEnable: false,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        chromiumDebuggingPort: chromium.port,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
      });

      const response = await requestApi(
        debuggerServer,
        new URL('/api/devtools/component-property', debuggerServer.url).toString(),
        {
          body: JSON.stringify({
            componentId: 'component:[null,"root"]',
            componentToken: 'a'.repeat(32),
            inspectedUrl,
            propertyName: 'title',
            protocolVersion: 1,
            sessionId: 'web-preview',
            snapshotRevision: 3,
            targetNonce: WEB_PREVIEW_NONCE,
            value: 'updated',
          }),
          headers: { 'Content-Type': 'application/json' },
          method: 'POST',
        },
      );

      expect(response.statusCode).toBe(409);
      expect(JSON.parse(response.body)).toEqual({
        error: 'The component property edit is stale or invalid.',
      });
    } finally {
      await chromium.close();
    }
  });

  it('lists proxy-only target IDs but rejects them as non-native attachments', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      targetDiscovery: {
        defaultDaemonEndpoints: [],
        discoverAndroidDaemonEndpoints: () => Promise.resolve({ endpoints: [], error: null }),
        discoverDebuggerProxyTargets: () =>
          Promise.resolve([
            {
              adapterType: '_android_emulator-5554',
              appId: 'com.example.android',
              id: 'proxy-only',
              metadata: { deviceId: 'emulator-5554' },
              webSocketDebuggerUrl: 'ws://127.0.0.1:9010/android/proxy-only',
            },
          ]),
        probeDebuggerProxy: () => Promise.resolve(true),
      },
    });

    const registry = await requestApi(
      debuggerServer,
      new URL('/api/devtools/targets', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    const target = (JSON.parse(registry.body) as { targets: Array<Record<string, unknown>> }).targets[0];
    if (!target) throw new Error('Expected a proxy-only debugger target.');
    const resolved = await requestApi(
      debuggerServer,
      new URL(
        `/api/devtools/target?targetId=${encodeURIComponent(String(target['id']))}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    const snapshot = await requestApi(
      debuggerServer,
      new URL(
        `/api/devtools/snapshot?targetId=${encodeURIComponent(String(target['id']))}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );

    expect(target).toEqual(
      jasmine.objectContaining({
        attachable: false,
        identityMode: 'target-id',
        transport: 'chromium-cdp',
      }),
    );
    expect(resolved.statusCode).toBe(400);
    expect(snapshot.statusCode).toBe(400);
    expect(JSON.parse(resolved.body)).toEqual({
      error: 'The selected debugger target cannot be attached by target ID.',
    });
  });

  it('refreshes the target registry without changing active web-preview performance ownership', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const inspectedUrl = `${applicationUrl}?valdiDevTools=1`;
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: false,
    });
    debuggerServer = await startDebuggerServer({
      assetRoot,
      chromiumDebuggingPort: chromium.port,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      targetDiscovery: targetDiscoveryFor(() => []),
      webPreviewUrl: applicationUrl,
    });
    const debuggerServerUrl = debuggerServer.url;
    const traceUrl = (pathname: string): string => {
      const url = new URL(pathname, debuggerServerUrl);
      url.searchParams.set('inspectedUrl', inspectedUrl);
      url.searchParams.set('sessionId', 'web-preview');
      url.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);
      return url.toString();
    };
    const postOptions: HttpRequestOptions = {
      body: '{}',
      headers: { 'Content-Type': 'application/json' },
      method: 'POST',
    };
    try {
      const started = await requestApi(debuggerServer, traceUrl('/api/devtools/performance/trace/start'), postOptions);
      const registry = await requestApi(
        debuggerServer,
        new URL('/api/devtools/targets', debuggerServer.url).toString(),
        GET_REQUEST_OPTIONS,
      );
      const status = await requestApi(
        debuggerServer,
        traceUrl('/api/devtools/performance/trace/status'),
        GET_REQUEST_OPTIONS,
      );
      const targetIdShortcut = new URL(traceUrl('/api/devtools/performance/trace/status'));
      targetIdShortcut.searchParams.set('targetId', 'owl:web-preview');
      const rejectedShortcut = await requestApi(debuggerServer, targetIdShortcut.toString(), GET_REQUEST_OPTIONS);
      const stopped = await requestApi(debuggerServer, traceUrl('/api/devtools/performance/trace/stop'), postOptions);

      expect(started.statusCode).withContext(started.body).toBe(200);
      expect(registry.statusCode).toBe(200);
      expect((JSON.parse(status.body) as { recording: boolean }).recording)
        .withContext(status.body)
        .toBeTrue();
      expect(rejectedShortcut.statusCode).toBe(400);
      expect(stopped.statusCode).withContext(stopped.body).toBe(200);
    } finally {
      await chromium.close();
    }
  });

  it('requires the exact session, inspected URL, and nonce for web preview performance routes', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html?tenant=alpha';
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      webPreviewUrl: applicationUrl,
      chromiumDebuggingPort: 9333,
    });
    const statusUrl = new URL('/api/devtools/performance/trace/status', debuggerServer.url);
    statusUrl.searchParams.set('inspectedUrl', `${applicationUrl}&valdiDevTools=1`);
    statusUrl.searchParams.set('sessionId', 'owl:web-preview');
    statusUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);
    const aliasedSession = await requestApi(debuggerServer, statusUrl.toString(), GET_REQUEST_OPTIONS);
    statusUrl.searchParams.set('sessionId', 'web-preview');
    statusUrl.searchParams.set('inspectedUrl', 'http://127.0.0.1:54321/other.html');
    const wrongUrl = await requestApi(debuggerServer, statusUrl.toString(), GET_REQUEST_OPTIONS);
    statusUrl.searchParams.set('inspectedUrl', `${applicationUrl}&valdiDevTools=1`);
    statusUrl.searchParams.delete('targetNonce');
    const missingNonce = await requestApi(debuggerServer, statusUrl.toString(), GET_REQUEST_OPTIONS);
    const duplicateResults: HttpResult[] = [];
    for (const [name, value] of [
      ['sessionId', 'conflicting-session'],
      ['inspectedUrl', 'http://127.0.0.1:54321/other.html'],
      ['targetNonce', 'conflicting-nonce-123456'],
    ] as const) {
      const duplicateUrl = new URL('/api/devtools/performance/trace/status', debuggerServer.url);
      duplicateUrl.searchParams.append('sessionId', 'web-preview');
      duplicateUrl.searchParams.append('inspectedUrl', `${applicationUrl}&valdiDevTools=1`);
      duplicateUrl.searchParams.append('targetNonce', WEB_PREVIEW_NONCE);
      duplicateUrl.searchParams.append(name, value);
      duplicateResults.push(await requestApi(debuggerServer, duplicateUrl.toString(), GET_REQUEST_OPTIONS));
    }

    expect(aliasedSession.statusCode).toBe(404);
    expect(JSON.parse(aliasedSession.body)).toEqual({
      error: 'The inspected web preview session is no longer available.',
    });
    expect(wrongUrl.statusCode).toBe(404);
    expect(JSON.parse(wrongUrl.body)).toEqual({
      error: 'The inspected page does not match the configured Valdi web preview target.',
    });
    expect(missingNonce.statusCode).toBe(400);
    expect(JSON.parse(missingNonce.body)).toEqual({
      error: 'targetNonce must appear exactly once for web preview performance requests.',
    });
    expect(duplicateResults.map(result => result.statusCode)).toEqual([400, 400, 400]);
    expect(duplicateResults.map(result => (JSON.parse(result.body) as { error: string }).error)).toEqual([
      'sessionId must appear exactly once for web preview performance requests.',
      'inspectedUrl must appear exactly once for web preview performance requests.',
      'targetNonce must appear exactly once for web preview performance requests.',
    ]);
  });

  it('serves bounded Chromium snapshots and traces on the isolated web preview routes', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const inspectedUrl = `${applicationUrl}?valdiDevTools=1`;
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: false,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      const route = (pathName: string): URL => {
        const url = new URL(pathName, debuggerServer?.url);
        url.searchParams.set('inspectedUrl', inspectedUrl);
        url.searchParams.set('sessionId', 'web-preview');
        url.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);
        return url;
      };

      const snapshot = await requestApi(
        debuggerServer,
        route('/api/devtools/performance/snapshot').toString(),
        GET_REQUEST_OPTIONS,
      );
      const captureResult = await requestApi(
        debuggerServer,
        route('/api/devtools/performance/trace/capture').toString(),
        {
          body: JSON.stringify({ durationMs: 100 }),
          headers: { 'Content-Type': 'application/json' },
          method: 'POST',
        },
      );
      const capture = JSON.parse(captureResult.body) as Record<string, unknown>;
      const traces = capture['traces'] as Array<Record<string, unknown>>;

      expect(snapshot.statusCode).toBe(200);
      expect(JSON.parse(snapshot.body)).toEqual(
        jasmine.objectContaining({
          mainThread: jasmine.objectContaining({ taskDurationMs: 12 }),
          memory: { totalBytes: 2048, usedBytes: 1024 },
          resourceCount: 4,
          transferSize: 2048,
        }),
      );
      expect(captureResult.statusCode).toBe(200);
      expect(Buffer.byteLength(captureResult.body, 'utf8')).toBeLessThanOrEqual(MAX_TRACE_HTTP_RESPONSE_BYTES);
      expect(traces.map(trace => trace['trace'])).toEqual([
        'Valdi.Renderer.onRender.Example',
        'Browser.Layout.Layout',
        'Browser.MainThread.Task',
      ]);
      expect(capture['browserMetrics']).toEqual(jasmine.objectContaining({ LayoutCount: 2, TaskDurationMs: 12 }));
      expect(capture['browserSummary']).toEqual(
        jasmine.objectContaining({ browserEventCount: 2, longTaskCount: 1, rendererEventCount: 1 }),
      );
      expect(capture['rawTraceEvents']).toBeUndefined();
      expect(capture['perfetto']).toBeUndefined();
      expect(capture['perfettoMetadata']).toEqual(jasmine.any(Object));
      expect(chromium.methods).toContain('Tracing.start');
      expect(chromium.methods).toContain('Tracing.end');
    } finally {
      await chromium.close();
    }
  });

  it('does not let a second same-URL tab steal a live performance trace', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const inspectedUrl = `${applicationUrl}?valdiDevTools=1`;
    const competingNonce = 'server-competing-nonce-654321';
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: false,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      const route = (pathName: string, targetNonce: string): URL => {
        const url = new URL(pathName, debuggerServer?.url);
        url.searchParams.set('inspectedUrl', inspectedUrl);
        url.searchParams.set('sessionId', 'web-preview');
        url.searchParams.set('targetNonce', targetNonce);
        return url;
      };
      const post = (url: URL): Promise<HttpResult> =>
        requestApi(debuggerServer, url.toString(), {
          body: '{}',
          headers: { 'Content-Type': 'application/json' },
          method: 'POST',
        });

      const started = await post(route('/api/devtools/performance/trace/start', WEB_PREVIEW_NONCE));
      const competing = await post(route('/api/devtools/performance/trace/start', competingNonce));

      expect(started.statusCode).toBe(200);
      expect(competing.statusCode).toBe(500);
      expect((JSON.parse(competing.body) as { error: string }).error).toContain(
        'Another inspected web preview owns the current Chromium performance trace.',
      );
      expect(chromium.methods.filter(method => method === 'Tracing.start').length).toBe(1);
      expect(chromium.methods).not.toContain('Tracing.end');
      await post(route('/api/devtools/performance/trace/stop', WEB_PREVIEW_NONCE));
    } finally {
      await chromium.close();
    }
  });

  it('ends an old nonce owner before starting after a verified web-preview reload', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const inspectedUrl = `${applicationUrl}?valdiDevTools=1`;
    const reloadedNonce = 'server-reloaded-nonce-654321';
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: false,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      const route = (pathName: string, targetNonce: string): URL => {
        const url = new URL(pathName, debuggerServer?.url);
        url.searchParams.set('inspectedUrl', inspectedUrl);
        url.searchParams.set('sessionId', 'web-preview');
        url.searchParams.set('targetNonce', targetNonce);
        return url;
      };
      const post = (url: URL): Promise<HttpResult> =>
        requestApi(debuggerServer, url.toString(), {
          body: '{}',
          headers: { 'Content-Type': 'application/json' },
          method: 'POST',
        });

      const initialStart = await post(route('/api/devtools/performance/trace/start', WEB_PREVIEW_NONCE));
      expect(initialStart.statusCode).toBe(200);
      chromium.setTargetNonce(reloadedNonce);
      const reloadedStart = await post(route('/api/devtools/performance/trace/start', reloadedNonce));
      expect(reloadedStart.statusCode).toBe(200);

      const startIndexes = chromium.methods
        .map((method, index) => (method === 'Tracing.start' ? index : -1))
        .filter(index => index >= 0);
      const endIndex = chromium.methods.indexOf('Tracing.end');
      expect(startIndexes.length).toBe(2);
      expect(endIndex).toBeGreaterThan(startIndexes[0] ?? -1);
      expect(startIndexes[1]).toBeGreaterThan(endIndex);
      await post(route('/api/devtools/performance/trace/stop', reloadedNonce));
    } finally {
      await chromium.close();
    }
  });

  it('recovers when the same nonce navigates away from the owner inspected URL', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const oldInspectedUrl = `${applicationUrl}?valdiDevTools=1#old`;
    const newInspectedUrl = `${applicationUrl}?valdiDevTools=1#new`;
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: false,
    });
    chromium.setInspectedUrl(oldInspectedUrl);
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      const route = (pathName: string, inspectedPageUrl: string): URL => {
        const url = new URL(pathName, debuggerServer?.url);
        url.searchParams.set('inspectedUrl', inspectedPageUrl);
        url.searchParams.set('sessionId', 'web-preview');
        url.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);
        return url;
      };
      const post = (url: URL): Promise<HttpResult> =>
        requestApi(debuggerServer, url.toString(), {
          body: '{}',
          headers: { 'Content-Type': 'application/json' },
          method: 'POST',
        });

      const initialStart = await post(route('/api/devtools/performance/trace/start', oldInspectedUrl));
      expect(initialStart.statusCode).toBe(200);
      chromium.setInspectedUrl(newInspectedUrl);
      const navigatedStart = await post(route('/api/devtools/performance/trace/start', newInspectedUrl));
      expect(navigatedStart.statusCode).toBe(200);

      expect(chromium.methods.filter(method => method === 'Tracing.start').length).toBe(2);
      expect(chromium.methods).toContain('Tracing.end');
      await post(route('/api/devtools/performance/trace/stop', newInspectedUrl));
    } finally {
      await chromium.close();
    }
  });

  it('retains fail-closed ownership when a Tracing.start response is lost', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      dropTracingStartResponse: true,
      holdRuntimeEnable: false,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      const startUrl = new URL('/api/devtools/performance/trace/start', debuggerServer.url);
      startUrl.searchParams.set('inspectedUrl', `${applicationUrl}?valdiDevTools=1`);
      startUrl.searchParams.set('sessionId', 'web-preview');
      startUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);
      const post = (): Promise<HttpResult> =>
        requestApi(debuggerServer, startUrl.toString(), {
          body: '{}',
          headers: { 'Content-Type': 'application/json' },
          method: 'POST',
        });

      const first = await post();
      const second = await post();

      expect(first.statusCode).toBe(500);
      expect(second.statusCode).toBe(500);
      expect((JSON.parse(second.body) as { error: string }).error).toContain(
        'Best-effort Chromium trace cleanup also failed',
      );
      expect(chromium.methods.filter(method => method === 'Tracing.start').length).toBe(1);
    } finally {
      await chromium.close();
    }
  });

  it('does not end a trace after Chromium definitively rejects Tracing.start', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: false,
      rejectTracingStart: true,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      const startUrl = new URL('/api/devtools/performance/trace/start', debuggerServer.url);
      startUrl.searchParams.set('inspectedUrl', `${applicationUrl}?valdiDevTools=1`);
      startUrl.searchParams.set('sessionId', 'web-preview');
      startUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);

      const result = await requestApi(debuggerServer, startUrl.toString(), {
        body: '{}',
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
      });

      expect(result.statusCode).toBe(500);
      expect((JSON.parse(result.body) as { error: string }).error).toContain(
        'Tracing is already started by another client.',
      );
      expect(chromium.methods.filter(method => method === 'Tracing.start').length).toBe(1);
      expect(chromium.methods).not.toContain('Tracing.end');
    } finally {
      await chromium.close();
    }
  });

  it('handles a socket close during Tracing.end without an unhandled rejection', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      closeOnTracingEnd: true,
      holdRuntimeEnable: false,
    });
    const unhandledRejections: unknown[] = [];
    const onUnhandledRejection = (reason: unknown): void => {
      unhandledRejections.push(reason);
    };
    process.on('unhandledRejection', onUnhandledRejection);
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      const route = (pathName: string): URL => {
        const url = new URL(pathName, debuggerServer?.url);
        url.searchParams.set('inspectedUrl', `${applicationUrl}?valdiDevTools=1`);
        url.searchParams.set('sessionId', 'web-preview');
        url.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);
        return url;
      };
      const post = (url: URL): Promise<HttpResult> =>
        requestApi(debuggerServer, url.toString(), {
          body: '{}',
          headers: { 'Content-Type': 'application/json' },
          method: 'POST',
        });

      const started = await post(route('/api/devtools/performance/trace/start'));
      const stopped = await post(route('/api/devtools/performance/trace/stop'));
      expect(started.statusCode).toBe(200);
      expect(stopped.statusCode).toBe(500);
      await new Promise<void>(resolve => setImmediate(resolve));
      expect(unhandledRejections).toEqual([]);
    } finally {
      process.off('unhandledRejection', onUnhandledRejection);
      await chromium.close();
    }
  });

  it('ends an owned trace before rejecting a target identity that changed during capture', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: false,
      rejectIdentityAfterTracingStart: true,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      const captureUrl = new URL('/api/devtools/performance/trace/capture', debuggerServer.url);
      captureUrl.searchParams.set('inspectedUrl', `${applicationUrl}?valdiDevTools=1`);
      captureUrl.searchParams.set('sessionId', 'web-preview');
      captureUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);

      const result = await requestApi(debuggerServer, captureUrl.toString(), {
        body: JSON.stringify({ durationMs: 100 }),
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
      });

      expect(result.statusCode).toBe(500);
      expect((JSON.parse(result.body) as { error: string }).error).toContain(
        'inspected web preview changed while the performance request was running',
      );
      const endIndex = chromium.methods.lastIndexOf('Tracing.end');
      const validationIndex = chromium.methods.lastIndexOf('Runtime.evaluate');
      expect(endIndex).toBeGreaterThan(-1);
      expect(validationIndex).toBeGreaterThan(endIndex);
    } finally {
      await chromium.close();
    }
  });

  it('enables renderer trace markers only through the exact web preview performance route', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html';
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: false,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      const enableUrl = new URL('/api/devtools/performance/trace/enable', debuggerServer.url);
      enableUrl.searchParams.set('inspectedUrl', `${applicationUrl}?valdiDevTools=1`);
      enableUrl.searchParams.set('sessionId', 'web-preview');
      enableUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);

      const result = await requestApi(debuggerServer, enableUrl.toString(), {
        body: '{}',
        headers: { 'Content-Type': 'application/json' },
        method: 'POST',
      });
      const payload = JSON.parse(result.body) as { inspectedUrl: string };

      expect(result.statusCode).toBe(200);
      expect(payload.inspectedUrl).toContain('valdiDevTools=1');
      expect(payload.inspectedUrl).toContain('valdiTrace=chrome');
      expect(chromium.methods).toContain('Page.navigate');
    } finally {
      await chromium.close();
    }
  });

  it('rejects console streams that do not carry the exact selected target tuple', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      webPreviewUrl: 'http://127.0.0.1:54321/index.html?tenant=alpha',
      chromiumDebuggingPort: 9333,
    });

    const missingSession = await requestApi(
      debuggerServer,
      new URL(
        `/api/devtools/console/stream?sessionId=missing&inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3Ftenant%3Dalpha&targetNonce=${WEB_PREVIEW_NONCE}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    const wrongPage = await requestApi(
      debuggerServer,
      new URL(
        `/api/devtools/console/stream?sessionId=web-preview&inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Fother.html&targetNonce=${WEB_PREVIEW_NONCE}`,
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );
    const missingNonce = await requestApi(
      debuggerServer,
      new URL(
        '/api/devtools/console/stream?sessionId=web-preview&inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3Ftenant%3Dalpha',
        debuggerServer.url,
      ).toString(),
      GET_REQUEST_OPTIONS,
    );

    expect(missingSession.statusCode).toBe(404);
    expect(JSON.parse(missingSession.body)).toEqual({
      error: 'The configured web preview debugger target is not available.',
    });
    expect(wrongPage.statusCode).toBe(404);
    expect(JSON.parse(wrongPage.body)).toEqual({
      error: 'The inspected page does not match the configured Valdi web preview target.',
    });
    expect(missingNonce.statusCode).toBe(400);
    expect(JSON.parse(missingNonce.body)).toEqual({
      error: 'DevTools target discovery requires a valid inspected-tab nonce.',
    });
  });

  it('streams bounded selected-target output and releases Chromium when the SSE client disconnects', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html?tenant=alpha';
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: false,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      const streamUrl = new URL('/api/devtools/console/stream', debuggerServer.url);
      streamUrl.searchParams.set('inspectedUrl', `${applicationUrl}&valdiDevTools=1`);
      streamUrl.searchParams.set('sessionId', 'web-preview');
      streamUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);

      const entries = await readSseEvents(
        eventSourceUrl(debuggerServer, streamUrl.toString()),
        'console',
        2,
        () => {},
      );

      expect(entries).toEqual([
        jasmine.objectContaining({
          level: 'warn',
          message: 'Synthetic <renderer> output',
          sessionId: 'web-preview',
          source: 'console',
          targetId: 'owl:web-preview',
        }),
        jasmine.objectContaining({
          level: 'error',
          message: 'authorization: [REDACTED]',
          sessionId: 'web-preview',
          source: 'browser',
          targetId: 'owl:web-preview',
        }),
      ]);
      expect(JSON.stringify(entries)).not.toContain('synthetic-private-token');
      expect(chromium.methods.slice(0, 4)).toEqual([
        'Runtime.evaluate',
        'Runtime.enable',
        'Log.enable',
        'Runtime.evaluate',
      ]);

      const deadline = Date.now() + 1000;
      while (chromium.debuggerSockets.size > 0 && Date.now() < deadline) {
        await new Promise<void>(resolve => setTimeout(resolve, 10));
      }
      expect(chromium.debuggerSockets.size).toBe(0);
    } finally {
      await chromium.close();
    }
  });

  it('releases Chromium when the SSE client aborts while Runtime.enable is pending', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html?tenant=abort-during-enable';
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: true,
    });
    try {
      debuggerServer = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      const streamUrl = new URL('/api/devtools/console/stream', debuggerServer.url);
      streamUrl.searchParams.set('inspectedUrl', `${applicationUrl}&valdiDevTools=1`);
      streamUrl.searchParams.set('sessionId', 'web-preview');
      streamUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);
      const stream = startStreamingRequest(eventSourceUrl(debuggerServer, streamUrl.toString()), 'GET');
      const streamResult = stream.result.catch(() => null);
      stream.request.end();

      await chromium.runtimeEnableReceived;
      expect(chromium.debuggerSockets.size).toBe(1);
      stream.request.destroy();
      await streamResult;

      const deadline = Date.now() + 1000;
      while (chromium.debuggerSockets.size > 0 && Date.now() < deadline) {
        await new Promise<void>(resolve => setTimeout(resolve, 10));
      }
      expect(chromium.debuggerSockets.size).toBe(0);
    } finally {
      chromium.releaseRuntimeEnable();
      await chromium.close();
    }
  });

  it('closes promptly when debugger shutdown starts while Runtime.enable is pending', async () => {
    const applicationUrl = 'http://127.0.0.1:54321/index.html?tenant=shutdown-during-enable';
    const chromium = await startMockChromiumConsoleServer(applicationUrl, WEB_PREVIEW_NONCE, {
      holdRuntimeEnable: true,
    });
    let chromiumClosed = false;
    try {
      const serverToClose = await startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
        webPreviewUrl: applicationUrl,
        chromiumDebuggingPort: chromium.port,
      });
      debuggerServer = serverToClose;
      const streamUrl = new URL('/api/devtools/console/stream', serverToClose.url);
      streamUrl.searchParams.set('inspectedUrl', `${applicationUrl}&valdiDevTools=1`);
      streamUrl.searchParams.set('sessionId', 'web-preview');
      streamUrl.searchParams.set('targetNonce', WEB_PREVIEW_NONCE);
      const stream = startStreamingRequest(eventSourceUrl(serverToClose, streamUrl.toString()), 'GET');
      const streamResult = stream.result.catch(() => null);
      stream.request.end();

      await chromium.runtimeEnableReceived;
      let shutdownComplete = false;
      debuggerServer = undefined;
      const shutdown = serverToClose.close().then(() => {
        shutdownComplete = true;
      });
      const deadline = Date.now() + 1000;
      while (!shutdownComplete && Date.now() < deadline) {
        await new Promise<void>(resolve => setTimeout(resolve, 10));
      }
      const shutdownCompletedDuringEnable = shutdownComplete;
      const chromiumReleasedDuringEnable = chromium.debuggerSockets.size === 0;

      stream.request.destroy();
      chromium.releaseRuntimeEnable();
      await chromium.close();
      chromiumClosed = true;
      await shutdown;
      await streamResult;

      expect(shutdownCompletedDuringEnable).toBeTrue();
      expect(chromiumReleasedDuringEnable).toBeTrue();
    } finally {
      if (!chromiumClosed) await chromium.close();
    }
  });

  it('requires GET for the integrated Chromium console stream', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      webPreviewUrl: 'http://127.0.0.1:54321/index.html',
    });

    const result = await requestApi(debuggerServer, '/api/devtools/console/stream', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{}',
    });

    expect(result.statusCode).toBe(405);
    expect(JSON.parse(result.body)).toEqual({ error: 'Valdi DevTools console streaming requires GET.' });
  });

  it('requires JSON for executable integrated DevTools routes', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
      webPreviewUrl: 'http://127.0.0.1:54321/index.html',
    });

    const plainText = await requestApi(debuggerServer, '/api/devtools/evaluate', {
      method: 'POST',
      headers: { 'Content-Type': 'text/plain' },
      body: '{"sessionId":"web-preview","expression":"1 + 1"}',
    });
    const json = await requestApi(debuggerServer, '/api/devtools/evaluate', {
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

    const result = await requestApi(debuggerServer, '/api/devtools/target', {
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

  it('does not retain a web preview target when server startup cannot bind', async () => {
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

    await closeServer(occupiedPortServer);
    occupiedPortServer = undefined;
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: occupiedPort,
      strictPort: true,
    });
    const result = await requestApi(
      debuggerServer,
      `/api/devtools/target?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html&targetNonce=${WEB_PREVIEW_NONCE}`,
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
      const outgoingRequest = http.get(eventSourceUrl(serverToClose, '/api/debugger/events'), resolve);
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

  it('validates the distinct UI and runtime service port environments', async () => {
    process.env['VALDI_DEBUGGER_UI_PORT'] = '9100';
    process.env['VALDI_DEBUGGER_SERVICE_PORT'] = '14000';
    expect(resolveDebuggerUiPort()).toBe(9100);
    expect(resolveDebuggerServicePort()).toBe(14000);

    process.env['VALDI_DEBUGGER_UI_PORT'] = '9100suffix';
    expect(() => resolveDebuggerUiPort()).toThrowError(/VALDI_DEBUGGER_UI_PORT must be an integer/);
    process.env['VALDI_DEBUGGER_UI_PORT'] = '9100';
    process.env['VALDI_DEBUGGER_SERVICE_PORT'] = '0';
    expect(() => resolveDebuggerServicePort()).toThrowError(/VALDI_DEBUGGER_SERVICE_PORT must be an integer/);

    process.env['VALDI_DEBUGGER_SERVICE_PORT'] = 'invalid';
    await expectAsync(
      startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
      }),
    ).toBeRejectedWithError(/VALDI_DEBUGGER_SERVICE_PORT must be an integer/);
    delete process.env['VALDI_DEBUGGER_SERVICE_PORT'];
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
  });

  it('uses the configured runtime service port for target discovery', async () => {
    const servicePort = await getFreePort();
    process.env['VALDI_DEBUGGER_SERVICE_PORT'] = String(servicePort);
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await requestApi(debuggerServer, '/api/status', GET_REQUEST_OPTIONS);
    const responseBody = JSON.parse(result.body) as { defaultPort: number; ports: Array<{ port: number }> };

    expect(result.statusCode).toBe(200);
    expect(responseBody.defaultPort).toBe(servicePort);
    expect(responseBody.ports.map(status => status.port)).toEqual([servicePort]);
  });

  it('allows only one debugger server per process and resets the guard after close', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    await expectAsync(
      startDebuggerServer({
        assetRoot,
        host: '127.0.0.1',
        port: await getFreePort(),
        strictPort: true,
      }),
    ).toBeRejectedWithError(/already starting or running/);
    expect((await request(debuggerServer.url, GET_REQUEST_OPTIONS)).statusCode).toBe(200);

    await debuggerServer.close();
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    expect((await request(debuggerServer.url, GET_REQUEST_OPTIONS)).statusCode).toBe(200);
  });

  it('requires the per-server token for API GET, POST, and event streams', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const missingGet = await request(new URL('/api/status', debuggerServer.url).toString(), GET_REQUEST_OPTIONS);
    const wrongGet = await request(new URL('/api/status', debuggerServer.url).toString(), {
      ...GET_REQUEST_OPTIONS,
      headers: { [debuggerServer.apiTokenHeader]: 'wrong-token' },
    });
    const queryOnNonStream = new URL('/api/status', debuggerServer.url);
    queryOnNonStream.searchParams.set('valdiDebuggerToken', debuggerServer.apiToken);
    const queryGet = await request(queryOnNonStream.toString(), GET_REQUEST_OPTIONS);
    const missingPost = await request(new URL('/api/debugger/actions', debuggerServer.url).toString(), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'setAutoRefresh', params: { enabled: true } }),
    });
    const missingStream = await request(
      new URL('/api/debugger/events', debuggerServer.url).toString(),
      GET_REQUEST_OPTIONS,
    );
    const wrongStream = new URL('/api/debugger/events', debuggerServer.url);
    wrongStream.searchParams.set('valdiDebuggerToken', 'wrong-token');
    const wrongStreamResult = await request(wrongStream.toString(), GET_REQUEST_OPTIONS);

    expect(missingGet.statusCode).toBe(401);
    expect(wrongGet.statusCode).toBe(401);
    expect(queryGet.statusCode).toBe(401);
    expect(missingPost.statusCode).toBe(401);
    expect(missingStream.statusCode).toBe(401);
    expect(wrongStreamResult.statusCode).toBe(401);
    expect(missingGet.body).not.toContain(debuggerServer.apiToken);

    const validPost = await requestApi(debuggerServer, '/api/debugger/actions', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'setAutoRefresh', params: { enabled: true } }),
    });
    const readyEvents = await readSseEvents(
      eventSourceUrl(debuggerServer, '/api/debugger/events'),
      'ready',
      1,
      () => undefined,
    );
    expect(validPost.statusCode).toBe(200);
    expect(readyEvents).toHaveSize(1);
  });

  it('lets an explicit stop interrupt a timed operation and shares one completion', async () => {
    let completionCount = 0;
    const operation = createInterruptibleOperation(60_000, async () => {
      completionCount += 1;
      return 'stopped';
    });

    const [stopResult, timedResult] = await Promise.all([operation.stop(), operation.result]);

    expect(stopResult).toBe('stopped');
    expect(timedResult).toBe('stopped');
    expect(completionCount).toBe(1);
  });

  it('redacts source URLs from CPU profile summaries without changing the raw profile', () => {
    const profile: CpuProfile = {
      nodes: [
        {
          id: 1,
          callFrame: {
            functionName: 'renderSecret',
            scriptId: '1',
            url: 'file:///Users/example/private/app.ts',
            lineNumber: 42,
            columnNumber: 1,
          },
          hitCount: 1,
        },
      ],
      startTime: 1_000,
      endTime: 2_000,
      samples: [1],
    };

    const summary = summarizeCpuProfile(profile) as { topFunctions: Array<Record<string, unknown>> };

    expect(summary.topFunctions[0]).toEqual({ name: 'renderSecret', lineNumber: 42, sampleCount: 1 });
    expect(profile.nodes[0]?.callFrame.url).toBe('file:///Users/example/private/app.ts');
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
        [debuggerServer.apiTokenHeader]: debuggerServer.apiToken,
      },
      body: JSON.stringify({ action: 'setAutoRefresh', params: { enabled: true } }),
    });

    expect(result.statusCode).toBe(200);
    const responseBody = JSON.parse(result.body) as { state: { autoRefresh: boolean } };
    expect(responseBody.state.autoRefresh).toBeTrue();
  });

  it('advertises debugger provider and published settings actions', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const result = await requestApi(debuggerServer, '/api/debugger/state', GET_REQUEST_OPTIONS);
    const responseBody = JSON.parse(result.body) as { capabilities: { actions: string[] } };

    expect(result.statusCode).toBe(200);
    expect(responseBody.capabilities.actions).toContain('refreshDebuggerProviders');
    expect(responseBody.capabilities.actions).toContain('refreshDebugSettings');
    expect(responseBody.capabilities.actions).toContain('setDebugSetting');
    expect(responseBody.capabilities.actions).toContain('resetDebugSetting');
  });

  it('reports debugger providers and settings as unavailable when no target backend exists', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const unavailableTargetPort = await getFreePort();

    const providers = await requestApi(
      debuggerServer,
      `/api/debugger/providers?port=${unavailableTargetPort}&clientId=1&contextId=test`,
      GET_REQUEST_OPTIONS,
    );
    const settings = await requestApi(
      debuggerServer,
      `/api/debugger/settings?port=${unavailableTargetPort}&clientId=1&contextId=test`,
      GET_REQUEST_OPTIONS,
    );
    const providersBody = JSON.parse(providers.body) as {
      handled: boolean;
      identifier: string;
      target: unknown;
      data: Record<string, unknown>;
      error: string | null;
      status: string;
    };
    const settingsBody = JSON.parse(settings.body) as typeof providersBody;

    expect(providers.statusCode).toBe(200);
    expect(providersBody.handled).toBeFalse();
    expect(providersBody.identifier).toBe('ValdiDebuggerProviders');
    expect(providersBody.target).toBeNull();
    expect(providersBody.data).toEqual({});
    expect(providersBody.error).not.toBeNull();
    expect(providersBody.status).toBe('unavailable');
    expect(settings.statusCode).toBe(200);
    expect(settingsBody.handled).toBeFalse();
    expect(settingsBody.identifier).toBe('ValdiDebuggerSettings');
    expect(settingsBody.target).toBeNull();
    expect(settingsBody.data).toEqual({});
    expect(settingsBody.error).not.toBeNull();
    expect(settingsBody.status).toBe('unavailable');
  });

  it('lists and dispatches an independently registered generic provider through the daemon wire contract', async () => {
    mockDaemon = await startMockDaemon();
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const targetQuery = `port=${mockDaemon.port.toString()}&clientId=1&contextId=mock-context`;

    const list = await requestApi(
      debuggerServer,
      `/api/debugger/providers?${targetQuery}`,
      GET_REQUEST_OPTIONS,
    );
    const dispatch = await requestApi(
      debuggerServer,
      `/api/debugger/providers/request?${targetQuery}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          action: 'snapshot',
          params: { action: 'cannot-override', limit: 25 },
          providerId: 'independent',
        }),
      },
    );
    const listBody = JSON.parse(list.body) as {
      data: { providers: Array<{ id: string }> };
      status: string;
    };
    const dispatchBody = JSON.parse(dispatch.body) as {
      data: { data: { request: { action: string; limit: number } } };
      status: string;
    };

    expect(list.statusCode).toBe(200);
    expect(listBody.status).toBe('handled');
    expect(listBody.data.providers).toEqual([jasmine.objectContaining({ id: 'independent' })]);
    expect(dispatch.statusCode).toBe(200);
    expect(dispatchBody.status).toBe('handled');
    expect(dispatchBody.data.data.request).toEqual({ action: 'snapshot', limit: 25 });
    expect(mockDaemon.requests.filter(item => item['type'] === 1000).length).toBe(2);
  });

  it('rejects unbounded or ambiguous generic provider request fields before daemon dispatch', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const targetQuery = 'port=13591&clientId=1&contextId=mock-context';
    const oversizedAction = await requestApi(
      debuggerServer,
      `/api/debugger/providers/request?${targetQuery}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action: 'x'.repeat(129), params: {}, providerId: 'independent' }),
      },
    );
    const tooManyParams = await requestApi(
      debuggerServer,
      `/api/debugger/providers/request?${targetQuery}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          action: 'snapshot',
          params: Object.fromEntries(Array.from({ length: 33 }, (_, index) => [`key${index.toString()}`, index])),
          providerId: 'independent',
        }),
      },
    );
    const invalidTarget = await requestApi(
      debuggerServer,
      '/api/debugger/providers?port=65536&clientId=1&contextId=mock-context',
      GET_REQUEST_OPTIONS,
    );

    expect(oversizedAction.statusCode).toBe(400);
    expect(tooManyParams.statusCode).toBe(400);
    expect(invalidTarget.statusCode).toBe(400);
  });

  it('distinguishes a malformed custom response from an unavailable target', async () => {
    mockDaemon = await startMockDaemon({ handled: 'yes', data: {} });
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const result = await requestApi(
      debuggerServer,
      `/api/debugger/providers?port=${mockDaemon.port.toString()}&clientId=1&contextId=mock-context`,
      GET_REQUEST_OPTIONS,
    );
    const body = JSON.parse(result.body) as { error: string; handled: boolean; status: string };

    expect(result.statusCode).toBe(200);
    expect(body.handled).toBeFalse();
    expect(body.status).toBe('protocol-error');
    expect(body.error).toContain('boolean handled field');
  });

  it('reports a truthful unsupported status when the target declines the generic contract', async () => {
    mockDaemon = await startMockDaemon({ handled: false });
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const result = await requestApi(
      debuggerServer,
      `/api/debugger/providers?port=${mockDaemon.port.toString()}&clientId=1&contextId=mock-context`,
      GET_REQUEST_OPTIONS,
    );
    const body = JSON.parse(result.body) as {
      data: Record<string, unknown>;
      handled: boolean;
      status: string;
      target: Record<string, unknown>;
    };

    expect(result.statusCode).toBe(200);
    expect(body.handled).toBeFalse();
    expect(body.status).toBe('unsupported');
    expect(body.data).toEqual({});
    expect(body.target).toEqual(jasmine.objectContaining({ clientId: '1', contextId: 'mock-context' }));
  });

  it('rejects malformed debugger provider and published settings requests', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const providerRequest = await requestApi(debuggerServer, '/api/debugger/providers/request', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ providerId: 'storage' }),
    });
    const settingsRequest = await requestApi(debuggerServer, '/api/debugger/settings', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'writeArbitraryData' }),
    });

    expect(providerRequest.statusCode).toBe(400);
    expect((JSON.parse(providerRequest.body) as { error: string }).error).toContain('action must be');
    expect(settingsRequest.statusCode).toBe(400);
    expect((JSON.parse(settingsRequest.body) as { error: string }).error).toContain('at most 16 characters');
  });

  it('enforces provider HTTP methods and shared JSON body status codes', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });
    const targetQuery = 'port=13591&clientId=1&contextId=test';
    const listPost = await requestApi(debuggerServer, `/api/debugger/providers?${targetQuery}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{}',
    });
    const malformed = await requestApi(
      debuggerServer,
      `/api/debugger/providers/request?${targetQuery}`,
      { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{not-json}' },
    );
    const nonObject = await requestApi(
      debuggerServer,
      `/api/debugger/providers/request?${targetQuery}`,
      { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '[]' },
    );
    const oversized = await requestApi(
      debuggerServer,
      `/api/debugger/providers/request?${targetQuery}`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: `{"value":"${'x'.repeat(1024 * 1024)}"}`,
      },
    );

    expect(listPost.statusCode).toBe(405);
    expect(malformed.statusCode).toBe(400);
    expect(nonObject.statusCode).toBe(400);
    expect(oversized.statusCode).toBe(413);
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
      headers: {
        'Content-Type': 'application/json',
        [debuggerServer.apiTokenHeader]: debuggerServer.apiToken,
      },
      body: JSON.stringify({ action: 'typoAction', params: {} }),
    });
    const invalidPort = await request(actionsUrl, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        [debuggerServer.apiTokenHeader]: debuggerServer.apiToken,
      },
      body: JSON.stringify({ action: 'setPort', params: { port: 70_000 } }),
    });
    const coercedPort = await requestApi(debuggerServer, actionsUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'setPort', params: { port: '13591' } }),
    });
    const invalidParams = await requestApi(debuggerServer, actionsUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'refreshSnapshot', params: [] }),
    });

    expect(unknown.statusCode).toBe(400);
    expect((JSON.parse(unknown.body) as { error: string }).error).toContain('Unknown debugger action');
    expect(invalidPort.statusCode).toBe(400);
    expect((JSON.parse(invalidPort.body) as { error: string }).error).toContain('between 1 and 65535');
    expect(coercedPort.statusCode).toBe(400);
    expect(invalidParams.statusCode).toBe(400);
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
      const result = await requestApi(debuggerServer, inputUrl, {
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
      const result = await requestApi(debuggerServer, inputCase.url, {
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

    const unsupportedMethod = await requestApi(debuggerServer, inputUrl, GET_REQUEST_OPTIONS);
    expect(unsupportedMethod.statusCode).toBe(405);
    expect((JSON.parse(unsupportedMethod.body) as { error: string }).error).toBe('Input dispatch requires POST.');

    const malformedJson = await requestApi(debuggerServer, inputUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{',
    });
    expect(malformedJson.statusCode).toBe(400);
    expect((JSON.parse(malformedJson.body) as { error: string }).error).toBe('Request body must contain valid JSON.');

    for (const body of ['null', '[]', '"input"']) {
      const malformedBody = await requestApi(debuggerServer, inputUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body,
      });
      expect(malformedBody.statusCode).toBe(400);
      expect((JSON.parse(malformedBody.body) as { error: string }).error).toBe('Request body must be a JSON object.');
    }

    const oversizedBody = await requestApi(debuggerServer, inputUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ padding: 'a'.repeat(1024 * 1024) }),
    });
    expect(oversizedBody.statusCode).toBe(413);
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
      [debuggerServer.apiTokenHeader]: debuggerServer.apiToken,
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
      [debuggerServer.apiTokenHeader]: debuggerServer.apiToken,
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
    const first = startStreamingRequest(profileUrl, 'POST', {
      'Content-Type': 'application/json',
      [debuggerServer.apiTokenHeader]: debuggerServer.apiToken,
    });
    first.request.write('{');
    await new Promise<void>(resolve => setTimeout(resolve, 20));

    const second = await request(profileUrl, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        [debuggerServer.apiTokenHeader]: debuggerServer.apiToken,
      },
      body: '{}',
    });
    first.request.end('invalid');
    await first.result;

    expect(second.statusCode).toBe(500);
    expect((JSON.parse(second.body) as { error: string }).error).toContain('transition is already in progress');
  });

  it('enforces HTTP methods for renderer trace routes before contacting a target', async () => {
    debuggerServer = await startDebuggerServer({
      assetRoot,
      host: '127.0.0.1',
      port: await getFreePort(),
      strictPort: true,
    });

    const status = await requestApi(debuggerServer, '/api/performance/trace/status', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{}',
    });
    const start = await requestApi(debuggerServer, '/api/performance/trace/start', GET_REQUEST_OPTIONS);

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

    const result = await requestApi(debuggerServer, '/api/performance/trace/start', {
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
    expect(fabricatedInstant).toEqual(jasmine.objectContaining({ ph: 'i', s: 't' }));
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
        webPreviewTrace: true,
        browserMetrics: {
          LayoutCount: 10,
          PrivateMetric: 1,
          ScriptDurationMs: 20,
          TaskDurationMs: 30,
        },
      },
      { contextId: '\0'.repeat(100_000), name: '\0'.repeat(100_000), port: 13_591 },
    );
    const serialized = JSON.stringify(result);

    expect(Buffer.byteLength(serialized, 'utf8')).toBeLessThanOrEqual(MAX_TRACE_HTTP_RESPONSE_BYTES);
    expect(result['perfetto']).toBeUndefined();
    expect(Array.isArray(result['traces'])).toBeTrue();
    expect(result['traceCount'] as number).toBeLessThan(512);
    expect(result['traceEventLimitReached']).toBeTrue();
    expect(result['browserMetrics']).toEqual({ LayoutCount: 10, ScriptDurationMs: 20, TaskDurationMs: 30 });
    expect((result['browserMetrics'] as Record<string, unknown>)['PrivateMetric']).toBeUndefined();
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

  it('serializes trace transitions per runtime without blocking unrelated clients', async () => {
    const coordinator = new PerformanceTraceCoordinator();
    let releaseFirst!: () => void;
    const firstTransition = coordinator.runTransition(
      '13591\0client-a',
      async () =>
        await new Promise<void>(resolve => {
          releaseFirst = resolve;
        }),
    );

    await expectAsync(coordinator.runTransition('13591\0client-a', () => Promise.resolve())).toBeRejectedWithError(
      Error,
      'Another renderer trace transition is already in progress for this runtime.',
    );

    let unrelatedTransitionRan = false;
    await coordinator.runTransition('13591\0client-b', () => {
      unrelatedTransitionRan = true;
      return Promise.resolve();
    });
    expect(unrelatedTransitionRan).toBeTrue();

    releaseFirst();
    await firstTransition;
  });

  it('lets Stop interrupt a one-shot capture and shares the retained result', async () => {
    const coordinator = new PerformanceTraceCoordinator();
    const actions: PerformanceTraceAction[] = [];
    let reportStarted!: () => void;
    const started = new Promise<void>(resolve => {
      reportStarted = resolve;
    });
    const retainedResult = { recording: false, traces: [{ trace: 'completed' }] };
    const capture = coordinator.capture(
      '13591\0client-a',
      { durationMs: 15_000, rendererTracing: true },
      {
        send: action => {
          actions.push(action);
          if (action === PerformanceTraceAction.Status) {
            return Promise.resolve({
              completedRecordingAvailable: false,
              recording: false,
              tracingSupported: true,
            });
          }
          if (action === PerformanceTraceAction.Start) {
            reportStarted();
            return Promise.resolve({ recording: true });
          }
          return Promise.resolve(retainedResult);
        },
        wait: () => new Promise<void>(() => {}),
      },
    );
    await started;

    let directStopCalled = false;
    const stopped = coordinator.stop('13591\0client-a', () => {
      directStopCalled = true;
      return Promise.resolve({});
    });
    const [captureResult, stopResult] = await Promise.all([capture, stopped]);

    expect(directStopCalled).toBeFalse();
    expect(captureResult).toBe(retainedResult);
    expect(stopResult).toBe(retainedResult);
    expect(actions).toEqual([PerformanceTraceAction.Status, PerformanceTraceAction.Start, PerformanceTraceAction.Stop]);
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

    const result = await requestApi(debuggerServer, '/api/unknown', GET_REQUEST_OPTIONS);

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

    const result = await requestApi(
      debuggerServer,
      '/api/runtime-logs?applicationId=example&platform=ios',
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

    const result = await requestApi(debuggerServer, '/api/runtime-logs', GET_REQUEST_OPTIONS);
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

    const result = await requestApi(
      debuggerServer,
      '/api/runtime-logs?applicationId=configured&platform=ios',
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
      eventSourceUrl(debuggerServer, '/api/runtime-logs/stream?applicationId=example&platform=ios&limit=2'),
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
