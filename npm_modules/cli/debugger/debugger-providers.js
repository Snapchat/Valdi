// Generic debugger-provider discovery plus Storage and SQL inspector surfaces.
function debuggerTargetKey(target = getSelectedTargetParams()) {
  const port = Number(target?.port ?? target?.proxyPort);
  const clientId = target?.clientId;
  const contextId = target?.contextId;
  if (!Number.isInteger(port) || port < 1 || port > 65535 || !clientId || !contextId) return null;
  return JSON.stringify([port, String(clientId), String(contextId)]);
}

function debuggerTargetRequest() {
  const selected = getSelectedTargetParams();
  return {
    params: {
      port: Number(selected.port),
      clientId: String(selected.clientId || ''),
      contextId: String(selected.contextId || ''),
    },
    targetKey: debuggerTargetKey(selected),
  };
}

function beginDebuggerToolRequest(toolState) {
  const target = debuggerTargetRequest();
  toolState.generation += 1;
  return { ...target, generation: toolState.generation };
}

function debuggerToolRequestIsCurrent(toolState, request, result) {
  if (!request || request.generation !== toolState.generation) return false;
  if (!request.targetKey || request.targetKey !== debuggerTargetKey()) return false;
  const returnedTargetKey = debuggerTargetKey(result?.target);
  if (returnedTargetKey !== null && returnedTargetKey !== request.targetKey) return false;
  if (result?.handled === true && returnedTargetKey === null) return false;
  return true;
}

function beginDebuggerActionResult(toolState, params, result) {
  const requestedTargetKey = debuggerTargetKey(params);
  if (!requestedTargetKey || requestedTargetKey !== debuggerTargetKey()) return null;
  const returnedTargetKey = debuggerTargetKey(result?.target);
  if (returnedTargetKey !== null && returnedTargetKey !== requestedTargetKey) return null;
  if (result?.handled === true && returnedTargetKey === null) return null;
  toolState.generation += 1;
  return {
    generation: toolState.generation,
    params: {
      port: Number(params.port),
      clientId: String(params.clientId),
      contextId: String(params.contextId),
    },
    targetKey: requestedTargetKey,
  };
}

function providerRegistryData(result) {
  return result?.handled === true && result.status === 'handled' && result.data && typeof result.data === 'object'
    ? result.data
    : null;
}

function registeredDebuggerProviders() {
  return Array.isArray(state.providers.registry?.providers) ? state.providers.registry.providers : [];
}

function debuggerProvidersForKind(kind) {
  return registeredDebuggerProviders().filter(provider => provider.kind === kind);
}

function selectedDebuggerProviderId(kind) {
  if (kind === 'storage') return state.providers.selectedStorageProviderId;
  if (kind === 'sql') return state.providers.selectedSqlProviderId;
  return null;
}

function setSelectedDebuggerProviderId(kind, providerId) {
  if (kind === 'storage') state.providers.selectedStorageProviderId = providerId;
  if (kind === 'sql') state.providers.selectedSqlProviderId = providerId;
}

function debuggerProviderForKind(kind) {
  const providers = debuggerProvidersForKind(kind);
  const selectedId = selectedDebuggerProviderId(kind);
  const provider = providers.find(candidate => candidate.id === selectedId) || providers[0] || null;
  setSelectedDebuggerProviderId(kind, provider?.id || null);
  return provider;
}

function selectDebuggerProvider(kind, providerId) {
  const provider = debuggerProvidersForKind(kind).find(candidate => candidate.id === providerId);
  if (!provider) return false;
  setSelectedDebuggerProviderId(kind, provider.id);
  state.providers.error = null;
  if (kind === 'storage') {
    state.providers.storage = null;
  } else if (kind === 'sql') {
    state.providers.sql = null;
    state.providers.sqlTable = null;
    state.providers.selectedDatabaseId = null;
    state.providers.selectedTable = null;
    state.providers.sqlOffset = 0;
  }
  return true;
}

function renderDebuggerProviderSelector(kind) {
  const providers = debuggerProvidersForKind(kind);
  if (providers.length <= 1) return '';
  const selected = debuggerProviderForKind(kind);
  const selectId = kind === 'sql' ? 'sqlProviderSelect' : 'storageProviderSelect';
  return `<div class="sql-controls"><label>Provider<select id="${selectId}">${providers.map(provider => `<option value="${escapeHtml(provider.id)}"${provider.id === selected?.id ? ' selected' : ''}>${escapeHtml(provider.label || provider.id)}</option>`).join('')}</select></label></div>`;
}

function debuggerProviderUnavailableMessage(kind, label) {
  if (!hasSelectedLiveTarget()) return `Attach to a live Valdi target to inspect ${label}.`;
  if (state.providers.error) return state.providers.error;
  const provider = debuggerProviderForKind(kind);
  if (provider && provider.available === false) return provider.message || `${label} is unavailable in this target.`;
  return `${label} support is not registered by this target runtime.`;
}

async function requestDebuggerProvider(request, providerId, action, params = {}) {
  return apiPost(
    '/api/debugger/providers/request',
    request.params,
    { providerId, action, params },
    { timeoutMs: 12000 },
  );
}

function debuggerProviderResponseData(result) {
  const envelope = providerRegistryData(result);
  if (!envelope) return null;
  if (envelope.stale === true) return null;
  if (envelope.unavailable === true || envelope.busy === true) {
    state.providers.error = envelope.message || 'The debugger provider is unavailable.';
    return null;
  }
  state.providers.error = null;
  return envelope.data && typeof envelope.data === 'object' ? envelope.data : null;
}

async function loadStorageProvider(options = {}, request = beginDebuggerToolRequest(state.providers)) {
  const provider = debuggerProviderForKind('storage');
  if (!provider || provider.available === false) {
    if (debuggerToolRequestIsCurrent(state.providers, request)) {
      state.providers.storage = null;
      renderDebuggerProviders();
    }
    return;
  }
  try {
    const result = await requestDebuggerProvider(request, provider.id, 'snapshot');
    if (!debuggerToolRequestIsCurrent(state.providers, request, result)) return;
    state.providers.storage = debuggerProviderResponseData(result);
    if (!result.handled) state.providers.error = result.message || 'Storage provider request was not handled.';
  } catch (error) {
    if (!debuggerToolRequestIsCurrent(state.providers, request)) return;
    state.providers.storage = null;
    state.providers.error = error.message;
    if (!options.silent) addLog('error', 'storage', error.message);
  }
  if (debuggerToolRequestIsCurrent(state.providers, request)) renderDebuggerProviders();
}

async function loadSqlDatabases(options = {}, request = beginDebuggerToolRequest(state.providers)) {
  const provider = debuggerProviderForKind('sql');
  if (!provider || provider.available === false) {
    if (debuggerToolRequestIsCurrent(state.providers, request)) {
      state.providers.sql = null;
      state.providers.sqlTable = null;
      renderDebuggerProviders();
    }
    return;
  }
  try {
    const result = await requestDebuggerProvider(request, provider.id, 'list');
    if (!debuggerToolRequestIsCurrent(state.providers, request, result)) return;
    state.providers.sql = debuggerProviderResponseData(result);
    const databases = Array.isArray(state.providers.sql?.databases) ? state.providers.sql.databases : [];
    const selectedDatabase = databases.find(database => database.id === state.providers.selectedDatabaseId) || databases[0];
    state.providers.selectedDatabaseId = selectedDatabase?.id || null;
    const tables = Array.isArray(selectedDatabase?.tables) ? selectedDatabase.tables : [];
    const selectedTable = tables.find(table => table.name === state.providers.selectedTable) || tables[0];
    state.providers.selectedTable = selectedTable?.name || null;
    state.providers.sqlOffset = 0;
    state.providers.sqlTable = null;
    if (state.providers.selectedDatabaseId && state.providers.selectedTable) {
      await loadSqlTable({ silent: true }, request);
    }
  } catch (error) {
    if (!debuggerToolRequestIsCurrent(state.providers, request)) return;
    state.providers.sql = null;
    state.providers.sqlTable = null;
    state.providers.error = error.message;
    if (!options.silent) addLog('error', 'sql', error.message);
  }
  if (debuggerToolRequestIsCurrent(state.providers, request)) renderDebuggerProviders();
}

async function loadSqlTable(options = {}, request = beginDebuggerToolRequest(state.providers)) {
  const provider = debuggerProviderForKind('sql');
  if (!provider || !state.providers.selectedDatabaseId || !state.providers.selectedTable) return;
  const databaseId = state.providers.selectedDatabaseId;
  const table = state.providers.selectedTable;
  const offset = state.providers.sqlOffset;
  state.providers.sqlLoading = true;
  renderDebuggerProviders();
  try {
    const result = await requestDebuggerProvider(request, provider.id, 'table', {
      databaseId,
      table,
      limit: state.providers.sqlLimit,
      offset,
    });
    if (!debuggerToolRequestIsCurrent(state.providers, request, result)) return;
    if (
      databaseId !== state.providers.selectedDatabaseId ||
      table !== state.providers.selectedTable ||
      offset !== state.providers.sqlOffset
    ) return;
    state.providers.sqlTable = debuggerProviderResponseData(result);
    if (!result.handled) state.providers.error = result.message || 'SQL table request was not handled.';
  } catch (error) {
    if (!debuggerToolRequestIsCurrent(state.providers, request)) return;
    state.providers.sqlTable = null;
    state.providers.error = error.message;
    if (!options.silent) addLog('error', 'sql', error.message);
  } finally {
    if (debuggerToolRequestIsCurrent(state.providers, request)) {
      state.providers.sqlLoading = false;
      renderDebuggerProviders();
    }
  }
}

async function hydrateDebuggerProviders(options = {}, request = beginDebuggerToolRequest(state.providers)) {
  if (state.providers.activeTab === 'storage') await loadStorageProvider(options, request);
  else await loadSqlDatabases(options, request);
}

async function refreshDebuggerProviders(options = {}) {
  const request = beginDebuggerToolRequest(state.providers);
  state.providers.targetKey = request.targetKey;
  state.providers.loading = true;
  state.providers.error = null;
  state.providers.registry = null;
  state.providers.storage = null;
  state.providers.sql = null;
  state.providers.sqlTable = null;
  renderDebuggerProviders();
  if (!request.targetKey || !hasSelectedLiveTarget()) {
    if (debuggerToolRequestIsCurrent(state.providers, request)) {
      state.providers.loading = false;
      state.providers.error = 'Attach to a live Valdi target before checking debugger providers.';
      renderDebuggerProviders();
    }
    return;
  }
  try {
    const result = await apiGet('/api/debugger/providers', request.params, { timeoutMs: 7000 });
    if (!debuggerToolRequestIsCurrent(state.providers, request, result)) return;
    state.providers.registry = providerRegistryData(result);
    if (!result.handled) state.providers.error = result.message || 'The target does not expose debugger providers.';
    if (!options.silent) addLog(result.handled ? 'info' : 'warn', 'data', result.message || 'Refreshed debugger providers.');
  } catch (error) {
    if (!debuggerToolRequestIsCurrent(state.providers, request)) return;
    state.providers.error = error.message;
    if (!options.silent) addLog('error', 'data', error.message);
  } finally {
    if (debuggerToolRequestIsCurrent(state.providers, request)) state.providers.loading = false;
  }
  if (!debuggerToolRequestIsCurrent(state.providers, request)) return;
  await hydrateDebuggerProviders(options, request);
  if (debuggerToolRequestIsCurrent(state.providers, request)) renderDebuggerProviders();
}

function applyDebuggerProvidersResult(result, params) {
  const request = beginDebuggerActionResult(state.providers, params, result);
  if (!request) return false;
  state.providers.registry = providerRegistryData(result);
  state.providers.error = result?.handled ? null : result?.message || 'The target does not expose debugger providers.';
  state.providers.targetKey = request.targetKey;
  void hydrateDebuggerProviders({ silent: true }, request);
  renderDebuggerProviders();
  return true;
}

function renderProviderStatus(provider, unavailableMessage) {
  const available = provider?.available === true;
  return `<div class="provider-status ${available ? 'available' : ''}"><span class="dot ${available ? 'good' : 'warn'}"></span><span>${escapeHtml(available ? provider.message || 'Available' : unavailableMessage)}</span></div>`;
}

function renderStorageEntry(entry) {
  const truncated = entry.valueTruncated || entry.keyTruncated;
  const metadata = [entry.encoding ?? 'unknown'];
  if (entry.byteLength !== undefined) metadata.push(`${entry.byteLength} bytes`);
  if (truncated) metadata.push('truncated');
  return `<details class="data-entry"><summary><code>${escapeHtml(entry.key || '')}</code><span>${escapeHtml(metadata.join(' · '))}</span></summary><pre class="codebox">${escapeHtml(entry.value ?? '')}</pre></details>`;
}

function renderStorageProjectionNote(storage) {
  const projection = storage?.projection;
  if (projection?.truncated !== true) return '';
  const details = [];
  const storesOmitted = Number(projection.storesOmitted);
  const entriesOmitted = Number(projection.entriesOmitted);
  const truncatedFields = Number(projection.truncatedFields);
  const invalidFields = Number(projection.invalidFields);
  if (Number.isInteger(storesOmitted) && storesOmitted > 0) details.push(`${storesOmitted} ${storesOmitted === 1 ? 'store' : 'stores'} omitted`);
  if (Number.isInteger(entriesOmitted) && entriesOmitted > 0) details.push(`${entriesOmitted} ${entriesOmitted === 1 ? 'entry' : 'entries'} omitted`);
  if (projection.sourceEntryCountIncomplete === true) details.push('source entry total incomplete');
  if (Number.isInteger(truncatedFields) && truncatedFields > 0) details.push(`${truncatedFields} ${truncatedFields === 1 ? 'field' : 'fields'} truncated`);
  if (Number.isInteger(invalidFields) && invalidFields > 0) details.push(`${invalidFields} invalid ${invalidFields === 1 ? 'field' : 'fields'} omitted`);
  const detail = details.length ? `: ${details.join(', ')}` : '';
  return `<div class="provider-note">The Storage transport projection was truncated${escapeHtml(detail)}.</div>`;
}

function renderStoragePanel() {
  const provider = debuggerProviderForKind('storage');
  const selector = renderDebuggerProviderSelector('storage');
  const unavailable = debuggerProviderUnavailableMessage('storage', 'Storage');
  if (!provider || provider.available !== true || !state.providers.storage) {
    return `${selector}${renderProviderStatus(provider, unavailable)}<div class="empty">${escapeHtml(unavailable)}</div>`;
  }
  const stores = Array.isArray(state.providers.storage.stores) ? state.providers.storage.stores : [];
  const storageIssues = `${state.providers.storage.storageError ? `<div class="issue warn"><div class="issue-message">${escapeHtml(state.providers.storage.storageError)}</div></div>` : ''}${state.providers.storage.storageInspectionTruncated ? '<div class="provider-note">Persistent browser-storage discovery was truncated by the inspection budget.</div>' : ''}${renderStorageProjectionNote(state.providers.storage)}`;
  if (!stores.length) return `${selector}${renderProviderStatus(provider, unavailable)}${storageIssues}<div class="empty">The registered Storage provider returned no stores.</div>`;
  return `${selector}${renderProviderStatus(provider, unavailable)}${storageIssues}<div class="storage-grid">${stores.map(store => {
    const entries = Array.isArray(store.entries) ? store.entries : [];
    return `<details class="storage-card" open><summary><strong>${escapeHtml(store.name || 'Storage')}</strong><span>${escapeHtml(store.backend ?? 'unknown')} · ${entries.length} ${entries.length === 1 ? 'entry' : 'entries'}</span></summary>${store.error ? `<div class="issue warn"><div class="issue-message">${escapeHtml(store.error)}</div></div>` : ''}${entries.length ? entries.map(renderStorageEntry).join('') : '<div class="empty">This store is empty.</div>'}${store.entriesTruncated ? '<div class="provider-note">Additional entries were omitted by the debugger snapshot budget.</div>' : ''}${store.inspectionTruncated ? '<div class="provider-note">Entry inspection stopped at the debugger scan limit.</div>' : ''}</details>`;
  }).join('')}</div>`;
}

function selectedSQLDatabase() {
  const databases = Array.isArray(state.providers.sql?.databases) ? state.providers.sql.databases : [];
  return databases.find(database => database.id === state.providers.selectedDatabaseId) || null;
}

function renderSQLTable() {
  const data = state.providers.sqlTable;
  if (state.providers.sqlLoading) return '<div class="empty">Loading SQL rows…</div>';
  if (!data) return '<div class="empty">Select a database table to inspect its rows.</div>';
  const columns = Array.isArray(data.columns) ? data.columns : [];
  const rows = Array.isArray(data.rows) ? data.rows : [];
  const names = columns.map(column => column.name).filter(Boolean);
  const headings = names.map(name => `<th>${escapeHtml(name)}</th>`).join('');
  const body = rows.map(row => `<tr>${names.map(name => `<td>${escapeHtml(row?.[name] ?? 'NULL')}</td>`).join('')}</tr>`).join('');
  const rowCount = Number(data.rowCount);
  const hasNext = rows.length >= state.providers.sqlLimit && (!Number.isFinite(rowCount) || state.providers.sqlOffset + rows.length < rowCount);
  return `<div class="sql-table-wrap"><table class="sql-table"><thead><tr>${headings}</tr></thead><tbody>${body}</tbody></table></div>${rows.length ? '' : '<div class="empty">This table returned no rows.</div>'}<div class="sql-pagination"><button id="sqlPreviousButton" ${state.providers.sqlOffset <= 0 ? 'disabled' : ''}>Previous</button><span>Offset ${state.providers.sqlOffset}</span><button id="sqlNextButton" ${hasNext ? '' : 'disabled'}>Next</button></div>`;
}

function renderSqlPanel() {
  const provider = debuggerProviderForKind('sql');
  const selector = renderDebuggerProviderSelector('sql');
  const unavailable = debuggerProviderUnavailableMessage('sql', 'SQL');
  if (!provider || provider.available !== true || !state.providers.sql) {
    return `${selector}${renderProviderStatus(provider, unavailable)}<div class="empty">${escapeHtml(unavailable)}</div>`;
  }
  const databases = Array.isArray(state.providers.sql.databases) ? state.providers.sql.databases : [];
  if (!databases.length) return `${selector}${renderProviderStatus(provider, unavailable)}<div class="empty">The registered SQL provider returned no databases.</div>`;
  const database = selectedSQLDatabase() || databases[0];
  const tables = Array.isArray(database?.tables) ? database.tables : [];
  return `${selector}${renderProviderStatus(provider, unavailable)}<div class="sql-controls"><label>Database<select id="sqlDatabaseSelect">${databases.map(item => `<option value="${escapeHtml(item.id)}"${item.id === state.providers.selectedDatabaseId ? ' selected' : ''}>${escapeHtml(item.name || item.id)}</option>`).join('')}</select></label><label>Table<select id="sqlTableSelect">${tables.map(table => `<option value="${escapeHtml(table.name)}"${table.name === state.providers.selectedTable ? ' selected' : ''}>${escapeHtml(table.name)}</option>`).join('')}</select></label></div>${tables.length ? renderSQLTable() : '<div class="empty">This database does not expose inspectable tables.</div>'}`;
}

function renderDebuggerProviders() {
  if (!elements.providerContent || !elements.providerStatusPill) return;
  elements.providerRefreshButton.disabled = state.providers.loading;
  elements.providerStatusPill.textContent = state.providers.loading ? 'Loading' : state.providers.error ? 'Unavailable' : state.providers.registry ? 'Ready' : 'Idle';
  elements.providerStatusPill.className = `source-pill ${state.providers.registry && !state.providers.error ? 'live' : ''}`;
  document.querySelectorAll('[data-provider-tab]').forEach(button => button.classList.toggle('active', button.dataset.providerTab === state.providers.activeTab));
  if (state.providers.loading && !state.providers.registry) {
    elements.providerContent.innerHTML = '<div class="empty">Discovering target debugger providers…</div>';
    return;
  }
  const metadataNote = state.providers.registry?.metadataTruncated === true
    ? `<div class="provider-note">Optional provider metadata was omitted to keep discovery within the debugger response budget${Number.isInteger(state.providers.registry.omittedMetadataFields) ? ` (${state.providers.registry.omittedMetadataFields} fields)` : ''}.</div>`
    : '';
  const panel = state.providers.activeTab === 'storage' ? renderStoragePanel() : renderSqlPanel();
  elements.providerContent.innerHTML = metadataNote + panel;
}

function setActiveProviderTab(tab) {
  state.providers.activeTab = tab === 'sql' ? 'sql' : 'storage';
  renderDebuggerProviders();
  if (!state.providers.registry || state.providers.targetKey !== debuggerTargetKey()) void refreshDebuggerProviders({ silent: true });
  else void hydrateDebuggerProviders({ silent: true });
}
