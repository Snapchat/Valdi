// Shared app state, DOM handles, and small argument helpers.
const AUTO_REFRESH_INTERVAL_MS = 2500;
const MAX_RUNTIME_LOG_ROWS = 400;
const UI_SECTION = 'ui';
const STANDALONE_DAEMON_PORT = 13591;
const MOBILE_DAEMON_PORT = 13592;

const emptyTarget = {
  id: '',
  name: 'No target attached',
  platform: 'n/a',
  transport: '',
  state: 'idle',
  proxyPort: STANDALONE_DAEMON_PORT,
};

function createEmptySnapshot() {
  return {
    target: { ...emptyTarget },
    targets: [],
    tree: null,
    issues: [],
    logs: [],
  };
}

const state = {
  snapshot: createEmptySnapshot(),
  selectedNodeId: null,
  activeSection: 'ui',
  activeTab: 'overview',
  overlayMode: 'live',
  attached: false,
  source: 'empty',
  lastStatus: null,
  lastError: null,
  selectedSnapshotImage: null,
  geometry: null,
  autoRefresh: false,
  autoRefreshTimer: null,
  refreshInFlight: false,
  refreshStartedAt: null,
  pendingRefreshRequest: null,
  lastUpdated: null,
  expandedNodeIds: new Set(),
  runtimeLogStream: null,
  runtimeLogStreamKey: null,
  runtimeLogPath: null,
  debuggerEventStream: null,
  debuggerEventsConnected: false,
  lastDebuggerRevision: 0,
  rootSnapshotImage: null,
  rootSnapshotRequestId: 0,
  inputRefreshTimers: new Map(),
  manualDetach: false,
  followLatestTarget: true,
  exportObjectUrl: null,
  performance: {
    traceActive: false,
    traceCapturePending: false,
    traceResultPending: false,
    traceStateUnknown: false,
    activeTraceTarget: null,
    traceSupported: true,
    lastTrace: null,
    profileActive: false,
    activeProfileContextId: null,
    lastProfile: null,
    profileContexts: [],
  },
  providers: {
    activeTab: 'storage',
    error: null,
    generation: 0,
    loading: false,
    registry: null,
    selectedDatabaseId: null,
    selectedTable: null,
    sql: null,
    sqlLimit: 50,
    sqlLoading: false,
    sqlOffset: 0,
    sqlTable: null,
    storage: null,
    targetKey: null,
  },
  settings: {
    error: null,
    generation: 0,
    loading: false,
    selectedGroupId: null,
    snapshot: null,
    targetKey: null,
  },
};

const elements = {
  sectionButtons: Array.from(document.querySelectorAll('[data-section-button]')),
  sectionPanels: Array.from(document.querySelectorAll('[data-section-panel]')),
  sessionSubtitle: document.getElementById('sessionSubtitle'),
  targetList: document.getElementById('targetList'),
  daemonStatus: document.getElementById('daemonStatus'),
  tree: document.getElementById('tree'),
  hierarchySource: document.getElementById('hierarchySource'),
  htmlPreviewRoot: document.getElementById('htmlPreviewRoot'),
  rootSnapshotImage: document.getElementById('rootSnapshotImage'),
  appPreview: document.getElementById('appPreview'),
  previewStage: document.getElementById('previewStage'),
  device: document.getElementById('deviceFrame'),
  screen: document.getElementById('screen'),
  inspector: document.getElementById('inspector'),
  selectedSummary: document.getElementById('selectedSummary'),
  logs: document.getElementById('logs'),
  treeSearch: document.getElementById('treeSearch'),
  logSearch: document.getElementById('logSearch'),
  exportPanel: document.getElementById('exportPanel'),
  exportTitle: document.getElementById('exportTitle'),
  exportOpenLink: document.getElementById('exportOpenLink'),
  exportText: document.getElementById('exportText'),
  copyExportButton: document.getElementById('copyExportButton'),
  closeExportButton: document.getElementById('closeExportButton'),
  reloaderDot: document.getElementById('reloaderDot'),
  reloaderState: document.getElementById('reloaderState'),
  proxyDot: document.getElementById('proxyDot'),
  proxyState: document.getElementById('proxyState'),
  attachButton: document.getElementById('attachButton'),
  portSelect: document.getElementById('portSelect'),
  snapshotButton: document.getElementById('snapshotButton'),
  copyPreviewButton: document.getElementById('copyPreviewButton'),
  autoRefreshToggle: document.getElementById('autoRefreshToggle'),
  rendererTracingToggle: document.getElementById('rendererTracingToggle'),
  traceDurationInput: document.getElementById('traceDurationInput'),
  traceStartButton: document.getElementById('traceStartButton'),
  traceStopButton: document.getElementById('traceStopButton'),
  traceCaptureButton: document.getElementById('traceCaptureButton'),
  traceExportButton: document.getElementById('traceExportButton'),
  traceStatusPill: document.getElementById('traceStatusPill'),
  traceSummary: document.getElementById('traceSummary'),
  traceEvents: document.getElementById('traceEvents'),
  profileContextSelect: document.getElementById('profileContextSelect'),
  profileRefreshButton: document.getElementById('profileRefreshButton'),
  profileDurationInput: document.getElementById('profileDurationInput'),
  profileStartButton: document.getElementById('profileStartButton'),
  profileStopButton: document.getElementById('profileStopButton'),
  profileCaptureButton: document.getElementById('profileCaptureButton'),
  profileExportButton: document.getElementById('profileExportButton'),
  profileStatusPill: document.getElementById('profileStatusPill'),
  profileSummary: document.getElementById('profileSummary'),
  providerContent: document.getElementById('providerContent'),
  providerRefreshButton: document.getElementById('providerRefreshButton'),
  providerStatusPill: document.getElementById('providerStatusPill'),
  settingsContent: document.getElementById('settingsContent'),
  settingsGroupSelect: document.getElementById('settingsGroupSelect'),
  settingsRefreshButton: document.getElementById('settingsRefreshButton'),
  settingsStatusPill: document.getElementById('settingsStatusPill'),
};

function hasSnapshotTree(snapshot = state.snapshot) {
  return Boolean(snapshot?.tree);
}

function actionString(params, ...keys) {
  for (const key of keys) {
    const value = params?.[key];
    if (value !== undefined && value !== null && value !== '') return String(value);
  }
  return null;
}

function actionBoolean(params, key, fallback) {
  const value = params?.[key];
  if (value === undefined || value === null || value === '') return fallback;
  if (typeof value === 'boolean') return value;
  const normalized = String(value).toLowerCase();
  if (['1', 'true', 'yes', 'on'].includes(normalized)) return true;
  if (['0', 'false', 'no', 'off'].includes(normalized)) return false;
  return Boolean(value);
}

function actionNumber(params, key) {
  const value = params?.[key];
  if (value === undefined || value === null || value === '') return null;
  const parsed = Number.parseInt(String(value), 10);
  return Number.isNaN(parsed) ? null : parsed;
}

function numericPortValue(value) {
  if (value === undefined || value === null || value === '') return null;
  const parsed = Number.parseInt(String(value), 10);
  return Number.isNaN(parsed) ? null : parsed;
}

function selectedDaemonPort(fallback = STANDALONE_DAEMON_PORT) {
  return (
    numericPortValue(elements.portSelect.value) ??
    numericPortValue(state.snapshot.target?.proxyPort) ??
    numericPortValue(state.snapshot.target?.port) ??
    fallback
  );
}

function debuggerTargets() {
  const targets = [...(state.snapshot.targets || [])];
  const targetPorts = new Set(targets.map(target => Number(target.port || target.proxyPort)).filter(Number.isFinite));
  for (const portStatus of state.lastStatus?.ports || []) {
    if (targetPorts.has(Number(portStatus.port))) continue;
    targets.push({
      id: `daemon:${portStatus.port}`,
      name: `${portStatus.portName || 'Valdi'} daemon`,
      platform: portStatus.portName || 'native',
      state: portStatus.connected ? 'waiting' : 'offline',
      transport: `daemon:${portStatus.port}`,
      port: portStatus.port,
      proxyPort: portStatus.port,
      daemonPort: true,
    });
  }
  return targets;
}
