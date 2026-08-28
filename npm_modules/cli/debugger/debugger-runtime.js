// Runtime logs, daemon target refresh, snapshots, heap, exports, and preview copy.
function renderLogs() {
  const search = elements.logSearch.value.trim().toLowerCase();
  const rows = state.snapshot.logs
    .filter(log => !search || `${log.level} ${log.source} ${log.message}`.toLowerCase().includes(search))
    .map(
      log => `
              <div class="log-row">
                <span>${escapeHtml(log.time)}</span>
                <span class="level-${escapeHtml(log.level)}">${escapeHtml(log.level.toUpperCase())}</span>
                <span>[${escapeHtml(log.source)}] ${escapeHtml(log.message)}</span>
              </div>
            `,
    )
    .join('');
  elements.logs.innerHTML = rows || `<div class="empty">No logs match the current filter.</div>`;
}

function trimRuntimeLogs() {
  if (state.snapshot.logs.length <= MAX_RUNTIME_LOG_ROWS) return;
  state.snapshot.logs = state.snapshot.logs.slice(-MAX_RUNTIME_LOG_ROWS);
}

function addLog(level, source, message) {
  const now = new Date();
  const time = now.toTimeString().slice(0, 8);
  state.snapshot.logs.push({ time, level, source, message });
  trimRuntimeLogs();
  if (state.activeSection === 'logs') {
    renderLogs();
    elements.logs.scrollTop = elements.logs.scrollHeight;
  }
}

function appendRuntimeLogs(logs) {
  let added = 0;
  for (const log of logs || []) {
    const normalized = {
      time: log.time || '',
      level: log.level || 'info',
      source: log.source || 'valdi',
      message: log.message || '',
    };
    state.snapshot.logs.push(normalized);
    added++;
  }
  if (added > 0) {
    trimRuntimeLogs();
    if (state.activeSection === 'logs') {
      renderLogs();
      elements.logs.scrollTop = elements.logs.scrollHeight;
    }
  }
  return added;
}

function runtimeLogParams() {
  const target = state.snapshot.target || {};
  const params = new URLSearchParams();
  params.set('limit', '120');
  if (target.applicationId) params.set('applicationId', target.applicationId);
  if (target.platform) params.set('platform', target.platform);
  return params;
}

function stopRuntimeLogStream() {
  if (state.runtimeLogStream) {
    state.runtimeLogStream.close();
    state.runtimeLogStream = null;
  }
  state.runtimeLogStreamKey = null;
}

function startRuntimeLogStream(options = {}) {
  if (state.source !== 'daemon' || !isDebuggerAttached()) {
    stopRuntimeLogStream();
    return;
  }

  const params = runtimeLogParams();
  const key = params.toString();
  if (state.runtimeLogStream && state.runtimeLogStreamKey === key) return;

  stopRuntimeLogStream();
  const source = new EventSource(debuggerEventSourceUrl(`/api/runtime-logs/stream?${key}`));
  state.runtimeLogStream = source;
  state.runtimeLogStreamKey = key;

  source.addEventListener('meta', event => {
    const payload = JSON.parse(event.data);
    state.runtimeLogPath = payload.logFile || null;
    if (!options.silent) {
      addLog(
        'info',
        'logs',
        payload.logFile
          ? `Streaming runtime logs from ${payload.logFile}.`
          : 'Runtime log stream is waiting for a Valdi log file.',
      );
    }
  });

  source.addEventListener('logs', event => {
    const payload = JSON.parse(event.data);
    appendRuntimeLogs(payload.logs || []);
  });

  source.addEventListener('stream-error', event => {
    const payload = JSON.parse(event.data);
    addLog('error', 'logs', payload.error || 'Runtime log stream failed.');
  });

  source.addEventListener('error', () => {
    if (state.runtimeLogStream === source) {
      state.runtimeLogPath = null;
    }
  });
}

async function refreshTargets(options = {}) {
  try {
    if (!options.silent) addLog('info', 'daemon', 'Refreshing Valdi daemon targets.');
    const status = await apiGet('/api/status');
    state.lastStatus = status;
    const targets = [];

    for (const portStatus of status.ports) {
      for (const client of portStatus.clients || []) {
        for (const context of client.contexts || []) {
          targets.push({
            id: `${portStatus.port}:${client.client_id}:${context.id}`,
            name: context.rootComponentName || client.application_id || `Client ${client.client_id}`,
            platform: client.platform || portStatus.portName,
            state: 'available',
            transport: `daemon:${portStatus.port}`,
            port: portStatus.port,
            proxyPort: portStatus.port,
            clientId: client.client_id,
            contextId: context.id,
            applicationId: client.application_id,
          });
        }
      }
    }

    if (targets.length) {
      const previousTargetId = state.snapshot.target?.id || null;
      const selectedTarget = chooseLiveTarget(targets);
      const targetChanged = selectedTarget.id !== previousTargetId;
      if (targetChanged && !(await preparePerformanceTraceTargetSwitch(selectedTarget))) {
        return;
      }
      state.snapshot.targets = markSelectedTarget(targets, selectedTarget);
      state.snapshot.target = {
        ...state.snapshot.target,
        ...selectedTarget,
      };
      elements.portSelect.value = String(selectedTarget.port);
      state.attached = false;
      state.rootSnapshotImage = null;
      state.rootSnapshotRequestId++;
      state.lastError = null;
      if (!options.silent)
        addLog('info', 'daemon', `Found ${targets.length} Valdi context(s); selected ${selectedTarget.name}.`);
      if (!state.manualDetach && options.autoAttach !== false) {
        if (options.skipSnapshotIfSame && !targetChanged) {
          render();
          return;
        }
        await loadRealSnapshot(selectedTarget, {
          silent: options.silent,
          preserveSelection: state.source === 'daemon' && !targetChanged,
          queueIfBusy: options.queueIfBusy === true,
        });
        return;
      }
    } else {
      const errors = status.ports
        .map(portStatus => `${portStatus.port}: ${portStatus.error || 'no contexts'}`)
        .join('; ');
      state.lastError = errors;
      state.snapshot.targets = [];
      state.attached = false;
      state.source = 'empty';
      state.rootSnapshotImage = null;
      state.rootSnapshotRequestId++;
      if (!options.silent) addLog('warn', 'daemon', `No live Valdi contexts found. ${errors}`);
    }
    render();
  } catch (error) {
    state.lastError = error.message;
    if (!options.silent) addLog('error', 'daemon', error.message);
    renderHeader();
  }
}

async function loadRealSnapshot(target, options = {}) {
  if (state.refreshInFlight) {
    // A fetch is already running. apiGet aborts on its own timeout and the finally block below
    // always clears refreshInFlight, so the in-flight request can never get permanently stuck.
    // Never force-restart here: the old 5s "stalled" bypass left the original request running and
    // stacked overlapping /api/snapshot calls onto the daemon. Queue the latest request or drop it.
    if (options.queueIfBusy) {
      state.pendingRefreshRequest = {
        target,
        options: {
          ...options,
          queueIfBusy: false,
          silent: false,
        },
      };
      if (!options.silent)
        addLog('info', 'daemon', 'A Valdi snapshot fetch is already running; queued another refresh.');
    }
    return;
  }
  state.refreshInFlight = true;
  state.refreshStartedAt = Date.now();
  const params = target
    ? { port: target.port || target.proxyPort, clientId: target.clientId, contextId: target.contextId }
    : getSelectedTargetParams();
  const previousSelectedNodeId = state.selectedNodeId;
  const previousTargetKey = debuggerTargetKey();
  const previousLogs = state.source === 'daemon' ? state.snapshot.logs : [];

  try {
    if (!options.silent) addLog('info', 'daemon', `Fetching Valdi tree from port ${params.port}.`);
    const snapshot = await apiGet('/api/snapshot', params, { timeoutMs: 20000 });
    state.snapshot = decorateSnapshot(snapshot);
    if (debuggerTargetKey() !== previousTargetKey) resetDebuggerToolsForTarget();
    state.snapshot.logs = [...previousLogs, ...(state.snapshot.logs || [])];
    trimRuntimeLogs();
    state.selectedNodeId =
      options.preserveSelection && findNodeInTree(state.snapshot.tree, previousSelectedNodeId)
        ? previousSelectedNodeId
        : getNodeId(state.snapshot.tree);
    if (!options.preserveSelection) collapseTree();
    state.source = 'daemon';
    state.attached = true;
    state.manualDetach = false;
    state.lastError = null;
    state.selectedSnapshotImage = null;
    state.lastUpdated = new Date().toLocaleTimeString();
    elements.portSelect.value = String(state.snapshot.target.proxyPort || params.port);
    render();
    maybeRefreshDebuggerTools(state.activeSection);
    if (!options.silent || !state.rootSnapshotImage) {
      void captureRootSnapshot(params, options);
    }
    if (state.activeSection === 'logs') {
      startRuntimeLogStream({ silent: options.silent });
    } else {
      stopRuntimeLogStream();
    }
  } catch (error) {
    state.lastError = error.message;
    state.source = 'empty';
    state.attached = false;
    state.rootSnapshotImage = null;
    state.rootSnapshotRequestId++;
    stopRuntimeLogStream();
    if (!options.silent) addLog('error', 'daemon', error.message);
    render();
  } finally {
    state.refreshInFlight = false;
    state.refreshStartedAt = null;
    const pendingRefreshRequest = state.pendingRefreshRequest;
    state.pendingRefreshRequest = null;
    if (pendingRefreshRequest) {
      window.setTimeout(() => {
        loadRealSnapshot(pendingRefreshRequest.target, pendingRefreshRequest.options);
      }, 0);
    }
  }
}

function firstElementDescendant(node) {
  if (!node) return null;
  let found = null;
  valdiDebuggerTreeModel.walk(
    node,
    current => {
      if (!current.element || current.element.id === undefined) return true;
      found = current;
      return false;
    },
    [],
    0,
  );
  return found;
}

async function captureSelectedElementSnapshot() {
  const node = selectedNode();
  if (!node) {
    addLog('warn', 'inspector', 'No node is selected because no snapshot is loaded.');
    return;
  }
  const elementNode = firstElementDescendant(node);
  const elementId =
    elementNode && elementNode.element && elementNode.element.id !== undefined ? String(elementNode.element.id) : null;
  if (!elementId) {
    addLog('warn', 'inspector', 'No element descendant is available for the selected node.');
    return;
  }

  try {
    const params = {
      ...getSelectedTargetParams(),
      elementId,
    };
    const result = await apiGet('/api/element-snapshot', params, { timeoutMs: 20000 });
    state.selectedSnapshotImage = result.image;
    const sourceNote = elementNode === node ? '' : ` via ${elementNode.tag} #${elementId}`;
    addLog('info', 'inspector', `Captured element snapshot for ${node.tag} #${getNodeId(node)}${sourceNote}.`);
    renderInspector();
  } catch (error) {
    addLog('error', 'inspector', error.message);
  }
}

async function dumpHeap() {
  try {
    const result = await apiPost('/api/heap', getSelectedTargetParams(), { performGC: false }, { timeoutMs: 65000 });
    const heap = result.heap || {};
    const fileName = `${targetFilePrefix()}-heap-${new Date().toISOString().replaceAll(':', '-')}.json`;
    downloadJson(heap, fileName, 'Captured heap report.');
    if (heap.memoryUsageBytes !== undefined) {
      const mb = (heap.memoryUsageBytes / 1024 / 1024).toFixed(2);
      addLog('info', 'heap', `Memory usage: ${mb} MB. The full report is available in the export panel.`);
    }
  } catch (error) {
    addLog('error', 'heap', error.message);
  }
}

function targetFilePrefix() {
  return (state.snapshot.target?.name || 'valdi').replace(/[^a-z0-9_-]+/gi, '-').toLowerCase();
}

function downloadJson(payload, fileName, logMessage) {
  const json = JSON.stringify(payload, null, 2);
  const blob = new Blob([json], { type: 'application/json' });
  if (state.exportObjectUrl) {
    URL.revokeObjectURL(state.exportObjectUrl);
    state.exportObjectUrl = null;
  }
  const url = URL.createObjectURL(blob);
  state.exportObjectUrl = url;
  elements.exportTitle.textContent = fileName;
  elements.exportText.value = json;
  elements.exportOpenLink.href = url;
  elements.exportOpenLink.download = fileName;
  elements.exportPanel.classList.add('open');
  addLog('info', 'debugger', `${logMessage} Open or copy it from the export panel.`);
}

async function copyPreview() {
  if (state.rootSnapshotImage && navigator.clipboard?.write && window.ClipboardItem) {
    try {
      const response = await fetch(state.rootSnapshotImage);
      const blob = await response.blob();
      const type = blob.type || 'image/png';
      await navigator.clipboard.write([new ClipboardItem({ [type]: blob })]);
      addLog('info', 'preview', 'Copied live preview image.');
      return;
    } catch (error) {
      addLog('warn', 'preview', `Preview image copy failed: ${error.message}. Copying snapshot JSON instead.`);
    }
  }

  if (!hasSnapshotTree()) {
    addLog('warn', 'preview', 'No preview image or snapshot JSON is available to copy.');
    return;
  }

  try {
    await navigator.clipboard.writeText(previewSnapshotProjectionJson());
    addLog('info', 'preview', 'Copied current snapshot JSON.');
  } catch (error) {
    addLog('error', 'preview', `Copy failed: ${error.message}`);
  }
}

function previewSnapshotProjectionJson() {
  return JSON.stringify(valdiDebuggerTreeModel.projectSnapshot(state.snapshot), null, 2);
}
