import * as net from 'net';
import * as http from 'http';
import * as crypto from 'crypto';
import {
  listHermesDevices,
  HermesConnection,
  NotHermesError,
  normalizeHermesProfile,
  type CpuProfile,
} from '../src/utils/hermesClient';

// ─── Helpers ─────────────────────────────────────────────────────────────────

function makeWsHandshakeResponse(key: string): string {
  const accept = crypto
    .createHash('sha1')
    .update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11')
    .digest('base64');
  return (
    'HTTP/1.1 101 Switching Protocols\r\n' +
    'Upgrade: websocket\r\n' +
    'Connection: Upgrade\r\n' +
    `Sec-WebSocket-Accept: ${accept}\r\n` +
    '\r\n'
  );
}

function encodeWsTextFrame(text: string): Buffer {
  const payload = Buffer.from(text, 'utf8');
  const len = payload.length;
  const header = len < 126 ? Buffer.alloc(2) : Buffer.alloc(4);
  header[0] = 0x81; // FIN + text opcode
  if (len < 126) {
    header[1] = len; // unmasked
  } else {
    header[1] = 126;
    header.writeUInt16BE(len, 2);
  }
  return Buffer.concat([header, payload]);
}

function encodePingFrame(): Buffer {
  const buf = Buffer.alloc(2);
  buf[0] = 0x89; // FIN + ping opcode
  buf[1] = 0x00; // no payload
  return buf;
}

/** Spin up a minimal WebSocket server on a random port. */
function createMockHermesServer(
  onConnection: (socket: net.Socket, request: string) => void,
): Promise<{ server: net.Server; port: number }> {
  return new Promise((resolve) => {
    const server = net.createServer((socket) => {
      let buf = '';
      socket.once('data', (chunk) => {
        buf += chunk.toString('utf8');
        onConnection(socket, buf);
      });
    });
    server.listen(0, '127.0.0.1', () => {
      const port = (server.address() as net.AddressInfo).port;
      resolve({ server, port });
    });
  });
}

// ─── normalizeHermesProfile ───────────────────────────────────────────────────

describe('normalizeHermesProfile', () => {
  const nodes: CpuProfile['nodes'] = [
    { id: 1, callFrame: { functionName: '[root]', scriptId: '0', url: '[root]', lineNumber: 0, columnNumber: 0 } },
    { id: 2, callFrame: { functionName: 'tick', scriptId: '0', url: 'app/src/App', lineNumber: 1, columnNumber: 0 } },
  ];

  it('strips the fake t=0 leading entry and sets startTime to the real monotonic origin', () => {
    const raw: CpuProfile = {
      nodes,
      startTime: 0,
      endTime: 877385746707,
      samples: [1, 1, 2, 1],
      timeDeltas: [0, 877380769876, 13673, 26294],
    };
    const norm = normalizeHermesProfile(raw);

    // startTime should be the absolute time of sample[1] (the real origin)
    expect(norm.startTime).toBe(877380769876);
    // First delta from real origin should be 0
    expect(norm.timeDeltas![0]).toBe(0);
    // Remaining deltas are unchanged
    expect(norm.timeDeltas![1]).toBe(13673);
    expect(norm.timeDeltas![2]).toBe(26294);
    // First sample is dropped
    expect(norm.samples).toEqual([1, 2, 1]);
    // Nodes unchanged
    expect(norm.nodes).toBe(nodes);
  });

  it('computes a ~5s duration from the remaining deltas', () => {
    const raw: CpuProfile = {
      nodes,
      startTime: 0,
      endTime: 877385746707,
      samples: [1, 1, 2, 1],
      timeDeltas: [0, 877380769876, 13673, 26294],
    };
    const norm = normalizeHermesProfile(raw);
    const duration = norm.timeDeltas!.reduce((a, b) => a + b, 0);
    expect(duration).toBe(13673 + 26294); // microseconds, not days
  });

  it('returns the profile unchanged when samples.length < 2', () => {
    const raw: CpuProfile = { nodes, startTime: 0, endTime: 100, samples: [1], timeDeltas: [0] };
    expect(normalizeHermesProfile(raw)).toBe(raw);
  });

  it('returns the profile unchanged when samples is empty', () => {
    const raw: CpuProfile = { nodes, startTime: 0, endTime: 100, samples: [], timeDeltas: [] };
    expect(normalizeHermesProfile(raw)).toBe(raw);
  });
});

// ─── listHermesDevices ────────────────────────────────────────────────────────

describe('listHermesDevices', () => {
  let server: http.Server | undefined;
  let port: number;

  beforeEach(() => { server = undefined; });
  afterEach((done) => { server ? server.close(done) : done(); });

  it('returns parsed device list from HTTP /json', async () => {
    const devices = [{ id: '1', title: 'Valdi Hermes', webSocketDebuggerUrl: 'ws://localhost:13595/1' }];
    server = http.createServer((_req, res) => {
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify(devices));
    });
    await new Promise<void>((resolve) => server.listen(0, '127.0.0.1', resolve));
    port = (server.address() as net.AddressInfo).port;

    const result = await listHermesDevices(port);
    expect(result).toEqual(devices);
  });

  it('throws NotHermesError on ECONNREFUSED', async () => {
    // Port 1 is privileged and always ECONNREFUSED on loopback (no process binds it)
    await expectAsync(listHermesDevices(1)).toBeRejectedWith(jasmine.any(NotHermesError));
  });

  it('throws NotHermesError on timeout when server accepts but never responds', async () => {
    server = http.createServer((_req, _res) => { /* never respond */ });
    await new Promise<void>((resolve) => server.listen(0, '127.0.0.1', resolve));
    port = (server.address() as net.AddressInfo).port;

    await expectAsync(listHermesDevices(port)).toBeRejectedWith(jasmine.any(NotHermesError));
  }, 5000);
});

// ─── HermesConnection ─────────────────────────────────────────────────────────

describe('HermesConnection', () => {
  let tcpServer: net.Server;
  let port: number;

  afterEach((done) => { tcpServer?.close(done); });

  /** Starts a mock WebSocket server that completes the handshake then calls `onReady`. */
  async function startMockServer(
    onReady: (socket: net.Socket) => void,
  ): Promise<void> {
    const { server, port: p } = await createMockHermesServer((socket, request) => {
      const keyMatch = /Sec-WebSocket-Key: (.+)\r\n/.exec(request);
      const key = keyMatch?.[1] ?? '';
      socket.write(makeWsHandshakeResponse(key));
      onReady(socket);
    });
    tcpServer = server;
    port = p;
  }

  it('resolves after a successful WebSocket handshake', async () => {
    await startMockServer(() => { /* nothing extra needed */ });
    const conn = await HermesConnection.connect(port, '1');
    conn.close();
  });

  it('sends Profiler.start and receives a result', async () => {
    await startMockServer((socket) => {
      socket.on('data', (chunk: Buffer) => {
        // Decode the masked client frame to read the CDP message
        const masked = chunk[1]! & 0x80;
        const payloadLen = chunk[1]! & 0x7f;
        const maskOffset = masked ? 2 : 2;
        const mask = masked ? chunk.subarray(maskOffset, maskOffset + 4) : null;
        const payloadOffset = maskOffset + (masked ? 4 : 0);
        const rawPayload = chunk.subarray(payloadOffset, payloadOffset + payloadLen);
        const payload = mask
          ? Buffer.from(rawPayload.map((b, i) => b ^ mask[i % 4]!))
          : rawPayload;
        const msg = JSON.parse(payload.toString('utf8')) as { id: number; method: string };
        // Reply with success
        socket.write(encodeWsTextFrame(JSON.stringify({ id: msg.id, result: {} })));
      });
    });

    const conn = await HermesConnection.connect(port, '1');
    await expectAsync(conn.startProfiling()).toBeResolved();
    conn.close();
  });

  it('stopProfiling returns a normalised CpuProfile', async () => {
    const rawProfile: CpuProfile = {
      nodes: [{ id: 1, callFrame: { functionName: '[root]', scriptId: '0', url: '[root]', lineNumber: 0, columnNumber: 0 } }],
      startTime: 0,
      endTime: 877385746707,
      samples: [1, 1, 1],
      timeDeltas: [0, 877380769876, 13673],
    };

    await startMockServer((socket) => {
      socket.on('data', (chunk: Buffer) => {
        const masked = chunk[1]! & 0x80;
        const payloadLen = chunk[1]! & 0x7f;
        const maskOffset = 2;
        const mask = masked ? chunk.subarray(maskOffset, maskOffset + 4) : null;
        const payloadOffset = maskOffset + (masked ? 4 : 0);
        const rawPayload = chunk.subarray(payloadOffset, payloadOffset + payloadLen);
        const payload = mask
          ? Buffer.from(rawPayload.map((b, i) => b ^ mask[i % 4]!))
          : rawPayload;
        const msg = JSON.parse(payload.toString('utf8')) as { id: number };
        socket.write(encodeWsTextFrame(JSON.stringify({ id: msg.id, result: { profile: rawProfile } })));
      });
    });

    const conn = await HermesConnection.connect(port, '1');
    const profile = await conn.stopProfiling();

    // startTime should be normalised (not 0)
    expect(profile.startTime).toBe(877380769876);
    // One leading sample stripped
    expect(profile.samples!.length).toBe(2);
    conn.close();
  });

  it('does not stall when server sends a ping frame before the CDP response', async () => {
    await startMockServer((socket) => {
      socket.on('data', (chunk: Buffer) => {
        const masked = chunk[1]! & 0x80;
        const payloadLen = chunk[1]! & 0x7f;
        const maskOffset = 2;
        const mask = masked ? chunk.subarray(maskOffset, maskOffset + 4) : null;
        const payloadOffset = maskOffset + (masked ? 4 : 0);
        const rawPayload = chunk.subarray(payloadOffset, payloadOffset + payloadLen);
        const payload = mask
          ? Buffer.from(rawPayload.map((b, i) => b ^ mask[i % 4]!))
          : rawPayload;
        const msg = JSON.parse(payload.toString('utf8')) as { id: number };
        // Send a ping before the real reply
        socket.write(encodePingFrame());
        socket.write(encodeWsTextFrame(JSON.stringify({ id: msg.id, result: {} })));
      });
    });

    const conn = await HermesConnection.connect(port, '1');
    await expectAsync(conn.startProfiling()).toBeResolved();
    conn.close();
  });

  it('rejects call() after timeout', async () => {
    await startMockServer(() => { /* never send a reply */ });
    const conn = await HermesConnection.connect(port, '1');
    await expectAsync(
      (conn as unknown as { call: (m: string, p: object, t: number) => Promise<unknown> })
        .call('Profiler.start', {}, 100),
    ).toBeRejectedWithError(/Timeout/);
    conn.close();
  });
});
