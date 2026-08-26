// Application-published debug settings UI.
function debugSettingsSnapshotFromResult(result) {
  return result?.handled === true && result.status === 'handled' && result.data && typeof result.data === 'object'
    ? result.data
    : null;
}

function acceptDebugSettingsResult(result, request) {
  if (!debuggerToolRequestIsCurrent(state.settings, request, result)) return false;
  const snapshot = debugSettingsSnapshotFromResult(result);
  state.settings.snapshot = snapshot;
  state.settings.error = snapshot ? null : result?.message || 'The target does not publish debug settings.';
  state.settings.targetKey = request.targetKey;
  const groups = Array.isArray(snapshot?.groups) ? snapshot.groups : [];
  if (!groups.some(group => group.id === state.settings.selectedGroupId)) {
    state.settings.selectedGroupId = groups[0]?.id || null;
  }
  renderDebugSettings();
  return true;
}

function applyDebugSettingsResult(result, params) {
  const request = beginDebuggerActionResult(state.settings, params, result);
  if (!request) return false;
  return acceptDebugSettingsResult(result, request);
}

async function refreshDebugSettings(options = {}) {
  const request = beginDebuggerToolRequest(state.settings);
  state.settings.loading = true;
  state.settings.error = null;
  state.settings.targetKey = request.targetKey;
  renderDebugSettings();
  if (!request.targetKey || !hasSelectedLiveTarget()) {
    if (debuggerToolRequestIsCurrent(state.settings, request)) {
      state.settings.loading = false;
      state.settings.snapshot = null;
      state.settings.error = 'Attach to a live Valdi target to inspect application settings.';
      renderDebugSettings();
    }
    return;
  }
  try {
    const result = await apiGet('/api/debugger/settings', request.params, { timeoutMs: 7000 });
    if (!acceptDebugSettingsResult(result, request)) return;
    if (!options.silent) addLog(result.handled ? 'info' : 'warn', 'settings', result.message || 'Refreshed settings.');
  } catch (error) {
    if (!debuggerToolRequestIsCurrent(state.settings, request)) return;
    state.settings.snapshot = null;
    state.settings.error = error.message;
    if (!options.silent) addLog('error', 'settings', error.message);
  } finally {
    if (debuggerToolRequestIsCurrent(state.settings, request)) {
      state.settings.loading = false;
      renderDebugSettings();
    }
  }
}

function selectedDebugSettingsGroup() {
  const groups = Array.isArray(state.settings.snapshot?.groups) ? state.settings.snapshot.groups : [];
  return groups.find(group => group.id === state.settings.selectedGroupId) || null;
}

function selectedDebugSetting(settingId) {
  const group = selectedDebugSettingsGroup();
  return Array.isArray(group?.settings) ? group.settings.find(setting => setting.id === settingId) || null : null;
}

function debugSettingOptionIndex(setting, value) {
  return Array.isArray(setting.options) ? setting.options.findIndex(option => option.value === value) : -1;
}

function isDebugSettingPrimitive(value) {
  return typeof value === 'boolean' || typeof value === 'string' || (typeof value === 'number' && Number.isFinite(value));
}

function unsupportedDebugSettingControl(message) {
  return `<span class="provider-note">Unsupported ${escapeHtml(message)}</span>`;
}

function renderDebugSettingControl(setting) {
  const attributes = `data-setting-id="${escapeHtml(setting.id)}" data-setting-kind="${escapeHtml(setting.kind)}"`;
  if (setting.kind === 'toggle') {
    if (typeof setting.value !== 'boolean') return unsupportedDebugSettingControl('toggle value');
    return `<input ${attributes} type="checkbox" aria-label="${escapeHtml(setting.label)}"${setting.value === true ? ' checked' : ''} />`;
  }
  if (setting.kind === 'select') {
    if (
      !Array.isArray(setting.options) ||
      setting.options.length === 0 ||
      setting.options.some(option => typeof option?.label !== 'string' || !isDebugSettingPrimitive(option?.value))
    ) {
      return unsupportedDebugSettingControl('select declaration');
    }
    const selectedIndex = debugSettingOptionIndex(setting, setting.value);
    if (selectedIndex < 0) return unsupportedDebugSettingControl('select value');
    const options = setting.options
      .map((option, index) => `<option value="${index.toString()}"${index === selectedIndex ? ' selected' : ''}>${escapeHtml(option.label)}</option>`)
      .join('');
    return `<select ${attributes} aria-label="${escapeHtml(setting.label)}">${options}</select>`;
  }
  if (setting.kind === 'text') {
    if (typeof setting.value !== 'string') return unsupportedDebugSettingControl('text value');
    return `<input ${attributes} type="text" aria-label="${escapeHtml(setting.label)}" value="${escapeHtml(setting.value)}" />`;
  }
  if (setting.kind === 'number') {
    if (typeof setting.value !== 'number' || !Number.isFinite(setting.value)) {
      return unsupportedDebugSettingControl('number value');
    }
    return `<input ${attributes} type="number" aria-label="${escapeHtml(setting.label)}" value="${escapeHtml(setting.value)}" />`;
  }
  return `<span class="provider-note">Unsupported setting kind: ${escapeHtml(setting.kind || 'unknown')}</span>`;
}

function renderDebugSettings() {
  if (!elements.settingsContent || !elements.settingsStatusPill) return;
  const snapshot = state.settings.snapshot;
  const groups = Array.isArray(snapshot?.groups) ? snapshot.groups : [];
  elements.settingsRefreshButton.disabled = state.settings.loading;
  elements.settingsStatusPill.textContent = state.settings.loading ? 'Loading' : state.settings.error ? 'Unavailable' : snapshot ? 'Ready' : 'Idle';
  elements.settingsStatusPill.className = `source-pill ${snapshot && !state.settings.error ? 'live' : ''}`;
  elements.settingsGroupSelect.innerHTML = groups
    .map(group => `<option value="${escapeHtml(group.id)}"${group.id === state.settings.selectedGroupId ? ' selected' : ''}>${escapeHtml(group.label)}</option>`)
    .join('');
  elements.settingsGroupSelect.disabled = state.settings.loading || groups.length === 0;

  if (state.settings.loading && !snapshot) {
    elements.settingsContent.innerHTML = '<div class="empty">Loading application-published settings…</div>';
    return;
  }
  if (state.settings.error) {
    elements.settingsContent.innerHTML = `<div class="empty">${escapeHtml(state.settings.error)}</div>`;
    return;
  }
  const group = selectedDebugSettingsGroup();
  if (!group) {
    elements.settingsContent.innerHTML = '<div class="empty">The selected application has not published any debug settings.</div>';
    return;
  }
  elements.settingsContent.innerHTML = (group.settings || [])
    .map(setting => {
      const changed = setting.value !== setting.defaultValue;
      return `<div class="settings-row"><div><div class="settings-label">${escapeHtml(setting.label)}</div>${setting.description ? `<div class="provider-note">${escapeHtml(setting.description)}</div>` : ''}</div><div class="settings-control">${renderDebugSettingControl(setting)}<button data-reset-setting="${escapeHtml(setting.id)}"${changed ? '' : ' disabled'}>Reset</button></div></div>`;
    })
    .join('');
}

function readDebugSettingInput(element) {
  const setting = selectedDebugSetting(element.dataset.settingId);
  if (!setting || setting.kind !== element.dataset.settingKind) return { error: 'The setting changed; refresh and try again.' };
  if (setting.kind === 'toggle') return { value: element.checked === true };
  if (setting.kind === 'select') {
    const index = Number(element.value);
    if (!Array.isArray(setting.options) || !Number.isInteger(index) || index < 0 || index >= setting.options.length) {
      return { error: 'Select a declared option.' };
    }
    const value = setting.options[index]?.value;
    return isDebugSettingPrimitive(value) ? { value } : { error: 'Select a declared primitive option.' };
  }
  if (setting.kind === 'text') return { value: element.value };
  if (setting.kind === 'number') {
    if (String(element.value).trim() === '') return { error: 'Enter a finite number.' };
    const value = Number(element.value);
    return Number.isFinite(value) ? { value } : { error: 'Enter a finite number.' };
  }
  return { error: `Unsupported setting kind: ${String(setting.kind)}` };
}

async function changeDebugSetting(action, settingId, value) {
  if (!state.settings.selectedGroupId) return;
  await requestDebuggerAction(action === 'set' ? 'setDebugSetting' : 'resetDebugSetting', {
    ...getSelectedTargetParams(),
    groupId: state.settings.selectedGroupId,
    settingId,
    ...(action === 'set' ? { value } : {}),
  });
}

function maybeRefreshDebuggerTools(section) {
  if (section === 'data' && (!state.providers.registry || state.providers.targetKey !== debuggerTargetKey())) {
    void refreshDebuggerProviders({ silent: true });
  }
  if (section === 'settings' && (!state.settings.snapshot || state.settings.targetKey !== debuggerTargetKey())) {
    void refreshDebugSettings({ silent: true });
  }
}

function resetDebuggerToolsForTarget() {
  state.providers.generation += 1;
  state.providers.error = null;
  state.providers.loading = false;
  state.providers.registry = null;
  state.providers.storage = null;
  state.providers.sql = null;
  state.providers.sqlLoading = false;
  state.providers.sqlTable = null;
  state.providers.selectedDatabaseId = null;
  state.providers.selectedTable = null;
  state.providers.sqlOffset = 0;
  state.providers.targetKey = null;
  state.settings.generation += 1;
  state.settings.error = null;
  state.settings.loading = false;
  state.settings.snapshot = null;
  state.settings.targetKey = null;
}
