import 'jasmine';
import crypto from 'node:crypto';
import http from 'node:http';
import type { AddressInfo, Socket } from 'node:net';
import { Script } from 'node:vm';
import {
  OWL_DEVTOOLS_TARGET_NONCE_PROPERTY,
  OwlChromiumConnection,
  connectToOwlApplication,
  evaluateOwlApplicationExpression,
  listOwlChromiumTargets,
  readOwlDebuggerSnapshot,
} from './owlCdpClient';

interface ChromiumDiscoveryServer {
  closedTargetIds: string[];
  events: string[];
  expressions: string[];
  emitEvent(event: Record<string, unknown>): void;
  port: number;
  server: http.Server;
  setDiscoveryBody(body: string): void;
  setDiscoveryChunkDelay(delayMs: number): void;
  setDiscoveryChunks(chunks: Buffer[]): void;
  setDiscoveryStatus(status: number): void;
  setWebSocketResponder(responder: WebSocketResponder): void;
  sockets: Set<Socket>;
}

interface DecodedWebSocketFrame {
  masked: boolean;
  opcode: number;
  payload: Buffer;
}

type WebSocketResponder = (socket: Socket, frame: DecodedWebSocketFrame, targetId: string) => void;

const TARGET_NONCE = 'owl-devtools-nonce-1234567890';

async function waitForSocketsToClose(sockets: Set<Socket>, timeoutMs: number): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (sockets.size > 0 && Date.now() < deadline) {
    await new Promise<void>(resolve => setTimeout(resolve, 10));
  }
}

function encodeWebSocketServerFrame(opcode: number, payload: Buffer, masked: boolean, finalFrame: boolean): Buffer {
  const maskLength = masked ? 4 : 0;
  const header = payload.length < 126 ? Buffer.alloc(2 + maskLength) : Buffer.alloc(4 + maskLength);
  header[0] = (finalFrame ? 0x80 : 0) | opcode;
  if (payload.length < 126) {
    header[1] = (masked ? 0x80 : 0) | payload.length;
  } else {
    header[1] = (masked ? 0x80 : 0) | 126;
    header.writeUInt16BE(payload.length, 2);
  }
  if (!masked) return Buffer.concat([header, payload]);

  const maskOffset = header.length - 4;
  const mask = Buffer.from([0x01, 0x02, 0x03, 0x04]);
  mask.copy(header, maskOffset);
  const maskedPayload = Buffer.from(payload.map((value, index) => value ^ mask[index % mask.length]!));
  return Buffer.concat([header, maskedPayload]);
}

function encodeWebSocketResponse(payload: unknown): Buffer {
  const serialized = JSON.stringify(payload);
  if (serialized === undefined) throw new Error('Cannot encode an undefined WebSocket test payload.');
  return encodeWebSocketServerFrame(0x01, Buffer.from(serialized, 'utf8'), false, true);
}

function decodeWebSocketFrame(frame: Buffer): DecodedWebSocketFrame {
  const opcode = frame[0]! & 0x0f;
  let offset = 2;
  let payloadLength = frame[1]! & 0x7f;
  if (payloadLength === 126) {
    payloadLength = frame.readUInt16BE(offset);
    offset += 2;
  } else if (payloadLength === 127) {
    payloadLength = Number(frame.readBigUInt64BE(offset));
    offset += 8;
  }
  const masked = (frame[1]! & 0x80) !== 0;
  const mask = masked ? frame.subarray(offset, offset + 4) : null;
  if (mask) offset += mask.length;
  const rawPayload = frame.subarray(offset, offset + payloadLength);
  const payload = mask ? Buffer.from(rawPayload.map((value, index) => value ^ mask[index % mask.length]!)) : rawPayload;
  return { masked, opcode, payload };
}

function decodeWebSocketCommand(frame: DecodedWebSocketFrame): {
  id: number;
  method: string;
  params: { expression?: string };
} {
  if (frame.opcode !== 0x01) throw new Error(`Expected a text command frame, received opcode ${frame.opcode}.`);
  return JSON.parse(frame.payload.toString('utf8')) as {
    id: number;
    method: string;
    params: { expression?: string };
  };
}

function evaluateCommandInPage(socket: Socket, frame: DecodedWebSocketFrame, page: Record<string, unknown>): void {
  const command = decodeWebSocketCommand(frame);
  const result = new Script(command.params.expression ?? '').runInNewContext(page) as unknown;
  void Promise.resolve(result).then(value => {
    socket.write(
      encodeWebSocketResponse({
        id: command.id,
        result: { result: { type: 'object', value } },
      }),
    );
  });
}

function debuggerSnapshotValue(): Record<string, unknown> {
  return {
    channel: 'valdi-web-debugger',
    snapshot: { tree: { id: '4', tag: 'view' }, viewport: { width: 400, height: 300 } },
    source: { title: 'Valdi Owl', url: 'http://127.0.0.1:54321/index.html' },
    type: 'snapshot',
  };
}

async function createChromiumDiscoveryServer(): Promise<ChromiumDiscoveryServer> {
  const expressions: string[] = [];
  const closedTargetIds: string[] = [];
  const closedTargets = new Set<string>();
  const events: string[] = [];
  const debuggerSockets = new Set<Socket>();
  const sockets = new Set<Socket>();
  let discoveryBody: string | null = null;
  let discoveryChunkDelayMs = 0;
  let discoveryChunks: Buffer[] | null = null;
  let discoveryStatus = 200;
  let webSocketResponder: WebSocketResponder | null = null;
  const server = http.createServer((request, response) => {
    if (request.url !== '/json/list') {
      response.writeHead(404).end();
      return;
    }
    const address = server.address() as AddressInfo;
    response.writeHead(discoveryStatus, { 'content-type': 'application/json' });
    const body =
      discoveryBody ??
      JSON.stringify([
        {
          id: 'owl-page',
          title: 'Valdi Owl',
          type: 'page',
          url: 'http://127.0.0.1:54321/index.html?valdiDebugger=1&valdiDevTools=1',
          webSocketDebuggerUrl: `ws://127.0.0.1:${address.port}/devtools/page/owl-page`,
        },
        {
          id: 'redirected-page',
          title: 'Untrusted loopback redirect',
          type: 'page',
          url: 'http://127.0.0.1:54321/index.html',
          webSocketDebuggerUrl: 'ws://127.0.0.1:1/devtools/page/redirected-page',
        },
      ]);
    if (!discoveryChunks) {
      response.end(body);
      return;
    }
    void (async () => {
      for (const chunk of discoveryChunks) {
        if (response.destroyed) break;
        response.write(chunk);
        await new Promise<void>(resolve => setTimeout(resolve, discoveryChunkDelayMs));
      }
      if (!response.destroyed) response.end();
    })();
  });

  server.on('connection', socket => {
    sockets.add(socket);
    socket.once('close', () => sockets.delete(socket));
  });

  server.on('upgrade', (request, socket: Socket) => {
    const key = request.headers['sec-websocket-key'];
    const targetMatch = /^\/devtools\/page\/([^/?]+)$/.exec(request.url ?? '');
    if (!targetMatch || typeof key !== 'string') {
      socket.destroy();
      return;
    }
    const targetId = targetMatch[1]!;
    debuggerSockets.add(socket);
    socket.once('close', () => debuggerSockets.delete(socket));
    events.push(`open:${targetId}`);
    const markTargetClosed = () => {
      if (closedTargets.has(targetId)) return;
      closedTargets.add(targetId);
      closedTargetIds.push(targetId);
      events.push(`close:${targetId}`);
    };
    socket.once('end', markTargetClosed);
    socket.once('close', markTargetClosed);

    const accept = crypto.createHash('sha1').update(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`).digest('base64');
    socket.write(
      `HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\n\r\n`,
    );
    socket.on('data', (frame: Buffer) => {
      const decodedFrame = decodeWebSocketFrame(frame);
      if (decodedFrame.opcode === 0x01) {
        const command = decodeWebSocketCommand(decodedFrame);
        if (command.params.expression !== undefined) expressions.push(command.params.expression);
        events.push(`${command.method}:${targetId}`);
      }
      if (webSocketResponder) {
        webSocketResponder(socket, decodedFrame, targetId);
        return;
      }
      const command = decodeWebSocketCommand(decodedFrame);
      const value = command.params.expression?.includes('__VALDI_WEB_DEBUGGER__') ? debuggerSnapshotValue() : true;
      socket.write(
        encodeWebSocketResponse({
          id: command.id,
          result: {
            result: {
              type: 'object',
              value: { __valdiDevToolsTargetMatched: true, value },
            },
          },
        }),
      );
    });
  });

  return await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      if (address === null || typeof address === 'string') {
        server.close();
        reject(new Error('Could not allocate a mock Owl Chromium discovery port.'));
        return;
      }
      resolve({
        closedTargetIds,
        emitEvent(event: Record<string, unknown>): void {
          for (const socket of debuggerSockets) socket.write(encodeWebSocketResponse(event));
        },
        events,
        expressions,
        port: address.port,
        server,
        setDiscoveryBody: body => {
          discoveryBody = body;
          discoveryChunks = null;
        },
        setDiscoveryChunkDelay: delayMs => {
          discoveryChunkDelayMs = delayMs;
        },
        setDiscoveryChunks: chunks => {
          discoveryChunks = chunks;
        },
        setDiscoveryStatus: status => {
          discoveryStatus = status;
        },
        setWebSocketResponder: responder => {
          webSocketResponder = responder;
        },
        sockets,
      });
    });
  });
}

describe('owlCdpClient', () => {
  let discovery: ChromiumDiscoveryServer;

  beforeEach(async () => {
    discovery = await createChromiumDiscoveryServer();
  });

  afterEach(async () => {
    for (const socket of discovery.sockets) socket.destroy();
    await new Promise<void>((resolve, reject) => {
      discovery.server.close(error => (error ? reject(error) : resolve()));
    });
  });

  it('discovers typed targets without trusting a redirected loopback WebSocket port', async () => {
    const targets = await listOwlChromiumTargets(discovery.port);

    expect(targets).toEqual([jasmine.objectContaining({ id: 'owl-page', title: 'Valdi Owl', type: 'page' })]);
  });

  it('rejects discovery responses above the byte limit before accumulating them', async () => {
    discovery.setDiscoveryBody(' '.repeat(1024 * 1024 + 1));

    await expectAsync(listOwlChromiumTargets(discovery.port)).toBeRejectedWithError(
      /exceeded the 1048576 byte response limit/,
    );
  });

  it('enforces an absolute discovery deadline while a 200 response trickles bytes', async () => {
    discovery.setDiscoveryChunkDelay(100);
    discovery.setDiscoveryChunks(Array.from({ length: 40 }, () => Buffer.from(' ')));
    const startedAt = Date.now();

    await expectAsync(listOwlChromiumTargets(discovery.port)).toBeRejectedWithError(/Timed out discovering/);
    await waitForSocketsToClose(discovery.sockets, 500);

    expect(Date.now() - startedAt).toBeLessThan(3800);
    expect(discovery.sockets.size).toBe(0);
  }, 5000);

  it('destroys a trickling non-200 discovery response immediately', async () => {
    discovery.setDiscoveryStatus(404);
    discovery.setDiscoveryChunkDelay(100);
    discovery.setDiscoveryChunks(Array.from({ length: 40 }, () => Buffer.from(' ')));
    const startedAt = Date.now();

    await expectAsync(listOwlChromiumTargets(discovery.port)).toBeRejectedWithError(/returned HTTP 404/);
    await waitForSocketsToClose(discovery.sockets, 500);

    expect(Date.now() - startedAt).toBeLessThan(1000);
    expect(discovery.sockets.size).toBe(0);
  });

  it('rejects malformed UTF-8 split across Chromium discovery response chunks', async () => {
    discovery.setDiscoveryChunks([
      Buffer.from('[{"id":"', 'utf8'),
      Buffer.from([0xc3]),
      Buffer.from([0x28]),
      Buffer.from('"}]', 'utf8'),
    ]);

    await expectAsync(listOwlChromiumTargets(discovery.port)).toBeRejectedWithError(/malformed UTF-8/);
  });

  it('rejects discovery responses with more than the accepted target limit', async () => {
    discovery.setDiscoveryBody(
      JSON.stringify(
        Array.from({ length: 257 }, (_, index) => ({
          id: `owl-page-${index}`,
          title: `Valdi Owl ${index}`,
          type: 'page',
          url: `http://127.0.0.1:54321/${index}.html`,
          webSocketDebuggerUrl: `ws://127.0.0.1:${discovery.port}/devtools/page/owl-page`,
        })),
      ),
    );

    await expectAsync(listOwlChromiumTargets(discovery.port)).toBeRejectedWithError(/more than 256 targets/);
  });

  it('reads the exact real Owl page without launching a second web renderer', async () => {
    const result = await readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE);

    expect(result['channel']).toBe('valdi-web-debugger');
    expect(result['snapshot']).toEqual(
      jasmine.objectContaining({ tree: jasmine.objectContaining({ id: '4', tag: 'view' }) }),
    );
    expect(discovery.expressions).toHaveSize(2);
    expect(discovery.expressions.every(expression => expression.includes(TARGET_NONCE))).toBeTrue();
    expect(
      discovery.expressions.every(expression => expression.includes('http://127.0.0.1:54321/index.html')),
    ).toBeTrue();
    expect(discovery.expressions[1]).toContain('globalThis.__VALDI_WEB_DEBUGGER__?.getSnapshot()');
  });

  it('works without the browser WebSocket global on supported Node.js versions', async () => {
    const previousWebSocket = (globalThis as unknown as { WebSocket?: unknown }).WebSocket;
    delete (globalThis as unknown as { WebSocket?: unknown }).WebSocket;

    try {
      await expectAsync(
        readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE),
      ).toBeResolved();
    } finally {
      (globalThis as unknown as { WebSocket?: unknown }).WebSocket = previousWebSocket;
    }
  });

  it('evaluates inspection expressions only against the matching Owl page', async () => {
    const expression = 'globalThis.__VALDI_WEB_DEBUGGER__?.getSnapshot()';

    const result = await evaluateOwlApplicationExpression(
      discovery.port,
      'http://127.0.0.1:54321/index.html',
      TARGET_NONCE,
      expression,
    );

    expect(result).toEqual(jasmine.objectContaining({ channel: 'valdi-web-debugger' }));
    expect(discovery.expressions).toHaveSize(2);
    expect(discovery.expressions[1]).toContain(expression);
  });

  it('keeps an exact nonce-bound connection open for events and reports teardown', async () => {
    const page: Record<string, unknown> = {
      URL,
      location: { href: 'http://127.0.0.1:54321/index.html?valdiDebugger=1&valdiDevTools=1' },
      [OWL_DEVTOOLS_TARGET_NONCE_PROPERTY]: TARGET_NONCE,
    };
    discovery.setWebSocketResponder((socket, frame) => {
      const command = decodeWebSocketCommand(frame);
      if (command.method === 'Runtime.evaluate') {
        evaluateCommandInPage(socket, frame, page);
        return;
      }
      socket.write(encodeWebSocketResponse({ id: command.id, result: {} }));
    });
    const connection = await connectToOwlApplication(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE);
    const events: Array<{ method: string; params: Record<string, unknown> }> = [];
    const closeErrors: Error[] = [];
    let resolveEvent: (() => void) | null = null;
    const receivedEvent = new Promise<void>(resolve => {
      resolveEvent = resolve;
    });
    const removeEvent = connection.onEvent(event => {
      events.push(event);
      resolveEvent?.();
    });
    connection.onClose(error => closeErrors.push(error));

    await expectAsync(connection.call('Runtime.enable', {}, 1000)).toBeResolvedTo({});
    discovery.emitEvent({
      method: 'Runtime.consoleAPICalled',
      params: { args: [{ type: 'string', value: 'Synthetic renderer message' }], type: 'log' },
    });
    await receivedEvent;

    expect(events).toEqual([
      {
        method: 'Runtime.consoleAPICalled',
        params: { args: [{ type: 'string', value: 'Synthetic renderer message' }], type: 'log' },
      },
    ]);
    expect(await connection.matchesTarget('http://127.0.0.1:54321/index.html', TARGET_NONCE)).toBeTrue();

    removeEvent();
    connection.close();
    expect(closeErrors).toHaveSize(1);
    expect(closeErrors[0]?.message).toContain('closed');
  });

  it('refuses to attach to a Chromium page for a different application path', async () => {
    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/other.html', TARGET_NONCE),
    ).toBeRejectedWithError(/No running Owl Chromium page matches/);
  });

  it('removes only injected Valdi parameters while matching the remaining query exactly', async () => {
    discovery.setDiscoveryBody(
      JSON.stringify([
        {
          id: 'owl-page',
          title: 'Valdi Owl',
          type: 'page',
          url: 'http://127.0.0.1:54321/index.html?valdiDevTools=1&mode=dev&tenant=alpha&valdiDebugger=1',
          webSocketDebuggerUrl: `ws://127.0.0.1:${discovery.port}/devtools/page/owl-page`,
        },
      ]),
    );

    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html?tenant=alpha&mode=dev', TARGET_NONCE),
    ).toBeResolved();
    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html?tenant=beta&mode=dev', TARGET_NONCE),
    ).toBeRejectedWithError(/No running Owl Chromium page matches/);
  });

  it('preserves the relative order of repeated query parameter values', async () => {
    discovery.setDiscoveryBody(
      JSON.stringify([
        {
          id: 'owl-page',
          title: 'Valdi Owl',
          type: 'page',
          url: 'http://127.0.0.1:54321/index.html?a=1&a=2&valdiDebugger=1&valdiDevTools=1',
          webSocketDebuggerUrl: `ws://127.0.0.1:${discovery.port}/devtools/page/owl-page`,
        },
      ]),
    );

    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html?a=1&a=2', TARGET_NONCE),
    ).toBeResolved();
    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html?a=2&a=1', TARGET_NONCE),
    ).toBeRejectedWithError(/No running Owl Chromium page matches/);
  });

  it('selects only the identical-URL target that owns the inspected-tab nonce and closes rejected candidates', async () => {
    discovery.setDiscoveryBody(
      JSON.stringify(
        ['first-page', 'second-page'].map(id => ({
          id,
          title: `Valdi Owl ${id}`,
          type: 'page',
          url: 'http://127.0.0.1:54321/index.html?valdiDebugger=1&valdiDevTools=1',
          webSocketDebuggerUrl: `ws://127.0.0.1:${discovery.port}/devtools/page/${id}`,
        })),
      ),
    );
    const snapshotCalls = new Map([
      ['first-page', 0],
      ['second-page', 0],
    ]);
    const pages: Record<string, Record<string, unknown>> = {
      'first-page': {
        URL,
        location: { href: 'http://127.0.0.1:54321/index.html?valdiDebugger=1&valdiDevTools=1' },
        [OWL_DEVTOOLS_TARGET_NONCE_PROPERTY]: 'another-tab-nonce-123456',
        __VALDI_WEB_DEBUGGER__: {
          getSnapshot: () => {
            snapshotCalls.set('first-page', (snapshotCalls.get('first-page') ?? 0) + 1);
            return debuggerSnapshotValue();
          },
        },
      },
      'second-page': {
        URL,
        location: { href: 'http://127.0.0.1:54321/index.html?valdiDebugger=1&valdiDevTools=1' },
        [OWL_DEVTOOLS_TARGET_NONCE_PROPERTY]: TARGET_NONCE,
        __VALDI_WEB_DEBUGGER__: {
          getSnapshot: () => {
            snapshotCalls.set('second-page', (snapshotCalls.get('second-page') ?? 0) + 1);
            return debuggerSnapshotValue();
          },
        },
      },
    };
    discovery.setWebSocketResponder((socket, frame, targetId) => {
      evaluateCommandInPage(socket, frame, pages[targetId]!);
    });

    const snapshot = await readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE);
    await new Promise<void>(resolve => setTimeout(resolve, 50));

    expect(snapshot['channel']).toBe('valdi-web-debugger');
    expect(snapshotCalls.get('first-page')).toBe(0);
    expect(snapshotCalls.get('second-page')).toBe(1);
    expect(discovery.closedTargetIds).toContain('first-page');
    expect(discovery.closedTargetIds).toContain('second-page');
  });

  it('rejects a target that navigates after discovery before the guarded evaluation', async () => {
    discovery.setDiscoveryBody(
      JSON.stringify([
        {
          id: 'navigated-page',
          title: 'Valdi Owl navigated',
          type: 'page',
          url: 'http://127.0.0.1:54321/index.html?valdiDebugger=1&valdiDevTools=1',
          webSocketDebuggerUrl: `ws://127.0.0.1:${discovery.port}/devtools/page/navigated-page`,
        },
      ]),
    );
    let snapshotCalls = 0;
    const navigatedPage: Record<string, unknown> = {
      URL,
      location: { href: 'http://127.0.0.1:54321/after-navigation.html' },
      [OWL_DEVTOOLS_TARGET_NONCE_PROPERTY]: TARGET_NONCE,
      __VALDI_WEB_DEBUGGER__: {
        getSnapshot: () => {
          snapshotCalls += 1;
          return debuggerSnapshotValue();
        },
      },
    };
    discovery.setWebSocketResponder((socket, frame) => {
      evaluateCommandInPage(socket, frame, navigatedPage);
    });

    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE),
    ).toBeRejectedWithError(/exact inspected DevTools tab/);
    expect(snapshotCalls).toBe(0);
  });

  it('rejects JSON null protocol messages without dereferencing them', async () => {
    discovery.setWebSocketResponder((socket, frame) => {
      decodeWebSocketCommand(frame);
      socket.write(encodeWebSocketResponse(null));
    });

    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE),
    ).toBeRejectedWithError(/non-object protocol message/);
  });

  it('rejects command responses with a non-numeric id', async () => {
    discovery.setWebSocketResponder((socket, frame) => {
      const command = decodeWebSocketCommand(frame);
      socket.write(encodeWebSocketResponse({ id: String(command.id), result: {} }));
    });

    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE),
    ).toBeRejectedWithError(/non-numeric command id/);
  });

  it('rejects oversized announced frame lengths before waiting for their payload', async () => {
    discovery.setWebSocketResponder((socket, frame) => {
      decodeWebSocketCommand(frame);
      const header = Buffer.alloc(10);
      header[0] = 0x81;
      header[1] = 127;
      header.writeBigUInt64BE(BigInt(16 * 1024 * 1024 + 1), 2);
      socket.write(header);
    });

    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE),
    ).toBeRejectedWithError(/announced a frame larger than 16777216 bytes/);
  });

  it('rejects masked server frames', async () => {
    discovery.setWebSocketResponder((socket, frame) => {
      const command = decodeWebSocketCommand(frame);
      const response = Buffer.from(
        JSON.stringify({ id: command.id, result: { result: { type: 'object', value: debuggerSnapshotValue() } } }),
        'utf8',
      );
      socket.write(encodeWebSocketServerFrame(0x01, response, true, true));
    });

    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE),
    ).toBeRejectedWithError(/must not mask WebSocket frames/);
  });

  it('rejects text frames containing malformed UTF-8', async () => {
    discovery.setWebSocketResponder((socket, frame) => {
      decodeWebSocketCommand(frame);
      socket.write(encodeWebSocketServerFrame(0x01, Buffer.from([0xc3, 0x28]), false, true));
    });

    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE),
    ).toBeRejectedWithError(/malformed UTF-8/);
  });

  it('answers a ping with a masked pong before accepting the command response', async () => {
    let commandId: number | null = null;
    let commandExpression = '';
    let pongMasked = false;
    let pongPayload: string | null = null;
    discovery.setWebSocketResponder((socket, frame) => {
      if (frame.opcode === 0x01) {
        const command = decodeWebSocketCommand(frame);
        commandId = command.id;
        commandExpression = command.params.expression ?? '';
        socket.write(encodeWebSocketServerFrame(0x09, Buffer.from('owl-ping'), false, true));
        return;
      }
      if (frame.opcode === 0x0a && commandId !== null) {
        pongMasked = frame.masked;
        pongPayload = frame.payload.toString('utf8');
        socket.write(
          encodeWebSocketResponse({
            id: commandId,
            result: {
              result: {
                type: 'object',
                value: {
                  __valdiDevToolsTargetMatched: true,
                  value: commandExpression.includes('__VALDI_WEB_DEBUGGER__') ? debuggerSnapshotValue() : true,
                },
              },
            },
          }),
        );
      }
    });

    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE),
    ).toBeResolved();
    expect(pongMasked).toBeTrue();
    expect(String(pongPayload)).toBe('owl-ping');
  });

  it('rejects close and fragmented frames without waiting for more data', async () => {
    discovery.setWebSocketResponder((socket, frame) => {
      decodeWebSocketCommand(frame);
      socket.write(encodeWebSocketServerFrame(0x08, Buffer.alloc(0), false, true));
    });
    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE),
    ).toBeRejectedWithError(/closed by the target/);

    discovery.setWebSocketResponder((socket, frame) => {
      decodeWebSocketCommand(frame);
      socket.write(encodeWebSocketServerFrame(0x01, Buffer.from('{}'), false, false));
    });
    await expectAsync(
      readOwlDebuggerSnapshot(discovery.port, 'http://127.0.0.1:54321/index.html', TARGET_NONCE),
    ).toBeRejectedWithError(/fragmented WebSocket frames are not supported/);
  });

  it('refuses non-loopback and credentialed Chromium websocket targets', async () => {
    await expectAsync(OwlChromiumConnection.connect('ws://example.com/devtools/page/owl')).toBeRejectedWithError(
      /only allows unauthenticated loopback/,
    );
    await expectAsync(
      OwlChromiumConnection.connect('ws://person:secret@127.0.0.1/devtools/page/owl'),
    ).toBeRejectedWithError(/only allows unauthenticated loopback/);
  });
});
