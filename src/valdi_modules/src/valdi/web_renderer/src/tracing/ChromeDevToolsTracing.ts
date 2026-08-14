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

const TRACK_NAME = 'Valdi JS';
const TRACK_GROUP = 'Valdi';
const TRACK_COLOR: DevToolsColor = 'primary';

export class ChromeDevToolsTracing implements ValdiWebTracing {
  private readonly activeTraces: ActiveTrace[] = [];

  beginTrace(tag: string): void {
    this.activeTraces.push({ tag, startTime: performance.now() });
  }

  endTrace(): void {
    const activeTrace = this.activeTraces.pop();
    if (!activeTrace) {
      return;
    }

    console.timeStamp(
      valdiWebTraceName(activeTrace.tag),
      activeTrace.startTime,
      performance.now(),
      TRACK_NAME,
      TRACK_GROUP,
      TRACK_COLOR,
    );
  }

  instantTrace(tag: string, args: readonly unknown[] | undefined): void {
    const timestamp = performance.now();
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
