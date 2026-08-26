// Renderer trace and Hermes CPU profile UI behavior.
function readSecondsInput(input, maximumSeconds) {
  const value = Number.parseFloat(input.value);
  if (!Number.isFinite(value)) return 5;
  return Math.max(0.1, Math.min(maximumSeconds, value));
}

function formatMs(value) {
  if (!Number.isFinite(value)) return 'n/a';
  return `${value.toFixed(value >= 100 ? 0 : 1)} ms`;
}

function traceSummaryText(result) {
  const summary = result?.summary;
  if (!summary || !result.traceCount) return 'No renderer trace captured.';
  const topComponent = summary.topComponents?.[0];
  const topTrigger = summary.topViewModelTriggers?.[0];
  const parts = [
    `<strong>${result.traceCount}</strong> events`,
    `<strong>${summary.durationTraceCount || 0}</strong> durations`,
    'process-wide capture',
  ];
  if (result.captureTarget?.contextId) {
    parts.push(`requested from ${escapeHtml(String(result.captureTarget.contextId))}`);
  }
  if (result.droppedTraceEventCount) parts.push(`${result.droppedTraceEventCount} dropped`);
  if (result.traceEventLimitReached) parts.push('native trace limit reached');
  if (result.elapsedMs !== undefined) parts.push(`window ${formatMs(result.elapsedMs)}`);
  if (topComponent) parts.push(`top render ${escapeHtml(topComponent.name)} ${formatMs(topComponent.durationMs)}`);
  if (topTrigger) parts.push(`top trigger ${escapeHtml(topTrigger.name)} x${topTrigger.count}`);
  return parts.join(' · ');
}

function traceEventsHtml(result) {
  const traces = Array.isArray(result?.traces) ? result.traces : [];
  if (!traces.length) return '';
  const baseMicros = traces.reduce((minimum, trace) => Math.min(minimum, trace.startMicros), traces[0].startMicros);
  const visibleTraces = traces.slice(0, 80);
  const eventLimitText =
    visibleTraces.length === traces.length ? '' : ` (first ${visibleTraces.length} of ${traces.length})`;
  const rows = visibleTraces.map(trace => {
    const durationMs = Math.max(0, trace.endMicros - trace.startMicros) / 1000;
    const details = JSON.stringify(
      {
        name: trace.trace,
        threadId: trace.threadId,
        startMs: (trace.startMicros - baseMicros) / 1000,
        durationMs,
      },
      null,
      2,
    );
    return `
      <details class="trace-event-row">
        <summary class="trace-event-summary" title="${escapeHtml(trace.trace)}">
          <div class="trace-event-name">${escapeHtml(trace.trace)}</div>
          <div class="trace-event-meta">${escapeHtml(((trace.startMicros - baseMicros) / 1000).toFixed(2))} ms</div>
          <div class="trace-event-meta">${escapeHtml(formatMs(durationMs))}</div>
        </summary>
        <pre class="trace-event-details">${escapeHtml(details)}</pre>
      </details>
    `;
  });
  return `
    <div class="trace-events-header">
      <div>Events${eventLimitText}</div>
      <div>Start</div>
      <div>Duration</div>
    </div>
    ${rows.join('')}
  `;
}

function buildPerformanceTracePerfettoPayload(result) {
  const traces = Array.isArray(result?.traces) ? result.traces : [];
  const minimumStartMicros = traces.reduce(
    (minimum, trace) => Math.min(minimum, trace.startMicros),
    traces[0]?.startMicros || 0,
  );
  const threadIds = Array.from(new Set(traces.map(trace => trace.threadId)))
    .sort((left, right) => left - right)
    .slice(0, 256);
  const traceEvents = [
    {
      name: 'process_name',
      ph: 'M',
      pid: 1,
      args: { name: 'Valdi' },
    },
    ...threadIds.map(threadId => ({
      name: 'thread_name',
      ph: 'M',
      pid: 1,
      tid: threadId,
      args: { name: `Valdi thread ${threadId}` },
    })),
  ];
  for (const trace of traces) {
    const instant = /(?:^|\.)Renderer\.viewModelChange\.[^.]+\..+$/.test(trace.trace);
    const event = {
      name: trace.trace,
      cat: 'valdi',
      ph: instant ? 'i' : 'X',
      pid: 1,
      tid: trace.threadId,
      ts: trace.startMicros - minimumStartMicros,
    };
    if (instant) event.s = 't';
    else event.dur = Math.max(0, trace.endMicros - trace.startMicros);
    traceEvents.push(event);
  }
  return {
    displayTimeUnit: 'ms',
    metadata: result?.perfettoMetadata || {},
    traceEvents,
  };
}

function profileSummaryText(result) {
  const summary = result?.summary;
  if (!summary || !result.profile) return 'No CPU profile captured.';
  const topFunction = summary.topFunctions?.[0];
  const parts = [
    `<strong>${summary.sampleCount || 0}</strong> samples`,
    `<strong>${summary.nodeCount || 0}</strong> nodes`,
  ];
  if (result.elapsedMs !== undefined) parts.push(`window ${formatMs(result.elapsedMs)}`);
  if (topFunction) parts.push(`top function ${escapeHtml(topFunction.name)} x${topFunction.sampleCount}`);
  return parts.join(' · ');
}

function normalizePerformanceTraceTarget(target) {
  const port = Number(target?.port);
  if (!Number.isFinite(port) || target?.clientId === undefined || target?.contextId === undefined) return null;
  return {
    port,
    clientId: String(target.clientId),
    contextId: String(target.contextId),
  };
}

function performanceTraceTargetsMatch(left, right) {
  const normalizedLeft = normalizePerformanceTraceTarget(left);
  const normalizedRight = normalizePerformanceTraceTarget(right);
  return Boolean(
    normalizedLeft &&
      normalizedRight &&
      normalizedLeft.port === normalizedRight.port &&
      normalizedLeft.clientId === normalizedRight.clientId &&
      normalizedLeft.contextId === normalizedRight.contextId,
  );
}

function syncActiveTraceState(result, fallbackTarget) {
  state.performance.traceStateUnknown = false;
  state.performance.traceActive = Boolean(result?.recording);
  state.performance.traceResultPending = Boolean(result?.completedRecordingAvailable);
  const resultContextId = result?.recording ? result.contextId : result?.completedContextId;
  const resultTarget =
    normalizePerformanceTraceTarget(result?.captureTarget) || normalizePerformanceTraceTarget(fallbackTarget);
  if ((result?.recording || result?.completedRecordingAvailable) && resultTarget) {
    state.performance.activeTraceTarget = {
      ...resultTarget,
      contextId: resultContextId === undefined ? resultTarget.contextId : String(resultContextId),
    };
  } else if (!state.performance.traceCapturePending) {
    state.performance.activeTraceTarget = null;
  }
  persistPerformanceTraceState();
}

function persistPerformanceTraceState() {
  if (typeof persistDebuggerSessionState === 'function') persistDebuggerSessionState();
}

function markPerformanceTraceStateUnknown(target) {
  const normalizedTarget = normalizePerformanceTraceTarget(target);
  state.performance.traceStateUnknown = true;
  if (normalizedTarget) state.performance.activeTraceTarget = normalizedTarget;
  persistPerformanceTraceState();
}

function syncActiveProfileState(activeProfile) {
  const contextId = activeProfile?.contextId;
  state.performance.profileActive = Boolean(activeProfile?.profiling);
  state.performance.activeProfileContextId =
    state.performance.profileActive && contextId !== undefined && contextId !== null ? String(contextId) : null;
}

function renderPerformance() {
  const perf = state.performance;
  const traceTargetAvailable = hasSelectedLiveTarget();
  const traceSupported = perf.traceSupported !== false;
  const traceBusy = perf.traceActive || perf.traceCapturePending || perf.traceResultPending || perf.traceStateUnknown;
  elements.traceStatusPill.textContent = perf.traceCapturePending
    ? 'Capturing'
    : perf.traceStateUnknown
      ? 'Unknown'
      : perf.traceActive
        ? 'Recording'
        : perf.traceResultPending
          ? 'Ready'
          : 'Idle';
  elements.traceStatusPill.className = `source-pill ${traceBusy ? 'live' : ''}`;
  elements.traceStartButton.disabled = !traceTargetAvailable || !traceSupported || traceBusy;
  elements.traceStopButton.disabled =
    !traceTargetAvailable ||
    (!perf.traceActive && !perf.traceResultPending && !perf.traceStateUnknown) ||
    perf.traceCapturePending;
  elements.traceCaptureButton.disabled = !traceTargetAvailable || !traceSupported || perf.traceCapturePending;
  elements.traceCaptureButton.textContent =
    perf.traceActive || perf.traceResultPending || perf.traceStateUnknown ? 'Stop & Capture' : 'Capture';
  elements.rendererTracingToggle.disabled = traceBusy;
  elements.traceDurationInput.disabled = traceBusy;
  elements.traceExportButton.disabled = !Array.isArray(perf.lastTrace?.traces);
  elements.traceSummary.innerHTML = traceSummaryText(perf.lastTrace);
  elements.traceEvents.innerHTML = traceEventsHtml(perf.lastTrace);

  elements.profileStatusPill.textContent = perf.profileActive ? 'Recording' : 'Idle';
  elements.profileStatusPill.className = `source-pill ${perf.profileActive ? 'live' : ''}`;
  elements.profileStartButton.disabled = perf.profileActive;
  elements.profileStopButton.disabled = !perf.profileActive;
  elements.profileCaptureButton.disabled = perf.profileActive;
  elements.profileExportButton.disabled = !perf.lastProfile?.profile;
  elements.profileSummary.innerHTML = profileSummaryText(perf.lastProfile);

  elements.profileContextSelect.disabled = perf.profileActive;
  const selectedContext = perf.activeProfileContextId || elements.profileContextSelect.value;
  elements.profileContextSelect.innerHTML = [
    `<option value="">Auto context</option>`,
    ...perf.profileContexts.map(context => {
      const selected = context.id === selectedContext ? ' selected' : '';
      return `<option value="${escapeHtml(context.id)}"${selected}>${escapeHtml(context.title || context.id)}</option>`;
    }),
  ].join('');
}

async function refreshPerformanceTraceStatus(options) {
  const targetParams =
    normalizePerformanceTraceTarget(options.target) ||
    normalizePerformanceTraceTarget(state.performance.activeTraceTarget) ||
    normalizePerformanceTraceTarget(getSelectedTargetParams());
  if (!targetParams) {
    syncActiveTraceState(null);
    state.performance.traceSupported = true;
    if (options.render !== false) renderPerformance();
    return null;
  }

  try {
    const result = await apiGet('/api/performance/trace/status', targetParams, {
      timeoutMs: options.timeoutMs || 10000,
    });
    syncActiveTraceState(result, targetParams);
    state.performance.traceSupported = result.tracingSupported === true;
    return result;
  } catch (error) {
    if (!options.silent) addLog('warn', 'trace', error.message);
    return null;
  } finally {
    if (options.render !== false) renderPerformance();
  }
}

async function startPerformanceTrace() {
  const targetParams = normalizePerformanceTraceTarget(getSelectedTargetParams());
  if (!targetParams) return;
  const wasUnknown = state.performance.traceStateUnknown === true;
  try {
    const status = await refreshPerformanceTraceStatus({ silent: true, render: false, target: targetParams });
    if (!status) {
      addLog('warn', 'trace', 'Could not verify renderer trace status; start was not sent.');
      return;
    }
    if (wasUnknown) {
      addLog('info', 'trace', 'Recovering the uncertain renderer trace before starting another.');
      await stopPerformanceTrace({ target: targetParams, render: false });
      return;
    }
    if (status?.recording || status?.completedRecordingAvailable) {
      syncActiveTraceState(status, targetParams);
      addLog('warn', 'trace', 'A renderer trace is active or waiting to be retrieved. Use Stop first.');
      return;
    }
    if (status.tracingSupported !== true) {
      addLog('warn', 'trace', 'This Valdi runtime does not support renderer trace capture.');
      return;
    }
    markPerformanceTraceStateUnknown(targetParams);
    const result = await apiPost('/api/performance/trace/start', targetParams, {
      rendererTracing: elements.rendererTracingToggle.checked,
    });
    syncActiveTraceState(result, targetParams);
    state.performance.traceSupported = result.tracingSupported !== false;
    addLog('info', 'trace', `Started process-wide renderer trace from context ${targetParams.contextId}.`);
  } catch (error) {
    addLog('error', 'trace', error.message);
    const status = await refreshPerformanceTraceStatus({ silent: true, render: false, target: targetParams });
    if (!status) markPerformanceTraceStateUnknown(targetParams);
  } finally {
    renderPerformance();
  }
}

async function stopPerformanceTrace(options) {
  const targetParams =
    normalizePerformanceTraceTarget(options.target) ||
    normalizePerformanceTraceTarget(state.performance.activeTraceTarget) ||
    normalizePerformanceTraceTarget(getSelectedTargetParams());
  if (!targetParams) return false;
  markPerformanceTraceStateUnknown(targetParams);
  const applyResult = result => {
    syncActiveTraceState(result, targetParams);
    state.performance.traceSupported = result.tracingSupported === true;
    state.performance.lastTrace = result;
    addLog('info', 'trace', `Captured ${result.traceCount || 0} Valdi trace event(s).`);
    if (result.completionError) addLog('warn', 'trace', result.completionError);
    return true;
  };
  try {
    const result = await apiPost('/api/performance/trace/stop', targetParams, {}, { timeoutMs: 30000 });
    return applyResult(result);
  } catch (error) {
    addLog('error', 'trace', error.message);
    const status = await refreshPerformanceTraceStatus({ silent: true, render: false, target: targetParams });
    if (!status) {
      markPerformanceTraceStateUnknown(targetParams);
      return false;
    }
    try {
      const replayedResult = await apiPost('/api/performance/trace/stop', targetParams, {}, { timeoutMs: 30000 });
      return applyResult(replayedResult);
    } catch (replayError) {
      addLog('error', 'trace', `Could not retrieve the retained renderer trace: ${replayError.message}`);
      markPerformanceTraceStateUnknown(targetParams);
      return false;
    }
  } finally {
    if (options.render !== false) renderPerformance();
  }
}

async function preparePerformanceTraceTargetSwitch(nextTarget) {
  const activeTarget = normalizePerformanceTraceTarget(state.performance.activeTraceTarget);
  if (!activeTarget) {
    if (state.performance.traceActive || state.performance.traceResultPending || state.performance.traceStateUnknown) {
      addLog('warn', 'trace', 'Recover the uncertain renderer trace before switching targets.');
      return false;
    }
    return true;
  }
  if (performanceTraceTargetsMatch(activeTarget, nextTarget)) return true;
  if (state.performance.traceCapturePending) {
    addLog('warn', 'trace', 'Wait for the process-wide renderer trace capture to finish before switching targets.');
    return false;
  }
  if (!state.performance.traceActive && !state.performance.traceResultPending && !state.performance.traceStateUnknown) {
    return true;
  }
  addLog('info', 'trace', 'Stopping the active process-wide renderer trace before switching targets.');
  if (await stopPerformanceTrace({ target: activeTarget, render: false })) return true;
  if (!(await isPerformanceTraceTargetVerifiedMissing(activeTarget))) return false;

  state.performance.traceActive = false;
  state.performance.traceResultPending = false;
  state.performance.traceStateUnknown = false;
  state.performance.activeTraceTarget = null;
  state.performance.traceSupported = true;
  persistPerformanceTraceState();
  addLog('warn', 'trace', 'The previous trace target disconnected; discarded its unrecoverable trace state.');
  return true;
}

async function isPerformanceTraceTargetVerifiedMissing(target) {
  let status;
  try {
    status = await apiGet('/api/status', { port: target.port }, { timeoutMs: 5000 });
  } catch {
    // A transport failure does not prove that the prior context disappeared.
    return false;
  }

  const portStatus = status?.ports?.find(candidate => Number(candidate.port) === target.port);
  if (!portStatus) return false;
  // inspectPort reports connected:false for transient connection and inventory failures too.
  // Only a successfully connected inventory can prove that the prior target disappeared.
  if (portStatus.connected !== true || !Array.isArray(portStatus.clients)) return false;

  const client = portStatus.clients.find(candidate => String(candidate.client_id) === target.clientId);
  if (!client) return true;
  if (client.contextError !== null && client.contextError !== undefined) return false;
  if (!Array.isArray(client.contexts)) return false;
  return !client.contexts.some(context => String(context.id) === target.contextId);
}

async function recoverPerformanceTraceState() {
  const target =
    normalizePerformanceTraceTarget(state.performance.activeTraceTarget) ||
    normalizePerformanceTraceTarget(getSelectedTargetParams());
  if (!target) return;
  const wasUnknown = state.performance.traceStateUnknown === true;
  const status = await refreshPerformanceTraceStatus({ silent: true, render: false, target });
  if (!status) {
    if (wasUnknown) markPerformanceTraceStateUnknown(target);
    renderPerformance();
    return;
  }
  if (status.recording || status.completedRecordingAvailable || wasUnknown) {
    const completedTarget = {
      ...target,
      contextId: String(status.completedContextId || target.contextId),
    };
    const recovered = await stopPerformanceTrace({ target: completedTarget, render: false });
    if (recovered) {
      addLog('info', 'trace', 'Recovered the completed process-wide renderer trace from the runtime.');
    }
  }
  renderPerformance();
}

async function capturePerformanceTrace() {
  const durationMs = Math.round(readSecondsInput(elements.traceDurationInput, 15) * 1000);
  const targetParams = normalizePerformanceTraceTarget(getSelectedTargetParams());
  if (!targetParams) return;
  const wasUnknown = state.performance.traceStateUnknown === true;
  try {
    const status = await refreshPerformanceTraceStatus({ silent: true, render: false, target: targetParams });
    if (!status) {
      addLog('warn', 'trace', 'Could not verify renderer trace status; capture was not sent.');
      return;
    }
    if (status.recording || status.completedRecordingAvailable || state.performance.traceActive || wasUnknown) {
      addLog('info', 'trace', 'Stopping active renderer trace.');
      await stopPerformanceTrace({ target: state.performance.activeTraceTarget || targetParams });
      return;
    }
    if (status.tracingSupported !== true) {
      addLog('warn', 'trace', 'This Valdi runtime does not support renderer trace capture.');
      return;
    }
    state.performance.traceCapturePending = true;
    markPerformanceTraceStateUnknown(targetParams);
    renderPerformance();
    addLog('info', 'trace', `Capturing renderer trace for ${(durationMs / 1000).toFixed(1)}s.`);
    const result = await apiPost(
      '/api/performance/trace/capture',
      targetParams,
      {
        rendererTracing: elements.rendererTracingToggle.checked,
        durationMs,
      },
      { timeoutMs: durationMs + 15000 },
    );
    syncActiveTraceState(result, targetParams);
    state.performance.traceSupported = result.tracingSupported === true;
    state.performance.lastTrace = result;
    addLog('info', 'trace', `Captured ${result.traceCount || 0} Valdi trace event(s).`);
  } catch (error) {
    addLog('error', 'trace', error.message);
    const status = await refreshPerformanceTraceStatus({ silent: true, render: false, target: targetParams });
    if (status) {
      await stopPerformanceTrace({ target: targetParams, render: false });
    } else {
      markPerformanceTraceStateUnknown(targetParams);
    }
  } finally {
    state.performance.traceCapturePending = false;
    if (
      !state.performance.traceActive &&
      !state.performance.traceResultPending &&
      !state.performance.traceStateUnknown
    ) {
      state.performance.activeTraceTarget = null;
    }
    renderPerformance();
  }
}

function exportPerformanceTrace() {
  const result = state.performance.lastTrace;
  if (!Array.isArray(result?.traces)) return;
  downloadJson(
    buildPerformanceTracePerfettoPayload(result),
    `${targetFilePrefix()}-valdi-trace.json`,
    'Exported Valdi trace JSON for Perfetto.',
  );
}

async function refreshProfileContexts(options) {
  try {
    const result = await apiGet('/api/performance/profile/contexts', {}, { timeoutMs: 5000 });
    state.performance.profileContexts = result.contexts || [];
    syncActiveProfileState(result.active);
    if (!options.silent) {
      addLog('info', 'profile', `Found ${state.performance.profileContexts.length} Hermes context(s).`);
    }
  } catch (error) {
    if (!options.silent) addLog('error', 'profile', error.message);
  } finally {
    renderPerformance();
  }
}

function selectedProfileContextId() {
  return elements.profileContextSelect.value || undefined;
}

async function startCpuProfile() {
  try {
    const result = await apiPost(
      '/api/performance/profile/start',
      {},
      {
        contextId: selectedProfileContextId(),
      },
    );
    syncActiveProfileState(result);
    addLog(
      'info',
      'profile',
      `Started CPU profile for ${result.contextTitle || result.contextId || 'Hermes context'}.`,
    );
  } catch (error) {
    addLog('error', 'profile', error.message);
  } finally {
    renderPerformance();
  }
}

async function stopCpuProfile() {
  try {
    const result = await apiPost('/api/performance/profile/stop', {}, {}, { timeoutMs: 65000 });
    syncActiveProfileState(null);
    state.performance.lastProfile = result;
    addLog('info', 'profile', `Captured CPU profile with ${result.summary?.sampleCount || 0} sample(s).`);
  } catch (error) {
    addLog('error', 'profile', error.message);
  } finally {
    renderPerformance();
  }
}

async function captureCpuProfile() {
  const durationMs = Math.round(readSecondsInput(elements.profileDurationInput, 60) * 1000);
  const contextId = selectedProfileContextId();
  syncActiveProfileState({ profiling: true, contextId });
  renderPerformance();
  try {
    addLog('info', 'profile', `Capturing CPU profile for ${(durationMs / 1000).toFixed(1)}s.`);
    const result = await apiPost(
      '/api/performance/profile/capture',
      {},
      {
        contextId,
        durationMs,
      },
      { timeoutMs: durationMs + 15000 },
    );
    syncActiveProfileState(null);
    state.performance.lastProfile = result;
    addLog('info', 'profile', `Captured CPU profile with ${result.summary?.sampleCount || 0} sample(s).`);
  } catch (error) {
    addLog('error', 'profile', error.message);
    await refreshProfileContexts({ silent: true });
  } finally {
    renderPerformance();
  }
}

function exportCpuProfile() {
  const result = state.performance.lastProfile;
  if (!result?.profile) return;
  downloadJson(result.profile, `${targetFilePrefix()}-hermes.cpuprofile`, 'Exported Hermes CPU profile.');
}
