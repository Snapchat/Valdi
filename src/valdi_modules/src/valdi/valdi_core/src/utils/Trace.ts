import { ValdiRuntime } from 'valdi_core/src/ValdiRuntime';

declare const runtime: ValdiRuntime;

export interface RecordedTrace {
  trace: string;
  startMicros: number;
  endMicros: number;
  threadId: number;
}

export interface TraceRecordingResult {
  traces: RecordedTrace[];
  droppedTraceEventCount: number;
}

const MAX_RECORDED_TRACE_COUNT = 10_000;
const MAX_TRACE_NAME_BYTES = 2048;

/**
 * Keep the serialized trace array below the daemon's 4 MiB trace-message limit.
 * The remaining 1 MiB is reserved for the response envelope and completion metadata.
 */
export const MAX_SERIALIZED_TRACE_EVENTS_BYTES = 3 * 1024 * 1024;

function utf8ByteLength(value: string, maximumBytes: number): number {
  let byteLength = 0;
  for (let index = 0; index < value.length; index++) {
    const codeUnit = value.charCodeAt(index);
    if (codeUnit <= 0x7f) {
      byteLength += 1;
    } else if (codeUnit <= 0x7ff) {
      byteLength += 2;
    } else if (codeUnit >= 0xd800 && codeUnit <= 0xdbff) {
      const nextCodeUnit = value.charCodeAt(index + 1);
      if (nextCodeUnit >= 0xdc00 && nextCodeUnit <= 0xdfff) {
        byteLength += 4;
        index++;
      } else {
        byteLength += 3;
      }
    } else {
      byteLength += 3;
    }
    if (byteLength > maximumBytes) {
      return byteLength;
    }
  }
  return byteLength;
}

function incrementDroppedTraceEventCount(value: number): number {
  return value >= Number.MAX_SAFE_INTEGER ? Number.MAX_SAFE_INTEGER : value + 1;
}

function normalizeNativeDroppedTraceEventCount(value: any): number {
  if (typeof value !== 'number' || !Number.isFinite(value) || !Number.isInteger(value) || value < 0) {
    return 0;
  }
  return Math.min(value, Number.MAX_SAFE_INTEGER);
}

function decodeTraceRecordingResult(
  result: any[],
  firstTraceIndex: number,
  nativeDroppedTraceEventCount: number,
): TraceRecordingResult {
  const traces: RecordedTrace[] = [];
  let droppedTraceEventCount = nativeDroppedTraceEventCount;
  let serializedTraceBytes = 2; // Opening and closing brackets for the trace array.

  for (let index = firstTraceIndex; index < result.length; index += 4) {
    if (result.length - index < 4) {
      droppedTraceEventCount = incrementDroppedTraceEventCount(droppedTraceEventCount);
      break;
    }

    const trace = result[index];
    const startMicros = result[index + 1];
    const endMicros = result[index + 2];
    const threadId = result[index + 3];
    if (
      typeof trace !== 'string' ||
      trace.length === 0 ||
      utf8ByteLength(trace, MAX_TRACE_NAME_BYTES) > MAX_TRACE_NAME_BYTES ||
      typeof startMicros !== 'number' ||
      !Number.isSafeInteger(startMicros) ||
      startMicros < 0 ||
      typeof endMicros !== 'number' ||
      !Number.isSafeInteger(endMicros) ||
      endMicros < startMicros ||
      typeof threadId !== 'number' ||
      !Number.isSafeInteger(threadId) ||
      threadId < 0
    ) {
      droppedTraceEventCount = incrementDroppedTraceEventCount(droppedTraceEventCount);
      continue;
    }

    const recordedTrace: RecordedTrace = { trace, startMicros, endMicros, threadId };
    const serializedTrace = JSON.stringify(recordedTrace);
    const separatorBytes = traces.length === 0 ? 0 : 1;
    const serializedEventBytes = utf8ByteLength(serializedTrace, MAX_SERIALIZED_TRACE_EVENTS_BYTES);
    if (
      traces.length >= MAX_RECORDED_TRACE_COUNT ||
      serializedTraceBytes + separatorBytes + serializedEventBytes > MAX_SERIALIZED_TRACE_EVENTS_BYTES
    ) {
      droppedTraceEventCount = incrementDroppedTraceEventCount(droppedTraceEventCount);
      continue;
    }

    traces.push(recordedTrace);
    serializedTraceBytes += separatorBytes + serializedEventBytes;
  }

  return { traces, droppedTraceEventCount };
}

export function decodeLegacyTraceRecordingResult(result: any[]): TraceRecordingResult {
  return decodeTraceRecordingResult(result, 0, 0);
}

export function decodeTraceRecordingResultWithNativeStats(result: any[]): TraceRecordingResult {
  return decodeTraceRecordingResult(result, 1, normalizeNativeDroppedTraceEventCount(result[0]));
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
  return stopTraceRecordingWithStats(id).traces;
}

/**
 * Stop recording and return both captured traces and the number discarded by native or
 * serialization bounds. Older runtimes retain the legacy trace-only method and are bounded here.
 */
export function stopTraceRecordingWithStats(id: number): TraceRecordingResult {
  if (runtime.stopTraceRecordingWithStats) {
    return decodeTraceRecordingResultWithNativeStats(runtime.stopTraceRecordingWithStats(id));
  }
  return decodeLegacyTraceRecordingResult(runtime.stopTraceRecording(id));
}

export function isTracingSupported(): boolean {
  return runtime.trace !== undefined;
}

/**
 * Execute the given function and associate it with a traced label
 * @param tag the trace tag to use
 * @param func to function to evaluate
 */
export function trace<T>(tag: string, func: () => T): T {
  if (!runtime.trace) {
    return func();
  } else {
    return runtime.trace(tag, func);
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
