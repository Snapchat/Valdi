import 'jasmine';
import {
  MAX_WEB_PREVIEW_TRACE_EVENTS,
  WEB_PREVIEW_TRACE_RESULT_TTL_MS,
  WEB_PREVIEW_TRACE_WATCHDOG_MS,
  WebPreviewPerformanceController,
  type WebPreviewPerformanceControllerDependencies,
  type WebPreviewPerformanceIdentity,
  type WebPreviewTraceCapture,
  WebPreviewTraceLifecycleError,
  WebPreviewTraceNormalizer,
  type WebPreviewTraceRecorderLike,
  normalizeWebPreviewPerformanceMetrics,
  saturatingWebPreviewTraceEventCount,
} from './webPreviewPerformance';

const FIRST_IDENTITY: WebPreviewPerformanceIdentity = {
  applicationUrl: 'http://127.0.0.1:54321/index.html',
  debuggingPort: 9222,
  inspectedUrl: 'http://127.0.0.1:54321/index.html?valdiDebugger=1',
  sessionId: 'web-preview',
  targetNonce: '0123456789abcdef',
};

const SECOND_IDENTITY: WebPreviewPerformanceIdentity = {
  ...FIRST_IDENTITY,
  inspectedUrl: 'http://127.0.0.1:54321/index.html?screen=second&valdiDebugger=1',
  targetNonce: 'fedcba9876543210',
};

const DUPLICATE_URL_IDENTITY: WebPreviewPerformanceIdentity = {
  ...FIRST_IDENTITY,
  targetNonce: 'duplicate12345678',
};

function capture(startedAtEpochMs: number): WebPreviewTraceCapture {
  return {
    browserMetrics: { TaskDurationMs: 12 },
    droppedTraceEventCount: 0,
    elapsedMs: 25,
    rendererTracingEnabled: true,
    startedAtEpochMs,
    timedOut: false,
    traces: [{ endMicros: 20, startMicros: 10, threadId: 1, trace: 'Valdi.Renderer.onRender.Example' }],
  };
}

class FakeRecorder implements WebPreviewTraceRecorderLike {
  closed = false;
  stopError: Error | undefined;
  readonly stopArguments: boolean[] = [];

  constructor(
    private readonly startedAtEpochMs: number,
    private readonly result: WebPreviewTraceCapture,
  ) {}

  close(): void {
    this.closed = true;
  }

  status(nowMs: number) {
    return {
      completedRecordingAvailable: false,
      elapsedMs: nowMs - this.startedAtEpochMs,
      recording: true,
      rendererTracingEnabled: true,
      startedAtEpochMs: this.startedAtEpochMs,
      tracingSupported: true as const,
    };
  }

  stop(timedOut: boolean): Promise<WebPreviewTraceCapture> {
    this.stopArguments.push(timedOut);
    this.closed = true;
    if (this.stopError) return Promise.reject(this.stopError);
    return Promise.resolve({ ...this.result, timedOut });
  }

  stopForRecovery(): Promise<void> {
    return this.stop(false).then(() => {});
  }
}

interface ControllerHarness {
  controller: WebPreviewPerformanceController;
  recorders: FakeRecorder[];
  waits: number[];
  advance(durationMs: number): void;
  runTimer(durationMs: number): Promise<void>;
  setOwnerPresence(value: boolean | Error): void;
}

function createControllerHarness(): ControllerHarness {
  let nowMs = 1000;
  let timerId = 0;
  const timers = new Map<NodeJS.Timeout, { callback: () => void; durationMs: number }>();
  const recorders: FakeRecorder[] = [];
  const waits: number[] = [];
  let ownerPresence: boolean | Error = true;
  const dependencies: WebPreviewPerformanceControllerDependencies = {
    clearTimer: timer => timers.delete(timer),
    enableTracing: identity => Promise.resolve(`${identity.inspectedUrl}&valdiTrace=chrome`),
    now: () => nowMs,
    readSnapshot: () =>
      Promise.resolve({
        mainThread: {},
        memory: null,
        navigation: {},
        paints: [],
        rendererTracingEnabled: false,
        resourceCount: 0,
        transferSize: 0,
        uptimeMs: 0,
      }),
    setTimer: (callback, durationMs) => {
      const timer = { timerId: ++timerId } as unknown as NodeJS.Timeout;
      timers.set(timer, { callback, durationMs });
      return timer;
    },
    startRecorder: () => {
      const recorder = new FakeRecorder(nowMs, capture(nowMs));
      recorders.push(recorder);
      return Promise.resolve(recorder);
    },
    targetPresent: () =>
      ownerPresence instanceof Error ? Promise.reject(ownerPresence) : Promise.resolve(ownerPresence),
    wait: durationMs => {
      waits.push(durationMs);
      nowMs += durationMs;
      return Promise.resolve();
    },
  };
  return {
    advance: durationMs => {
      nowMs += durationMs;
    },
    controller: new WebPreviewPerformanceController(dependencies),
    recorders,
    runTimer: async durationMs => {
      const entry = Array.from(timers.values()).find(candidate => candidate.durationMs === durationMs);
      if (!entry) throw new Error(`No ${durationMs.toString()}ms timer is scheduled.`);
      entry.callback();
      await new Promise<void>(resolve => setImmediate(resolve));
    },
    setOwnerPresence: value => {
      ownerPresence = value;
    },
    waits,
  };
}

describe('web preview performance', () => {
  it('incrementally normalizes allowlisted browser and Valdi events without retaining unrelated payloads', () => {
    const normalizer = new WebPreviewTraceNormalizer();
    normalizer.accept({
      args: { data: { end: 1300, name: 'Valdi.Renderer.onRender.Example', start: 1000 } },
      name: 'TimeStamp',
      ph: 'I',
      tid: 7,
      ts: 1100,
    });
    normalizer.accept({ args: { private: 'discarded' }, dur: 500, name: 'Layout', ph: 'X', tid: 7, ts: 1400 });
    normalizer.accept({ id: 'render', name: 'Valdi.StateChange', ph: 'B', tid: 7, ts: 2000 });
    normalizer.accept({ id: 'render', name: 'Valdi.StateChange', ph: 'E', tid: 7, ts: 2300 });
    normalizer.accept({ args: { large: 'x'.repeat(100_000) }, name: 'Unrelated', ph: 'X', tid: 7, ts: 2500 });

    const result = normalizer.result();
    expect(result).toEqual({
      droppedTraceEventCount: 0,
      traces: [
        { endMicros: 1300, startMicros: 1000, threadId: 7, trace: 'Valdi.Renderer.onRender.Example' },
        { endMicros: 1900, startMicros: 1400, threadId: 7, trace: 'Browser.Layout.Layout' },
        { endMicros: 2300, startMicros: 2000, threadId: 7, trace: 'Valdi.StateChange' },
      ],
    });
    expect(JSON.stringify(result)).not.toContain('private');
    expect(JSON.stringify(result)).not.toContain('Unrelated');
  });

  it('uses collision-free begin/end identities and counts unfinished pairs as dropped', () => {
    const normalizer = new WebPreviewTraceNormalizer();
    normalizer.accept({ id: 'c', name: 'Valdi.a:b', ph: 'B', tid: 1, ts: 10 });
    normalizer.accept({ id: 'b:c', name: 'Valdi.a', ph: 'B', tid: 1, ts: 20 });
    normalizer.accept({ id: 'c', name: 'Valdi.a:b', ph: 'E', tid: 1, ts: 30 });
    normalizer.accept({ id: 'unfinished', name: 'Valdi.pending', ph: 'B', tid: 1, ts: 40 });

    const result = normalizer.result();
    expect(result.traces).toEqual([{ endMicros: 30, startMicros: 10, threadId: 1, trace: 'Valdi.a:b' }]);
    expect(result.droppedTraceEventCount).toBe(2);
    expect(normalizer.result()).toEqual(result);
  });

  it('retains only the fixed Chromium metric whitelist in a prototype-safe record', () => {
    const metrics = normalizeWebPreviewPerformanceMetrics({
      metrics: [
        { name: 'TaskDuration', value: 1.5 },
        { name: 'JSHeapUsedSize', value: 2048 },
        { name: '__proto__', value: 1 },
        { name: 'PrivateMetric', value: 42 },
      ],
    });

    expect(Object.getPrototypeOf(metrics)).toBeNull();
    expect({ ...metrics }).toEqual({ JSHeapUsedSize: 2048, TaskDuration: 1.5 });
    expect(metrics['PrivateMetric']).toBeUndefined();
  });

  it('enforces the event and UTF-8 trace-name bounds during normalization', () => {
    const normalizer = new WebPreviewTraceNormalizer();
    const allowedName = `Valdi.${'é'.repeat(1021)}`;
    const oversizedName = `Valdi.${'é'.repeat(1022)}`;
    normalizer.accept({ name: allowedName, ph: 'I', tid: 1, ts: 1 });
    normalizer.accept({ name: oversizedName, ph: 'I', tid: 1, ts: 2 });
    for (let index = 1; index <= MAX_WEB_PREVIEW_TRACE_EVENTS; index++) {
      normalizer.accept({ name: 'Layout', ph: 'X', tid: 1, ts: index + 2 });
    }

    const result = normalizer.result();
    expect(result.traces.length).toBe(MAX_WEB_PREVIEW_TRACE_EVENTS);
    expect(result.traces[0]?.trace).toBe(allowedName);
    expect(result.traces.some(trace => trace.trace === oversizedName)).toBeFalse();
    expect(result.droppedTraceEventCount).toBe(1);
    expect(saturatingWebPreviewTraceEventCount(Number.MAX_SAFE_INTEGER - 1, 10)).toBe(Number.MAX_SAFE_INTEGER);
  });

  it('keeps one global recording when another same-URL tab still has the owner nonce', async () => {
    const harness = createControllerHarness();
    await harness.controller.start(FIRST_IDENTITY);

    await expectAsync(harness.controller.status(SECOND_IDENTITY)).toBeRejectedWithError(
      'Another inspected web preview owns the current Chromium performance trace.',
    );
    await expectAsync(harness.controller.stop(SECOND_IDENTITY)).toBeRejectedWithError(
      'Another inspected web preview owns the current Chromium performance trace.',
    );
    await expectAsync(harness.controller.status(DUPLICATE_URL_IDENTITY)).toBeRejectedWithError(
      'Another inspected web preview owns the current Chromium performance trace.',
    );
    await expectAsync(harness.controller.start(SECOND_IDENTITY)).toBeRejectedWithError(
      'Another inspected web preview owns the current Chromium performance trace.',
    );

    const result = await harness.controller.stop(FIRST_IDENTITY);
    expect(result.traces.length).toBe(1);
    expect(harness.recorders[0]?.stopArguments).toEqual([false]);
  });

  it('ends and replaces an old recording only after its nonce-bound target is verified missing', async () => {
    const harness = createControllerHarness();
    await harness.controller.start(FIRST_IDENTITY);
    harness.setOwnerPresence(false);

    const replacement = await harness.controller.start(DUPLICATE_URL_IDENTITY);

    expect(replacement.recording).toBeTrue();
    expect(harness.recorders.length).toBe(2);
    expect(harness.recorders[0]?.stopArguments).toEqual([false]);
    await harness.controller.stop(DUPLICATE_URL_IDENTITY);
  });

  it('retains the old owner when its trace cannot be ended after the target disappears', async () => {
    const harness = createControllerHarness();
    await harness.controller.start(FIRST_IDENTITY);
    const recorder = harness.recorders[0];
    if (!recorder) throw new Error('Expected an active fake recorder.');
    recorder.stopError = new Error('Synthetic Tracing.end failure.');
    harness.setOwnerPresence(false);

    await expectAsync(harness.controller.start(DUPLICATE_URL_IDENTITY)).toBeRejectedWithError(
      'Could not end the previous web preview performance trace after its inspected target disappeared: Synthetic Tracing.end failure.',
    );
    expect(harness.recorders.length).toBe(1);
    expect(recorder.stopArguments).toEqual([false]);
    await expectAsync(harness.controller.status(DUPLICATE_URL_IDENTITY)).toBeRejectedWithError(
      'Could not end the previous web preview performance trace after its inspected target disappeared: Synthetic Tracing.end failure.',
    );
  });

  it('fails closed when the old owner target cannot be verified present or missing', async () => {
    const harness = createControllerHarness();
    await harness.controller.start(FIRST_IDENTITY);
    harness.setOwnerPresence(new Error('Synthetic discovery uncertainty.'));

    await expectAsync(harness.controller.start(DUPLICATE_URL_IDENTITY)).toBeRejectedWithError(
      'Synthetic discovery uncertainty.',
    );
    expect(harness.recorders[0]?.stopArguments).toEqual([]);
    harness.setOwnerPresence(true);
    await harness.controller.stop(FIRST_IDENTITY);
  });

  it('finalizes on the watchdog and expires an undelivered result after sixty seconds', async () => {
    const harness = createControllerHarness();
    await harness.controller.start(FIRST_IDENTITY);
    harness.advance(WEB_PREVIEW_TRACE_WATCHDOG_MS);
    await harness.runTimer(WEB_PREVIEW_TRACE_WATCHDOG_MS);

    expect(await harness.controller.status(FIRST_IDENTITY)).toEqual(
      jasmine.objectContaining({ completedRecordingAvailable: true, recording: false }),
    );
    expect(harness.recorders[0]?.stopArguments).toEqual([true]);

    const firstDelivery = await harness.controller.stop(FIRST_IDENTITY);
    const replay = await harness.controller.stop(FIRST_IDENTITY);
    expect(firstDelivery.timedOut).toBeTrue();
    expect(replay).toEqual(firstDelivery);

    await harness.controller.start(FIRST_IDENTITY);
    harness.advance(WEB_PREVIEW_TRACE_WATCHDOG_MS);
    await harness.runTimer(WEB_PREVIEW_TRACE_WATCHDOG_MS);

    harness.advance(WEB_PREVIEW_TRACE_RESULT_TTL_MS);
    await harness.runTimer(WEB_PREVIEW_TRACE_RESULT_TTL_MS);
    expect(await harness.controller.status(FIRST_IDENTITY)).toEqual(
      jasmine.objectContaining({ completedRecordingAvailable: false, recording: false }),
    );
    await expectAsync(harness.controller.stop(FIRST_IDENTITY)).toBeRejectedWithError(
      'No Chromium performance trace is available for the inspected web preview.',
    );
  });

  it('uses the requested bounded duration for one-shot capture', async () => {
    const harness = createControllerHarness();

    const result = await harness.controller.capture(FIRST_IDENTITY, 1250);

    expect(harness.waits).toEqual([1250]);
    expect(result.timedOut).toBeFalse();
    expect(harness.recorders[0]?.stopArguments).toEqual([false]);
    await expectAsync(harness.runTimer(WEB_PREVIEW_TRACE_WATCHDOG_MS)).toBeRejectedWithError(
      `No ${WEB_PREVIEW_TRACE_WATCHDOG_MS.toString()}ms timer is scheduled.`,
    );
  });

  it('retains fail-closed ownership when explicit Stop cannot confirm trace termination', async () => {
    const harness = createControllerHarness();
    await harness.controller.start(FIRST_IDENTITY);
    const recorder = harness.recorders[0];
    if (!recorder) throw new Error('Expected an active fake recorder.');
    recorder.stopError = new Error('Synthetic completion failure.');

    await expectAsync(harness.controller.stop(FIRST_IDENTITY)).toBeRejectedWithError('Synthetic completion failure.');
    await expectAsync(harness.controller.status(FIRST_IDENTITY)).toBeRejectedWithError('Synthetic completion failure.');
    await expectAsync(harness.controller.start(FIRST_IDENTITY)).toBeRejectedWithError('Synthetic completion failure.');
  });

  it('releases explicit Stop ownership when the trace ended before result acceptance failed', async () => {
    const harness = createControllerHarness();
    await harness.controller.start(FIRST_IDENTITY);
    const recorder = harness.recorders[0];
    if (!recorder) throw new Error('Expected an active fake recorder.');
    recorder.stopError = new WebPreviewTraceLifecycleError('Synthetic post-trace validation failure.', true, null);

    await expectAsync(harness.controller.stop(FIRST_IDENTITY)).toBeRejectedWithError(
      'Synthetic post-trace validation failure.',
    );
    await expectAsync(harness.controller.start(FIRST_IDENTITY)).toBeResolved();
  });

  it('acknowledges and clears an undelivered watchdog completion error through stop', async () => {
    const harness = createControllerHarness();
    await harness.controller.start(FIRST_IDENTITY);
    const recorder = harness.recorders[0];
    if (!recorder) throw new Error('Expected an active fake recorder.');
    recorder.stopError = new WebPreviewTraceLifecycleError('Synthetic watchdog failure.', true, null);
    harness.advance(WEB_PREVIEW_TRACE_WATCHDOG_MS);
    await harness.runTimer(WEB_PREVIEW_TRACE_WATCHDOG_MS);

    expect(await harness.controller.status(FIRST_IDENTITY)).toEqual(
      jasmine.objectContaining({ completionError: 'Synthetic watchdog failure.', recording: false }),
    );
    await expectAsync(harness.controller.stop(FIRST_IDENTITY)).toBeRejectedWithError('Synthetic watchdog failure.');
    await expectAsync(harness.controller.start(FIRST_IDENTITY)).toBeResolved();
  });

  it('closes an active recorder and cancels its watchdog during shutdown', async () => {
    const harness = createControllerHarness();
    await harness.controller.start(FIRST_IDENTITY);

    await harness.controller.close();

    expect(harness.recorders[0]?.closed).toBeTrue();
    expect(harness.recorders[0]?.stopArguments).toEqual([false]);
    expect(await harness.controller.status(FIRST_IDENTITY)).toEqual(
      jasmine.objectContaining({ completedRecordingAvailable: false, recording: false }),
    );
    await expectAsync(harness.runTimer(WEB_PREVIEW_TRACE_WATCHDOG_MS)).toBeRejectedWithError(
      `No ${WEB_PREVIEW_TRACE_WATCHDOG_MS.toString()}ms timer is scheduled.`,
    );
  });
});
