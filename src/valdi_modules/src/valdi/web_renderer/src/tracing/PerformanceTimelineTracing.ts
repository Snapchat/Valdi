import { ValdiWebTracing, valdiWebTraceArguments, valdiWebTraceName } from './ValdiWebTracing';

interface ActiveTrace {
  readonly tag: string;
  readonly startTime: number;
}

export class PerformanceTimelineTracing implements ValdiWebTracing {
  private readonly activeTraces: ActiveTrace[] = [];

  beginTrace(tag: string): void {
    this.activeTraces.push({ tag, startTime: performance.now() });
  }

  endTrace(): void {
    const activeTrace = this.activeTraces.pop();
    if (!activeTrace) {
      return;
    }

    performance.measure(valdiWebTraceName(activeTrace.tag), {
      start: activeTrace.startTime,
      end: performance.now(),
    });
  }

  instantTrace(tag: string, args: readonly unknown[] | undefined): void {
    const traceName = valdiWebTraceName(tag);
    const detail = valdiWebTraceArguments(args);
    if (!detail) {
      performance.mark(traceName);
      return;
    }

    try {
      performance.mark(traceName, { detail });
    } catch (error) {
      if (!(error instanceof Error) || error.name !== 'DataCloneError') {
        throw error;
      }
      performance.mark(traceName);
    }
  }
}
