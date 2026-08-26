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

export function beginValdiWebTrace(tag: string): ValdiWebTracing | undefined {
  const tracing = currentTracing;
  if (!tracing) {
    return undefined;
  }

  try {
    tracing.beginTrace(tag);
    return tracing;
  } catch (error) {
    logTracingError('begin', tag, error);
    return undefined;
  }
}

export function endValdiWebTrace(tracing: ValdiWebTracing | undefined): void {
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
    const handle = beginValdiWebTrace(tag);
    try {
      return callback.apply(this, parameters);
    } finally {
      endValdiWebTrace(handle);
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
    Object.defineProperty(traceArguments, String(args[index]), {
      configurable: true,
      enumerable: true,
      value: args[index + 1],
      writable: true,
    });
  }
  return traceArguments;
}

function logTracingError(operation: string, tag: string, error: unknown): void {
  console.error(`[ValdiWebTracing] Failed to ${operation} trace '${tag}'`, error);
}
