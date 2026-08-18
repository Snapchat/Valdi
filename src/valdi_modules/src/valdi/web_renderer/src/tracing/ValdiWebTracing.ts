export interface ValdiWebTracing {
  beginTrace(tag: string): void;
  endTrace(): void;
  instantTrace(tag: string, args: readonly unknown[] | undefined): void;
}

const TRACE_NAME_PREFIX = 'Valdi.';

let currentTracing: ValdiWebTracing | undefined;

export function setValdiWebTracing(tracing: ValdiWebTracing | undefined): void {
  currentTracing = tracing;
}

export function isValdiWebTracingEnabled(): boolean {
  return currentTracing !== undefined;
}

export function beginValdiWebTrace(tag: string): void {
  const tracing = currentTracing;
  if (!tracing) {
    return;
  }

  try {
    tracing.beginTrace(tag);
  } catch (error) {
    logTracingError('begin', tag, error);
  }
}

export function endValdiWebTrace(): void {
  const tracing = currentTracing;
  if (!tracing) {
    return;
  }

  try {
    tracing.endTrace();
  } catch (error) {
    console.error('[ValdiWebTracing] Failed to end trace', error);
  }
}

export function instantValdiWebTrace(tag: string, args: readonly unknown[] | undefined): void {
  const tracing = currentTracing;
  if (!tracing) {
    return;
  }

  try {
    tracing.instantTrace(tag, args);
  } catch (error) {
    logTracingError('emit instant', tag, error);
  }
}

export function makeValdiWebTraceProxy(tag: string, callback: Function): (...parameters: any[]) => any {
  return function (this: unknown, ...parameters: any[]) {
    beginValdiWebTrace(tag);
    try {
      return callback.apply(this, parameters);
    } finally {
      endValdiWebTrace();
    }
  };
}

export function valdiWebTraceName(tag: string): string {
  return `${TRACE_NAME_PREFIX}${tag}`;
}

export function valdiWebTraceArguments(args: readonly unknown[] | undefined): Record<string, unknown> | undefined {
  if (!args || args.length === 0) {
    return undefined;
  }

  const traceArguments: Record<string, unknown> = {};
  for (let index = 0; index < args.length; index += 2) {
    traceArguments[String(args[index])] = args[index + 1];
  }
  return traceArguments;
}

function logTracingError(operation: string, tag: string, error: unknown): void {
  console.error(`[ValdiWebTracing] Failed to ${operation} trace '${tag}'`, error);
}
