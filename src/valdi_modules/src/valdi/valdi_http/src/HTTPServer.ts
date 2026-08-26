import { ByteBuffer } from 'coreutils/src/ByteBuffer';
import { StringMap } from 'coreutils/src/StringMap';
import { TextDecoder, TextEncoder } from 'coreutils/src/unicode/TextCoding';
import { RequireFunc } from 'valdi_core/src/IModuleLoader';
import { beginKeepAlive, endKeepAlive, type KeepAlive } from 'valdi_core/src/utils/KeepAliveCallback';

declare const require: RequireFunc;

type TCPSocketProtocol = 'raw' | 'string' | 'valdi-bytes' | 'valdi-string';

interface TCPSocketConnection {
  address: string;
  send(data: Uint8Array | string): void;
  close(error: string): void;
}

interface TCPSocketServerListeners {
  onConnected?: (clientId: number, connection: TCPSocketConnection) => void;
  onDisconnected?: (clientId: number, error: string) => void;
  onDataReceived?: (clientId: number, data: Uint8Array) => void;
}

interface TCPSocketServer {
  start(): void;
  stop(): void;
  getPort(): number;
  getAddresses(): string[];
}

interface TCPSocketModule {
  createServer(port: number, protocol: TCPSocketProtocol, listeners: TCPSocketServerListeners): TCPSocketServer;
}

const TCPSocket = require('TCPSocket') as TCPSocketModule;
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

interface HTTPConnectionState {
  connection: TCPSocketConnection;
  buffer: ByteBuffer;
  connected: boolean;
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

  constructor(private readonly handler: HTTPServerHandler) {}

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

    this.nativeServer = TCPSocket.createServer(port, 'raw', {
      onConnected: (clientId, connection) => {
        this.connections[String(clientId)] = {
          connection,
          buffer: new ByteBuffer(),
          connected: true,
          readOffset: 0,
          responseStreams: [],
        };
      },
      onDisconnected: clientId => {
        const state = this.connections[String(clientId)];
        if (state !== undefined) {
          state.connected = false;
          for (const stream of state.responseStreams.slice()) {
            stream.disconnected();
          }
          delete this.connections[String(clientId)];
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
        state.connected = false;
        for (const stream of state.responseStreams.slice()) {
          stream.disconnected();
        }
        delete this.connections[key];
      }
    }
  }

  private receiveData(clientId: number, data: Uint8Array): void {
    const state = this.connections[String(clientId)];
    if (!state) {
      return;
    }

    state.buffer.appendData(data);
    this.processBufferedRequests(clientId, state);
  }

  private processBufferedRequests(clientId: number, state: HTTPConnectionState): void {
    while (true) {
      const buffer = state.buffer.inner;
      const headerEnd = findHeaderEnd(buffer, state.readOffset);
      if (headerEnd < 0) {
        this.compactConsumedBytes(state);
        return;
      }

      const headerText = utf8BytesToString(buffer.subarray(state.readOffset, headerEnd));
      const lines = headerText.split('\r\n');
      const requestLine = lines.shift();
      if (!requestLine) {
        this.sendResponse(state, {
          statusCode: 400,
          headers: { 'content-type': 'text/plain; charset=utf-8' },
          body: 'Bad Request',
        });
        state.readOffset = headerEnd + 4;
        this.compactConsumedBytes(state);
        continue;
      }

      const requestParts = requestLine.split(' ');
      if (requestParts.length < 3) {
        this.sendResponse(state, {
          statusCode: 400,
          headers: { 'content-type': 'text/plain; charset=utf-8' },
          body: 'Bad Request',
        });
        state.readOffset = headerEnd + 4;
        this.compactConsumedBytes(state);
        continue;
      }

      const headers = parseHeaders(lines);
      const contentLength = Number(headers['content-length'] ?? '0');
      const bodyStart = headerEnd + 4;
      const requestEnd = bodyStart + contentLength;
      if (state.buffer.size < requestEnd) {
        this.compactConsumedBytes(state);
        return;
      }

      const body = buffer.subarray(bodyStart, requestEnd);
      state.readOffset = requestEnd;
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
      void this.handleRequest(state, request);
      this.compactConsumedBytes(state);
    }
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

  private async handleRequest(state: HTTPConnectionState, request: HTTPServerRequest): Promise<void> {
    try {
      const response = await this.handler(request);
      if (state.connected) {
        this.sendResponse(state, response);
      }
    } catch (error) {
      if (state.connected) {
        this.sendResponse(state, {
          statusCode: 500,
          headers: { 'content-type': 'text/plain; charset=utf-8' },
          body: error instanceof Error ? error.message : String(error),
        });
      }
    }
  }

  private sendResponse(state: HTTPConnectionState, response: HTTPServerResponse): void {
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
      return;
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
    response.stream(stream);
  }
}
