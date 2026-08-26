const query = new URLSearchParams(window.location.search);
const inspectedUrl = query.get('inspectedUrl');
const inspectedTargetNonce = query.get('targetNonce');
const MAX_CONSOLE_ENTRIES = 500;
const MAX_CONSOLE_ENTRY_CHARACTERS = 50_000;
const MAX_CONSOLE_HISTORY_ENTRIES = 100;

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
  hoveredNodeId: null,
  highlightTimer: null,
  consoleEntries: [],
  consoleHistory: [],
  consoleHistoryIndex: 0,
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

function formatNumber(value) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric)) return '—';
  return Number.isInteger(numeric)
    ? numeric.toLocaleString()
    : numeric.toLocaleString(undefined, { maximumFractionDigits: 1 });
}

function nodeAttributes(node) {
  return valdiDebuggerTreeModel.attributes(node);
}

function nodeId(node) {
  return valdiDebuggerTreeModel.id(node);
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
  valdiDebuggerTreeModel.walkVisible(
    node,
    callback,
    current => state.expandedNodeIds.has(nodeId(current)),
    [],
    0,
  );
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
    const payload = await requestJson(
      '/api/devtools/target',
      { inspectedUrl, targetNonce: inspectedTargetNonce },
      {},
    );
    state.target = payload.target;
    elements.targetName.textContent = state.target.name || 'Valdi application';
    elements.targetName.title = state.target.applicationUrl || inspectedUrl;
    elements.targetMetadata.textContent = `Chromium · :${state.target.debuggingPort}`;
    setConnected(true);
    addConsoleEntry('info', `Connected to ${state.target.applicationUrl}`);
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
  try {
    const snapshot = await requestJson(
      '/api/devtools/snapshot',
      { inspectedUrl, sessionId: state.target.sessionId, targetNonce: inspectedTargetNonce },
      {},
    );
    snapshot.tree = valdiDebuggerTreeModel.restoreTree(snapshot.tree);
    const wasEmpty = !state.snapshot?.tree;
    state.snapshot = snapshot;
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
    if (!state.autoRefresh || document.hidden || state.activeSection !== 'elements') return;
    void refreshSnapshot();
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
      <div id="${escapeHtml(treeRowId(id))}" class="tree-row${selected ? ' selected' : ''}" data-node-id="${escapeHtml(id)}" role="treeitem" aria-level="${depth + 1}" aria-selected="${selected}"${hasChildren ? ` aria-expanded="${expanded}"` : ''} style="padding-left:${depth * 13 + 2}px">
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

  if (state.activeDetail === 'styles') {
    elements.inspector.innerHTML = renderStyles(node);
  } else if (state.activeDetail === 'computed') {
    elements.inspector.innerHTML = renderComputed(node);
  } else {
    const textContent = node.element?.dom?.textContent
      ? valdiDebuggerTreeModel.formatValue(node.element.dom.textContent, 0)
      : '';
    elements.inspector.innerHTML = `<div class="rule-header">Rendered &lt;${escapeHtml(valdiDebuggerTreeModel.formatValue(node.element?.dom?.tagName || 'div', 0))}&gt;</div>${propertyRows(node.element?.dom?.attributes, { css: false })}${textContent ? `<div class="rule-header">Text content</div><pre class="json-view">${escapeHtml(textContent)}</pre>` : ''}`;
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

function queueHighlight(nodeIdValue) {
  if (!state.target || state.hoveredNodeId === nodeIdValue) return;
  state.hoveredNodeId = nodeIdValue;
  if (state.highlightTimer) window.clearTimeout(state.highlightTimer);
  state.highlightTimer = window.setTimeout(
    () => {
      void requestJson(
        '/api/devtools/highlight',
        {},
        {
          body: {
            inspectedUrl,
            sessionId: state.target.sessionId,
            targetNonce: inspectedTargetNonce,
            ...(nodeIdValue ? { nodeId: nodeIdValue } : {}),
          },
        },
      ).catch(error => console.warn('Unable to update the inspected Valdi highlight.', error));
    },
    nodeIdValue ? 80 : 20,
  );
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

function addConsoleEntry(kind, value) {
  const text = String(value);
  const boundedValue =
    text.length > MAX_CONSOLE_ENTRY_CHARACTERS
      ? `${text.slice(0, MAX_CONSOLE_ENTRY_CHARACTERS - 1)}…`
      : text;
  state.consoleEntries.push({ kind, value: boundedValue });
  if (state.consoleEntries.length > MAX_CONSOLE_ENTRIES) {
    state.consoleEntries.splice(0, state.consoleEntries.length - MAX_CONSOLE_ENTRIES);
  }
  elements.consoleMessages.innerHTML = state.consoleEntries
    .map(
      entry =>
        `<div class="console-entry ${escapeHtml(entry.kind)}"><span class="console-chevron">${entry.kind === 'input' ? '›' : entry.kind === 'error' ? '×' : '‹'}</span><pre>${escapeHtml(entry.value)}</pre></div>`,
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
  elements.refreshButton.addEventListener('click', () => void refreshSnapshot());
  elements.autoRefreshToggle.addEventListener('change', () => {
    state.autoRefresh = elements.autoRefreshToggle.checked;
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
    if (row) queueHighlight(row.dataset.nodeId);
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
  document.addEventListener('visibilitychange', () => {
    if (!document.hidden && state.activeSection === 'elements') void refreshSnapshot();
  });
}

applyTheme(query.get('theme'));
wireEvents();
void connectToInspectedApplication();
