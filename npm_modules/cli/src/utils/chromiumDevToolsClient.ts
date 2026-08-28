import crypto from 'node:crypto';
import http from 'node:http';
import type { Socket } from 'node:net';
import { TextDecoder } from 'node:util';
import { isLoopbackHost } from './loopbackHost';

const WEBSOCKET_ACCEPT_SUFFIX = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';
const MAX_WEBSOCKET_FRAME_BYTES = 16 * 1024 * 1024;
const MAX_WEBSOCKET_RECEIVE_BYTES = MAX_WEBSOCKET_FRAME_BYTES + 10;
const FATAL_UTF8_DECODER = new TextDecoder('utf8', { fatal: true });

interface PendingDevToolsCommand {
  timer: NodeJS.Timeout;
  reject(error: Error): void;
  resolve(result: unknown): void;
}

interface DevToolsCommandResponse {
  error?: {
    message?: string;
  };
  id?: number;
  method?: string;
  params?: Record<string, unknown>;
  result?: unknown;
}

export interface ChromiumDevToolsEvent {
  method: string;
  params: Record<string, unknown>;
}

export class ChromiumDevToolsProtocolError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'ChromiumDevToolsProtocolError';
  }
}

function encodeWebSocketFrame(opcode: number, payload: Buffer): Buffer {
  const mask = crypto.randomBytes(4);
  let header: Buffer;

  if (payload.length < 126) {
    header = Buffer.alloc(6);
    header[1] = 0x80 | payload.length;
    mask.copy(header, 2);
  } else if (payload.length <= 0xff_ff) {
    header = Buffer.alloc(8);
    header[1] = 0x80 | 126;
    header.writeUInt16BE(payload.length, 2);
    mask.copy(header, 4);
  } else {
    header = Buffer.alloc(14);
    header[1] = 0x80 | 127;
    header.writeBigUInt64BE(BigInt(payload.length), 2);
    mask.copy(header, 10);
  }

  header[0] = 0x80 | opcode;
  const maskedPayload = Buffer.from(payload);
  for (let index = 0; index < maskedPayload.length; index += 1) {
    maskedPayload[index] = (maskedPayload[index] ?? 0) ^ (mask[index % mask.length] ?? 0);
  }

  return Buffer.concat([header, maskedPayload]);
}

function encodeWebSocketTextFrame(text: string): Buffer {
  return encodeWebSocketFrame(0x01, Buffer.from(text, 'utf8'));
}

/** Shared, dependency-free Chromium DevTools transport for Owl and Hermes. */
export class ChromiumDevToolsConnection {
  private readonly closeListeners = new Set<(error: Error) => void>();
  private readonly eventListeners = new Set<(event: ChromiumDevToolsEvent) => void>();
  private readonly pending = new Map<number, PendingDevToolsCommand>();
  private closedError: Error | null = null;
  private nextCommandId = 0;
  private receiveBuffer = Buffer.alloc(0);

  private constructor(
    private readonly socket: Socket,
    initialData: Buffer,
  ) {
    socket.on('data', this.handleData);
    socket.on('close', this.handleClose);
    socket.on('error', this.handleError);
    if (initialData.length > 0) {
      this.handleData(initialData);
    }
  }

  static async connect(webSocketUrl: string, timeoutMs: number): Promise<ChromiumDevToolsConnection> {
    const url = new URL(webSocketUrl);
    if (url.protocol !== 'ws:' || !isLoopbackHost(url.hostname) || url.username || url.password) {
      throw new Error('Chromium DevTools inspection only allows unauthenticated loopback WebSocket targets.');
    }

    const key = crypto.randomBytes(16).toString('base64');
    const expectedAccept = crypto.createHash('sha1').update(`${key}${WEBSOCKET_ACCEPT_SUFFIX}`).digest('base64');
    const hostname = url.hostname === '[::1]' ? '::1' : url.hostname;

    return await new Promise<ChromiumDevToolsConnection>((resolve, reject) => {
      let settled = false;
      const request = http.request({
        headers: {
          Connection: 'Upgrade',
          Host: url.host,
          'Sec-WebSocket-Key': key,
          'Sec-WebSocket-Version': '13',
          Upgrade: 'websocket',
        },
        hostname,
        method: 'GET',
        path: `${url.pathname}${url.search}`,
        port: url.port ? Number.parseInt(url.port, 10) : 80,
      });

      const timer = setTimeout(() => {
        fail(new Error('Timed out connecting to the Chromium DevTools target.'));
      }, timeoutMs);

      function finish(): boolean {
        if (settled) return false;
        settled = true;
        clearTimeout(timer);
        return true;
      }

      function fail(error: Error): void {
        if (!finish()) return;
        request.destroy();
        reject(error);
      }

      request.once('upgrade', (response, socket, head) => {
        if (response.headers['sec-websocket-accept'] !== expectedAccept) {
          socket.destroy();
          fail(new Error('The Chromium DevTools WebSocket handshake returned an invalid accept key.'));
          return;
        }
        if (finish()) {
          resolve(new ChromiumDevToolsConnection(socket, head));
        } else {
          socket.destroy();
        }
      });
      request.once('response', response => {
        response.resume();
        fail(new Error(`Chromium DevTools WebSocket upgrade returned HTTP ${String(response.statusCode)}.`));
      });
      request.once('error', fail);
      request.end();
    });
  }

  async call(method: string, params: Record<string, unknown>, timeoutMs: number): Promise<unknown> {
    if (this.closedError) throw this.closedError;
    const id = ++this.nextCommandId;
    return await new Promise<unknown>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`Timeout waiting for the Chromium DevTools ${method} response.`));
      }, timeoutMs);
      this.pending.set(id, { reject, resolve, timer });
      try {
        this.socket.write(encodeWebSocketTextFrame(JSON.stringify({ id, method, params })));
      } catch (error) {
        clearTimeout(timer);
        this.pending.delete(id);
        reject(error instanceof Error ? error : new Error(`Could not send the Chromium DevTools ${method} request.`));
      }
    });
  }

  onClose(listener: (error: Error) => void): () => void {
    if (this.closedError) {
      listener(this.closedError);
      return () => {};
    }
    this.closeListeners.add(listener);
    return () => {
      this.closeListeners.delete(listener);
    };
  }

  onEvent(listener: (event: ChromiumDevToolsEvent) => void): () => void {
    if (this.closedError) throw this.closedError;
    this.eventListeners.add(listener);
    return () => {
      this.eventListeners.delete(listener);
    };
  }

  close(): void {
    this.failConnection(new Error('The Chromium DevTools WebSocket closed.'));
  }

  private readonly handleData = (chunk: Buffer): void => {
    if (this.closedError) return;
    if (chunk.length > MAX_WEBSOCKET_RECEIVE_BYTES - this.receiveBuffer.length) {
      this.failConnection(
        new Error(`Chromium DevTools exceeded the ${MAX_WEBSOCKET_RECEIVE_BYTES} byte receive buffer limit.`),
      );
      return;
    }
    this.receiveBuffer = Buffer.concat([this.receiveBuffer, chunk]);

    while (this.receiveBuffer.length >= 2) {
      const firstByte = this.receiveBuffer.readUInt8(0);
      const secondByte = this.receiveBuffer.readUInt8(1);
      const finalFrame = (firstByte & 0x80) !== 0;
      const opcode = firstByte & 0x0f;
      if ((firstByte & 0x70) !== 0) {
        this.failConnection(new Error('Chromium DevTools returned a WebSocket frame with reserved bits set.'));
        return;
      }
      if ((secondByte & 0x80) !== 0) {
        this.failConnection(new Error('Chromium DevTools servers must not mask WebSocket frames.'));
        return;
      }
      let payloadLength = secondByte & 0x7f;
      let offset = 2;

      if (payloadLength === 126) {
        if (this.receiveBuffer.length < 4) return;
        payloadLength = this.receiveBuffer.readUInt16BE(2);
        offset = 4;
      } else if (payloadLength === 127) {
        if (this.receiveBuffer.length < 10) return;
        const length = this.receiveBuffer.readBigUInt64BE(2);
        if (length > BigInt(MAX_WEBSOCKET_FRAME_BYTES)) {
          this.failConnection(
            new Error(`Chromium DevTools announced a frame larger than ${MAX_WEBSOCKET_FRAME_BYTES} bytes.`),
          );
          return;
        }
        payloadLength = Number(length);
        offset = 10;
      }

      if (payloadLength > MAX_WEBSOCKET_FRAME_BYTES) {
        this.failConnection(
          new Error(`Chromium DevTools announced a frame larger than ${MAX_WEBSOCKET_FRAME_BYTES} bytes.`),
        );
        return;
      }
      const controlFrame = opcode >= 0x08;
      if (controlFrame && payloadLength > 125) {
        this.failConnection(new Error('Chromium DevTools returned an oversized WebSocket control frame.'));
        return;
      }
      if (!finalFrame) {
        this.failConnection(new Error('Chromium DevTools fragmented WebSocket frames are not supported.'));
        return;
      }

      const totalLength = offset + payloadLength;
      if (this.receiveBuffer.length < totalLength) return;

      const payload = this.receiveBuffer.subarray(offset, totalLength);
      this.receiveBuffer = this.receiveBuffer.subarray(totalLength);

      switch (opcode) {
        case 0x01:
        case 0x02: {
          this.handleMessage(payload);
          break;
        }
        case 0x08: {
          this.failConnection(new Error('The Chromium DevTools WebSocket closed by the target.'));
          return;
        }
        case 0x09: {
          try {
            this.socket.write(encodeWebSocketFrame(0x0a, payload));
          } catch (error) {
            this.failConnection(
              error instanceof Error ? error : new Error('Could not answer the Chromium DevTools WebSocket ping.'),
            );
            return;
          }
          break;
        }
        case 0x0a: {
          break;
        }
        default: {
          this.failConnection(new Error(`Chromium DevTools returned unsupported WebSocket opcode ${opcode}.`));
          return;
        }
      }
    }
  };

  private handleMessage(payload: Buffer): void {
    let text: string;
    try {
      text = FATAL_UTF8_DECODER.decode(payload);
    } catch {
      this.failConnection(new Error('Chromium DevTools returned malformed UTF-8.'));
      return;
    }

    let parsed: unknown;
    try {
      parsed = JSON.parse(text) as unknown;
    } catch {
      this.failConnection(new Error('Chromium DevTools returned malformed JSON.'));
      return;
    }

    if (typeof parsed !== 'object' || parsed === null || Array.isArray(parsed)) {
      this.failConnection(new Error('Chromium DevTools returned a non-object protocol message.'));
      return;
    }
    const response = parsed as DevToolsCommandResponse;
    if (response.id === undefined) {
      if (typeof response.method !== 'string' || response.method.length === 0 || response.method.length > 256) {
        this.failConnection(new Error('Chromium DevTools returned a malformed protocol event.'));
        return;
      }
      const params = response.params;
      if (params !== undefined && (typeof params !== 'object' || params === null || Array.isArray(params))) {
        this.failConnection(new Error('Chromium DevTools returned an event with invalid parameters.'));
        return;
      }
      const event: ChromiumDevToolsEvent = {
        method: response.method,
        params: params ?? {},
      };
      for (const listener of Array.from(this.eventListeners)) {
        try {
          listener(event);
        } catch (error) {
          console.warn(`[Valdi DevTools] A Chromium ${response.method} event listener failed.`, error);
        }
      }
      return;
    }
    if (!Number.isInteger(response.id)) {
      this.failConnection(new Error('Chromium DevTools returned a response with a non-numeric command id.'));
      return;
    }
    const command = this.pending.get(response.id);
    if (!command) return;

    this.pending.delete(response.id);
    clearTimeout(command.timer);
    if (response.error) {
      command.reject(
        new ChromiumDevToolsProtocolError(
          response.error.message ?? 'The Chromium DevTools target rejected the request.',
        ),
      );
      return;
    }
    command.resolve(response.result);
  }

  private readonly handleClose = (): void => {
    this.failConnection(new Error('The Chromium DevTools WebSocket closed.'));
  };

  private readonly handleError = (error: Error): void => {
    this.failConnection(error);
  };

  private failConnection(error: Error): void {
    if (this.closedError) return;
    this.closedError = error;
    this.receiveBuffer = Buffer.alloc(0);
    this.rejectPending(error);
    this.eventListeners.clear();
    for (const listener of Array.from(this.closeListeners)) {
      try {
        listener(error);
      } catch (listenerError) {
        console.warn('[Valdi DevTools] A Chromium close listener failed.', listenerError);
      }
    }
    this.closeListeners.clear();
    if (!this.socket.destroyed) this.socket.destroy();
  }

  private rejectPending(error: Error): void {
    for (const command of this.pending.values()) {
      clearTimeout(command.timer);
      command.reject(error);
    }
    this.pending.clear();
  }
}
