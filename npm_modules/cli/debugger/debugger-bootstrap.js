// DOM event wiring and initial debugger boot sequence.
elements.screen.addEventListener('click', event => {
  void dispatchTapInput(event);
});

elements.screen.addEventListener('mousedown', event => {
  const overlayNode = event.target.closest('.overlay-node');
  if (overlayNode && elements.screen.contains(overlayNode)) {
    event.preventDefault();
  }
});

elements.previewStage.addEventListener('wheel', forwardPreviewWheelToPage, { passive: false });

document.addEventListener('click', event => {
  const toggle = event.target.closest('.tree-toggle');
  if (toggle && !toggle.classList.contains('empty')) {
    event.stopPropagation();
    toggleTreeNode(toggle.dataset.toggleNodeId);
    return;
  }

  const treeNode = event.target.closest('.tree-node');
  if (treeNode) void requestDebuggerAction('selectNode', { id: treeNode.dataset.nodeId });

  const tab = event.target.closest('[data-tab]');
  if (tab) void requestDebuggerAction('setActiveTab', { tab: tab.dataset.tab });

  const issue = event.target.closest('[data-issue-node]');
  if (issue && issue.dataset.issueNode) void requestDebuggerAction('selectNode', { id: issue.dataset.issueNode });

  const target = event.target.closest('.target');
  if (target) void requestDebuggerAction('selectTarget', { id: target.dataset.targetId });
});

document.addEventListener('keydown', event => {
  if (event.key !== 'Enter' && event.key !== ' ') return;
  const actionable = event.target.closest('.tree-node, .target, [data-issue-node]');
  if (!actionable) return;
  event.preventDefault();
  actionable.click();
});

function selectTarget(id) {
  const target = debuggerTargets().find(candidate => candidate.id === id);
  if (!target) return;
  if (target.daemonPort && target.port) {
    setTargetPort(target.port);
    state.snapshot.target = {
      ...state.snapshot.target,
      ...target,
    };
    addLog(
      'info',
      'daemon',
      `Waiting for a Valdi context on ${target.name} :${target.port}; Auto will attach when one appears.`,
    );
    render();
    refreshTargets({ autoAttach: true });
    return;
  }
  state.followLatestTarget = false;
  if (target.clientId && target.contextId) {
    loadRealSnapshot(target);
    return;
  }
  state.snapshot.target = {
    ...state.snapshot.target,
    id: target.id,
    name: target.name,
    platform: target.platform,
    state: 'attached',
  };
  state.snapshot.targets = state.snapshot.targets.map(candidate => ({
    ...candidate,
    state: candidate.id === id ? 'attached' : candidate.state === 'attached' ? 'available' : candidate.state,
  }));
  addLog('info', 'daemon', `Selected target ${target.name}.`);
  render();
}

elements.treeSearch.addEventListener('input', renderTree);
elements.logSearch.addEventListener('input', renderLogs);

elements.htmlPreviewRoot.addEventListener('click', event => {
  void dispatchHtmlPreviewTapInput(event);
});
elements.htmlPreviewRoot.addEventListener(
  'wheel',
  event => {
    void dispatchHtmlPreviewScrollInput(event);
  },
  { passive: false },
);
elements.htmlPreviewRoot.addEventListener('input', dispatchHtmlPreviewTextInput);
elements.htmlPreviewRoot.addEventListener('keydown', event => {
  void dispatchHtmlPreviewKeyInput(event);
});
elements.htmlPreviewRoot.addEventListener('focusin', event => {
  void dispatchHtmlPreviewFocusInput(event, true);
});
elements.htmlPreviewRoot.addEventListener('focusout', event => {
  void dispatchHtmlPreviewFocusInput(event, false);
});

document
  .getElementById('modeLive')
  .addEventListener('click', () => void requestDebuggerAction('setOverlayMode', { mode: 'live' }));
document
  .getElementById('modeIssues')
  .addEventListener('click', () => void requestDebuggerAction('setOverlayMode', { mode: 'issues' }));

function setOverlayMode(mode) {
  const normalizedMode = mode === 'views' || mode === 'components' ? 'live' : mode;
  if (!['live', 'issues'].includes(normalizedMode)) {
    addLog('warn', 'debugger', `Unknown preview mode: ${mode}.`);
    return;
  }
  state.overlayMode = normalizedMode;
  document.querySelectorAll('.segmented button').forEach(button => button.classList.remove('active'));
  document.getElementById(`mode${normalizedMode[0].toUpperCase()}${normalizedMode.slice(1)}`).classList.add('active');
  renderOverlay();
}

elements.closeExportButton.addEventListener('click', () => {
  elements.exportPanel.classList.remove('open');
});

elements.copyExportButton.addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText(elements.exportText.value);
    addLog('info', 'debugger', 'Copied JSON export to clipboard.');
  } catch (error) {
    elements.exportText.focus();
    elements.exportText.select();
    addLog('warn', 'debugger', `Clipboard write failed: ${error.message}`);
  }
});

document.getElementById('attachButton').addEventListener('click', () => {
  void requestDebuggerAction(isDebuggerAttached() ? 'detach' : 'attach');
});

document.getElementById('reloadButton').addEventListener('click', () => {
  void requestDebuggerAction('refreshSnapshot');
});

document.getElementById('refreshTargets').addEventListener('click', () => {
  void requestDebuggerAction('refreshTargets');
});

elements.portSelect.addEventListener('change', () => {
  const value = elements.portSelect.value;
  void requestDebuggerAction('setPort', { port: Number.parseInt(value, 10) });
});

elements.sectionButtons.forEach(button => {
  button.addEventListener('click', () => {
    void requestDebuggerAction('setActiveSection', { section: button.dataset.sectionButton });
  });
});

elements.snapshotButton.addEventListener('click', () => void requestDebuggerAction('captureElementSnapshot'));
elements.copyPreviewButton.addEventListener('click', () => void copyPreview());
elements.autoRefreshToggle.addEventListener(
  'change',
  () => void requestDebuggerAction('setAutoRefresh', { enabled: elements.autoRefreshToggle.checked }),
);
elements.profileRefreshButton.addEventListener('click', () => void requestDebuggerAction('refreshHermesContexts'));
elements.profileStartButton.addEventListener('click', () => void requestDebuggerAction('startCpuProfile'));
elements.profileStopButton.addEventListener('click', () => void requestDebuggerAction('stopCpuProfile'));
elements.profileCaptureButton.addEventListener('click', () => void requestDebuggerAction('captureCpuProfile'));
elements.profileExportButton.addEventListener('click', exportCpuProfile);

document.getElementById('copyPathButton').addEventListener('click', async () => {
  const path = getPathToNode(state.selectedNodeId)
    .map(node => `${node.tag}#${getNodeId(node)}`)
    .join(' > ');
  if (!path) {
    addLog('warn', 'inspector', 'No selected node path to copy.');
    return;
  }
  try {
    await navigator.clipboard.writeText(path);
    addLog('info', 'inspector', 'Copied selected node path.');
  } catch (error) {
    addLog('warn', 'inspector', `Copy failed: ${error.message}`);
  }
});

document.getElementById('clearLogsButton').addEventListener('click', () => {
  void requestDebuggerAction('clearLogs');
});

document.getElementById('commandForm').addEventListener('submit', event => {
  event.preventDefault();
  const input = document.getElementById('commandInput');
  runCommand(input.value);
  input.value = '';
});

window.addEventListener('resize', renderOverlay);

startDebuggerDevReload();
startDebuggerActionStream();
const restoredDebuggerSession = restoreDebuggerSessionState();
state.snapshot = decorateSnapshot(state.snapshot);
render();
applyDebuggerSessionDomState();
setAutoRefresh(state.autoRefresh, { silent: true });
installDebuggerSessionPersistence();
void refreshProfileContexts({ silent: true });
refreshTargets({ silent: restoredDebuggerSession, autoAttach: !state.manualDetach });
