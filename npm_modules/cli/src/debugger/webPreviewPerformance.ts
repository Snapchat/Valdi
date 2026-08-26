import { ChromiumDevToolsProtocolError } from '../utils/chromiumDevToolsClient';
import {
  OwlChromiumConnection,
  connectToOwlApplication,
  listOwlChromiumTargets,
  matchesOwlApplicationUrl,
} from '../utils/owlCdpClient';

const CHROMIUM_COMMAND_TIMEOUT_MS = 10_000;
const TRACE_COMPLETION_TIMEOUT_MS = 10_000;
const RECORDED_TRACE_TYPE_INSTANT = 1;
const MIN_BROWSER_TASK_DURATION_MICROS = 100;
const MAX_ACTIVE_TRACE_PAIRS = 10_000;
const MAX_TRACE_PAIR_ID_BYTES = 256;
const MAX_PAINT_ENTRIES = 16;
const MAX_PAINT_NAME_BYTES = 256;
const CHROMIUM_METRIC_NAMES = new Set([
  'TaskDuration',
  'ScriptDuration',
  'LayoutDuration',
  'LayoutCount',
  'RecalcStyleCount',
  'JSHeapUsedSize',
  'JSHeapTotalSize',
]);
const TRACE_CATEGORIES = [
  'devtools.timeline',
  'blink.user_timing',
  'blink.console',
  'disabled-by-default-devtools.timeline',
].join(',');

export const MAX_WEB_PREVIEW_TRACE_EVENTS = 10_000;
export const MAX_WEB_PREVIEW_TRACE_NAME_BYTES = 2048;
export const WEB_PREVIEW_TRACE_WATCHDOG_MS = 15_000;
export const WEB_PREVIEW_TRACE_RESULT_TTL_MS = 60_000;

export interface WebPreviewPerformanceIdentity {
  applicationUrl: string;
  debuggingPort: number;
  inspectedUrl: string;
  sessionId: string;
  targetNonce: string;
}

export interface WebPreviewRecordedTrace {
  endMicros: number;
  startMicros: number;
  threadId: number;
  trace: string;
  type?: number;
}

export interface WebPreviewTraceCapture {
  browserMetrics: Record<string, number>;
  droppedTraceEventCount: number;
  elapsedMs: number;
  rendererTracingEnabled: boolean;
  startedAtEpochMs: number;
  timedOut: boolean;
  traces: WebPreviewRecordedTrace[];
}

export interface WebPreviewPerformanceSnapshot {
  mainThread: {
    layoutDurationMs?: number;
    scriptDurationMs?: number;
    taskDurationMs?: number;
  };
  memory: {
    totalBytes?: number;
    usedBytes?: number;
  } | null;
  navigation: {
    domContentLoadedMs?: number;
    loadMs?: number;
  };
  paints: Array<{ name: string; startTime: number }>;
  rendererTracingEnabled: boolean;
  resourceCount: number;
  transferSize: number;
  uptimeMs: number;
}

export interface WebPreviewTraceStatus {
  completedRecordingAvailable: boolean;
  completionError?: string;
  elapsedMs?: number;
  recording: boolean;
  rendererTracingEnabled: boolean;
  startedAtEpochMs?: number;
  tracingSupported: true;
}

interface ActiveTracePair {
  name: string;
  startMicros: number;
  threadId: number;
}

interface ChromiumMetricsResult {
  metrics?: Array<{ name: string; value: number }>;
}

interface TraceCompletion {
  capture?: WebPreviewTraceCapture;
  delivered: boolean;
  error?: Error;
  expiresAtMs: number;
  identity: WebPreviewPerformanceIdentity;
}

interface ActiveWebPreviewTrace {
  cleanupError?: Error;
  identity: WebPreviewPerformanceIdentity;
  recorder: WebPreviewTraceRecorderLike;
  watchdog: NodeJS.Timeout | undefined;
}

interface TransitionGate {
  promise: Promise<void>;
  resolve(): void;
}

export interface WebPreviewTraceRecorderLike {
  close(): void;
  status(nowMs: number): WebPreviewTraceStatus;
  stop(timedOut: boolean): Promise<WebPreviewTraceCapture>;
  stopForRecovery(): Promise<void>;
}

export class WebPreviewTraceLifecycleError extends Error {
  constructor(
    message: string,
    readonly traceEnded: boolean,
    readonly recorder: WebPreviewTraceRecorderLike | null,
  ) {
    super(message);
    this.name = 'WebPreviewTraceLifecycleError';
  }
}

export interface WebPreviewPerformanceControllerDependencies {
  clearTimer(timer: NodeJS.Timeout): void;
  enableTracing(identity: WebPreviewPerformanceIdentity): Promise<string>;
  now(): number;
  readSnapshot(identity: WebPreviewPerformanceIdentity): Promise<WebPreviewPerformanceSnapshot>;
  setTimer(callback: () => void, durationMs: number): NodeJS.Timeout;
  startRecorder(identity: WebPreviewPerformanceIdentity): Promise<WebPreviewTraceRecorderLike>;
  targetPresent(identity: WebPreviewPerformanceIdentity): Promise<boolean>;
  wait(durationMs: number): Promise<void>;
}

const BROWSER_TRACE_NAMES: ReadonlyMap<string, string> = new Map([
  ['RunTask', 'Browser.MainThread.Task'],
  ['FunctionCall', 'Browser.JavaScript.FunctionCall'],
  ['EvaluateScript', 'Browser.JavaScript.EvaluateScript'],
  ['EventDispatch', 'Browser.JavaScript.EventDispatch'],
  ['TimerFire', 'Browser.JavaScript.TimerFire'],
  ['FireAnimationFrame', 'Browser.JavaScript.AnimationFrame'],
  ['UpdateLayoutTree', 'Browser.Layout.UpdateLayoutTree'],
  ['Layout', 'Browser.Layout.Layout'],
  ['RecalculateStyles', 'Browser.Layout.RecalculateStyles'],
  ['ScheduleStyleRecalculation', 'Browser.Layout.ScheduleStyleRecalculation'],
  ['Paint', 'Browser.Paint.Paint'],
  ['PrePaint', 'Browser.Paint.PrePaint'],
  ['Layerize', 'Browser.Paint.Layerize'],
  ['RasterTask', 'Browser.Paint.RasterTask'],
  ['CompositeLayers', 'Browser.Paint.CompositeLayers'],
  ['BeginFrame', 'Browser.Frames.BeginFrame'],
  ['DrawFrame', 'Browser.Frames.DrawFrame'],
  ['AnimationFrame', 'Browser.Frames.AnimationFrame'],
  ['AnimationFrame::Render', 'Browser.Frames.Render'],
  ['Commit', 'Browser.Frames.Commit'],
  ['MinorGC', 'Browser.GC.Minor'],
  ['MajorGC', 'Browser.GC.Major'],
]);

const PERFORMANCE_SNAPSHOT_EXPRESSION = `(() => {
  const resources = globalThis.performance.getEntriesByType('resource');
  const navigation = globalThis.performance.getEntriesByType('navigation')[0];
  const paints = globalThis.performance.getEntriesByType('paint').slice(0, ${MAX_PAINT_ENTRIES});
  const parameters = new URLSearchParams(globalThis.location.search);
  return {
    navigation: navigation ? {
      domContentLoadedMs: navigation.domContentLoadedEventEnd,
      loadMs: navigation.loadEventEnd,
    } : {},
    paints: paints.map(entry => ({ name: String(entry.name), startTime: entry.startTime })),
    rendererTracingEnabled:
      parameters.getAll('valdiDevTools').length === 1 && parameters.get('valdiDevTools') === '1' &&
      parameters.getAll('valdiTrace').length === 1 && parameters.get('valdiTrace') === 'chrome',
    resourceCount: resources.length,
    transferSize: resources.reduce((total, entry) => total + (Number(entry.transferSize) || 0), 0),
    uptimeMs: globalThis.performance.now(),
  };
})()`;

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
    ? (value as Record<string, unknown>)
    : undefined;
}

function boundedSafeInteger(value: unknown): number | undefined {
  if (typeof value !== 'number' || !Number.isFinite(value) || value < 0) return undefined;
  const rounded = Math.round(value);
  return Number.isSafeInteger(rounded) ? rounded : undefined;
}

function finiteNonNegativeNumber(value: unknown): number | undefined {
  return typeof value === 'number' && Number.isFinite(value) && value >= 0 ? value : undefined;
}

function traceName(value: unknown): string | undefined {
  return typeof value === 'string' &&
    value.length > 0 &&
    Buffer.byteLength(value, 'utf8') <= MAX_WEB_PREVIEW_TRACE_NAME_BYTES
    ? value
    : undefined;
}

function normalizedTraceName(value: unknown): string | undefined {
  const name = traceName(value);
  if (name === undefined) return undefined;
  if (name.startsWith('Valdi.')) return name;
  return BROWSER_TRACE_NAMES.get(name);
}

function tracePairId(value: unknown): string | undefined {
  if (value === undefined) return '';
  if (typeof value !== 'string' && typeof value !== 'number') return undefined;
  const serialized = String(value);
  return Buffer.byteLength(serialized, 'utf8') <= MAX_TRACE_PAIR_ID_BYTES ? serialized : undefined;
}

export function saturatingWebPreviewTraceEventCount(left: number, right: number): number {
  return left >= Number.MAX_SAFE_INTEGER - right ? Number.MAX_SAFE_INTEGER : left + right;
}

export class WebPreviewTraceNormalizer {
  private readonly activePairs = new Map<string, ActiveTracePair[]>();
  private activePairCount = 0;
  private droppedTraceEventCount = 0;
  private finalized = false;
  private readonly traces: WebPreviewRecordedTrace[] = [];

  accept(value: unknown): void {
    if (this.finalized) return;
    const event = asRecord(value);
    if (event === undefined) return;

    const timestampTrace = this.normalizeTimestamp(event);
    if (timestampTrace !== undefined) {
      this.append(timestampTrace);
      return;
    }

    const instantOrDuration = this.normalizeInstantOrDuration(event);
    if (instantOrDuration !== undefined) {
      this.append(instantOrDuration);
      return;
    }

    const name = normalizedTraceName(event['name']);
    const timestamp = boundedSafeInteger(event['ts']);
    const pairId = tracePairId(event['id']);
    if (name === undefined || timestamp === undefined || pairId === undefined) return;

    const threadId = boundedSafeInteger(event['tid']) ?? 0;
    const key = JSON.stringify([threadId, name, pairId]);
    if (event['ph'] === 'B' || event['ph'] === 'b') {
      if (this.activePairCount >= MAX_ACTIVE_TRACE_PAIRS) {
        this.incrementDropped();
        return;
      }
      const active = this.activePairs.get(key) ?? [];
      active.push({ name, startMicros: timestamp, threadId });
      this.activePairs.set(key, active);
      this.activePairCount++;
      return;
    }
    if (event['ph'] !== 'E' && event['ph'] !== 'e') return;

    const active = this.activePairs.get(key);
    const start = active?.pop();
    if (start === undefined) return;
    this.activePairCount--;
    if (active?.length === 0) this.activePairs.delete(key);
    this.append({
      endMicros: Math.max(start.startMicros, timestamp),
      startMicros: start.startMicros,
      threadId: start.threadId,
      trace: start.name,
    });
  }

  dataLossOccurred(): void {
    if (this.finalized) return;
    this.incrementDropped();
  }

  result(): { droppedTraceEventCount: number; traces: WebPreviewRecordedTrace[] } {
    if (!this.finalized) {
      this.droppedTraceEventCount = saturatingWebPreviewTraceEventCount(
        this.droppedTraceEventCount,
        this.activePairCount,
      );
      this.activePairs.clear();
      this.activePairCount = 0;
      this.finalized = true;
    }
    return {
      droppedTraceEventCount: this.droppedTraceEventCount,
      traces: [...this.traces].sort(
        (left, right) => left.startMicros - right.startMicros || right.endMicros - left.endMicros,
      ),
    };
  }

  private append(trace: WebPreviewRecordedTrace): void {
    if (this.traces.length >= MAX_WEB_PREVIEW_TRACE_EVENTS) {
      this.incrementDropped();
      return;
    }
    this.traces.push(trace);
  }

  private incrementDropped(): void {
    this.droppedTraceEventCount = saturatingWebPreviewTraceEventCount(this.droppedTraceEventCount, 1);
  }

  private normalizeTimestamp(event: Record<string, unknown>): WebPreviewRecordedTrace | undefined {
    if (event['name'] !== 'TimeStamp') return undefined;
    const data = asRecord(asRecord(event['args'])?.['data']);
    const name = traceName(data?.['name']) ?? traceName(data?.['message']);
    if (name === undefined || !name.startsWith('Valdi.')) return undefined;
    const timestamp = boundedSafeInteger(event['ts']);
    const startMicros = boundedSafeInteger(data?.['start']) ?? timestamp;
    const endMicros = boundedSafeInteger(data?.['end']) ?? startMicros;
    if (startMicros === undefined || endMicros === undefined) return undefined;
    return {
      endMicros: Math.max(startMicros, endMicros),
      startMicros,
      threadId: boundedSafeInteger(event['tid']) ?? 0,
      trace: name,
      ...(startMicros === endMicros ? { type: RECORDED_TRACE_TYPE_INSTANT } : {}),
    };
  }

  private normalizeInstantOrDuration(event: Record<string, unknown>): WebPreviewRecordedTrace | undefined {
    const name = normalizedTraceName(event['name']);
    const phase = String(event['ph']);
    const startMicros = boundedSafeInteger(event['ts']);
    if (name === undefined || startMicros === undefined || !['X', 'I', 'i', 'R'].includes(phase)) {
      return undefined;
    }
    const durationMicros = boundedSafeInteger(event['dur']) ?? 0;
    if (event['name'] === 'RunTask' && durationMicros < MIN_BROWSER_TASK_DURATION_MICROS) return undefined;
    if (durationMicros > Number.MAX_SAFE_INTEGER - startMicros) return undefined;
    return {
      endMicros: startMicros + durationMicros,
      startMicros,
      threadId: boundedSafeInteger(event['tid']) ?? 0,
      trace: name,
      ...(phase === 'X' ? {} : { type: RECORDED_TRACE_TYPE_INSTANT }),
    };
  }
}

export function normalizeWebPreviewPerformanceMetrics(result: unknown): Record<string, number> {
  const metrics = (asRecord(result) as ChromiumMetricsResult | undefined)?.metrics;
  const normalized = Object.create(null) as Record<string, number>;
  if (!Array.isArray(metrics)) return normalized;
  for (const metric of metrics) {
    if (
      typeof metric?.name === 'string' &&
      CHROMIUM_METRIC_NAMES.has(metric.name) &&
      typeof metric.value === 'number' &&
      Number.isFinite(metric.value)
    ) {
      normalized[metric.name] = metric.value;
    }
  }
  return normalized;
}

function metricDifferences(start: Record<string, number>, end: Record<string, number>): Record<string, number> {
  const result = Object.create(null) as Record<string, number>;
  for (const name of ['TaskDuration', 'ScriptDuration', 'LayoutDuration']) {
    if (end[name] !== undefined) result[`${name}Ms`] = Math.max(0, end[name] - (start[name] ?? 0)) * 1000;
  }
  for (const name of ['LayoutCount', 'RecalcStyleCount']) {
    if (end[name] !== undefined) result[name] = Math.max(0, end[name] - (start[name] ?? 0));
  }
  return result;
}

function rendererTracingEnabled(inspectedUrl: string): boolean {
  try {
    const parameters = new URL(inspectedUrl).searchParams;
    return (
      parameters.getAll('valdiDevTools').length === 1 &&
      parameters.get('valdiDevTools') === '1' &&
      parameters.getAll('valdiTrace').length === 1 &&
      parameters.get('valdiTrace') === 'chrome'
    );
  } catch {
    return false;
  }
}

function identitiesMatch(left: WebPreviewPerformanceIdentity, right: WebPreviewPerformanceIdentity): boolean {
  return (
    left.sessionId === right.sessionId &&
    left.inspectedUrl === right.inspectedUrl &&
    left.targetNonce === right.targetNonce
  );
}

export async function isWebPreviewPerformanceTargetPresent(identity: WebPreviewPerformanceIdentity): Promise<boolean> {
  const targets = await listOwlChromiumTargets(identity.debuggingPort);
  const candidates = targets.filter(
    target => target.type === 'page' && matchesOwlApplicationUrl(target.url, identity.applicationUrl),
  );
  let probeError: Error | undefined;
  for (const candidate of candidates) {
    let connection: OwlChromiumConnection | undefined;
    try {
      connection = await OwlChromiumConnection.connect(candidate.webSocketDebuggerUrl);
      if (!(await connection.matchesTarget(identity.applicationUrl, identity.targetNonce))) continue;
      const currentUrl = await connection.evaluate('String(globalThis.location.href)');
      if (typeof currentUrl !== 'string') {
        throw new TypeError('The previous inspected web preview returned an invalid current URL.');
      }
      if (new URL(currentUrl).toString() === new URL(identity.inspectedUrl).toString()) return true;
    } catch (error) {
      probeError = error instanceof Error ? error : new Error(String(error));
    } finally {
      connection?.close();
    }
  }
  if (probeError) {
    throw new Error(
      `Could not verify whether the previous inspected web preview is still present: ${probeError.message}`,
    );
  }
  return false;
}

function createTransitionGate(): TransitionGate {
  let resolveGate: (() => void) | undefined;
  const promise = new Promise<void>(resolve => {
    resolveGate = resolve;
  });
  if (!resolveGate) throw new Error('Could not initialize the web preview performance transition gate.');
  return { promise, resolve: resolveGate };
}

async function assertCurrentTarget(
  connection: OwlChromiumConnection,
  identity: WebPreviewPerformanceIdentity,
): Promise<void> {
  if (!(await connection.matchesTarget(identity.applicationUrl, identity.targetNonce))) {
    throw new Error('The inspected web preview changed while the performance request was running.');
  }
}

async function assertInspectedUrl(
  connection: OwlChromiumConnection,
  identity: WebPreviewPerformanceIdentity,
): Promise<string> {
  const currentUrl = await connection.evaluate('String(globalThis.location.href)');
  if (typeof currentUrl !== 'string' || new URL(currentUrl).toString() !== new URL(identity.inspectedUrl).toString()) {
    throw new Error('The inspected web preview URL changed while the performance request was running.');
  }
  return currentUrl;
}

export class WebPreviewTraceRecorder implements WebPreviewTraceRecorderLike {
  private completionReject: ((error: Error) => void) | undefined;
  private completionResolve: (() => void) | undefined;
  private completionTimer: NodeJS.Timeout | undefined;
  private readonly normalizer = new WebPreviewTraceNormalizer();
  private stopped = false;
  private readonly unsubscribeClose: () => void;
  private readonly unsubscribeEvents: () => void;

  private constructor(
    private readonly connection: OwlChromiumConnection,
    private readonly identity: WebPreviewPerformanceIdentity,
    private readonly startMetrics: Record<string, number>,
    private readonly rendererTracing: boolean,
    private readonly startedAtEpochMs: number,
  ) {
    this.unsubscribeEvents = connection.onEvent(event => {
      if (event.method === 'Tracing.dataCollected') {
        const values = event.params['value'];
        if (!Array.isArray(values)) return;
        for (const value of values) this.normalizer.accept(value);
        return;
      }
      if (event.method === 'Tracing.tracingComplete') {
        if (event.params['dataLossOccurred'] === true) this.normalizer.dataLossOccurred();
        this.completionResolve?.();
      }
    });
    this.unsubscribeClose = connection.onClose(error => this.completionReject?.(error));
  }

  static async start(identity: WebPreviewPerformanceIdentity): Promise<WebPreviewTraceRecorder> {
    const connection = await connectToOwlApplication(
      identity.debuggingPort,
      identity.applicationUrl,
      identity.targetNonce,
    );
    try {
      await assertCurrentTarget(connection, identity);
      const currentUrl = await assertInspectedUrl(connection, identity);
      await connection.call('Performance.enable', {}, CHROMIUM_COMMAND_TIMEOUT_MS);
      const startMetrics = normalizeWebPreviewPerformanceMetrics(
        await connection.call('Performance.getMetrics', {}, CHROMIUM_COMMAND_TIMEOUT_MS),
      );
      const recorder = new WebPreviewTraceRecorder(
        connection,
        identity,
        startMetrics,
        rendererTracingEnabled(currentUrl),
        Date.now(),
      );
      try {
        await connection.call(
          'Tracing.start',
          {
            categories: TRACE_CATEGORIES,
            options: 'record-as-much-as-possible',
            transferMode: 'ReportEvents',
          },
          CHROMIUM_COMMAND_TIMEOUT_MS,
        );
      } catch (error) {
        const startError = error instanceof Error ? error : new Error(String(error));
        if (startError instanceof ChromiumDevToolsProtocolError) {
          recorder.close();
          throw startError;
        }
        let cleanupError: Error | undefined;
        try {
          await recorder.stopForRecovery();
        } catch (error_) {
          cleanupError = error_ instanceof Error ? error_ : new Error(String(error_));
        }
        if (cleanupError) {
          throw new WebPreviewTraceLifecycleError(
            `${startError.message} Best-effort Chromium trace cleanup also failed: ${cleanupError.message}`,
            false,
            recorder,
          );
        }
        throw new WebPreviewTraceLifecycleError(startError.message, true, null);
      }
      return recorder;
    } catch (error) {
      connection.close();
      throw error;
    }
  }

  status(nowMs: number): WebPreviewTraceStatus {
    return {
      completedRecordingAvailable: false,
      elapsedMs: Math.max(0, nowMs - this.startedAtEpochMs),
      recording: true,
      rendererTracingEnabled: this.rendererTracing,
      startedAtEpochMs: this.startedAtEpochMs,
      tracingSupported: true,
    };
  }

  async stop(timedOut: boolean): Promise<WebPreviewTraceCapture> {
    if (this.stopped) throw new Error('The web preview performance trace has already stopped.');
    this.stopped = true;
    let traceEnded = false;
    try {
      await this.endTracing();
      traceEnded = true;
      await assertCurrentTarget(this.connection, this.identity);
      await assertInspectedUrl(this.connection, this.identity);
      const endMetrics = normalizeWebPreviewPerformanceMetrics(
        await this.connection.call('Performance.getMetrics', {}, CHROMIUM_COMMAND_TIMEOUT_MS),
      );
      const normalized = this.normalizer.result();
      return {
        browserMetrics: metricDifferences(this.startMetrics, endMetrics),
        droppedTraceEventCount: normalized.droppedTraceEventCount,
        elapsedMs: Math.max(0, Date.now() - this.startedAtEpochMs),
        rendererTracingEnabled: this.rendererTracing,
        startedAtEpochMs: this.startedAtEpochMs,
        timedOut,
        traces: normalized.traces,
      };
    } catch (error) {
      const normalized = error instanceof Error ? error : new Error(String(error));
      throw new WebPreviewTraceLifecycleError(normalized.message, traceEnded, null);
    } finally {
      this.close();
    }
  }

  async stopForRecovery(): Promise<void> {
    if (this.stopped) throw new Error('The web preview performance trace has already stopped.');
    this.stopped = true;
    try {
      await this.endTracing();
    } catch (error) {
      const normalized = error instanceof Error ? error : new Error(String(error));
      throw new WebPreviewTraceLifecycleError(normalized.message, false, null);
    } finally {
      this.close();
    }
  }

  close(): void {
    if (this.completionTimer !== undefined) {
      clearTimeout(this.completionTimer);
      this.completionTimer = undefined;
    }
    this.completionResolve = undefined;
    this.completionReject = undefined;
    this.unsubscribeEvents();
    this.unsubscribeClose();
    this.connection.close();
  }

  private async endTracing(): Promise<void> {
    const completed = new Promise<void>((resolve, reject) => {
      this.completionResolve = resolve;
      this.completionReject = reject;
      this.completionTimer = setTimeout(() => {
        reject(new Error('Timed out waiting for the Chromium performance trace to finish.'));
      }, TRACE_COMPLETION_TIMEOUT_MS);
    });
    const completionError = completed.then(
      () => null,
      error => (error instanceof Error ? error : new Error(String(error))),
    );

    await this.connection.call('Tracing.end', {}, CHROMIUM_COMMAND_TIMEOUT_MS);
    const error = await completionError;
    if (error) throw error;
  }
}

function normalizedSnapshotData(value: unknown): Omit<WebPreviewPerformanceSnapshot, 'mainThread' | 'memory'> {
  const record = asRecord(value) ?? {};
  const navigation = asRecord(record['navigation']) ?? {};
  const rawPaints = Array.isArray(record['paints']) ? record['paints'] : [];
  const paints: Array<{ name: string; startTime: number }> = [];
  for (const item of rawPaints.slice(0, MAX_PAINT_ENTRIES)) {
    const paint = asRecord(item);
    const name = typeof paint?.['name'] === 'string' ? paint['name'] : undefined;
    const startTime = finiteNonNegativeNumber(paint?.['startTime']);
    if (name !== undefined && Buffer.byteLength(name, 'utf8') <= MAX_PAINT_NAME_BYTES && startTime !== undefined) {
      paints.push({ name, startTime });
    }
  }
  const domContentLoadedMs = finiteNonNegativeNumber(navigation['domContentLoadedMs']);
  const loadMs = finiteNonNegativeNumber(navigation['loadMs']);
  return {
    navigation: {
      ...(domContentLoadedMs === undefined ? {} : { domContentLoadedMs }),
      ...(loadMs === undefined ? {} : { loadMs }),
    },
    paints,
    rendererTracingEnabled: record['rendererTracingEnabled'] === true,
    resourceCount: boundedSafeInteger(record['resourceCount']) ?? 0,
    transferSize: finiteNonNegativeNumber(record['transferSize']) ?? 0,
    uptimeMs: finiteNonNegativeNumber(record['uptimeMs']) ?? 0,
  };
}

export async function readWebPreviewPerformanceSnapshot(
  identity: WebPreviewPerformanceIdentity,
): Promise<WebPreviewPerformanceSnapshot> {
  const connection = await connectToOwlApplication(
    identity.debuggingPort,
    identity.applicationUrl,
    identity.targetNonce,
  );
  try {
    await assertCurrentTarget(connection, identity);
    await assertInspectedUrl(connection, identity);
    await connection.call('Performance.enable', {}, CHROMIUM_COMMAND_TIMEOUT_MS);
    const [snapshotValue, metricsValue] = await Promise.all([
      connection.evaluate(PERFORMANCE_SNAPSHOT_EXPRESSION),
      connection.call('Performance.getMetrics', {}, CHROMIUM_COMMAND_TIMEOUT_MS),
    ]);
    await assertCurrentTarget(connection, identity);
    await assertInspectedUrl(connection, identity);
    const snapshot = normalizedSnapshotData(snapshotValue);
    const metrics = normalizeWebPreviewPerformanceMetrics(metricsValue);
    const heapUsedBytes = finiteNonNegativeNumber(metrics['JSHeapUsedSize']);
    const heapTotalBytes = finiteNonNegativeNumber(metrics['JSHeapTotalSize']);
    return {
      ...snapshot,
      mainThread: {
        ...(finiteNonNegativeNumber(metrics['TaskDuration']) === undefined
          ? {}
          : { taskDurationMs: (metrics['TaskDuration'] ?? 0) * 1000 }),
        ...(finiteNonNegativeNumber(metrics['ScriptDuration']) === undefined
          ? {}
          : { scriptDurationMs: (metrics['ScriptDuration'] ?? 0) * 1000 }),
        ...(finiteNonNegativeNumber(metrics['LayoutDuration']) === undefined
          ? {}
          : { layoutDurationMs: (metrics['LayoutDuration'] ?? 0) * 1000 }),
      },
      memory:
        heapUsedBytes === undefined && heapTotalBytes === undefined
          ? null
          : {
              ...(heapUsedBytes === undefined ? {} : { usedBytes: heapUsedBytes }),
              ...(heapTotalBytes === undefined ? {} : { totalBytes: heapTotalBytes }),
            },
    };
  } finally {
    connection.close();
  }
}

export async function enableWebPreviewRendererTracing(identity: WebPreviewPerformanceIdentity): Promise<string> {
  const connection = await connectToOwlApplication(
    identity.debuggingPort,
    identity.applicationUrl,
    identity.targetNonce,
  );
  try {
    await assertCurrentTarget(connection, identity);
    const currentUrl = await assertInspectedUrl(connection, identity);
    const nextUrl = new URL(currentUrl);
    nextUrl.searchParams.set('valdiDevTools', '1');
    nextUrl.searchParams.set('valdiTrace', 'chrome');
    await assertCurrentTarget(connection, identity);
    await connection.call('Page.navigate', { url: nextUrl.toString() }, CHROMIUM_COMMAND_TIMEOUT_MS);
    return nextUrl.toString();
  } finally {
    connection.close();
  }
}

function defaultDependencies(): WebPreviewPerformanceControllerDependencies {
  return {
    clearTimer: timer => clearTimeout(timer),
    enableTracing: enableWebPreviewRendererTracing,
    now: () => Date.now(),
    readSnapshot: readWebPreviewPerformanceSnapshot,
    setTimer: (callback, durationMs) => setTimeout(callback, durationMs),
    startRecorder: identity => WebPreviewTraceRecorder.start(identity),
    targetPresent: isWebPreviewPerformanceTargetPresent,
    wait: async durationMs =>
      await new Promise<void>(resolve => {
        setTimeout(resolve, durationMs);
      }),
  };
}

export class WebPreviewPerformanceController {
  private activeTrace: ActiveWebPreviewTrace | undefined;
  private completion: TraceCompletion | undefined;
  private completionExpiryTimer: NodeJS.Timeout | undefined;
  private transitionTail: Promise<void> = Promise.resolve();

  constructor(private readonly dependencies: WebPreviewPerformanceControllerDependencies) {}

  async snapshot(identity: WebPreviewPerformanceIdentity): Promise<WebPreviewPerformanceSnapshot> {
    return await this.dependencies.readSnapshot(identity);
  }

  async enableTracing(identity: WebPreviewPerformanceIdentity): Promise<string> {
    return await this.runExclusive(async () => {
      this.expireCompletion();
      await this.recoverMissingOwner(identity);
      if (this.activeTrace || (this.completion && !this.completion.delivered)) {
        throw new Error('Stop the current web preview performance trace before enabling renderer events.');
      }
      return this.dependencies.enableTracing(identity);
    });
  }

  async status(identity: WebPreviewPerformanceIdentity): Promise<WebPreviewTraceStatus> {
    return await this.runExclusive(async () => {
      this.expireCompletion();
      await this.recoverMissingOwner(identity);
      if (this.activeTrace) {
        this.assertIdentity(this.activeTrace.identity, identity);
        if (this.activeTrace.cleanupError) throw this.activeTrace.cleanupError;
        return this.activeTrace.recorder.status(this.dependencies.now());
      }
      if (this.completion && !this.completion.delivered) {
        this.assertIdentity(this.completion.identity, identity);
        return {
          completedRecordingAvailable: this.completion.capture !== undefined,
          ...(this.completion.error ? { completionError: this.completion.error.message } : {}),
          recording: false,
          rendererTracingEnabled: rendererTracingEnabled(identity.inspectedUrl),
          tracingSupported: true,
        };
      }
      return {
        completedRecordingAvailable: false,
        recording: false,
        rendererTracingEnabled: rendererTracingEnabled(identity.inspectedUrl),
        tracingSupported: true,
      };
    });
  }

  async start(identity: WebPreviewPerformanceIdentity): Promise<WebPreviewTraceStatus> {
    return await this.runExclusive(async () => await this.startTrace(identity, true));
  }

  async stop(identity: WebPreviewPerformanceIdentity): Promise<WebPreviewTraceCapture> {
    return await this.runExclusive(async () => {
      this.expireCompletion();
      await this.recoverMissingOwner(identity);
      if (this.activeTrace) {
        this.assertIdentity(this.activeTrace.identity, identity);
        if (this.activeTrace.cleanupError) throw this.activeTrace.cleanupError;
        return await this.finishActiveTrace(false, true);
      }
      if (this.completion) {
        this.assertIdentity(this.completion.identity, identity);
        if (this.completion.error) {
          const error = this.completion.error;
          this.clearCompletion();
          throw error;
        }
        if (!this.completion.capture) throw new Error('The completed Chromium trace is unavailable.');
        this.completion.delivered = true;
        return this.completion.capture;
      }
      throw new Error('No Chromium performance trace is available for the inspected web preview.');
    });
  }

  async capture(identity: WebPreviewPerformanceIdentity, durationMs: number): Promise<WebPreviewTraceCapture> {
    await this.runExclusive(async () => await this.startTrace(identity, false));
    try {
      await this.dependencies.wait(durationMs);
    } catch (error) {
      try {
        await this.stop(identity);
      } catch (cleanupError) {
        throw new Error(
          `${error instanceof Error ? error.message : String(error)} Best-effort Chromium trace cleanup also failed: ${
            cleanupError instanceof Error ? cleanupError.message : String(cleanupError)
          }`,
        );
      }
      throw error;
    }
    return await this.stop(identity);
  }

  async close(): Promise<void> {
    await this.runExclusive(async () => {
      let closeError: Error | undefined;
      if (this.activeTrace) {
        const active = this.activeTrace;
        this.activeTrace = undefined;
        this.clearWatchdog(active.watchdog);
        try {
          await active.recorder.stop(false);
        } catch (error) {
          closeError = error instanceof Error ? error : new Error(String(error));
        }
      }
      this.clearCompletion();
      if (closeError) throw closeError;
    });
  }

  private assertIdentity(expected: WebPreviewPerformanceIdentity, actual: WebPreviewPerformanceIdentity): void {
    if (!identitiesMatch(expected, actual)) {
      throw new Error('Another inspected web preview owns the current Chromium performance trace.');
    }
  }

  private async finishActiveTrace(timedOut: boolean, delivered: boolean): Promise<WebPreviewTraceCapture> {
    const active = this.activeTrace;
    if (!active) throw new Error('No Chromium performance trace is recording.');
    this.clearWatchdog(active.watchdog);
    active.watchdog = undefined;
    try {
      const capture = await active.recorder.stop(timedOut);
      if (this.activeTrace === active) this.activeTrace = undefined;
      this.retainCompletion({ capture, delivered, identity: active.identity });
      return capture;
    } catch (error) {
      const normalized = error instanceof Error ? error : new Error(String(error));
      if (error instanceof WebPreviewTraceLifecycleError && error.traceEnded) {
        if (this.activeTrace === active) this.activeTrace = undefined;
        this.retainCompletion({ delivered, error: normalized, identity: active.identity });
      } else {
        active.cleanupError = normalized;
      }
      throw normalized;
    }
  }

  private async startTrace(
    identity: WebPreviewPerformanceIdentity,
    armWatchdog: boolean,
  ): Promise<WebPreviewTraceStatus> {
    this.expireCompletion();
    await this.recoverMissingOwner(identity);
    if (this.activeTrace) {
      if (this.activeTrace.cleanupError) throw this.activeTrace.cleanupError;
      throw new Error('A Chromium performance trace is already recording.');
    }
    if (this.completion && !this.completion.delivered) {
      throw new Error('Retrieve the completed Chromium performance trace before starting another recording.');
    }
    this.clearCompletion();
    let recorder: WebPreviewTraceRecorderLike;
    try {
      recorder = await this.dependencies.startRecorder(identity);
    } catch (error) {
      if (error instanceof WebPreviewTraceLifecycleError && !error.traceEnded && error.recorder) {
        this.activeTrace = {
          cleanupError: error,
          identity,
          recorder: error.recorder,
          watchdog: undefined,
        };
      }
      throw error;
    }
    const watchdog = armWatchdog
      ? this.dependencies.setTimer(() => {
          void this.runExclusive(async () => {
            if (!this.activeTrace || this.activeTrace.recorder !== recorder) return;
            await this.finishActiveTrace(true, false);
          }).catch(error => {
            console.warn('[Valdi DevTools] Could not finalize the web preview performance trace.', error);
          });
        }, WEB_PREVIEW_TRACE_WATCHDOG_MS)
      : undefined;
    this.activeTrace = { identity, recorder, watchdog };
    return recorder.status(this.dependencies.now());
  }

  private clearWatchdog(watchdog: NodeJS.Timeout | undefined): void {
    if (watchdog !== undefined) this.dependencies.clearTimer(watchdog);
  }

  private async recoverMissingOwner(identity: WebPreviewPerformanceIdentity): Promise<void> {
    const owner =
      this.activeTrace?.identity ??
      (this.completion && !this.completion.delivered ? this.completion.identity : undefined);
    if (!owner || identitiesMatch(owner, identity)) return;
    if (await this.dependencies.targetPresent(owner)) {
      throw new Error('Another inspected web preview owns the current Chromium performance trace.');
    }

    if (this.activeTrace) {
      const active = this.activeTrace;
      if (active.cleanupError) throw active.cleanupError;
      this.clearWatchdog(active.watchdog);
      try {
        await active.recorder.stopForRecovery();
      } catch (error) {
        const normalized = error instanceof Error ? error : new Error(String(error));
        active.cleanupError = new Error(
          `Could not end the previous web preview performance trace after its inspected target disappeared: ${normalized.message}`,
        );
        throw active.cleanupError;
      }
      if (this.activeTrace === active) this.activeTrace = undefined;
    }
    this.clearCompletion();
  }

  private retainCompletion(value: Omit<TraceCompletion, 'expiresAtMs'>): void {
    this.clearCompletion();
    const expiresAtMs = this.dependencies.now() + WEB_PREVIEW_TRACE_RESULT_TTL_MS;
    this.completion = { ...value, expiresAtMs };
    this.completionExpiryTimer = this.dependencies.setTimer(() => {
      void this.runExclusive(() => this.expireCompletion()).catch(error => {
        console.warn('[Valdi DevTools] Could not expire the web preview performance result.', error);
      });
    }, WEB_PREVIEW_TRACE_RESULT_TTL_MS);
  }

  private expireCompletion(): void {
    if (this.completion && this.completion.expiresAtMs <= this.dependencies.now()) this.clearCompletion();
  }

  private clearCompletion(): void {
    if (this.completionExpiryTimer !== undefined) {
      this.dependencies.clearTimer(this.completionExpiryTimer);
      this.completionExpiryTimer = undefined;
    }
    this.completion = undefined;
  }

  private async runExclusive<Value>(operation: () => Promise<Value> | Value): Promise<Value> {
    const previous = this.transitionTail;
    const gate = createTransitionGate();
    this.transitionTail = gate.promise;
    await previous;
    try {
      return await operation();
    } finally {
      gate.resolve();
    }
  }
}

export function createWebPreviewPerformanceController(): WebPreviewPerformanceController {
  return new WebPreviewPerformanceController(defaultDependencies());
}
