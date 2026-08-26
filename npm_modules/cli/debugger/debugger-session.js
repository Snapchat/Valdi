// Lightweight debugger UI session persistence across page reloads.
const DEBUGGER_SESSION_STORAGE_KEY = 'valdi.debugger.session.v1';

let debuggerSessionPersistInterval = null;

function debuggerSessionTarget() {
  const target = state.snapshot.target || {};
  if (!target.id && !target.clientId && !target.contextId) return null;
  return {
    id: target.id || '',
    name: target.name || '',
    platform: target.platform || '',
    transport: target.transport || '',
    state: target.state || 'available',
    port: target.port || target.proxyPort || selectedDaemonPort(),
    proxyPort: target.proxyPort || target.port || selectedDaemonPort(),
    clientId: target.clientId,
    contextId: target.contextId,
    applicationId: target.applicationId,
  };
}

function debuggerSessionPayload() {
  return {
    activeSection: state.activeSection,
    activeTab: state.activeTab,
    overlayMode: state.overlayMode,
    autoRefresh: state.autoRefresh,
    followLatestTarget: state.followLatestTarget,
    manualDetach: state.manualDetach,
    selectedNodeId: state.selectedNodeId,
    target: debuggerSessionTarget(),
    port: selectedDaemonPort(),
    treeSearch: elements.treeSearch.value,
    logSearch: elements.logSearch.value,
    expandedNodeIds: Array.from(state.expandedNodeIds),
    rendererTracing: elements.rendererTracingToggle.checked,
    traceDuration: elements.traceDurationInput.value,
    activeTraceTarget: state.performance.activeTraceTarget,
    traceActive: state.performance.traceActive,
    traceResultPending: state.performance.traceResultPending,
    traceStateUnknown: state.performance.traceStateUnknown,
    profileDuration: elements.profileDurationInput.value,
    profileContextId: elements.profileContextSelect.value,
    providerTab: state.providers.activeTab,
    settingsGroupId: state.settings.selectedGroupId,
  };
}

function persistDebuggerSessionState() {
  try {
    window.sessionStorage.setItem(DEBUGGER_SESSION_STORAGE_KEY, JSON.stringify(debuggerSessionPayload()));
  } catch (error) {
    console.warn(`Could not persist debugger session: ${error.message}`);
  }
}

function readDebuggerSessionState() {
  try {
    const raw = window.sessionStorage.getItem(DEBUGGER_SESSION_STORAGE_KEY);
    return raw ? JSON.parse(raw) : null;
  } catch (error) {
    console.warn(`Could not read debugger session: ${error.message}`);
    return null;
  }
}

function restoreDebuggerSessionState() {
  const saved = readDebuggerSessionState();
  if (!saved || typeof saved !== 'object') return false;

  const activeSection = normalizeSection(saved.activeSection);
  if (elements.sectionPanels.some(panel => panel.dataset.sectionPanel === activeSection)) {
    state.activeSection = activeSection;
  }

  if (['overview', 'props', 'state', 'issues', 'raw'].includes(saved.activeTab)) {
    state.activeTab = saved.activeTab;
  }

  if (['live', 'views', 'components', 'issues'].includes(saved.overlayMode)) {
    state.overlayMode =
      saved.overlayMode === 'views' || saved.overlayMode === 'components' ? 'live' : saved.overlayMode;
  }

  if (typeof saved.autoRefresh === 'boolean') state.autoRefresh = saved.autoRefresh;
  if (typeof saved.followLatestTarget === 'boolean') state.followLatestTarget = saved.followLatestTarget;
  if (typeof saved.manualDetach === 'boolean') state.manualDetach = saved.manualDetach;
  if (saved.selectedNodeId !== undefined && saved.selectedNodeId !== null) {
    state.selectedNodeId = String(saved.selectedNodeId);
  }

  if (Array.isArray(saved.expandedNodeIds)) {
    state.expandedNodeIds = new Set(saved.expandedNodeIds.map(String));
  }

  const port = Number.parseInt(String(saved.port || saved.target?.port || saved.target?.proxyPort || ''), 10);
  if (Number.isFinite(port)) {
    elements.portSelect.value = String(port);
    state.snapshot.target = {
      ...state.snapshot.target,
      port,
      proxyPort: port,
    };
  }

  if (saved.target && typeof saved.target === 'object') {
    state.snapshot.target = {
      ...state.snapshot.target,
      ...saved.target,
    };
    state.snapshot.targets = [state.snapshot.target];
    state.source = 'daemon';
  }

  if (saved.treeSearch !== undefined) elements.treeSearch.value = String(saved.treeSearch);
  if (saved.logSearch !== undefined) elements.logSearch.value = String(saved.logSearch);
  if (typeof saved.rendererTracing === 'boolean') elements.rendererTracingToggle.checked = saved.rendererTracing;
  if (saved.traceDuration !== undefined) elements.traceDurationInput.value = String(saved.traceDuration);
  const persistedTraceMayBeActive =
    saved.traceStateUnknown === true || saved.traceActive === true || saved.traceResultPending === true;
  if (saved.activeTraceTarget && typeof saved.activeTraceTarget === 'object') {
    const activeTracePort = Number.parseInt(String(saved.activeTraceTarget.port || ''), 10);
    if (
      Number.isFinite(activeTracePort) &&
      saved.activeTraceTarget.clientId !== undefined &&
      saved.activeTraceTarget.contextId !== undefined
    ) {
      state.performance.activeTraceTarget = {
        port: activeTracePort,
        clientId: String(saved.activeTraceTarget.clientId),
        contextId: String(saved.activeTraceTarget.contextId),
      };
    }
  }
  if (state.performance.activeTraceTarget === null && persistedTraceMayBeActive) {
    const fallbackTraceTarget = state.snapshot.target || saved.target;
    const fallbackTracePort = Number.parseInt(String(fallbackTraceTarget?.port || ''), 10);
    if (
      Number.isFinite(fallbackTracePort) &&
      fallbackTraceTarget?.clientId !== undefined &&
      fallbackTraceTarget?.contextId !== undefined
    ) {
      state.performance.activeTraceTarget = {
        port: fallbackTracePort,
        clientId: String(fallbackTraceTarget.clientId),
        contextId: String(fallbackTraceTarget.contextId),
      };
    }
  }
  state.performance.traceActive = false;
  state.performance.traceResultPending = false;
  state.performance.traceStateUnknown = state.performance.activeTraceTarget !== null || persistedTraceMayBeActive;
  if (saved.profileDuration !== undefined) elements.profileDurationInput.value = String(saved.profileDuration);
  if (saved.profileContextId !== undefined) elements.profileContextSelect.value = String(saved.profileContextId);
  if (saved.providerTab === 'storage' || saved.providerTab === 'sql') state.providers.activeTab = saved.providerTab;
  if (saved.settingsGroupId !== undefined && saved.settingsGroupId !== null) {
    state.settings.selectedGroupId = String(saved.settingsGroupId);
  }

  return true;
}

function applyDebuggerSessionDomState() {
  elements.sectionButtons.forEach(button => {
    button.classList.toggle('active', button.dataset.sectionButton === state.activeSection);
  });
  elements.sectionPanels.forEach(panel => {
    panel.classList.toggle('active', panel.dataset.sectionPanel === state.activeSection);
  });
  document.querySelectorAll('.tabs button').forEach(button => {
    button.classList.toggle('active', button.dataset.tab === state.activeTab);
  });
  document.querySelectorAll('.segmented button').forEach(button => button.classList.remove('active'));
  document
    .getElementById(`mode${state.overlayMode[0].toUpperCase()}${state.overlayMode.slice(1)}`)
    ?.classList.add('active');
  if (state.activeSection === 'logs') renderLogs();
}

function installDebuggerSessionPersistence() {
  if (debuggerSessionPersistInterval) clearInterval(debuggerSessionPersistInterval);
  debuggerSessionPersistInterval = window.setInterval(persistDebuggerSessionState, 1000);
  window.addEventListener('beforeunload', persistDebuggerSessionState);
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) persistDebuggerSessionState();
  });
}
