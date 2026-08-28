import { IRenderedVirtualNodeData } from '../IRenderedVirtualNodeData';

export const enum DaemonClientMessageType {
  ERROR_RESPONSE = -1,
  LIST_CONTEXTS_REQUEST = 2,
  LIST_CONTEXTS_RESPONSE = -2,
  GET_CONTEXT_TREE_REQUEST = 3,
  GET_CONTEXT_TREE_RESPONSE = -3,
  TAKE_ELEMENT_SNAPSHOT_REQUEST = 4,
  TAKE_ELEMENT_SNAPSHOT_RESPONSE = -4,
  DUMP_HEAP_REQUEST = 5,
  DUMP_HEAP_RESPONSE = -5,
  PERFORMANCE_TRACE_STATUS_REQUEST = 6,
  PERFORMANCE_TRACE_STATUS_RESPONSE = -6,
  PERFORMANCE_TRACE_START_REQUEST = 7,
  PERFORMANCE_TRACE_START_RESPONSE = -7,
  PERFORMANCE_TRACE_STOP_REQUEST = 8,
  PERFORMANCE_TRACE_STOP_RESPONSE = -8,
  CUSTOM_REQUEST = 1000,
  CUSTOM_RESPONSE = -1000,
}

interface DaemonClientMessageBase<Type extends DaemonClientMessageType, BodyType> {
  senderClientId: number;
  requestId: string;
  type: Type;
  body: BodyType;
}

export interface ErrorBody {
  message: string;
  stack: string | undefined;
}

export interface RemoteValdiContext {
  id: string;
  rootComponentName: string;
}

export interface GetContextTreeBody {
  id: string;
  includeComponentData?: boolean;
}

export interface TakeElementSnapshotBody {
  contextId: string;
  elementId: number;
}

export interface DumpHeapRequestBody {
  performGC: boolean;
}

export interface DumpHeapResponseBody {
  memoryUsageBytes: number;
  heapDumpJSON: string;
}

export interface PerformanceTraceStatusRequestBody {
  contextId?: string;
}

export interface PerformanceTraceStartRequestBody {
  contextId: string;
  rendererTracing?: boolean;
}

export interface PerformanceTraceStopRequestBody {
  contextId: string;
}

export interface PerformanceTraceEvent {
  trace: string;
  startMicros: number;
  endMicros: number;
  threadId: number;
}

export interface PerformanceTraceStatusBody {
  recording: boolean;
  contextId: string | undefined;
  completedRecordingAvailable: boolean;
  completedContextId: string | undefined;
  completionError: string | undefined;
  rendererTracingEnabled: boolean;
  tracingSupported: boolean;
  startedAtEpochMs: number | undefined;
  elapsedMs: number | undefined;
}

export interface PerformanceTraceStopBody extends PerformanceTraceStatusBody {
  traces: PerformanceTraceEvent[];
  traceEventCount: number;
  droppedTraceEventCount: number;
  timedOut: boolean;
}

export interface CustomMessageRequestBody {
  identifier: string;
  data: any;
}

export interface CustomMessageResponseBody {
  handled: boolean;
  data: any | undefined;
}

export const MAX_PERFORMANCE_TRACE_MESSAGE_BYTES = 4 * 1024 * 1024;
export const MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES = 4096;
export const MAX_PERFORMANCE_TRACE_ERROR_BYTES = 64 * 1024;
const MAX_DEBUGGER_ERROR_STACK_BYTES = 128 * 1024;

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
    if (byteLength > maximumBytes) return byteLength;
  }
  return byteLength;
}

function serializedByteLength(value: unknown, maximumBytes: number): number {
  return utf8ByteLength(JSON.stringify(value), maximumBytes);
}

export function truncateDebuggerString(value: string, maximumSerializedBytes: number): string {
  if (serializedByteLength(value, maximumSerializedBytes) <= maximumSerializedBytes) return value;

  const suffix = '…';
  let minimumLength = 0;
  let maximumLength = value.length;
  let truncated = suffix;
  while (minimumLength <= maximumLength) {
    const candidateLength = Math.floor((minimumLength + maximumLength) / 2);
    const candidate = `${value.slice(0, candidateLength)}${suffix}`;
    if (serializedByteLength(candidate, maximumSerializedBytes) <= maximumSerializedBytes) {
      truncated = candidate;
      minimumLength = candidateLength + 1;
    } else {
      maximumLength = candidateLength - 1;
    }
  }
  return truncated;
}

export function isPerformanceTraceContextIdWithinLimit(contextId: string): boolean {
  return (
    contextId.length > 0 &&
    serializedByteLength(contextId, MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES) <= MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES
  );
}

export function truncatePerformanceTraceError(error: string): string {
  return truncateDebuggerString(error, MAX_PERFORMANCE_TRACE_ERROR_BYTES);
}

function sanitizeOptionalTraceString(value: string | undefined, maximumSerializedBytes: number): string | undefined {
  return value === undefined ? undefined : truncateDebuggerString(value, maximumSerializedBytes);
}

function sanitizePerformanceTraceStatusBody(body: PerformanceTraceStatusBody): PerformanceTraceStatusBody {
  return {
    ...body,
    contextId: sanitizeOptionalTraceString(body.contextId, MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES),
    completedContextId: sanitizeOptionalTraceString(body.completedContextId, MAX_PERFORMANCE_TRACE_CONTEXT_ID_BYTES),
    completionError: sanitizeOptionalTraceString(body.completionError, MAX_PERFORMANCE_TRACE_ERROR_BYTES),
  };
}

function saturatingAdd(left: number, right: number): number {
  return left >= Number.MAX_SAFE_INTEGER - right ? Number.MAX_SAFE_INTEGER : left + right;
}

function serializePerformanceTraceMessage(
  type: DaemonClientMessageType,
  requestId: string,
  body: PerformanceTraceStatusBody,
): string {
  const serialized = JSON.stringify({ type, requestId, body: sanitizePerformanceTraceStatusBody(body) });
  if (utf8ByteLength(serialized, MAX_PERFORMANCE_TRACE_MESSAGE_BYTES) > MAX_PERFORMANCE_TRACE_MESSAGE_BYTES) {
    throw new Error('Valdi performance trace response exceeds the protocol size limit.');
  }
  return serialized;
}

function serializePerformanceTraceStopMessage(requestId: string, body: PerformanceTraceStopBody): string {
  const sanitizedStatus = sanitizePerformanceTraceStatusBody(body);
  const originalTraces = body.traces;
  const originalDroppedTraceEventCount =
    Number.isSafeInteger(body.droppedTraceEventCount) && body.droppedTraceEventCount >= 0
      ? body.droppedTraceEventCount
      : 0;
  const serializeTracePrefix = (traceCount: number): string => {
    const omittedTraceCount = originalTraces.length - traceCount;
    const boundedBody: PerformanceTraceStopBody = {
      ...body,
      ...sanitizedStatus,
      traces: originalTraces.slice(0, traceCount),
      traceEventCount: traceCount,
      droppedTraceEventCount: saturatingAdd(originalDroppedTraceEventCount, omittedTraceCount),
    };
    return JSON.stringify({
      type: DaemonClientMessageType.PERFORMANCE_TRACE_STOP_RESPONSE,
      requestId,
      body: boundedBody,
    });
  };

  let minimumTraceCount = 0;
  let maximumTraceCount = originalTraces.length;
  let serialized = serializeTracePrefix(0);
  if (utf8ByteLength(serialized, MAX_PERFORMANCE_TRACE_MESSAGE_BYTES) > MAX_PERFORMANCE_TRACE_MESSAGE_BYTES) {
    throw new Error('Valdi performance trace response metadata exceeds the protocol size limit.');
  }
  while (minimumTraceCount <= maximumTraceCount) {
    const candidateTraceCount = Math.floor((minimumTraceCount + maximumTraceCount) / 2);
    const candidate = serializeTracePrefix(candidateTraceCount);
    if (utf8ByteLength(candidate, MAX_PERFORMANCE_TRACE_MESSAGE_BYTES) <= MAX_PERFORMANCE_TRACE_MESSAGE_BYTES) {
      serialized = candidate;
      minimumTraceCount = candidateTraceCount + 1;
    } else {
      maximumTraceCount = candidateTraceCount - 1;
    }
  }
  return serialized;
}

export type ErrorResponse = DaemonClientMessageBase<DaemonClientMessageType.ERROR_RESPONSE, ErrorBody>;
export type ListContextsRequest = DaemonClientMessageBase<DaemonClientMessageType.LIST_CONTEXTS_REQUEST, {}>;
export type ListContextsResponse = DaemonClientMessageBase<
  DaemonClientMessageType.LIST_CONTEXTS_RESPONSE,
  RemoteValdiContext[]
>;

export type GetContextTreeRequest = DaemonClientMessageBase<
  DaemonClientMessageType.GET_CONTEXT_TREE_REQUEST,
  GetContextTreeBody
>;
export type GetContextTreeResponse = DaemonClientMessageBase<
  DaemonClientMessageType.GET_CONTEXT_TREE_RESPONSE,
  IRenderedVirtualNodeData
>;

export type DumpHeapRequest = DaemonClientMessageBase<DaemonClientMessageType.DUMP_HEAP_REQUEST, DumpHeapRequestBody>;
export type DumpHeapResponse = DaemonClientMessageBase<
  DaemonClientMessageType.DUMP_HEAP_RESPONSE,
  DumpHeapResponseBody
>;

export type TakeElementSnapshotRequest = DaemonClientMessageBase<
  DaemonClientMessageType.TAKE_ELEMENT_SNAPSHOT_REQUEST,
  TakeElementSnapshotBody
>;
export type TakeElementSnapshotResponse = DaemonClientMessageBase<
  DaemonClientMessageType.TAKE_ELEMENT_SNAPSHOT_RESPONSE,
  string
>;

export type PerformanceTraceStatusRequest = DaemonClientMessageBase<
  DaemonClientMessageType.PERFORMANCE_TRACE_STATUS_REQUEST,
  PerformanceTraceStatusRequestBody
>;
export type PerformanceTraceStatusResponse = DaemonClientMessageBase<
  DaemonClientMessageType.PERFORMANCE_TRACE_STATUS_RESPONSE,
  PerformanceTraceStatusBody
>;
export type PerformanceTraceStartRequest = DaemonClientMessageBase<
  DaemonClientMessageType.PERFORMANCE_TRACE_START_REQUEST,
  PerformanceTraceStartRequestBody
>;
export type PerformanceTraceStartResponse = DaemonClientMessageBase<
  DaemonClientMessageType.PERFORMANCE_TRACE_START_RESPONSE,
  PerformanceTraceStatusBody
>;
export type PerformanceTraceStopRequest = DaemonClientMessageBase<
  DaemonClientMessageType.PERFORMANCE_TRACE_STOP_REQUEST,
  PerformanceTraceStopRequestBody
>;
export type PerformanceTraceStopResponse = DaemonClientMessageBase<
  DaemonClientMessageType.PERFORMANCE_TRACE_STOP_RESPONSE,
  PerformanceTraceStopBody
>;

export type CustomMessageRequest = DaemonClientMessageBase<
  DaemonClientMessageType.CUSTOM_REQUEST,
  CustomMessageRequestBody
>;
export type CustomMessageResponse = DaemonClientMessageBase<
  DaemonClientMessageType.CUSTOM_RESPONSE,
  CustomMessageResponseBody
>;

export type DaemonClientMessage =
  | ListContextsRequest
  | ListContextsResponse
  | GetContextTreeRequest
  | GetContextTreeResponse
  | TakeElementSnapshotRequest
  | TakeElementSnapshotResponse
  | DumpHeapRequest
  | DumpHeapResponse
  | PerformanceTraceStatusRequest
  | PerformanceTraceStatusResponse
  | PerformanceTraceStartRequest
  | PerformanceTraceStartResponse
  | PerformanceTraceStopRequest
  | PerformanceTraceStopResponse
  | CustomMessageRequest
  | CustomMessageResponse
  | ErrorResponse;

export function isAnyResponse(message: DaemonClientMessage): boolean {
  return message.type < 0;
}

export namespace Messages {
  export function listContextsRequest(requestId: string): string {
    return JSON.stringify({ type: DaemonClientMessageType.LIST_CONTEXTS_REQUEST, requestId, body: {} });
  }

  export function listContextsResponse(requestId: string, contexts: RemoteValdiContext[]) {
    return JSON.stringify({ type: DaemonClientMessageType.LIST_CONTEXTS_RESPONSE, requestId, body: contexts });
  }

  export function getContextTreeRequest(requestId: string, body: GetContextTreeBody) {
    return JSON.stringify({
      type: DaemonClientMessageType.GET_CONTEXT_TREE_REQUEST,
      requestId,
      body,
    });
  }

  export function getContextTreeResponse(requestId: string, body: IRenderedVirtualNodeData) {
    return JSON.stringify({
      type: DaemonClientMessageType.GET_CONTEXT_TREE_RESPONSE,
      requestId,
      body,
    });
  }

  export function takeElementSnapshotRequest(requestId: string, body: TakeElementSnapshotBody) {
    return JSON.stringify({
      type: DaemonClientMessageType.TAKE_ELEMENT_SNAPSHOT_REQUEST,
      requestId,
      body,
    });
  }

  export function takeElementSnapshotResponse(requestId: string, data: string) {
    return JSON.stringify({
      type: DaemonClientMessageType.TAKE_ELEMENT_SNAPSHOT_RESPONSE,
      requestId,
      body: data,
    });
  }

  export function dumpHeapRequest(requestId: string, body: DumpHeapRequestBody) {
    return JSON.stringify({
      type: DaemonClientMessageType.DUMP_HEAP_REQUEST,
      requestId,
      body: body,
    });
  }

  export function dumpHeapResponse(requestId: string, body: DumpHeapResponseBody) {
    return JSON.stringify({
      type: DaemonClientMessageType.DUMP_HEAP_RESPONSE,
      requestId,
      body: body,
    });
  }

  export function performanceTraceStatusRequest(requestId: string, body: PerformanceTraceStatusRequestBody): string {
    return JSON.stringify({
      type: DaemonClientMessageType.PERFORMANCE_TRACE_STATUS_REQUEST,
      requestId,
      body,
    });
  }

  export function performanceTraceStatusResponse(requestId: string, body: PerformanceTraceStatusBody): string {
    return serializePerformanceTraceMessage(DaemonClientMessageType.PERFORMANCE_TRACE_STATUS_RESPONSE, requestId, body);
  }

  export function performanceTraceStartRequest(requestId: string, body: PerformanceTraceStartRequestBody): string {
    return JSON.stringify({
      type: DaemonClientMessageType.PERFORMANCE_TRACE_START_REQUEST,
      requestId,
      body,
    });
  }

  export function performanceTraceStartResponse(requestId: string, body: PerformanceTraceStatusBody): string {
    return serializePerformanceTraceMessage(DaemonClientMessageType.PERFORMANCE_TRACE_START_RESPONSE, requestId, body);
  }

  export function performanceTraceStopRequest(requestId: string, body: PerformanceTraceStopRequestBody): string {
    return JSON.stringify({
      type: DaemonClientMessageType.PERFORMANCE_TRACE_STOP_REQUEST,
      requestId,
      body,
    });
  }

  export function performanceTraceStopResponse(requestId: string, body: PerformanceTraceStopBody): string {
    return serializePerformanceTraceStopMessage(requestId, body);
  }

  export function customMessageRequest(requestId: string, body: CustomMessageRequestBody) {
    return JSON.stringify({
      type: DaemonClientMessageType.CUSTOM_REQUEST,
      requestId,
      body,
    });
  }

  export function customMessageResponse(requestId: string, body: CustomMessageResponseBody) {
    return JSON.stringify({
      type: DaemonClientMessageType.CUSTOM_RESPONSE,
      requestId,
      body,
    });
  }

  export function errorResponse(requestId: string, error: string | Error): string {
    let message: string;
    let stack: string | undefined;
    if (typeof error === 'string') {
      message = error;
    } else {
      message = `${error.name}: ${error.message}`;
      stack = error.stack;
    }

    const body: ErrorBody = {
      message: truncateDebuggerString(message, MAX_PERFORMANCE_TRACE_ERROR_BYTES),
      stack: sanitizeOptionalTraceString(stack, MAX_DEBUGGER_ERROR_STACK_BYTES),
    };

    const serialized = JSON.stringify({ type: DaemonClientMessageType.ERROR_RESPONSE, requestId, body });
    if (utf8ByteLength(serialized, MAX_PERFORMANCE_TRACE_MESSAGE_BYTES) > MAX_PERFORMANCE_TRACE_MESSAGE_BYTES) {
      throw new Error('Valdi debugger error response exceeds the protocol size limit.');
    }
    return serialized;
  }

  export function parse(senderClientId: number, jsonBlob: any): DaemonClientMessage {
    if (typeof jsonBlob === 'string' && senderClientId) {
      const message = JSON.parse(jsonBlob);
      if (message.type && message.body !== undefined && message.requestId) {
        return {
          senderClientId: senderClientId,
          requestId: message.requestId,
          body: message.body,
          type: message.type,
        };
      }
    }

    throw new Error(`Failed to parse message (${typeof senderClientId}, ${typeof jsonBlob})`);
  }
}
