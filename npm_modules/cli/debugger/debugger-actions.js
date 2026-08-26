// Section switching, auto-refresh, action dispatch, and console commands.
function normalizeSection(section) {
  const normalized = String(section || '')
    .trim()
    .toLowerCase();
  if (normalized === 'logger' || normalized === 'loger') return 'logs';
  return normalized;
}

function setActiveSection(section) {
  const normalized = normalizeSection(section);
  if (!elements.sectionPanels.some(panel => panel.dataset.sectionPanel === normalized)) {
    addLog('warn', 'debugger', `Unknown debugger section: ${section}.`);
    return;
  }

  state.activeSection = normalized;
  elements.sectionButtons.forEach(button => {
    button.classList.toggle('active', button.dataset.sectionButton === normalized);
  });
  elements.sectionPanels.forEach(panel => {
    panel.classList.toggle('active', panel.dataset.sectionPanel === normalized);
  });
  if (normalized === 'logs') {
    renderLogs();
    startRuntimeLogStream({ silent: true });
  } else {
    stopRuntimeLogStream();
  }
  if (normalized === UI_SECTION && state.autoRefresh && state.source === 'daemon' && isDebuggerAttached()) {
    window.setTimeout(() => refreshLiveSnapshot(), 0);
  }
}

function shouldAutoRefresh() {
  return (
    !document.hidden &&
    !state.performance.traceActive &&
    !state.performance.traceCapturePending &&
    !state.performance.traceResultPending &&
    !state.performance.traceStateUnknown &&
    !state.performance.profileActive
  );
}

function shouldAutoRefreshLiveSnapshot() {
  return (
    shouldAutoRefresh() && state.activeSection === UI_SECTION && !document.querySelector('.performance-panel:hover')
  );
}

function autoRefreshLiveDebugger() {
  if (!shouldAutoRefresh()) {
    return;
  }

  if (state.followLatestTarget && !state.manualDetach) {
    void refreshTargets({
      silent: true,
      autoAttach: state.activeSection === UI_SECTION,
      skipSnapshotIfSame: state.activeSection !== UI_SECTION,
      queueIfBusy: true,
    });
    return;
  }

  if (state.source !== 'daemon' || !isDebuggerAttached()) {
    return;
  }

  if (shouldAutoRefreshLiveSnapshot()) {
    void loadRealSnapshot(null, { silent: true, preserveSelection: true, queueIfBusy: true });
  }
}

function setAutoRefresh(enabled, options = {}) {
  state.autoRefresh = enabled;
  if (state.autoRefreshTimer) {
    clearInterval(state.autoRefreshTimer);
    state.autoRefreshTimer = null;
  }

  if (enabled) {
    state.autoRefreshTimer = window.setInterval(() => {
      autoRefreshLiveDebugger();
    }, AUTO_REFRESH_INTERVAL_MS);
    if (!options.silent) addLog('info', 'debugger', 'Auto-refresh enabled.');
  } else {
    if (!options.silent) addLog('info', 'debugger', 'Auto-refresh disabled.');
  }
  renderHeader();
}

function refreshLiveSnapshot() {
  const target = state.snapshot.target || {};
  if (state.followLatestTarget) {
    state.manualDetach = false;
    refreshTargets();
    return;
  }
  if (state.source === 'daemon' || (target.clientId && target.contextId)) {
    state.manualDetach = false;
    loadRealSnapshot(null, { preserveSelection: true, queueIfBusy: true });
  } else {
    addLog('info', 'daemon', 'No live Valdi daemon target is selected yet. Refreshing targets first.');
    refreshTargets();
  }
}

function scrollSelectedTreeNodeIntoView() {
  const selectedRow = Array.from(elements.tree.querySelectorAll('.tree-node')).find(
    row => row.dataset.nodeId === state.selectedNodeId,
  );
  selectedRow?.scrollIntoView({ block: 'nearest', inline: 'nearest' });
}

function selectNode(id, options = {}) {
  const node = findNode(id);
  if (!node) {
    addLog('warn', 'console', `No node found for id ${id}.`);
    return;
  }
  state.selectedNodeId = getNodeId(node);
  state.selectedSnapshotImage = null;
  expandPathToNode(state.selectedNodeId);
  renderTree();
  if (options.scrollTree !== false) {
    window.requestAnimationFrame(scrollSelectedTreeNodeIntoView);
  }
  renderOverlay();
  renderInspector();
}

function setActiveTab(tab) {
  if (!['overview', 'props', 'state', 'issues', 'raw'].includes(tab)) {
    addLog('warn', 'debugger', `Unknown inspector tab: ${tab}.`);
    return;
  }
  state.activeTab = tab;
  document
    .querySelectorAll('.tabs button')
    .forEach(button => button.classList.toggle('active', button.dataset.tab === tab));
  renderInspector();
}

function attachDebuggerView() {
  state.manualDetach = false;
  state.followLatestTarget = true;
  refreshTargets();
}

function detachDebuggerView() {
  state.attached = false;
  state.manualDetach = true;
  state.rootSnapshotImage = null;
  state.rootSnapshotRequestId++;
  stopRuntimeLogStream();
  addLog('warn', 'proxy', 'Detached debugger view from live daemon data.');
  render();
}

async function setTargetPort(port, nextTarget) {
  if (!(await preparePerformanceTraceTargetSwitch(nextTarget))) return;
  elements.portSelect.value = String(port);
  state.snapshot.target = {
    ...state.snapshot.target,
    proxyPort: port,
    port,
  };
  state.attached = false;
  state.performance.traceSupported = true;
  state.manualDetach = false;
  state.followLatestTarget = true;
  state.rootSnapshotImage = null;
  state.rootSnapshotRequestId++;
  stopRuntimeLogStream();
  render();
}

function clearDebuggerLogs() {
  state.snapshot.logs = [];
  renderLogs();
}

function applyDebuggerAction(action, params = {}) {
  if (action === 'selectNode') {
    const nodeId = actionString(params, 'id', 'nodeId');
    if (nodeId) selectNode(nodeId);
    return;
  }

  if (action === 'selectTarget') {
    const targetId = actionString(params, 'id', 'targetId');
    if (targetId) selectTarget(targetId);
    return;
  }

  if (action === 'setActiveTab') {
    setActiveTab(actionString(params, 'tab'));
    return;
  }

  if (action === 'setActiveSection') {
    setActiveSection(actionString(params, 'section'));
    return;
  }

  if (action === 'setOverlayMode') {
    setOverlayMode(actionString(params, 'mode'));
    return;
  }

  if (action === 'setAutoRefresh') {
    setAutoRefresh(actionBoolean(params, 'enabled', state.autoRefresh));
    return;
  }

  if (action === 'setPort') {
    const port = actionNumber(params, 'port');
    if (port !== null) void setTargetPort(port, { port });
    return;
  }

  if (action === 'attach') {
    attachDebuggerView();
    return;
  }

  if (action === 'detach') {
    detachDebuggerView();
    return;
  }

  if (action === 'refreshTargets') {
    refreshTargets();
    return;
  }

  if (action === 'refreshSnapshot') {
    refreshLiveSnapshot();
    return;
  }

  if (action === 'clearLogs') {
    clearDebuggerLogs();
    return;
  }

  if (action === 'captureElementSnapshot') {
    void captureSelectedElementSnapshot();
    return;
  }

  if (action === 'dumpHeap') {
    void dumpHeap();
    return;
  }

  if (action === 'startRendererTrace') {
    void startPerformanceTrace();
    return;
  }

  if (action === 'stopRendererTrace') {
    void stopPerformanceTrace({});
    return;
  }

  if (action === 'captureRendererTrace') {
    void capturePerformanceTrace();
    return;
  }

  if (action === 'refreshHermesContexts') {
    void refreshProfileContexts({ silent: false });
    return;
  }

  if (action === 'startCpuProfile') {
    void startCpuProfile();
    return;
  }

  if (action === 'stopCpuProfile') {
    void stopCpuProfile();
    return;
  }

  if (action === 'captureCpuProfile') {
    void captureCpuProfile();
    return;
  }

  addLog('warn', 'debugger', `Unknown debugger action: ${action}.`);
}

function runCommand(rawCommand) {
  const command = rawCommand.trim();
  if (!command) return;
  addLog('info', 'console', `> ${command}`);

  if (command === 'help') {
    addLog(
      'info',
      'console',
      'Commands: help, section <ui|performance|logs>, path, issues, select <id>, connect, refresh, reload, status, snapshot, heap, trace, profile, auto on, auto off, clear.',
    );
  } else if (command.startsWith('section ')) {
    void requestDebuggerAction('setActiveSection', { section: command.slice('section '.length).trim() });
  } else if (command === 'path') {
    const path = getPathToNode(state.selectedNodeId)
      .map(node => `${node.tag}#${getNodeId(node)}`)
      .join(' > ');
    addLog('info', 'inspector', path || 'No node selected.');
  } else if (command === 'issues') {
    addLog(
      'info',
      'inspector',
      `${state.snapshot.issues.length} issue(s): ${state.snapshot.issues.map(issue => issue.title).join(', ')}`,
    );
  } else if (command.startsWith('select ')) {
    void requestDebuggerAction('selectNode', { id: command.slice('select '.length).trim() });
  } else if (command === 'connect') {
    void requestDebuggerAction('attach');
  } else if (command === 'status') {
    void requestDebuggerAction('refreshTargets');
  } else if (command === 'refresh' || command === 'reload') {
    void requestDebuggerAction('refreshSnapshot');
  } else if (command === 'snapshot') {
    void requestDebuggerAction('captureElementSnapshot');
  } else if (command === 'heap') {
    void requestDebuggerAction('dumpHeap');
  } else if (command === 'trace') {
    void requestDebuggerAction('captureRendererTrace');
  } else if (command === 'profile') {
    void requestDebuggerAction('captureCpuProfile');
  } else if (command === 'auto on') {
    void requestDebuggerAction('setAutoRefresh', { enabled: true });
  } else if (command === 'auto off') {
    void requestDebuggerAction('setAutoRefresh', { enabled: false });
  } else if (command === 'clear') {
    void requestDebuggerAction('clearLogs');
  } else {
    addLog('warn', 'console', `Unknown command: ${command}. Try help.`);
  }
}
