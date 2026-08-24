// Hermes CPU profile UI behavior.
function readSecondsInput(input) {
  const value = Number.parseFloat(input.value);
  if (!Number.isFinite(value)) return 5;
  return Math.max(0.1, Math.min(60, value));
}

function formatMs(value) {
  if (!Number.isFinite(value)) return 'n/a';
  return `${value.toFixed(value >= 100 ? 0 : 1)} ms`;
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

function syncActiveProfileState(activeProfile) {
  const contextId = activeProfile?.contextId;
  state.performance.profileActive = Boolean(activeProfile?.profiling);
  state.performance.activeProfileContextId =
    state.performance.profileActive && contextId !== undefined && contextId !== null ? String(contextId) : null;
}

function renderPerformance() {
  const perf = state.performance;
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
  const durationMs = Math.round(readSecondsInput(elements.profileDurationInput) * 1000);
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
