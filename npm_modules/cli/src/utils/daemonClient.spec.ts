import 'jasmine';
import { EventEmitter } from 'node:events';
import type { Socket } from 'node:net';
import {
  DaemonConnection,
  DaemonMsgType,
  DaemonProtocolError,
  MAX_DAEMON_INNER_PAYLOAD_BYTES,
  MAX_DAEMON_PACKET_PAYLOAD_BYTES,
  MAX_DAEMON_TRACE_PAYLOAD_BYTES,
} from './daemonClient';

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
class TestResponseSocket extends EventEmitter {
  readonly remotePort = 13_591;
  readonly requests: Array<Record<string, unknown>> = [];

  constructor(
    private readonly responseType: number,
    private readonly responseBody: unknown,
  ) {
    super();
  }

  write(data: Buffer, callback?: (error?: Error) => void): boolean {
    const packet = JSON.parse(data.subarray(8).toString('utf8')) as Record<string, unknown>;
    const event = packet['event'] as Record<string, unknown> | undefined;
    const payloadFromClient = event?.['payload_from_client'] as Record<string, unknown> | undefined;
    if (payloadFromClient) {
      const request = JSON.parse(String(payloadFromClient['payload_string'])) as Record<string, unknown>;
      this.requests.push(request);
      const errorResponse = {
        request: {
          forward_client_payload: {
            client_id: 1,
            payload_string: JSON.stringify({
              type: this.responseType,
              requestId: request['requestId'],
              body: this.responseBody,
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
              type: DaemonMsgType.CUSTOM_RESPONSE,
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

// net.Socket uses EventEmitter semantics, which this protocol test mirrors.
// eslint-disable-next-line unicorn/prefer-event-target
class TraceResponseSocket extends EventEmitter {
  readonly remotePort = 13_591;
  readonly messageTypes: number[] = [];
  readonly messageBodies: Array<Record<string, unknown>> = [];

  constructor(
    private readonly makeResponse?: (
      messageType: number,
      requestBody: Record<string, unknown>,
    ) => Record<string, unknown>,
  ) {
    super();
  }

  write(data: Buffer, callback?: (error?: Error) => void): boolean {
    const packet = JSON.parse(data.subarray(8).toString('utf8')) as Record<string, unknown>;
    const event = packet['event'] as Record<string, unknown> | undefined;
    const payloadFromClient = event?.['payload_from_client'] as Record<string, unknown> | undefined;
    if (payloadFromClient) {
      const request = JSON.parse(String(payloadFromClient['payload_string'])) as Record<string, unknown>;
      const messageType = Number(request['type']);
      const requestBody = request['body'] as Record<string, unknown>;
      this.messageTypes.push(messageType);
      this.messageBodies.push(requestBody);
      const recording = messageType === DaemonMsgType.PERFORMANCE_TRACE_START_REQUEST;
      const defaultBody: Record<string, unknown> = {
        recording,
        contextId: recording ? requestBody['contextId'] : undefined,
        completedRecordingAvailable: false,
        rendererTracingEnabled: recording,
        tracingSupported: true,
      };
      if (messageType === DaemonMsgType.PERFORMANCE_TRACE_STOP_REQUEST) {
        defaultBody['contextId'] = requestBody['contextId'];
        defaultBody['traces'] = [];
        defaultBody['traceEventCount'] = 0;
        defaultBody['droppedTraceEventCount'] = 0;
        defaultBody['timedOut'] = false;
      }
      const innerResponse = this.makeResponse?.(messageType, requestBody) ?? {
        type: -messageType,
        body: defaultBody,
      };
      const response = {
        request: {
          forward_client_payload: {
            client_id: 1,
            payload_string: JSON.stringify({
              requestId: request['requestId'],
              ...innerResponse,
            }),
          },
          request_id: `device-response-${this.messageTypes.length}`,
        },
      };
      queueMicrotask(() => this.emit('data', encodeTestPacket(response)));
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
class PassiveSocket extends EventEmitter {
  readonly remotePort = 13_591;
  destroyedError: Error | undefined;

  write(_data: Buffer, callback?: (error?: Error) => void): boolean {
    callback?.();
    return true;
  }

  destroy(error?: Error): this {
    this.destroyedError = error;
    this.emit('close');
    return this;
  }
}

function makeTraceStopResponse(trace: Record<string, unknown>): (messageType: number) => Record<string, unknown> {
  return messageType => ({
    type: -messageType,
    body: {
      recording: false,
      contextId: 'root',
      completedRecordingAvailable: false,
      rendererTracingEnabled: false,
      tracingSupported: true,
      traces: [trace],
      traceEventCount: 1,
      droppedTraceEventCount: 0,
      timedOut: false,
    },
  });
}

describe('DaemonConnection', () => {
  it('surfaces runtime error responses from debugger requests', async () => {
    const socket = new TestResponseSocket(-1, { message: 'Heap dump failed.' });
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

  it('accepts only well-formed handled custom responses', async () => {
    const socket = new TestResponseSocket(-1000, { handled: true, data: { providers: [] } });
    const connection = new DaemonConnection(socket as unknown as Socket);

    try {
      await expectAsync(connection.customRequest('1', 'ValdiDebuggerProviders', { action: 'list' })).toBeResolvedTo({
        handled: true,
        data: { providers: [] },
      });
      expect(socket.requests[0]?.['type']).toBe(1000);
    } finally {
      connection.close();
    }
  });

  it('routes renderer trace status, start, and stop through the runtime protocol', async () => {
    const socket = new TraceResponseSocket();
    const connection = new DaemonConnection(socket as unknown as Socket);

    try {
      const status = await connection.performanceTraceStatus('1', { contextId: 'root' }, 1000);
      const start = await connection.performanceTraceStart('1', { contextId: 'root', rendererTracing: true }, 1000);
      const stop = await connection.performanceTraceStop('1', { contextId: 'root' }, 1000);

      expect(socket.messageTypes).toEqual([
        DaemonMsgType.PERFORMANCE_TRACE_STATUS_REQUEST,
        DaemonMsgType.PERFORMANCE_TRACE_START_REQUEST,
        DaemonMsgType.PERFORMANCE_TRACE_STOP_REQUEST,
      ]);
      expect(socket.messageBodies).toEqual([
        { contextId: 'root' },
        { contextId: 'root', rendererTracing: true },
        { contextId: 'root' },
      ]);
      expect(status['recording']).toBeFalse();
      expect(start['recording']).toBeTrue();
      expect(stop['recording']).toBeFalse();
      expect(stop['contextId']).toBe('root');
      expect(stop.traces).toEqual([]);
    } finally {
      connection.close();
    }
  });

  it('accepts a well-formed unhandled custom response without fabricated data', async () => {
    const socket = new TestResponseSocket(-1000, { handled: false });
    const connection = new DaemonConnection(socket as unknown as Socket);
    try {
      await expectAsync(connection.customRequest('1', 'ValdiDebuggerProviders', { action: 'list' })).toBeResolvedTo({
        handled: false,
      });
    } finally {
      connection.close();
    }
  });

  it('rejects a trace response with the wrong discriminator', async () => {
    const socket = new TraceResponseSocket(() => ({
      type: DaemonMsgType.PERFORMANCE_TRACE_STOP_RESPONSE,
      body: {},
    }));
    const connection = new DaemonConnection(socket as unknown as Socket);

    try {
      await expectAsync(connection.performanceTraceStatus('1', { contextId: 'root' }, 1000)).toBeRejectedWithError(
        /expected -6/,
      );
    } finally {
      connection.close();
    }
  });

  it('rejects a trace response with a malformed body schema', async () => {
    const socket = new TraceResponseSocket(messageType => ({
      type: -messageType,
      body: { recording: 'yes' },
    }));
    const connection = new DaemonConnection(socket as unknown as Socket);

    try {
      await expectAsync(connection.performanceTraceStatus('1', { contextId: 'root' }, 1000)).toBeRejectedWithError(
        /invalid recording/,
      );
    } finally {
      connection.close();
    }
  });

  it('enforces the renderer trace name and timestamp contract', async () => {
    const invalidTraces = [
      { trace: '', startMicros: 1, endMicros: 2, threadId: 1 },
      { trace: 'é'.repeat(1025), startMicros: 1, endMicros: 2, threadId: 1 },
      { trace: 'backwards', startMicros: 2, endMicros: 1, threadId: 1 },
      { trace: 'unsafe', startMicros: 1, endMicros: 2, threadId: Number.MAX_SAFE_INTEGER + 1 },
    ];

    for (const trace of invalidTraces) {
      const socket = new TraceResponseSocket(makeTraceStopResponse(trace));
      const connection = new DaemonConnection(socket as unknown as Socket);
      try {
        await expectAsync(connection.performanceTraceStop('1', { contextId: 'root' }, 1000)).toBeRejectedWithError(
          /malformed trace event/,
        );
      } finally {
        connection.close();
      }
    }

    const validSocket = new TraceResponseSocket(
      makeTraceStopResponse({ trace: 'é'.repeat(1024), startMicros: 1, endMicros: 2, threadId: 1 }),
    );
    const validConnection = new DaemonConnection(validSocket as unknown as Socket);
    try {
      const result = await validConnection.performanceTraceStop('1', { contextId: 'root' }, 1000);
      expect(result.traces.length).toBe(1);
    } finally {
      validConnection.close();
    }
  });

  it('rejects oversized trace context and completion-error metadata', async () => {
    const socket = new TraceResponseSocket(messageType => ({
      type: -messageType,
      body: {
        recording: false,
        contextId: 'x'.repeat(4097),
        completedRecordingAvailable: false,
        completionError: 'x'.repeat(64 * 1024 + 1),
        rendererTracingEnabled: false,
        tracingSupported: true,
      },
    }));
    const connection = new DaemonConnection(socket as unknown as Socket);

    try {
      await expectAsync(connection.performanceTraceStatus('1', { contextId: 'root' }, 1000)).toBeRejectedWithError(
        /invalid contextId/,
      );
    } finally {
      connection.close();
    }
  });

  [
    { body: { handled: true, data: {} }, label: 'response type', type: -2 },
    { body: null, label: 'response body', type: -1000 },
    { body: { handled: 'yes', data: {} }, label: 'handled field', type: -1000 },
    { body: { handled: true, data: [] }, label: 'handled response data', type: -1000 },
    { body: { handled: false, data: 'invalid' }, label: 'optional response data', type: -1000 },
    { body: { handled: true, data: { value: 'x'.repeat(128 * 1024) } }, label: 'oversized response data', type: -1000 },
  ].forEach(testCase => {
    it(`rejects malformed custom ${testCase.label}`, async () => {
      const socket = new TestResponseSocket(testCase.type, testCase.body);
      const connection = new DaemonConnection(socket as unknown as Socket);
      try {
        await expectAsync(
          connection.customRequest('1', 'ValdiDebuggerProviders', { action: 'list' }),
        ).toBeRejectedWithError(DaemonProtocolError);
      } finally {
        connection.close();
      }
    });
  });

  it('rejects unbounded identifiers and request data before transport', async () => {
    const socket = new TestResponseSocket(-1000, { handled: true, data: {} });
    const connection = new DaemonConnection(socket as unknown as Socket);
    try {
      await expectAsync(connection.customRequest('1', 'x'.repeat(129), {})).toBeRejectedWithError(
        DaemonProtocolError,
        /1 to 128/,
      );
      await expectAsync(
        connection.customRequest('1', 'bounded', { value: 'x'.repeat(128 * 1024) }),
      ).toBeRejectedWithError(DaemonProtocolError, /exceeds 128 KiB/);
      expect(socket.requests).toEqual([]);
    } finally {
      connection.close();
    }
  });

  it('accepts a valid heap response larger than the trace-specific payload limit', async () => {
    const heapDumpJSON = 'x'.repeat(MAX_DAEMON_TRACE_PAYLOAD_BYTES + 1024);
    const socket = new TraceResponseSocket(messageType => ({
      type: -messageType,
      body: { memoryUsageBytes: heapDumpJSON.length, heapDumpJSON },
    }));
    const connection = new DaemonConnection(socket as unknown as Socket);

    try {
      const result = (await connection.dumpHeap('1', false)) as Record<string, unknown>;
      expect((result['heapDumpJSON'] as string).length).toBe(heapDumpJSON.length);
    } finally {
      connection.close();
    }
  });

  it('rejects a trace response larger than the trace-specific payload limit', async () => {
    const socket = new TraceResponseSocket(messageType => ({
      type: -messageType,
      body: {
        recording: false,
        completedRecordingAvailable: false,
        rendererTracingEnabled: false,
        tracingSupported: true,
        padding: 'x'.repeat(MAX_DAEMON_TRACE_PAYLOAD_BYTES),
      },
    }));
    const connection = new DaemonConnection(socket as unknown as Socket);

    try {
      await expectAsync(connection.performanceTraceStatus('1', { contextId: 'root' }, 1000)).toBeRejectedWithError(
        /performance trace payload exceeds/,
      );
    } finally {
      connection.close();
    }
  });

  it('rejects an oversized outer packet from its header before buffering its payload', () => {
    const socket = new PassiveSocket();
    const connection = new DaemonConnection(socket as unknown as Socket);
    const header = Buffer.alloc(8);
    TEST_MAGIC.copy(header, 0);
    header.writeUInt32LE(MAX_DAEMON_PACKET_PAYLOAD_BYTES + 1, 4);

    socket.emit('data', header);

    expect(socket.destroyedError?.message).toContain('payload exceeds');
    connection.close();
  });

  it('keeps the generic inner payload limit larger than the trace-specific limit', () => {
    expect(MAX_DAEMON_INNER_PAYLOAD_BYTES).toBeGreaterThan(MAX_DAEMON_TRACE_PAYLOAD_BYTES);
  });
});
