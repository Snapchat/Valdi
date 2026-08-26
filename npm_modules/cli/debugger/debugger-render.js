// Main UI rendering for preview, hierarchy, overlay, targets, and inspector.
function render() {
  renderHeader();
  renderTargets();
  renderDaemonStatus();
  renderTree();
  renderPreview();
  renderOverlay();
  renderInspector();
  renderLogs();
  renderPerformance();
}

function renderPreview() {
  elements.appPreview.querySelectorAll('.preview-warning').forEach(node => node.remove());
  const htmlPreviewRendered = renderHtmlPreview();
  if (state.rootSnapshotImage && !htmlPreviewRendered) {
    elements.rootSnapshotImage.src = state.rootSnapshotImage;
    elements.rootSnapshotImage.hidden = false;
  } else {
    elements.rootSnapshotImage.removeAttribute('src');
    elements.rootSnapshotImage.hidden = true;
  }
  if (!hasSnapshotTree()) {
    const empty = document.createElement('div');
    empty.className = 'preview-warning';
    empty.textContent = 'No Valdi snapshot loaded. Attach to a live target.';
    elements.appPreview.appendChild(empty);
  } else if (state.source !== 'daemon' && state.snapshot.target?.clientId && state.snapshot.target?.contextId) {
    const warning = document.createElement('div');
    warning.className = 'preview-warning';
    warning.textContent =
      'Live Valdi target found, but this preview is not attached. Press Attach or Refresh to fetch the current tree.';
    elements.appPreview.appendChild(warning);
  }
  elements.copyPreviewButton.disabled = !state.rootSnapshotImage && !hasSnapshotTree();
}

function renderPreviewTargetSelect() {
  const options = [
    {
      value: String(STANDALONE_DAEMON_PORT),
      label: `SnapDrawing / standalone :${STANDALONE_DAEMON_PORT}`,
    },
    {
      value: String(MOBILE_DAEMON_PORT),
      label: `mobile :${MOBILE_DAEMON_PORT}`,
    },
  ];

  const selectedPort = String(selectedDaemonPort());
  const selectedValue = options.some(option => option.value === selectedPort)
    ? selectedPort
    : String(STANDALONE_DAEMON_PORT);
  const markup = options
    .map(option => `<option value="${escapeHtml(option.value)}">${escapeHtml(option.label)}</option>`)
    .join('');
  if (elements.portSelect.innerHTML !== markup) {
    elements.portSelect.innerHTML = markup;
  }
  elements.portSelect.value = selectedValue;
}

function renderHeader() {
  renderPreviewTargetSelect();
  const target = state.snapshot.target || emptyTarget;
  const attached = isDebuggerAttached();
  elements.sessionSubtitle.textContent =
    state.source === 'empty'
      ? 'No session selected'
      : `${target.name} - ${displayPlatform(target)} - ${target.transport || 'daemon'}`;
  elements.reloaderDot.className = state.source === 'daemon' ? 'dot good' : 'dot warn';
  elements.reloaderState.textContent = state.source === 'daemon' ? 'Live snapshot' : 'No live snapshot';
  const proxyOnline = state.lastStatus?.hotReloadProxy?.connected;
  elements.proxyDot.className = proxyOnline || attached ? 'dot good' : 'dot warn';
  elements.proxyState.textContent = proxyOnline
    ? `Proxy ${state.lastStatus.hotReloadProxy.port} online`
    : attached
      ? `Target ${target.proxyPort || target.port || elements.portSelect.value} attached`
      : hasSelectedLiveTarget()
        ? `Target ${target.proxyPort || target.port || elements.portSelect.value} available`
        : `Port ${elements.portSelect.value} idle`;
  elements.attachButton.textContent = attached ? 'Detach' : hasSelectedLiveTarget() ? 'Resume' : 'Attach';
  elements.autoRefreshToggle.checked = state.autoRefresh;
  elements.hierarchySource.textContent = hierarchySourceLabel();
  elements.hierarchySource.className = `source-pill ${state.source === 'daemon' ? 'live' : ''}`;
}

function renderTargets() {
  const targets = debuggerTargets();
  if (!targets.length) {
    elements.targetList.innerHTML = `<div class="empty">No Valdi targets found. Start a playground app and hot reloader, then refresh.</div>`;
    return;
  }

  elements.targetList.innerHTML = targets
    .map(target => {
      const active = target.id === state.snapshot.target?.id ? ' active' : '';
      const dot = target.state === 'attached' ? 'good' : target.state === 'paused' ? 'warn' : '';
      return `
              <div class="target${active}" data-target-id="${escapeHtml(target.id)}" role="button" tabindex="0">
                <div class="target-top">
                  <span class="target-name">${escapeHtml(target.name)}</span>
                  <span class="status-pill"><span class="dot ${dot}"></span>${escapeHtml(target.state)}</span>
                </div>
                <div class="meta-grid">
                  <div class="metric"><div class="metric-label">Platform</div><div class="metric-value">${escapeHtml(displayPlatform(target))}</div></div>
                  <div class="metric"><div class="metric-label">Transport</div><div class="metric-value">${escapeHtml(target.transport || state.snapshot.target.transport || 'n/a')}</div></div>
                  ${target.applicationId ? `<div class="metric"><div class="metric-label">App</div><div class="metric-value">${escapeHtml(target.applicationId)}</div></div>` : ''}
                </div>
              </div>
            `;
    })
    .join('');
}

function renderDaemonStatus() {
  if (!state.lastStatus) {
    elements.daemonStatus.innerHTML = `<div class="empty">Status has not been checked yet.</div>`;
    return;
  }

  const rows = [];
  for (const portStatus of state.lastStatus.ports || []) {
    const contextCount = (portStatus.clients || []).reduce((sum, client) => sum + (client.contexts || []).length, 0);
    const dot = portStatus.connected && contextCount ? 'good' : portStatus.connected ? 'warn' : 'bad';
    const detail = portStatus.connected
      ? `${(portStatus.clients || []).length} client(s), ${contextCount} context(s)`
      : portStatus.error || 'not reachable';
    rows.push(`
            <div class="daemon-row">
              <div class="daemon-row-top">
                <strong>${escapeHtml(portStatus.portName)} :${escapeHtml(portStatus.port)}</strong>
                <span class="status-pill"><span class="dot ${dot}"></span>${portStatus.connected ? 'reachable' : 'offline'}</span>
              </div>
              <div class="daemon-detail">${escapeHtml(detail)}</div>
            </div>
          `);
  }

  const proxy = state.lastStatus.hotReloadProxy;
  if (proxy) {
    rows.push(`
            <div class="daemon-row">
              <div class="daemon-row-top">
                <strong>hotreload proxy :${escapeHtml(proxy.port)}</strong>
                <span class="status-pill"><span class="dot ${proxy.connected ? 'good' : 'warn'}"></span>${proxy.connected ? 'online' : 'idle'}</span>
              </div>
              <div class="daemon-detail">hot reload connection proxy</div>
            </div>
          `);
  }

  elements.daemonStatus.innerHTML = rows.join('');
}

function nodeMatchesSearch(node, search) {
  if (!search) return true;
  const id = getNodeId(node);
  const attributes = valdiDebuggerTreeModel.stringifyValue(getNodeAttributes(node), 0).toLowerCase();
  const viewModel = valdiDebuggerTreeModel.stringifyValue(node.viewModel || {}, 0).toLowerCase();
  const component = valdiDebuggerTreeModel.stringifyValue(node.component || {}, 0).toLowerCase();
  const componentState = valdiDebuggerTreeModel.stringifyValue(node.state || {}, 0).toLowerCase();
  const haystack = `${node.tag} ${id} #${id} ${attributes} ${viewModel} ${component} ${componentState}`.toLowerCase();
  return haystack.includes(search);
}

function collectSearchVisibleNodeIds(root, search) {
  const visibleNodeIds = new Set();
  const matchedLineage = new Set();
  const records = [];
  valdiDebuggerTreeModel.walk(
    root,
    (node, ancestors) => {
      const parent = ancestors.at(-1);
      const ancestorMatched = parent ? matchedLineage.has(parent) : false;
      const selfMatched = nodeMatchesSearch(node, search);
      records.push({ node, parent, selfMatched });
      if (ancestorMatched || selfMatched || getNodeId(node) === state.selectedNodeId) {
        visibleNodeIds.add(getNodeId(node));
      }
      if (ancestorMatched || selfMatched) matchedLineage.add(node);
    },
    [],
    0,
  );
  const matchedSubtrees = new Set();
  for (let index = records.length - 1; index >= 0; index -= 1) {
    const record = records[index];
    if (!record.selfMatched && !matchedSubtrees.has(record.node)) continue;
    visibleNodeIds.add(getNodeId(record.node));
    if (record.parent) matchedSubtrees.add(record.parent);
  }
  return visibleNodeIds;
}

function parseExactNodeIdSearch(search) {
  const match = search.match(/^#?(\d+)$/);
  return match ? match[1] : null;
}

function renderTree() {
  if (!hasSnapshotTree()) {
    elements.tree.innerHTML = `<div class="empty">No hierarchy loaded. Attach to a target.</div>`;
    return;
  }
  const search = elements.treeSearch.value.trim().toLowerCase();
  const exactSearchNodeId = search ? parseExactNodeIdSearch(search) : null;
  const exactSearchRoot = exactSearchNodeId ? findNodeInTree(state.snapshot.tree, exactSearchNodeId) : null;
  const searchVisibleNodeIds =
    search && !exactSearchRoot ? collectSearchVisibleNodeIds(state.snapshot.tree, search) : null;
  const rows = [];
  const traversal = exactSearchRoot ? walk : search ? walk : walkVisible;
  const traversalRoot = exactSearchRoot || state.snapshot.tree;
  traversal(traversalRoot, (node, parent, depth) => {
    const id = getNodeId(node);
    const isSelected = id === state.selectedNodeId;
    const selected = isSelected ? ' selected' : '';
    const hidden = searchVisibleNodeIds && !searchVisibleNodeIds.has(id) ? ' filtered-out' : '';
    const kind = getNodeKind(node);
    const hasChildren = valdiDebuggerTreeModel.hasChildren(node);
    const expanded = search || exactSearchRoot ? hasChildren : state.expandedNodeIds.has(id);
    const toggleClass = hasChildren ? '' : ' empty';
    const toggleLabel = hasChildren ? (expanded ? '-' : '+') : '';
    const ariaExpanded = hasChildren ? `aria-expanded="${expanded}"` : '';
    rows.push(`
            <div class="tree-node${selected}${hidden}" data-node-id="${escapeHtml(id)}" role="button" tabindex="0" style="padding-left: ${7 + depth * 14}px">
              <button class="tree-toggle${toggleClass}" data-toggle-node-id="${escapeHtml(id)}" ${ariaExpanded} tabindex="-1" title="${hasChildren ? (expanded ? 'Collapse' : 'Expand') : ''}">${toggleLabel}</button>
              <span class="node-kind ${kind}"></span>
              <span class="node-label">${escapeHtml(node.tag)}</span>
              <span class="node-id">#${escapeHtml(id)}</span>
            </div>
          `);
  });
  elements.tree.innerHTML = rows.join('');
}

function clipBoundsToViewport(bounds, viewport) {
  const left = Math.max(bounds.x, viewport.x);
  const top = Math.max(bounds.y, viewport.y);
  const right = Math.min(bounds.x + bounds.width, viewport.x + viewport.width);
  const bottom = Math.min(bounds.y + bounds.height, viewport.y + viewport.height);
  if (right <= left || bottom <= top) return null;
  return {
    x: left,
    y: top,
    width: right - left,
    height: bottom - top,
  };
}

function shouldRenderOverlayNode(node) {
  const id = getNodeId(node);
  if (state.overlayMode === 'issues') {
    return state.snapshot.issues.some(issue => String(issue.nodeId) === id);
  }
  return false;
}

function collectOverlayEntries(viewport) {
  const entries = [];
  if (!hasSnapshotTree()) return entries;
  walk(state.snapshot.tree, (node, _parent, depth) => {
    if (!shouldRenderOverlayNode(node)) return;
    const geometry = getNodeGeometry(node);
    if (!geometry) return;
    const bounds = clipBoundsToViewport(geometry.absolute, viewport);
    if (!bounds) return;
    entries.push({ node, depth, bounds });
  });

  return entries;
}

function renderOverlay() {
  elements.screen.querySelectorAll('.overlay-node').forEach(node => node.remove());
  const viewport = getViewportBounds();
  elements.device.style.setProperty('--device-w', viewport.width);
  elements.device.style.setProperty('--device-h', viewport.height);
  updateHtmlPreviewScale();
  markHtmlPreviewSelection();
  const screenBounds = elements.screen.getBoundingClientRect();
  const scaleX = screenBounds.width / viewport.width;
  const scaleY = screenBounds.height / viewport.height;
  for (const entry of collectOverlayEntries(viewport)) {
    const node = entry.node;
    const id = getNodeId(node);
    const bounds = entry.bounds;
    const left = (bounds.x - viewport.x) * scaleX;
    const top = (bounds.y - viewport.y) * scaleY;
    const overlay = document.createElement('button');
    overlay.className = `overlay-node ${getNodeKind(node)}${id === state.selectedNodeId ? ' selected' : ''}`;
    overlay.dataset.nodeId = id;
    overlay.style.left = `${left}px`;
    overlay.style.top = `${top}px`;
    overlay.style.width = `${Math.max(8, bounds.width * scaleX)}px`;
    overlay.style.height = `${Math.max(8, bounds.height * scaleY)}px`;
    const label = overlayNodeLabel(node);
    const detail = overlayNodeDetail(node);
    overlay.title = describeOverlayNode(node);
    overlay.innerHTML = detail
      ? `<span class="label"><span class="label-line">${escapeHtml(label)}</span><span class="label-line label-detail">${escapeHtml(detail)}</span></span>`
      : `<span class="label"><span class="label-line">${escapeHtml(label)}</span></span>`;
    elements.screen.appendChild(overlay);
  }
}

function selectedNode() {
  if (!hasSnapshotTree()) return null;
  return findNode(state.selectedNodeId) || state.snapshot.tree;
}

function getComponentDebugData(node) {
  return node.component && typeof node.component === 'object' ? node.component : {};
}

function isComponentDebugDataOmitted(node) {
  return getComponentDebugData(node).debugDataOmitted === true;
}

function hasDebugPayload(value) {
  return value !== undefined;
}

function getNodeViewModelPayload(node) {
  const componentData = getComponentDebugData(node);
  if (componentData.viewModel !== undefined) return componentData.viewModel;
  if (node.viewModel !== undefined) return node.viewModel;
  return undefined;
}

function getNodeStatePayload(node) {
  const componentData = getComponentDebugData(node);
  if (componentData.state !== undefined) return componentData.state;
  if (node.state !== undefined) return node.state;
  return undefined;
}

function payloadToDisplayString(payload) {
  return valdiDebuggerTreeModel.formatValue(payload, 2);
}

function renderPayload(payload) {
  return `<pre class="codebox">${escapeHtml(payloadToDisplayString(payload))}</pre>`;
}

function renderDataSection(title, body) {
  return `
          <section class="data-section">
            <h3 class="data-heading">${escapeHtml(title)}</h3>
            ${body}
          </section>
        `;
}

function renderAttributesTable(attributes) {
  const rows = Object.entries(attributes)
    .map(
      ([key, value]) =>
        `<div>${escapeHtml(key)}</div><div>${escapeHtml(valdiDebuggerTreeModel.formatValue(value, 0))}</div>`,
    )
    .join('');
  return rows ? `<div class="kv">${rows}</div>` : '';
}

function serializeRawInspectorNode(node, geometry, target) {
  const projectedGeometry = valdiDebuggerTreeModel.projectValue(geometry);
  const projectedNode = valdiDebuggerTreeModel.projectTree(node);
  const projectedTarget = valdiDebuggerTreeModel.projectValue(target);
  return JSON.stringify(
    {
      geometry: projectedGeometry.value,
      node: projectedNode,
      projectionComplete: projectedGeometry.complete && projectedNode.complete && projectedTarget.complete,
      target: projectedTarget.value,
    },
    null,
    2,
  );
}

async function captureRootSnapshot(params, options = {}) {
  if (!hasSnapshotTree()) {
    state.rootSnapshotImage = null;
    renderPreview();
    return;
  }
  const rootElement = firstElementWithFrame(state.snapshot.tree);
  if (state.source !== 'daemon' || !rootElement?.element) {
    state.rootSnapshotImage = null;
    renderPreview();
    return;
  }

  const requestId = ++state.rootSnapshotRequestId;
  try {
    const result = await apiGet('/api/element-snapshot', {
      ...params,
      elementId: rootElement.element.id,
    });
    if (requestId !== state.rootSnapshotRequestId || state.source !== 'daemon') return;
    state.rootSnapshotImage = result.image || null;
    renderPreview();
  } catch (error) {
    if (requestId !== state.rootSnapshotRequestId) return;
    state.rootSnapshotImage = null;
    renderPreview();
    if (!options.silent) addLog('warn', 'inspector', `Could not capture live root snapshot: ${error.message}`);
  }
}

function renderInspector() {
  const node = selectedNode();
  if (!node) {
    elements.selectedSummary.textContent = 'No node selected';
    elements.inspector.innerHTML = `<div class="empty">No node is selected because no snapshot is loaded.</div>`;
    return;
  }
  const id = getNodeId(node);
  const kind = getNodeKind(node);
  const path = getPathToNode(id);
  const geometry = getNodeGeometry(node);
  const scrollOffset = getScrollOffset(node);
  const viewModelPayload = getNodeViewModelPayload(node);
  const statePayload = getNodeStatePayload(node);
  elements.selectedSummary.textContent = `${node.tag} #${id} - ${kind}`;

  const tab = state.activeTab;
  if (tab === 'overview') {
    elements.inspector.innerHTML = `
            ${renderBreadcrumb(path)}
            <div class="kv">
              <div>Tag</div><div>${escapeHtml(node.tag)}</div>
              <div>Kind</div><div>${escapeHtml(kind)}</div>
              <div>Node id</div><div>${escapeHtml(id)}</div>
              <div>Key</div><div>${escapeHtml(node.key || 'n/a')}</div>
              <div>Children</div><div>${valdiDebuggerTreeModel.children(node).length}</div>
              <div>Local bounds</div><div>${renderBounds(geometry?.local || node.bounds)}</div>
              <div>Absolute bounds</div><div>${renderBounds(geometry?.absolute)}</div>
              ${hasScrollState(node) ? `<div>Host scroll offset</div><div>x ${scrollOffset.x}, y ${scrollOffset.y}</div>` : ''}
              <div>Platform class</div><div>${escapeHtml(resolvePlatformClass(node))}</div>
              <div>Props</div><div>${hasDebugPayload(viewModelPayload) || Object.keys(getNodeAttributes(node)).length ? 'available' : isComponentDebugDataOmitted(node) ? 'omitted by snapshot budget' : 'not exported'}</div>
              <div>State</div><div>${hasDebugPayload(statePayload) ? 'available' : isComponentDebugDataOmitted(node) ? 'omitted by snapshot budget' : 'not present'}</div>
              <div>Last updated</div><div>${escapeHtml(state.lastUpdated || 'n/a')}</div>
            </div>
            ${state.selectedSnapshotImage ? `<div class="snapshot-preview" data-selected-snapshot></div>` : ''}
          `;
    if (state.selectedSnapshotImage) {
      const snapshotContainer = elements.inspector.querySelector('[data-selected-snapshot]');
      if (snapshotContainer) {
        const snapshotImage = document.createElement('img');
        snapshotImage.alt = 'Selected element snapshot';
        snapshotImage.src = state.selectedSnapshotImage;
        snapshotContainer.appendChild(snapshotImage);
      }
    }
    return;
  }

  if (tab === 'props') {
    const attributes = getNodeAttributes(node);
    const sections = [];
    if (hasDebugPayload(viewModelPayload)) {
      sections.push(renderDataSection('ViewModel', renderPayload(viewModelPayload)));
    }
    const attributesTable = renderAttributesTable(attributes);
    if (attributesTable) {
      sections.push(renderDataSection('Element Attributes', attributesTable));
    }
    elements.inspector.innerHTML = sections.length
      ? `${sections.join('')}${isComponentDebugDataOmitted(node) ? '<div class="empty">Some component debug data was omitted by the snapshot budget.</div>' : ''}`
      : `<div class="empty">This node has no exported ViewModel or element attributes.</div>`;
    return;
  }

  if (tab === 'state') {
    elements.inspector.innerHTML = hasDebugPayload(statePayload)
      ? renderDataSection('Component State', renderPayload(statePayload))
      : isComponentDebugDataOmitted(node)
        ? `<div class="empty">Component state was omitted by the snapshot budget.</div>`
        : `<div class="empty">No component state is exported for this node. State appears for StatefulComponent instances after the runtime exports a non-undefined state value.</div>`;
    return;
  }

  if (tab === 'raw') {
    const payload = serializeRawInspectorNode(node, geometry, state.snapshot.target);
    elements.inspector.innerHTML = `<pre class="codebox">${escapeHtml(payload)}</pre>`;
    return;
  }

  const related = state.snapshot.issues.filter(issue => String(issue.nodeId) === id || issue.nodeId === undefined);
  elements.inspector.innerHTML = related.length
    ? `<div class="issue-list">${related.map(renderIssue).join('')}</div>`
    : `<div class="empty">No runtime issues are attached to this node.</div>`;
}

function renderBreadcrumb(path) {
  return `<div class="breadcrumb">${path
    .map(node => `<span class="crumb">${escapeHtml(node.tag)} #${escapeHtml(getNodeId(node))}</span>`)
    .join('')}</div>`;
}

function renderBounds(bounds) {
  if (!bounds) return 'not reported';
  return `x ${bounds.x}, y ${bounds.y}, ${bounds.width} x ${bounds.height}`;
}

function resolvePlatformClass(node) {
  const attrs = getNodeAttributes(node);
  if ((state.snapshot.target?.platform === 'macOS' || isMacDesktopTarget()) && !attrs.macosClass && attrs.iosClass) {
    return `${attrs.iosClass} (macOS fallback from iosClass)`;
  }
  return attrs.macosClass || attrs.iosClass || attrs.androidClass || attrs.webClass || 'n/a';
}

function renderIssue(issue) {
  return `
          <div class="issue ${issue.severity === 'error' ? 'error' : 'warn'}" data-issue-node="${escapeHtml(issue.nodeId || '')}" role="button" tabindex="0">
            <div class="issue-title">${escapeHtml(issue.title)} <span class="node-id">${escapeHtml(issue.time || '')}</span></div>
            <div class="issue-message">${escapeHtml(issue.message)}</div>
          </div>
        `;
}
