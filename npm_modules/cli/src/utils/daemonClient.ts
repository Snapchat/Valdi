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
  LIST_CONTEXTS_REQUEST = 2,
  LIST_CONTEXTS_RESPONSE = -2,
  GET_CONTEXT_TREE_REQUEST = 3,
  GET_CONTEXT_TREE_RESPONSE = -3,
  TAKE_ELEMENT_SNAPSHOT_REQUEST = 4,
  TAKE_ELEMENT_SNAPSHOT_RESPONSE = -4,
  DUMP_HEAP_REQUEST = 5,
  DUMP_HEAP_RESPONSE = -5,
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

// ─── Packet encoding ─────────────────────────────────────────────────────────

const MAGIC = Buffer.from([0x33, 0xc6, 0x00, 0x01]);
const HEADER_SIZE = 8; // 4 magic + 4 uint32LE length

function encodePacket(json: object): Buffer {
  const payload = Buffer.from(JSON.stringify(json), 'utf8');
  const header = Buffer.alloc(8);
  MAGIC.copy(header, 0);
  header.writeUInt32LE(payload.length, 4);
  return Buffer.concat([header, payload]);
}

// ─── DaemonConnection ────────────────────────────────────────────────────────

// In the direct-to-device protocol we are "client 1" (non-zero required by device JS check).
const DIRECT_CLIENT_ID = 1;

interface PendingRequest {
  resolve: (value: Record<string, unknown>) => void;
  reject: (err: Error) => void;
}

export class DaemonConnection {
  private socket: net.Socket;
  private recvBuf: Buffer = Buffer.alloc(0);
  private recvChunks: Buffer[] = [];
  private recvLen = 0;
  private reqCounter = 0;
  private msgCounter = 0;
  private pendingRequests = new Map<string, PendingRequest>();
  private payloadListeners = new Map<string, PendingRequest>();
  // Resolved when the device sends its initial configure request (session ready)
  private configureReady: PendingRequest | null = null;
  // Saved from the device's configure handshake
  private configureData: Record<string, unknown> | null = null;

  constructor(socket: net.Socket) {
    this.socket = socket;
    socket.on('data', (data: Buffer) => this.onData(data));
    socket.on('close', () => this.rejectAllPending(new Error('Connection closed unexpectedly')));
    socket.on('error', (err) => this.rejectAllPending(err));
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
    this.recvChunks.push(chunk);
    this.recvLen += chunk.length;
    this.drainBuffer();
  }

  private drainBuffer(): void {
    for (;;) {
      if (this.recvLen < HEADER_SIZE) break;

      // Materialise the flat buffer only when we actually have enough data to inspect.
      if (this.recvBuf.length < this.recvLen) {
        this.recvBuf = Buffer.concat(this.recvChunks, this.recvLen);
        this.recvChunks = [this.recvBuf];
      }

      if (
        this.recvBuf[0] !== 0x33 ||
        this.recvBuf[1] !== 0xc6 ||
        this.recvBuf[2] !== 0x00 ||
        this.recvBuf[3] !== 0x01
      ) {
        this.socket.destroy(new Error('ValdiPacket: bad magic'));
        break;
      }

      const payloadLen = this.recvBuf.readUInt32LE(4);
      if (this.recvLen < HEADER_SIZE + payloadLen) break;

      const raw = this.recvBuf.subarray(HEADER_SIZE, HEADER_SIZE + payloadLen).toString('utf8');
      const consumed = HEADER_SIZE + payloadLen;
      this.recvBuf = this.recvBuf.subarray(consumed);
      this.recvLen -= consumed;
      this.recvChunks = this.recvLen > 0 ? [this.recvBuf] : [];

      try {
        this.dispatchMessage(JSON.parse(raw) as Record<string, unknown>);
      } catch {
        // ignore malformed packets
      }
    }
  }

  private dispatchMessage(msg: Record<string, unknown>): void {
    if (msg['request']) {
      // The device sends us incoming requests (configure handshake, and Messages.ts responses
      // routed back via forward_client_payload).  Auto-respond to all of them.
      const req = msg['request'] as Record<string, unknown>;
      const reqId = req['request_id'] as string;
      const respKey = Object.keys(req).find((k) => k !== 'request_id');
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
              const inner = JSON.parse(fcp['payload_string'] as string) as Record<string, unknown>;
              const msgId = String(inner['requestId']);
              const listener = this.payloadListeners.get(msgId);
              if (listener) {
                this.payloadListeners.delete(msgId);
                listener.resolve(inner);
              }
            } catch {
              // ignore malformed inner payload
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

  private sendRequest(
    payload: Record<string, unknown>,
    timeoutMs = 5_000,
  ): Promise<Record<string, unknown>> {
    const reqId = String(++this.reqCounter);
    const envelope = { request: { ...payload, request_id: reqId } };
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pendingRequests.delete(reqId);
        reject(new CliError(
          `Valdi daemon did not respond (port ${(this.socket.remotePort ?? '?')}).\n` +
          `Is the hot-reloader actually running and connected?`,
        ));
      }, timeoutMs);
      this.pendingRequests.set(reqId, {
        resolve: (v) => { clearTimeout(timer); resolve(v); },
        reject: (e) => { clearTimeout(timer); reject(e); },
      });
      this.socket.write(encodePacket(envelope), (err) => {
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
    if (this.configureData) return Promise.resolve();
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.configureReady = null;
        reject(new CliError(
          `Valdi daemon did not respond (port ${this.socket.remotePort ?? '?'}).\n` +
          `Is the hot-reloader actually running and connected?`,
        ));
      }, 5_000);
      this.configureReady = {
        resolve: () => { clearTimeout(timer); resolve(); },
        reject: (e) => { clearTimeout(timer); reject(e); },
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

    const resultPromise = new Promise<Record<string, unknown>>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.payloadListeners.delete(msgId);
        reject(new Error('Timeout waiting for device response. Is the app running?'));
      }, timeoutMs);
      this.payloadListeners.set(msgId, {
        resolve: (v) => { clearTimeout(timer); resolve(v); },
        reject: (e) => { clearTimeout(timer); reject(e); },
      });
    });

    // Send as an event — the device processes payload_from_client events directly.
    // It routes responses back as forward_client_payload requests (handled in dispatchMessage).
    this.socket.write(encodePacket({
      event: {
        payload_from_client: {
          sender_client_id: DIRECT_CLIENT_ID,
          payload_string: payloadString,
        },
      },
    }), (err) => {
      if (err) {
        const listener = this.payloadListeners.get(msgId);
        if (listener) {
          this.payloadListeners.delete(msgId);
          listener.reject(err);
        }
      }
    });

    return resultPromise;
  }

  async listContexts(clientId: string): Promise<RemoteContext[]> {
    const resp = await this.forwardAndWait(clientId, DaemonMsgType.LIST_CONTEXTS_REQUEST, {});
    return (resp['body'] ?? []) as RemoteContext[];
  }

  async getContextTree(clientId: string, contextId: string): Promise<unknown> {
    const resp = await this.forwardAndWait(clientId, DaemonMsgType.GET_CONTEXT_TREE_REQUEST, { id: contextId });
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

  close(): void {
    this.socket.destroy();
  }
}

// ─── Factory ─────────────────────────────────────────────────────────────────

async function tryAdbForward(port: number): Promise<void> {
  try {
    await runCliCommand(`adb forward tcp:${port} tcp:${port}`);
  } catch {
    // adb may not be installed or no Android devices attached — non-fatal
  }
}

export async function connectToDaemon(port: number = DEFAULT_PORT): Promise<DaemonConnection> {
  // Only set up adb forwarding for mobile ports — standalone macOS apps listen
  // directly on localhost and adb forward would shadow them.
  if (port !== STANDALONE_PORT) {
    await tryAdbForward(port);
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
        reject(new CliError(
          `Valdi daemon not running on port ${port}.\n` +
          `Start the hot-reloader (valdi hotreload) or pass --port to use a different port.`,
        ));
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
    if (!contexts.some((c) => c.id === contextIdOverride)) {
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
    contexts.map((c) => ({
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
export async function resolveClientId(
  conn: DaemonConnection,
  clientIdOverride?: string,
): Promise<string> {
  const clients = await conn.listConnectedClients();

  if (clients.length === 0) {
    throw new CliError(
      'No devices connected to the Valdi daemon.\n' +
      'Make sure the Valdi app is running and connected to the hot-reloader.',
    );
  }

  if (clientIdOverride) {
    if (!clients.some((c) => c.client_id === clientIdOverride)) {
      throw new CliError(
        `Client "${clientIdOverride}" not found. Run "valdi inspect devices" to see connected clients.`,
      );
    }
    return clientIdOverride;
  }

  const config = loadInspectConfig();
  if (config.selectedClientId && clients.some((c) => c.client_id === config.selectedClientId)) {
    return config.selectedClientId;
  }

  if (clients.length === 1) {
    return clients[0]!.client_id;
  }

  // Multiple devices — prompt
  return getUserChoice(
    clients.map((c) => ({
      name: `${c.application_id} (${c.platform})  [${c.client_id}]`,
      value: c.client_id,
    })),
    'Multiple devices connected. Select one:',
  );
}
