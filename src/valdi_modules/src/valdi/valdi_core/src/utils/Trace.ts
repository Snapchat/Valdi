import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';

const runtime = getValdiRuntime();

export const enum RecordedTraceType {
  Duration = 0,
  Instant = 1,
}

export interface RecordedTrace {
  trace: string;
  startMicros: number;
  endMicros: number;
  threadId: number;
  type: RecordedTraceType;
  args?: Record<string, TraceArgumentValue>;
}

export type TraceArgumentValue = unknown;
export type TraceArgumentsFor<Args extends readonly unknown[]> = Args extends readonly []
  ? readonly []
  : Args extends readonly [infer Key, infer Value, ...infer Rest]
    ? Key extends string
      ? readonly [Key, Value, ...TraceArgumentsFor<Rest>]
      : never
    : never;

function traceArgumentsToRecord(args: unknown): Record<string, TraceArgumentValue> | undefined {
  if (!Array.isArray(args) || !args.length) {
    return undefined;
  }

  const out: Record<string, TraceArgumentValue> = {};
  let hasValue = false;
  for (let i = 0; i < args.length; i += 2) {
    const key = args[i];
    const value = args[i + 1];
    if (typeof key !== 'string') {
      continue;
    }

    out[key] = value;
    hasValue = true;
  }

  return hasValue ? out : undefined;
}

/**
 * Start recording traces happening in the Valdi Runtime.
 * It is absolutely essential that this call is eventually followed
 * by "stopTraceRecording", or the memory will keep growing.
 *
 * The function returns an identifier which should be passed in
 * to "stopTraceRecording"
 */
export function startTraceRecording(): number {
  return runtime.startTraceRecording();
}

/**
 * Stop recording the traces from a previous startTraceRecording call.
 * Returns the captured traces.
 */
export function stopTraceRecording(id: number): RecordedTrace[] {
  const result = runtime.stopTraceRecording(id);
  const out: RecordedTrace[] = [];

  for (let i = 0; i < result.length; ) {
    const trace = result[i++];
    const startMicros = result[i++];
    const endMicros = result[i++];
    const threadId = result[i++];
    const type = result[i++];
    const args = traceArgumentsToRecord(result[i++]);

    const recordedTrace: RecordedTrace = {
      trace,
      startMicros,
      endMicros,
      threadId,
      type,
    };
    if (args) {
      recordedTrace.args = args;
    }
    out.push(recordedTrace);
  }

  return out;
}

export function isTracingSupported(): boolean {
  return !!runtime.beginTrace && !!runtime.endTrace;
}

export function beginTrace(tag: string): void {
  const begin = runtime.beginTrace;
  if (begin) {
    begin(tag);
  }
}

export function endTrace(): void {
  const end = runtime.endTrace;
  if (end) {
    end();
  }
}

export function instantTrace(tag: string): void {
  const instant = runtime.instantTrace;
  if (instant) {
    instant(tag);
  }
}

export function instantTraceWithArgs<Args extends readonly unknown[]>(
  tag: string,
  args: Args & TraceArgumentsFor<Args>,
): void {
  const instant = runtime.instantTrace as ((tag: string, args: readonly TraceArgumentValue[]) => void) | undefined;
  if (instant) {
    instant(tag, args);
  }
}

/**
 * Execute the given function and associate it with a traced label
 * @param tag the trace tag to use
 * @param func to function to evaluate
 */
export function trace<T>(tag: string, func: () => T): T {
  const begin = runtime.beginTrace;
  const end = runtime.endTrace;
  if (!begin || !end) {
    return func();
  }

  begin(tag);
  try {
    return func();
  } finally {
    end();
  }
}

const TRACE_PROXY_KEY = '$trace-proxy-target';

function makeProxyFunction(tag: string, fn: (...params: any[]) => any): (...params: any[]) => any {
  const makeTraceProxy = runtime.makeTraceProxy;
  if (!makeTraceProxy) {
    return fn;
  }

  const proxyFunction = makeTraceProxy(tag, fn);
  Object.defineProperty(proxyFunction, 'name', { value: fn.name });
  (proxyFunction as any)[TRACE_PROXY_KEY] = fn;
  return proxyFunction;
}

function makeTraceProxyFunctionForProperty(
  target: any,
  propertyName: string,
  propertyDescriptor: PropertyDescriptor,
): ((...params: any[]) => any) | undefined {
  if (propertyDescriptor.get || propertyDescriptor.set || !propertyDescriptor.writable) {
    return undefined;
  }

  const propertyValue = propertyDescriptor.value;

  if (!(typeof propertyValue === 'function')) {
    return undefined;
  }
  const ctor = target.constructor;

  if (propertyValue === ctor) {
    return undefined;
  }

  if (propertyValue[TRACE_PROXY_KEY]) {
    return undefined;
  }

  const currentClassName = ctor?.name;
  const tag = currentClassName ? `${currentClassName}.${propertyName}` : `<anon>.${propertyName}`;

  return makeProxyFunction(tag, propertyValue);
}

/**
 * Setup the given object class so that calling its methods
 * will automatically trace the calls.
 * @param objectClass
 * @returns
 */
export function installTraceProxy(objectClass: new (...input: any[]) => any) {
  if (!runtime.makeTraceProxy) {
    return;
  }

  let current = objectClass.prototype;
  while (current) {
    if (
      current === Object.prototype ||
      current === Array.prototype ||
      current === Function.prototype ||
      current === Number.prototype
    ) {
      break;
    }

    const propertyNames = Object.getOwnPropertyNames(current);
    for (const propertyName of propertyNames) {
      const propertyDescriptor = Object.getOwnPropertyDescriptor(current, propertyName);
      if (!propertyDescriptor) {
        continue;
      }

      const fn = makeTraceProxyFunctionForProperty(current, propertyName, propertyDescriptor);
      if (fn) {
        current[propertyName] = fn;
      }
    }

    current = Object.getPrototypeOf(current);
  }
}

/**
 * Install trace proxies on all the methods of the given class.
 */
export function Trace(cls: new (...input: any[]) => any): void {
  installTraceProxy(cls);
}

/**
 * Install a trace proxy on a single method.
 */
export function TraceMethod(target: any, propertyKey: string, descriptor: PropertyDescriptor) {
  if (!runtime.makeTraceProxy) {
    return;
  }

  const fn = makeTraceProxyFunctionForProperty(target, propertyKey, descriptor);
  if (fn) {
    descriptor.value = fn;
  }
}
