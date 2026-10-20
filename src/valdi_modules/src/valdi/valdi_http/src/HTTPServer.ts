import { ByteBuffer } from 'coreutils/src/ByteBuffer';
import { StringMap } from 'coreutils/src/StringMap';
import { TextDecoder, TextEncoder } from 'coreutils/src/unicode/TextCoding';
import { RequireFunc } from 'valdi_core/src/IModuleLoader';
import { beginKeepAlive, endKeepAlive, type KeepAlive } from 'valdi_core/src/utils/KeepAliveCallback';

declare const require: RequireFunc;

type TCPSocketProtocol = 'raw' | 'string' | 'valdi-bytes' | 'valdi-string';

/** The subset of the native TCPSocket connection that HTTPServer uses. */
export interface TCPSocketConnection {
  address: string;
  send(data: Uint8Array | string): void;
  close(error: string): void;
}

/** Callbacks a TCPSocket server delivers to HTTPServer. */
export interface TCPSocketServerListeners {
  onConnected?: (clientId: number, connection: TCPSocketConnection) => void;
  onDisconnected?: (clientId: number, error: string) => void;
  onDataReceived?: (clientId: number, data: Uint8Array) => void;
}

/** The subset of the native TCPSocket server that HTTPServer uses. */
export interface TCPSocketServer {
  start(): void;
  stop(): void;
  getPort(): number;
  getAddresses(): string[];
}

/** The native TCPSocket module, injectable so HTTPServer can be tested without sockets. */
export interface TCPSocketModule {
  createServer(port: number, protocol: TCPSocketProtocol, listeners: TCPSocketServerListeners): TCPSocketServer;
}

const CR = 13;
const LF = 10;
const utf8TextDecoder = new TextDecoder('utf-8');
const utf8TextEncoder = new TextEncoder('utf-8');

export interface HTTPServerRequest {
  clientId: number;
  method: string;
  target: string;
  path: string;
  queryString: string;
  version: string;
  headers: StringMap<string>;
  body: Uint8Array;
}

export type HTTPServerBody = ArrayBuffer | Uint8Array | string | undefined;

/** A chunked HTTP response that remains open until close() or client disconnect. */
export interface HTTPServerResponseStream {
  readonly closed: boolean;
  write(body: Exclude<HTTPServerBody, undefined>): void;
  close(): void;
  onClose(listener: () => void): void;
}

export interface HTTPServerResponse {
  statusCode: number;
  headers?: StringMap<string>;
  body?: HTTPServerBody;
  /**
   * Starts an HTTP/1.1 chunked response after the headers and optional initial
   * body have been sent. The callback may retain the stream and write later.
   */
  stream?: (stream: HTTPServerResponseStream) => void;
}

export type HTTPServerHandler = (request: HTTPServerRequest) => HTTPServerResponse | Promise<HTTPServerResponse>;

export interface HTTPServerOptions {
  /** Largest request line plus headers accepted before the connection is rejected. Defaults to 64 KiB. */
  maxHeaderBytes?: number;
  /** Largest Content-Length accepted before the connection is rejected. Defaults to 10 MiB. */
  maxBodyBytes?: number;
  /** Socket implementation; defaults to the native TCPSocket module. */
  socketModule?: TCPSocketModule;
}

const DEFAULT_MAX_HEADER_BYTES = 64 * 1024;
const DEFAULT_MAX_BODY_BYTES = 10 * 1024 * 1024;

interface HTTPConnectionState {
  connection: TCPSocketConnection;
  buffer: ByteBuffer;
  connected: boolean;
  // A request is being handled; the next one is parsed only once its response
  // has finished, so pipelined responses go out in order and never interleave.
  busy: boolean;
  readOffset: number;
  responseStreams: HTTPServerResponseStreamImpl[];
}

class HTTPServerResponseStreamImpl implements HTTPServerResponseStream {
  private closeListeners: Array<() => void> = [];
  private isClosed = false;

  constructor(
    private readonly connection: TCPSocketConnection,
    private readonly didClose: () => void,
  ) {}

  get closed(): boolean {
    return this.isClosed;
  }

  write(body: Exclude<HTTPServerBody, undefined>): void {
    if (this.isClosed) {
      return;
    }
    const bytes = bodyToBytes(body);
    if (bytes.length === 0) {
      return;
    }
    this.connection.send(stringToUtf8Bytes(`${bytes.length.toString(16)}\r\n`));
    this.connection.send(bytes);
    this.connection.send(stringToUtf8Bytes('\r\n'));
  }

  close(): void {
    if (this.isClosed) {
      return;
    }
    this.connection.send(stringToUtf8Bytes('0\r\n\r\n'));
    this.finish();
  }

  onClose(listener: () => void): void {
    if (this.isClosed) {
      listener();
      return;
    }
    this.closeListeners.push(listener);
  }

  disconnected(): void {
    this.finish();
  }

  private finish(): void {
    if (this.isClosed) {
      return;
    }
    this.isClosed = true;
    this.didClose();
    const listeners = this.closeListeners;
    this.closeListeners = [];
    for (const listener of listeners) {
      listener();
    }
  }
}

function findHeaderEnd(buffer: Uint8Array, offset: number): number {
  for (let index = offset; index <= buffer.length - 4; index += 1) {
    if (buffer[index] === CR && buffer[index + 1] === LF && buffer[index + 2] === CR && buffer[index + 3] === LF) {
      return index;
    }
  }
  return -1;
}

function utf8BytesToString(bytes: Uint8Array): string {
  return utf8TextDecoder.decode(bytes.slice().buffer);
}

function stringToUtf8Bytes(value: string): Uint8Array {
  return utf8TextEncoder.encode(value);
}

function bodyToBytes(body: HTTPServerBody): Uint8Array {
  if (body === undefined) {
    return new Uint8Array(0);
  }
  if (typeof body === 'string') {
    return stringToUtf8Bytes(body);
  }
  if (body instanceof Uint8Array) {
    return body;
  }
  return new Uint8Array(body);
}

function parseHeaders(lines: string[]): StringMap<string> {
  const headers: StringMap<string> = {};
  for (const line of lines) {
    const separator = line.indexOf(':');
    if (separator < 0) {
      continue;
    }
    const key = line.substring(0, separator).trim().toLowerCase();
    const value = line.substring(separator + 1).trim();
    headers[key] = value;
  }
  return headers;
}

function parseTarget(target: string): { path: string; queryString: string } {
  const queryIndex = target.indexOf('?');
  if (queryIndex < 0) {
    return { path: target, queryString: '' };
  }
  return {
    path: target.substring(0, queryIndex),
    queryString: target.substring(queryIndex + 1),
  };
}

function statusText(statusCode: number): string {
  switch (statusCode) {
    case 200:
      return 'OK';
    case 201:
      return 'Created';
    case 204:
      return 'No Content';
    case 400:
      return 'Bad Request';
    case 403:
      return 'Forbidden';
    case 404:
      return 'Not Found';
    case 405:
      return 'Method Not Allowed';
    case 413:
      return 'Payload Too Large';
    case 431:
      return 'Request Header Fields Too Large';
    case 500:
      return 'Internal Server Error';
    default:
      return 'OK';
  }
}

function responseHeaders(body: Uint8Array, response: HTTPServerResponse): StringMap<string> {
  const headers: StringMap<string> = {
    connection: 'keep-alive',
    ...(response.headers ?? {}),
  };
  if (response.stream === undefined) {
    delete headers['transfer-encoding'];
    headers['content-length'] = String(body.length);
  } else {
    delete headers['content-length'];
    headers['transfer-encoding'] = 'chunked';
  }
  return headers;
}

/**
 * A small HTTP/1.1 server intended for local Valdi tooling, such as browser-backed
 * integration tests. It parses request lines, headers, Content-Length bodies, and
 * pipelined requests over raw TCPSocket connections. It writes fixed-length or
 * chunked streaming responses and keeps connections open for reuse.
 *
 * This is not a complete production HTTP server: it does not implement TLS,
 * streaming request bodies, compression, multipart parsing, protocol upgrades,
 * or advanced connection management.
 */
export class HTTPServer {
  private nativeServer: TCPSocketServer | undefined;
  private keepAlive: KeepAlive | undefined;
  private readonly connections: StringMap<HTTPConnectionState> = {};
  private readonly maxHeaderBytes: number;
  private readonly maxBodyBytes: number;

  constructor(
    private readonly handler: HTTPServerHandler,
    private readonly options: HTTPServerOptions = {},
  ) {
    this.maxHeaderBytes = options.maxHeaderBytes ?? DEFAULT_MAX_HEADER_BYTES;
    this.maxBodyBytes = options.maxBodyBytes ?? DEFAULT_MAX_BODY_BYTES;
  }

  get port(): number {
    if (!this.nativeServer) {
      throw new Error('HTTPServer has not been started');
    }
    return this.nativeServer.getPort();
  }

  get addresses(): string[] {
    if (!this.nativeServer) {
      return [];
    }
    return this.nativeServer.getAddresses();
  }

  async start(port = 0): Promise<void> {
    if (this.nativeServer) {
      throw new Error('HTTPServer is already started');
    }

    // Loaded on start rather than at import so the module stays importable where
    // TCPSocket is unavailable (it only ships in dev/gold native builds).
    const socketModule = this.options.socketModule ?? (require('TCPSocket') as TCPSocketModule);
    this.nativeServer = socketModule.createServer(port, 'raw', {
      onConnected: (clientId, connection) => {
        this.connections[String(clientId)] = {
          connection,
          buffer: new ByteBuffer(),
          connected: true,
          busy: false,
          readOffset: 0,
          responseStreams: [],
        };
      },
      onDisconnected: clientId => {
        const state = this.connections[String(clientId)];
        if (state !== undefined) {
          this.dropConnection(String(clientId), state);
        }
      },
      onDataReceived: (clientId, data) => {
        this.receiveData(clientId, data);
      },
    });
    this.nativeServer.start();
    this.keepAlive = beginKeepAlive();
  }

  stop(): void {
    if (!this.nativeServer) {
      return;
    }

    this.nativeServer.stop();
    this.nativeServer = undefined;
    endKeepAlive(this.keepAlive!);
    this.keepAlive = undefined;
    for (const key in this.connections) {
      if (Object.prototype.hasOwnProperty.call(this.connections, key)) {
        const state = this.connections[key]!;
        // Stopping the listener does not end established connections.
        state.connection.close('HTTPServer stopped');
        this.dropConnection(key, state);
      }
    }
  }

  private dropConnection(key: string, state: HTTPConnectionState): void {
    state.connected = false;
    for (const stream of state.responseStreams.slice()) {
      stream.disconnected();
    }
    delete this.connections[key];
  }

  private receiveData(clientId: number, data: Uint8Array): void {
    const state = this.connections[String(clientId)];
    if (!state || !state.connected) {
      return;
    }

    state.buffer.appendData(data);
    // Bound what an unparsed request may buffer: headers up to maxHeaderBytes,
    // plus a body up to maxBodyBytes once the headers have arrived.
    const pending = state.buffer.size - state.readOffset;
    const headerEnd = findHeaderEnd(state.buffer.inner, state.readOffset);
    if (headerEnd < 0 && pending > this.maxHeaderBytes) {
      this.rejectConnection(clientId, state, 431);
      return;
    }
    if (pending > this.maxHeaderBytes + this.maxBodyBytes) {
      this.rejectConnection(clientId, state, 413);
      return;
    }
    this.processNextRequest(clientId, state);
  }

  private processNextRequest(clientId: number, state: HTTPConnectionState): void {
    if (state.busy || !state.connected) {
      return;
    }
    const buffer = state.buffer.inner;
    const headerEnd = findHeaderEnd(buffer, state.readOffset);
    if (headerEnd < 0) {
      this.compactConsumedBytes(state);
      return;
    }
    if (headerEnd - state.readOffset > this.maxHeaderBytes) {
      this.rejectConnection(clientId, state, 431);
      return;
    }

    const headerText = utf8BytesToString(buffer.subarray(state.readOffset, headerEnd));
    const lines = headerText.split('\r\n');
    const requestParts = (lines.shift() ?? '').split(' ');
    if (requestParts.length < 3) {
      this.rejectConnection(clientId, state, 400);
      return;
    }

    const headers = parseHeaders(lines);
    const rawContentLength = headers['content-length'] ?? '0';
    // Anything but a plain decimal leaves no safe way to find the next request.
    if (!/^[0-9]+$/.test(rawContentLength)) {
      this.rejectConnection(clientId, state, 400);
      return;
    }
    const contentLength = Number(rawContentLength);
    if (contentLength > this.maxBodyBytes) {
      this.rejectConnection(clientId, state, 413);
      return;
    }
    const bodyStart = headerEnd + 4;
    const requestEnd = bodyStart + contentLength;
    if (state.buffer.size < requestEnd) {
      return;
    }

    const body = buffer.slice(bodyStart, requestEnd);
    state.readOffset = requestEnd;
    this.compactConsumedBytes(state);
    const target = requestParts[1]!;
    const parsedTarget = parseTarget(target);
    const request: HTTPServerRequest = {
      clientId,
      method: requestParts[0]!,
      target,
      path: parsedTarget.path,
      queryString: parsedTarget.queryString,
      version: requestParts[2]!,
      headers,
      body,
    };
    state.busy = true;
    void this.handleRequest(state, request).then(() => {
      state.busy = false;
      this.processNextRequest(clientId, state);
    });
  }

  /** Answers with an error status and closes, since the request stream can no longer be framed. */
  private rejectConnection(clientId: number, state: HTTPConnectionState, statusCode: number): void {
    if (state.connected && !state.busy) {
      this.sendResponse(state, {
        statusCode,
        headers: { connection: 'close', 'content-type': 'text/plain; charset=utf-8' },
        body: statusText(statusCode),
      });
    }
    state.connection.close(statusText(statusCode));
    this.dropConnection(String(clientId), state);
  }

  private compactConsumedBytes(state: HTTPConnectionState): void {
    if (state.readOffset === 0) {
      return;
    }

    const buffer = state.buffer.inner;
    if (state.readOffset >= buffer.length) {
      state.buffer = new ByteBuffer(state.buffer.capacity);
      state.readOffset = 0;
      return;
    }

    const remaining = buffer.subarray(state.readOffset);
    const nextBuffer = new ByteBuffer(Math.max(state.buffer.capacity, remaining.length));
    nextBuffer.appendData(remaining);
    state.buffer = nextBuffer;
    state.readOffset = 0;
  }

  /** Resolves once the response has been fully written, including any stream. */
  private async handleRequest(state: HTTPConnectionState, request: HTTPServerRequest): Promise<void> {
    let response: HTTPServerResponse;
    try {
      response = await this.handler(request);
    } catch (error) {
      response = {
        statusCode: 500,
        headers: { 'content-type': 'text/plain; charset=utf-8' },
        body: error instanceof Error ? error.message : String(error),
      };
    }
    if (!state.connected) {
      // The client is gone, but a streaming handler may hold resources that it
      // releases from onClose, so still hand it a stream that is already closed.
      if (response.stream !== undefined) {
        const closedStream = new HTTPServerResponseStreamImpl(state.connection, () => {});
        closedStream.disconnected();
        startResponseStream(response.stream, closedStream);
      }
      return;
    }
    const stream = this.sendResponse(state, response);
    if (stream !== undefined) {
      await new Promise<void>(resolve => stream.onClose(resolve));
    }
  }

  private sendResponse(
    state: HTTPConnectionState,
    response: HTTPServerResponse,
  ): HTTPServerResponseStreamImpl | undefined {
    const body = bodyToBytes(response.body);
    const headers = responseHeaders(body, response);
    let headerText = `HTTP/1.1 ${response.statusCode} ${statusText(response.statusCode)}\r\n`;
    for (const key in headers) {
      if (Object.prototype.hasOwnProperty.call(headers, key)) {
        headerText += `${key}: ${headers[key]}\r\n`;
      }
    }
    headerText += '\r\n';
    state.connection.send(stringToUtf8Bytes(headerText));
    if (response.stream === undefined) {
      if (body.length > 0) {
        state.connection.send(body);
      }
      return undefined;
    }

    const stream = new HTTPServerResponseStreamImpl(state.connection, () => {
      const index = state.responseStreams.indexOf(stream);
      if (index >= 0) {
        state.responseStreams.splice(index, 1);
      }
    });
    state.responseStreams.push(stream);
    if (body.length > 0) {
      stream.write(body);
    }
    startResponseStream(response.stream, stream);
    return stream;
  }
}

/** Runs a stream callback; once headers are out, a throw can only end the stream. */
function startResponseStream(
  start: (stream: HTTPServerResponseStream) => void,
  stream: HTTPServerResponseStreamImpl,
): void {
  try {
    start(stream);
  } catch (error) {
    console.error('HTTPServer response stream failed to start', error);
    stream.close();
  }
}
