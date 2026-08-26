import { watch as watchFileSystem } from 'node:fs';
import type { FSWatcher, Stats } from 'node:fs';
import fs from 'node:fs/promises';
import type { IncomingMessage, Server, ServerResponse } from 'node:http';
import http from 'node:http';
import net from 'node:net';
import path from 'node:path';
import { TextDecoder } from 'node:util';
import {
  type DaemonConnectedClient,
  type DaemonConnection,
  MOBILE_PORT,
  type RemoteContext,
  STANDALONE_PORT,
  connectToDaemon,
} from '../utils/daemonClient';
import { getUserConfig, resolveFilePath } from '../utils/fileUtils';
import { type CpuProfile, HERMES_PORT, HermesConnection, listHermesDevices } from '../utils/hermesClient';
import { isLoopbackHost, normalizedHostname } from '../utils/loopbackHost';
import {
  evaluateOwlApplicationExpression,
  matchesOwlApplicationUrl,
  readOwlDebuggerSnapshot,
} from '../utils/owlCdpClient';
import { DebuggerInputType, sendDebuggerInput, validateDebuggerInputRequest } from './inputClient';

const DEFAULT_HOST = process.env['VALDI_DEBUGGER_HOST'] || '127.0.0.1';
const DEFAULT_PORT = Number.parseInt(process.env['VALDI_DEBUGGER_PORT'] || '8765', 10);
const DEFAULT_CHROMIUM_DEBUGGING_PORT = Number.parseInt(process.env['VALDI_CHROMIUM_DEBUGGING_PORT'] || '9222', 10);
const HOT_RELOAD_PROXY_PORT = Number.parseInt(process.env['VALDI_HOT_RELOAD_PROXY_PORT'] || '9010', 10);
const PORT_SEARCH_LIMIT = 50;
const MAX_RUNTIME_LOG_READ_BYTES = 1024 * 1024;
const FATAL_JSON_UTF8_DECODER = new TextDecoder('utf8', { fatal: true });
const WEB_PREVIEW_NONCE_PATTERN = /^[\w-]{16,128}$/;
const MAX_DEBUGGER_TREE_NODES = 25_000;
const MAX_DEBUGGER_PROJECTION_VALUES = 250_000;
const MAX_DEBUGGER_PROJECTION_DEPTH = 64;
const MAX_DEBUGGER_PROJECTION_STRING_LENGTH = 50_000;
const DEFAULT_TRACE_CAPTURE_DURATION_MS = 5000;
const MIN_TRACE_CAPTURE_DURATION_MS = 100;
const MAX_TRACE_CAPTURE_DURATION_MS = 15_000;
export const MAX_TRACE_EVENT_COUNT = 10_000;
const MAX_TRACE_NAME_BYTES = 2048;
const MAX_TRACE_THREAD_METADATA_COUNT = 256;
export const MAX_TRACE_HTTP_RESPONSE_BYTES = 4 * 1024 * 1024;
const MAX_TRACE_HTTP_STRING_BYTES = 64 * 1024;
const TRACE_DAEMON_TIMEOUT_MS = 30_000;
const PERFETTO_PROCESS_ID = 1;
const PERFETTO_PROCESS_NAME = 'Valdi';
const PERFETTO_TRACE_CATEGORY = 'valdi';
const PROCESS_WIDE_CAPTURE_SCOPE = 'process-wide';
const TRACE_CAPTURE_TARGET_STRING_KEYS = [
  'id',
  'name',
  'platform',
  'transport',
  'state',
  'clientId',
  'contextId',
  'applicationId',
] as const;

const MIME_TYPES: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.svg': 'image/svg+xml',
};

interface LogEntry {
  time: string;
  level: string;
  source: string;
  message: string;
  timestamp?: string;
}

interface LogFile {
  path: string;
  stat: Stats;
}

interface ParsedLogRecord {
  entry: LogEntry;
  offset: number;
}

interface RuntimeLogTail {
  content: string;
  startOffset: number;
}

interface ClientWithContexts extends DaemonConnectedClient {
  contexts: RemoteContext[];
  contextError: string | null;
}

export interface RecordedTrace {
  trace: string;
  startMicros: number;
  endMicros: number;
  threadId: number;
}

export interface PerfettoCaptureMetadata {
  captureScope: string;
  captureTargetContextId: unknown;
  captureTargetName: unknown;
  droppedTraceEventCount: number;
}

export interface PerfettoTraceEvent {
  name: string;
  cat?: string;
  ph: string;
  pid: number;
  tid?: number;
  ts?: number;
  dur?: number;
  s?: string;
  args?: Record<string, unknown>;
}

export interface TraceComponentSummary {
  name: string;
  count: number;
  durationMs: number;
}

export interface TraceViewModelTriggerSummary {
  name: string;
  count: number;
}

export interface RendererTraceSummary {
  captureScope: string;
  traceCount: number;
  durationTraceCount: number;
  instantTraceCount: number;
  topComponents: TraceComponentSummary[];
  topViewModelTriggers: TraceViewModelTriggerSummary[];
}

export interface PerfettoTracePayload {
  displayTimeUnit: string;
  metadata: PerfettoCaptureMetadata;
  traceEvents: PerfettoTraceEvent[];
}

export enum PerformanceTraceAction {
  Status,
  Start,
  Stop,
}

export interface PerformanceTraceCaptureOptions {
  durationMs: number;
  rendererTracing: boolean;
}

export interface PerformanceTraceCaptureDependencies {
  send(action: PerformanceTraceAction, data: Record<string, unknown>): Promise<Record<string, unknown>>;
  wait(durationMs: number): Promise<void>;
}

interface ActiveProfileSession {
  conn: HermesConnection;
  port: number;
  contextId: string;
  contextTitle: string;
  startedAtMs: number;
  startedAtEpochMs: number;
}

interface DebuggerActionRecord {
  action: string;
  params: Record<string, unknown>;
  source: string;
  time: string;
}

interface DebuggerUiState {
  revision: number;
  selectedNodeId: string | null;
  selectedTargetId: string | null;
  activeSection: string;
  activeTab: string;
  overlayMode: string;
  autoRefresh: boolean;
  followLatestTarget: boolean;
  port: number;
  updatedAt: string;
  lastAction: DebuggerActionRecord | null;
}

interface DebuggerServerOptions {
  host?: string;
  port?: number;
  strictPort?: boolean;
  assetRoot?: string;
  logsDirectory?: string;
  webPreviewUrl?: string;
  chromiumDebuggingPort?: number;
}

interface WebPreviewDebuggerTarget {
  applicationUrl: string;
  debuggingPort: number;
  id: string;
  sessionId: string;
}

class ApiRequestError extends Error {
  readonly statusCode: number;

  constructor(statusCode: number, message: string) {
    super(message);
    this.statusCode = statusCode;
  }
}

export interface DebuggerServerInfo {
  server: Server;
  close: () => Promise<void>;
  host: string;
  port: number;
  url: string;
  requestedPort: number;
  portWasAutoSelected: boolean;
}

const devEventClients = new Set<ServerResponse>();
const debuggerEventClients = new Set<ServerResponse>();
const eventStreamClosers = new Set<() => void>();
const assetWatchers = new Set<FSWatcher>();
const debuggerActions = [
  'attach',
  'detach',
  'refreshTargets',
  'refreshSnapshot',
  'selectTarget',
  'selectNode',
  'setActiveSection',
  'setActiveTab',
  'setOverlayMode',
  'setAutoRefresh',
  'setPort',
  'clearLogs',
  'captureElementSnapshot',
  'dumpHeap',
  'startRendererTrace',
  'stopRendererTrace',
  'captureRendererTrace',
  'refreshHermesContexts',
  'startCpuProfile',
  'stopCpuProfile',
  'captureCpuProfile',
];
let devRevision = 0;
let debuggerEventRevision = 0;
let devReloadTimer: NodeJS.Timeout | null = null;
let activeHost = DEFAULT_HOST;
let assetRoot = getDefaultAssetRoot();
let activeLogsDirectory: string | null = null;
let activeWebPreviewTarget: WebPreviewDebuggerTarget | null = null;
let activeProfileSession: ActiveProfileSession | null = null;
let profileTransitionInProgress = false;
let traceTransitionInProgress = false;
const debuggerUiState = createDebuggerUiState();

function getDefaultAssetRoot(): string {
  // The published CLI is emitted as CommonJS, so __dirname is the reliable package-relative anchor.
  // eslint-disable-next-line unicorn/prefer-module
  return path.resolve(__dirname, '..', '..', 'debugger');
}

function hostForUrl(host: string): string {
  return net.isIP(normalizedHostname(host)) === 6 ? `[${normalizedHostname(host)}]` : host;
}

function parseHttpUrl(value: string): URL | null {
  try {
    return new URL(value);
  } catch {
    return null;
  }
}

function hasExpectedLoopbackAuthority(url: URL, port: number): boolean {
  const effectivePort = url.port || (url.protocol === 'http:' ? '80' : '443');
  return (
    url.protocol === 'http:' &&
    isLoopbackHost(url.hostname) &&
    effectivePort === String(port) &&
    !url.username &&
    !url.password
  );
}

function isAllowedRequestHost(hostHeader: string | undefined, port: number): boolean {
  if (!hostHeader) return false;
  const parsed = parseHttpUrl(`http://${hostHeader}`);
  return parsed !== null && hasExpectedLoopbackAuthority(parsed, port);
}

function isAllowedRequestOrigin(
  originHeader: string | undefined,
  hostHeader: string | undefined,
  port: number,
): boolean {
  if (originHeader === undefined) return true;
  const parsed = parseHttpUrl(originHeader);
  const requestAuthority = hostHeader ? parseHttpUrl(`http://${hostHeader}`) : null;
  return (
    parsed !== null &&
    requestAuthority !== null &&
    hasExpectedLoopbackAuthority(parsed, port) &&
    hasExpectedLoopbackAuthority(requestAuthority, port) &&
    normalizedHostname(parsed.hostname) === normalizedHostname(requestAuthority.hostname) &&
    parsed.pathname === '/' &&
    !parsed.search &&
    !parsed.hash
  );
}

function isAllowedApiFetchSite(fetchSiteHeader: string | undefined): boolean {
  return fetchSiteHeader === undefined || fetchSiteHeader === 'same-origin' || fetchSiteHeader === 'none';
}

function probeTcpPort(port: number, timeoutMs: number): Promise<boolean> {
  return new Promise(resolve => {
    const socket = connectToTcpPort(port);
    let settled = false;
    const done = (connected: boolean) => {
      if (settled) return;
      settled = true;
      socket.destroy();
      resolve(connected);
    };
    socket.setTimeout(timeoutMs, () => done(false));
    socket.once('connect', () => done(true));
    socket.once('error', () => done(false));
  });
}

function connectToTcpPort(port: number): net.Socket {
  return net.connect({ host: activeHost, port });
}

function portName(port: number): string {
  if (port === STANDALONE_PORT) return 'standalone';
  if (port === MOBILE_PORT) return 'mobile';
  return 'custom';
}

function truncateStringForJson(value: string, maximumBytes: number): string {
  if (Buffer.byteLength(JSON.stringify(value), 'utf8') <= maximumBytes) return value;
  const suffix = '…';
  let minimumLength = 0;
  let maximumLength = value.length;
  let truncated = suffix;
  while (minimumLength <= maximumLength) {
    const candidateLength = Math.floor((minimumLength + maximumLength) / 2);
    const candidate = `${value.slice(0, candidateLength)}${suffix}`;
    if (Buffer.byteLength(JSON.stringify(candidate), 'utf8') <= maximumBytes) {
      truncated = candidate;
      minimumLength = candidateLength + 1;
    } else {
      maximumLength = candidateLength - 1;
    }
  }
  return truncated;
}

function errorPayload(error: unknown): { error: string } {
  return {
    error: truncateStringForJson(error instanceof Error ? error.message : String(error), MAX_TRACE_HTTP_STRING_BYTES),
  };
}

function isFileSystemError(error: unknown): error is NodeJS.ErrnoException {
  if (!(error instanceof Error)) return false;
  const fileSystemError = error as NodeJS.ErrnoException;
  return (
    typeof fileSystemError.code === 'string' &&
    typeof fileSystemError.path === 'string' &&
    typeof fileSystemError.syscall === 'string'
  );
}

function clientErrorPayload(error: unknown): { error: string } {
  if (!isFileSystemError(error)) return errorPayload(error);
  console.warn(`Debugger filesystem error: ${errorPayload(error).error}`);
  return { error: 'A local filesystem operation failed. See the debugger server output for details.' };
}

function isValidSnapshotBase64(value: string): boolean {
  if (!value || value.length % 4 === 1) return false;
  const firstPaddingIndex = value.indexOf('=');
  const dataLength = firstPaddingIndex === -1 ? value.length : firstPaddingIndex;
  const paddingLength = value.length - dataLength;
  if (paddingLength > 2 || (paddingLength > 0 && value.length % 4 !== 0)) return false;

  for (let index = 0; index < dataLength; index += 1) {
    const code = value.codePointAt(index) ?? 0;
    const valid =
      (code >= 65 && code <= 90) ||
      (code >= 97 && code <= 122) ||
      (code >= 48 && code <= 57) ||
      code === 43 ||
      code === 47;
    if (!valid) return false;
  }
  for (let index = dataLength; index < value.length; index += 1) {
    if (value[index] !== '=') return false;
  }
  return true;
}

interface DebuggerProjectionState {
  complete: boolean;
  seen: Map<object, string>;
  valueCount: number;
}

interface DebuggerTreeProjectionNode {
  childIndexes: number[];
  data: Record<string, unknown>;
  depth: number;
  index: number;
  parentIndex: number | null;
  sourceChildIndex: number | null;
}

interface DebuggerTreeProjection {
  complete: boolean;
  format: 'valdi-debugger-tree-v1';
  nodeCount: number;
  nodes: DebuggerTreeProjectionNode[];
  rootIndex: number;
  truncations: Array<Record<string, string>>;
}

type DebuggerProjectionContainer = Record<string, unknown> | unknown[];

interface DebuggerProjectionEntry {
  accessor: boolean;
  childPath: string;
  key: string;
  value: unknown;
}

interface DebuggerProjectionFrame {
  depth: number;
  entries: DebuggerProjectionEntry[];
  index: number;
  path: string;
  sparse: boolean;
  target: DebuggerProjectionContainer;
}

function createDebuggerJsonRecord<Value>(): Record<string, Value> {
  return Object.create(null) as Record<string, Value>;
}

function setDebuggerJsonProperty<Value>(target: Record<string, Value>, key: string, value: Value): void {
  Object.defineProperty(target, key, {
    configurable: true,
    enumerable: true,
    value,
    writable: true,
  });
}

function debuggerProjectionTruncation(reason: string, path: string): Record<string, string> {
  const marker = createDebuggerJsonRecord<string>();
  setDebuggerJsonProperty(marker, '$at', path);
  setDebuggerJsonProperty(marker, '$truncated', reason);
  return marker;
}

function markDebuggerProjectionTruncated(target: DebuggerProjectionContainer, reason: string, path: string): void {
  const marker = debuggerProjectionTruncation(reason, path);
  if (Array.isArray(target)) {
    target.push(marker);
  } else if (target['$type'] === 'array' && Array.isArray(target['$entries'])) {
    target['$entries'].push(marker);
  } else {
    setDebuggerJsonProperty(target, '$at', marker['$at']);
    setDebuggerJsonProperty(target, '$truncated', marker['$truncated']);
  }
}

function isDebuggerArrayIndex(key: string): boolean {
  const index = Number(key);
  return Number.isInteger(index) && index >= 0 && index < 4_294_967_295 && String(index) === key;
}

function debuggerPrimitiveProjection(value: unknown, state: DebuggerProjectionState): unknown {
  if (typeof value === 'string') {
    if (value.length <= MAX_DEBUGGER_PROJECTION_STRING_LENGTH) return value;
    state.complete = false;
    const suffix = '…[truncated]';
    return `${value.slice(0, MAX_DEBUGGER_PROJECTION_STRING_LENGTH - suffix.length)}${suffix}`;
  }
  if (value === null || typeof value === 'boolean') return value;
  if (typeof value === 'number') return Number.isFinite(value) ? value : String(value);
  if (typeof value === 'bigint') return `${value}n`;
  if (value === undefined) return '[undefined]';
  if (typeof value === 'function') return `[Function ${value.name || 'anonymous'}]`;
  if (typeof value === 'symbol') return String(value);
  return undefined;
}

function debuggerOwnEntries(
  source: object,
  path: string,
  state: DebuggerProjectionState,
): {
  descriptors?: PropertyDescriptorMap;
  entries: DebuggerProjectionEntry[];
  inspectionError: Record<string, string> | null;
} {
  let descriptors: PropertyDescriptorMap;
  try {
    descriptors = Object.getOwnPropertyDescriptors(source);
  } catch {
    state.complete = false;
    return { entries: [], inspectionError: debuggerProjectionTruncation('unavailable-properties', path) };
  }

  const entries: DebuggerProjectionEntry[] = [];
  for (const key of Object.keys(descriptors)) {
    const descriptor = descriptors[key];
    if (!descriptor?.enumerable) continue;
    if (entries.length >= MAX_DEBUGGER_PROJECTION_VALUES) {
      state.complete = false;
      break;
    }
    const childPath = isDebuggerArrayIndex(key) ? `${path}[${key}]` : `${path}.${key}`;
    if (!Object.prototype.hasOwnProperty.call(descriptor, 'value')) {
      state.complete = false;
      entries.push({
        accessor: true,
        childPath,
        key,
        value: debuggerProjectionTruncation('accessor', childPath),
      });
      continue;
    }
    entries.push({ accessor: false, childPath, key, value: descriptor.value });
  }
  return { descriptors, entries, inspectionError: null };
}

function debuggerProjectionFrame(
  source: object,
  targetPath: string,
  state: DebuggerProjectionState,
): Omit<DebuggerProjectionFrame, 'depth' | 'index' | 'path'> {
  const inspection = debuggerOwnEntries(source, targetPath, state);
  if (inspection.inspectionError) {
    return { entries: [], sparse: false, target: inspection.inspectionError };
  }
  if (!Array.isArray(source)) {
    return { entries: inspection.entries, sparse: false, target: createDebuggerJsonRecord<unknown>() };
  }

  const lengthDescriptor = inspection.descriptors?.['length'];
  const length =
    lengthDescriptor && Object.prototype.hasOwnProperty.call(lengthDescriptor, 'value')
      ? Number(lengthDescriptor.value)
      : 0;
  let expectedIndex = 0;
  let dense = Number.isSafeInteger(length) && length >= 0;
  for (const key of Object.keys(inspection.descriptors ?? {})) {
    const descriptor = inspection.descriptors?.[key];
    if (!descriptor?.enumerable) continue;
    if (
      !Object.prototype.hasOwnProperty.call(descriptor, 'value') ||
      !isDebuggerArrayIndex(key) ||
      Number(key) !== expectedIndex
    ) {
      dense = false;
      break;
    }
    expectedIndex += 1;
  }
  if (expectedIndex !== length) dense = false;
  if (dense) return { entries: inspection.entries, sparse: false, target: [] };

  state.complete = false;
  const target = createDebuggerJsonRecord<unknown>();
  setDebuggerJsonProperty(target, '$at', targetPath);
  setDebuggerJsonProperty(target, '$entries', []);
  setDebuggerJsonProperty(target, '$length', Number.isSafeInteger(length) && length >= 0 ? length : '[unavailable]');
  setDebuggerJsonProperty(target, '$truncated', 'sparse-array');
  setDebuggerJsonProperty(target, '$type', 'array');
  return {
    entries: inspection.entries,
    sparse: true,
    target,
  };
}

function setDebuggerProjectionEntry(
  frame: DebuggerProjectionFrame,
  entry: DebuggerProjectionEntry,
  value: unknown,
): void {
  if (frame.sparse && !Array.isArray(frame.target)) {
    const entries = frame.target['$entries'];
    if (Array.isArray(entries)) {
      const projectedEntry = createDebuggerJsonRecord<unknown>();
      setDebuggerJsonProperty(
        projectedEntry,
        isDebuggerArrayIndex(entry.key) ? '$index' : '$key',
        isDebuggerArrayIndex(entry.key) ? Number(entry.key) : entry.key,
      );
      setDebuggerJsonProperty(projectedEntry, 'value', value);
      entries.push(projectedEntry);
    }
  } else if (Array.isArray(frame.target)) {
    const index = Number(entry.key);
    if (Number.isInteger(index) && index >= 0) frame.target[index] = value;
  } else {
    setDebuggerJsonProperty(frame.target, entry.key, value);
  }
}

function projectDebuggerValue(value: unknown, state: DebuggerProjectionState, path: string): unknown {
  if (value === null || typeof value !== 'object') {
    if (state.valueCount >= MAX_DEBUGGER_PROJECTION_VALUES) {
      state.complete = false;
      return debuggerProjectionTruncation('value-limit', path);
    }
    state.valueCount += 1;
    return debuggerPrimitiveProjection(value, state);
  }
  const knownPath = state.seen.get(value);
  if (knownPath !== undefined) {
    if (state.valueCount >= MAX_DEBUGGER_PROJECTION_VALUES) {
      state.complete = false;
      return debuggerProjectionTruncation('value-limit', path);
    }
    state.valueCount += 1;
    const reference = createDebuggerJsonRecord<unknown>();
    setDebuggerJsonProperty(reference, '$ref', knownPath);
    return reference;
  }
  if (state.valueCount >= MAX_DEBUGGER_PROJECTION_VALUES) {
    state.complete = false;
    return debuggerProjectionTruncation('value-limit', path);
  }

  const rootFrame = debuggerProjectionFrame(value, path, state);
  const root = rootFrame.target;
  state.seen.set(value, path);
  state.valueCount += 1;
  const stack: DebuggerProjectionFrame[] = [{ ...rootFrame, depth: 0, index: 0, path }];
  while (stack.length > 0) {
    const frame = stack.at(-1);
    if (!frame) break;
    if (frame.index >= frame.entries.length) {
      stack.pop();
      continue;
    }
    if (state.valueCount >= MAX_DEBUGGER_PROJECTION_VALUES) {
      state.complete = false;
      markDebuggerProjectionTruncated(frame.target, 'value-limit', frame.path);
      stack.pop();
      continue;
    }

    const entry = frame.entries[frame.index];
    if (!entry) {
      stack.pop();
      continue;
    }
    frame.index += 1;
    const childValue = entry.value;
    if (entry.accessor || childValue === null || typeof childValue !== 'object') {
      setDebuggerProjectionEntry(
        frame,
        entry,
        entry.accessor ? childValue : debuggerPrimitiveProjection(childValue, state),
      );
      state.valueCount += 1;
      continue;
    }
    const childKnownPath = state.seen.get(childValue);
    if (childKnownPath !== undefined) {
      const reference = createDebuggerJsonRecord<unknown>();
      setDebuggerJsonProperty(reference, '$ref', childKnownPath);
      setDebuggerProjectionEntry(frame, entry, reference);
      state.valueCount += 1;
      continue;
    }
    if (frame.depth + 1 >= MAX_DEBUGGER_PROJECTION_DEPTH) {
      state.complete = false;
      setDebuggerProjectionEntry(frame, entry, debuggerProjectionTruncation('depth-limit', entry.childPath));
      state.valueCount += 1;
      continue;
    }

    const childFrame = debuggerProjectionFrame(childValue, entry.childPath, state);
    setDebuggerProjectionEntry(frame, entry, childFrame.target);
    state.seen.set(childValue, entry.childPath);
    state.valueCount += 1;
    stack.push({ ...childFrame, depth: frame.depth + 1, index: 0, path: entry.childPath });
  }
  return root;
}

function debuggerTreeRecord(value: unknown): {
  record: Record<string, unknown> | null;
  unavailable: boolean;
} {
  if (typeof value !== 'object' || value === null) return { record: null, unavailable: false };
  try {
    return Array.isArray(value)
      ? { record: null, unavailable: false }
      : { record: value as Record<string, unknown>, unavailable: false };
  } catch {
    return { record: null, unavailable: true };
  }
}

function debuggerTreeChildren(
  node: Record<string, unknown>,
  path: string,
): {
  complete: boolean;
  entries: Array<{ index: number; node: Record<string, unknown> }>;
  truncations: Array<Record<string, string>>;
} {
  let childrenDescriptor: PropertyDescriptor | undefined;
  try {
    childrenDescriptor = Object.getOwnPropertyDescriptor(node, 'children');
  } catch {
    return {
      complete: false,
      entries: [],
      truncations: [debuggerProjectionTruncation('unavailable-children', path)],
    };
  }
  if (!childrenDescriptor) return { complete: true, entries: [], truncations: [] };
  if (!Object.prototype.hasOwnProperty.call(childrenDescriptor, 'value')) {
    return {
      complete: false,
      entries: [],
      truncations: [debuggerProjectionTruncation('accessor', path)],
    };
  }
  const children = childrenDescriptor.value as unknown;
  try {
    if (!Array.isArray(children)) return { complete: true, entries: [], truncations: [] };
  } catch {
    return {
      complete: false,
      entries: [],
      truncations: [debuggerProjectionTruncation('unavailable-children', path)],
    };
  }

  let descriptors: PropertyDescriptorMap;
  try {
    descriptors = Object.getOwnPropertyDescriptors(children) as unknown as PropertyDescriptorMap;
  } catch {
    return {
      complete: false,
      entries: [],
      truncations: [debuggerProjectionTruncation('unavailable-children', path)],
    };
  }
  const lengthDescriptor = descriptors['length'];
  const length =
    lengthDescriptor && Object.prototype.hasOwnProperty.call(lengthDescriptor, 'value')
      ? Number(lengthDescriptor.value)
      : 0;
  const entries: Array<{ index: number; node: Record<string, unknown> }> = [];
  const truncations: Array<Record<string, string>> = [];
  let complete = true;
  let numericProperties = 0;
  for (const key of Object.keys(descriptors)) {
    if (!isDebuggerArrayIndex(key)) continue;
    numericProperties += 1;
    const descriptor = descriptors[key];
    if (!descriptor?.enumerable || !Object.prototype.hasOwnProperty.call(descriptor, 'value')) {
      complete = false;
      truncations.push(debuggerProjectionTruncation('unavailable-child', `${path}[${key}]`));
      continue;
    }
    if (entries.length >= MAX_DEBUGGER_TREE_NODES) {
      complete = false;
      truncations.push(debuggerProjectionTruncation('tree-node-limit', path));
      break;
    }
    const child = debuggerTreeRecord(descriptor.value);
    if (child.record) {
      entries.push({ index: Number(key), node: child.record });
    } else if (child.unavailable) {
      complete = false;
      truncations.push(debuggerProjectionTruncation('unavailable-child', `${path}[${key}]`));
    }
  }
  if (!Number.isSafeInteger(length) || length < 0 || numericProperties !== length) {
    complete = false;
    truncations.push(debuggerProjectionTruncation('sparse-children', path));
  }
  return { complete, entries, truncations };
}

export function projectDebuggerTreeForJson(rootValue: unknown): DebuggerTreeProjection {
  const root = debuggerTreeRecord(rootValue).record;
  if (!root) throw new Error('The debugger snapshot tree must be a non-null object.');
  const nodes: DebuggerTreeProjectionNode[] = [];
  const truncations: Array<Record<string, string>> = [];
  const visited = new Set<object>();
  const projectionState: DebuggerProjectionState = { complete: true, seen: new Map(), valueCount: 0 };
  const stack: Array<{
    childEntries: Array<{ index: number; node: Record<string, unknown> }> | null;
    childIndex: number;
    depth: number;
    entered: boolean;
    index: number | null;
    node: Record<string, unknown>;
    parentIndex: number | null;
    sourceChildIndex: number | null;
  }> = [
    {
      childEntries: null,
      childIndex: 0,
      depth: 0,
      entered: false,
      index: null,
      node: root,
      parentIndex: null,
      sourceChildIndex: null,
    },
  ];
  while (stack.length > 0) {
    const frame = stack.at(-1);
    if (!frame) break;
    if (!frame.entered) {
      if (visited.has(frame.node)) {
        stack.pop();
        continue;
      }
      if (nodes.length >= MAX_DEBUGGER_TREE_NODES) {
        projectionState.complete = false;
        truncations.push(debuggerProjectionTruncation('tree-node-limit', '$.nodes'));
        break;
      }
      visited.add(frame.node);
      frame.entered = true;
      frame.index = nodes.length;
      const data = createDebuggerJsonRecord<unknown>();
      projectionState.seen.set(frame.node, `$.nodes[${frame.index}].data`);
      const inspection = debuggerOwnEntries(frame.node, `$.nodes[${frame.index}].data`, projectionState);
      if (inspection.inspectionError) {
        markDebuggerProjectionTruncated(data, 'unavailable-properties', `$.nodes[${frame.index}].data`);
      }
      for (const entry of inspection.entries) {
        if (entry.key === 'children') continue;
        if (projectionState.valueCount >= MAX_DEBUGGER_PROJECTION_VALUES) {
          projectionState.complete = false;
          markDebuggerProjectionTruncated(data, 'value-limit', `$.nodes[${frame.index}].data`);
          break;
        }
        setDebuggerJsonProperty(
          data,
          entry.key,
          entry.accessor ? entry.value : projectDebuggerValue(entry.value, projectionState, entry.childPath),
        );
      }
      nodes.push({
        childIndexes: [],
        data,
        depth: frame.depth,
        index: frame.index,
        parentIndex: frame.parentIndex,
        sourceChildIndex: frame.sourceChildIndex,
      });
      if (frame.parentIndex !== null) nodes[frame.parentIndex]?.childIndexes.push(frame.index);
      const children = debuggerTreeChildren(frame.node, `$.nodes[${frame.index}].children`);
      if (!children.complete) projectionState.complete = false;
      truncations.push(...children.truncations);
      frame.childEntries = children.entries;
      continue;
    }

    let child: { index: number; node: Record<string, unknown> } | null = null;
    while (frame.childEntries && frame.childIndex < frame.childEntries.length && !child) {
      const candidate = frame.childEntries[frame.childIndex];
      frame.childIndex += 1;
      if (candidate && !visited.has(candidate.node)) child = candidate;
    }
    if (child) {
      stack.push({
        childEntries: null,
        childIndex: 0,
        depth: frame.depth + 1,
        entered: false,
        index: null,
        node: child.node,
        parentIndex: frame.index,
        sourceChildIndex: child.index,
      });
      continue;
    }
    stack.pop();
  }
  return {
    complete: projectionState.complete,
    format: 'valdi-debugger-tree-v1',
    nodeCount: nodes.length,
    nodes,
    rootIndex: 0,
    truncations,
  };
}

function sendJson(response: ServerResponse, status: number, payload: unknown): void {
  response.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
  });
  response.end(JSON.stringify(payload));
}

function sendTraceJson(response: ServerResponse, status: number, payload: unknown): void {
  const serialized = JSON.stringify(payload);
  if (Buffer.byteLength(serialized, 'utf8') > MAX_TRACE_HTTP_RESPONSE_BYTES) {
    sendJson(response, 500, { error: 'Valdi performance trace response exceeded the HTTP size limit.' });
    return;
  }
  response.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
  });
  response.end(serialized);
}

async function readJsonBody(request: IncomingMessage): Promise<Record<string, unknown>> {
  const chunks: Buffer[] = [];
  let byteLength = 0;
  for await (const chunk of request) {
    const buffer = Buffer.isBuffer(chunk) ? chunk : Buffer.from(String(chunk));
    byteLength += buffer.length;
    if (byteLength > 1024 * 1024) {
      throw new ApiRequestError(400, 'Request body is too large.');
    }
    chunks.push(buffer);
  }

  let raw: string;
  try {
    raw = FATAL_JSON_UTF8_DECODER.decode(Buffer.concat(chunks, byteLength));
  } catch {
    throw new ApiRequestError(400, 'Request body must contain valid UTF-8.');
  }
  if (!raw.trim()) return {};
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw) as unknown;
  } catch {
    throw new ApiRequestError(400, 'Request body must contain valid JSON.');
  }
  if (typeof parsed !== 'object' || parsed === null || Array.isArray(parsed)) {
    throw new ApiRequestError(400, 'Request body must be a JSON object.');
  }
  return parsed as Record<string, unknown>;
}

function readNumber(searchParams: URLSearchParams, key: string, fallback: number): number {
  const value = searchParams.get(key);
  if (!value) return fallback;
  const parsed = Number.parseInt(value, 10);
  return Number.isNaN(parsed) ? fallback : parsed;
}

function readBodyNumber(body: Record<string, unknown>, key: string, fallback: number): number {
  const value = body[key];
  if (value === undefined) return fallback;
  const parsed = Number.parseInt(String(value), 10);
  return Number.isNaN(parsed) ? fallback : parsed;
}

function readRendererTracing(body: Record<string, unknown>): boolean {
  const value = body['rendererTracing'];
  if (value === undefined) return true;
  if (typeof value !== 'boolean') {
    throw new ApiRequestError(400, 'rendererTracing must be a boolean when provided.');
  }
  return value;
}

export function normalizeTraceCaptureDurationMs(body: Record<string, unknown>): number {
  const value = body['durationMs'];
  if (value === undefined) return DEFAULT_TRACE_CAPTURE_DURATION_MS;
  if (typeof value !== 'number' || !Number.isFinite(value)) {
    throw new ApiRequestError(400, 'durationMs must be a finite number when provided.');
  }
  return clampNumber(Math.round(value), MIN_TRACE_CAPTURE_DURATION_MS, MAX_TRACE_CAPTURE_DURATION_MS);
}

function readBodyString(body: Record<string, unknown>, key: string): string | undefined {
  const value = body[key];
  if (value === undefined || value === null || value === '') return undefined;
  return String(value);
}

function readBodyRecord(body: Record<string, unknown>, key: string): Record<string, unknown> {
  const value = body[key];
  if (!value || typeof value !== 'object' || Array.isArray(value)) return {};
  return value as Record<string, unknown>;
}

function readRecordString(record: Record<string, unknown>, key: string): string | undefined {
  const value = record[key];
  if (value === undefined || value === null || value === '') return undefined;
  return String(value);
}

function readRecordNumber(record: Record<string, unknown>, key: string): number | undefined {
  const value = record[key];
  if (value === undefined || value === null || value === '') return undefined;
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : undefined;
}

function readRecordBoolean(record: Record<string, unknown>, key: string): boolean | undefined {
  const value = record[key];
  if (value === undefined || value === null || value === '') return undefined;
  if (typeof value === 'boolean') return value;
  const normalized = String(value).toLowerCase();
  if (['1', 'true', 'yes', 'on'].includes(normalized)) return true;
  if (['0', 'false', 'no', 'off'].includes(normalized)) return false;
  return Boolean(value);
}

function clampNumber(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value));
}

function delay(durationMs: number): Promise<void> {
  return new Promise(resolve => {
    setTimeout(resolve, durationMs);
  });
}

function createWebPreviewDebuggerTarget(
  webPreviewUrl: string | undefined,
  debuggingPort: number,
): WebPreviewDebuggerTarget | null {
  const rawUrl = webPreviewUrl?.trim();
  if (!rawUrl) return null;
  if (!Number.isInteger(debuggingPort) || debuggingPort < 1 || debuggingPort > 65_535) {
    throw new Error(
      `Chromium debugging port must be an integer between 1 and 65535; received '${String(debuggingPort)}'.`,
    );
  }

  let applicationUrl: URL;
  try {
    applicationUrl = new URL(rawUrl);
  } catch {
    throw new Error(`Invalid web preview URL: ${rawUrl}`);
  }
  if (
    applicationUrl.protocol !== 'http:' ||
    !isLoopbackHost(applicationUrl.hostname) ||
    applicationUrl.username ||
    applicationUrl.password
  ) {
    throw new Error('The integrated DevTools web preview must use an unauthenticated loopback HTTP URL.');
  }

  return {
    applicationUrl: applicationUrl.toString(),
    debuggingPort,
    id: 'owl:web-preview',
    sessionId: 'web-preview',
  };
}

function webPreviewTargetPayload(target: WebPreviewDebuggerTarget): Record<string, unknown> {
  const applicationUrl = new URL(target.applicationUrl);
  const pathName = applicationUrl.pathname.split('/').filter(Boolean).at(-1) ?? applicationUrl.hostname;
  return {
    applicationId: target.applicationUrl,
    applicationUrl: target.applicationUrl,
    debuggingPort: target.debuggingPort,
    id: target.id,
    name: pathName,
    owlTarget: true,
    platform: 'web',
    sessionId: target.sessionId,
    state: 'available',
    transport: 'chromium-cdp',
  };
}

function resolveWebPreviewDebuggerTarget(sessionId: string | undefined): WebPreviewDebuggerTarget {
  if (
    !activeWebPreviewTarget ||
    !sessionId ||
    ![activeWebPreviewTarget.id, activeWebPreviewTarget.sessionId].includes(sessionId)
  ) {
    throw new ApiRequestError(404, 'The configured web preview debugger target is not available.');
  }
  return activeWebPreviewTarget;
}

interface InspectedWebPreviewContext {
  inspectedUrl: string;
  targetNonce: string;
}

function resolveInspectedWebPreviewContext(
  target: WebPreviewDebuggerTarget,
  inspectedUrl: string | undefined,
  targetNonce: string | undefined,
): InspectedWebPreviewContext {
  if (!inspectedUrl) {
    throw new ApiRequestError(400, 'DevTools target discovery requires the inspected page URL.');
  }
  if (!targetNonce || !WEB_PREVIEW_NONCE_PATTERN.test(targetNonce)) {
    throw new ApiRequestError(400, 'DevTools target discovery requires a valid inspected-tab nonce.');
  }

  let inspected: URL;
  try {
    inspected = new URL(inspectedUrl);
  } catch {
    throw new ApiRequestError(400, 'The inspected web preview URL is invalid.');
  }
  if (
    inspected.protocol !== 'http:' ||
    !isLoopbackHost(inspected.hostname) ||
    inspected.username ||
    inspected.password
  ) {
    throw new ApiRequestError(400, 'Valdi DevTools only attaches to an unauthenticated loopback application page.');
  }
  if (!matchesOwlApplicationUrl(inspected.toString(), target.applicationUrl)) {
    throw new ApiRequestError(404, 'The inspected page does not match the configured Valdi web preview target.');
  }
  return { inspectedUrl: inspected.toString(), targetNonce };
}

function resolveInspectedWebPreviewTarget(searchParams: URLSearchParams): Record<string, unknown> {
  if (!activeWebPreviewTarget) {
    throw new ApiRequestError(404, 'Start valdi debugger with --web-preview-url before opening the DevTools panel.');
  }
  resolveInspectedWebPreviewContext(
    activeWebPreviewTarget,
    searchParams.get('inspectedUrl') ?? undefined,
    searchParams.get('targetNonce') ?? undefined,
  );
  return { target: webPreviewTargetPayload(activeWebPreviewTarget) };
}

function readUnknownRecord(value: unknown): Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value) ? (value as Record<string, unknown>) : {};
}

async function inspectWebPreviewSnapshot(searchParams: URLSearchParams): Promise<Record<string, unknown>> {
  const target = resolveWebPreviewDebuggerTarget(searchParams.get('sessionId') ?? undefined);
  const context = resolveInspectedWebPreviewContext(
    target,
    searchParams.get('inspectedUrl') ?? undefined,
    searchParams.get('targetNonce') ?? undefined,
  );
  const bridgePayload = await readOwlDebuggerSnapshot(target.debuggingPort, target.applicationUrl, context.targetNonce);
  if (bridgePayload['channel'] !== 'valdi-web-debugger' || bridgePayload['type'] !== 'snapshot') {
    throw new Error('The running web preview returned an invalid Valdi debugger bridge payload.');
  }
  const snapshot = readUnknownRecord(bridgePayload['snapshot']);
  const tree = snapshot['tree'];
  if (typeof tree !== 'object' || tree === null || Array.isArray(tree)) {
    throw new Error('The running web preview has not mounted a Valdi renderer.');
  }
  return {
    issues: [],
    logs: [],
    selectedNodeId: bridgePayload['selectedNodeId'],
    source: 'owl',
    target: { ...webPreviewTargetPayload(target), state: 'attached' },
    targets: [webPreviewTargetPayload(target)],
    tree: projectDebuggerTreeForJson(tree),
    viewport: snapshot['viewport'],
  };
}

async function evaluateWebPreviewConsole(params: Record<string, unknown>): Promise<Record<string, unknown>> {
  const target = resolveWebPreviewDebuggerTarget(readRecordString(params, 'sessionId'));
  const context = resolveInspectedWebPreviewContext(
    target,
    readRecordString(params, 'inspectedUrl'),
    readRecordString(params, 'targetNonce'),
  );
  const expression = readRecordString(params, 'expression')?.trim();
  if (!expression) {
    throw new ApiRequestError(400, 'Web preview console evaluation requires a JavaScript expression.');
  }
  if (expression.length > 10_000) {
    throw new ApiRequestError(400, 'Web preview console expressions cannot exceed 10,000 characters.');
  }
  const wrapped =
    `Promise.resolve().then(() => (0, eval)(${JSON.stringify(expression)})).then(value => ({ ` +
    "type: value === null ? 'null' : typeof value, value: value === undefined ? null : value }))";
  const result = await evaluateOwlApplicationExpression(
    target.debuggingPort,
    target.applicationUrl,
    context.targetNonce,
    wrapped,
  );
  if (typeof result !== 'object' || result === null || Array.isArray(result)) {
    throw new Error('The web preview console returned an invalid evaluation result.');
  }
  return { ...(result as Record<string, unknown>), sessionId: target.sessionId };
}

async function highlightWebPreviewNode(params: Record<string, unknown>): Promise<Record<string, unknown>> {
  const target = resolveWebPreviewDebuggerTarget(readRecordString(params, 'sessionId'));
  const context = resolveInspectedWebPreviewContext(
    target,
    readRecordString(params, 'inspectedUrl'),
    readRecordString(params, 'targetNonce'),
  );
  const nodeId = readRecordString(params, 'nodeId');
  const expression =
    nodeId === undefined
      ? 'Boolean(globalThis.__VALDI_WEB_DEBUGGER__?.clearHighlight?.())'
      : `Boolean(globalThis.__VALDI_WEB_DEBUGGER__?.highlightNode?.(${JSON.stringify(nodeId)}))`;
  const highlighted = await evaluateOwlApplicationExpression(
    target.debuggingPort,
    target.applicationUrl,
    context.targetNonce,
    expression,
  );
  return {
    highlighted: highlighted === true,
    ...(nodeId === undefined ? {} : { nodeId }),
    sessionId: target.sessionId,
  };
}

function createDebuggerUiState(): DebuggerUiState {
  return {
    revision: 0,
    selectedNodeId: null,
    selectedTargetId: null,
    activeSection: 'ui',
    activeTab: 'overview',
    overlayMode: 'live',
    autoRefresh: false,
    followLatestTarget: true,
    port: STANDALONE_PORT,
    updatedAt: new Date().toISOString(),
    lastAction: null,
  };
}

function normalizeDebuggerSection(section: string): string {
  const normalized = section.trim().toLowerCase();
  if (normalized === 'logger' || normalized === 'loger') return 'logs';
  return normalized;
}

function readValdiLogsDirectory(): string {
  if (activeLogsDirectory !== null) return activeLogsDirectory;
  const logsOutputDirectory = getUserConfig().logs_output_dir;
  if (typeof logsOutputDirectory !== 'string' || logsOutputDirectory.trim() === '') {
    throw new Error("Missing 'logs_output_dir' config value in the Valdi config file.");
  }
  return resolveFilePath(logsOutputDirectory);
}

async function collectLogFiles(directory: string): Promise<LogFile[]> {
  const files: LogFile[] = [];

  async function visit(dir: string): Promise<void> {
    const entries = await fs.readdir(dir, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        await visit(fullPath);
      } else if (entry.isFile() && fullPath.endsWith('.log')) {
        const stat = await fs.stat(fullPath);
        files.push({ path: fullPath, stat });
      }
    }
  }

  await visit(directory);
  return files.sort((a, b) => b.stat.mtimeMs - a.stat.mtimeMs);
}

function parseValdiLogRecords(content: string, limit: number, startOffset: number): ParsedLogRecord[] {
  const records: ParsedLogRecord[] = [];
  let current: ParsedLogRecord | null = null;
  let byteOffset = startOffset;
  const linePattern = /^(\d{4}-\d{2}-\d{2}) (\d{2}:\d{2}:\d{2}) [+-]\d{4} \[(debug|log|info|warn|error)] (.*)$/;

  function pushCurrent() {
    if (current) records.push(current);
    current = null;
  }

  for (const rawLine of content.match(/[^\n]*(?:\n|$)/g) ?? []) {
    if (rawLine === '') continue;
    const lineOffset = byteOffset;
    byteOffset += Buffer.byteLength(rawLine, 'utf8');
    const line = rawLine.endsWith('\n') ? rawLine.slice(0, -1).replace(/\r$/, '') : rawLine;
    const match = line.match(linePattern);
    if (match) {
      pushCurrent();
      const rawMessage = match[4] ?? '';
      const isJsLog = rawMessage.startsWith('[JS]');
      current = {
        entry: {
          time: match[2] ?? '',
          level: match[3] === 'log' ? 'info' : (match[3] ?? 'info'),
          source: isJsLog ? 'js' : 'valdi',
          message: isJsLog ? rawMessage.replace(/^\[JS]\s*/, '') : rawMessage,
          timestamp: `${match[1] ?? ''}T${match[2] ?? ''}Z`,
        },
        offset: lineOffset,
      };
    } else if (/^-+ Session started /.test(line)) {
      pushCurrent();
      records.length = 0;
    } else if (/^-+ Session /.test(line)) {
      pushCurrent();
    } else if (current && line.trim()) {
      current.entry.message += `\n${line}`;
    }
  }
  pushCurrent();

  return records.slice(-limit);
}

function parseValdiLog(content: string, limit: number): LogEntry[] {
  return parseValdiLogRecords(content, limit, 0).map(record => record.entry);
}

async function inspectRuntimeLogs(searchParams: URLSearchParams): Promise<Record<string, unknown>> {
  const limit = clampNumber(readNumber(searchParams, 'limit', 120), 1, 1000);
  const selected = await selectRuntimeLogFile(searchParams);
  if (!selected.latest) return { logFile: null, logs: [] };

  const { content } = await readRuntimeLogTail(selected.latest);
  return {
    logFile: path.basename(selected.latest.path),
    size: selected.latest.stat.size,
    modifiedAt: selected.latest.stat.mtime.toISOString(),
    logs: parseValdiLog(content, limit),
  };
}

async function readRuntimeLogTail(logFile: LogFile): Promise<RuntimeLogTail> {
  if (logFile.stat.size <= MAX_RUNTIME_LOG_READ_BYTES) {
    return {
      content: await fs.readFile(logFile.path, 'utf8'),
      startOffset: 0,
    };
  }

  const file = await fs.open(logFile.path, 'r');
  try {
    const buffer = Buffer.alloc(MAX_RUNTIME_LOG_READ_BYTES);
    const position = Math.max(0, logFile.stat.size - MAX_RUNTIME_LOG_READ_BYTES);
    const { bytesRead } = await file.read(buffer, 0, buffer.length, position);
    const tail = buffer.subarray(0, bytesRead);
    const firstLineBreak = tail.indexOf(10);
    if (firstLineBreak === -1) return { content: '', startOffset: position + bytesRead };
    return {
      content: tail.subarray(firstLineBreak + 1).toString('utf8'),
      startOffset: position + firstLineBreak + 1,
    };
  } finally {
    await file.close();
  }
}

async function selectRuntimeLogFile(searchParams: URLSearchParams): Promise<{
  logsDirectory: string;
  latest: LogFile | null;
}> {
  const applicationId = searchParams.get('applicationId');
  const platform = searchParams.get('platform');
  const logsDirectory = readValdiLogsDirectory();
  const logFiles = await collectLogFiles(logsDirectory);
  let latest: LogFile | undefined;
  if (applicationId !== null && platform !== null) {
    latest = logFiles.find(file => path.basename(file.path) === `${platform}-${applicationId}.log`);
  } else if (applicationId !== null) {
    latest = logFiles.find(file => path.basename(file.path).endsWith(`-${applicationId}.log`));
  } else if (platform !== null) {
    latest = logFiles.find(file => path.basename(file.path).startsWith(`${platform}-`));
  } else if (logFiles.length === 1) {
    latest = logFiles[0];
  }

  return { logsDirectory, latest: latest ?? null };
}

function sendSse(response: ServerResponse, event: string, payload: unknown): void {
  response.write(`event: ${event}\n`);
  response.write(`data: ${JSON.stringify(payload)}\n\n`);
}

function registerEventStream(request: IncomingMessage, response: ServerResponse, onClose: () => void): () => void {
  let closed = false;
  const close = () => {
    if (closed) return;
    closed = true;
    eventStreamClosers.delete(close);
    onClose();
    if (!response.writableEnded) response.end();
  };
  eventStreamClosers.add(close);
  request.once('aborted', close);
  response.once('close', close);
  response.once('finish', close);
  return close;
}

function broadcastDevEvent(event: string, payload: unknown): void {
  for (const response of devEventClients) {
    try {
      sendSse(response, event, payload);
    } catch {
      devEventClients.delete(response);
    }
  }
}

function scheduleDevReload(fileName: string): void {
  if (devReloadTimer) clearTimeout(devReloadTimer);
  devReloadTimer = setTimeout(() => {
    devReloadTimer = null;
    devRevision += 1;
    broadcastDevEvent('reload', {
      revision: devRevision,
      file: fileName,
      time: new Date().toISOString(),
    });
  }, 75);
}

function streamDevEvents(request: IncomingMessage, response: ServerResponse): void {
  response.writeHead(200, {
    'Content-Type': 'text/event-stream; charset=utf-8',
    'Cache-Control': 'no-store, no-transform',
    Connection: 'keep-alive',
  });
  response.write('\n');
  devEventClients.add(response);
  sendSse(response, 'ready', {
    revision: devRevision,
    time: new Date().toISOString(),
  });

  const heartbeat = setInterval(() => {
    try {
      sendSse(response, 'heartbeat', {
        revision: devRevision,
        time: new Date().toISOString(),
      });
    } catch {
      if (!response.writableEnded) response.end();
    }
  }, 15_000);
  registerEventStream(request, response, () => {
    devEventClients.delete(response);
    clearInterval(heartbeat);
  });
}

function cloneDebuggerActionRecord(record: DebuggerActionRecord | null): DebuggerActionRecord | null {
  if (!record) return null;
  return {
    ...record,
    params: { ...record.params },
  };
}

function cloneDebuggerUiState(): DebuggerUiState {
  return {
    ...debuggerUiState,
    lastAction: cloneDebuggerActionRecord(debuggerUiState.lastAction),
  };
}

function debuggerStatePayload(): Record<string, unknown> {
  return {
    state: cloneDebuggerUiState(),
    capabilities: {
      actions: debuggerActions,
    },
    server: {
      host: activeHost,
      hotReloadProxyPort: HOT_RELOAD_PROXY_PORT,
      time: new Date().toISOString(),
    },
  };
}

function broadcastDebuggerEvent(event: string, payload: unknown): void {
  for (const response of debuggerEventClients) {
    try {
      sendSse(response, event, payload);
    } catch {
      debuggerEventClients.delete(response);
    }
  }
}

function streamDebuggerEvents(request: IncomingMessage, response: ServerResponse): void {
  response.writeHead(200, {
    'Content-Type': 'text/event-stream; charset=utf-8',
    'Cache-Control': 'no-store, no-transform',
    Connection: 'keep-alive',
  });
  response.write('\n');
  debuggerEventClients.add(response);
  sendSse(response, 'ready', debuggerStatePayload());

  const heartbeat = setInterval(() => {
    try {
      sendSse(response, 'heartbeat', debuggerStatePayload());
    } catch {
      if (!response.writableEnded) response.end();
    }
  }, 15_000);
  registerEventStream(request, response, () => {
    debuggerEventClients.delete(response);
    clearInterval(heartbeat);
  });
}

function applyDebuggerUiAction(action: string, params: Record<string, unknown>, source: string): DebuggerActionRecord {
  const time = new Date().toISOString();
  const nextParams = { ...params };

  switch (action) {
    case 'selectNode': {
      const id = readRecordString(nextParams, 'id') ?? readRecordString(nextParams, 'nodeId');
      if (id !== undefined) debuggerUiState.selectedNodeId = id;

      break;
    }
    case 'selectTarget': {
      const id = readRecordString(nextParams, 'id') ?? readRecordString(nextParams, 'targetId');
      if (id !== undefined) {
        debuggerUiState.selectedTargetId = id;
        debuggerUiState.followLatestTarget = false;
      }

      break;
    }
    case 'setActiveTab': {
      const tab = readRecordString(nextParams, 'tab');
      if (tab !== undefined) debuggerUiState.activeTab = tab;

      break;
    }
    case 'setActiveSection': {
      const section = readRecordString(nextParams, 'section');
      if (section !== undefined) {
        const normalizedSection = normalizeDebuggerSection(section);
        nextParams['section'] = normalizedSection;
        debuggerUiState.activeSection = normalizedSection;
      }

      break;
    }
    case 'setOverlayMode': {
      const mode = readRecordString(nextParams, 'mode');
      if (mode !== undefined) {
        const normalizedMode = mode === 'views' || mode === 'components' ? 'live' : mode;
        nextParams['mode'] = normalizedMode;
        debuggerUiState.overlayMode = normalizedMode;
      }

      break;
    }
    case 'setAutoRefresh': {
      const enabled = readRecordBoolean(nextParams, 'enabled');
      if (enabled !== undefined) debuggerUiState.autoRefresh = enabled;

      break;
    }
    case 'setPort': {
      const port = readRecordNumber(nextParams, 'port');
      if (port === undefined || !Number.isInteger(port) || port < 1 || port > 65_535) {
        throw new ApiRequestError(400, 'setPort requires an integer port between 1 and 65535.');
      }
      debuggerUiState.port = port;

      break;
    }
    case 'attach': {
      debuggerUiState.followLatestTarget = true;

      break;
    }
    case 'detach': {
      debuggerUiState.followLatestTarget = false;

      break;
    }
    // No default
  }

  const record = {
    action,
    params: nextParams,
    source,
    time,
  };
  debuggerEventRevision += 1;
  debuggerUiState.revision = debuggerEventRevision;
  debuggerUiState.updatedAt = time;
  debuggerUiState.lastAction = record;
  return record;
}

async function handleDebuggerAction(request: IncomingMessage): Promise<Record<string, unknown>> {
  const body = await readJsonBody(request);
  const action = readBodyString(body, 'action') ?? readBodyString(body, 'type');
  if (!action) {
    throw new ApiRequestError(400, 'Debugger action requests require an action.');
  }
  if (!debuggerActions.includes(action)) {
    throw new ApiRequestError(400, `Unknown debugger action '${action}'.`);
  }
  const params = readBodyRecord(body, 'params');
  const source = readBodyString(body, 'source') ?? 'agent';
  const record = applyDebuggerUiAction(action, params, source);
  const payload = {
    ...record,
    state: cloneDebuggerUiState(),
  };
  broadcastDebuggerEvent('debugger-action', payload);
  broadcastDebuggerEvent('debugger-state', debuggerStatePayload());
  return {
    ok: true,
    ...payload,
  };
}

async function streamRuntimeLogs(
  request: IncomingMessage,
  response: ServerResponse,
  searchParams: URLSearchParams,
): Promise<void> {
  const limit = clampNumber(readNumber(searchParams, 'limit', 120), 1, 1000);
  const sentKeyLimit = limit * 2;
  const sent = new Set<string>();
  let closed = false;
  let polling = false;
  let lastLogPath: string | null = null;
  let lastLogInode: number | null = null;
  let lastLogModifiedMs = -1;
  let lastLogSize = -1;

  response.writeHead(200, {
    'Content-Type': 'text/event-stream; charset=utf-8',
    'Cache-Control': 'no-store, no-transform',
    Connection: 'keep-alive',
  });
  response.write('\n');

  async function readAndSend() {
    if (closed) return;
    try {
      const selected = await selectRuntimeLogFile(searchParams);
      if (!selected.latest) {
        sendSse(response, 'meta', { logFile: null });
        return;
      }

      const logFileChanged = selected.latest.path !== lastLogPath || selected.latest.stat.ino !== lastLogInode;
      const logFileTruncated = !logFileChanged && lastLogSize >= 0 && selected.latest.stat.size < lastLogSize;
      if (logFileChanged || logFileTruncated) {
        sent.clear();
        lastLogPath = selected.latest.path;
        lastLogInode = selected.latest.stat.ino;
        lastLogModifiedMs = -1;
        lastLogSize = -1;
        sendSse(response, 'meta', {
          logFile: path.basename(selected.latest.path),
          size: selected.latest.stat.size,
          modifiedAt: selected.latest.stat.mtime.toISOString(),
        });
      }

      if (selected.latest.stat.mtimeMs === lastLogModifiedMs && selected.latest.stat.size === lastLogSize) {
        return;
      }
      lastLogModifiedMs = selected.latest.stat.mtimeMs;
      lastLogSize = selected.latest.stat.size;

      const tail = await readRuntimeLogTail(selected.latest);
      const records = parseValdiLogRecords(tail.content, limit, tail.startOffset);
      const nextLogs: LogEntry[] = [];
      for (const record of records) {
        const key = `${selected.latest.stat.ino}:${record.offset}`;
        if (sent.has(key)) continue;
        sent.add(key);
        nextLogs.push(record.entry);
      }
      while (sent.size > sentKeyLimit) {
        const oldestKey = sent.values().next().value;
        if (oldestKey === undefined) break;
        sent.delete(oldestKey);
      }

      if (nextLogs.length > 0) sendSse(response, 'logs', { logs: nextLogs });
    } catch (error) {
      sendSse(response, 'stream-error', clientErrorPayload(error));
    }
  }

  async function poll() {
    if (closed || polling) return;
    polling = true;
    try {
      await readAndSend();
    } finally {
      polling = false;
    }
  }

  // Poll instead of fs.watch so several debugger tabs do not exhaust macOS file watcher limits.
  const poller = setInterval(() => {
    void poll();
  }, 1000);

  const heartbeat = setInterval(() => {
    if (!closed) sendSse(response, 'heartbeat', { time: new Date().toISOString() });
  }, 15_000);

  registerEventStream(request, response, () => {
    closed = true;
    clearInterval(poller);
    clearInterval(heartbeat);
  });

  await poll();
}

async function withConnection<T>(port: number, callback: (conn: DaemonConnection) => Promise<T>): Promise<T> {
  const conn = await connectToDaemon(port);
  try {
    await conn.configure();
    return await callback(conn);
  } finally {
    conn.close();
  }
}

async function collectClientContexts(
  conn: DaemonConnection,
  clients: DaemonConnectedClient[],
  knownClientId: string | null,
  knownContexts: RemoteContext[] | null,
): Promise<ClientWithContexts[]> {
  const clientsWithContexts: ClientWithContexts[] = [];
  for (const client of clients) {
    let contexts: RemoteContext[] = [];
    let contextError = null;
    if (client.client_id === knownClientId && knownContexts !== null) {
      contexts = knownContexts;
    } else {
      try {
        contexts = await conn.listContexts(client.client_id);
      } catch (error) {
        contextError = clientErrorPayload(error).error;
      }
    }

    clientsWithContexts.push({
      ...client,
      contexts,
      contextError,
    });
  }
  return clientsWithContexts;
}

async function inspectPort(port: number): Promise<{
  port: number;
  portName: string;
  connected: boolean;
  clients: ClientWithContexts[];
  error: string | null;
}> {
  try {
    return await withConnection(port, async conn => {
      const clients = await conn.listConnectedClients();
      const clientsWithContexts = await collectClientContexts(conn, clients, null, null);

      return {
        port,
        portName: portName(port),
        connected: true,
        clients: clientsWithContexts,
        error: null,
      };
    });
  } catch (error) {
    return {
      port,
      portName: portName(port),
      connected: false,
      clients: [],
      error: clientErrorPayload(error).error,
    };
  }
}

async function inspectStatus(searchParams: URLSearchParams): Promise<Record<string, unknown>> {
  const explicitPort = searchParams.get('port');
  const ports = explicitPort ? [readNumber(searchParams, 'port', MOBILE_PORT)] : [STANDALONE_PORT, MOBILE_PORT];

  const results: Array<Awaited<ReturnType<typeof inspectPort>>> = [];
  for (const port of ports) {
    results.push(await inspectPort(port));
  }

  return {
    ports: results,
    hotReloadProxy: {
      port: HOT_RELOAD_PROXY_PORT,
      connected: await probeTcpPort(HOT_RELOAD_PROXY_PORT, 750),
    },
    defaultPort: STANDALONE_PORT,
    webPreviewTarget: activeWebPreviewTarget ? webPreviewTargetPayload(activeWebPreviewTarget) : null,
  };
}

async function resolveTarget(
  searchParams: URLSearchParams,
  conn: DaemonConnection,
): Promise<{
  clients: DaemonConnectedClient[];
  client: DaemonConnectedClient;
  contexts: RemoteContext[];
  context: RemoteContext;
}> {
  const { clients, client } = await resolveClient(searchParams, conn);
  const contexts = await conn.listContexts(client.client_id);
  if (contexts.length === 0) {
    throw new Error(`No Valdi contexts found for client ${client.client_id}.`);
  }

  const requestedContextId = searchParams.get('contextId');
  const lastContext = contexts.at(-1);
  if (!lastContext) {
    throw new Error(`No Valdi contexts found for client ${client.client_id}.`);
  }
  let context = lastContext;
  if (requestedContextId !== null) {
    const requestedContext = contexts.find(candidate => candidate.id === requestedContextId);
    if (!requestedContext) {
      throw new Error(`Client ${client.client_id} has no Valdi context with id ${requestedContextId}.`);
    }
    context = requestedContext;
  }

  return { clients, client, contexts, context };
}

async function resolveClient(
  searchParams: URLSearchParams,
  conn: DaemonConnection,
): Promise<{ clients: DaemonConnectedClient[]; client: DaemonConnectedClient }> {
  const clients = await conn.listConnectedClients();
  if (clients.length === 0) {
    throw new Error('No clients connected to this Valdi daemon.');
  }

  const requestedClientId = searchParams.get('clientId');
  const firstClient = clients[0];
  if (!firstClient) {
    throw new Error('No clients connected to this Valdi daemon.');
  }
  let client = firstClient;
  if (requestedClientId !== null) {
    const requestedClient = clients.find(candidate => candidate.client_id === requestedClientId);
    if (!requestedClient) {
      throw new Error(`No connected Valdi client has id ${requestedClientId}.`);
    }
    client = requestedClient;
  }

  return { clients, client };
}

function createDaemonTargetPayload(
  port: number,
  client: DaemonConnectedClient,
  context: RemoteContext,
): Record<string, unknown> {
  return {
    id: `${port}:${client.client_id}:${context.id}`,
    name: context.rootComponentName || client.application_id || `Client ${client.client_id}`,
    platform: client.platform || portName(port),
    transport: `daemon:${port}`,
    state: 'attached',
    port,
    clientId: client.client_id,
    contextId: context.id,
    applicationId: client.application_id,
  };
}

function flattenTargets(
  port: number,
  clients: ClientWithContexts[],
  selectedClientId: string,
  selectedContextId: string,
): Array<Record<string, unknown>> {
  const targets: Array<Record<string, unknown>> = [];
  for (const client of clients) {
    for (const context of client.contexts || []) {
      const selected = client.client_id === selectedClientId && context.id === selectedContextId;
      targets.push({
        id: `${port}:${client.client_id}:${context.id}`,
        name: context.rootComponentName || client.application_id || `Client ${client.client_id}`,
        platform: client.platform || portName(port),
        state: selected ? 'attached' : 'available',
        transport: `daemon:${port}`,
        port,
        clientId: client.client_id,
        contextId: context.id,
        applicationId: client.application_id,
      });
    }
  }
  return targets;
}

async function inspectSnapshot(searchParams: URLSearchParams): Promise<Record<string, unknown>> {
  const port = readNumber(searchParams, 'port', STANDALONE_PORT);
  return await withConnection(port, async conn => {
    const { clients, client, contexts, context } = await resolveTarget(searchParams, conn);
    const tree = await conn.getContextTree(client.client_id, context.id, true);
    const clientsWithContexts = await collectClientContexts(conn, clients, client.client_id, contexts);
    const targets = flattenTargets(port, clientsWithContexts, client.client_id, context.id);

    return {
      source: 'valdi-daemon',
      target: {
        id: `${port}:${client.client_id}:${context.id}`,
        name: context.rootComponentName || client.application_id || `Client ${client.client_id}`,
        platform: client.platform || portName(port),
        transport: `daemon:${port}`,
        state: 'attached',
        proxyPort: port,
        clientId: client.client_id,
        contextId: context.id,
        applicationId: client.application_id,
      },
      targets,
      contexts: targets,
      tree: projectDebuggerTreeForJson(tree),
      issues: [],
      logs: [
        {
          time: new Date().toTimeString().slice(0, 8),
          level: 'info',
          source: 'daemon',
          message: `Fetched ${context.rootComponentName || context.id} from Valdi daemon port ${port}.`,
        },
      ],
    };
  });
}

async function inspectElementSnapshot(searchParams: URLSearchParams): Promise<Record<string, unknown>> {
  const port = readNumber(searchParams, 'port', STANDALONE_PORT);
  const elementId = searchParams.get('elementId');
  if (!elementId) {
    throw new Error('Missing required elementId query parameter.');
  }

  return await withConnection(port, async conn => {
    const { client, context } = await resolveTarget(searchParams, conn);
    const base64 = await conn.takeSnapshot(client.client_id, elementId, context.id);
    if (!isValidSnapshotBase64(base64)) {
      throw new Error('The target returned an invalid base64 element snapshot.');
    }
    return {
      image: `data:image/png;base64,${base64}`,
      elementId,
      port,
      clientId: client.client_id,
      contextId: context.id,
    };
  });
}

async function inspectHeap(request: IncomingMessage, searchParams: URLSearchParams): Promise<Record<string, unknown>> {
  if (request.method !== 'POST') {
    throw new Error('Heap inspection requires POST.');
  }

  const body = await readJsonBody(request);
  const port = readNumber(searchParams, 'port', STANDALONE_PORT);
  const performGC = body['performGC'] === true;
  return await withConnection(port, async conn => {
    const { client } = await resolveTarget(searchParams, conn);
    const heap = await conn.dumpHeap(client.client_id, performGC);
    return {
      port,
      clientId: client.client_id,
      heap,
    };
  });
}

async function sendPerformanceTraceMessage(
  searchParams: URLSearchParams,
  action: PerformanceTraceAction,
  data: Record<string, unknown>,
): Promise<Record<string, unknown>> {
  const port = readNumber(searchParams, 'port', STANDALONE_PORT);
  return await withConnection(port, async conn => {
    const { client, context } = await resolveTarget(searchParams, conn);

    let result: Record<string, unknown>;
    if (action === PerformanceTraceAction.Status) {
      result = await conn.performanceTraceStatus(client.client_id, { contextId: context.id }, TRACE_DAEMON_TIMEOUT_MS);
    } else if (action === PerformanceTraceAction.Start) {
      const rendererTracing = data['rendererTracing'];
      if (typeof rendererTracing !== 'boolean') {
        throw new TypeError('Performance trace start requires a rendererTracing boolean.');
      }
      result = await conn.performanceTraceStart(
        client.client_id,
        {
          contextId: context.id,
          rendererTracing,
        },
        TRACE_DAEMON_TIMEOUT_MS,
      );
    } else {
      result = await conn.performanceTraceStop(client.client_id, { contextId: context.id }, TRACE_DAEMON_TIMEOUT_MS);
    }

    assertPerformanceTraceContext(action, result, context.id);

    return decorateTraceResult(result, createDaemonTargetPayload(port, client, context));
  });
}

export function assertPerformanceTraceContext(
  action: PerformanceTraceAction,
  result: Record<string, unknown>,
  expectedContextId: string,
): void {
  if (action !== PerformanceTraceAction.Status && result['contextId'] !== expectedContextId) {
    throw new Error(
      `The Valdi runtime returned trace data for context ${String(result['contextId'])}, expected ${expectedContextId}.`,
    );
  }
}

async function inspectPerformanceTraceStatus(
  request: IncomingMessage,
  searchParams: URLSearchParams,
): Promise<Record<string, unknown>> {
  if (request.method !== 'GET') {
    throw new Error('Performance trace status requires GET.');
  }
  return await sendPerformanceTraceMessage(searchParams, PerformanceTraceAction.Status, {});
}

async function startPerformanceTrace(
  request: IncomingMessage,
  searchParams: URLSearchParams,
): Promise<Record<string, unknown>> {
  if (request.method !== 'POST') {
    throw new Error('Performance trace start requires POST.');
  }
  return await runTraceTransition(async () => {
    const body = await readJsonBody(request);
    return await sendPerformanceTraceMessage(searchParams, PerformanceTraceAction.Start, {
      rendererTracing: readRendererTracing(body),
    });
  });
}

async function stopPerformanceTrace(
  request: IncomingMessage,
  searchParams: URLSearchParams,
): Promise<Record<string, unknown>> {
  if (request.method !== 'POST') {
    throw new Error('Performance trace stop requires POST.');
  }
  return await runTraceTransition(
    async () => await sendPerformanceTraceMessage(searchParams, PerformanceTraceAction.Stop, {}),
  );
}

async function capturePerformanceTrace(
  request: IncomingMessage,
  searchParams: URLSearchParams,
): Promise<Record<string, unknown>> {
  if (request.method !== 'POST') {
    throw new Error('Performance trace capture requires POST.');
  }
  return await runTraceTransition(async () => {
    const body = await readJsonBody(request);
    const options: PerformanceTraceCaptureOptions = {
      durationMs: normalizeTraceCaptureDurationMs(body),
      rendererTracing: readRendererTracing(body),
    };
    return await runPerformanceTraceCapture(options, {
      send: async (action, data) => await sendPerformanceTraceMessage(searchParams, action, data),
      wait: delay,
    });
  });
}

export async function runPerformanceTraceCapture(
  options: PerformanceTraceCaptureOptions,
  dependencies: PerformanceTraceCaptureDependencies,
): Promise<Record<string, unknown>> {
  const status = await dependencies.send(PerformanceTraceAction.Status, {});
  if (status['tracingSupported'] !== true) {
    throw new Error('This Valdi runtime does not support renderer trace capture.');
  }
  if (status['recording'] || status['completedRecordingAvailable']) {
    const contextId = status['contextId'] ?? status['completedContextId'];
    const contextSuffix = typeof contextId === 'string' ? ` for context ${contextId}` : '';
    throw new Error(
      `A renderer trace recording is already active or waiting to be retrieved${contextSuffix}. Stop it before one-shot capture.`,
    );
  }

  await dependencies.send(PerformanceTraceAction.Start, { rendererTracing: options.rendererTracing });
  try {
    await dependencies.wait(options.durationMs);
  } catch (waitError) {
    try {
      await dependencies.send(PerformanceTraceAction.Stop, {});
    } catch (cleanupError) {
      throw new Error(
        `${errorPayload(waitError).error} Best-effort renderer trace cleanup also failed: ${errorPayload(cleanupError).error}`,
      );
    }
    throw waitError;
  }

  try {
    return await dependencies.send(PerformanceTraceAction.Stop, {});
  } catch (stopError) {
    try {
      return await dependencies.send(PerformanceTraceAction.Stop, {});
    } catch (cleanupError) {
      throw new Error(
        `${errorPayload(stopError).error} Best-effort renderer trace cleanup also failed: ${errorPayload(cleanupError).error}`,
      );
    }
  }
}

async function runTraceTransition<T>(transition: () => Promise<T>): Promise<T> {
  if (traceTransitionInProgress) {
    throw new Error('Another renderer trace transition is already in progress.');
  }

  traceTransitionInProgress = true;
  try {
    return await transition();
  } finally {
    traceTransitionInProgress = false;
  }
}

export function decorateTraceResult(
  result: Record<string, unknown>,
  captureTarget: Record<string, unknown>,
): Record<string, unknown> {
  const traces = readRecordedTraces(result['traces']);
  const receivedTraceCount = Array.isArray(result['traces']) ? result['traces'].length : 0;
  const runtimeDroppedTraceCount =
    typeof result['droppedTraceEventCount'] === 'number' &&
    Number.isSafeInteger(result['droppedTraceEventCount']) &&
    result['droppedTraceEventCount'] >= 0
      ? Math.max(0, result['droppedTraceEventCount'])
      : 0;
  const localDroppedTraceCount = Math.max(0, receivedTraceCount - traces.length);
  const initialDroppedTraceEventCount = saturatingAdd(runtimeDroppedTraceCount, localDroppedTraceCount);
  const boundedCaptureTarget: Record<string, unknown> = {};
  for (const key of TRACE_CAPTURE_TARGET_STRING_KEYS) {
    const value = captureTarget[key];
    if (typeof value === 'string') {
      boundedCaptureTarget[key] = truncateStringForJson(value, MAX_TRACE_HTTP_STRING_BYTES);
    }
  }
  if (typeof captureTarget['port'] === 'number' && Number.isFinite(captureTarget['port'])) {
    boundedCaptureTarget['port'] = captureTarget['port'];
  }
  const boundedContextId =
    typeof result['contextId'] === 'string'
      ? truncateStringForJson(result['contextId'], MAX_TRACE_HTTP_STRING_BYTES)
      : undefined;
  const boundedCompletedContextId =
    typeof result['completedContextId'] === 'string'
      ? truncateStringForJson(result['completedContextId'], MAX_TRACE_HTTP_STRING_BYTES)
      : undefined;
  const boundedCompletionError =
    typeof result['completionError'] === 'string'
      ? truncateStringForJson(result['completionError'], MAX_TRACE_HTTP_STRING_BYTES)
      : undefined;

  const buildResult = (includedTraceCount: number): Record<string, unknown> => {
    const includedTraces = traces.slice(0, includedTraceCount);
    const droppedTraceEventCount = saturatingAdd(initialDroppedTraceEventCount, traces.length - includedTraceCount);
    const perfettoMetadata = buildPerfettoCaptureMetadata(boundedCaptureTarget, droppedTraceEventCount);
    return {
      recording: result['recording'] === true,
      contextId: boundedContextId,
      completedRecordingAvailable: result['completedRecordingAvailable'] === true,
      completedContextId: boundedCompletedContextId,
      completionError: boundedCompletionError,
      rendererTracingEnabled: result['rendererTracingEnabled'] === true,
      tracingSupported: result['tracingSupported'] === true,
      startedAtEpochMs: typeof result['startedAtEpochMs'] === 'number' ? result['startedAtEpochMs'] : undefined,
      elapsedMs: typeof result['elapsedMs'] === 'number' ? result['elapsedMs'] : undefined,
      timedOut: result['timedOut'] === true,
      traces: includedTraces,
      captureScope: PROCESS_WIDE_CAPTURE_SCOPE,
      captureTarget: boundedCaptureTarget,
      traceCount: includedTraces.length,
      droppedTraceEventCount,
      traceEventLimitReached: droppedTraceEventCount > 0,
      summary: summarizeTraces(includedTraces),
      perfettoMetadata,
    };
  };

  let minimumTraceCount = 0;
  let maximumTraceCount = traces.length;
  let boundedResult = buildResult(0);
  if (Buffer.byteLength(JSON.stringify(boundedResult), 'utf8') > MAX_TRACE_HTTP_RESPONSE_BYTES) {
    throw new Error('Valdi performance trace response metadata exceeds the HTTP size limit.');
  }
  while (minimumTraceCount <= maximumTraceCount) {
    const candidateTraceCount = Math.floor((minimumTraceCount + maximumTraceCount) / 2);
    const candidate = buildResult(candidateTraceCount);
    if (Buffer.byteLength(JSON.stringify(candidate), 'utf8') <= MAX_TRACE_HTTP_RESPONSE_BYTES) {
      boundedResult = candidate;
      minimumTraceCount = candidateTraceCount + 1;
    } else {
      maximumTraceCount = candidateTraceCount - 1;
    }
  }
  return boundedResult;
}

function saturatingAdd(left: number, right: number): number {
  return left >= Number.MAX_SAFE_INTEGER - right ? Number.MAX_SAFE_INTEGER : left + right;
}

export function readRecordedTraces(value: unknown): RecordedTrace[] {
  if (!Array.isArray(value)) return [];
  const values = value as unknown[];
  const traces: RecordedTrace[] = [];
  const inputCount = Math.min(values.length, MAX_TRACE_EVENT_COUNT);
  for (let index = 0; index < inputCount; index++) {
    const item = values[index];
    if (item === null || typeof item !== 'object' || Array.isArray(item)) continue;
    const candidate = item as Record<string, unknown>;
    const trace = candidate['trace'];
    const startMicros = candidate['startMicros'];
    const endMicros = candidate['endMicros'];
    const threadId = candidate['threadId'];
    if (
      typeof trace !== 'string' ||
      trace.length === 0 ||
      Buffer.byteLength(trace, 'utf8') > MAX_TRACE_NAME_BYTES ||
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
      continue;
    }
    const recordedTrace: RecordedTrace = {
      trace,
      startMicros,
      endMicros,
      threadId,
    };
    traces.push(recordedTrace);
  }
  return traces;
}

export function summarizeTraces(traces: readonly RecordedTrace[]): RendererTraceSummary {
  let durationTraceCount = 0;
  let instantTraceCount = 0;
  const componentDurations = new Map<string, { count: number; durationMicros: number }>();
  const viewModelTriggers = new Map<string, number>();

  for (const trace of traces) {
    if (isRendererViewModelChangeTrace(trace.trace)) {
      instantTraceCount += 1;
    } else {
      durationTraceCount += 1;
    }

    const durationMicros = Math.max(0, trace.endMicros - trace.startMicros);
    const renderMatch = trace.trace.match(/(?:^|\.)Renderer\.onRender\.([^.]+)$/);
    if (renderMatch?.[1]) {
      const componentName = renderMatch[1];
      const componentSummary = componentDurations.get(componentName) ?? { count: 0, durationMicros: 0 };
      componentSummary.count += 1;
      componentSummary.durationMicros += durationMicros;
      componentDurations.set(componentName, componentSummary);
    }

    const triggerMatch = trace.trace.match(/(?:^|\.)Renderer\.viewModelChange\.([^.]+)\.(.+)$/);
    if (triggerMatch?.[1] && triggerMatch[2]) {
      const trigger = `${triggerMatch[1]}.${triggerMatch[2]}`;
      viewModelTriggers.set(trigger, (viewModelTriggers.get(trigger) ?? 0) + 1);
    }
  }

  return {
    captureScope: PROCESS_WIDE_CAPTURE_SCOPE,
    traceCount: traces.length,
    durationTraceCount,
    instantTraceCount,
    topComponents: Array.from(componentDurations.entries())
      .map(([name, value]) => ({
        name,
        count: value.count,
        durationMs: value.durationMicros / 1000,
      }))
      .sort((left, right) => right.durationMs - left.durationMs)
      .slice(0, 12),
    topViewModelTriggers: Array.from(viewModelTriggers.entries())
      .map(([name, count]) => ({ name, count }))
      .sort((left, right) => right.count - left.count)
      .slice(0, 12),
  };
}

export function isRendererViewModelChangeTrace(traceName: string): boolean {
  return /(?:^|\.)Renderer\.viewModelChange\.[^.]+\..+$/.test(traceName);
}

export function buildPerfettoTracePayload(
  traces: readonly RecordedTrace[],
  captureTarget: Record<string, unknown>,
  droppedTraceEventCount: number,
): PerfettoTracePayload {
  const minStartMicros = traces.reduce(
    (minimum, trace) => Math.min(minimum, trace.startMicros),
    traces[0]?.startMicros ?? 0,
  );
  const threadIds = Array.from(new Set(traces.map(trace => trace.threadId)))
    .sort((left, right) => left - right)
    .slice(0, MAX_TRACE_THREAD_METADATA_COUNT);
  const traceEvents: PerfettoTraceEvent[] = [
    {
      name: 'process_name',
      ph: 'M',
      pid: PERFETTO_PROCESS_ID,
      args: { name: PERFETTO_PROCESS_NAME },
    },
  ];

  for (const threadId of threadIds) {
    traceEvents.push({
      name: 'thread_name',
      ph: 'M',
      pid: PERFETTO_PROCESS_ID,
      tid: threadId,
      args: { name: `Valdi thread ${threadId}` },
    });
  }

  for (const trace of traces) {
    const isInstant = isRendererViewModelChangeTrace(trace.trace);
    const event: PerfettoTraceEvent = {
      name: trace.trace,
      cat: PERFETTO_TRACE_CATEGORY,
      ph: isInstant ? 'i' : 'X',
      pid: PERFETTO_PROCESS_ID,
      tid: trace.threadId,
      ts: trace.startMicros - minStartMicros,
    };
    if (isInstant) {
      event.s = 't';
    } else {
      event.dur = Math.max(0, trace.endMicros - trace.startMicros);
    }
    traceEvents.push(event);
  }

  return {
    displayTimeUnit: 'ms',
    metadata: buildPerfettoCaptureMetadata(captureTarget, droppedTraceEventCount),
    traceEvents,
  };
}

function buildPerfettoCaptureMetadata(
  captureTarget: Record<string, unknown>,
  droppedTraceEventCount: number,
): PerfettoCaptureMetadata {
  return {
    captureScope: PROCESS_WIDE_CAPTURE_SCOPE,
    captureTargetContextId: captureTarget['contextId'],
    captureTargetName: captureTarget['name'],
    droppedTraceEventCount,
  };
}

async function dispatchInput(
  request: IncomingMessage,
  searchParams: URLSearchParams,
): Promise<Record<string, unknown>> {
  if (request.method !== 'POST') {
    throw new ApiRequestError(405, 'Input dispatch requires POST.');
  }

  const body = await readJsonBody(request);
  const validationError = validateDebuggerInputRequest(body);
  if (validationError) {
    throw new ApiRequestError(400, validationError);
  }
  const inputType = body['type'] as DebuggerInputType;

  const portValue = searchParams.get('port');
  if (portValue !== null && !/^\d+$/.test(portValue)) {
    throw new ApiRequestError(400, 'Input port must be an integer between 1 and 65535.');
  }
  const port = portValue === null ? STANDALONE_PORT : Number(portValue);
  if (!Number.isInteger(port) || port < 1 || port > 65_535) {
    throw new ApiRequestError(400, 'Input port must be an integer between 1 and 65535.');
  }
  return await withConnection(port, async conn => {
    const target =
      inputType === DebuggerInputType.Capabilities
        ? { ...(await resolveClient(searchParams, conn)), context: undefined }
        : await resolveTarget(searchParams, conn);
    const { client, context } = target;

    const elementLabel = body['elementId'] === undefined ? 'none' : String(body['elementId']);
    console.log(
      `[input] dispatch type=${inputType} element=${elementLabel} client=${client.client_id} context=${context?.id ?? 'none'}`,
    );
    const result = await sendDebuggerInput(conn, client.client_id, {
      ...body,
      ...(context ? { contextId: context.id } : {}),
    });
    console.log(
      `[input] result handled=${String(Boolean(result['handled']))} element=${String(result['elementId'] ?? 'none')} action=${String(result['action'] ?? 'none')}`,
    );

    return {
      port,
      clientId: client.client_id,
      contextId: context?.id,
      input: result,
    };
  });
}

async function inspectProfileContexts(searchParams: URLSearchParams): Promise<Record<string, unknown>> {
  const port = readNumber(searchParams, 'hermesPort', HERMES_PORT);
  const contexts = await listHermesDevices(port);
  return {
    port,
    contexts,
    active: profileStatusPayload(),
  };
}

async function startCpuProfile(
  request: IncomingMessage,
  searchParams: URLSearchParams,
): Promise<Record<string, unknown>> {
  if (request.method !== 'POST') {
    throw new Error('CPU profile start requires POST.');
  }
  return await runProfileTransition(async () => {
    assertNoActiveProfile();
    const body = await readJsonBody(request);
    activeProfileSession = await createProfileSession(searchParams, body);
    return profileStatusPayload();
  });
}

async function stopCpuProfile(request: IncomingMessage): Promise<Record<string, unknown>> {
  if (request.method !== 'POST') {
    throw new Error('CPU profile stop requires POST.');
  }

  return await runProfileTransition(stopActiveProfileSession);
}

async function captureCpuProfile(
  request: IncomingMessage,
  searchParams: URLSearchParams,
): Promise<Record<string, unknown>> {
  if (request.method !== 'POST') {
    throw new Error('CPU profile capture requires POST.');
  }
  return await runProfileTransition(async () => {
    assertNoActiveProfile();
    const body = await readJsonBody(request);
    const durationMs = clampNumber(readBodyNumber(body, 'durationMs', 5000), 100, 60_000);
    activeProfileSession = await createProfileSession(searchParams, body);
    await delay(durationMs);
    return await stopActiveProfileSession();
  });
}

async function runProfileTransition<T>(transition: () => Promise<T>): Promise<T> {
  if (profileTransitionInProgress) {
    throw new Error('Another CPU profile transition is already in progress.');
  }

  profileTransitionInProgress = true;
  try {
    return await transition();
  } finally {
    profileTransitionInProgress = false;
  }
}

function assertNoActiveProfile(): void {
  if (activeProfileSession) {
    throw new Error(`CPU profiling is already active for context ${activeProfileSession.contextId}.`);
  }
}

async function createProfileSession(
  searchParams: URLSearchParams,
  body: Record<string, unknown>,
): Promise<ActiveProfileSession> {
  const port = readBodyNumber(body, 'port', readNumber(searchParams, 'hermesPort', HERMES_PORT));
  const contexts = await listHermesDevices(port);
  if (contexts.length === 0) {
    throw new Error('No debuggable Hermes JS contexts found.');
  }

  const requestedContextId = readBodyString(body, 'contextId') ?? readBodyString(body, 'context');
  const firstContext = contexts[0];
  if (!firstContext) {
    throw new Error('No debuggable Hermes JS contexts found.');
  }
  const context =
    requestedContextId === undefined ? firstContext : contexts.find(candidate => candidate.id === requestedContextId);
  if (!context) {
    throw new Error(`Hermes context ${String(requestedContextId)} was not found.`);
  }

  const conn = await HermesConnection.connect(port, context.id);
  try {
    await conn.startProfiling();
  } catch (error) {
    conn.close();
    throw error;
  }

  return {
    conn,
    port,
    contextId: context.id,
    contextTitle: context.title,
    startedAtMs: Date.now(),
    startedAtEpochMs: Date.now(),
  };
}

async function stopActiveProfileSession(): Promise<Record<string, unknown>> {
  const session = activeProfileSession;
  if (!session) {
    throw new Error('No CPU profile recording is active.');
  }

  activeProfileSession = null;
  try {
    const profile = await session.conn.stopProfiling();
    return {
      profiling: false,
      port: session.port,
      contextId: session.contextId,
      contextTitle: session.contextTitle,
      startedAtEpochMs: session.startedAtEpochMs,
      elapsedMs: Date.now() - session.startedAtMs,
      profile,
      summary: summarizeCpuProfile(profile),
    };
  } finally {
    session.conn.close();
  }
}

function profileStatusPayload(): Record<string, unknown> {
  if (!activeProfileSession) {
    return {
      profiling: false,
    };
  }

  return {
    profiling: true,
    port: activeProfileSession.port,
    contextId: activeProfileSession.contextId,
    contextTitle: activeProfileSession.contextTitle,
    startedAtEpochMs: activeProfileSession.startedAtEpochMs,
    elapsedMs: Date.now() - activeProfileSession.startedAtMs,
  };
}

function summarizeCpuProfile(profile: CpuProfile): Record<string, unknown> {
  const samples = profile.samples ?? [];
  const sampleCountByNodeId = new Map<number, number>();
  for (const sample of samples) {
    sampleCountByNodeId.set(sample, (sampleCountByNodeId.get(sample) ?? 0) + 1);
  }

  const topFunctions = profile.nodes
    .map(node => ({
      name: node.callFrame.functionName || '(anonymous)',
      url: node.callFrame.url,
      lineNumber: node.callFrame.lineNumber,
      sampleCount: sampleCountByNodeId.get(node.id) ?? node.hitCount ?? 0,
    }))
    .filter(node => node.sampleCount > 0)
    .sort((left, right) => right.sampleCount - left.sampleCount)
    .slice(0, 12);

  return {
    nodeCount: profile.nodes.length,
    sampleCount: samples.length,
    durationMs: (profile.endTime - profile.startTime) / 1000,
    topFunctions,
  };
}

async function handleApi(request: IncomingMessage, response: ServerResponse, url: URL): Promise<void> {
  try {
    if (url.pathname === '/api/dev-events') {
      streamDevEvents(request, response);
      return;
    }

    if (url.pathname === '/api/debugger/events') {
      streamDebuggerEvents(request, response);
      return;
    }

    if (url.pathname === '/api/debugger/state') {
      sendJson(response, 200, debuggerStatePayload());
      return;
    }

    if (url.pathname === '/api/debugger/actions') {
      if (request.method !== 'POST') {
        sendJson(response, 405, { error: 'Debugger actions require POST.' });
        return;
      }
      sendJson(response, 200, await handleDebuggerAction(request));
      return;
    }

    if (url.pathname === '/api/runtime-logs/stream') {
      await streamRuntimeLogs(request, response, url.searchParams);
      return;
    }

    if (url.pathname === '/api/status') {
      sendJson(response, 200, await inspectStatus(url.searchParams));
      return;
    }

    if (url.pathname === '/api/devtools/target') {
      if (request.method !== 'GET') {
        sendJson(response, 405, { error: 'Valdi DevTools target discovery requires GET.' });
        return;
      }
      sendJson(response, 200, resolveInspectedWebPreviewTarget(url.searchParams));
      return;
    }

    if (url.pathname === '/api/devtools/snapshot') {
      if (request.method !== 'GET') {
        sendJson(response, 405, { error: 'Valdi DevTools snapshots require GET.' });
        return;
      }
      sendJson(response, 200, await inspectWebPreviewSnapshot(url.searchParams));
      return;
    }

    if (url.pathname === '/api/devtools/evaluate' || url.pathname === '/api/devtools/highlight') {
      if (request.method !== 'POST') {
        sendJson(response, 405, { error: 'Valdi DevTools runtime actions require POST.' });
        return;
      }
      const body = await readJsonBody(request);
      const result = url.pathname.endsWith('/evaluate')
        ? await evaluateWebPreviewConsole(body)
        : await highlightWebPreviewNode(body);
      sendJson(response, 200, result);
      return;
    }

    if (url.pathname === '/api/snapshot') {
      sendJson(response, 200, await inspectSnapshot(url.searchParams));
      return;
    }

    if (url.pathname === '/api/element-snapshot') {
      sendJson(response, 200, await inspectElementSnapshot(url.searchParams));
      return;
    }

    if (url.pathname === '/api/heap') {
      sendJson(response, 200, await inspectHeap(request, url.searchParams));
      return;
    }

    if (url.pathname === '/api/input') {
      sendJson(response, 200, await dispatchInput(request, url.searchParams));
      return;
    }

    if (url.pathname === '/api/performance/trace/status') {
      sendTraceJson(response, 200, await inspectPerformanceTraceStatus(request, url.searchParams));
      return;
    }

    if (url.pathname === '/api/performance/trace/start') {
      sendTraceJson(response, 200, await startPerformanceTrace(request, url.searchParams));
      return;
    }

    if (url.pathname === '/api/performance/trace/stop') {
      sendTraceJson(response, 200, await stopPerformanceTrace(request, url.searchParams));
      return;
    }

    if (url.pathname === '/api/performance/trace/capture') {
      sendTraceJson(response, 200, await capturePerformanceTrace(request, url.searchParams));
      return;
    }

    if (url.pathname === '/api/performance/profile/status') {
      sendJson(response, 200, profileStatusPayload());
      return;
    }

    if (url.pathname === '/api/performance/profile/contexts') {
      sendJson(response, 200, await inspectProfileContexts(url.searchParams));
      return;
    }

    if (url.pathname === '/api/performance/profile/start') {
      sendJson(response, 200, await startCpuProfile(request, url.searchParams));
      return;
    }

    if (url.pathname === '/api/performance/profile/stop') {
      sendJson(response, 200, await stopCpuProfile(request));
      return;
    }

    if (url.pathname === '/api/performance/profile/capture') {
      sendJson(response, 200, await captureCpuProfile(request, url.searchParams));
      return;
    }

    if (url.pathname === '/api/runtime-logs') {
      sendJson(response, 200, await inspectRuntimeLogs(url.searchParams));
      return;
    }

    sendJson(response, 404, { error: `Unknown API route ${url.pathname}` });
  } catch (error) {
    const status = error instanceof ApiRequestError ? error.statusCode : 500;
    const payload = clientErrorPayload(error);
    if (url.pathname.startsWith('/api/performance/trace/')) {
      sendTraceJson(response, status, payload);
    } else {
      sendJson(response, status, payload);
    }
  }
}

async function serveStatic(_request: IncomingMessage, response: ServerResponse, url: URL): Promise<void> {
  let requestedPath: string;
  try {
    requestedPath = decodeURIComponent(url.pathname === '/' ? '/index.html' : url.pathname);
  } catch {
    response.writeHead(400);
    response.end('Invalid URL encoding');
    return;
  }

  const resolvedPath = path.resolve(assetRoot, requestedPath.replace(/^\/+/, ''));
  const relativePath = path.relative(assetRoot, resolvedPath);
  if (relativePath === '..' || relativePath.startsWith(`..${path.sep}`) || path.isAbsolute(relativePath)) {
    response.writeHead(403);
    response.end('Forbidden');
    return;
  }

  try {
    const data = await fs.readFile(resolvedPath);
    const ext = path.extname(resolvedPath);
    const isDevToolsPanel = requestedPath === '/devtools-panel.html';
    const headers: Record<string, string> = {
      'Content-Type': MIME_TYPES[ext] ?? 'application/octet-stream',
      'Cache-Control': 'no-store',
      'Content-Security-Policy': `default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; frame-src 'none'; object-src 'none'; base-uri 'none'; frame-ancestors ${isDevToolsPanel ? 'chrome-extension://*' : "'none'"}`,
      'X-Content-Type-Options': 'nosniff',
    };
    if (!isDevToolsPanel) headers['X-Frame-Options'] = 'DENY';
    response.writeHead(200, headers);
    response.end(data);
  } catch {
    response.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
    response.end(`Not found: ${url.pathname}`);
  }
}

function createDebuggerHttpServer(host: string, port: number): Server {
  return http.createServer(async (request, response) => {
    const url = new URL(request.url || '/', `http://${hostForUrl(host)}:${port}`);
    const apiFetchSiteAllowed =
      !url.pathname.startsWith('/api/') || isAllowedApiFetchSite(request.headers['sec-fetch-site']);
    if (
      !isAllowedRequestHost(request.headers.host, port) ||
      !isAllowedRequestOrigin(request.headers.origin, request.headers.host, port) ||
      !apiFetchSiteAllowed
    ) {
      response.writeHead(403, { 'Content-Type': 'text/plain; charset=utf-8' });
      response.end('Forbidden');
      return;
    }

    if (url.pathname.startsWith('/api/')) {
      if (url.pathname.startsWith('/api/devtools/') && request.method === 'POST') {
        const contentType = request.headers['content-type']?.split(';')[0]?.trim().toLowerCase();
        if (contentType !== 'application/json') {
          sendJson(response, 415, { error: 'Valdi DevTools actions require an application/json request.' });
          return;
        }
      }
      await handleApi(request, response, url);
      return;
    }

    await serveStatic(request, response, url);
  });
}

function listen(server: Server, host: string, port: number): Promise<void> {
  return new Promise<void>((resolve, reject) => {
    const onError = (error: Error) => {
      server.off('listening', onListening);
      reject(error);
    };
    const onListening = () => {
      server.off('error', onError);
      resolve();
    };
    server.once('error', onError);
    server.once('listening', onListening);
    server.listen(port, host);
  });
}

async function watchDebuggerAssets(root: string): Promise<void> {
  let fileNames: string[] = [];
  try {
    const entries = await fs.readdir(root, { withFileTypes: true });
    fileNames = entries
      .filter(entry => entry.isFile() && /\.(?:html|css|js)$/.test(entry.name))
      .map(entry => entry.name)
      .sort();
  } catch (error) {
    console.warn(`Could not list debugger assets in ${root}: ${errorPayload(error).error}`);
  }

  if (!fileNames.includes('index.html')) fileNames.push('index.html');

  for (const fileName of fileNames) {
    const watchedPath = path.join(root, fileName);
    try {
      const watcher = watchFileSystem(watchedPath, { persistent: false }, () => {
        scheduleDevReload(fileName);
      });
      assetWatchers.add(watcher);
      watcher.once('close', () => assetWatchers.delete(watcher));
    } catch (error) {
      console.warn(`Could not watch ${watchedPath}: ${errorPayload(error).error}`);
    }
  }
}

function closeAssetWatchers(): void {
  for (const watcher of Array.from(assetWatchers)) {
    watcher.close();
  }
  assetWatchers.clear();
}

async function closeHttpServer(server: Server): Promise<void> {
  await new Promise<void>((resolve, reject) => {
    server.close(error => (error ? reject(error) : resolve()));
  });
}

async function closeDebuggerServer(server: Server): Promise<void> {
  for (const closeStream of Array.from(eventStreamClosers)) {
    closeStream();
  }
  closeAssetWatchers();
  if (devReloadTimer) {
    clearTimeout(devReloadTimer);
    devReloadTimer = null;
  }
  // Wait for in-flight profile setup/capture requests before inspecting the
  // active session so shutdown cannot orphan a session created late.
  await closeHttpServer(server);
  if (activeProfileSession) {
    try {
      await stopActiveProfileSession();
    } catch (error) {
      console.warn(`Could not stop the active Hermes profile during shutdown: ${errorPayload(error).error}`);
    }
  }
  activeWebPreviewTarget = null;
}

export async function startDebuggerServer(options: DebuggerServerOptions): Promise<DebuggerServerInfo> {
  activeHost = options.host ?? DEFAULT_HOST;
  if (!isLoopbackHost(activeHost)) {
    throw new Error(`The debugger server only binds to loopback hosts; received '${activeHost}'.`);
  }

  assetRoot = options.assetRoot ?? getDefaultAssetRoot();
  activeLogsDirectory = options.logsDirectory ?? null;
  const preferredPort = options.port ?? DEFAULT_PORT;
  if (!Number.isInteger(preferredPort) || preferredPort < 1 || preferredPort > 65_535) {
    throw new Error(`Debugger port must be an integer between 1 and 65535; received '${String(preferredPort)}'.`);
  }
  const previousWebPreviewTarget = activeWebPreviewTarget;
  activeWebPreviewTarget = createWebPreviewDebuggerTarget(
    options.webPreviewUrl,
    options.chromiumDebuggingPort ?? DEFAULT_CHROMIUM_DEBUGGING_PORT,
  );
  const strictPort = Boolean(options.strictPort);
  const maxAttempts = strictPort ? 1 : PORT_SEARCH_LIMIT;

  try {
    for (let attempt = 0; attempt < maxAttempts; attempt += 1) {
      const port = preferredPort + attempt;
      const server = createDebuggerHttpServer(activeHost, port);
      try {
        await listen(server, activeHost, port);
        closeAssetWatchers();
        await watchDebuggerAssets(assetRoot);
        let closePromise: Promise<void> | undefined;
        return {
          server,
          close: () => {
            closePromise ??= closeDebuggerServer(server);
            return closePromise;
          },
          host: activeHost,
          port,
          url: `http://${hostForUrl(activeHost)}:${port}/`,
          requestedPort: preferredPort,
          portWasAutoSelected: port !== preferredPort,
        };
      } catch (error) {
        server.close();
        const err = error as NodeJS.ErrnoException;
        if (strictPort || err.code !== 'EADDRINUSE') {
          throw error;
        }
      }
    }

    throw new Error(`No available port found in ${preferredPort}-${preferredPort + maxAttempts - 1}.`);
  } catch (error) {
    activeWebPreviewTarget = previousWebPreviewTarget;
    throw error;
  }
}
