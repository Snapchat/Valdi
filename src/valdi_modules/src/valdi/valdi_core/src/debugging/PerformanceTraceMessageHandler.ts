import { isTracingSupported, startTraceRecording, stopTraceRecordingWithStats } from '../utils/Trace';
import type { TraceRecordingResult } from '../utils/Trace';
import type {
  PerformanceTraceStartRequestBody,
  PerformanceTraceStatusBody,
  PerformanceTraceStopBody,
  PerformanceTraceStopRequestBody,
} from './Messages';
import {
  isPerformanceTraceContextIdWithinLimit,
  MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES,
  truncatePerformanceTraceError,
} from './Messages';

export const PERFORMANCE_TRACE_HANDLER_TIMEOUT_MS = 20_000;
export const PERFORMANCE_TRACE_RESULT_TTL_MS = 60_000;

export interface PerformanceTraceRenderer {
  setTracingEnabled(enabled: boolean): void;
  isTracingEnabled(): boolean;
}

export type PerformanceTraceRendererResolver = (contextId: string) => PerformanceTraceRenderer | undefined;

export interface PerformanceTraceRuntime {
  cancelTimeout(timeoutId: number): void;
  isTracingSupported(): boolean;
  nowMs(): number;
  nowEpochMs(): number;
  scheduleTimeout(callback: () => void, timeoutMs: number): number;
  startTraceRecording(): number;
  stopTraceRecording(recordingId: number): TraceRecordingResult;
}

const defaultPerformanceTraceRuntime: PerformanceTraceRuntime = {
  cancelTimeout: timeoutId => clearTimeout(timeoutId),
  isTracingSupported,
  nowMs: () => performance.now(),
  nowEpochMs: () => Date.now(),
  scheduleTimeout: (callback, timeoutMs) => setTimeout(callback, timeoutMs),
  startTraceRecording,
  stopTraceRecording: stopTraceRecordingWithStats,
};

function errorMessage(error: unknown): string {
  return truncatePerformanceTraceError(error instanceof Error ? `${error.name}: ${error.message}` : String(error));
}

function appendCompletionError(existing: string | undefined, next: string): string {
  return truncatePerformanceTraceError(existing ? `${existing} ${next}` : next);
}

export class PerformanceTraceMessageHandler {
  private recordingId: number | undefined;
  private recordingStartedAtMs: number | undefined;
  private recordingStartedAtEpochMs: number | undefined;
  private rendererTracingWasEnabled: boolean | undefined;
  private activeContextId: string | undefined;
  private activeRenderer: PerformanceTraceRenderer | undefined;
  private recordingTimeoutId: number | undefined;
  private completedRecording: PerformanceTraceStopBody | undefined;
  private completedRecordingExpiryTimeoutId: number | undefined;
  private recentlyCompletedRecording: PerformanceTraceStopBody | undefined;
  private recentlyCompletedRecordingExpiryTimeoutId: number | undefined;

  static create(getRendererForContextId: PerformanceTraceRendererResolver): PerformanceTraceMessageHandler {
    return new PerformanceTraceMessageHandler(getRendererForContextId, defaultPerformanceTraceRuntime);
  }

  constructor(
    private readonly getRendererForContextId: PerformanceTraceRendererResolver,
    private readonly traceRuntime: PerformanceTraceRuntime,
  ) {}

  startRecording(body: PerformanceTraceStartRequestBody): PerformanceTraceStatusBody {
    if (this.recordingId !== undefined) {
      throw new Error('A Valdi performance trace recording is already active.');
    }
    if (this.completedRecording !== undefined) {
      throw new Error(
        `A completed timed-out Valdi performance trace for context ${this.completedRecording.contextId} is waiting to be retrieved.`,
      );
    }
    const contextId = this.requireContextId(body.contextId);
    const rendererTracing = this.readRendererTracing(body.rendererTracing);
    const renderer = this.getRendererForContextId(contextId);
    if (!renderer) {
      throw new Error(`No Valdi renderer found for context ${contextId}.`);
    }

    const rendererTracingWasEnabled = renderer.isTracingEnabled();
    const startedAtMs = this.traceRuntime.nowMs();
    const startedAtEpochMs = this.traceRuntime.nowEpochMs();
    try {
      renderer.setTracingEnabled(rendererTracing);
      const recordingId = this.traceRuntime.startTraceRecording();
      this.clearRecentlyCompletedRecording();
      this.rendererTracingWasEnabled = rendererTracingWasEnabled;
      this.recordingId = recordingId;
      this.recordingStartedAtMs = startedAtMs;
      this.recordingStartedAtEpochMs = startedAtEpochMs;
      this.activeContextId = contextId;
      this.activeRenderer = renderer;
      this.recordingTimeoutId = this.traceRuntime.scheduleTimeout(
        () => this.completeTimedOutRecording(recordingId),
        PERFORMANCE_TRACE_HANDLER_TIMEOUT_MS,
      );
    } catch (error) {
      const recordingId = this.recordingId;
      if (recordingId !== undefined) {
        this.completeActiveRecording(recordingId, false);
      } else {
        this.clearActiveRecordingState();
        renderer.setTracingEnabled(rendererTracingWasEnabled);
      }
      throw error;
    }

    return this.getStatus();
  }

  stopRecording(body: PerformanceTraceStopRequestBody): PerformanceTraceStopBody {
    const requestedContextId = this.requireContextId(body.contextId);
    const recordingId = this.recordingId;
    if (recordingId !== undefined) {
      this.assertContextMatches(requestedContextId, this.activeContextId);
      const result = this.completeActiveRecording(recordingId, false);
      this.storeRecentlyCompletedRecording(result);
      return result;
    }

    const completedRecording = this.completedRecording;
    if (completedRecording !== undefined) {
      this.assertContextMatches(requestedContextId, completedRecording.contextId);
      this.clearCompletedRecording();
      const result = {
        ...completedRecording,
        completedRecordingAvailable: false,
        completedContextId: undefined,
      };
      this.storeRecentlyCompletedRecording(result);
      return result;
    }

    const recentlyCompletedRecording = this.recentlyCompletedRecording;
    if (recentlyCompletedRecording !== undefined) {
      this.assertContextMatches(requestedContextId, recentlyCompletedRecording.contextId);
      return recentlyCompletedRecording;
    }

    throw new Error('No Valdi performance trace recording is active or waiting to be retrieved.');
  }

  getStatus(): PerformanceTraceStatusBody {
    const startedAtMs = this.recordingStartedAtMs;
    const recording = this.recordingId !== undefined;
    const completedRecording = this.completedRecording;
    return {
      recording,
      contextId: recording ? this.activeContextId : undefined,
      completedRecordingAvailable: completedRecording !== undefined,
      completedContextId: completedRecording?.contextId,
      completionError: completedRecording?.completionError,
      rendererTracingEnabled:
        this.activeRenderer?.isTracingEnabled() ?? completedRecording?.rendererTracingEnabled ?? false,
      tracingSupported: this.traceRuntime.isTracingSupported(),
      startedAtEpochMs: recording ? this.recordingStartedAtEpochMs : completedRecording?.startedAtEpochMs,
      elapsedMs:
        recording && startedAtMs !== undefined
          ? this.traceRuntime.nowMs() - startedAtMs
          : completedRecording?.elapsedMs,
    };
  }

  abortRecording(): void {
    const recordingId = this.recordingId;
    const result = recordingId === undefined ? undefined : this.completeActiveRecording(recordingId, false);
    this.clearCompletedRecording();
    this.clearRecentlyCompletedRecording();
    if (result?.completionError) {
      throw new Error(result.completionError);
    }
  }

  abortRecordingForContext(contextId: string): void {
    if (
      this.activeContextId === contextId ||
      this.completedRecording?.contextId === contextId ||
      this.recentlyCompletedRecording?.contextId === contextId
    ) {
      this.abortRecording();
    }
  }

  private completeTimedOutRecording(recordingId: number): void {
    if (this.recordingId !== recordingId) {
      return;
    }
    this.storeCompletedRecording(this.completeActiveRecording(recordingId, true));
  }

  private completeActiveRecording(recordingId: number, timedOut: boolean): PerformanceTraceStopBody {
    const renderer = this.activeRenderer;
    const contextId = this.activeContextId;
    const rendererTracingWasEnabled = this.rendererTracingWasEnabled;
    const startedAtMs = this.recordingStartedAtMs;
    const startedAtEpochMs = this.recordingStartedAtEpochMs;
    const elapsedMs = startedAtMs === undefined ? undefined : this.traceRuntime.nowMs() - startedAtMs;
    let traceResult: TraceRecordingResult = { traces: [], droppedTraceEventCount: 0 };
    let completionError: string | undefined;
    let rendererTracingEnabled = false;

    try {
      traceResult = this.traceRuntime.stopTraceRecording(recordingId);
    } catch (error) {
      completionError = truncatePerformanceTraceError(
        `Failed to stop Valdi performance trace recording: ${errorMessage(error)}`,
      );
    } finally {
      this.clearActiveRecordingState();
      if (rendererTracingWasEnabled !== undefined) {
        try {
          renderer?.setTracingEnabled(rendererTracingWasEnabled);
        } catch (error) {
          const restoreError = `Failed to restore Renderer tracing: ${errorMessage(error)}`;
          completionError = appendCompletionError(completionError, restoreError);
        }
      }
      try {
        rendererTracingEnabled = renderer?.isTracingEnabled() ?? false;
      } catch (error) {
        const statusError = `Failed to read Renderer tracing state: ${errorMessage(error)}`;
        completionError = appendCompletionError(completionError, statusError);
      }
    }

    const traces = traceResult.traces;

    return {
      recording: false,
      contextId,
      completedRecordingAvailable: false,
      completedContextId: undefined,
      completionError,
      rendererTracingEnabled,
      tracingSupported: this.traceRuntime.isTracingSupported(),
      startedAtEpochMs,
      elapsedMs,
      traces,
      traceEventCount: traces.length,
      droppedTraceEventCount: traceResult.droppedTraceEventCount,
      timedOut,
    };
  }

  private assertContextMatches(requestedContextId: string, activeContextId: string | undefined): void {
    if (requestedContextId !== activeContextId) {
      throw new Error(
        `Valdi performance trace recording belongs to context ${String(activeContextId)}, not ${requestedContextId}.`,
      );
    }
  }

  private requireContextId(contextId: string): string {
    if (typeof contextId !== 'string' || contextId.length === 0) {
      throw new Error('A non-empty contextId is required for Valdi performance tracing.');
    }
    if (!isPerformanceTraceContextIdWithinLimit(contextId)) {
      throw new Error(
        `Valdi performance trace contextId exceeds the ${MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES}-byte serialized limit.`,
      );
    }
    return contextId;
  }

  private readRendererTracing(rendererTracing: boolean | undefined): boolean {
    if (rendererTracing !== undefined && typeof rendererTracing !== 'boolean') {
      throw new Error('rendererTracing must be a boolean when provided.');
    }
    return rendererTracing !== false;
  }

  private clearActiveRecordingState(): void {
    const timeoutId = this.recordingTimeoutId;
    this.recordingTimeoutId = undefined;
    if (timeoutId !== undefined) {
      this.traceRuntime.cancelTimeout(timeoutId);
    }
    this.recordingId = undefined;
    this.recordingStartedAtMs = undefined;
    this.recordingStartedAtEpochMs = undefined;
    this.rendererTracingWasEnabled = undefined;
    this.activeContextId = undefined;
    this.activeRenderer = undefined;
  }

  private storeCompletedRecording(recording: PerformanceTraceStopBody): void {
    this.clearCompletedRecording();
    this.completedRecording = recording;
    this.completedRecordingExpiryTimeoutId = this.traceRuntime.scheduleTimeout(() => {
      if (this.completedRecording === recording) {
        this.completedRecording = undefined;
        this.completedRecordingExpiryTimeoutId = undefined;
      }
    }, PERFORMANCE_TRACE_RESULT_TTL_MS);
  }

  private clearCompletedRecording(): void {
    const timeoutId = this.completedRecordingExpiryTimeoutId;
    this.completedRecording = undefined;
    this.completedRecordingExpiryTimeoutId = undefined;
    if (timeoutId !== undefined) {
      this.traceRuntime.cancelTimeout(timeoutId);
    }
  }

  private storeRecentlyCompletedRecording(recording: PerformanceTraceStopBody): void {
    this.clearRecentlyCompletedRecording();
    this.recentlyCompletedRecording = recording;
    this.recentlyCompletedRecordingExpiryTimeoutId = this.traceRuntime.scheduleTimeout(() => {
      if (this.recentlyCompletedRecording === recording) {
        this.recentlyCompletedRecording = undefined;
        this.recentlyCompletedRecordingExpiryTimeoutId = undefined;
      }
    }, PERFORMANCE_TRACE_RESULT_TTL_MS);
  }

  private clearRecentlyCompletedRecording(): void {
    const timeoutId = this.recentlyCompletedRecordingExpiryTimeoutId;
    this.recentlyCompletedRecording = undefined;
    this.recentlyCompletedRecordingExpiryTimeoutId = undefined;
    if (timeoutId !== undefined) {
      this.traceRuntime.cancelTimeout(timeoutId);
    }
  }
}
