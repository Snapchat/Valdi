/**
 * Valdi daemon TCP client — connects directly to the device's DebuggerService
 * and implements both the ValdiPacket framing protocol and the Messages.ts
 * inner inspection protocol.
 *
 * Wire format (ValdiPacket):
 *   [0x33 0xC6 0x00 0x01][uint32LE payload_length][UTF-8 JSON payload]
 *
 * Outer envelope:
 *   Request: {"request": { ...fields..., "request_id": "N" }}
 *   Response: {"response": { ...fields..., "request_id": "N" }}
 *   Event: {"event": { ...fields... }}
 *
 * Session handshake (device initiates):
 *   1. CLI connects to port 13592 (device's DebuggerService TCP server).
 *   2. Device sends: {"request":{"configure":{...},"request_id":"1"}}
 *   3. CLI responds:  {"response":{"configure":{},"request_id":"1"}}
 *   4. Device sends: {"event":{"js_debugger_info":{...}}}  (informational)
 *
 * Inner Messages.ts inspection protocol (direct-to-device):
 *   CLI sends:  {"event":{"payload_from_client":{"sender_client_id":1,"payload_string":"..."}}}
 *     where payload_string = JSON.stringify({ type: <positive int>, requestId: "<id>", body: {} })
 *   Device responds: {"request":{"forward_client_payload":{"client_id":1,"payload_string":"..."},"request_id":"N"}}
 *     CLI auto-responds with an empty response; inner payload_string contains the Messages.ts response.
 */

import * as net from 'net';
import * as os from 'os';
import * as path from 'path';
import * as fs from 'fs';
import { CliError } from '../core/errors';
import { getUserChoice, runCliCommand } from './cliUtils';

// ─── Ports ───────────────────────────────────────────────────────────────────

/** DebuggerService port for standalone Valdi apps: macOS, CLI runner, standalone iOS/Android. */
export const STANDALONE_PORT = 13591;
/** DebuggerService port for in-app mobile targets (iOS in Snapchat, Android). */
export const MOBILE_PORT = 13592;
export const DEFAULT_PORT = MOBILE_PORT;

// ─── Messages.ts protocol (inlined so CLI stays self-contained in open_source) ─

export const enum DaemonMsgType {
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

// ─── Config ──────────────────────────────────────────────────────────────────

export interface InspectConfig {
  selectedClientId?: string;
}

const CONFIG_PATH = path.join(os.homedir(), '.valdi-inspect.json');

export function loadInspectConfig(): InspectConfig {
  try {
    return JSON.parse(fs.readFileSync(CONFIG_PATH, 'utf8')) as InspectConfig;
  } catch {
    return {};
  }
}

export function saveInspectConfig(config: InspectConfig): void {
  fs.writeFileSync(CONFIG_PATH, JSON.stringify(config, null, 2), 'utf8');
}

// ─── Types ───────────────────────────────────────────────────────────────────

export interface DaemonConnectedClient {
  client_id: string;
  platform: string;
  application_id: string;
}

export interface RemoteContext {
  id: string;
  rootComponentName: string;
}

/** An existing platform tunnel must not be replaced by generic ADB forwarding. */
export interface DaemonConnectionEndpoint {
  readonly autoForward: boolean;
  readonly deviceId?: string;
  readonly port: number;
}

export interface PerformanceTraceStatusRequestBody extends Record<string, unknown> {
  contextId?: string;
}

export interface PerformanceTraceStartRequestBody extends Record<string, unknown> {
  contextId: string;
  rendererTracing?: boolean;
}

export interface PerformanceTraceStopRequestBody extends Record<string, unknown> {
  contextId: string;
}

export interface DaemonPerformanceTraceStatusBody extends Record<string, unknown> {
  recording: boolean;
  contextId?: string;
  completedRecordingAvailable: boolean;
  completedContextId?: string;
  completionError?: string;
  rendererTracingEnabled: boolean;
  tracingSupported: boolean;
  startedAtEpochMs?: number;
  elapsedMs?: number;
}

export interface DaemonPerformanceTraceStopBody extends DaemonPerformanceTraceStatusBody {
  traces: Array<Record<string, unknown>>;
  traceEventCount: number;
  droppedTraceEventCount: number;
  timedOut: boolean;
}

export interface CustomRequestResponse {
  data?: Record<string, unknown>;
  handled: boolean;
}

export class DaemonProtocolError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'DaemonProtocolError';
  }
}

// ─── Packet encoding ─────────────────────────────────────────────────────────

const MAGIC = Buffer.from([0x33, 0xc6, 0x00, 0x01]);
const HEADER_SIZE = 8; // 4 magic + 4 uint32LE length
// Heap dumps and element snapshots legitimately exceed trace-sized payloads. Keep generic
// framing bounded while applying the tighter trace limit once the inner request is identified.
export const MAX_DAEMON_INNER_PAYLOAD_BYTES = 64 * 1024 * 1024;
export const MAX_DAEMON_PACKET_PAYLOAD_BYTES = 128 * 1024 * 1024;
export const MAX_DAEMON_TRACE_PAYLOAD_BYTES = 4 * 1024 * 1024;
const MAX_DAEMON_BUFFERED_BYTES = HEADER_SIZE + MAX_DAEMON_PACKET_PAYLOAD_BYTES;

function encodePacket(json: object): Buffer {
  const serialized = JSON.stringify(json);
  const payloadLength = Buffer.byteLength(serialized, 'utf8');
  if (payloadLength > MAX_DAEMON_PACKET_PAYLOAD_BYTES) {
    throw new Error(`ValdiPacket payload exceeds the ${MAX_DAEMON_PACKET_PAYLOAD_BYTES}-byte limit.`);
  }
  const payload = Buffer.from(serialized, 'utf8');
  const header = Buffer.alloc(8);
  MAGIC.copy(header, 0);
  header.writeUInt32LE(payload.length, 4);
  return Buffer.concat([header, payload]);
}

// ─── DaemonConnection ────────────────────────────────────────────────────────

// In the direct-to-device protocol we are "client 1" (non-zero required by device JS check).
const DIRECT_CLIENT_ID = 1;
const ADB_FORWARD_REFRESH_INTERVAL_MS = 5000;
const adbForwardedAtByEndpoint = new Map<string, number>();
const adbForwardRefreshByEndpoint = new Map<string, Promise<void>>();

interface PendingRequest {
  resolve: (value: Record<string, unknown>) => void;
  reject: (err: Error) => void;
}

interface PendingPayloadRequest extends PendingRequest {
  maxPayloadBytes: number;
}

const MAX_DAEMON_TRACE_EVENT_COUNT = 10_000;
const MAX_DAEMON_TRACE_NAME_BYTES = 2048;
const MAX_DAEMON_TRACE_CONTEXT_ID_BYTES = 4096;
const MAX_DAEMON_TRACE_ERROR_BYTES = 64 * 1024;

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function isPerformanceTraceMessageType(value: unknown): boolean {
  return (
    value === DaemonMsgType.PERFORMANCE_TRACE_STATUS_REQUEST ||
    value === DaemonMsgType.PERFORMANCE_TRACE_STATUS_RESPONSE ||
    value === DaemonMsgType.PERFORMANCE_TRACE_START_REQUEST ||
    value === DaemonMsgType.PERFORMANCE_TRACE_START_RESPONSE ||
    value === DaemonMsgType.PERFORMANCE_TRACE_STOP_REQUEST ||
    value === DaemonMsgType.PERFORMANCE_TRACE_STOP_RESPONSE
  );
}

function isPerformanceTraceRequestType(value: DaemonMsgType): boolean {
  return (
    value === DaemonMsgType.PERFORMANCE_TRACE_STATUS_REQUEST ||
    value === DaemonMsgType.PERFORMANCE_TRACE_START_REQUEST ||
    value === DaemonMsgType.PERFORMANCE_TRACE_STOP_REQUEST
  );
}

function assertOptionalFiniteNumber(body: Record<string, unknown>, key: string): void {
  const value = body[key];
  if (value !== undefined && (typeof value !== 'number' || !Number.isFinite(value))) {
    throw new TypeError(`Valdi runtime trace response has an invalid ${key}.`);
  }
}

function assertOptionalBoundedString(
  body: Record<string, unknown>,
  key: string,
  maximumBytes: number,
  allowEmpty: boolean,
): void {
  const value = body[key];
  if (
    value !== undefined &&
    (typeof value !== 'string' ||
      (!allowEmpty && value.length === 0) ||
      Buffer.byteLength(value, 'utf8') > maximumBytes)
  ) {
    throw new TypeError(`Valdi runtime trace response has an invalid ${key}.`);
  }
}

function validatePerformanceTraceStatusBody(value: unknown): DaemonPerformanceTraceStatusBody {
  if (!isRecord(value)) {
    throw new TypeError('Valdi runtime trace response body must be an object.');
  }
  for (const key of ['recording', 'completedRecordingAvailable', 'rendererTracingEnabled', 'tracingSupported']) {
    if (typeof value[key] !== 'boolean') {
      throw new TypeError(`Valdi runtime trace response has an invalid ${key}.`);
    }
  }
  assertOptionalBoundedString(value, 'contextId', MAX_DAEMON_TRACE_CONTEXT_ID_BYTES, false);
  assertOptionalBoundedString(value, 'completedContextId', MAX_DAEMON_TRACE_CONTEXT_ID_BYTES, false);
  assertOptionalBoundedString(value, 'completionError', MAX_DAEMON_TRACE_ERROR_BYTES, true);
  assertOptionalFiniteNumber(value, 'startedAtEpochMs');
  assertOptionalFiniteNumber(value, 'elapsedMs');
  if (value['recording'] && (typeof value['contextId'] !== 'string' || value['contextId'].length === 0)) {
    throw new Error('Valdi runtime trace response is recording without a contextId.');
  }
  if (
    value['completedRecordingAvailable'] &&
    (typeof value['completedContextId'] !== 'string' || value['completedContextId'].length === 0)
  ) {
    throw new Error('Valdi runtime trace response has a completed recording without a completedContextId.');
  }
  return value as unknown as DaemonPerformanceTraceStatusBody;
}

function validatePerformanceTraceStopBody(value: unknown): DaemonPerformanceTraceStopBody {
  const status = validatePerformanceTraceStatusBody(value);
  const body = status as unknown as Record<string, unknown>;
  const traces = body['traces'];
  if (!Array.isArray(traces) || traces.length > MAX_DAEMON_TRACE_EVENT_COUNT) {
    throw new TypeError('Valdi runtime trace stop response has an invalid traces array.');
  }
  for (const trace of traces) {
    if (
      !isRecord(trace) ||
      typeof trace['trace'] !== 'string' ||
      trace['trace'].length === 0 ||
      Buffer.byteLength(trace['trace'], 'utf8') > MAX_DAEMON_TRACE_NAME_BYTES ||
      typeof trace['startMicros'] !== 'number' ||
      !Number.isSafeInteger(trace['startMicros']) ||
      trace['startMicros'] < 0 ||
      typeof trace['endMicros'] !== 'number' ||
      !Number.isSafeInteger(trace['endMicros']) ||
      trace['endMicros'] < trace['startMicros'] ||
      typeof trace['threadId'] !== 'number' ||
      !Number.isSafeInteger(trace['threadId']) ||
      trace['threadId'] < 0
    ) {
      throw new TypeError('Valdi runtime trace stop response contains a malformed trace event.');
    }
  }
  if (
    typeof body['traceEventCount'] !== 'number' ||
    !Number.isSafeInteger(body['traceEventCount']) ||
    body['traceEventCount'] < 0 ||
    body['traceEventCount'] !== traces.length ||
    typeof body['droppedTraceEventCount'] !== 'number' ||
    !Number.isSafeInteger(body['droppedTraceEventCount']) ||
    body['droppedTraceEventCount'] < 0 ||
    typeof body['timedOut'] !== 'boolean'
  ) {
    throw new TypeError('Valdi runtime trace stop response has invalid completion metadata.');
  }
  if (typeof body['contextId'] !== 'string' || body['contextId'].length === 0) {
    throw new TypeError('Valdi runtime trace stop response has an invalid contextId.');
  }
  return value as DaemonPerformanceTraceStopBody;
}

export class DaemonConnection {
  private socket: net.Socket;
  private recvBuf: Buffer = Buffer.alloc(0);
  private reqCounter = 0;
  private msgCounter = 0;
  private pendingRequests = new Map<string, PendingRequest>();
  private payloadListeners = new Map<string, PendingPayloadRequest>();
  // Resolved when the device sends its initial configure request (session ready)
  private configureReady: PendingRequest | null = null;
  // Saved from the device's configure handshake
  private configureData: Record<string, unknown> | null = null;

  constructor(socket: net.Socket) {
    this.socket = socket;
    socket.on('data', (data: Buffer) => this.onData(data));
    socket.on('close', () => this.rejectAllPending(new Error('Connection closed unexpectedly')));
    socket.on('error', err => this.rejectAllPending(err));
  }

  private rejectAllPending(err: Error): void {
    this.configureReady?.reject(err);
    this.configureReady = null;
    for (const pending of this.pendingRequests.values()) pending.reject(err);
    for (const listener of this.payloadListeners.values()) listener.reject(err);
    this.pendingRequests.clear();
    this.payloadListeners.clear();
  }

  private onData(chunk: Buffer): void {
    const bufferedLength = this.recvBuf.length + chunk.length;
    if (bufferedLength > MAX_DAEMON_BUFFERED_BYTES) {
      this.failProtocol(`ValdiPacket buffered data exceeds the ${MAX_DAEMON_BUFFERED_BYTES}-byte limit.`);
      return;
    }
    this.recvBuf = this.recvBuf.length === 0 ? chunk : Buffer.concat([this.recvBuf, chunk], bufferedLength);
    this.drainBuffer();
  }

  private drainBuffer(): void {
    for (;;) {
      if (this.recvBuf.length < HEADER_SIZE) break;

      if (
        this.recvBuf[0] !== 0x33 ||
        this.recvBuf[1] !== 0xc6 ||
        this.recvBuf[2] !== 0x00 ||
        this.recvBuf[3] !== 0x01
      ) {
        this.failProtocol('ValdiPacket: bad magic');
        break;
      }

      const payloadLen = this.recvBuf.readUInt32LE(4);
      if (payloadLen > MAX_DAEMON_PACKET_PAYLOAD_BYTES) {
        this.failProtocol(`ValdiPacket payload exceeds the ${MAX_DAEMON_PACKET_PAYLOAD_BYTES}-byte limit.`);
        break;
      }
      if (this.recvBuf.length < HEADER_SIZE + payloadLen) break;

      const raw = this.recvBuf.subarray(HEADER_SIZE, HEADER_SIZE + payloadLen).toString('utf8');
      const consumed = HEADER_SIZE + payloadLen;
      this.recvBuf = this.recvBuf.subarray(consumed);

      try {
        const parsed = JSON.parse(raw) as unknown;
        if (parsed === null || typeof parsed !== 'object' || Array.isArray(parsed)) {
          throw new Error('ValdiPacket payload must be a JSON object.');
        }
        this.dispatchMessage(parsed as Record<string, unknown>);
      } catch {
        this.failProtocol('ValdiPacket payload contains malformed JSON.');
        break;
      }
    }
  }

  private failProtocol(message: string): void {
    const error = new Error(message);
    this.recvBuf = Buffer.alloc(0);
    this.rejectAllPending(error);
    this.socket.destroy(error);
  }

  private dispatchMessage(msg: Record<string, unknown>): void {
    if (msg['request']) {
      // The device sends us incoming requests (configure handshake, and Messages.ts responses
      // routed back via forward_client_payload).  Auto-respond to all of them.
      const req = msg['request'] as Record<string, unknown>;
      const reqId = req['request_id'] as string;
      const respKey = Object.keys(req).find(k => k !== 'request_id');
      if (respKey) {
        this.socket.write(encodePacket({ response: { [respKey]: {}, request_id: reqId } }));
        if (respKey === 'configure') {
          this.configureData = req['configure'] as Record<string, unknown>;
          if (this.configureReady) {
            const waiter = this.configureReady;
            this.configureReady = null;
            waiter.resolve(req);
          }
        } else if (respKey === 'forward_client_payload') {
          // Device is returning a Messages.ts response routed back to us.
          const fcp = req['forward_client_payload'] as Record<string, unknown> | undefined;
          if (fcp) {
            try {
              const payloadString = fcp['payload_string'];
              if (typeof payloadString !== 'string') {
                throw new TypeError('Valdi debugger inner payload must be a string.');
              }
              const payloadBytes = Buffer.byteLength(payloadString, 'utf8');
              if (payloadBytes > MAX_DAEMON_INNER_PAYLOAD_BYTES) {
                this.failProtocol(
                  `Valdi debugger inner payload exceeds the ${MAX_DAEMON_INNER_PAYLOAD_BYTES}-byte limit.`,
                );
                return;
              }
              const parsed = JSON.parse(payloadString) as unknown;
              if (parsed === null || typeof parsed !== 'object' || Array.isArray(parsed)) {
                throw new Error('Valdi debugger inner payload must be a JSON object.');
              }
              const inner = parsed as Record<string, unknown>;
              const requestId = inner['requestId'];
              if (typeof requestId !== 'string' || requestId.length === 0) {
                throw new TypeError('Valdi debugger inner payload must have a requestId.');
              }
              const msgId = requestId;
              const listener = this.payloadListeners.get(msgId);
              const payloadLimit = listener?.maxPayloadBytes ?? MAX_DAEMON_INNER_PAYLOAD_BYTES;
              if (
                payloadBytes > payloadLimit ||
                (isPerformanceTraceMessageType(inner['type']) && payloadBytes > MAX_DAEMON_TRACE_PAYLOAD_BYTES)
              ) {
                this.failProtocol(
                  `Valdi debugger performance trace payload exceeds the ${MAX_DAEMON_TRACE_PAYLOAD_BYTES}-byte limit.`,
                );
                return;
              }
              if (listener) {
                this.payloadListeners.delete(msgId);
                listener.resolve(inner);
              }
            } catch {
              this.failProtocol('Valdi debugger inner payload is malformed.');
              return;
            }
          }
        }
      }
    } else if (msg['response']) {
      const resp = msg['response'] as Record<string, unknown>;
      const reqId = resp['request_id'] as string;
      const pending = this.pendingRequests.get(reqId);
      if (pending) {
        this.pendingRequests.delete(reqId);
        if (resp['error']) {
          const errMsg = (resp['error'] as Record<string, unknown>)['error_message'] as string;
          pending.reject(new Error(errMsg));
        } else {
          pending.resolve(resp);
        }
      }
    }
    // Note: events from the device (js_debugger_info, new_logs, etc.) are ignored.
  }

  private sendRequest(payload: Record<string, unknown>, timeoutMs = 5_000): Promise<Record<string, unknown>> {
    const reqId = String(++this.reqCounter);
    const envelope = { request: { ...payload, request_id: reqId } };
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pendingRequests.delete(reqId);
        reject(
          new CliError(
            `Valdi daemon did not respond (port ${this.socket.remotePort ?? '?'}).\n` +
              `Is the hot-reloader actually running and connected?`,
          ),
        );
      }, timeoutMs);
      this.pendingRequests.set(reqId, {
        resolve: v => {
          clearTimeout(timer);
          resolve(v);
        },
        reject: e => {
          clearTimeout(timer);
          reject(e);
        },
      });
      this.socket.write(encodePacket(envelope), err => {
        if (err) {
          clearTimeout(timer);
          this.pendingRequests.delete(reqId);
          reject(err);
        }
      });
    });
  }

  private nextMsgId(): string {
    return String(++this.msgCounter);
  }

  // ── Daemon-level calls ────────────────────────────────────────────────────

  /**
   * Wait for the device's initial configure handshake (the device sends its configure
   * request immediately on connect; we respond automatically and this resolves when done).
   */
  configure(): Promise<void> {
    return this.configureWithTimeout(5000);
  }

  configureWithTimeout(timeoutMs: number): Promise<void> {
    if (!Number.isSafeInteger(timeoutMs) || timeoutMs < 1 || timeoutMs > 5000) {
      return Promise.reject(new Error('Valdi daemon configure timeout must be between 1 and 5000 milliseconds.'));
    }
    if (this.configureData) return Promise.resolve();
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.configureReady = null;
        reject(
          new CliError(
            `Valdi daemon did not respond (port ${this.socket.remotePort ?? '?'}).\n` +
              `Is the hot-reloader actually running and connected?`,
          ),
        );
      }, timeoutMs);
      this.configureReady = {
        resolve: () => {
          clearTimeout(timer);
          resolve();
        },
        reject: e => {
          clearTimeout(timer);
          reject(e);
        },
      };
    });
  }

  listConnectedClients(): Promise<DaemonConnectedClient[]> {
    // When connected directly to a device its configure handshake provides all we need.
    // The Companion's list_connected_clients concept doesn't apply in direct mode.
    if (this.configureData) {
      const client: DaemonConnectedClient = {
        client_id: String(DIRECT_CLIENT_ID),
        platform: String(this.configureData['platform'] ?? 'unknown'),
        application_id: String(this.configureData['application_id'] ?? 'unknown'),
      };
      return Promise.resolve([client]);
    }
    return Promise.resolve([]);
  }

  // ── Messages.ts direct-to-device calls ───────────────────────────────────

  private async forwardAndWait(
    _clientId: string,
    msgType: DaemonMsgType,
    body: Record<string, unknown>,
    timeoutMs = 15_000,
  ): Promise<Record<string, unknown>> {
    const msgId = this.nextMsgId();
    const payloadString = JSON.stringify({ type: msgType, requestId: msgId, body });
    const maxPayloadBytes = isPerformanceTraceRequestType(msgType)
      ? MAX_DAEMON_TRACE_PAYLOAD_BYTES
      : MAX_DAEMON_INNER_PAYLOAD_BYTES;
    if (Buffer.byteLength(payloadString, 'utf8') > maxPayloadBytes) {
      throw new Error(`Valdi debugger request payload exceeds the ${maxPayloadBytes}-byte limit.`);
    }

    const resultPromise = new Promise<Record<string, unknown>>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.payloadListeners.delete(msgId);
        reject(new Error('Timeout waiting for device response. Is the app running?'));
      }, timeoutMs);
      this.payloadListeners.set(msgId, {
        maxPayloadBytes,
        resolve: v => {
          clearTimeout(timer);
          resolve(v);
        },
        reject: e => {
          clearTimeout(timer);
          reject(e);
        },
      });
    });

    // Send as an event — the device processes payload_from_client events directly.
    // It routes responses back as forward_client_payload requests (handled in dispatchMessage).
    this.socket.write(
      encodePacket({
        event: {
          payload_from_client: {
            sender_client_id: DIRECT_CLIENT_ID,
            payload_string: payloadString,
          },
        },
      }),
      err => {
        if (err) {
          const listener = this.payloadListeners.get(msgId);
          if (listener) {
            this.payloadListeners.delete(msgId);
            listener.reject(err);
          }
        }
      },
    );

    const response = await resultPromise;
    if (response['requestId'] !== msgId) {
      throw new Error('The Valdi runtime returned a debugger response with a mismatched requestId.');
    }
    if (response['type'] === DaemonMsgType.ERROR_RESPONSE) {
      const body = response['body'];
      const message =
        body !== null && typeof body === 'object' && !Array.isArray(body)
          ? (body as Record<string, unknown>)['message']
          : undefined;
      throw new Error(message === undefined ? 'The Valdi runtime rejected the debugger request.' : String(message));
    }
    if (response['type'] !== -msgType) {
      const message = `The Valdi runtime returned debugger response type ${String(response['type'])}; expected ${-msgType}.`;
      if (msgType === DaemonMsgType.CUSTOM_REQUEST) {
        throw new DaemonProtocolError(message);
      }
      throw new Error(message);
    }
    return response;
  }

  async listContexts(clientId: string): Promise<RemoteContext[]> {
    return await this.listContextsWithTimeout(clientId, 15_000);
  }

  async listContextsWithTimeout(clientId: string, timeoutMs: number): Promise<RemoteContext[]> {
    const resp = await this.forwardAndWait(clientId, DaemonMsgType.LIST_CONTEXTS_REQUEST, {}, timeoutMs);
    return (resp['body'] ?? []) as RemoteContext[];
  }

  async getContextTree(clientId: string, contextId: string, includeComponentData: boolean): Promise<unknown> {
    const resp = await this.forwardAndWait(clientId, DaemonMsgType.GET_CONTEXT_TREE_REQUEST, {
      id: contextId,
      includeComponentData,
    });
    return resp['body'];
  }

  async takeSnapshot(clientId: string, elementId: string, contextId: string): Promise<string> {
    const resp = await this.forwardAndWait(clientId, DaemonMsgType.TAKE_ELEMENT_SNAPSHOT_REQUEST, {
      elementId: parseInt(elementId, 10),
      contextId,
    });
    return resp['body'] as string;
  }

  async dumpHeap(clientId: string, performGC = false): Promise<unknown> {
    const resp = await this.forwardAndWait(clientId, DaemonMsgType.DUMP_HEAP_REQUEST, { performGC }, 60_000);
    return resp['body'];
  }

  async performanceTraceStatus(
    clientId: string,
    body: PerformanceTraceStatusRequestBody,
    timeoutMs: number,
  ): Promise<DaemonPerformanceTraceStatusBody> {
    const resp = await this.forwardAndWait(clientId, DaemonMsgType.PERFORMANCE_TRACE_STATUS_REQUEST, body, timeoutMs);
    return validatePerformanceTraceStatusBody(resp['body']);
  }

  async performanceTraceStart(
    clientId: string,
    body: PerformanceTraceStartRequestBody,
    timeoutMs: number,
  ): Promise<DaemonPerformanceTraceStatusBody> {
    const resp = await this.forwardAndWait(clientId, DaemonMsgType.PERFORMANCE_TRACE_START_REQUEST, body, timeoutMs);
    return validatePerformanceTraceStatusBody(resp['body']);
  }

  async performanceTraceStop(
    clientId: string,
    body: PerformanceTraceStopRequestBody,
    timeoutMs: number,
  ): Promise<DaemonPerformanceTraceStopBody> {
    const resp = await this.forwardAndWait(clientId, DaemonMsgType.PERFORMANCE_TRACE_STOP_REQUEST, body, timeoutMs);
    return validatePerformanceTraceStopBody(resp['body']);
  }

  // eslint-disable-next-line @typescript-eslint/member-ordering -- follows the existing public daemon operation grouping
  async customRequest(
    clientId: string,
    identifier: string,
    data: Record<string, unknown>,
    timeoutMs = 15_000,
  ): Promise<CustomRequestResponse> {
    if (typeof identifier !== 'string' || identifier.trim().length === 0 || identifier.length > 128) {
      throw new DaemonProtocolError('Custom debugger request identifiers must contain 1 to 128 characters.');
    }
    if (typeof data !== 'object' || data === null || Array.isArray(data)) {
      throw new DaemonProtocolError('Custom debugger request data must be an object.');
    }
    let serializedData: string;
    try {
      serializedData = JSON.stringify(data);
    } catch {
      throw new DaemonProtocolError('Custom debugger request data must be JSON serializable.');
    }
    if (typeof serializedData !== 'string') {
      throw new DaemonProtocolError('Custom debugger request data must be JSON serializable.');
    }
    if (Buffer.byteLength(serializedData, 'utf8') > 128 * 1024) {
      throw new DaemonProtocolError('Custom debugger request data exceeds 128 KiB.');
    }
    const resp = await this.forwardAndWait(clientId, DaemonMsgType.CUSTOM_REQUEST, { identifier, data }, timeoutMs);
    const body = resp['body'];
    if (typeof body !== 'object' || body === null || Array.isArray(body)) {
      throw new DaemonProtocolError('The Valdi runtime returned a non-object custom debugger response body.');
    }
    const response = body as Record<string, unknown>;
    if (typeof response['handled'] !== 'boolean') {
      throw new DaemonProtocolError('The Valdi runtime custom debugger response requires a boolean handled field.');
    }
    const responseData = response['data'];
    if (
      response['handled'] === true &&
      (typeof responseData !== 'object' || responseData === null || Array.isArray(responseData))
    ) {
      throw new DaemonProtocolError('Handled custom debugger responses require object data.');
    }
    if (
      responseData !== undefined &&
      (typeof responseData !== 'object' || responseData === null || Array.isArray(responseData))
    ) {
      throw new DaemonProtocolError('Custom debugger response data must be an object when present.');
    }
    if (responseData !== undefined) {
      let serializedResponseData: string;
      try {
        serializedResponseData = JSON.stringify(responseData);
      } catch {
        throw new DaemonProtocolError('Custom debugger response data must be JSON serializable.');
      }
      if (typeof serializedResponseData !== 'string') {
        throw new DaemonProtocolError('Custom debugger response data must be JSON serializable.');
      }
      if (Buffer.byteLength(serializedResponseData, 'utf8') > 128 * 1024) {
        throw new DaemonProtocolError('Custom debugger response data exceeds 128 KiB.');
      }
    }
    return {
      handled: response['handled'],
      ...(responseData === undefined ? {} : { data: responseData as Record<string, unknown> }),
    };
  }

  close(): void {
    this.socket.destroy();
  }
}

// ─── Factory ─────────────────────────────────────────────────────────────────

function adbForwardKey(endpoint: DaemonConnectionEndpoint): string {
  return `${endpoint.deviceId ?? ''}\0${endpoint.port.toString()}`;
}

async function tryAdbForward(endpoint: DaemonConnectionEndpoint): Promise<void> {
  if (endpoint.deviceId !== undefined && !/^[\w.:-]{1,128}$/.test(endpoint.deviceId)) {
    throw new CliError('Android device serial contains unsupported characters.');
  }
  const key = adbForwardKey(endpoint);
  const lastForwardedAt = adbForwardedAtByEndpoint.get(key);
  if (lastForwardedAt !== undefined && Date.now() - lastForwardedAt < ADB_FORWARD_REFRESH_INTERVAL_MS) return;

  const pendingRefresh = adbForwardRefreshByEndpoint.get(key);
  if (pendingRefresh) {
    await pendingRefresh;
    return;
  }

  const refresh = refreshAdbForward(endpoint, key);
  adbForwardRefreshByEndpoint.set(key, refresh);
  try {
    await refresh;
  } finally {
    if (adbForwardRefreshByEndpoint.get(key) === refresh) adbForwardRefreshByEndpoint.delete(key);
  }
}

async function refreshAdbForward(endpoint: DaemonConnectionEndpoint, key: string): Promise<void> {
  try {
    const deviceSelector = endpoint.deviceId === undefined ? '' : `-s ${endpoint.deviceId} `;
    await runCliCommand(`adb ${deviceSelector}forward tcp:${endpoint.port} tcp:${endpoint.port}`);
    adbForwardedAtByEndpoint.set(key, Date.now());
  } catch {
    // ADB is optional for standalone targets; retry the next time a mobile connection is requested.
  }
}

export async function connectToDaemon(target?: number | DaemonConnectionEndpoint): Promise<DaemonConnection> {
  const resolvedTarget = target ?? DEFAULT_PORT;
  const endpoint: DaemonConnectionEndpoint =
    typeof resolvedTarget === 'number'
      ? { autoForward: resolvedTarget !== STANDALONE_PORT, port: resolvedTarget }
      : resolvedTarget;
  const port = endpoint.port;
  // Only set up adb forwarding for mobile ports — standalone macOS apps listen
  // directly on localhost and companion-owned device tunnels must remain intact.
  if (endpoint.autoForward && port !== STANDALONE_PORT) {
    await tryAdbForward(endpoint);
  }
  return new Promise((resolve, reject) => {
    const socket = net.createConnection({ port, host: '127.0.0.1' });

    const timer = setTimeout(() => {
      socket.destroy();
      reject(new Error(`Timeout connecting to Valdi daemon on port ${port}. Is the hot-reloader running?`));
    }, 5_000);

    socket.once('connect', () => {
      clearTimeout(timer);
      resolve(new DaemonConnection(socket));
    });

    socket.once('error', (err: NodeJS.ErrnoException) => {
      clearTimeout(timer);
      if (err.code === 'ECONNREFUSED') {
        reject(
          new CliError(
            `Valdi daemon not running on port ${port}.\n` +
              `Start the hot-reloader (valdi hotreload) or pass --port to use a different port.`,
          ),
        );
      } else {
        reject(err);
      }
    });
  });
}

// ─── Device / context selection helpers ──────────────────────────────────────

/**
 * Resolve which context to target.
 * Priority: explicit contextId argument → auto (single context) → prompt.
 */
export async function resolveContextId(
  conn: DaemonConnection,
  clientId: string,
  contextIdOverride?: string,
): Promise<string> {
  const contexts = await conn.listContexts(clientId);

  if (contexts.length === 0) {
    throw new CliError('No contexts found on the connected device.');
  }

  if (contextIdOverride) {
    if (!contexts.some(c => c.id === contextIdOverride)) {
      throw new CliError(
        `Context "${contextIdOverride}" not found. Run "valdi inspect contexts" to see available contexts.`,
      );
    }
    return contextIdOverride;
  }

  if (contexts.length === 1) {
    return contexts[0]!.id;
  }

  // Multiple contexts — prompt
  return getUserChoice(
    contexts.map(c => ({
      name: `${c.rootComponentName}  [${c.id}]`,
      value: c.id,
    })),
    'Multiple contexts found. Select one:',
  );
}

/**
 * Resolve which connected client to target.
 * Priority: explicit --client flag → saved config → auto (single device) → prompt.
 */
export async function resolveClientId(conn: DaemonConnection, clientIdOverride?: string): Promise<string> {
  const clients = await conn.listConnectedClients();

  if (clients.length === 0) {
    throw new CliError(
      'No devices connected to the Valdi daemon.\n' +
        'Make sure the Valdi app is running and connected to the hot-reloader.',
    );
  }

  if (clientIdOverride) {
    if (!clients.some(c => c.client_id === clientIdOverride)) {
      throw new CliError(
        `Client "${clientIdOverride}" not found. Run "valdi inspect devices" to see connected clients.`,
      );
    }
    return clientIdOverride;
  }

  const config = loadInspectConfig();
  if (config.selectedClientId && clients.some(c => c.client_id === config.selectedClientId)) {
    return config.selectedClientId;
  }

  if (clients.length === 1) {
    return clients[0]!.client_id;
  }

  // Multiple devices — prompt
  return getUserChoice(
    clients.map(c => ({
      name: `${c.application_id} (${c.platform})  [${c.client_id}]`,
      value: c.client_id,
    })),
    'Multiple devices connected. Select one:',
  );
}
