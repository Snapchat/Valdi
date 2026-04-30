/**
 * Hermes debug WebSocket client — connects to the Hermes JS runtime's debug server
 * and implements the Chrome DevTools Protocol (CDP) Profiler domain.
 *
 * Architecture (confirmed from source):
 *   The Hermes runtime opens a WebSocket server on a random port. The hot-reloader
 *   discovers that port via the daemon protocol and sets up:
 *     adb forward tcp:13595 tcp:<random>
 *   so that external tools can connect via a stable port.
 *
 * Connection flow:
 *   1. HTTP GET http://localhost:<port>/json → [{id, webSocketDebuggerUrl, ...}]
 *   2. Connect via WebSocket to ws://localhost:<port>/<id>
 *   3. Standard CDP Profiler.* calls work on that connection.
 *
 * Requires the hot-reloader to be running (`valdi hotreload android|ios`) so that
 * the adb port forward to the Hermes debug socket is established.
 */

import * as net from 'net';
import * as crypto from 'crypto';
import * as http from 'http';
import { CliError } from '../core/errors';

// ─── Ports ───────────────────────────────────────────────────────────────────

export const HERMES_PORT = 13595;

// ─── Types ───────────────────────────────────────────────────────────────────

export interface HermesDebuggableDevice {
  id: string;
  title: string;
  webSocketDebuggerUrl: string;
}

export interface CpuProfile {
  nodes: CpuProfileNode[];
  startTime: number;
  endTime: number;
  samples?: number[];
  timeDeltas?: number[];
}

export interface CpuProfileNode {
  id: number;
  callFrame: {
    functionName: string;
    scriptId: string;
    url: string;
    lineNumber: number;
    columnNumber: number;
  };
  hitCount?: number;
  children?: number[];
}

// ─── Errors ──────────────────────────────────────────────────────────────────

export const HERMES_BUILD_FLAG = '--@valdi//bzl/valdi:js_engine=hermes --@valdi//bzl/valdi:js_bytecode_format=hermes';

/**
 * Thrown when the Hermes debug socket is not available — either the app was
 * built with a different JS engine (QuickJS is the default), the Hermes
 * debugger is not enabled, or the hot-reloader is not running.
 */
export class NotHermesError extends CliError {
  constructor(port: number) {
    super(
      `Hermes debug socket not found on port ${port}.\n\n` +
      `CPU profiling requires:\n` +
      `  1. A Hermes build (the default JS engine is QuickJS):\n` +
      `       valdi install android --bazel_args="${HERMES_BUILD_FLAG}"\n` +
      `       valdi install ios     --bazel_args="${HERMES_BUILD_FLAG}"\n\n` +
      `  2. The hot-reloader running (it establishes the debug tunnel on port ${port}):\n` +
      `       valdi hotreload android\n`,
    );
  }
}

// ─── HTTP /json endpoint ──────────────────────────────────────────────────────

export async function listHermesDevices(port: number): Promise<HermesDebuggableDevice[]> {
  // Do NOT run adb forward here — the Companion manages the port-13595 tunnel.
  // Running adb forward would overwrite the Companion's mapping to the random Hermes port.
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      req.destroy();
      reject(new NotHermesError(port));
    }, 3_000);

    const req = http.get(`http://127.0.0.1:${port}/json`, (res) => {
      let body = '';
      res.on('data', (chunk: string) => { body += chunk; });
      res.on('end', () => {
        clearTimeout(timer);
        try {
          resolve(JSON.parse(body) as HermesDebuggableDevice[]);
        } catch {
          reject(new CliError(`Invalid response from Hermes debug server on port ${port}`));
        }
      });
    });

    req.on('error', (err: NodeJS.ErrnoException) => {
      clearTimeout(timer);
      if (err.code === 'ECONNREFUSED') {
        reject(new NotHermesError(port));
      } else {
        reject(err);
      }
    });
  });
}

// ─── Minimal WebSocket framing ────────────────────────────────────────────────

function encodeWsFrame(text: string): Buffer {
  const payload = Buffer.from(text, 'utf8');
  const mask = crypto.randomBytes(4);
  const masked = Buffer.alloc(payload.length);
  for (let i = 0; i < payload.length; i++) {
    masked[i] = payload[i]! ^ mask[i % 4]!;
  }

  const len = payload.length;
  let header: Buffer;
  if (len < 126) {
    header = Buffer.alloc(6);
    header[0] = 0x81; // FIN + text opcode
    header[1] = 0x80 | len; // MASK bit + length
    mask.copy(header, 2);
  } else {
    header = Buffer.alloc(8);
    header[0] = 0x81;
    header[1] = 0x80 | 126;
    header.writeUInt16BE(len, 2);
    mask.copy(header, 4);
  }
  return Buffer.concat([header, masked]);
}

// ─── Profile normalisation ────────────────────────────────────────────────────

/**
 * Hermes emits a quirky cpuprofile layout that confuses viewers like Speedscope
 * and Chrome DevTools:
 *
 *   startTime: 0
 *   timeDeltas: [0, <huge monotonic-clock offset>, <real deltas …>]
 *   samples:    [1 (root), 1 (root), <real samples …>]
 *
 * The second timeDelta is the absolute monotonic clock value at the moment
 * profiling started (µs since device boot), which makes the timeline appear
 * to span days. Fix: drop the two synthetic leading entries and set
 * `startTime` to the actual wall-clock origin so the timeline reads correctly.
 */
export function normalizeHermesProfile(profile: CpuProfile): CpuProfile {
  const { samples = [], timeDeltas = [], startTime, endTime, nodes } = profile;
  if (samples.length < 2 || timeDeltas.length < 2) return profile;

  // Compute absolute sample times
  const absTimes: number[] = [];
  let t = startTime;
  for (const d of timeDeltas) {
    t += d;
    absTimes.push(t);
  }

  // absTimes[0] = 0 (fake start), absTimes[1] = real monotonic origin
  const realStart = absTimes[1]!;
  const realAbsTimes = absTimes.slice(1);
  const realSamples = samples.slice(1);

  const newDeltas: number[] = [0];
  for (let i = 1; i < realAbsTimes.length; i++) {
    newDeltas.push(realAbsTimes[i]! - realAbsTimes[i - 1]!);
  }

  return { nodes, startTime: realStart, endTime, samples: realSamples, timeDeltas: newDeltas };
}

// ─── PendingCall ─────────────────────────────────────────────────────────────

interface PendingCall {
  resolve: (result: unknown) => void;
  reject: (err: Error) => void;
}

// ─── HermesConnection ────────────────────────────────────────────────────────

export class HermesConnection {
  private socket: net.Socket;
  private recvBuf: Buffer = Buffer.alloc(0);
  private callId = 0;
  private pending = new Map<number, PendingCall>();

  private constructor(socket: net.Socket) {
    this.socket = socket;
    socket.on('data', (chunk: Buffer) => this.onData(chunk));
    socket.on('close', () => {
      this.rejectAll(new Error('Hermes debug WebSocket closed unexpectedly'));
    });
    socket.on('error', (err) => this.rejectAll(err));
  }

  /**
   * Connect to the Hermes debug WebSocket for the given device context ID.
   * The path on the WebSocket server is `/<deviceId>`.
   */
  static connect(port: number, deviceId: string): Promise<HermesConnection> {
    return new Promise((resolve, reject) => {
      const socket = net.createConnection({ port, host: '127.0.0.1' });

      const timer = setTimeout(() => {
        socket.destroy();
        reject(new CliError(
          `Timeout connecting to Hermes debug socket on port ${port}.\n` +
          `Make sure the Valdi app is running and the hot-reloader is active.`,
        ));
      }, 5_000);

      socket.once('error', (err: NodeJS.ErrnoException) => {
        clearTimeout(timer);
        if (err.code === 'ECONNREFUSED') {
          reject(new NotHermesError(port));
        } else {
          reject(err);
        }
      });

      socket.once('connect', () => {
        // Perform WebSocket upgrade handshake
        const key = crypto.randomBytes(16).toString('base64');
        const handshake =
          `GET /${deviceId} HTTP/1.1\r\n` +
          `Host: localhost:${port}\r\n` +
          `Upgrade: websocket\r\n` +
          `Connection: Upgrade\r\n` +
          `Sec-WebSocket-Key: ${key}\r\n` +
          `Sec-WebSocket-Version: 13\r\n` +
          `\r\n`;
        socket.write(handshake);

        let handshakeBuf = '';
        const onHandshakeData = (chunk: Buffer) => {
          handshakeBuf += chunk.toString('utf8');
          if (handshakeBuf.includes('\r\n\r\n')) {
            socket.removeListener('data', onHandshakeData);
            clearTimeout(timer);
            if (!handshakeBuf.includes('101 Switching Protocols')) {
              socket.destroy();
              reject(new CliError(`Hermes WebSocket handshake failed: ${handshakeBuf.slice(0, 100)}`));
              return;
            }
            resolve(new HermesConnection(socket));
          }
        };
        socket.on('data', onHandshakeData);
      });
    });
  }

  // ── Raw CDP call ──────────────────────────────────────────────────────────

  call(method: string, params: object = {}, timeoutMs = 10_000): Promise<unknown> {
    const id = ++this.callId;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`Timeout waiting for response to ${method}`));
      }, timeoutMs);
      this.pending.set(id, {
        resolve: (v) => { clearTimeout(timer); resolve(v); },
        reject: (e) => { clearTimeout(timer); reject(e); },
      });
      this.socket.write(encodeWsFrame(JSON.stringify({ id, method, params })));
    });
  }

  // ── Profiler ──────────────────────────────────────────────────────────────

  async startProfiling(): Promise<void> {
    await this.call('Profiler.start');
  }

  async stopProfiling(): Promise<CpuProfile> {
    const result = await this.call('Profiler.stop', {}, 60_000) as { profile: CpuProfile };
    return normalizeHermesProfile(result.profile);
  }

  // ── Lifecycle ─────────────────────────────────────────────────────────────

  close(): void {
    this.socket.destroy();
  }

  // ── Internal ─────────────────────────────────────────────────────────────

  private rejectAll(err: Error): void {
    for (const pending of this.pending.values()) pending.reject(err);
    this.pending.clear();
  }

  private onData(chunk: Buffer): void {
    this.recvBuf = Buffer.concat([this.recvBuf, chunk]);
    this.drainBuffer();
  }

  private drainBuffer(): void {
    for (;;) {
      if (this.recvBuf.length < 2) break;

      const firstByte = this.recvBuf[0]!;
      const secondByte = this.recvBuf[1]!;
      const opcode = firstByte & 0x0f;

      if (opcode === 0x08) {
        // Connection close frame
        this.rejectAll(new Error('Hermes debug WebSocket closed by server'));
        return;
      }

      const isMasked = (secondByte & 0x80) !== 0;
      let payloadLen = secondByte & 0x7f;
      let offset = 2;

      if (payloadLen === 126) {
        if (this.recvBuf.length < 4) break;
        payloadLen = this.recvBuf.readUInt16BE(2);
        offset = 4;
      } else if (payloadLen === 127) {
        if (this.recvBuf.length < 10) break;
        payloadLen = this.recvBuf.readUInt32BE(6); // lower 32 bits
        offset = 10;
      }

      const maskLen = isMasked ? 4 : 0;
      const totalLen = offset + maskLen + payloadLen;
      if (this.recvBuf.length < totalLen) break;

      // Skip non-text/binary frames (ping=0x09, pong=0x0A, continuation=0x00)
      if (opcode !== 0x01 && opcode !== 0x02) {
        this.recvBuf = this.recvBuf.subarray(totalLen);
        continue;
      }

      let payload = this.recvBuf.subarray(offset + maskLen, totalLen);
      if (isMasked) {
        const mask = this.recvBuf.subarray(offset, offset + 4);
        payload = Buffer.from(payload.map((b, i) => b ^ mask[i % 4]!));
      }

      this.recvBuf = this.recvBuf.subarray(totalLen);

      try {
        this.dispatchMessage(JSON.parse(payload.toString('utf8')) as Record<string, unknown>);
      } catch {
        // ignore malformed messages
      }
    }
  }

  private dispatchMessage(msg: Record<string, unknown>): void {
    if (typeof msg['id'] !== 'number') return; // ignore events

    const pending = this.pending.get(msg['id'] as number);
    if (!pending) return;
    this.pending.delete(msg['id'] as number);

    if (msg['error']) {
      const err = msg['error'] as Record<string, unknown>;
      pending.reject(new Error(String(err['message'] ?? 'Unknown CDP error')));
    } else {
      pending.resolve(msg['result']);
    }
  }
}

