import 'jasmine/src/jasmine';
import {
  PERFORMANCE_TRACE_HANDLER_TIMEOUT_MS,
  PERFORMANCE_TRACE_RESULT_TTL_MS,
  PerformanceTraceMessageHandler,
  PerformanceTraceRuntime,
} from '../src/debugging/PerformanceTraceMessageHandler';
import {
  MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES,
  MAX_PERFORMANCE_TRACE_ERROR_BYTES,
  MAX_PERFORMANCE_TRACE_MESSAGE_BYTES,
  Messages,
  PerformanceTraceEvent,
  PerformanceTraceStartRequestBody,
  PerformanceTraceStopBody,
} from '../src/debugging/Messages';
import {
  decodeLegacyTraceRecordingResult,
  decodeTraceRecordingResultWithNativeStats,
  MAX_SERIALIZED_TRACE_EVENTS_BYTES,
  TraceRecordingResult,
} from '../src/utils/Trace';

class FakeRenderer {
  readonly tracingValues: boolean[] = [];

  constructor(private tracingEnabled: boolean) {}

  setTracingEnabled(enabled: boolean): void {
    this.tracingEnabled = enabled;
    this.tracingValues.push(enabled);
  }

  isTracingEnabled(): boolean {
    return this.tracingEnabled;
  }
}

interface FakeRuntimeControls {
  runtime: PerformanceTraceRuntime;
  cancelledTimeoutIds: number[];
  advanceTimeMs(value: number): void;
  fireTimeout(): void;
  setDroppedTraceEventCount(value: number): void;
  setNowMs(value: number): void;
  setStartError(error: Error | undefined): void;
  setStopError(error: Error | undefined): void;
  stopRecordingIds: number[];
  timeoutMs: number | undefined;
}

function createRuntime(traces: PerformanceTraceEvent[]): FakeRuntimeControls {
  let nowMs = 100;
  let droppedTraceEventCount = 0;
  let startError: Error | undefined;
  let stopError: Error | undefined;
  let nextTimeoutId = 7;
  const scheduledTimeouts = new Map<number, { callback: () => void; deadlineMs: number; timeoutMs: number }>();
  const stopRecordingIds: number[] = [];
  const cancelledTimeoutIds: number[] = [];
  const runDueTimeouts = (): void => {
    for (;;) {
      const nextTimeout = Array.from(scheduledTimeouts.entries())
        .filter(([, timeout]) => timeout.deadlineMs <= nowMs)
        .sort((left, right) => left[1].deadlineMs - right[1].deadlineMs)[0];
      if (!nextTimeout) return;
      scheduledTimeouts.delete(nextTimeout[0]);
      nextTimeout[1].callback();
    }
  };
  const runtime: PerformanceTraceRuntime = {
    cancelTimeout: timeoutId => {
      cancelledTimeoutIds.push(timeoutId);
      scheduledTimeouts.delete(timeoutId);
    },
    isTracingSupported: () => true,
    nowMs: () => nowMs,
    nowEpochMs: () => 1000,
    scheduleTimeout: (callback, delayMs) => {
      const timeoutId = nextTimeoutId++;
      scheduledTimeouts.set(timeoutId, { callback, deadlineMs: nowMs + delayMs, timeoutMs: delayMs });
      return timeoutId;
    },
    startTraceRecording: () => {
      if (startError) {
        throw startError;
      }
      return 42;
    },
    stopTraceRecording: recordingId => {
      stopRecordingIds.push(recordingId);
      if (stopError) {
        throw stopError;
      }
      return { traces, droppedTraceEventCount };
    },
  };
  return {
    runtime,
    cancelledTimeoutIds,
    advanceTimeMs: value => {
      nowMs += value;
      runDueTimeouts();
    },
    fireTimeout: () => {
      const nextTimeout = Array.from(scheduledTimeouts.values()).sort(
        (left, right) => left.deadlineMs - right.deadlineMs,
      )[0];
      if (!nextTimeout) return;
      nowMs = Math.max(nowMs, nextTimeout.deadlineMs);
      runDueTimeouts();
    },
    setDroppedTraceEventCount: value => {
      droppedTraceEventCount = value;
    },
    setNowMs: value => {
      nowMs = value;
    },
    setStartError: error => {
      startError = error;
    },
    setStopError: error => {
      stopError = error;
    },
    stopRecordingIds,
    get timeoutMs() {
      return Array.from(scheduledTimeouts.values()).sort((left, right) => left.deadlineMs - right.deadlineMs)[0]
        ?.timeoutMs;
    },
  };
}

function createHandler(renderer: FakeRenderer, runtime: PerformanceTraceRuntime): PerformanceTraceMessageHandler {
  return new PerformanceTraceMessageHandler(contextId => (contextId === 'root' ? renderer : undefined), runtime);
}

function serializeTraceStopResult(result: TraceRecordingResult): string {
  const body: PerformanceTraceStopBody = {
    recording: false,
    contextId: 'root',
    completedRecordingAvailable: false,
    completedContextId: undefined,
    completionError: undefined,
    rendererTracingEnabled: false,
    tracingSupported: true,
    startedAtEpochMs: 1000,
    elapsedMs: 10,
    traces: result.traces,
    traceEventCount: result.traces.length,
    droppedTraceEventCount: result.droppedTraceEventCount,
    timedOut: false,
  };
  return Messages.performanceTraceStopResponse('trace-request', body);
}

describe('PerformanceTraceMessageHandler', () => {
  it('starts and stops a recording with trace output and active context status', () => {
    const traces = [{ trace: 'Renderer.onRender.App', startMicros: 10, endMicros: 25, threadId: 1 }];
    const controls = createRuntime(traces);
    const renderer = new FakeRenderer(false);
    const handler = createHandler(renderer, controls.runtime);

    const started = handler.startRecording({ contextId: 'root', rendererTracing: true });
    expect(controls.timeoutMs).toBe(PERFORMANCE_TRACE_HANDLER_TIMEOUT_MS);
    controls.setNowMs(125);
    const stopped = handler.stopRecording({ contextId: 'root' });

    expect(started.recording).toBeTrue();
    expect(started.contextId).toBe('root');
    expect(stopped.contextId).toBe('root');
    expect(stopped.elapsedMs).toBe(25);
    expect(stopped.traces).toEqual(traces);
    expect(stopped.traceEventCount).toBe(1);
    expect(stopped.droppedTraceEventCount).toBe(0);
    expect(stopped.timedOut).toBeFalse();
    expect(controls.stopRecordingIds).toEqual([42]);
    expect(controls.cancelledTimeoutIds).toEqual([7]);
    expect(handler.getStatus().recording).toBeFalse();
    expect(handler.getStatus().contextId).toBeUndefined();
  });

  it('rejects overlapping starts without replacing the active context', () => {
    const controls = createRuntime([]);
    const handler = createHandler(new FakeRenderer(false), controls.runtime);
    handler.startRecording({ contextId: 'root' });

    expect(() => handler.startRecording({ contextId: 'other' })).toThrowError(
      Error,
      'A Valdi performance trace recording is already active.',
    );
    expect(handler.getStatus().contextId).toBe('root');
    handler.abortRecording();
  });

  it('rejects a stop when no recording or completed timeout is available', () => {
    const controls = createRuntime([]);
    const handler = createHandler(new FakeRenderer(false), controls.runtime);

    expect(() => handler.stopRecording({ contextId: 'root' })).toThrowError(
      Error,
      'No Valdi performance trace recording is active or waiting to be retrieved.',
    );
  });

  it('allows idempotent stop retries until the bounded result TTL expires', () => {
    const traces = [{ trace: 'Renderer.onRender.App', startMicros: 10, endMicros: 25, threadId: 1 }];
    const controls = createRuntime(traces);
    const handler = createHandler(new FakeRenderer(false), controls.runtime);
    handler.startRecording({ contextId: 'root' });

    const first = handler.stopRecording({ contextId: 'root' });
    const retry = handler.stopRecording({ contextId: 'root' });

    expect(retry).toEqual(first);
    expect(controls.stopRecordingIds).toEqual([42]);
    controls.advanceTimeMs(PERFORMANCE_TRACE_RESULT_TTL_MS - 1);
    expect(handler.stopRecording({ contextId: 'root' })).toEqual(first);
    controls.advanceTimeMs(1);
    expect(() => handler.stopRecording({ contextId: 'root' })).toThrowError(
      Error,
      'No Valdi performance trace recording is active or waiting to be retrieved.',
    );
  });

  it('does not let a different context stop or relabel the active recording', () => {
    const controls = createRuntime([]);
    const handler = createHandler(new FakeRenderer(false), controls.runtime);
    handler.startRecording({ contextId: 'root' });

    expect(() => handler.stopRecording({ contextId: 'other' })).toThrowError(
      Error,
      'Valdi performance trace recording belongs to context root, not other.',
    );
    expect(handler.getStatus().recording).toBeTrue();
    expect(handler.getStatus().contextId).toBe('root');
    expect(controls.stopRecordingIds).toEqual([]);
    handler.stopRecording({ contextId: 'root' });
  });

  it('cleans up and restores renderer tracing when startTraceRecording fails', () => {
    const controls = createRuntime([]);
    const renderer = new FakeRenderer(false);
    const handler = createHandler(renderer, controls.runtime);
    controls.setStartError(new Error('runtime start failed'));

    expect(() => handler.startRecording({ contextId: 'root', rendererTracing: true })).toThrowError(
      Error,
      'runtime start failed',
    );
    expect(renderer.isTracingEnabled()).toBeFalse();
    expect(handler.getStatus().recording).toBeFalse();

    controls.setStartError(undefined);
    expect(handler.startRecording({ contextId: 'root' }).recording).toBeTrue();
    handler.abortRecording();
  });

  it('restores the renderer tracing flag that existed before capture', () => {
    const controls = createRuntime([]);
    const renderer = new FakeRenderer(true);
    const handler = createHandler(renderer, controls.runtime);

    handler.startRecording({ contextId: 'root', rendererTracing: false });
    expect(renderer.isTracingEnabled()).toBeFalse();
    handler.stopRecording({ contextId: 'root' });

    expect(renderer.isTracingEnabled()).toBeTrue();
    expect(renderer.tracingValues).toEqual([false, true]);
  });

  it('aborts the active context during teardown and restores renderer tracing', () => {
    const controls = createRuntime([]);
    const renderer = new FakeRenderer(false);
    const handler = createHandler(renderer, controls.runtime);
    handler.startRecording({ contextId: 'root', rendererTracing: true });

    handler.abortRecordingForContext('other');
    expect(handler.getStatus().recording).toBeTrue();
    handler.abortRecordingForContext('root');

    expect(handler.getStatus().recording).toBeFalse();
    expect(renderer.isTracingEnabled()).toBeFalse();
    expect(controls.stopRecordingIds).toEqual([42]);
  });

  it('auto-stops before the native leak guard and retains timed-out traces for retrieval', () => {
    const traces = [{ trace: 'Renderer.onRender.App', startMicros: 10, endMicros: 25, threadId: 1 }];
    const controls = createRuntime(traces);
    const renderer = new FakeRenderer(false);
    const handler = createHandler(renderer, controls.runtime);
    handler.startRecording({ contextId: 'root', rendererTracing: true });
    controls.setNowMs(20_100);

    controls.fireTimeout();

    const status = handler.getStatus();
    expect(status.recording).toBeFalse();
    expect(status.completedRecordingAvailable).toBeTrue();
    expect(status.completedContextId).toBe('root');
    expect(renderer.isTracingEnabled()).toBeFalse();
    expect(controls.stopRecordingIds).toEqual([42]);

    const result = handler.stopRecording({ contextId: 'root' });
    expect(result.timedOut).toBeTrue();
    expect(result.traces).toEqual(traces);
    expect(result.completedRecordingAvailable).toBeFalse();
    expect(handler.getStatus().completedRecordingAvailable).toBeFalse();
  });

  it('expires an unretrieved timed-out result after the bounded result TTL', () => {
    const controls = createRuntime([]);
    const handler = createHandler(new FakeRenderer(false), controls.runtime);
    handler.startRecording({ contextId: 'root' });
    controls.fireTimeout();

    expect(handler.getStatus().completedRecordingAvailable).toBeTrue();
    controls.advanceTimeMs(PERFORMANCE_TRACE_RESULT_TTL_MS - 1);
    expect(handler.getStatus().completedRecordingAvailable).toBeTrue();
    controls.advanceTimeMs(1);
    expect(handler.getStatus().completedRecordingAvailable).toBeFalse();
    expect(() => handler.stopRecording({ contextId: 'root' })).toThrowError(
      Error,
      'No Valdi performance trace recording is active or waiting to be retrieved.',
    );
  });

  it('propagates native dropped trace event counts', () => {
    const controls = createRuntime([]);
    controls.setDroppedTraceEventCount(3);
    const handler = createHandler(new FakeRenderer(false), controls.runtime);
    handler.startRecording({ contextId: 'root' });

    const result = handler.stopRecording({ contextId: 'root' });

    expect(result.traceEventCount).toBe(0);
    expect(result.droppedTraceEventCount).toBe(3);
  });

  it('reports a timed-out native stop failure while still restoring renderer state', () => {
    const controls = createRuntime([]);
    const renderer = new FakeRenderer(false);
    const handler = createHandler(renderer, controls.runtime);
    handler.startRecording({ contextId: 'root', rendererTracing: true });
    controls.setStopError(new Error('native stop failed'));

    controls.fireTimeout();

    expect(handler.getStatus().completionError).toContain('native stop failed');
    expect(renderer.isTracingEnabled()).toBeFalse();
    const result = handler.stopRecording({ contextId: 'root' });
    expect(result.timedOut).toBeTrue();
    expect(result.completionError).toContain('native stop failed');
  });

  it('rejects a new start until a completed timed-out recording is retrieved', () => {
    const controls = createRuntime([]);
    const handler = createHandler(new FakeRenderer(false), controls.runtime);
    handler.startRecording({ contextId: 'root' });
    controls.fireTimeout();

    expect(() => handler.startRecording({ contextId: 'root' })).toThrowError(
      Error,
      'A completed timed-out Valdi performance trace for context root is waiting to be retrieved.',
    );
    handler.stopRecording({ contextId: 'root' });
    expect(handler.startRecording({ contextId: 'root' }).recording).toBeTrue();
    handler.abortRecording();
  });

  it('rejects a non-boolean rendererTracing value before changing renderer state', () => {
    const controls = createRuntime([]);
    const renderer = new FakeRenderer(false);
    const handler = createHandler(renderer, controls.runtime);
    const invalidBody = { contextId: 'root', rendererTracing: 'yes' } as unknown as PerformanceTraceStartRequestBody;

    expect(() => handler.startRecording(invalidBody)).toThrowError(
      Error,
      'rendererTracing must be a boolean when provided.',
    );
    expect(renderer.tracingValues).toEqual([]);
  });

  it('rejects a context id whose escaped representation exceeds the runtime bound', () => {
    const controls = createRuntime([]);
    const renderer = new FakeRenderer(false);
    const handler = createHandler(renderer, controls.runtime);

    expect(() =>
      handler.startRecording({ contextId: '\0'.repeat(MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES) }),
    ).toThrowError(
      Error,
      `Valdi performance trace contextId exceeds the ${MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES}-byte serialized limit.`,
    );
    expect(renderer.tracingValues).toEqual([]);
  });

  it('bounds cached completion errors and generic debugger error responses', () => {
    const controls = createRuntime([]);
    const handler = createHandler(new FakeRenderer(false), controls.runtime);
    handler.startRecording({ contextId: 'root' });
    controls.setStopError(new Error('\0'.repeat(MAX_PERFORMANCE_TRACE_ERROR_BYTES)));

    const result = handler.stopRecording({ contextId: 'root' });
    const serializedResult = Messages.performanceTraceStopResponse('trace-request', result);
    const error = new Error('\0'.repeat(MAX_PERFORMANCE_TRACE_ERROR_BYTES));
    error.stack = '\0'.repeat(MAX_PERFORMANCE_TRACE_ERROR_BYTES * 4);
    const serializedError = Messages.errorResponse('trace-request', error);

    expect(JSON.stringify(result.completionError).length).toBeLessThanOrEqual(MAX_PERFORMANCE_TRACE_ERROR_BYTES);
    expect(serializedResult.length).toBeLessThanOrEqual(MAX_PERFORMANCE_TRACE_MESSAGE_BYTES);
    expect(serializedError.length).toBeLessThanOrEqual(MAX_PERFORMANCE_TRACE_MESSAGE_BYTES);
  });

  it('bounds the complete runtime trace response with adversarial metadata', () => {
    const flatEvents: any[] = [];
    for (let index = 0; index < 512; index++) {
      flatEvents.push('\0'.repeat(2048), index, index + 1, 1);
    }
    const traces = decodeLegacyTraceRecordingResult(flatEvents);
    const body: PerformanceTraceStopBody = {
      recording: false,
      contextId: '\0'.repeat(MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES),
      completedRecordingAvailable: false,
      completedContextId: undefined,
      completionError: '\0'.repeat(MAX_PERFORMANCE_TRACE_ERROR_BYTES),
      rendererTracingEnabled: false,
      tracingSupported: true,
      startedAtEpochMs: 1000,
      elapsedMs: 10,
      traces: traces.traces,
      traceEventCount: traces.traces.length,
      droppedTraceEventCount: traces.droppedTraceEventCount,
      timedOut: false,
    };

    const serialized = Messages.performanceTraceStopResponse('trace-request', body);
    const parsed = JSON.parse(serialized) as { body: PerformanceTraceStopBody };

    expect(serialized.length).toBeLessThanOrEqual(MAX_PERFORMANCE_TRACE_MESSAGE_BYTES);
    expect(parsed.body.traceEventCount).toBe(parsed.body.traces.length);
    expect(parsed.body.droppedTraceEventCount).toBeGreaterThanOrEqual(body.droppedTraceEventCount);
    expect(JSON.stringify(parsed.body.contextId).length).toBeLessThanOrEqual(MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES);
    expect(JSON.stringify(parsed.body.completionError).length).toBeLessThanOrEqual(MAX_PERFORMANCE_TRACE_ERROR_BYTES);
  });

  it('bounds legacy runtime trace results and truthfully counts dropped events', () => {
    const legacyResult: any[] = [];
    for (let index = 0; index < 10_001; index++) {
      legacyResult.push(`trace-${index}`, index, index + 1, 1);
    }

    const result = decodeLegacyTraceRecordingResult(legacyResult);

    expect(result.traces.length).toBe(10_000);
    expect(result.droppedTraceEventCount).toBe(1);
  });

  it('preserves JavaScript-safe native drop counts and saturates larger integral values', () => {
    expect(decodeTraceRecordingResultWithNativeStats([Number.MAX_SAFE_INTEGER]).droppedTraceEventCount).toBe(
      Number.MAX_SAFE_INTEGER,
    );
    expect(decodeTraceRecordingResultWithNativeStats([Number.MAX_SAFE_INTEGER + 2]).droppedTraceEventCount).toBe(
      Number.MAX_SAFE_INTEGER,
    );
    expect(decodeTraceRecordingResultWithNativeStats([Number.MAX_VALUE]).droppedTraceEventCount).toBe(
      Number.MAX_SAFE_INTEGER,
    );

    for (const invalidCount of [Number.POSITIVE_INFINITY, Number.NaN, -1, 1.5, '3', undefined]) {
      expect(decodeTraceRecordingResultWithNativeStats([invalidCount]).droppedTraceEventCount).toBe(0);
    }
  });

  it('accounts for JSON escaping before accepting legacy and native trace events', () => {
    const eventCount = 512;
    const escapedTraceName = '\0'.repeat(2048);
    const flatEvents: any[] = [];
    for (let index = 0; index < eventCount; index++) {
      flatEvents.push(escapedTraceName, index, index + 1, 1);
    }

    const legacyResult = decodeLegacyTraceRecordingResult(flatEvents);
    const nativeResult = decodeTraceRecordingResultWithNativeStats([3, ...flatEvents]);

    expect(legacyResult.traces.length).toBeLessThan(eventCount);
    expect(legacyResult.droppedTraceEventCount).toBe(eventCount - legacyResult.traces.length);
    expect(nativeResult.traces.length).toBe(legacyResult.traces.length);
    expect(nativeResult.droppedTraceEventCount).toBe(3 + eventCount - nativeResult.traces.length);
    expect(JSON.stringify(legacyResult.traces).length).toBeLessThanOrEqual(MAX_SERIALIZED_TRACE_EVENTS_BYTES);
    // This payload is ASCII after JSON escaping, so string length is its exact UTF-8 byte length.
    expect(serializeTraceStopResult(legacyResult).length).toBeLessThanOrEqual(4 * 1024 * 1024);
    expect(serializeTraceStopResult(nativeResult).length).toBeLessThanOrEqual(4 * 1024 * 1024);
  });
});
