const query = new URLSearchParams(window.location.search);
const MAX_REGISTRY_CAPABILITIES = 32;
const MAX_REGISTRY_ENTRIES = 256;
const MAX_REGISTRY_ID_CHARACTERS = 512;
const MAX_REGISTRY_LABEL_CHARACTERS = 96;
const MAX_REGISTRY_LABEL_TOTAL_CHARACTERS = 180;
const MAX_CONSOLE_ENTRIES = 500;
const MAX_CONSOLE_ENTRY_CHARACTERS = 50_000;
const MAX_CONSOLE_HISTORY_ENTRIES = 100;
const MAX_PERFORMANCE_SAMPLES = 120;
const MAX_PERFORMANCE_TIMELINE_ROWS = 120;
const MAX_PERFORMANCE_SUMMARY_ROWS = 12;
const MAX_RUNTIME_STATE_CHARACTERS = 65_536;
const MAX_RUNTIME_STATE_COMPONENT_ROWS = 500;
const MAX_RUNTIME_STATE_CONTAINER_ENTRIES = 100;
const MAX_RUNTIME_STATE_DEPTH = 12;
const MAX_RUNTIME_STATE_KEY_CHARACTERS = 1_024;
const MAX_RUNTIME_STATE_TOKENS = 2_000;
const MAX_RUNTIME_STATE_VALUE_ROWS = 1_000;
const RUNTIME_STATE_SOURCE_NATIVE = 'native';
const RUNTIME_STATE_SOURCE_WEB = 'web';
const COMPONENT_PROPERTY_TOKEN_PATTERN = /^[0-9a-f]{32}$/;
const COMPONENT_PROPERTY_EDIT_ERROR = 'The component property edit is stale or invalid.';
const COMPONENT_PROPERTY_EDIT_PROTOCOL_VERSION = 1;
const SNAPSHOT_REFRESH_APPLIED = 'applied';
const SNAPSHOT_REFRESH_RUNTIME_STATE_DEFERRED = 'runtime-state-deferred';
const SNAPSHOT_REFRESH_SKIPPED = 'skipped';
const FORBIDDEN_COMPONENT_PROPERTY_NAMES = new Set(['__proto__', 'children', 'constructor', 'prototype']);
const componentPropertyEditorBindings = new WeakMap();
const runtimeStateInspectBindings = new WeakMap();
const runtimeStateParseCache = new WeakMap();
const MANUAL_WEB_CAPABILITIES = new Set([
  'component-properties',
  'components',
  'console',
  'highlight',
  'performance',
  'snapshot',
  'storage',
]);

function parseLaunchIdentity(searchParams) {
  const targetIds = searchParams.getAll('targetId');
  const inspectedUrls = searchParams.getAll('inspectedUrl');
  const targetNonces = searchParams.getAll('targetNonce');
  if (targetIds.length > 1 || inspectedUrls.length > 1 || targetNonces.length > 1) {
    return { error: 'The DevTools URL contains a duplicated target identity.' };
  }

  const targetId = targetIds[0];
  const inspectedPageUrl = inspectedUrls[0];
  const targetNonce = targetNonces[0];
  if (targetId !== undefined) {
    if (
      targetId.length === 0 ||
      targetId.length > MAX_REGISTRY_ID_CHARACTERS ||
      /[\u0000-\u001f\u007f]/.test(targetId) ||
      inspectedPageUrl !== undefined ||
      targetNonce !== undefined
    ) {
      return { error: 'Direct DevTools requires one bounded targetId and no inspected-page identity.' };
    }
    return { mode: 'target-id', requestedTargetId: targetId };
  }

  if (
    inspectedPageUrl === undefined ||
    inspectedPageUrl.length === 0 ||
    targetNonce === undefined ||
    targetNonce.length === 0
  ) {
    return { error: 'Inspected-page DevTools requires both inspectedUrl and targetNonce.' };
  }
  return { inspectedUrl: inspectedPageUrl, mode: 'inspected-page', targetNonce };
}

const launchIdentity = parseLaunchIdentity(query);
const inspectedUrl = launchIdentity.mode === 'inspected-page' ? launchIdentity.inspectedUrl : null;
const inspectedTargetNonce = launchIdentity.mode === 'inspected-page' ? launchIdentity.targetNonce : null;

const state = {
  target: null,
  targetGeneration: 0,
  targetSwitchMessage: null,
  registryTargets: [],
  registryError: null,
  registryGeneration: 0,
  registryRequestGeneration: 0,
  registryPending: false,
  connectionRequestGeneration: 0,
  initialTargetResolutionPending: launchIdentity.mode === 'target-id',
  unavailableTargetId: null,
  snapshot: null,
  activeSection: 'elements',
  activeDetail: 'styles',
  selectedNodeId: null,
  remoteSelectedNodeId: null,
  expandedNodeIds: new Set(),
  search: '',
  autoRefresh: true,
  refreshTimer: null,
  refreshPending: false,
  snapshotRequestCompletion: null,
  snapshotGeneration: 0,
  snapshotRequestGeneration: 0,
  hoveredNodeId: null,
  hoveredSnapshotGeneration: 0,
  highlightTimer: null,
  highlightIntentGeneration: 0,
  highlightMayBeActive: false,
  highlightRequestTail: Promise.resolve(),
  consoleEntries: [],
  consoleEntryKeys: new Set(),
  consoleHistory: [],
  consoleHistoryIndex: 0,
  consoleStream: null,
  consoleStreamTargetKey: null,
  performance: {
    data: null,
    durationSeconds: 3,
    error: null,
    lastTrace: null,
    navigationExpanded: false,
    operationGeneration: 0,
    ownerIdentity: null,
    pending: false,
    rendererTracingEnabled: false,
    requestGeneration: 0,
    samples: [],
    snapshotPending: false,
    traceActive: false,
    traceScope: 'valdi',
    traceSearch: '',
  },
  componentPropertyEdit: {
    error: null,
    focused: false,
    operationGeneration: 0,
    pending: false,
  },
  runtimeState: {
    expandedComponents: new Set(),
    expandedInspectorValues: new Set(),
    expandedMainValues: new Set(),
    inspectGeneration: 0,
    inspectorNodeId: null,
    search: '',
  },
  error: null,
};

const elements = {
  mainTabs: Array.from(document.querySelectorAll('.main-tab')),
  detailTabs: Array.from(document.querySelectorAll('.detail-tab')),
  sections: Array.from(document.querySelectorAll('.section')),
  targetStatusDot: document.getElementById('targetStatusDot'),
  targetSelectLabel: document.getElementById('targetSelectLabel'),
  targetSelect: document.getElementById('targetSelect'),
  targetName: document.getElementById('targetName'),
  targetMetadata: document.getElementById('targetMetadata'),
  targetPickerStatus: document.getElementById('targetPickerStatus'),
  autoRefreshToggle: document.getElementById('autoRefreshToggle'),
  refreshButton: document.getElementById('refreshButton'),
  treeFilter: document.getElementById('treeFilter'),
  filterSummary: document.getElementById('filterSummary'),
  tree: document.getElementById('tree'),
  treeEmpty: document.getElementById('treeEmpty'),
  expandButton: document.getElementById('expandButton'),
  inspector: document.getElementById('inspector'),
  copyNodeButton: document.getElementById('copyNodeButton'),
  breadcrumbs: document.getElementById('breadcrumbs'),
  nodeSummary: document.getElementById('nodeSummary'),
  elementsSection: document.getElementById('elementsSection'),
  splitHandle: document.getElementById('splitHandle'),
  clearConsoleButton: document.getElementById('clearConsoleButton'),
  consoleMessages: document.getElementById('consoleMessages'),
  consoleForm: document.getElementById('consoleForm'),
  consoleInput: document.getElementById('consoleInput'),
  performanceContent: document.getElementById('performanceContent'),
  stateContent: document.getElementById('stateContent'),
  stateFilter: document.getElementById('stateFilter'),
  stateSection: document.getElementById('stateSection'),
  stateSummary: document.getElementById('stateSummary'),
};

function escapeHtml(value) {
  return String(value ?? '')
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}

function applyTheme(theme) {
  document.documentElement.dataset.theme = theme === 'dark' ? 'dark' : 'light';
}

async function requestJson(path, params, options) {
  const url = new URL(path, window.location.origin);
  for (const [key, value] of Object.entries(params)) {
    if (value !== undefined && value !== null && value !== '') {
      url.searchParams.set(key, String(value));
    }
  }

  const response = await fetch(url, {
    method: options.body ? 'POST' : 'GET',
    cache: 'no-store',
    headers: debuggerApiHeaders(options.body ? { 'Content-Type': 'application/json' } : {}),
    ...(options.body ? { body: JSON.stringify(options.body) } : {}),
  });
  const payload = await response.json();
  if (!response.ok || payload.error) {
    throw new Error(payload.error || `Request failed: ${response.status}`);
  }
  return payload;
}

function isDirectMode() {
  return launchIdentity.mode === 'target-id';
}

function targetCapabilities(target = state.target) {
  if (Array.isArray(target?.capabilities)) return new Set(target.capabilities);
  return launchIdentity.mode === 'inspected-page' ? MANUAL_WEB_CAPABILITIES : new Set();
}

function targetSupports(capability, target = state.target) {
  return Boolean(target && targetCapabilities(target).has(capability));
}

function targetIdentityParameters(target = state.target) {
  if (!target) throw new Error('No debugger target is selected.');
  if (isDirectMode()) return { targetId: target.id };
  if (!target.sessionId || !inspectedUrl || !inspectedTargetNonce) {
    throw new Error('The selected inspected page does not have a complete target identity.');
  }
  return {
    inspectedUrl,
    sessionId: target.sessionId,
    targetNonce: inspectedTargetNonce,
  };
}

function targetIdentityKey(target = state.target) {
  const identity = targetIdentityParameters(target);
  return Object.keys(identity)
    .sort()
    .map(key => `${key}:${identity[key]}`)
    .join('|');
}

function targetOperationalKey(target) {
  return `${targetIdentityKey(target)}|capabilities:${JSON.stringify(Array.from(targetCapabilities(target)).sort())}`;
}

function targetEventMatches(target, payload) {
  if (!target || payload.targetId !== target.id) return false;
  return isDirectMode() || payload.sessionId === target.sessionId;
}

function boundedRegistryLabel(value, fallback) {
  if (typeof value !== 'string') return fallback;
  const normalized = value
    .slice(0, MAX_REGISTRY_LABEL_CHARACTERS * 2)
    .replace(/[\u0000-\u001f\u007f]+/g, ' ')
    .trim();
  if (!normalized) return fallback;
  const characters = Array.from(normalized);
  return characters.length <= MAX_REGISTRY_LABEL_CHARACTERS
    ? normalized
    : `${characters.slice(0, MAX_REGISTRY_LABEL_CHARACTERS - 1).join('')}…`;
}

function parseRegistryTarget(value) {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) return null;
  const id = value.id;
  if (
    typeof id !== 'string' ||
    id.length === 0 ||
    id.length > MAX_REGISTRY_ID_CHARACTERS ||
    /[\u0000-\u001f\u007f]/.test(id) ||
    typeof value.attachable !== 'boolean' ||
    !['inspected-page', 'target-id'].includes(value.identityMode) ||
    typeof value.transport !== 'string' ||
    value.transport.length === 0 ||
    value.transport.length > 64 ||
    !Array.isArray(value.capabilities) ||
    value.capabilities.length > MAX_REGISTRY_CAPABILITIES
  ) {
    return null;
  }

  const capabilities = [];
  const seenCapabilities = new Set();
  for (const capability of value.capabilities) {
    if (
      typeof capability !== 'string' ||
      capability.length === 0 ||
      capability.length > 64 ||
      /[\u0000-\u001f\u007f]/.test(capability) ||
      seenCapabilities.has(capability)
    ) {
      return null;
    }
    seenCapabilities.add(capability);
    capabilities.push(capability);
  }

  if (value.state !== undefined && !['attached', 'available', 'waiting'].includes(value.state)) return null;
  if (value.platform !== undefined && (typeof value.platform !== 'string' || value.platform.length > 32)) return null;
  if (value.port !== undefined && (!Number.isSafeInteger(value.port) || value.port <= 0 || value.port > 65_535)) {
    return null;
  }
  const name = boundedRegistryLabel(value.name, '');
  return {
    attachable: value.attachable,
    capabilities,
    id,
    identityMode: value.identityMode,
    name: name || boundedRegistryLabel(value.applicationId, 'Valdi target'),
    platform: boundedRegistryLabel(value.platform, 'unknown'),
    ...(value.port === undefined ? {} : { port: value.port }),
    state: value.state || 'available',
    transport: value.transport,
  };
}

function parseTargetRegistry(payload) {
  if (typeof payload !== 'object' || payload === null || Array.isArray(payload) || !Array.isArray(payload.targets)) {
    throw new Error('The debugger target registry returned an invalid response.');
  }
  if (payload.targets.length > MAX_REGISTRY_ENTRIES) {
    throw new Error(`The debugger target registry exceeded ${MAX_REGISTRY_ENTRIES} entries.`);
  }
  const targets = [];
  const ids = new Set();
  for (const value of payload.targets) {
    const target = parseRegistryTarget(value);
    if (!target) throw new Error('The debugger target registry contains a malformed entry.');
    if (ids.has(target.id)) throw new Error(`The debugger target registry contains duplicate target IDs.`);
    ids.add(target.id);
    targets.push(target);
  }
  return targets;
}

function isSelectableDirectTarget(target) {
  const capabilities = targetCapabilities(target);
  return Boolean(
    target.attachable &&
      target.state !== 'waiting' &&
      target.identityMode === 'target-id' &&
      target.transport === 'valdi-daemon' &&
      capabilities.has('components') &&
      capabilities.has('snapshot'),
  );
}

function directTargetUnavailableReason(target) {
  if (target.identityMode === 'inspected-page') return 'Open from the inspected page';
  if (!target.attachable || target.state === 'waiting') return 'Waiting for application';
  if (target.transport !== 'valdi-daemon') return 'Unsupported transport';
  if (!targetSupports('components', target) || !targetSupports('snapshot', target)) {
    return 'Component snapshots unavailable';
  }
  return 'Unavailable';
}

function directTargetLabel(target) {
  const metadata = [target.platform, target.port === undefined ? null : `:${target.port}`].filter(Boolean).join(' ');
  const reason = isSelectableDirectTarget(target) ? '' : ` — ${directTargetUnavailableReason(target)}`;
  const label = `${target.name}${metadata ? ` (${metadata})` : ''}${reason}`;
  const characters = Array.from(label);
  return characters.length <= MAX_REGISTRY_LABEL_TOTAL_CHARACTERS
    ? label
    : `${characters.slice(0, MAX_REGISTRY_LABEL_TOTAL_CHARACTERS - 1).join('')}…`;
}

function appendTargetOption(value, label, options = {}) {
  const option = document.createElement('option');
  option.value = value;
  option.textContent = label;
  option.disabled = Boolean(options.disabled);
  option.selected = Boolean(options.selected);
  elements.targetSelect.append(option);
}

function setTargetPickerStatus(message) {
  elements.targetPickerStatus.textContent = message || '';
}

function renderTargetPicker() {
  const direct = isDirectMode();
  elements.targetSelect.hidden = !direct;
  elements.targetSelectLabel.hidden = !direct;
  elements.targetName.hidden = direct;
  if (!direct) return;

  elements.targetSelect.replaceChildren();
  const selectedId = state.target?.id || null;
  const unavailableTarget = state.unavailableTargetId
    ? state.registryTargets.find(target => target.id === state.unavailableTargetId)
    : null;
  appendTargetOption('', state.registryPending ? 'Loading debugger targets…' : 'Choose a debugger target', {
    disabled: false,
    selected: selectedId === null,
  });
  for (const target of state.registryTargets) {
    appendTargetOption(target.id, directTargetLabel(target), {
      disabled: !isSelectableDirectTarget(target),
      selected: target.id === selectedId,
    });
  }
  if (state.unavailableTargetId && !unavailableTarget) {
    appendTargetOption(state.unavailableTargetId, 'Requested target is unavailable', { disabled: true });
  }
  elements.targetSelect.value = selectedId || '';
  if (state.targetSwitchMessage) {
    setTargetPickerStatus(state.targetSwitchMessage);
  } else if (state.registryError) {
    setTargetPickerStatus(state.registryError);
  } else if (state.error) {
    setTargetPickerStatus(state.error);
  } else if (state.target) {
    setTargetPickerStatus(`Connected to ${state.target.name}.`);
  } else if (state.unavailableTargetId) {
    setTargetPickerStatus(
      unavailableTarget && isSelectableDirectTarget(unavailableTarget)
        ? 'The requested target is available. Select it to connect.'
        : unavailableTarget
          ? directTargetUnavailableReason(unavailableTarget)
          : 'The requested target is unavailable. Choose another target.',
    );
  } else {
    setTargetPickerStatus('Choose a target. Targets are never selected automatically.');
  }
}

function stopPerformanceOnPageHide() {
  const identity = state.performance.ownerIdentity;
  if (!identity) return;
  const url = new URL('/api/devtools/performance/trace/stop', window.location.origin);
  for (const [key, value] of Object.entries(identity)) url.searchParams.set(key, String(value));
  void fetch(url, {
    body: '{}',
    headers: debuggerApiHeaders({ 'Content-Type': 'application/json' }),
    keepalive: true,
    method: 'POST',
  }).catch(error => console.warn('Unable to stop the web preview performance trace while closing DevTools.', error));
}

function formatNumber(value) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric)) return '—';
  return Number.isInteger(numeric)
    ? numeric.toLocaleString()
    : numeric.toLocaleString(undefined, { maximumFractionDigits: 1 });
}

function formatBytes(value) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric) || numeric < 0) return '—';
  if (numeric < 1024) return `${formatNumber(numeric)} B`;
  if (numeric < 1024 * 1024) return `${formatNumber(numeric / 1024)} KiB`;
  return `${formatNumber(numeric / (1024 * 1024))} MiB`;
}

function formatDuration(value) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric) || numeric < 0) return '—';
  if (numeric < 1) return `${formatNumber(numeric * 1000)} µs`;
  if (numeric < 1000) return `${formatNumber(numeric)} ms`;
  return `${formatNumber(numeric / 1000)} s`;
}

function formatUptime(value) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric) || numeric < 0) return '—';
  if (numeric < 60_000) return `${formatNumber(numeric / 1000)} s`;
  return `${formatNumber(numeric / 60_000)} min`;
}

function performanceIdentity(target = state.target) {
  if (!targetSupports('performance', target)) throw new Error('The selected target does not support Performance.');
  return targetIdentityParameters(target);
}

function samePerformanceIdentity(left, right) {
  if (!left || !right) return false;
  if (left.targetId !== undefined || right.targetId !== undefined) {
    return left.targetId !== undefined && left.targetId === right.targetId;
  }
  return (
    left.sessionId === right.sessionId &&
    left.inspectedUrl === right.inspectedUrl &&
    left.targetNonce === right.targetNonce
  );
}

function performanceIdentityIsCurrent(identity) {
  // This only rejects stale client-side responses. The server validates the
  // complete session, inspected URL, and per-tab nonce on every request.
  try {
    return samePerformanceIdentity(identity, performanceIdentity());
  } catch {
    return false;
  }
}

function performancePollingInputIsFocused() {
  return ['performanceDurationInput', 'performanceTraceFilter'].includes(document.activeElement?.id);
}

function preparePerformanceForTargetChange() {
  const perf = state.performance;
  perf.requestGeneration++;
  perf.operationGeneration++;
  perf.snapshotPending = false;
  perf.pending = false;
  perf.data = null;
  perf.lastTrace = null;
  perf.navigationExpanded = false;
  perf.rendererTracingEnabled = false;
  perf.samples = [];
  perf.traceScope = 'valdi';
  perf.traceSearch = '';
  if (perf.traceActive || perf.ownerIdentity) {
    perf.error = 'The previous web preview still owns a performance recording. Stop and retrieve it before switching.';
    return;
  }
  perf.error = null;
}

function performanceBlocksTargetSwitch() {
  const perf = state.performance;
  return Boolean(perf.pending || perf.traceActive || perf.ownerIdentity);
}

function sectionIsAvailable(section) {
  if (section === 'elements') return true;
  if (section === 'state') return targetSupports('components') && targetSupports('snapshot');
  if (section === 'console') return targetSupports('console');
  if (section === 'performance') return targetSupports('performance') || Boolean(state.performance.ownerIdentity);
  return false;
}

function updateCapabilityUi() {
  for (const tab of elements.mainTabs) {
    const available = sectionIsAvailable(tab.dataset.section);
    tab.disabled = !available;
    tab.setAttribute('aria-disabled', String(!available));
  }
  elements.consoleInput.disabled = !targetSupports('console');
  elements.clearConsoleButton.disabled = !targetSupports('console');
  elements.refreshButton.disabled = !state.target;
  if (!sectionIsAvailable(state.activeSection)) setActiveSection('elements');
}

function resetHighlightForTargetChange() {
  state.highlightIntentGeneration++;
  if (state.highlightTimer) window.clearTimeout(state.highlightTimer);
  state.highlightTimer = null;
  state.hoveredNodeId = null;
  state.hoveredSnapshotGeneration = 0;
  state.highlightMayBeActive = false;
}

function enqueueExactHighlightClear(target) {
  if (!target || !targetSupports('highlight', target) || !state.highlightMayBeActive) return;
  const identity = targetIdentityParameters(target);
  const request = state.highlightRequestTail.then(() =>
    requestJson('/api/devtools/highlight', {}, { body: identity }).catch(error => {
      console.warn('Unable to clear the previous Valdi target highlight.', error);
    }),
  );
  state.highlightRequestTail = request.catch(error => {
    console.warn('Unable to order the previous Valdi target highlight clear.', error);
  });
}

function resetConsoleForTargetChange() {
  clearConsole();
  state.consoleHistory = [];
  state.consoleHistoryIndex = 0;
  elements.consoleInput.value = '';
}

function resetComponentPropertyEditForTargetChange() {
  state.componentPropertyEdit.operationGeneration++;
  state.componentPropertyEdit.pending = false;
  state.componentPropertyEdit.focused = false;
  state.componentPropertyEdit.error = null;
}

function resetRuntimeStateForTargetChange() {
  const runtimeState = state.runtimeState;
  runtimeState.inspectGeneration++;
  runtimeState.inspectorNodeId = null;
  runtimeState.search = '';
  runtimeState.expandedComponents.clear();
  runtimeState.expandedInspectorValues.clear();
  runtimeState.expandedMainValues.clear();
  if (elements.stateFilter) elements.stateFilter.value = '';
  if (elements.stateSummary) elements.stateSummary.textContent = '';
  if (elements.stateContent) elements.stateContent.innerHTML = '';
}

function resetRuntimeStateForSelectionChange(selectedNodeId) {
  if (state.runtimeState.inspectorNodeId === selectedNodeId) return;
  state.runtimeState.inspectGeneration++;
  state.runtimeState.inspectorNodeId = selectedNodeId;
  state.runtimeState.expandedInspectorValues.clear();
}

function clearTargetPresentation(message) {
  state.snapshotRequestGeneration++;
  state.refreshPending = false;
  state.snapshotRequestCompletion = null;
  state.snapshot = null;
  state.snapshotGeneration++;
  state.selectedNodeId = null;
  state.remoteSelectedNodeId = null;
  state.expandedNodeIds.clear();
  resetHighlightForTargetChange();
  resetConsoleForTargetChange();
  resetComponentPropertyEditForTargetChange();
  resetRuntimeStateForTargetChange();
  preparePerformanceForTargetChange();
  elements.treeEmpty.textContent = message;
  render();
}

function applyDirectTargetSelection(nextTarget, options = {}) {
  const currentId = state.target?.id || null;
  const nextId = nextTarget?.id || null;
  if (currentId !== null && currentId === nextId && !options.force) {
    state.targetSwitchMessage = null;
    renderTargetPicker();
    return true;
  }
  if (!options.force && performanceBlocksTargetSwitch()) {
    state.targetSwitchMessage = 'Stop or finish the current Performance operation before switching debugger targets.';
    renderTargetPicker();
    return false;
  }

  stopConsoleStream();
  enqueueExactHighlightClear(state.target);
  state.targetGeneration++;
  clearTargetPresentation(nextTarget ? 'Loading the selected target…' : 'Choose a debugger target to inspect.');
  state.target = nextTarget;
  state.unavailableTargetId = options.unavailableTargetId || null;
  state.targetSwitchMessage = options.message || null;
  state.error = nextTarget
    ? null
    : options.unavailableTargetId
      ? options.message || 'The selected target is unavailable.'
      : null;
  if (nextTarget) {
    elements.targetMetadata.textContent = [
      nextTarget.platform,
      nextTarget.port === undefined ? null : `:${nextTarget.port}`,
    ]
      .filter(Boolean)
      .join(' · ');
    setConnected(true);
  } else {
    elements.targetMetadata.textContent = '';
    setConnected(false);
  }
  updateCapabilityUi();
  if (state.activeSection === 'performance') renderPerformance();
  renderTargetPicker();
  if (nextTarget) {
    startConsoleStream();
    void refreshSnapshot();
  }
  return true;
}

function nodeAttributes(node) {
  return valdiDebuggerTreeModel.attributes(node);
}

function nodeId(node) {
  return valdiDebuggerTreeModel.id(node);
}

function inspectedNodeId(node) {
  if (!node) return null;
  if (node.component) {
    return node.component.elementId === undefined ? null : String(node.component.elementId);
  }
  return nodeId(node);
}

function inspectedNode(node) {
  const id = inspectedNodeId(node);
  return id === null ? node : findNode(id) || node;
}

function treeRowId(id) {
  return `valdi-tree-node-${encodeURIComponent(id)}`;
}

function nodeText(node) {
  const attributes = nodeAttributes(node);
  const candidates = [attributes.accessibilityLabel, node?.element?.dom?.textContent, attributes.value];
  for (const candidate of candidates) {
    if (candidate === undefined || candidate === null || candidate === '') continue;
    const formatted = valdiDebuggerTreeModel.formatValue(candidate, 0).trim();
    if (formatted) return formatted;
  }
  return '';
}

function walk(node, callback) {
  valdiDebuggerTreeModel.walk(node, callback, [], 0);
}

function walkVisible(node, callback) {
  valdiDebuggerTreeModel.walkVisible(node, callback, current => state.expandedNodeIds.has(nodeId(current)), [], 0);
}

function nodeCount() {
  let count = 0;
  walk(state.snapshot?.tree, () => count++);
  return count;
}

function findNode(id) {
  return valdiDebuggerTreeModel.findNode(state.snapshot?.tree, id);
}

function findNodePath(id) {
  return valdiDebuggerTreeModel.pathToNode(state.snapshot?.tree, id);
}

function selectedNodeProjectionJson() {
  const node = findNode(state.selectedNodeId);
  return node ? JSON.stringify(valdiDebuggerTreeModel.projectTree(node), null, 2) : null;
}

function chooseInitialNode(root) {
  let selected = null;
  walk(root, node => {
    const attributes = nodeAttributes(node);
    if (attributes.accessibilityId || attributes.accessibilityLabel || typeof attributes.value === 'string') {
      selected = node;
      return false;
    }
    return true;
  });
  return selected || root;
}

function revealPath(id) {
  const path = findNodePath(id);
  for (const node of path.slice(0, -1)) {
    state.expandedNodeIds.add(nodeId(node));
  }
}

function expandUsefulNodes(root) {
  if (!root) return;
  let current = root;
  let depth = 0;
  while (current && depth < 8) {
    state.expandedNodeIds.add(nodeId(current));
    const children = valdiDebuggerTreeModel.children(current);
    if (children.length !== 1) break;
    current = children[0];
    depth++;
  }
  for (const child of valdiDebuggerTreeModel.children(current)) {
    if (valdiDebuggerTreeModel.hasChildren(child) && depth < 5) {
      state.expandedNodeIds.add(nodeId(child));
    }
  }
}

function setConnected(connected, message) {
  elements.targetStatusDot.className = `status-dot${connected ? '' : state.error ? ' error' : ' connecting'}`;
  if (message) {
    if (isDirectMode()) setTargetPickerStatus(message);
    else elements.targetName.textContent = message;
  }
}

function reportError(error) {
  const message = error instanceof Error ? error.message : String(error);
  state.error = message;
  setConnected(false, message);
  elements.treeEmpty.textContent = message;
}

async function refreshTargetRegistry() {
  if (!isDirectMode() || state.registryPending) return;
  state.registryPending = true;
  const requestGeneration = ++state.registryRequestGeneration;
  renderTargetPicker();
  try {
    const payload = await requestJson('/api/devtools/targets', {}, {});
    if (requestGeneration !== state.registryRequestGeneration) return;
    const targets = parseTargetRegistry(payload);
    state.registryTargets = targets;
    state.registryGeneration++;
    state.registryError = null;
    if (!state.target) state.error = null;

    if (state.initialTargetResolutionPending) {
      state.initialTargetResolutionPending = false;
      const requestedId = launchIdentity.requestedTargetId;
      const requestedTarget = targets.find(target => target.id === requestedId);
      if (requestedTarget && isSelectableDirectTarget(requestedTarget)) {
        state.unavailableTargetId = null;
        applyDirectTargetSelection(requestedTarget, { force: true });
      } else {
        state.unavailableTargetId = requestedId;
        state.error = 'The requested target is unavailable.';
        updateCapabilityUi();
        setConnected(false);
      }
    } else if (state.target) {
      const refreshedTarget = targets.find(target => target.id === state.target.id);
      if (!refreshedTarget || !isSelectableDirectTarget(refreshedTarget)) {
        const unavailableTargetId = state.target.id;
        applyDirectTargetSelection(null, {
          force: true,
          message: 'The selected target is no longer available. Choose another target.',
          unavailableTargetId,
        });
      } else {
        const previousTarget = state.target;
        const targetChanged = targetOperationalKey(previousTarget) !== targetOperationalKey(refreshedTarget);
        if (targetChanged) {
          stopConsoleStream();
          enqueueExactHighlightClear(previousTarget);
          state.targetGeneration++;
          clearTargetPresentation('Refreshing the selected target…');
        }
        state.target = refreshedTarget;
        elements.targetMetadata.textContent = [
          refreshedTarget.platform,
          refreshedTarget.port === undefined ? null : `:${refreshedTarget.port}`,
        ]
          .filter(Boolean)
          .join(' · ');
        updateCapabilityUi();
        if (state.activeSection === 'performance') renderPerformance();
        if (targetChanged) {
          startConsoleStream();
          void refreshSnapshot();
        }
      }
    }
    if (!state.target) setConnected(false);
    renderTargetPicker();
  } catch (error) {
    if (requestGeneration !== state.registryRequestGeneration) return;
    state.registryError = error instanceof Error ? error.message : String(error);
    if (!state.target) {
      state.error = state.registryError;
      setConnected(false);
    }
    renderTargetPicker();
  } finally {
    if (requestGeneration === state.registryRequestGeneration) {
      state.registryPending = false;
      renderTargetPicker();
    }
  }
}

async function connectToInspectedPage() {
  const connectionGeneration = ++state.connectionRequestGeneration;

  try {
    const payload = await requestJson('/api/devtools/target', { inspectedUrl, targetNonce: inspectedTargetNonce }, {});
    if (connectionGeneration !== state.connectionRequestGeneration) return;
    const previousTargetKey = state.target
      ? `${state.target.id}:${state.target.sessionId}:${inspectedTargetNonce}`
      : null;
    const nextTargetKey = `${payload.target.id}:${payload.target.sessionId}:${inspectedTargetNonce}`;
    if (previousTargetKey !== null && previousTargetKey !== nextTargetKey) {
      stopConsoleStream();
      enqueueExactHighlightClear(state.target);
      resetConsoleForTargetChange();
      resetComponentPropertyEditForTargetChange();
      resetRuntimeStateForTargetChange();
      preparePerformanceForTargetChange();
      state.snapshotRequestGeneration++;
      state.refreshPending = false;
      state.snapshotRequestCompletion = null;
      state.snapshot = null;
      state.snapshotGeneration++;
      state.selectedNodeId = null;
      state.remoteSelectedNodeId = null;
      state.expandedNodeIds.clear();
      resetHighlightForTargetChange();
      render();
    }
    if (previousTargetKey !== nextTargetKey) state.targetGeneration++;
    state.target = payload.target;
    if (previousTargetKey !== null && previousTargetKey !== nextTargetKey) render();
    elements.targetName.textContent = state.target.name || 'Valdi application';
    elements.targetName.title = state.target.applicationUrl || inspectedUrl;
    elements.targetMetadata.textContent = `Chromium · :${state.target.debuggingPort}`;
    renderTargetPicker();
    updateCapabilityUi();
    if (state.activeSection === 'performance') renderPerformance();
    setConnected(true);
    if (previousTargetKey !== nextTargetKey) {
      addConsoleEntry('info', `Connected to ${state.target.applicationUrl}`);
    }
    startConsoleStream();
    await refreshSnapshot();
    startRefreshTimer();
  } catch (error) {
    if (connectionGeneration !== state.connectionRequestGeneration) return;
    reportError(error);
    window.setTimeout(connectToInspectedPage, 1500);
  }
}

async function connectToInspectedApplication() {
  renderTargetPicker();
  updateCapabilityUi();
  if (launchIdentity.error) {
    state.error = launchIdentity.error;
    setConnected(false, state.error);
    elements.treeEmpty.textContent = state.error;
    return;
  }
  if (isDirectMode()) {
    await refreshTargetRegistry();
    startRefreshTimer();
    return;
  }
  await connectToInspectedPage();
}

async function refreshSnapshot() {
  await refreshSnapshotInternal(null);
}

function runtimeStatePresentationHasFocus() {
  const activeElement = document.activeElement;
  if (!activeElement) return false;
  if (elements.stateSection?.contains(activeElement)) return true;
  return state.activeDetail === 'state' && elements.inspector?.contains(activeElement);
}

async function refreshSnapshotInternal(componentPropertyEditOperationGeneration) {
  const componentPropertyEditOwnsRefresh = () =>
    componentPropertyEditOperationGeneration !== null &&
    state.componentPropertyEdit.operationGeneration === componentPropertyEditOperationGeneration &&
    state.componentPropertyEdit.pending;
  const refreshIsAllowed = () =>
    state.target &&
    targetSupports('components') &&
    targetSupports('snapshot') &&
    ((!state.componentPropertyEdit.focused && !state.componentPropertyEdit.pending) ||
      componentPropertyEditOwnsRefresh()) &&
    !runtimeStatePresentationHasFocus();
  if (!refreshIsAllowed()) {
    return runtimeStatePresentationHasFocus() ? SNAPSHOT_REFRESH_RUNTIME_STATE_DEFERRED : SNAPSHOT_REFRESH_SKIPPED;
  }
  if (state.refreshPending) {
    const activeRequestCompletion = state.snapshotRequestCompletion;
    if (!componentPropertyEditOwnsRefresh() || activeRequestCompletion === null) {
      return SNAPSHOT_REFRESH_SKIPPED;
    }
    await activeRequestCompletion;
    if (!refreshIsAllowed() || state.refreshPending) {
      return runtimeStatePresentationHasFocus() ? SNAPSHOT_REFRESH_RUNTIME_STATE_DEFERRED : SNAPSHOT_REFRESH_SKIPPED;
    }
  }
  state.refreshPending = true;
  let resolveRequestCompletion;
  const requestCompletion = new Promise(resolve => {
    resolveRequestCompletion = resolve;
  });
  state.snapshotRequestCompletion = requestCompletion;
  const requestTarget = state.target;
  const requestTargetGeneration = state.targetGeneration;
  const requestGeneration = ++state.snapshotRequestGeneration;
  const requestIsCurrent = () =>
    state.targetGeneration === requestTargetGeneration &&
    state.target?.id === requestTarget.id &&
    state.snapshotRequestGeneration === requestGeneration;
  try {
    const snapshot = await requestJson('/api/devtools/snapshot', targetIdentityParameters(requestTarget), {});
    if (!requestIsCurrent()) return SNAPSHOT_REFRESH_SKIPPED;
    if (runtimeStatePresentationHasFocus()) return SNAPSHOT_REFRESH_RUNTIME_STATE_DEFERRED;
    if (
      (state.componentPropertyEdit.focused || state.componentPropertyEdit.pending) &&
      !componentPropertyEditOwnsRefresh()
    ) {
      return SNAPSHOT_REFRESH_SKIPPED;
    }
    if (
      snapshot.target?.id === requestTarget.id &&
      Array.isArray(snapshot.target.capabilities) &&
      snapshot.target.capabilities.length <= MAX_REGISTRY_CAPABILITIES &&
      snapshot.target.capabilities.every(capability => typeof capability === 'string' && capability.length <= 64)
    ) {
      state.target = { ...requestTarget, capabilities: [...snapshot.target.capabilities] };
      updateCapabilityUi();
    }
    snapshot.tree = valdiDebuggerTreeModel.restoreTree(snapshot.tree);
    const wasEmpty = !state.snapshot?.tree;
    const previousSelectedNodeId = state.selectedNodeId;
    const shouldClearHighlight = state.hoveredNodeId !== null || state.highlightMayBeActive;
    state.snapshot = snapshot;
    state.runtimeState.inspectGeneration++;
    state.componentPropertyEdit.error = null;
    state.snapshotGeneration++;
    if (state.highlightTimer) window.clearTimeout(state.highlightTimer);
    state.highlightTimer = null;
    state.hoveredNodeId = null;
    state.hoveredSnapshotGeneration = state.snapshotGeneration - 1;
    if (shouldClearHighlight) queueHighlight(null);
    state.error = null;
    setConnected(true);
    renderTargetPicker();

    if (wasEmpty) {
      expandUsefulNodes(snapshot.tree);
      state.selectedNodeId = nodeId(chooseInitialNode(snapshot.tree));
      revealPath(state.selectedNodeId);
    }
    if (snapshot.selectedNodeId && snapshot.selectedNodeId !== state.remoteSelectedNodeId) {
      state.remoteSelectedNodeId = String(snapshot.selectedNodeId);
      state.selectedNodeId = state.remoteSelectedNodeId;
      revealPath(state.selectedNodeId);
    }
    if (!findNode(state.selectedNodeId)) {
      state.selectedNodeId = nodeId(chooseInitialNode(snapshot.tree));
      revealPath(state.selectedNodeId);
    }
    if (state.selectedNodeId !== previousSelectedNodeId) {
      resetRuntimeStateForSelectionChange(state.selectedNodeId);
    }
    render();
    return SNAPSHOT_REFRESH_APPLIED;
  } catch (error) {
    if (requestIsCurrent()) reportError(error);
    return SNAPSHOT_REFRESH_SKIPPED;
  } finally {
    if (state.snapshotRequestCompletion === requestCompletion) {
      state.snapshotRequestCompletion = null;
      state.refreshPending = false;
    }
    resolveRequestCompletion();
  }
}

function startRefreshTimer() {
  if (state.refreshTimer) window.clearInterval(state.refreshTimer);
  state.refreshTimer = window.setInterval(() => {
    if (document.hidden) return;
    if (isDirectMode()) void refreshTargetRegistry();
    if (!state.autoRefresh) return;
    if (state.componentPropertyEdit.focused || state.componentPropertyEdit.pending) return;
    if (runtimeStatePresentationHasFocus()) return;
    if (state.activeSection === 'elements' || state.activeSection === 'state') void refreshSnapshot();
    if (state.activeSection === 'performance') void refreshPerformance({ silent: true });
  }, 1200);
}

function visibleSearchIds(root, search) {
  const ids = new Set();
  const normalized = search.toLowerCase();
  const records = [];
  walk(root, (node, ancestors) => {
    const metadata = valdiDebuggerTreeModel.stringifyValue(nodeAttributes(node), 0);
    const text = `${node.tag} ${nodeText(node)} ${metadata}`.toLowerCase();
    records.push({ matched: text.includes(normalized), node, parent: ancestors.at(-1) });
  });
  const matchedSubtrees = new Set();
  for (let index = records.length - 1; index >= 0; index -= 1) {
    const record = records[index];
    if (!record.matched && !matchedSubtrees.has(record.node)) continue;
    ids.add(nodeId(record.node));
    if (record.parent) matchedSubtrees.add(record.parent);
  }
  return ids;
}

function selectedAttribute(node) {
  const attributes = nodeAttributes(node);
  if (attributes.accessibilityId) {
    return ['id', valdiDebuggerTreeModel.formatValue(attributes.accessibilityId, 0)];
  }
  if (attributes.accessibilityCategory && attributes.accessibilityCategory !== 'view') {
    return ['role', valdiDebuggerTreeModel.formatValue(attributes.accessibilityCategory, 0)];
  }
  if (attributes.onTap || attributes.touchEnabled) return ['interactive', 'true'];
  return null;
}

function renderTree() {
  const root = state.snapshot?.tree;
  if (!root) {
    elements.tree.innerHTML = '';
    elements.tree.removeAttribute('aria-activedescendant');
    elements.filterSummary.textContent = '';
    return;
  }

  const search = state.search.trim();
  const visibleIds = search ? visibleSearchIds(root, search) : null;
  const rows = [];
  let matches = 0;
  const normalizedSearch = search.toLowerCase();
  const traversal = search ? walk : walkVisible;
  traversal(root, (node, _ancestors, depth) => {
    const id = nodeId(node);
    if (visibleIds && !visibleIds.has(id)) return;
    const hasChildren = valdiDebuggerTreeModel.hasChildren(node);
    const expanded = Boolean(search) || state.expandedNodeIds.has(id);
    const selected = id === state.selectedNodeId;
    const attribute = selectedAttribute(node);
    const text = nodeText(node);
    const attributes = nodeAttributes(node);
    const serializedAttributes = valdiDebuggerTreeModel.stringifyValue(attributes, 0);
    if (!search || `${node.tag} ${text} ${serializedAttributes}`.toLowerCase().includes(normalizedSearch)) {
      matches++;
    }
    rows.push(`
      <div id="${escapeHtml(treeRowId(id))}" class="tree-row${node.component ? ' component-row' : ''}${selected ? ' selected' : ''}" data-node-id="${escapeHtml(id)}" role="treeitem" aria-level="${depth + 1}" aria-selected="${selected}"${hasChildren ? ` aria-expanded="${expanded}"` : ''} style="padding-left:${depth * 13 + 2}px">
        <button class="disclosure${hasChildren ? '' : ' empty'}" data-toggle-id="${escapeHtml(id)}" tabindex="-1" aria-label="${expanded ? 'Collapse' : 'Expand'}">${expanded ? '▾' : '▸'}</button>
        <span class="tag-bracket">&lt;</span><span class="tag-name">${escapeHtml(node.tag || 'view')}</span>
        ${attribute ? `<span class="attribute-name">${escapeHtml(attribute[0])}</span>=<span class="attribute-value">"${escapeHtml(attribute[1])}"</span>` : ''}<span class="tag-bracket">&gt;</span>
        ${text ? `<span class="tree-text">${escapeHtml(text)}</span>` : ''}
      </div>
    `);
  });

  const scrollTop = elements.tree.scrollTop;
  elements.tree.innerHTML = rows.join('');
  elements.tree.scrollTop = scrollTop;
  const selectedRow = document.getElementById(treeRowId(state.selectedNodeId));
  if (selectedRow && elements.tree.contains(selectedRow)) {
    elements.tree.setAttribute('aria-activedescendant', selectedRow.id);
  } else {
    elements.tree.removeAttribute('aria-activedescendant');
  }
  elements.filterSummary.textContent = search ? `${matches} match${matches === 1 ? '' : 'es'}` : '';
}

function renderValue(value) {
  if (value === undefined || value === null) return '<span class="property-value empty">null</span>';
  const type = typeof value;
  const serialized = valdiDebuggerTreeModel.formatValue(value, 0);
  const printable = serialized === undefined ? String(value) : serialized;
  const formatted = printable.length > 160 ? `${printable.slice(0, 157)}…` : printable;
  return `<span class="property-value ${type}">${escapeHtml(formatted)}</span>`;
}

function propertyRows(attributes, options) {
  const entries = Object.entries(attributes || {}).sort(([first], [second]) => first.localeCompare(second));
  if (!entries.length) return '<div class="empty-state">No properties available.</div>';
  return entries
    .map(
      ([key, value]) =>
        `<div class="property-row"><span class="property-name">${escapeHtml(key)}</span>: ${renderValue(value)}${options.css ? ';' : ''}</div>`,
    )
    .join('');
}

function runtimeStateParseError(message) {
  return new Error(`Runtime state ${message}.`);
}

function runtimeStateKeywordAt(source, offset, keyword) {
  return source.startsWith(keyword, offset) && !/[A-Za-z0-9_$]/.test(source[offset + keyword.length] || '');
}

function readRuntimeStateToken(source, parser) {
  while (parser.offset < source.length && /[\t\n\r ]/.test(source[parser.offset])) parser.offset++;
  if (parser.offset >= source.length) return { type: 'end' };
  parser.tokens++;
  if (parser.tokens > MAX_RUNTIME_STATE_TOKENS) throw runtimeStateParseError('exceeds the token limit');
  if (parser.sourceType !== RUNTIME_STATE_SOURCE_WEB) {
    throw runtimeStateParseError('cannot structurally parse a native snapshot');
  }

  const character = source[parser.offset];
  if ('{}[]:,'.includes(character)) {
    parser.offset++;
    return { type: 'punctuation', value: character };
  }
  if (character === '"') {
    const start = parser.offset;
    parser.offset++;
    let closed = false;
    while (parser.offset < source.length) {
      const code = source.charCodeAt(parser.offset);
      if (code < 0x20) throw runtimeStateParseError('contains an invalid string');
      if (source[parser.offset] === '"') {
        parser.offset++;
        closed = true;
        break;
      }
      if (source[parser.offset] !== '\\') {
        parser.offset++;
        continue;
      }
      parser.offset++;
      const escape = source[parser.offset];
      if ('"\\/bfnrt'.includes(escape)) {
        parser.offset++;
        continue;
      }
      if (escape !== 'u' || !/^[0-9a-fA-F]{4}$/.test(source.slice(parser.offset + 1, parser.offset + 5))) {
        throw runtimeStateParseError('contains an invalid string escape');
      }
      parser.offset += 5;
    }
    if (!closed) throw runtimeStateParseError('contains an unterminated string');
    let value;
    try {
      value = JSON.parse(source.slice(start, parser.offset));
    } catch (_error) {
      throw runtimeStateParseError('contains an invalid string');
    }
    return { type: 'string', value };
  }
  if (runtimeStateKeywordAt(source, parser.offset, 'true')) {
    parser.offset += 4;
    return { type: 'value', value: true };
  }
  if (runtimeStateKeywordAt(source, parser.offset, 'false')) {
    parser.offset += 5;
    return { type: 'value', value: false };
  }
  if (runtimeStateKeywordAt(source, parser.offset, 'null')) {
    parser.offset += 4;
    return { type: 'value', value: null };
  }

  const start = parser.offset;
  if (source[parser.offset] === '-') parser.offset++;
  if (source[parser.offset] === '0') {
    parser.offset++;
    if (/\d/.test(source[parser.offset] || '')) throw runtimeStateParseError('contains an invalid number');
  } else if (/[1-9]/.test(source[parser.offset] || '')) {
    while (/\d/.test(source[parser.offset] || '')) parser.offset++;
  } else {
    throw runtimeStateParseError('contains an unsupported token');
  }
  if (source[parser.offset] === '.') {
    parser.offset++;
    if (!/\d/.test(source[parser.offset] || '')) throw runtimeStateParseError('contains an invalid number');
    while (/\d/.test(source[parser.offset] || '')) parser.offset++;
  }
  if (source[parser.offset] === 'e' || source[parser.offset] === 'E') {
    parser.offset++;
    if (source[parser.offset] === '+' || source[parser.offset] === '-') parser.offset++;
    if (!/\d/.test(source[parser.offset] || '')) throw runtimeStateParseError('contains an invalid number');
    while (/\d/.test(source[parser.offset] || '')) parser.offset++;
  }
  const value = Number(source.slice(start, parser.offset));
  if (!Number.isFinite(value)) throw runtimeStateParseError('contains a non-finite number');
  return { type: 'value', value };
}

function parseRuntimeState(source, sourceType) {
  if (typeof source !== 'string') {
    return { error: 'Runtime state is not a serialized document.', parsed: false, value: null };
  }
  if (source.length === 0 || source.length > MAX_RUNTIME_STATE_CHARACTERS) {
    return {
      error: `Runtime state must contain between 1 and ${MAX_RUNTIME_STATE_CHARACTERS} characters.`,
      parsed: false,
      value: null,
    };
  }
  if (sourceType !== RUNTIME_STATE_SOURCE_NATIVE && sourceType !== RUNTIME_STATE_SOURCE_WEB) {
    return { error: 'Runtime state has an unknown snapshot source.', parsed: false, value: null };
  }
  if (sourceType === RUNTIME_STATE_SOURCE_NATIVE) {
    return {
      error: 'Native runtime state is displayed as escaped raw text.',
      parsed: false,
      value: null,
    };
  }

  const parser = { offset: 0, sourceType, tokens: 0 };
  const stack = [];
  let root;
  let rootSet = false;

  const setValue = value => {
    if (stack.length === 0) {
      if (rootSet) throw runtimeStateParseError('contains trailing data');
      root = value;
      rootSet = true;
      return;
    }
    const frame = stack[stack.length - 1];
    if (frame.type === 'array') {
      if (frame.stage !== 'initialOrEnd' && frame.stage !== 'value') {
        throw runtimeStateParseError('contains an unexpected array value');
      }
      if (frame.entries >= MAX_RUNTIME_STATE_CONTAINER_ENTRIES) {
        throw runtimeStateParseError('exceeds the per-container entry limit');
      }
      frame.values.push(value);
      frame.entries++;
      frame.stage = 'commaOrEnd';
      return;
    }
    if (frame.stage !== 'value' || frame.key === null) {
      throw runtimeStateParseError('contains an unexpected object value');
    }
    if (frame.entries >= MAX_RUNTIME_STATE_CONTAINER_ENTRIES) {
      throw runtimeStateParseError('exceeds the per-container entry limit');
    }
    if (Object.prototype.hasOwnProperty.call(frame.target, frame.key)) {
      throw runtimeStateParseError('contains a duplicate object key');
    }
    Object.defineProperty(frame.target, frame.key, {
      configurable: true,
      enumerable: true,
      value,
      writable: true,
    });
    frame.entries++;
    frame.key = null;
    frame.stage = 'commaOrEnd';
  };

  const beginContainer = type => {
    if (stack.length >= MAX_RUNTIME_STATE_DEPTH) throw runtimeStateParseError('exceeds the depth limit');
    const target = type === 'array' ? [] : Object.create(null);
    setValue(target);
    stack.push({ entries: 0, key: null, stage: 'initialOrEnd', target, type, values: target });
  };

  const acceptValueToken = token => {
    if (token.type === 'value' || token.type === 'string') {
      setValue(token.value);
      return true;
    }
    if (token.type === 'punctuation' && token.value === '{') {
      beginContainer('object');
      return true;
    }
    if (token.type === 'punctuation' && token.value === '[') {
      beginContainer('array');
      return true;
    }
    return false;
  };

  try {
    while (true) {
      const token = readRuntimeStateToken(source, parser);
      if (token.type === 'end') {
        if (!rootSet) throw runtimeStateParseError('is empty');
        if (stack.length > 0) throw runtimeStateParseError('contains an unterminated container');
        return { error: null, parsed: true, value: root };
      }
      if (stack.length === 0) {
        if (rootSet || !acceptValueToken(token)) throw runtimeStateParseError('contains trailing data');
        continue;
      }

      const frame = stack[stack.length - 1];
      if (frame.type === 'object') {
        if (frame.stage === 'initialOrEnd' || frame.stage === 'key') {
          if (token.type === 'punctuation' && token.value === '}' && frame.stage === 'initialOrEnd') {
            stack.pop();
            continue;
          }
          if (token.type !== 'string') {
            throw runtimeStateParseError('contains an invalid object key');
          }
          if (token.value.length > MAX_RUNTIME_STATE_KEY_CHARACTERS) {
            throw runtimeStateParseError('contains an overlong object key');
          }
          frame.key = token.value;
          frame.stage = 'colon';
          continue;
        }
        if (frame.stage === 'colon') {
          if (token.type !== 'punctuation' || token.value !== ':') {
            throw runtimeStateParseError('is missing an object value separator');
          }
          frame.stage = 'value';
          continue;
        }
        if (frame.stage === 'value') {
          if (!acceptValueToken(token)) throw runtimeStateParseError('contains an invalid object value');
          continue;
        }
        if (token.type === 'punctuation' && token.value === ',') {
          frame.stage = 'key';
          continue;
        }
        if (token.type === 'punctuation' && token.value === '}') {
          stack.pop();
          continue;
        }
        throw runtimeStateParseError('contains an invalid object separator');
      }

      if (frame.stage === 'initialOrEnd') {
        if (token.type === 'punctuation' && token.value === ']') {
          stack.pop();
          continue;
        }
        if (!acceptValueToken(token)) throw runtimeStateParseError('contains an invalid array value');
        continue;
      }
      if (frame.stage === 'value') {
        if (!acceptValueToken(token)) throw runtimeStateParseError('contains an invalid array value');
        continue;
      }
      if (token.type === 'punctuation' && token.value === ',') {
        frame.stage = 'value';
        continue;
      }
      if (token.type === 'punctuation' && token.value === ']') {
        stack.pop();
        continue;
      }
      throw runtimeStateParseError('contains an invalid array separator');
    }
  } catch (error) {
    return {
      error: error instanceof Error ? error.message : 'Runtime state could not be parsed.',
      parsed: false,
      value: null,
    };
  }
}

function runtimeStateOwnDataValue(source, propertyName) {
  if (typeof source !== 'object' || source === null) return null;
  try {
    const descriptor = Object.getOwnPropertyDescriptor(source, propertyName);
    if (descriptor?.enumerable !== true || !Object.prototype.hasOwnProperty.call(descriptor, 'value')) {
      return null;
    }
    return { value: descriptor.value };
  } catch (_error) {
    return null;
  }
}

function runtimeStateExactNodeId(value) {
  if (typeof value === 'string' && value.length > 0) return value;
  if (Number.isSafeInteger(value) && value >= 0) return String(value);
  return null;
}

function runtimeStateSelectableNodeId(node) {
  if (typeof node !== 'object' || node === null) return null;
  try {
    const nodeIdDescriptor = Object.getOwnPropertyDescriptor(node, 'id');
    if (nodeIdDescriptor !== undefined) {
      if (nodeIdDescriptor.enumerable !== true || !Object.prototype.hasOwnProperty.call(nodeIdDescriptor, 'value')) {
        return null;
      }
      return runtimeStateExactNodeId(nodeIdDescriptor.value);
    }
  } catch (_error) {
    return null;
  }
  const elementValue = runtimeStateOwnDataValue(node, 'element')?.value;
  return runtimeStateExactNodeId(runtimeStateOwnDataValue(elementValue, 'id')?.value);
}

function runtimeStateSourceType() {
  return launchIdentity.mode === 'inspected-page' ? RUNTIME_STATE_SOURCE_WEB : RUNTIME_STATE_SOURCE_NATIVE;
}

function runtimeStateComponentRecord(node, structuralId) {
  const componentValue = runtimeStateOwnDataValue(node, 'component')?.value;
  const stateValue = runtimeStateOwnDataValue(componentValue, 'state')?.value;
  if (typeof stateValue !== 'string' || stateValue.length === 0 || stateValue.length > MAX_RUNTIME_STATE_CHARACTERS) {
    return null;
  }
  const sourceType = runtimeStateSourceType();
  const nameValue = runtimeStateOwnDataValue(componentValue, 'name')?.value;
  const componentKeyValue = runtimeStateOwnDataValue(componentValue, 'key')?.value;
  const nodeKeyValue = runtimeStateOwnDataValue(node, 'key')?.value;
  const keyValue = sourceType === RUNTIME_STATE_SOURCE_WEB ? componentKeyValue : nodeKeyValue;
  const tagValue = runtimeStateOwnDataValue(node, 'tag')?.value;
  return {
    key: typeof keyValue === 'string' ? keyValue : '',
    name:
      typeof nameValue === 'string' && nameValue.length > 0
        ? nameValue
        : typeof tagValue === 'string' && tagValue.length > 0
          ? tagValue
          : 'Component',
    node,
    selectableNodeId: runtimeStateSelectableNodeId(node),
    source: stateValue,
    sourceType,
    structuralId,
  };
}

function parseRuntimeStateRecord(record) {
  const cached = runtimeStateParseCache.get(record.node);
  if (cached?.source === record.source && cached.sourceType === record.sourceType) return cached.parsed;
  const parsed = parseRuntimeState(record.source, record.sourceType);
  runtimeStateParseCache.set(record.node, {
    parsed,
    source: record.source,
    sourceType: record.sourceType,
  });
  return parsed;
}

function collectRuntimeStateComponents(root) {
  const exactSelectableNodes = new Map();
  const records = [];
  const structuralPaths = new WeakMap();
  let truncated = false;
  walk(root, (node, ancestors, _depth, sourceChildIndex) => {
    let structuralPath;
    if (ancestors.length === 0) {
      structuralPath = [];
    } else {
      const parentPath = structuralPaths.get(ancestors[ancestors.length - 1]);
      if (!Array.isArray(parentPath) || !Number.isSafeInteger(sourceChildIndex) || sourceChildIndex < 0) {
        return false;
      }
      structuralPath = [...parentPath, sourceChildIndex];
    }
    structuralPaths.set(node, structuralPath);
    const selectableNodeId = runtimeStateSelectableNodeId(node);
    if (selectableNodeId !== null) {
      if (exactSelectableNodes.has(selectableNodeId)) exactSelectableNodes.set(selectableNodeId, null);
      else exactSelectableNodes.set(selectableNodeId, node);
    }
    const record = runtimeStateComponentRecord(node, JSON.stringify(structuralPath));
    if (record === null) return true;
    if (records.length >= MAX_RUNTIME_STATE_COMPONENT_ROWS) {
      truncated = true;
      return true;
    }
    records.push(record);
    return true;
  });
  for (const record of records) {
    if (record.selectableNodeId !== null && exactSelectableNodes.get(record.selectableNodeId) !== record.node) {
      record.selectableNodeId = null;
    }
  }
  return { records, truncated };
}

function findRuntimeStateRecordByStructuralId(structuralId) {
  return (
    collectRuntimeStateComponents(state.snapshot?.tree).records.find(record => record.structuralId === structuralId) ||
    null
  );
}

function runtimeStateContainerEntries(value) {
  if (Array.isArray(value)) return value.map((entry, index) => [String(index), entry]);
  if (typeof value !== 'object' || value === null) return [];
  return Object.keys(value).map(key => [key, Object.getOwnPropertyDescriptor(value, key)?.value]);
}

function runtimeStateDescription(value) {
  if (Array.isArray(value)) return `${value.length} item${value.length === 1 ? '' : 's'}`;
  if (typeof value === 'object' && value !== null) {
    const count = Object.keys(value).length;
    return `${count} field${count === 1 ? '' : 's'}`;
  }
  return typeof value;
}

function renderRuntimeStatePrimitive(value) {
  if (value === null) return '<span class="runtime-state-value-null">null</span>';
  const type = typeof value;
  const serialized = type === 'string' ? JSON.stringify(value) : String(value);
  return `<span class="runtime-state-value-${escapeHtml(type)}">${escapeHtml(serialized)}</span>`;
}

function runtimeStateValuePath(scope, componentId, segments) {
  return JSON.stringify([scope, componentId, ...segments]);
}

function renderRuntimeStateEntries(value, context, segments) {
  const entries = runtimeStateContainerEntries(value);
  if (entries.length === 0) return '<div class="empty-state">This state container is empty.</div>';
  const rows = [];
  for (const [key, entryValue] of entries) {
    if (context.rows >= MAX_RUNTIME_STATE_VALUE_ROWS) {
      context.truncated = true;
      break;
    }
    context.rows++;
    const entrySegments = [...segments, key];
    const keyLabel = Array.isArray(value) ? `[${key}]` : key;
    if (typeof entryValue !== 'object' || entryValue === null) {
      rows.push(
        `<div class="runtime-state-value-row"><span class="runtime-state-value-key">${escapeHtml(keyLabel)}</span>: ${renderRuntimeStatePrimitive(entryValue)}</div>`,
      );
      continue;
    }
    const path = runtimeStateValuePath(context.scope, context.componentId, entrySegments);
    const expanded = context.expandedPaths.has(path);
    rows.push(`
      <details class="runtime-state-value-details" data-runtime-state-value-path="${escapeHtml(path)}" data-runtime-state-value-scope="${escapeHtml(context.scope)}"${expanded ? ' open' : ''}>
        <summary class="runtime-state-value-summary"><span class="runtime-state-disclosure">›</span><span class="runtime-state-value-key">${escapeHtml(keyLabel)}</span>: <span class="runtime-state-value-description">${escapeHtml(runtimeStateDescription(entryValue))}</span></summary>
        ${expanded ? `<div class="runtime-state-value-children">${renderRuntimeStateEntries(entryValue, context, entrySegments)}</div>` : ''}
      </details>
    `);
  }
  if (context.truncated && !context.limitRendered) {
    context.limitRendered = true;
    rows.push('<div class="runtime-state-limit">Additional state rows were omitted.</div>');
  }
  return rows.join('');
}

function renderRuntimeStateDocument(record, parsed, context) {
  if (!parsed.parsed || typeof parsed.value !== 'object' || parsed.value === null) {
    return `<div class="runtime-state-limit">${escapeHtml(parsed.error || 'Runtime state is not an object document.')}</div><pre class="runtime-state-raw">${escapeHtml(record.source)}</pre>`;
  }
  return renderRuntimeStateEntries(parsed.value, context, []);
}

function hydrateRuntimeStateInspectButtons(models) {
  const buttons = elements.stateContent?.querySelectorAll?.('[data-runtime-state-inspect-slot]') || [];
  for (const button of buttons) {
    const index = Number(button.dataset.runtimeStateInspectSlot);
    const model = Number.isSafeInteger(index) && index >= 0 ? models[index] : undefined;
    if (!model) continue;
    delete button.dataset.runtimeStateInspectSlot;
    button.dataset.runtimeStateInspect = '';
    runtimeStateInspectBindings.set(button, model);
  }
}

function renderRuntimeStateSection() {
  if (!elements.stateContent || !elements.stateSummary) return;
  const renderGeneration = ++state.runtimeState.inspectGeneration;
  const snapshotGeneration = state.snapshotGeneration;
  const collected = collectRuntimeStateComponents(state.snapshot?.tree);
  const normalizedSearch = state.runtimeState.search.trim().toLowerCase();
  const records = normalizedSearch
    ? collected.records.filter(record =>
        `${record.name} ${record.key} ${record.source}`.toLowerCase().includes(normalizedSearch),
      )
    : collected.records;
  elements.stateSummary.textContent = `${records.length}${normalizedSearch ? ` of ${collected.records.length}` : ''} component${collected.records.length === 1 ? '' : 's'}${collected.truncated ? ' · first 500' : ''}`;
  if (!state.snapshot?.tree) {
    elements.stateContent.innerHTML = '<div class="empty-state">Waiting for the inspected component hierarchy…</div>';
    return;
  }
  if (records.length === 0) {
    elements.stateContent.innerHTML = `<div class="empty-state">${normalizedSearch ? 'No component state matches this filter.' : 'No mounted component has published bounded runtime state.'}</div>`;
    return;
  }

  const inspectModels = [];
  const renderContext = {
    componentId: '',
    expandedPaths: state.runtimeState.expandedMainValues,
    rows: 0,
    limitRendered: false,
    scope: 'main',
    truncated: false,
  };
  const markup = records
    .map(record => {
      const expanded = state.runtimeState.expandedComponents.has(record.structuralId);
      const parsed = parseRuntimeStateRecord(record);
      let inspectButton = '';
      if (record.selectableNodeId !== null) {
        const inspectIndex =
          inspectModels.push({
            generation: renderGeneration,
            node: record.node,
            selectableNodeId: record.selectableNodeId,
            snapshotGeneration,
            source: record.source,
            structuralId: record.structuralId,
          }) - 1;
        inspectButton = `<button type="button" class="runtime-state-inspect" data-runtime-state-inspect-slot="${inspectIndex}" aria-label="${escapeHtml(`Inspect ${record.name}${record.key ? ` ${record.key}` : ''} state, component ${record.selectableNodeId}`)}">Inspect</button>`;
      }
      renderContext.componentId = record.structuralId;
      const description = parsed.parsed ? runtimeStateDescription(parsed.value) : 'bounded raw snapshot';
      const body = expanded
        ? `<div class="runtime-state-component-body">${renderRuntimeStateDocument(record, parsed, renderContext)}</div>`
        : '';
      return `
        <div class="runtime-state-component">
          <details class="runtime-state-component-details" data-runtime-state-component-id="${escapeHtml(record.structuralId)}"${expanded ? ' open' : ''}>
            <summary class="runtime-state-component-summary"><span class="runtime-state-disclosure">›</span><span class="runtime-state-component-name">${escapeHtml(record.name)}</span>${record.key ? `<span class="runtime-state-component-key">${escapeHtml(record.key)}</span>` : ''}<span class="runtime-state-component-description">${escapeHtml(description)}</span></summary>
            ${body}
          </details>
          ${inspectButton}
        </div>
      `;
    })
    .join('');
  elements.stateContent.innerHTML = markup;
  hydrateRuntimeStateInspectButtons(inspectModels);
}

function renderSelectedRuntimeState(node) {
  resetRuntimeStateForSelectionChange(nodeId(node));
  const record = runtimeStateComponentRecord(node, JSON.stringify(['selected']));
  if (record === null) {
    return '<div class="empty-state">This selected component has no bounded runtime state snapshot.</div>';
  }
  const parsed = parseRuntimeStateRecord(record);
  const context = {
    componentId: record.structuralId,
    expandedPaths: state.runtimeState.expandedInspectorValues,
    rows: 0,
    limitRendered: false,
    scope: 'inspector',
    truncated: false,
  };
  return `<section class="runtime-state-inspector"><div class="rule-header">Component state <span class="rule-origin">${escapeHtml(record.name)}</span></div>${renderRuntimeStateDocument(record, parsed, context)}</section>`;
}

function inspectRuntimeStateBinding(binding) {
  if (
    binding?.generation !== state.runtimeState.inspectGeneration ||
    binding.snapshotGeneration !== state.snapshotGeneration
  ) {
    return false;
  }
  const currentRecord = findRuntimeStateRecordByStructuralId(binding.structuralId);
  if (
    currentRecord?.node !== binding.node ||
    currentRecord.selectableNodeId !== binding.selectableNodeId ||
    currentRecord.source !== binding.source
  ) {
    return false;
  }
  setActiveSection('elements');
  selectNode(binding.selectableNodeId);
  setActiveDetail('state');
  elements.detailTabs.find(tab => tab.dataset.detail === 'state')?.focus?.({ preventScroll: true });
  return true;
}

function restoreRuntimeStateDisclosureFocus(scope, componentId, valuePath) {
  const container = scope === 'inspector' ? elements.inspector : elements.stateContent;
  const detailsElements = container?.querySelectorAll?.('details') || [];
  for (const candidate of detailsElements) {
    if (
      (componentId !== null && candidate.dataset.runtimeStateComponentId === componentId) ||
      (valuePath !== null && candidate.dataset.runtimeStateValuePath === valuePath)
    ) {
      candidate.querySelector?.('summary')?.focus?.({ preventScroll: true });
      return;
    }
  }
}

function updateRuntimeStateDisclosure(details) {
  if (!details?.dataset) return;
  const valuePath = details.dataset.runtimeStateValuePath;
  if (valuePath) {
    const summary = details.querySelector?.('summary');
    const restoreFocus = summary !== null && summary !== undefined && summary === document.activeElement;
    const scope = details.dataset.runtimeStateValueScope === 'inspector' ? 'inspector' : 'main';
    const expandedPaths =
      scope === 'inspector' ? state.runtimeState.expandedInspectorValues : state.runtimeState.expandedMainValues;
    if (details.open === expandedPaths.has(valuePath)) return;
    if (details.open) expandedPaths.add(valuePath);
    else expandedPaths.delete(valuePath);
    if (scope === 'inspector') renderInspector();
    else renderRuntimeStateSection();
    if (restoreFocus) restoreRuntimeStateDisclosureFocus(scope, null, valuePath);
    return;
  }
  const structuralId = details.dataset.runtimeStateComponentId;
  if (!structuralId) return;
  const summary = details.querySelector?.('summary');
  const restoreFocus = summary !== null && summary !== undefined && summary === document.activeElement;
  const expanded = state.runtimeState.expandedComponents.has(structuralId);
  if (details.open === expanded) return;
  const record = findRuntimeStateRecordByStructuralId(structuralId);
  if (details.open && record === null) return;
  if (details.open) state.runtimeState.expandedComponents.add(structuralId);
  else state.runtimeState.expandedComponents.delete(structuralId);
  renderRuntimeStateSection();
  if (restoreFocus) restoreRuntimeStateDisclosureFocus('main', structuralId, null);
}

function componentPropertyEditMetadata(node, propertyName, value) {
  if (
    launchIdentity.mode !== 'inspected-page' ||
    state.componentPropertyEdit.error !== null ||
    !targetSupports('component-properties') ||
    !targetSupports('component-property-edit') ||
    !node.component ||
    typeof node.component.propertyEdits !== 'object' ||
    node.component.propertyEdits === null ||
    propertyName.trim().length === 0 ||
    FORBIDDEN_COMPONENT_PROPERTY_NAMES.has(propertyName) ||
    !['boolean', 'number', 'string'].includes(typeof value) ||
    (typeof value === 'number' && (!Number.isFinite(value) || Object.is(value, -0)))
  ) {
    return null;
  }
  let metadataDescriptor;
  try {
    metadataDescriptor = Object.getOwnPropertyDescriptor(node.component.propertyEdits, propertyName);
  } catch (_error) {
    return null;
  }
  const metadata = metadataDescriptor?.value;
  if (
    metadataDescriptor?.enumerable !== true ||
    metadataDescriptor.get !== undefined ||
    metadataDescriptor.set !== undefined ||
    typeof metadata !== 'object' ||
    metadata === null ||
    Array.isArray(metadata)
  ) {
    return null;
  }
  let componentTokenDescriptor;
  let snapshotRevisionDescriptor;
  try {
    componentTokenDescriptor = Object.getOwnPropertyDescriptor(metadata, 'componentToken');
    snapshotRevisionDescriptor = Object.getOwnPropertyDescriptor(metadata, 'snapshotRevision');
  } catch (_error) {
    return null;
  }
  const componentToken = componentTokenDescriptor?.value;
  const snapshotRevision = snapshotRevisionDescriptor?.value;
  if (
    componentTokenDescriptor?.enumerable !== true ||
    componentTokenDescriptor.get !== undefined ||
    componentTokenDescriptor.set !== undefined ||
    snapshotRevisionDescriptor?.enumerable !== true ||
    snapshotRevisionDescriptor.get !== undefined ||
    snapshotRevisionDescriptor.set !== undefined ||
    typeof componentToken !== 'string' ||
    !COMPONENT_PROPERTY_TOKEN_PATTERN.test(componentToken) ||
    !Number.isSafeInteger(snapshotRevision) ||
    snapshotRevision <= 0
  ) {
    return null;
  }
  return { componentToken, snapshotRevision };
}

function createComponentPropertyEditor(node, propertyName, value, metadata) {
  const valueType = typeof value;
  const form = document.createElement('form');
  form.className = 'component-property-editor';
  form.dataset.componentPropertyEditor = '';
  const label = document.createElement('label');
  label.className = 'component-property-label';
  const propertyNameLabel = document.createElement('span');
  propertyNameLabel.className = 'property-name';
  propertyNameLabel.textContent = propertyName;
  const separator = document.createElement('span');
  separator.setAttribute('aria-hidden', 'true');
  separator.textContent = ':';
  let editor;
  if (valueType === 'string') {
    editor = document.createElement('textarea');
    editor.className = 'component-property-input component-property-string-input';
    editor.setAttribute('aria-label', `Edit Valdi prop ${propertyName} as a JSON string literal`);
    editor.setAttribute('rows', '1');
    editor.setAttribute('spellcheck', 'false');
    editor.value = JSON.stringify(value);
  } else {
    editor = document.createElement('input');
    editor.className = 'component-property-input';
    editor.setAttribute('aria-label', `Edit Valdi prop ${propertyName}`);
    editor.setAttribute('type', valueType === 'boolean' ? 'checkbox' : 'number');
    if (valueType === 'boolean') editor.checked = value;
    else {
      editor.setAttribute('step', 'any');
      editor.value = String(value);
    }
  }
  editor.dataset.componentPropertyInput = '';
  const applyButton = document.createElement('button');
  applyButton.className = 'component-property-apply';
  applyButton.setAttribute('aria-label', `Apply Valdi prop ${propertyName}`);
  applyButton.setAttribute('type', 'submit');
  applyButton.disabled = state.componentPropertyEdit.pending;
  applyButton.textContent = 'Apply';
  label.append(propertyNameLabel);
  label.append(separator);
  label.append(editor);
  form.append(label);
  form.append(applyButton);
  componentPropertyEditorBindings.set(form, {
    componentId: node.id,
    componentToken: metadata.componentToken,
    propertyName,
    snapshotRevision: metadata.snapshotRevision,
    valueType,
  });
  return form;
}

function componentPropertyRows(node, editorModels) {
  const entries = Object.entries(node.component?.properties || {}).sort(([first], [second]) =>
    first.localeCompare(second),
  );
  if (!entries.length) return '<div class="empty-state">No properties available.</div>';
  return entries
    .map(([propertyName, value]) => {
      const metadata = componentPropertyEditMetadata(node, propertyName, value);
      if (metadata) {
        const editorIndex = editorModels.push({ metadata, node, propertyName, value }) - 1;
        return `<div class="component-property-editor-slot" data-component-property-editor-slot="${editorIndex}"></div>`;
      }
      return `<div class="property-row"><span class="property-name">${escapeHtml(propertyName)}</span>: ${renderValue(value)}</div>`;
    })
    .join('');
}

function hydrateComponentPropertyEditors(editorModels) {
  const slots = elements.inspector.querySelectorAll?.('[data-component-property-editor-slot]') || [];
  for (const slot of slots) {
    const editorIndex = Number(slot.dataset.componentPropertyEditorSlot);
    const model = Number.isSafeInteger(editorIndex) && editorIndex >= 0 ? editorModels[editorIndex] : undefined;
    if (model) {
      slot.replaceChildren(createComponentPropertyEditor(model.node, model.propertyName, model.value, model.metadata));
    }
  }
}

async function submitComponentPropertyEdit(componentId, propertyName, componentToken, snapshotRevision, value) {
  if (!state.target || state.componentPropertyEdit.pending) return;
  const node = findNode(componentId);
  let currentValue;
  try {
    currentValue = Object.getOwnPropertyDescriptor(node?.component?.properties, propertyName)?.value;
  } catch (_error) {
    return;
  }
  const metadata = node ? componentPropertyEditMetadata(node, propertyName, currentValue) : null;
  if (
    !node?.component ||
    metadata === null ||
    metadata.componentToken !== componentToken ||
    metadata.snapshotRevision !== snapshotRevision
  ) {
    return;
  }
  const requestTarget = state.target;
  const targetGeneration = state.targetGeneration;
  const snapshotGeneration = state.snapshotGeneration;
  const selectedNodeId = state.selectedNodeId;
  const operationGeneration = ++state.componentPropertyEdit.operationGeneration;
  const operationIsCurrent = () => state.componentPropertyEdit.operationGeneration === operationGeneration;
  const requestIsCurrent = () =>
    operationIsCurrent() &&
    state.targetGeneration === targetGeneration &&
    state.target?.id === requestTarget.id &&
    state.snapshotGeneration === snapshotGeneration &&
    state.selectedNodeId === selectedNodeId;
  state.componentPropertyEdit.focused = false;
  state.componentPropertyEdit.pending = true;
  state.componentPropertyEdit.error = null;
  state.snapshotRequestGeneration++;
  renderInspector();
  let updated = false;
  let refreshed = false;
  let refreshDeferredForRuntimeState = false;
  try {
    const result = await requestJson(
      '/api/devtools/component-property',
      {},
      {
        body: {
          ...targetIdentityParameters(requestTarget),
          componentId,
          componentToken,
          propertyName,
          protocolVersion: COMPONENT_PROPERTY_EDIT_PROTOCOL_VERSION,
          snapshotRevision,
          value,
        },
      },
    );
    if (requestIsCurrent()) {
      updated = result.updated === true;
      if (updated) {
        const refreshOutcome = await refreshSnapshotInternal(operationGeneration);
        refreshed = refreshOutcome === SNAPSHOT_REFRESH_APPLIED;
        refreshDeferredForRuntimeState = refreshOutcome === SNAPSHOT_REFRESH_RUNTIME_STATE_DEFERRED;
      }
    }
  } catch (error) {
    if (requestIsCurrent()) {
      state.componentPropertyEdit.error = error instanceof Error ? error.message : String(error);
    }
  } finally {
    if (operationIsCurrent()) {
      state.componentPropertyEdit.pending = false;
      state.componentPropertyEdit.focused = false;
      if (updated && !refreshed && !refreshDeferredForRuntimeState && state.componentPropertyEdit.error === null) {
        state.componentPropertyEdit.error = COMPONENT_PROPERTY_EDIT_ERROR;
      }
      if (!runtimeStatePresentationHasFocus()) renderInspector();
    }
  }
}

function readComponentPropertyEditorValue(editor, valueType) {
  if (valueType === 'boolean') return Boolean(editor.checked);
  if (valueType === 'number') {
    const input = editor.value.trim();
    if (!input) return undefined;
    const value = Number(input);
    return Number.isFinite(value) && !Object.is(value, -0) ? value : undefined;
  }
  if (valueType !== 'string') return undefined;
  try {
    const value = JSON.parse(editor.value);
    return typeof value === 'string' ? value : undefined;
  } catch (_error) {
    return undefined;
  }
}

function componentMetadata(node) {
  if (!node.component) return {};
  return {
    ...(node.component.elementId === undefined ? {} : { elementId: node.component.elementId }),
    key: node.component.key,
    name: node.component.name,
  };
}

function renderComponentProperties(node, editorModels) {
  if (node.component?.properties === undefined || !targetSupports('component-properties')) return '';
  const editable = targetSupports('component-property-edit') && state.componentPropertyEdit.error === null;
  return `
    <section class="component-properties" aria-label="Valdi props">
      <div class="rule-header">Valdi props <span class="rule-origin">${editable ? 'editable scalars' : 'read only'}</span></div>
      ${state.componentPropertyEdit.error ? `<div class="component-property-error" role="alert">${escapeHtml(state.componentPropertyEdit.error)}</div>` : ''}
      <div class="property-list">${componentPropertyRows(node, editorModels)}</div>
    </section>
  `;
}

function renderStyles(node) {
  const attributes = nodeAttributes(node);
  const domStyle = node.element?.dom?.attributes?.style
    ? valdiDebuggerTreeModel.formatValue(node.element.dom.attributes.style, 0)
    : '';
  return `
    <div class="rule-header">element.style <span class="rule-origin">Valdi ${escapeHtml(valdiDebuggerTreeModel.formatValue(node.tag, 0))}</span></div>
    <div>{</div><div class="property-list">${propertyRows(attributes, { css: true })}</div><div>}</div>
    ${
      domStyle
        ? `<div class="rule-header">DOM style <span class="rule-origin">rendered</span></div><div>{</div><div class="property-list">${domStyle
            .split(';')
            .map(rule => rule.trim())
            .filter(Boolean)
            .map(rule => {
              const [name, ...value] = rule.split(':');
              return `<div class="property-row"><span class="property-name">${escapeHtml(name)}</span>: ${escapeHtml(value.join(':').trim())};</div>`;
            })
            .join('')}</div><div>}</div>`
        : ''
    }
  `;
}

function edgeValues(attributes, prefix) {
  const all = attributes[prefix];
  return {
    top: attributes[`${prefix}Top`] ?? all ?? 0,
    right: attributes[`${prefix}Right`] ?? all ?? 0,
    bottom: attributes[`${prefix}Bottom`] ?? all ?? 0,
    left: attributes[`${prefix}Left`] ?? all ?? 0,
  };
}

function renderComputed(node) {
  const attributes = nodeAttributes(node);
  const bounds = node.bounds || {};
  const margin = edgeValues(attributes, 'margin');
  const padding = edgeValues(attributes, 'padding');
  const computed = {
    width: `${formatNumber(bounds.width)} px`,
    height: `${formatNumber(bounds.height)} px`,
    x: `${formatNumber(bounds.x)} px`,
    y: `${formatNumber(bounds.y)} px`,
    display: attributes.flexDirection ? 'flex' : 'block',
    position: attributes.position || 'relative',
    ...(attributes.flexDirection ? { 'flex-direction': attributes.flexDirection } : {}),
    ...(attributes.alignItems ? { 'align-items': attributes.alignItems } : {}),
    ...(attributes.justifyContent ? { 'justify-content': attributes.justifyContent } : {}),
    ...(attributes.gap !== undefined ? { gap: `${attributes.gap} px` } : {}),
    ...(attributes.backgroundColor ? { background: attributes.backgroundColor } : {}),
    ...(attributes.color ? { color: attributes.color } : {}),
  };
  return `
    <div class="box-model">
      <div class="box-layer margin"><span class="box-label">margin ${escapeHtml(valdiDebuggerTreeModel.formatValue(margin.top, 0))} ${escapeHtml(valdiDebuggerTreeModel.formatValue(margin.right, 0))} ${escapeHtml(valdiDebuggerTreeModel.formatValue(margin.bottom, 0))} ${escapeHtml(valdiDebuggerTreeModel.formatValue(margin.left, 0))}</span>
        <div class="box-layer border"><span class="box-label">border</span>
          <div class="box-layer padding"><span class="box-label">padding ${escapeHtml(valdiDebuggerTreeModel.formatValue(padding.top, 0))} ${escapeHtml(valdiDebuggerTreeModel.formatValue(padding.right, 0))} ${escapeHtml(valdiDebuggerTreeModel.formatValue(padding.bottom, 0))} ${escapeHtml(valdiDebuggerTreeModel.formatValue(padding.left, 0))}</span>
            <div class="box-layer content">${formatNumber(bounds.width)} × ${formatNumber(bounds.height)}</div>
          </div>
        </div>
      </div>
    </div>
    <table class="computed-list">${Object.entries(computed)
      .map(
        ([key, value]) =>
          `<tr><td>${escapeHtml(key)}</td><td>${escapeHtml(valdiDebuggerTreeModel.formatValue(value, 0))}</td></tr>`,
      )
      .join('')}</table>
  `;
}

function renderInspector() {
  const node = findNode(state.selectedNodeId);
  if (!node) {
    elements.inspector.innerHTML = '<div class="empty-state">Select a Valdi element to inspect it.</div>';
    return;
  }

  const renderedNode = inspectedNode(node);
  const componentPropertyEditorModels = [];
  const renderMarkup = markup => {
    elements.inspector.innerHTML = markup;
    hydrateComponentPropertyEditors(componentPropertyEditorModels);
  };
  if (state.activeDetail === 'state') {
    renderMarkup(renderSelectedRuntimeState(node));
    return;
  }
  const componentProperties = renderComponentProperties(node, componentPropertyEditorModels);
  if (node.component && renderedNode === node) {
    renderMarkup(
      `<div class="rule-header">Valdi component <span class="rule-origin">${escapeHtml(node.tag)}</span></div>${propertyRows(componentMetadata(node), { css: false })}${componentProperties}<div class="empty-state">This component does not currently render a backing element.</div>`,
    );
    return;
  }

  if (state.activeDetail === 'styles') {
    renderMarkup(`${componentProperties}${renderStyles(renderedNode)}`);
  } else if (state.activeDetail === 'computed') {
    renderMarkup(`${componentProperties}${renderComputed(renderedNode)}`);
  } else {
    const textContent = renderedNode.element?.dom?.textContent
      ? valdiDebuggerTreeModel.formatValue(renderedNode.element.dom.textContent, 0)
      : '';
    const componentDetails = node.component
      ? `<div class="rule-header">Valdi component <span class="rule-origin">${escapeHtml(node.tag)}</span></div>${propertyRows(componentMetadata(node), { css: false })}`
      : '';
    renderMarkup(
      `${componentDetails}${componentProperties}<div class="rule-header">Rendered &lt;${escapeHtml(valdiDebuggerTreeModel.formatValue(renderedNode.element?.dom?.tagName || 'div', 0))}&gt;</div>${propertyRows(renderedNode.element?.dom?.attributes, { css: false })}${textContent ? `<div class="rule-header">Text content</div><pre class="json-view">${escapeHtml(textContent)}</pre>` : ''}`,
    );
  }
}

function renderBreadcrumbs() {
  const path = findNodePath(state.selectedNodeId);
  elements.breadcrumbs.innerHTML = path
    .map(
      (node, index) =>
        `${index ? ' › ' : ''}<button class="breadcrumb" data-breadcrumb-id="${escapeHtml(nodeId(node))}">${escapeHtml(node.tag || 'view')}</button>`,
    )
    .join('');
  elements.nodeSummary.textContent = `${nodeCount()} nodes`;
}

function render() {
  renderTree();
  renderInspector();
  renderBreadcrumbs();
  if (state.activeSection === 'state') renderRuntimeStateSection();
}

function selectNode(id) {
  const node = findNode(id);
  if (!node) return;
  const selectedNodeId = nodeId(node);
  resetRuntimeStateForSelectionChange(selectedNodeId);
  state.selectedNodeId = selectedNodeId;
  revealPath(state.selectedNodeId);
  render();
}

function scrollSelectedTreeRowIntoView() {
  document.getElementById(treeRowId(state.selectedNodeId))?.scrollIntoView({ block: 'nearest', inline: 'nearest' });
}

function handleTreeNavigation(event) {
  if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) return;
  const key = event.key;
  if (!['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(key)) return;

  event.preventDefault();
  event.stopPropagation();
  const rows = Array.from(elements.tree.querySelectorAll('.tree-row'));
  if (!rows.length) return;

  const selectedIndex = rows.findIndex(row => row.dataset.nodeId === state.selectedNodeId);
  const selectedNode = findNode(state.selectedNodeId);
  let nextNodeId = null;

  if (key === 'ArrowUp') {
    nextNodeId = rows[Math.max(0, selectedIndex - 1)]?.dataset.nodeId;
  } else if (key === 'ArrowDown') {
    nextNodeId = rows[Math.min(rows.length - 1, selectedIndex + 1)]?.dataset.nodeId;
  } else if (key === 'Home') {
    nextNodeId = rows[0].dataset.nodeId;
  } else if (key === 'End') {
    nextNodeId = rows[rows.length - 1].dataset.nodeId;
  } else if (key === 'ArrowRight' && selectedNode) {
    const children = valdiDebuggerTreeModel.children(selectedNode);
    const expanded = Boolean(state.search.trim()) || state.expandedNodeIds.has(state.selectedNodeId);
    if (children.length && !expanded) {
      state.expandedNodeIds.add(state.selectedNodeId);
      renderTree();
    } else if (children.length) {
      nextNodeId = rows[selectedIndex + 1]?.dataset.nodeId;
    }
  } else if (key === 'ArrowLeft' && selectedNode) {
    if (
      valdiDebuggerTreeModel.hasChildren(selectedNode) &&
      state.expandedNodeIds.has(state.selectedNodeId) &&
      !state.search.trim()
    ) {
      state.expandedNodeIds.delete(state.selectedNodeId);
      renderTree();
    } else {
      const path = findNodePath(state.selectedNodeId);
      nextNodeId = path.length > 1 ? nodeId(path[path.length - 2]) : null;
    }
  }

  if (nextNodeId && nextNodeId !== state.selectedNodeId) selectNode(nextNodeId);
  scrollSelectedTreeRowIntoView();
}

function enqueueHighlightRequest(
  intentGeneration,
  target,
  targetGeneration,
  snapshotGeneration,
  identity,
  nodeIdValue,
) {
  const request = state.highlightRequestTail.then(async () => {
    if (
      intentGeneration !== state.highlightIntentGeneration ||
      state.targetGeneration !== targetGeneration ||
      state.target?.id !== target.id ||
      state.snapshotGeneration !== snapshotGeneration
    ) {
      return;
    }
    try {
      await requestJson(
        '/api/devtools/highlight',
        {},
        {
          body: {
            ...identity,
            ...(nodeIdValue ? { nodeId: nodeIdValue } : {}),
          },
        },
      );
      if (
        nodeIdValue === null &&
        intentGeneration === state.highlightIntentGeneration &&
        state.targetGeneration === targetGeneration
      ) {
        state.highlightMayBeActive = false;
      }
    } catch (error) {
      console.warn('Unable to update the inspected Valdi highlight.', error);
    }
  });
  state.highlightRequestTail = request.catch(error => {
    console.warn('Unable to order the inspected Valdi highlight request.', error);
  });
}

function queueHighlight(nodeIdValue) {
  if (
    !state.target ||
    !targetSupports('highlight') ||
    (state.hoveredNodeId === nodeIdValue &&
      state.hoveredSnapshotGeneration === state.snapshotGeneration &&
      !(nodeIdValue === null && state.highlightMayBeActive))
  )
    return;
  const target = state.target;
  const targetGeneration = state.targetGeneration;
  const identity = targetIdentityParameters(target);
  const snapshotGeneration = state.snapshotGeneration;
  const intentGeneration = ++state.highlightIntentGeneration;
  state.hoveredNodeId = nodeIdValue;
  state.hoveredSnapshotGeneration = snapshotGeneration;
  if (state.highlightTimer) window.clearTimeout(state.highlightTimer);
  state.highlightTimer = window.setTimeout(
    () => {
      state.highlightTimer = null;
      if (
        state.targetGeneration !== targetGeneration ||
        state.target?.id !== target.id ||
        state.snapshotGeneration !== snapshotGeneration
      )
        return;
      if (nodeIdValue !== null) state.highlightMayBeActive = true;
      enqueueHighlightRequest(intentGeneration, target, targetGeneration, snapshotGeneration, identity, nodeIdValue);
    },
    nodeIdValue ? 80 : 20,
  );
}

function renderPerformanceMetric(label, value) {
  return `<div class="metric-card"><div class="metric-label">${escapeHtml(label)}</div><div class="metric-value">${escapeHtml(value)}</div></div>`;
}

function recordPerformanceSample(data) {
  const uptimeMs = Number(data?.uptimeMs);
  if (!Number.isFinite(uptimeMs) || uptimeMs < 0) return;
  const samples = state.performance.samples;
  const previous = samples[samples.length - 1];
  if (previous?.uptimeMs === uptimeMs) return;
  if (previous && previous.uptimeMs > uptimeMs) samples.length = 0;
  const sample = { uptimeMs };
  for (const [property, value] of [
    ['heapUsedBytes', data?.memory?.usedBytes],
    ['layoutDurationMs', data?.mainThread?.layoutDurationMs],
    ['resourceCount', data?.resourceCount],
    ['scriptDurationMs', data?.mainThread?.scriptDurationMs],
    ['taskDurationMs', data?.mainThread?.taskDurationMs],
  ]) {
    const numeric = Number(value);
    if (Number.isFinite(numeric) && numeric >= 0) sample[property] = numeric;
  }
  samples.push(sample);
  if (samples.length > MAX_PERFORMANCE_SAMPLES) {
    samples.splice(0, samples.length - MAX_PERFORMANCE_SAMPLES);
  }
}

function performanceGraphPath(points, minimum, maximum) {
  if (!points.length) return '';
  const firstTime = points[0].time;
  const lastTime = points[points.length - 1].time;
  const timeSpan = Math.max(lastTime - firstTime, 1);
  const valueSpan = Math.max(maximum - minimum, 1);
  return points
    .map((point, index) => {
      const x = points.length === 1 ? 100 : ((point.time - firstTime) / timeSpan) * 100;
      const y = 27 - Math.max(0, Math.min(1, (point.value - minimum) / valueSpan)) * 23;
      return `${index === 0 ? 'M' : 'L'}${x.toFixed(2)} ${y.toFixed(2)}`;
    })
    .join(' ');
}

function renderSampledPerformanceMetric(label, kind, property, formattedValue) {
  const points = state.performance.samples
    .filter(sample => Number.isFinite(sample[property]))
    .map(sample => ({ time: sample.uptimeMs, value: sample[property] }));
  if (!points.length) return renderPerformanceMetric(label, formattedValue);
  const minimum = Math.min(...points.map(point => point.value));
  const maximum = Math.max(...points.map(point => point.value));
  const padding = Math.max((maximum - minimum) * 0.12, maximum * 0.015, 1);
  const path = performanceGraphPath(points, Math.max(0, minimum - padding), maximum + padding);
  return `
    <article class="metric-card metric-card-chart">
      <div class="metric-label">${escapeHtml(label)}</div>
      <div class="metric-value">${escapeHtml(formattedValue)}</div>
      <svg class="performance-sparkline ${escapeHtml(kind)}" viewBox="0 0 100 30" preserveAspectRatio="none" role="img" aria-label="${escapeHtml(label)} over time">
        <path class="performance-sparkline-baseline" d="M0 27H100" />
        <path class="performance-sparkline-area" d="${path} L100 27 L0 27 Z" />
        <path class="performance-sparkline-line" d="${path}" />
      </svg>
    </article>
  `;
}

function performanceScopeMatches(trace, scope) {
  const name = String(trace?.trace || '');
  if (scope === 'valdi') return name.startsWith('Valdi.');
  if (scope === 'browser') return name.startsWith('Browser.');
  return name.startsWith('Valdi.') || name.startsWith('Browser.');
}

function filteredPerformanceTraces(result) {
  const search = String(state.performance.traceSearch || '')
    .trim()
    .toLowerCase();
  const traces = Array.isArray(result?.traces) ? result.traces : [];
  return traces.filter(
    trace =>
      performanceScopeMatches(trace, state.performance.traceScope) &&
      (!search ||
        String(trace.trace || '')
          .toLowerCase()
          .includes(search)),
  );
}

function performanceScopeCounts(traces) {
  const counts = { all: 0, browser: 0, valdi: 0 };
  for (const trace of traces) {
    if (!performanceScopeMatches(trace, 'all')) continue;
    counts.all++;
    if (performanceScopeMatches(trace, 'valdi')) counts.valdi++;
    if (performanceScopeMatches(trace, 'browser')) counts.browser++;
  }
  return counts;
}

function performanceLane(trace) {
  const name = String(trace?.trace || '');
  if (name.startsWith('Valdi.')) return 'valdi';
  if (name.startsWith('Browser.Layout.')) return 'layout';
  if (name.startsWith('Browser.Paint.')) return 'paint';
  if (name.startsWith('Browser.Frames.')) return 'frames';
  if (name.startsWith('Browser.GC.')) return 'gc';
  return 'script';
}

function renderPerformanceTimeline(result) {
  const traces = Array.isArray(result?.traces) ? result.traces : [];
  const counts = performanceScopeCounts(traces);
  const scopes = [
    ['valdi', 'Valdi'],
    ['browser', 'Browser'],
    ['all', 'All'],
  ];
  const controls = `
    <div class="performance-trace-filters">
      <div class="performance-scope-group" role="group" aria-label="Trace event scope">
        ${scopes
          .map(
            ([scope, label]) =>
              `<button type="button" class="performance-scope-button${state.performance.traceScope === scope ? ' selected' : ''}" data-performance-scope="${scope}" aria-pressed="${state.performance.traceScope === scope ? 'true' : 'false'}">${label} <span>${escapeHtml(formatNumber(counts[scope]))}</span></button>`,
          )
          .join('')}
      </div>
      <input id="performanceTraceFilter" class="performance-trace-search" type="search" value="${escapeHtml(state.performance.traceSearch)}" placeholder="Filter trace names" aria-label="Filter trace names" />
    </div>
  `;
  if (!traces.length) {
    return `${controls}<div class="empty-state">Record an interaction to inspect browser and Valdi renderer events.</div>`;
  }
  const filtered = filteredPerformanceTraces(result);
  if (!filtered.length) return `${controls}<div class="empty-state">No captured events match this filter.</div>`;

  let firstTimestamp = Infinity;
  let lastTimestamp = -Infinity;
  for (const trace of filtered) {
    firstTimestamp = Math.min(firstTimestamp, Number(trace.startMicros));
    lastTimestamp = Math.max(lastTimestamp, Number(trace.endMicros));
  }
  const spanMicros = Math.max(lastTimestamp - firstTimestamp, 1000);
  const displayed = filtered.slice(0, MAX_PERFORMANCE_TIMELINE_ROWS);
  const rows = displayed
    .map(trace => {
      const startMicros = Number(trace.startMicros);
      const durationMicros = Math.max(0, Number(trace.endMicros) - startMicros);
      const offset = Math.max(0, Math.min(100, ((startMicros - firstTimestamp) / spanMicros) * 100));
      const width = Math.max(0.65, Math.min(100 - offset, (durationMicros / spanMicros) * 100));
      const duration = trace.type === 1 ? 'instant' : formatDuration(durationMicros / 1000);
      const name = String(trace.trace || '').replace(/^Browser\./, '');
      return `
        <div class="performance-timeline-row" title="${escapeHtml(trace.trace)}">
          <span class="performance-event-name">${escapeHtml(name)}</span>
          <span class="performance-event-track"><span class="performance-event-bar ${performanceLane(trace)}" style="left:${offset.toFixed(3)}%;width:${width.toFixed(3)}%"></span></span>
          <span class="performance-event-duration">${escapeHtml(duration)}</span>
        </div>
      `;
    })
    .join('');
  const truncated =
    filtered.length > displayed.length
      ? `<div class="performance-caption">Showing ${formatNumber(displayed.length)} of ${formatNumber(filtered.length)} matching events. Export includes every bounded event.</div>`
      : '';
  return `
    ${controls}
    <div class="performance-legend">
      <span class="performance-legend-item valdi">Valdi</span>
      <span class="performance-legend-item script">JavaScript</span>
      <span class="performance-legend-item layout">Layout</span>
      <span class="performance-legend-item paint">Paint</span>
      <span class="performance-legend-item frames">Frames</span>
      <span class="performance-legend-item gc">GC</span>
    </div>
    <div class="performance-timeline">${rows}</div>
    ${truncated}
  `;
}

function renderPerformanceSummary(result) {
  const grouped = new Map();
  for (const trace of filteredPerformanceTraces(result)) {
    const name = String(trace.trace || '');
    const event = grouped.get(name) || { count: 0, durationMs: 0, name };
    event.count++;
    event.durationMs += Math.max(0, Number(trace.endMicros) - Number(trace.startMicros)) / 1000;
    grouped.set(name, event);
  }
  const rows = Array.from(grouped.values())
    .sort((left, right) => right.durationMs - left.durationMs || right.count - left.count)
    .slice(0, MAX_PERFORMANCE_SUMMARY_ROWS)
    .map(
      event =>
        `<tr><td>${escapeHtml(event.name)}</td><td>${escapeHtml(formatNumber(event.count))}</td><td>${escapeHtml(formatDuration(event.durationMs))}</td></tr>`,
    )
    .join('');
  if (!rows) return '';
  return `
    <div class="utility-heading">Total captured duration by event</div>
    <table class="performance-results-table"><thead><tr><th>Operation</th><th>Calls</th><th>Inclusive total</th></tr></thead><tbody>${rows}</tbody></table>
  `;
}

function performanceButton(label, action, options = {}) {
  return `<button type="button" class="performance-button${options.primary ? ' primary' : ''}" data-performance-action="${escapeHtml(action)}"${options.disabled ? ' disabled' : ''}>${escapeHtml(label)}</button>`;
}

function replacePerformanceContent(html) {
  const contentScrollTop = elements.performanceContent.scrollTop;
  const previousTimeline = elements.performanceContent.querySelector?.('.performance-timeline');
  const timelineScrollLeft = previousTimeline?.scrollLeft ?? 0;
  const timelineScrollTop = previousTimeline?.scrollTop ?? 0;
  elements.performanceContent.innerHTML = html;
  elements.performanceContent.scrollTop = contentScrollTop;
  const nextTimeline = elements.performanceContent.querySelector?.('.performance-timeline');
  if (nextTimeline) {
    nextTimeline.scrollLeft = timelineScrollLeft;
    nextTimeline.scrollTop = timelineScrollTop;
  }
}

function renderPerformance(data = state.performance.data) {
  const perf = state.performance;
  if (!data) {
    const error = perf.error ? `<div class="performance-error" role="alert">${escapeHtml(perf.error)}</div>` : '';
    const ownerRecovery = perf.ownerIdentity
      ? `<section class="performance-recorder-card"><div class="performance-recorder-heading"><strong>Previous web preview recording</strong><span class="performance-status recording" role="status" aria-live="polite">${perf.traceActive ? 'Recording' : 'Result pending'}</span></div><div class="performance-recorder-controls">${performanceButton('Stop and retrieve', 'trace-stop', { disabled: perf.pending, primary: true })}</div></section>`
      : '';
    replacePerformanceContent(
      error || ownerRecovery
        ? `${error}${ownerRecovery}`
        : '<div class="empty-state">Loading web preview performance…</div>',
    );
    return;
  }
  perf.data = data;
  recordPerformanceSample(data);
  const browserMetrics = perf.lastTrace?.browserMetrics || {};
  const browserSummary = perf.lastTrace?.browserSummary || {};
  const metrics = [
    renderSampledPerformanceMetric('JS heap', 'heap', 'heapUsedBytes', formatBytes(data.memory?.usedBytes)),
    renderSampledPerformanceMetric(
      'Main-thread time (page lifetime)',
      'main-thread',
      'taskDurationMs',
      formatDuration(data.mainThread?.taskDurationMs),
    ),
    renderSampledPerformanceMetric(
      'JavaScript time (page lifetime)',
      'script',
      'scriptDurationMs',
      formatDuration(data.mainThread?.scriptDurationMs),
    ),
    renderSampledPerformanceMetric(
      'Layout time (page lifetime)',
      'layout',
      'layoutDurationMs',
      formatDuration(data.mainThread?.layoutDurationMs),
    ),
    renderSampledPerformanceMetric('Resources', 'resources', 'resourceCount', formatNumber(data.resourceCount)),
    renderPerformanceMetric('Page uptime', formatUptime(data.uptimeMs)),
  ];
  if (perf.lastTrace) {
    metrics.push(
      renderPerformanceMetric('Captured events', formatNumber(perf.lastTrace.traceCount)),
      renderPerformanceMetric('Long tasks', formatNumber(browserSummary.longTaskCount)),
      renderPerformanceMetric('Layout passes', formatNumber(browserMetrics.LayoutCount)),
    );
  }
  const hasTraceOwner = Boolean(perf.traceActive || perf.ownerIdentity);
  const rendererStatus = perf.rendererTracingEnabled
    ? '<span class="performance-tracing-badge enabled">Valdi renderer events enabled</span>'
    : '<span class="performance-tracing-badge disabled">Valdi renderer events disabled</span>';
  const rendererNotice = perf.rendererTracingEnabled
    ? ''
    : `<div class="performance-notice">Browser events can be recorded now. Valdi renderer events require reloading the inspected page; reloading can reset page state. ${performanceButton('Enable renderer events', 'enable-tracing', { disabled: perf.pending || hasTraceOwner })}</div>`;
  const traceStatus = `<span class="performance-status${hasTraceOwner ? ' recording' : ''}" role="status" aria-live="polite">${perf.traceActive ? 'Recording' : perf.ownerIdentity ? 'Result pending' : 'Idle'}</span>`;
  const traceCaption = perf.lastTrace
    ? `<div class="performance-caption">${escapeHtml(formatNumber(perf.lastTrace.traceCount))} events · ${escapeHtml(formatDuration(perf.lastTrace.elapsedMs))}${perf.lastTrace.droppedTraceEventCount ? ` · ${escapeHtml(formatNumber(perf.lastTrace.droppedTraceEventCount))} dropped` : ''}</div>`
    : '';
  const paints = (Array.isArray(data.paints) ? data.paints : [])
    .map(paint => `<tr><td>${escapeHtml(paint.name)}</td><td>${escapeHtml(formatDuration(paint.startTime))}</td></tr>`)
    .join('');
  replacePerformanceContent(`
    <div class="metric-grid">${metrics.join('')}</div>
    <div class="performance-caption">Main-thread, JavaScript, and layout counters are cumulative for the current page lifetime.</div>
    ${perf.error ? `<div class="performance-error" role="alert">${escapeHtml(perf.error)}</div>` : ''}
    <section class="performance-recorder-card">
      <div class="performance-recorder-heading"><div><strong>Rendering and main-thread trace</strong>${rendererStatus}</div>${traceStatus}</div>
      <div class="performance-recorder-controls">
        <label class="performance-duration">Duration <input id="performanceDurationInput" type="number" min="0.1" max="15" step="0.5" value="${escapeHtml(perf.durationSeconds)}" /> s</label>
        ${performanceButton('Start', 'trace-start', { disabled: perf.pending || hasTraceOwner })}
        ${performanceButton('Stop', 'trace-stop', { disabled: perf.pending || !hasTraceOwner, primary: hasTraceOwner })}
        ${performanceButton('Capture', 'trace-capture', { disabled: perf.pending || hasTraceOwner, primary: !hasTraceOwner })}
        ${performanceButton('Export trace', 'trace-export', { disabled: !perf.lastTrace })}
      </div>
      ${rendererNotice}
      ${traceCaption}
      ${renderPerformanceTimeline(perf.lastTrace)}
      ${renderPerformanceSummary(perf.lastTrace)}
    </section>
    <details class="performance-navigation"${perf.navigationExpanded ? ' open' : ''}>
      <summary>Initial page-load milestones</summary>
      <div class="performance-caption">DOM ready ${escapeHtml(formatDuration(data.navigation?.domContentLoadedMs))} · Page load ${escapeHtml(formatDuration(data.navigation?.loadMs))} · Transferred ${escapeHtml(formatBytes(data.transferSize))}</div>
      ${paints ? `<table class="performance-results-table"><tbody>${paints}</tbody></table>` : '<div class="empty-state">No paint milestones have been reported.</div>'}
    </details>
  `);
}

function buildPerformanceTraceExport(result) {
  const traces = Array.isArray(result?.traces) ? result.traces : [];
  const firstTimestamp = traces.reduce(
    (minimum, trace) => Math.min(minimum, Number(trace.startMicros)),
    Number(traces[0]?.startMicros || 0),
  );
  const threadIds = Array.from(new Set(traces.map(trace => Number(trace.threadId)))).slice(0, 256);
  const traceEvents = [
    { args: { name: 'Valdi web preview' }, name: 'process_name', ph: 'M', pid: 1 },
    ...threadIds.map(threadId => ({
      args: { name: `Thread ${threadId}` },
      name: 'thread_name',
      ph: 'M',
      pid: 1,
      tid: threadId,
    })),
  ];
  for (const trace of traces) {
    const instant = trace.type === 1;
    const event = {
      cat: String(trace.trace || '').startsWith('Valdi.') ? 'valdi' : 'browser',
      name: String(trace.trace || ''),
      ph: instant ? 'i' : 'X',
      pid: 1,
      tid: Number(trace.threadId),
      ts: Number(trace.startMicros) - firstTimestamp,
    };
    if (instant) event.s = 't';
    else event.dur = Math.max(0, Number(trace.endMicros) - Number(trace.startMicros));
    traceEvents.push(event);
  }
  return {
    displayTimeUnit: 'ms',
    metadata: result?.perfettoMetadata || {},
    traceEvents,
  };
}

function downloadPerformanceTrace(result) {
  const blob = new Blob([JSON.stringify(buildPerformanceTraceExport(result), null, 2)], {
    type: 'application/json',
  });
  const artifactUrl = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = artifactUrl;
  link.download = `valdi-web-preview-${Date.now()}.trace.json`;
  link.click();
  window.setTimeout(() => URL.revokeObjectURL(artifactUrl), 1000);
}

async function refreshPerformance(options = {}) {
  if (options.silent && performancePollingInputIsFocused()) return;
  if (
    !state.target ||
    !targetSupports('performance') ||
    state.performance.pending ||
    state.performance.snapshotPending
  ) {
    return;
  }
  const perf = state.performance;
  const identity = performanceIdentity();
  const targetGeneration = state.targetGeneration;
  const requestGeneration = ++perf.requestGeneration;
  const requestIsCurrent = () =>
    targetGeneration === state.targetGeneration &&
    requestGeneration === perf.requestGeneration &&
    performanceIdentityIsCurrent(identity);
  perf.snapshotPending = true;
  if (!options.silent && !perf.data) renderPerformance();
  try {
    const [data, status] = await Promise.all([
      requestJson('/api/devtools/performance/snapshot', identity, {}),
      requestJson('/api/devtools/performance/trace/status', identity, {}),
    ]);
    if (status.completionError) {
      try {
        await requestJson('/api/devtools/performance/trace/stop', identity, { body: {} });
      } catch {
        // Stop surfaces the retained completion error after clearing it on the server.
      }
      if (requestIsCurrent() && (!perf.ownerIdentity || samePerformanceIdentity(perf.ownerIdentity, identity))) {
        perf.traceActive = false;
        perf.ownerIdentity = null;
      }
      throw new Error(status.completionError);
    }
    let completedTrace = null;
    if (status.completedRecordingAvailable) {
      completedTrace = await requestJson('/api/devtools/performance/trace/stop', identity, { body: {} });
    }
    if (!requestIsCurrent()) return;
    perf.data = data;
    if (!perf.ownerIdentity || samePerformanceIdentity(perf.ownerIdentity, identity)) {
      perf.traceActive = Boolean(status.recording);
      perf.ownerIdentity = perf.traceActive ? identity : null;
    }
    perf.rendererTracingEnabled = Boolean(data.rendererTracingEnabled || status.rendererTracingEnabled);
    if (completedTrace) {
      perf.traceActive = false;
      perf.ownerIdentity = null;
      perf.lastTrace = completedTrace;
    }
    perf.error = null;
    renderPerformance(data);
  } catch (error) {
    if (requestIsCurrent()) {
      perf.error = error instanceof Error ? error.message : String(error);
      renderPerformance(perf.data);
    }
  } finally {
    if (requestIsCurrent()) perf.snapshotPending = false;
  }
}

function retainStalePerformanceOwnerAfterCleanupFailure(identity, cleanupError, operationGeneration) {
  const perf = state.performance;
  if (perf.ownerIdentity) {
    const ownerDescription = samePerformanceIdentity(perf.ownerIdentity, identity)
      ? 'the same recording is already retained by a newer operation'
      : 'a different recording is already owned by a newer operation';
    console.warn(`Unable to retain a stale Performance recording because ${ownerDescription}.`, cleanupError);
    return;
  }
  if (operationGeneration !== perf.operationGeneration && perf.pending) {
    console.warn('Unable to retain a stale Performance recording while a newer operation is pending.', cleanupError);
    return;
  }

  // Exact cleanup failed. Claim a fresh repair generation only when no newer
  // operation or owner needs the state, then retain the orphaned exact owner.
  const repairGeneration =
    operationGeneration === perf.operationGeneration ? operationGeneration : ++perf.operationGeneration;
  if (repairGeneration !== perf.operationGeneration) return;
  perf.traceActive = true;
  perf.ownerIdentity = identity;
  perf.error = `The previous web preview still owns a performance recording: ${cleanupError instanceof Error ? cleanupError.message : String(cleanupError)}`;
  updateCapabilityUi();
  if (state.activeSection === 'performance') renderPerformance(perf.data);
}

async function runPerformanceAction(action) {
  const perf = state.performance;
  if (action === 'refresh') {
    await refreshPerformance();
    return;
  }
  if (action === 'trace-export') {
    if (perf.lastTrace) downloadPerformanceTrace(perf.lastTrace);
    return;
  }
  const stoppingPreviousOwner = action === 'trace-stop' && perf.ownerIdentity;
  if (perf.pending || (!stoppingPreviousOwner && (!state.target || !targetSupports('performance')))) return;
  if (['enable-tracing', 'trace-capture', 'trace-start'].includes(action) && (perf.traceActive || perf.ownerIdentity)) {
    return;
  }
  if (
    action === 'enable-tracing' &&
    !window.confirm('Enable Valdi renderer events? This reloads the inspected page and can reset page state.')
  ) {
    return;
  }

  const selectedIdentity = state.target && targetSupports('performance') ? performanceIdentity() : null;
  const targetGeneration = state.targetGeneration;
  const identity = stoppingPreviousOwner ? perf.ownerIdentity : selectedIdentity;
  if (!identity) return;
  perf.requestGeneration++;
  perf.snapshotPending = false;
  const operationGeneration = ++perf.operationGeneration;
  const operationIsCurrent = () => operationGeneration === perf.operationGeneration;
  const selectedTargetIsCurrent = () =>
    operationIsCurrent() &&
    selectedIdentity !== null &&
    targetGeneration === state.targetGeneration &&
    performanceIdentityIsCurrent(selectedIdentity);
  const operationOwnsTrace = () => samePerformanceIdentity(perf.ownerIdentity, identity);
  let refreshSelectedTarget = false;
  perf.pending = true;
  perf.error = null;
  renderPerformance(perf.data);
  try {
    if (action === 'enable-tracing') {
      await requestJson('/api/devtools/performance/trace/enable', identity, { body: {} });
      if (!selectedTargetIsCurrent()) return;
      perf.rendererTracingEnabled = true;
      addConsoleEntry('info', 'Reloaded the inspected page with Valdi renderer events enabled.');
    } else {
      const operation = action.slice('trace-'.length);
      const durationMs = Math.round(Math.max(0.1, Math.min(15, Number(perf.durationSeconds) || 3)) * 1000);
      if (operation === 'capture') {
        perf.traceActive = true;
        perf.ownerIdentity = identity;
        renderPerformance(perf.data);
      }
      const result = await requestJson(`/api/devtools/performance/trace/${operation}`, identity, {
        body: operation === 'capture' ? { durationMs } : {},
      });
      if (operation === 'start') {
        if (!selectedTargetIsCurrent()) {
          if (result.recording) {
            try {
              await requestJson('/api/devtools/performance/trace/stop', identity, { body: {} });
            } catch (cleanupError) {
              retainStalePerformanceOwnerAfterCleanupFailure(identity, cleanupError, operationGeneration);
            }
          }
          return;
        }
        perf.traceActive = Boolean(result.recording);
        perf.ownerIdentity = perf.traceActive ? identity : null;
        refreshSelectedTarget = true;
      } else {
        if (!operationIsCurrent()) return;
        const completedOwnedTrace = ['capture', 'stop'].includes(operation) && operationOwnsTrace();
        const completedPreviousOwner = completedOwnedTrace && !samePerformanceIdentity(identity, selectedIdentity);
        if (completedOwnedTrace) {
          perf.traceActive = false;
          perf.ownerIdentity = null;
        }
        if (completedPreviousOwner) {
          perf.data = null;
          perf.lastTrace = null;
          perf.rendererTracingEnabled = false;
          perf.samples = [];
        }
        if (selectedTargetIsCurrent() && !completedPreviousOwner) {
          perf.traceActive = false;
          perf.ownerIdentity = null;
          perf.lastTrace = result;
        }
        refreshSelectedTarget = selectedTargetIsCurrent();
      }
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    if (action === 'trace-capture' && selectedTargetIsCurrent()) refreshSelectedTarget = true;
    if (operationIsCurrent() && (selectedTargetIsCurrent() || operationOwnsTrace())) {
      perf.error = message;
      if (state.activeSection === 'performance') renderPerformance(perf.data);
    } else {
      console.warn('Ignoring a stale web preview performance action error.', error);
    }
  } finally {
    if (operationIsCurrent()) {
      perf.pending = false;
      if (state.activeSection === 'performance') renderPerformance(perf.data);
      updateCapabilityUi();
    }
  }
  if (refreshSelectedTarget && selectedTargetIsCurrent() && state.target) {
    await refreshPerformance({ silent: true });
  }
}

function setActiveSection(section) {
  const activeSection = sectionIsAvailable(section) ? section : 'elements';
  state.activeSection = activeSection;
  for (const tab of elements.mainTabs) {
    const selected = tab.dataset.section === activeSection;
    tab.classList.toggle('selected', selected);
    tab.setAttribute('aria-selected', String(selected));
    tab.tabIndex = selected ? 0 : -1;
  }
  for (const panel of elements.sections) {
    const selected = panel.dataset.panel === activeSection;
    panel.classList.toggle('selected', selected);
    panel.hidden = !selected;
  }
  if (activeSection === 'console') elements.consoleInput.focus();
  if (activeSection === 'elements') void refreshSnapshot();
  if (activeSection === 'state') {
    renderRuntimeStateSection();
    void refreshSnapshot();
  }
  if (activeSection === 'performance') {
    renderPerformance();
    void refreshPerformance();
  }
}

function handleMainTabNavigation(event) {
  if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
  const tabs = elements.mainTabs.filter(tab => !tab.disabled);
  if (!tabs.length) return;
  event.preventDefault();
  const currentIndex = Math.max(0, tabs.indexOf(event.currentTarget));
  const nextIndex =
    event.key === 'Home'
      ? 0
      : event.key === 'End'
        ? tabs.length - 1
        : (currentIndex + (event.key === 'ArrowRight' ? 1 : -1) + tabs.length) % tabs.length;
  const nextTab = tabs[nextIndex];
  setActiveSection(nextTab.dataset.section);
  nextTab.focus();
}

function setActiveDetail(detail) {
  state.activeDetail = detail;
  for (const tab of elements.detailTabs) {
    const selected = tab.dataset.detail === detail;
    tab.classList.toggle('selected', selected);
    tab.setAttribute('aria-selected', String(selected));
    tab.tabIndex = selected ? 0 : -1;
    if (selected && tab.id) elements.inspector.setAttribute('aria-labelledby', tab.id);
  }
  renderInspector();
}

function handleDetailTabNavigation(event) {
  if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
  const tabs = elements.detailTabs.filter(tab => !tab.disabled);
  if (!tabs.length) return;
  event.preventDefault();
  const currentIndex = Math.max(0, tabs.indexOf(event.currentTarget));
  const nextIndex =
    event.key === 'Home'
      ? 0
      : event.key === 'End'
        ? tabs.length - 1
        : (currentIndex + (event.key === 'ArrowRight' ? 1 : -1) + tabs.length) % tabs.length;
  const nextTab = tabs[nextIndex];
  setActiveDetail(nextTab.dataset.detail);
  nextTab.focus();
}

function stopConsoleStream() {
  if (state.consoleStream) state.consoleStream.close();
  state.consoleStream = null;
  state.consoleStreamTargetKey = null;
}

function startConsoleStream() {
  if (!state.target || !state.autoRefresh || !targetSupports('console')) {
    stopConsoleStream();
    return;
  }
  const target = state.target;
  const targetGeneration = state.targetGeneration;
  const identity = targetIdentityParameters(target);
  const targetKey = `${targetGeneration}:${targetIdentityKey(target)}`;
  if (state.consoleStream && state.consoleStreamTargetKey === targetKey) return;
  stopConsoleStream();

  const url = new URL('/api/devtools/console/stream', window.location.origin);
  for (const [key, value] of Object.entries(identity)) url.searchParams.set(key, String(value));
  const stream = new EventSource(debuggerEventSourceUrl(url.toString()));
  state.consoleStream = stream;
  state.consoleStreamTargetKey = targetKey;

  stream.addEventListener('console', event => {
    if (state.consoleStream !== stream || state.targetGeneration !== targetGeneration || state.target?.id !== target.id)
      return;
    let entry;
    try {
      entry = JSON.parse(event.data);
    } catch (error) {
      console.warn('[Valdi DevTools] Ignoring a malformed Chromium console event.', error);
      return;
    }
    if (typeof entry !== 'object' || entry === null || Array.isArray(entry)) return;
    if (!targetEventMatches(target, entry) || typeof entry.message !== 'string') {
      return;
    }
    addConsoleEntry(entry.level, entry.message, entry.timestamp, entry.source);
  });

  stream.addEventListener('stream-error', event => {
    if (state.consoleStream !== stream || state.targetGeneration !== targetGeneration || state.target?.id !== target.id)
      return;
    try {
      const payload = JSON.parse(event.data);
      if (typeof payload !== 'object' || payload === null || Array.isArray(payload)) return;
      if (targetEventMatches(target, payload) && typeof payload.error === 'string') {
        addConsoleEntry('error', payload.error);
      }
    } catch (error) {
      console.warn('[Valdi DevTools] Ignoring a malformed Chromium console stream error.', error);
    }
  });

  stream.addEventListener('stream-warning', event => {
    if (state.consoleStream !== stream || state.targetGeneration !== targetGeneration || state.target?.id !== target.id)
      return;
    try {
      const payload = JSON.parse(event.data);
      if (typeof payload !== 'object' || payload === null || Array.isArray(payload)) return;
      if (targetEventMatches(target, payload) && typeof payload.message === 'string') {
        addConsoleEntry('warn', payload.message);
      }
    } catch (error) {
      console.warn('[Valdi DevTools] Ignoring a malformed Chromium console stream warning.', error);
    }
  });
}

function addConsoleEntry(kind, value, timestamp, source) {
  const normalizedKind = ['debug', 'error', 'info', 'input', 'log', 'result', 'warn'].includes(kind) ? kind : 'log';
  const text = String(value);
  const boundedValue =
    text.length > MAX_CONSOLE_ENTRY_CHARACTERS ? `${text.slice(0, MAX_CONSOLE_ENTRY_CHARACTERS - 1)}…` : text;
  const key = timestamp === undefined ? null : `${timestamp}:${normalizedKind}:${String(source ?? '')}:${boundedValue}`;
  if (key !== null) {
    if (state.consoleEntryKeys.has(key)) return;
    state.consoleEntryKeys.add(key);
  }
  state.consoleEntries.push({ key, kind: normalizedKind, value: boundedValue });
  if (state.consoleEntries.length > MAX_CONSOLE_ENTRIES) {
    const discarded = state.consoleEntries.splice(0, state.consoleEntries.length - MAX_CONSOLE_ENTRIES);
    for (const entry of discarded) {
      if (entry.key !== null) state.consoleEntryKeys.delete(entry.key);
    }
  }
  elements.consoleMessages.innerHTML = state.consoleEntries
    .map(
      entry =>
        `<div class="console-entry ${escapeHtml(entry.kind)}"><span class="console-chevron">${entry.kind === 'input' ? '›' : entry.kind === 'error' ? '×' : entry.kind === 'warn' ? '!' : '‹'}</span><pre>${escapeHtml(entry.value)}</pre></div>`,
    )
    .join('');
  elements.consoleMessages.scrollTop = elements.consoleMessages.scrollHeight;
}

function clearConsole() {
  state.consoleEntries = [];
  state.consoleEntryKeys.clear();
  elements.consoleMessages.innerHTML = '';
}

async function evaluateConsoleExpression(expression) {
  if (!state.target || !targetSupports('console')) return;
  const target = state.target;
  const targetGeneration = state.targetGeneration;
  const identity = targetIdentityParameters(target);
  const requestIsCurrent = () => state.targetGeneration === targetGeneration && state.target?.id === target.id;
  addConsoleEntry('input', expression);
  try {
    const result = await requestJson(
      '/api/devtools/evaluate',
      {},
      {
        body: {
          expression,
          ...identity,
        },
      },
    );
    if (!requestIsCurrent()) return;
    const serialized = result.type === 'undefined' ? undefined : JSON.stringify(result.value, null, 2);
    const value = result.type === 'undefined' ? 'undefined' : (serialized ?? String(result.value));
    addConsoleEntry('result', value);
  } catch (error) {
    if (requestIsCurrent()) addConsoleEntry('error', error instanceof Error ? error.message : String(error));
  }
}

function startSplitResize(event) {
  event.preventDefault();
  const bounds = elements.elementsSection.getBoundingClientRect();
  function resize(moveEvent) {
    const treeHeight = Math.max(120, Math.min(bounds.height - 200, moveEvent.clientY - bounds.top));
    elements.elementsSection.style.gridTemplateRows = `${treeHeight}px 5px minmax(170px, 1fr) 24px`;
  }
  function stopResize() {
    window.removeEventListener('pointermove', resize);
    window.removeEventListener('pointerup', stopResize);
  }
  window.addEventListener('pointermove', resize);
  window.addEventListener('pointerup', stopResize);
}

function wireEvents() {
  for (const tab of elements.mainTabs) {
    tab.addEventListener('click', () => setActiveSection(tab.dataset.section));
    tab.addEventListener('keydown', handleMainTabNavigation);
  }
  for (const tab of elements.detailTabs) {
    tab.addEventListener('click', () => setActiveDetail(tab.dataset.detail));
    tab.addEventListener('keydown', handleDetailTabNavigation);
  }
  elements.targetSelect.addEventListener('change', () => {
    if (!isDirectMode()) return;
    const targetId = elements.targetSelect.value;
    if (!targetId) {
      applyDirectTargetSelection(null);
      return;
    }
    const target = state.registryTargets.find(candidate => candidate.id === targetId);
    if (!target || !isSelectableDirectTarget(target)) {
      state.targetSwitchMessage = target
        ? directTargetUnavailableReason(target)
        : 'The requested target is unavailable.';
      renderTargetPicker();
      return;
    }
    state.unavailableTargetId = null;
    applyDirectTargetSelection(target);
  });
  elements.refreshButton.addEventListener('click', () => {
    if (isDirectMode()) void refreshTargetRegistry();
    if (state.activeSection === 'performance') void refreshPerformance();
    else void refreshSnapshot();
  });
  for (const button of document.querySelectorAll('.section-header [data-performance-action]')) {
    button.addEventListener('click', () => void runPerformanceAction(button.dataset.performanceAction));
  }
  elements.performanceContent.addEventListener('click', event => {
    const scope = event.target.closest('[data-performance-scope]');
    if (scope) {
      state.performance.traceScope = scope.dataset.performanceScope;
      renderPerformance();
      return;
    }
    const button = event.target.closest('[data-performance-action]');
    if (button && !button.disabled) void runPerformanceAction(button.dataset.performanceAction);
  });
  elements.performanceContent.addEventListener('input', event => {
    if (event.target.id === 'performanceDurationInput') {
      state.performance.durationSeconds = Math.max(0.1, Math.min(15, Number(event.target.value) || 3));
      return;
    }
    if (event.target.id !== 'performanceTraceFilter') return;
    const selectionStart = event.target.selectionStart;
    const selectionEnd = event.target.selectionEnd;
    state.performance.traceSearch = event.target.value;
    renderPerformance();
    const filter = document.getElementById('performanceTraceFilter');
    filter?.focus();
    if (selectionStart !== null && selectionEnd !== null) filter?.setSelectionRange(selectionStart, selectionEnd);
  });
  elements.performanceContent.addEventListener(
    'toggle',
    event => {
      if (event.target?.tagName === 'DETAILS' && event.target.classList.contains('performance-navigation')) {
        state.performance.navigationExpanded = event.target.open;
      }
    },
    true,
  );
  elements.autoRefreshToggle.addEventListener('change', () => {
    state.autoRefresh = elements.autoRefreshToggle.checked;
    if (state.autoRefresh) {
      startConsoleStream();
    } else {
      stopConsoleStream();
    }
  });
  elements.treeFilter.addEventListener('input', () => {
    state.search = elements.treeFilter.value;
    renderTree();
  });
  elements.stateFilter.addEventListener('input', () => {
    state.runtimeState.search = elements.stateFilter.value;
    renderRuntimeStateSection();
  });
  elements.stateContent.addEventListener('click', event => {
    const button = event.target.closest?.('[data-runtime-state-inspect]');
    if (!button) return;
    event.preventDefault();
    event.stopPropagation();
    inspectRuntimeStateBinding(runtimeStateInspectBindings.get(button));
  });
  elements.stateContent.addEventListener('toggle', event => updateRuntimeStateDisclosure(event.target), true);
  elements.expandButton.addEventListener('click', () => {
    expandUsefulNodes(state.snapshot?.tree);
    if (state.selectedNodeId) revealPath(state.selectedNodeId);
    renderTree();
  });
  elements.tree.addEventListener('keydown', handleTreeNavigation);
  elements.tree.addEventListener('click', event => {
    const toggle = event.target.closest('[data-toggle-id]');
    if (toggle) {
      elements.tree.focus({ preventScroll: true });
      const id = toggle.dataset.toggleId;
      if (state.expandedNodeIds.has(id)) {
        state.expandedNodeIds.delete(id);
      } else {
        state.expandedNodeIds.add(id);
      }
      renderTree();
      return;
    }
    const row = event.target.closest('[data-node-id]');
    if (row) {
      elements.tree.focus({ preventScroll: true });
      selectNode(row.dataset.nodeId);
    }
  });
  elements.tree.addEventListener('pointerover', event => {
    const row = event.target.closest('[data-node-id]');
    if (row) queueHighlight(inspectedNodeId(findNode(row.dataset.nodeId)));
  });
  elements.tree.addEventListener('pointerleave', () => queueHighlight(null));
  elements.inspector.addEventListener('focusin', event => {
    if (event.target.closest?.('[data-component-property-editor]')) {
      state.componentPropertyEdit.focused = true;
    }
  });
  elements.inspector.addEventListener('focusout', () => {
    window.setTimeout(() => {
      state.componentPropertyEdit.focused = Boolean(
        document.activeElement?.closest?.('[data-component-property-editor]'),
      );
    }, 0);
  });
  elements.inspector.addEventListener('submit', event => {
    const form = event.target.closest?.('[data-component-property-editor]');
    if (!form) return;
    event.preventDefault();
    const binding = componentPropertyEditorBindings.get(form);
    const editor = form.querySelector('[data-component-property-input]');
    const value = editor && binding ? readComponentPropertyEditorValue(editor, binding.valueType) : undefined;
    if (
      !binding ||
      value === undefined ||
      !Number.isSafeInteger(binding.snapshotRevision) ||
      binding.snapshotRevision <= 0 ||
      !COMPONENT_PROPERTY_TOKEN_PATTERN.test(binding.componentToken)
    ) {
      state.componentPropertyEdit.error = 'Enter a valid scalar value before applying this property.';
      state.componentPropertyEdit.focused = false;
      renderInspector();
      return;
    }
    void submitComponentPropertyEdit(
      binding.componentId,
      binding.propertyName,
      binding.componentToken,
      binding.snapshotRevision,
      value,
    );
  });
  elements.inspector.addEventListener('toggle', event => updateRuntimeStateDisclosure(event.target), true);
  elements.breadcrumbs.addEventListener('click', event => {
    const button = event.target.closest('[data-breadcrumb-id]');
    if (button) selectNode(button.dataset.breadcrumbId);
  });
  elements.copyNodeButton.addEventListener('click', async () => {
    const json = selectedNodeProjectionJson();
    if (json) await navigator.clipboard.writeText(json);
  });
  elements.splitHandle.addEventListener('pointerdown', startSplitResize);
  elements.clearConsoleButton.addEventListener('click', clearConsole);
  elements.consoleForm.addEventListener('submit', event => {
    event.preventDefault();
    const expression = elements.consoleInput.value.trim();
    if (!expression || !state.target || !targetSupports('console')) return;
    state.consoleHistory.push(expression);
    if (state.consoleHistory.length > MAX_CONSOLE_HISTORY_ENTRIES) {
      state.consoleHistory.splice(0, state.consoleHistory.length - MAX_CONSOLE_HISTORY_ENTRIES);
    }
    state.consoleHistoryIndex = state.consoleHistory.length;
    elements.consoleInput.value = '';
    void evaluateConsoleExpression(expression);
  });
  elements.consoleInput.addEventListener('keydown', event => {
    if (event.key !== 'ArrowUp' && event.key !== 'ArrowDown') return;
    event.preventDefault();
    state.consoleHistoryIndex = Math.max(
      0,
      Math.min(state.consoleHistory.length, state.consoleHistoryIndex + (event.key === 'ArrowUp' ? -1 : 1)),
    );
    elements.consoleInput.value = state.consoleHistory[state.consoleHistoryIndex] || '';
  });
  window.addEventListener('message', event => {
    if (
      event.source === window.parent &&
      event.origin.startsWith('chrome-extension://') &&
      event.data?.channel === 'valdi-devtools-theme'
    ) {
      applyTheme(event.data.theme);
    }
  });
  window.addEventListener('pagehide', () => {
    stopConsoleStream();
    stopPerformanceOnPageHide();
  });
  document.addEventListener('visibilitychange', () => {
    if (!document.hidden && isDirectMode()) void refreshTargetRegistry();
    if (!document.hidden && (state.activeSection === 'elements' || state.activeSection === 'state')) {
      void refreshSnapshot();
    }
    if (!document.hidden && state.activeSection === 'performance') void refreshPerformance({ silent: true });
  });
}

applyTheme(query.get('theme'));
wireEvents();
void connectToInspectedApplication();
