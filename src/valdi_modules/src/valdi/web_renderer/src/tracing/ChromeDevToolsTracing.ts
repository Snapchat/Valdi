import { ValdiWebTracing, valdiWebTraceArguments, valdiWebTraceName } from './ValdiWebTracing';

type DevToolsColor =
  | 'primary'
  | 'primary-light'
  | 'primary-dark'
  | 'secondary'
  | 'secondary-light'
  | 'secondary-dark'
  | 'tertiary'
  | 'tertiary-light'
  | 'tertiary-dark'
  | 'error';

declare global {
  interface Console {
    timeStamp(
      label: string,
      start: string | number,
      end: string | number | undefined,
      trackName: string,
      trackGroup: string,
      color: DevToolsColor,
      data?: Record<string, unknown>,
    ): void;
  }
}

interface ActiveTrace {
  readonly tag: string;
  readonly startTime: number;
}

export interface WebTracingClock {
  now(): number;
}

const TRACK_NAME = 'Valdi JS';
const TRACK_GROUP = 'Valdi';
const TRACK_COLOR: DevToolsColor = 'primary';
const BROWSER_WEB_TRACING_CLOCK: WebTracingClock = {
  now: () => globalThis.performance.now(),
};

export class ChromeDevToolsTracing implements ValdiWebTracing {
  private readonly activeTraces: ActiveTrace[] = [];

  constructor(private readonly clock: WebTracingClock) {}

  beginTrace(tag: string): void {
    this.activeTraces.push({ tag, startTime: this.clock.now() });
  }

  endTrace(): void {
    const activeTrace = this.activeTraces.pop();
    if (!activeTrace) {
      return;
    }

    console.timeStamp(
      valdiWebTraceName(activeTrace.tag),
      activeTrace.startTime,
      this.clock.now(),
      TRACK_NAME,
      TRACK_GROUP,
      TRACK_COLOR,
    );
  }

  instantTrace(tag: string, args: readonly unknown[] | undefined): void {
    const timestamp = this.clock.now();
    console.timeStamp(
      valdiWebTraceName(tag),
      timestamp,
      timestamp,
      TRACK_NAME,
      TRACK_GROUP,
      TRACK_COLOR,
      valdiWebTraceArguments(args),
    );
  }
}

export function createBrowserChromeDevToolsTracing(): ChromeDevToolsTracing {
  return new ChromeDevToolsTracing(BROWSER_WEB_TRACING_CLOCK);
}
