const query = new URLSearchParams(window.location.search);
const inspectedUrl = query.get('inspectedUrl');
const inspectedTargetNonce = query.get('targetNonce');
const MAX_CONSOLE_ENTRIES = 500;
const MAX_CONSOLE_ENTRY_CHARACTERS = 50_000;
const MAX_CONSOLE_HISTORY_ENTRIES = 100;
const MAX_PERFORMANCE_SAMPLES = 120;
const MAX_PERFORMANCE_TIMELINE_ROWS = 120;
const MAX_PERFORMANCE_SUMMARY_ROWS = 12;

const state = {
  target: null,
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
  error: null,
};

const elements = {
  mainTabs: Array.from(document.querySelectorAll('.main-tab')),
  detailTabs: Array.from(document.querySelectorAll('.detail-tab')),
  sections: Array.from(document.querySelectorAll('.section')),
  targetStatusDot: document.getElementById('targetStatusDot'),
  targetName: document.getElementById('targetName'),
  targetMetadata: document.getElementById('targetMetadata'),
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
  consoleMessages: document.getElementById('consoleMessages'),
  consoleForm: document.getElementById('consoleForm'),
  consoleInput: document.getElementById('consoleInput'),
  performanceContent: document.getElementById('performanceContent'),
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
    ...(options.body
      ? {
          body: JSON.stringify(options.body),
          headers: { 'Content-Type': 'application/json' },
        }
      : {}),
  });
  const payload = await response.json();
  if (!response.ok || payload.error) {
    throw new Error(payload.error || `Request failed: ${response.status}`);
  }
  return payload;
}

function stopPerformanceOnPageHide() {
  const identity = state.performance.ownerIdentity;
  if (!identity) return;
  const url = new URL('/api/devtools/performance/trace/stop', window.location.origin);
  for (const [key, value] of Object.entries(identity)) url.searchParams.set(key, String(value));
  void fetch(url, {
    body: '{}',
    headers: { 'Content-Type': 'application/json' },
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
  if (!target?.sessionId || !inspectedUrl || !inspectedTargetNonce) {
    throw new Error('The selected web preview does not have a complete performance identity.');
  }
  return {
    inspectedUrl,
    sessionId: target.sessionId,
    targetNonce: inspectedTargetNonce,
  };
}

function samePerformanceIdentity(left, right) {
  return Boolean(
    left &&
      right &&
      left.sessionId === right.sessionId &&
      left.inspectedUrl === right.inspectedUrl &&
      left.targetNonce === right.targetNonce,
  );
}

function performanceIdentityIsCurrent(identity) {
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
  perf.rendererTracingEnabled = false;
  perf.samples = [];
  if (perf.traceActive || perf.ownerIdentity) {
    perf.error = 'The previous web preview still owns a performance recording. Stop and retrieve it before switching.';
    return;
  }
  perf.error = null;
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
  if (message) elements.targetName.textContent = message;
}

function reportError(error) {
  const message = error instanceof Error ? error.message : String(error);
  state.error = message;
  setConnected(false, message);
  elements.treeEmpty.textContent = message;
}

async function connectToInspectedApplication() {
  if (!inspectedUrl || !inspectedTargetNonce) {
    state.error = 'The DevTools extension did not provide its inspected page identity.';
    setConnected(false, state.error);
    return;
  }

  try {
    const payload = await requestJson('/api/devtools/target', { inspectedUrl, targetNonce: inspectedTargetNonce }, {});
    const previousTargetKey = state.target
      ? `${state.target.id}:${state.target.sessionId}:${inspectedTargetNonce}`
      : null;
    const nextTargetKey = `${payload.target.id}:${payload.target.sessionId}:${inspectedTargetNonce}`;
    if (previousTargetKey !== null && previousTargetKey !== nextTargetKey) {
      stopConsoleStream();
      state.consoleEntries = [];
      state.consoleEntryKeys.clear();
      elements.consoleMessages.innerHTML = '';
      preparePerformanceForTargetChange();
    }
    state.target = payload.target;
    elements.targetName.textContent = state.target.name || 'Valdi application';
    elements.targetName.title = state.target.applicationUrl || inspectedUrl;
    elements.targetMetadata.textContent = `Chromium · :${state.target.debuggingPort}`;
    setConnected(true);
    if (previousTargetKey !== nextTargetKey) {
      addConsoleEntry('info', `Connected to ${state.target.applicationUrl}`);
    }
    startConsoleStream();
    await refreshSnapshot();
    startRefreshTimer();
  } catch (error) {
    reportError(error);
    window.setTimeout(connectToInspectedApplication, 1500);
  }
}

async function refreshSnapshot() {
  if (!state.target || state.refreshPending) return;
  state.refreshPending = true;
  const requestTarget = state.target;
  const requestGeneration = ++state.snapshotRequestGeneration;
  try {
    const snapshot = await requestJson(
      '/api/devtools/snapshot',
      { inspectedUrl, sessionId: requestTarget.sessionId, targetNonce: inspectedTargetNonce },
      {},
    );
    if (state.target !== requestTarget || state.snapshotRequestGeneration !== requestGeneration) return;
    snapshot.tree = valdiDebuggerTreeModel.restoreTree(snapshot.tree);
    const wasEmpty = !state.snapshot?.tree;
    const shouldClearHighlight = state.hoveredNodeId !== null || state.highlightMayBeActive;
    state.snapshot = snapshot;
    state.snapshotGeneration++;
    if (state.highlightTimer) window.clearTimeout(state.highlightTimer);
    state.highlightTimer = null;
    state.hoveredNodeId = null;
    state.hoveredSnapshotGeneration = state.snapshotGeneration - 1;
    if (shouldClearHighlight) queueHighlight(null);
    state.error = null;
    setConnected(true);

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
    render();
  } catch (error) {
    reportError(error);
  } finally {
    state.refreshPending = false;
  }
}

function startRefreshTimer() {
  if (state.refreshTimer) window.clearInterval(state.refreshTimer);
  state.refreshTimer = window.setInterval(() => {
    if (!state.autoRefresh || document.hidden) return;
    if (state.activeSection === 'elements') void refreshSnapshot();
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
  if (node.component && renderedNode === node) {
    elements.inspector.innerHTML = `<div class="rule-header">Valdi component <span class="rule-origin">${escapeHtml(node.tag)}</span></div>${propertyRows(node.component, { css: false })}<div class="empty-state">This component does not currently render a backing element.</div>`;
    return;
  }

  if (state.activeDetail === 'styles') {
    elements.inspector.innerHTML = renderStyles(renderedNode);
  } else if (state.activeDetail === 'computed') {
    elements.inspector.innerHTML = renderComputed(renderedNode);
  } else {
    const textContent = renderedNode.element?.dom?.textContent
      ? valdiDebuggerTreeModel.formatValue(renderedNode.element.dom.textContent, 0)
      : '';
    const componentDetails = node.component
      ? `<div class="rule-header">Valdi component <span class="rule-origin">${escapeHtml(node.tag)}</span></div>${propertyRows(node.component, { css: false })}`
      : '';
    elements.inspector.innerHTML = `${componentDetails}<div class="rule-header">Rendered &lt;${escapeHtml(valdiDebuggerTreeModel.formatValue(renderedNode.element?.dom?.tagName || 'div', 0))}&gt;</div>${propertyRows(renderedNode.element?.dom?.attributes, { css: false })}${textContent ? `<div class="rule-header">Text content</div><pre class="json-view">${escapeHtml(textContent)}</pre>` : ''}`;
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
}

function selectNode(id) {
  const node = findNode(id);
  if (!node) return;
  state.selectedNodeId = nodeId(node);
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

function enqueueHighlightRequest(intentGeneration, target, targetSessionId, snapshotGeneration, nodeIdValue) {
  const request = state.highlightRequestTail.then(async () => {
    if (
      intentGeneration !== state.highlightIntentGeneration ||
      state.target !== target ||
      target.sessionId !== targetSessionId ||
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
            inspectedUrl,
            sessionId: targetSessionId,
            targetNonce: inspectedTargetNonce,
            ...(nodeIdValue ? { nodeId: nodeIdValue } : {}),
          },
        },
      );
      if (nodeIdValue === null && intentGeneration === state.highlightIntentGeneration) {
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
    (state.hoveredNodeId === nodeIdValue &&
      state.hoveredSnapshotGeneration === state.snapshotGeneration &&
      !(nodeIdValue === null && state.highlightMayBeActive))
  )
    return;
  const target = state.target;
  const targetSessionId = target.sessionId;
  const snapshotGeneration = state.snapshotGeneration;
  const intentGeneration = ++state.highlightIntentGeneration;
  state.hoveredNodeId = nodeIdValue;
  state.hoveredSnapshotGeneration = snapshotGeneration;
  if (state.highlightTimer) window.clearTimeout(state.highlightTimer);
  state.highlightTimer = window.setTimeout(
    () => {
      state.highlightTimer = null;
      if (state.target !== target || state.snapshotGeneration !== snapshotGeneration) return;
      if (nodeIdValue !== null) state.highlightMayBeActive = true;
      enqueueHighlightRequest(intentGeneration, target, targetSessionId, snapshotGeneration, nodeIdValue);
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
  if (!state.target || state.performance.pending || state.performance.snapshotPending) return;
  const perf = state.performance;
  const identity = performanceIdentity();
  const requestGeneration = ++perf.requestGeneration;
  const requestIsCurrent = () => requestGeneration === perf.requestGeneration && performanceIdentityIsCurrent(identity);
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
  if (!state.target || perf.pending) return;
  if (['enable-tracing', 'trace-capture', 'trace-start'].includes(action) && (perf.traceActive || perf.ownerIdentity)) {
    return;
  }
  if (
    action === 'enable-tracing' &&
    !window.confirm('Enable Valdi renderer events? This reloads the inspected page and can reset page state.')
  ) {
    return;
  }

  const selectedIdentity = performanceIdentity();
  const identity = action === 'trace-stop' && perf.ownerIdentity ? perf.ownerIdentity : selectedIdentity;
  perf.requestGeneration++;
  perf.snapshotPending = false;
  const operationGeneration = ++perf.operationGeneration;
  const selectedTargetIsCurrent = () =>
    operationGeneration === perf.operationGeneration && performanceIdentityIsCurrent(selectedIdentity);
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
              if (!perf.ownerIdentity || samePerformanceIdentity(perf.ownerIdentity, identity)) {
                perf.traceActive = true;
                perf.ownerIdentity = identity;
                perf.error = `The previous web preview still owns a performance recording: ${cleanupError instanceof Error ? cleanupError.message : String(cleanupError)}`;
                if (state.activeSection === 'performance') renderPerformance(perf.data);
              }
            }
          }
          return;
        }
        perf.traceActive = Boolean(result.recording);
        perf.ownerIdentity = perf.traceActive ? identity : null;
        refreshSelectedTarget = true;
      } else {
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
    if (selectedTargetIsCurrent() || operationOwnsTrace()) {
      perf.error = message;
      if (state.activeSection === 'performance') renderPerformance(perf.data);
    } else {
      console.warn('Ignoring a stale web preview performance action error.', error);
    }
  } finally {
    if (selectedTargetIsCurrent()) {
      perf.pending = false;
      if (state.activeSection === 'performance') renderPerformance(perf.data);
    }
  }
  if (refreshSelectedTarget && selectedTargetIsCurrent() && state.target) {
    await refreshPerformance({ silent: true });
  }
}

function setActiveSection(section) {
  state.activeSection = section;
  for (const tab of elements.mainTabs) {
    const selected = tab.dataset.section === section;
    tab.classList.toggle('selected', selected);
    tab.setAttribute('aria-selected', String(selected));
  }
  for (const panel of elements.sections) {
    panel.classList.toggle('selected', panel.dataset.panel === section);
  }
  if (section === 'console') elements.consoleInput.focus();
  if (section === 'elements') void refreshSnapshot();
  if (section === 'performance') void refreshPerformance();
}

function setActiveDetail(detail) {
  state.activeDetail = detail;
  for (const tab of elements.detailTabs) {
    const selected = tab.dataset.detail === detail;
    tab.classList.toggle('selected', selected);
    tab.setAttribute('aria-selected', String(selected));
  }
  renderInspector();
}

function stopConsoleStream() {
  if (!state.consoleStream) return;
  state.consoleStream.close();
  state.consoleStream = null;
  state.consoleStreamTargetKey = null;
}

function startConsoleStream() {
  if (!state.target || !state.autoRefresh || !inspectedUrl || !inspectedTargetNonce) {
    stopConsoleStream();
    return;
  }
  const targetKey = `${state.target.id}:${state.target.sessionId}:${inspectedTargetNonce}`;
  if (state.consoleStream && state.consoleStreamTargetKey === targetKey) return;
  stopConsoleStream();

  const url = new URL('/api/devtools/console/stream', window.location.origin);
  url.searchParams.set('inspectedUrl', inspectedUrl);
  url.searchParams.set('sessionId', state.target.sessionId);
  url.searchParams.set('targetNonce', inspectedTargetNonce);
  const stream = new EventSource(url.toString());
  state.consoleStream = stream;
  state.consoleStreamTargetKey = targetKey;

  stream.addEventListener('console', event => {
    if (state.consoleStream !== stream || !state.target) return;
    let entry;
    try {
      entry = JSON.parse(event.data);
    } catch (error) {
      console.warn('[Valdi DevTools] Ignoring a malformed Chromium console event.', error);
      return;
    }
    if (typeof entry !== 'object' || entry === null || Array.isArray(entry)) return;
    if (
      entry.sessionId !== state.target.sessionId ||
      entry.targetId !== state.target.id ||
      typeof entry.message !== 'string'
    ) {
      return;
    }
    addConsoleEntry(entry.level, entry.message, entry.timestamp, entry.source);
  });

  stream.addEventListener('stream-error', event => {
    if (state.consoleStream !== stream || !state.target) return;
    try {
      const payload = JSON.parse(event.data);
      if (typeof payload !== 'object' || payload === null || Array.isArray(payload)) return;
      if (
        payload.sessionId === state.target.sessionId &&
        payload.targetId === state.target.id &&
        typeof payload.error === 'string'
      ) {
        addConsoleEntry('error', payload.error);
      }
    } catch (error) {
      console.warn('[Valdi DevTools] Ignoring a malformed Chromium console stream error.', error);
    }
  });

  stream.addEventListener('stream-warning', event => {
    if (state.consoleStream !== stream || !state.target) return;
    try {
      const payload = JSON.parse(event.data);
      if (typeof payload !== 'object' || payload === null || Array.isArray(payload)) return;
      if (
        payload.sessionId === state.target.sessionId &&
        payload.targetId === state.target.id &&
        typeof payload.message === 'string'
      ) {
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

async function evaluateConsoleExpression(expression) {
  addConsoleEntry('input', expression);
  try {
    const result = await requestJson(
      '/api/devtools/evaluate',
      {},
      {
        body: {
          expression,
          inspectedUrl,
          sessionId: state.target.sessionId,
          targetNonce: inspectedTargetNonce,
        },
      },
    );
    const serialized = result.type === 'undefined' ? undefined : JSON.stringify(result.value, null, 2);
    const value = result.type === 'undefined' ? 'undefined' : (serialized ?? String(result.value));
    addConsoleEntry('result', value);
  } catch (error) {
    addConsoleEntry('error', error.message);
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
  }
  for (const tab of elements.detailTabs) {
    tab.addEventListener('click', () => setActiveDetail(tab.dataset.detail));
  }
  elements.refreshButton.addEventListener('click', () => {
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
  elements.breadcrumbs.addEventListener('click', event => {
    const button = event.target.closest('[data-breadcrumb-id]');
    if (button) selectNode(button.dataset.breadcrumbId);
  });
  elements.copyNodeButton.addEventListener('click', async () => {
    const json = selectedNodeProjectionJson();
    if (json) await navigator.clipboard.writeText(json);
  });
  elements.splitHandle.addEventListener('pointerdown', startSplitResize);
  elements.consoleForm.addEventListener('submit', event => {
    event.preventDefault();
    const expression = elements.consoleInput.value.trim();
    if (!expression || !state.target) return;
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
    if (!document.hidden && state.activeSection === 'elements') void refreshSnapshot();
    if (!document.hidden && state.activeSection === 'performance') void refreshPerformance({ silent: true });
  });
}

applyTheme(query.get('theme'));
wireEvents();
void connectToInspectedApplication();
