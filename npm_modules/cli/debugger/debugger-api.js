// HTTP helpers and the external debugger action/event bridge.
async function apiGet(path, params = {}, options = {}) {
  const url = new URL(path, window.location.origin);
  for (const [key, value] of Object.entries(params)) {
    if (value !== undefined && value !== null && value !== '') {
      url.searchParams.set(key, String(value));
    }
  }

  const controller = new AbortController();
  const timeout = window.setTimeout(() => controller.abort(), options.timeoutMs || 8000);
  try {
    const response = await fetch(url, { cache: 'no-store', signal: controller.signal });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || `Request failed: ${response.status}`);
    }
    return data;
  } catch (error) {
    if (error.name === 'AbortError') {
      throw new Error(`Timed out fetching ${url.pathname}.`);
    }
    throw error;
  } finally {
    window.clearTimeout(timeout);
  }
}

async function apiPost(path, params = {}, body = {}, options = {}) {
  const url = new URL(path, window.location.origin);
  for (const [key, value] of Object.entries(params)) {
    if (value !== undefined && value !== null && value !== '') {
      url.searchParams.set(key, String(value));
    }
  }

  const controller = new AbortController();
  const timeout = window.setTimeout(() => controller.abort(), options.timeoutMs || 8000);
  try {
    const response = await fetch(url, {
      method: 'POST',
      cache: 'no-store',
      signal: controller.signal,
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || `Request failed: ${response.status}`);
    }
    return data;
  } catch (error) {
    if (error.name === 'AbortError') {
      throw new Error(`Timed out posting ${url.pathname}.`);
    }
    throw error;
  } finally {
    window.clearTimeout(timeout);
  }
}

function startDebuggerDevReload() {
  if (!window.EventSource || window.location.search.includes('noDevReload=1')) return;
  const source = new EventSource('/api/dev-events');
  let loadedRevision = null;

  source.addEventListener('ready', event => {
    const payload = JSON.parse(event.data);
    loadedRevision = payload.revision;
  });

  source.addEventListener('reload', event => {
    const payload = JSON.parse(event.data);
    if (loadedRevision === null) {
      loadedRevision = payload.revision;
      return;
    }
    if (payload.revision > loadedRevision) {
      persistDebuggerSessionState();
      window.location.reload();
    }
  });
}

function startDebuggerActionStream() {
  if (!window.EventSource) return;
  const source = new EventSource('/api/debugger/events');
  state.debuggerEventStream = source;

  source.addEventListener('ready', event => {
    state.debuggerEventsConnected = true;
    applyDebuggerStatePayload(JSON.parse(event.data));
  });

  source.addEventListener('debugger-action', event => {
    state.debuggerEventsConnected = true;
    applyDebuggerActionPayload(JSON.parse(event.data));
  });

  source.addEventListener('error', () => {
    state.debuggerEventsConnected = false;
  });
}

function applyDebuggerStatePayload(payload) {
  const debuggerState = payload?.state;
  if (!debuggerState) return;
  const revision = Number(debuggerState.revision) || 0;
  if (revision && revision <= state.lastDebuggerRevision) return;
  syncDebuggerSessionState(debuggerState);

  if (revision) state.lastDebuggerRevision = revision;
}

function syncDebuggerSessionState(debuggerState) {
  if (debuggerState.activeSection && debuggerState.activeSection !== state.activeSection) {
    setActiveSection(debuggerState.activeSection);
  }

  if (debuggerState.overlayMode && debuggerState.overlayMode !== state.overlayMode) {
    setOverlayMode(debuggerState.overlayMode);
  }

  if (debuggerState.activeTab && debuggerState.activeTab !== state.activeTab) {
    setActiveTab(debuggerState.activeTab);
  }

  if (typeof debuggerState.autoRefresh === 'boolean' && debuggerState.autoRefresh !== state.autoRefresh) {
    setAutoRefresh(debuggerState.autoRefresh, { silent: true });
  }

  if (
    debuggerState.selectedNodeId &&
    debuggerState.selectedNodeId !== state.selectedNodeId &&
    findNode(debuggerState.selectedNodeId)
  ) {
    selectNode(debuggerState.selectedNodeId, { scrollTree: false });
  }
}

function applyDebuggerActionPayload(payload) {
  const revision = Number(payload?.state?.revision) || 0;
  if (revision && revision <= state.lastDebuggerRevision) return;
  if (payload?.action) {
    applyDebuggerAction(payload.action, payload.params || {});
  }
  if (revision) state.lastDebuggerRevision = revision;
}

async function requestDebuggerAction(action, params = {}) {
  const shouldApplyLocally = !state.debuggerEventsConnected;
  if (shouldApplyLocally) {
    applyDebuggerAction(action, params);
  }

  try {
    await apiPost(
      '/api/debugger/actions',
      {},
      {
        action,
        params,
        source: 'ui',
      },
      { timeoutMs: 5000 },
    );
  } catch (error) {
    if (!shouldApplyLocally) {
      applyDebuggerAction(action, params);
    }
    addLog('warn', 'debugger', `Debugger action bus unavailable; applied ${action} locally. ${error.message}`);
  }
}

function getSelectedTargetParams() {
  const target = state.snapshot.target || {};
  return {
    port: target.proxyPort || target.port || selectedDaemonPort(),
    clientId: target.clientId,
    contextId: target.contextId,
  };
}

function hasSelectedLiveTarget() {
  const params = getSelectedTargetParams();
  return Boolean(params.clientId && params.contextId && Number.isFinite(params.port));
}

function isDebuggerAttached() {
  return state.attached || (state.source === 'daemon' && !state.manualDetach && hasSelectedLiveTarget());
}
