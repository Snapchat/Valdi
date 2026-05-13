import { EventEmitter } from 'node:events';
import { readFile } from 'node:fs/promises';
import { run } from './CommandHelpers';

const HEADER_END = '\r\n\r\n';

export interface TSServerClient {
  command: (command: string, args?: Record<string, unknown>) => Promise<Record<string, unknown>>;
  disconnect: () => Promise<number>;
}

export function createClient(workingDir: string): TSServerClient {
  const reserveSequence = createNonceService();
  const read = createMessageBuffer();

  // eslint-disable-next-line unicorn/prefer-event-target
  const queue = new EventEmitter<{
    response: [Record<string, unknown>];
    message: [Record<string, unknown>];
  }>();

  const service = run('npx', ['tsserver'], {cwd: workingDir});

  service.process.stdout?.on('data', (chunk: Buffer) => {
    for (const [message] of read(chunk)) {
      if ('type' in message && message['type'] === 'response') {
        queue.emit('response', message);
      }
      queue.emit('message', message);
    }
  });

  service.process.stderr?.pipe(process.stderr)

  const send = (
    request: Record<string, unknown>,
    { timeout = 300000 }: { timeout?: number } = {},
  ): Promise<Record<string, unknown>> => {
    const seq = reserveSequence();
    const body = JSON.stringify({
      seq,
      ...request,
    });
    const payload = body + '\r\n';
    service.process.stdin.write(payload);

    return withTimeout(
      new Promise(resolve => {
        const listener = (message: Record<string, unknown>) => {
          if ('request_seq' in message && message['request_seq'] !== seq) {
            return;
          }
          queue.removeListener('response', listener);
          resolve(message);
        };
        queue.addListener('response', listener);
      }),
      timeout,
    );
  };

  return {
    command: (command: string, args?: Record<string, unknown>) =>
      send({
        command,
        ...(args ? { arguments: args } : null),
      }),
    disconnect: () => {
      service.process.stdin.end();
      service.process.kill('SIGINT');
      return service.wait();
    },
  };
}

export interface FilePosition {
  /**
   * 1 indexed line number (first line is line 1)
   */
  line: number;
  /**
   * 1 indexed column number (first column is column 1)
   */
  offset: number;
  /**
   * 0-index within the file content, the nth character
   */
  position: number;
}

export async function lineAndOffset(searchToken: string, file: string): Promise<FilePosition> {
  const buffer = await readFile(file);
  const position = buffer.indexOf(searchToken);

  if (position === -1) {
    throw new Error('searchToken not found in file');
  }

  let line = 1;
  let index = 0;
  let offset = -1;
  while (index <= position) {
    const nextLine = buffer.indexOf('\n', index);
    if (nextLine < position) {
      line += 1;
      index = nextLine + 1;
      continue;
    } else {
      offset = position - index;
      break;
    }
  }
  return { line, offset: Math.max(0, offset), position };
}

function withTimeout<T>(promise: Promise<T>, ms: number | false = 300000): Promise<T> {
  if (ms === false) {
    return promise;
  }
  let timer: NodeJS.Timeout;
  return Promise.race([
    new Promise<never>((_resolve, reject) => {
      timer = setTimeout(() => reject(new Error('timeout')), ms);
    }),
    promise.then(value => {
      clearTimeout(timer);
      return value;
    }),
  ]);
}

function* eachLine(buffer: Buffer) {
  let index = 0;
  while (index > -1 && index < buffer.length) {
    const endIndex = buffer.indexOf('\n', index);
    if (endIndex === -1) {
      yield buffer.subarray(index);
      break;
    }
    yield buffer.subarray(index, endIndex);
    index = endIndex;
  }
}

function* parseHeaders(chunk: Buffer): Generator<[name: string, value: string]> {
  for (const line of eachLine(chunk)) {
    const sep = line.indexOf(':');
    const name = line.subarray(0, sep);
    const value = line.subarray(sep + 1);
    yield [name.toString('utf8'), value.toString('utf8').trim()];
  }
}

/**
 * Create a statefull message buffer that parses TSServer messages.
 *
 * @returns {(chunk: Buffer) => Generator<[body: Record<string, unknown>, headers: Array<[name: string, value: string]>]>}
 */
function createMessageBuffer(): (
  chunk: Buffer,
) => Generator<[body: Record<string, unknown>, headers: Array<[name: string, value: string]>]> {
  let buffer = Buffer.from([]);
  /**
   *
   * @param {Buffer} chunk
   * @return {Generator<[body: Record<string, unknown>, headers: Array<[name: string, value: string]>]>}
   */
  return function* read(chunk) {
    buffer = Buffer.concat([buffer, chunk]);

    let position = 0;

    while (position < buffer.length) {
      const headerSuffix = buffer.indexOf(HEADER_END, position);

      if (headerSuffix === -1) {
        throw new Error('no headers ' + buffer.toString('utf8'));
      }

      const headers: [string, string][] = Array.from(parseHeaders(buffer.subarray(position, headerSuffix)));

      // Content length
      const contentLength = Number.parseInt(
        headers.find(([name]) => name === 'Content-Length')?.[1] ?? Number.NaN.toString(),
      );
      if (Number.isNaN(contentLength)) {
        throw new TypeError('invalid message, no Content-Length');
      }

      const bodyStart = headerSuffix + HEADER_END.length;
      const bodyEnd = bodyStart + contentLength;

      if (bodyEnd > buffer.length) {
        buffer = buffer.subarray(position);
        return;
      }

      yield [JSON.parse(buffer.subarray(bodyStart, bodyEnd).toString('utf8')), headers];

      position = bodyEnd;
    }
  };
}

/**
 * Every time the returned function is called it will return a new sequenc.
 *
 * @returns {() => number}
 */
function createNonceService() {
  let sequence = 1;
  return () => {
    const seq = sequence;
    sequence += 1;
    return seq;
  };
}
