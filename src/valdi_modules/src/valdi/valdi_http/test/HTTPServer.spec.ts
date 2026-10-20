import 'jasmine/src/jasmine';
import { TextDecoder, TextEncoder } from 'coreutils/src/unicode/TextCoding';
import {
  HTTPServer,
  HTTPServerHandler,
  HTTPServerOptions,
  HTTPServerResponseStream,
  TCPSocketConnection,
  TCPSocketModule,
  TCPSocketServerListeners,
} from 'valdi_http/src/HTTPServer';

const encoder = new TextEncoder('utf-8');
const decoder = new TextDecoder('utf-8');

class FakeConnection implements TCPSocketConnection {
  readonly address = '127.0.0.1';
  readonly sent: string[] = [];
  closedWith: string | undefined;

  send(data: Uint8Array | string): void {
    this.sent.push(typeof data === 'string' ? data : decoder.decode(data.slice().buffer));
  }

  close(error: string): void {
    this.closedWith = error;
  }

  get output(): string {
    return this.sent.join('');
  }
}

class FakeSocketModule implements TCPSocketModule {
  listeners: TCPSocketServerListeners = {};
  stopped = false;

  createServer(_port: number, _protocol: string, listeners: TCPSocketServerListeners) {
    this.listeners = listeners;
    return {
      start: () => {},
      stop: () => {
        this.stopped = true;
      },
      getPort: () => 8080,
      getAddresses: () => ['127.0.0.1'],
    };
  }

  connect(clientId: number): FakeConnection {
    const connection = new FakeConnection();
    this.listeners.onConnected?.(clientId, connection);
    return connection;
  }

  send(clientId: number, text: string): void {
    this.listeners.onDataReceived?.(clientId, encoder.encode(text));
  }

  disconnect(clientId: number): void {
    this.listeners.onDisconnected?.(clientId, 'gone');
  }
}

// Lets the server's promise chain (handler, response, next request) run to completion.
async function settle(): Promise<void> {
  for (let tick = 0; tick < 5; tick++) {
    await new Promise<void>(resolve => setTimeout(resolve, 0));
  }
}

// Bodies are not newline-terminated, so a later status line can follow one directly.
function statusLines(output: string): string[] {
  return output.match(/HTTP\/1\.1 [0-9]{3}[^\r]*/g) ?? [];
}

describe('HTTPServer', () => {
  let socket: FakeSocketModule;
  let server: HTTPServer | undefined;

  async function startServer(handler: HTTPServerHandler, options: HTTPServerOptions = {}): Promise<void> {
    socket = new FakeSocketModule();
    server = new HTTPServer(handler, { ...options, socketModule: socket });
    await server.start();
  }

  afterEach(() => {
    server?.stop();
    server = undefined;
  });

  it('serves a simple request', async () => {
    await startServer(request => ({ statusCode: 200, body: `path=${request.path}` }));
    const connection = socket.connect(1);
    socket.send(1, 'GET /hello HTTP/1.1\r\nHost: x\r\n\r\n');
    await settle();

    expect(statusLines(connection.output)).toEqual(['HTTP/1.1 200 OK']);
    expect(connection.output).toContain('path=/hello');
  });

  it('rejects a non-numeric Content-Length instead of looping', async () => {
    let calls = 0;
    await startServer(() => {
      calls++;
      return { statusCode: 200 };
    });
    const connection = socket.connect(1);
    socket.send(1, 'POST / HTTP/1.1\r\nContent-Length: abc\r\n\r\n');
    await settle();

    expect(calls).toBe(0);
    expect(statusLines(connection.output)).toEqual(['HTTP/1.1 400 Bad Request']);
    expect(connection.closedWith).toBeDefined();
  });

  it('rejects a negative Content-Length', async () => {
    await startServer(() => ({ statusCode: 200 }));
    const connection = socket.connect(1);
    socket.send(1, 'POST / HTTP/1.1\r\nContent-Length: -5\r\n\r\n');
    await settle();

    expect(statusLines(connection.output)).toEqual(['HTTP/1.1 400 Bad Request']);
    expect(connection.closedWith).toBeDefined();
  });

  it('rejects headers larger than maxHeaderBytes', async () => {
    await startServer(() => ({ statusCode: 200 }), { maxHeaderBytes: 64 });
    const connection = socket.connect(1);
    socket.send(1, `GET / HTTP/1.1\r\nX-Long: ${'a'.repeat(100)}`);
    await settle();

    expect(statusLines(connection.output)).toEqual(['HTTP/1.1 431 Request Header Fields Too Large']);
    expect(connection.closedWith).toBeDefined();
  });

  it('rejects a Content-Length larger than maxBodyBytes', async () => {
    await startServer(() => ({ statusCode: 200 }), { maxBodyBytes: 10 });
    const connection = socket.connect(1);
    socket.send(1, 'POST / HTTP/1.1\r\nContent-Length: 11\r\n\r\n');
    await settle();

    expect(statusLines(connection.output)).toEqual(['HTTP/1.1 413 Payload Too Large']);
    expect(connection.closedWith).toBeDefined();
  });

  it('answers pipelined requests in order, one at a time', async () => {
    const pending: Array<() => void> = [];
    await startServer(
      request =>
        new Promise(resolve => {
          pending.push(() => resolve({ statusCode: 200, body: request.path }));
        }),
    );
    const connection = socket.connect(1);
    socket.send(1, 'GET /first HTTP/1.1\r\n\r\nGET /second HTTP/1.1\r\n\r\n');
    await settle();

    // Only the first request is in flight; the second waits for its response.
    expect(pending.length).toBe(1);
    pending[0]!();
    await settle();
    expect(pending.length).toBe(2);
    pending[1]!();
    await settle();

    const output = connection.output;
    expect(statusLines(output).length).toBe(2);
    expect(output.indexOf('/first')).toBeLessThan(output.indexOf('/second'));
  });

  it('holds the next pipelined request until a streamed response closes', async () => {
    let stream: HTTPServerResponseStream | undefined;
    await startServer(request =>
      request.path === '/stream'
        ? { statusCode: 200, stream: s => (stream = s) }
        : { statusCode: 200, body: 'after' },
    );
    const connection = socket.connect(1);
    socket.send(1, 'GET /stream HTTP/1.1\r\n\r\nGET /next HTTP/1.1\r\n\r\n');
    await settle();
    stream!.write('chunk');
    await settle();
    expect(connection.output).not.toContain('after');

    stream!.close();
    await settle();
    expect(connection.output.indexOf('0\r\n\r\n')).toBeLessThan(connection.output.indexOf('after'));
  });

  it('still lets a streaming handler clean up when the client leaves first', async () => {
    let resolveHandler: () => void = () => {};
    let closed = false;
    await startServer(
      () =>
        new Promise(resolve => {
          resolveHandler = () =>
            resolve({
              statusCode: 200,
              stream: s => s.onClose(() => (closed = true)),
            });
        }),
    );
    const connection = socket.connect(1);
    socket.send(1, 'GET /stream HTTP/1.1\r\n\r\n');
    await settle();
    socket.disconnect(1);
    resolveHandler();
    await settle();

    expect(closed).toBe(true);
    expect(connection.output).toBe('');
  });

  it('ends the stream instead of writing a second response when the stream callback throws', async () => {
    await startServer(() => ({
      statusCode: 200,
      stream: () => {
        throw new Error('boom');
      },
    }));
    const connection = socket.connect(1);
    socket.send(1, 'GET / HTTP/1.1\r\n\r\n');
    await settle();

    expect(statusLines(connection.output)).toEqual(['HTTP/1.1 200 OK']);
    expect(connection.output.endsWith('0\r\n\r\n')).toBe(true);
  });

  it('closes open connections on stop', async () => {
    await startServer(() => ({ statusCode: 200 }));
    const connection = socket.connect(1);
    server!.stop();
    server = undefined;

    expect(socket.stopped).toBe(true);
    expect(connection.closedWith).toBeDefined();
  });
});
