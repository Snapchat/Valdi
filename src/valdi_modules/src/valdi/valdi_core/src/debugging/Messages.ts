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

export interface CustomMessageRequestBody {
  identifier: string;
  data: any;
}

export interface CustomMessageResponseBody {
  handled: boolean;
  data: any | undefined;
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

  export function errorResponse(requestId: string, error: string | Error) {
    let message: string;
    let stack: string | undefined;
    if (typeof error === 'string') {
      message = error;
    } else {
      message = `${error.name}: ${error.message}`;
      stack = error.stack;
    }

    const body: ErrorBody = {
      message,
      stack,
    };

    return JSON.stringify({ type: DaemonClientMessageType.ERROR_RESPONSE, requestId, body });
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
