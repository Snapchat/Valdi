import 'jasmine';
import { EventEmitter } from 'node:events';
import type { Socket } from 'node:net';
import { DaemonConnection } from './daemonClient';

const TEST_MAGIC = Buffer.from([0x33, 0xc6, 0x00, 0x01]);

function encodeTestPacket(payload: object): Buffer {
  const body = Buffer.from(JSON.stringify(payload), 'utf8');
  const header = Buffer.alloc(8);
  TEST_MAGIC.copy(header, 0);
  header.writeUInt32LE(body.length, 4);
  return Buffer.concat([header, body]);
}

// net.Socket uses EventEmitter semantics, which this protocol test mirrors.
// eslint-disable-next-line unicorn/prefer-event-target
class ErrorResponseSocket extends EventEmitter {
  readonly remotePort = 13_591;

  write(data: Buffer, callback?: (error?: Error) => void): boolean {
    const packet = JSON.parse(data.subarray(8).toString('utf8')) as Record<string, unknown>;
    const event = packet['event'] as Record<string, unknown> | undefined;
    const payloadFromClient = event?.['payload_from_client'] as Record<string, unknown> | undefined;
    if (payloadFromClient) {
      const request = JSON.parse(String(payloadFromClient['payload_string'])) as Record<string, unknown>;
      const errorResponse = {
        request: {
          forward_client_payload: {
            client_id: 1,
            payload_string: JSON.stringify({
              type: -1,
              requestId: request['requestId'],
              body: { message: 'Heap dump failed.' },
            }),
          },
          request_id: 'device-response-1',
        },
      };
      queueMicrotask(() => this.emit('data', encodeTestPacket(errorResponse)));
    }
    callback?.();
    return true;
  }

  destroy(): this {
    this.emit('close');
    return this;
  }
}

// net.Socket uses EventEmitter semantics, which this protocol test mirrors.
// eslint-disable-next-line unicorn/prefer-event-target
class CustomResponseSocket extends EventEmitter {
  readonly remotePort = 13_591;
  request: Record<string, unknown> | undefined;

  write(data: Buffer, callback?: (error?: Error) => void): boolean {
    const packet = JSON.parse(data.subarray(8).toString('utf8')) as Record<string, unknown>;
    const event = packet['event'] as Record<string, unknown> | undefined;
    const payloadFromClient = event?.['payload_from_client'] as Record<string, unknown> | undefined;
    if (payloadFromClient) {
      this.request = JSON.parse(String(payloadFromClient['payload_string'])) as Record<string, unknown>;
      const customResponse = {
        request: {
          forward_client_payload: {
            client_id: 1,
            payload_string: JSON.stringify({
              type: -1000,
              requestId: this.request['requestId'],
              body: { handled: true, data: { contractVersion: 1 } },
            }),
          },
          request_id: 'device-response-1',
        },
      };
      queueMicrotask(() => this.emit('data', encodeTestPacket(customResponse)));
    }
    callback?.();
    return true;
  }

  destroy(): this {
    this.emit('close');
    return this;
  }
}

describe('DaemonConnection', () => {
  it('surfaces runtime error responses from debugger requests', async () => {
    const socket = new ErrorResponseSocket();
    const connection = new DaemonConnection(socket as unknown as Socket);

    try {
      await expectAsync(connection.dumpHeap('1', false)).toBeRejectedWithError('Heap dump failed.');
    } finally {
      connection.close();
    }
  });

  it('sends custom debugger messages through the shared runtime request type', async () => {
    const socket = new CustomResponseSocket();
    const connection = new DaemonConnection(socket as unknown as Socket);

    try {
      const response = await connection.customRequest('1', 'ValdiDebuggerInput', { type: 'capabilities' }, 5000);

      expect(socket.request).toEqual(
        jasmine.objectContaining({
          type: 1000,
          body: {
            identifier: 'ValdiDebuggerInput',
            data: { type: 'capabilities' },
          },
        }),
      );
      expect(response).toEqual({ handled: true, data: { contractVersion: 1 } });
    } finally {
      connection.close();
    }
  });
});
