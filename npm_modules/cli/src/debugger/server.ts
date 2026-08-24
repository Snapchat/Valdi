import { watch as watchFileSystem } from 'node:fs';
import type { FSWatcher, Stats } from 'node:fs';
import fs from 'node:fs/promises';
import type { IncomingMessage, Server, ServerResponse } from 'node:http';
import http from 'node:http';
import net from 'node:net';
import path from 'node:path';
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

const DEFAULT_HOST = process.env['VALDI_DEBUGGER_HOST'] || '127.0.0.1';
const DEFAULT_PORT = Number.parseInt(process.env['VALDI_DEBUGGER_PORT'] || '8765', 10);
const HOT_RELOAD_PROXY_PORT = Number.parseInt(process.env['VALDI_HOT_RELOAD_PROXY_PORT'] || '9010', 10);
const PORT_SEARCH_LIMIT = 50;
const MAX_RUNTIME_LOG_READ_BYTES = 1024 * 1024;

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
let activeProfileSession: ActiveProfileSession | null = null;
let profileTransitionInProgress = false;
const debuggerUiState = createDebuggerUiState();

function getDefaultAssetRoot(): string {
  // The published CLI is emitted as CommonJS, so __dirname is the reliable package-relative anchor.
  // eslint-disable-next-line unicorn/prefer-module
  return path.resolve(__dirname, '..', '..', 'debugger');
}

function normalizedHostname(hostname: string): string {
  return hostname.startsWith('[') && hostname.endsWith(']') ? hostname.slice(1, -1) : hostname;
}

function isLoopbackHost(host: string): boolean {
  const normalizedHost = normalizedHostname(host);
  return (
    normalizedHost === 'localhost' ||
    normalizedHost === '::1' ||
    (net.isIP(normalizedHost) === 4 && normalizedHost.startsWith('127.'))
  );
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

function errorPayload(error: unknown): { error: string } {
  return {
    error: error instanceof Error ? error.message : String(error),
  };
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

function sendJson(response: ServerResponse, status: number, payload: unknown): void {
  response.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
  });
  response.end(JSON.stringify(payload));
}

async function readJsonBody(request: IncomingMessage): Promise<Record<string, unknown>> {
  const chunks: Buffer[] = [];
  let byteLength = 0;
  for await (const chunk of request) {
    const buffer = Buffer.isBuffer(chunk) ? chunk : Buffer.from(String(chunk));
    byteLength += buffer.length;
    if (byteLength > 1024 * 1024) {
      throw new Error('Request body is too large.');
    }
    chunks.push(buffer);
  }

  const raw = Buffer.concat(chunks, byteLength).toString('utf8');
  if (!raw.trim()) return {};
  const parsed = JSON.parse(raw) as unknown;
  if (typeof parsed !== 'object' || parsed === null || Array.isArray(parsed)) {
    throw new Error('Request body must be a JSON object.');
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
      sendSse(response, 'stream-error', errorPayload(error));
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
        contextError = errorPayload(error).error;
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
      error: errorPayload(error).error,
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
      tree,
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
    sendJson(response, error instanceof ApiRequestError ? error.statusCode : 500, errorPayload(error));
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
    response.writeHead(200, {
      'Content-Type': MIME_TYPES[ext] ?? 'application/octet-stream',
      'Cache-Control': 'no-store',
      'Content-Security-Policy':
        "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; frame-src 'none'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'",
      'X-Content-Type-Options': 'nosniff',
      'X-Frame-Options': 'DENY',
    });
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
  const strictPort = Boolean(options.strictPort);
  const maxAttempts = strictPort ? 1 : PORT_SEARCH_LIMIT;

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
}
