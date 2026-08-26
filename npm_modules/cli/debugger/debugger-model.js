// Target selection, tree decoration, geometry, search, and analysis helpers.
function lastItem(items) {
  return items[items.length - 1];
}

function matchingTargetForCurrentSelection(targets) {
  const current = state.snapshot.target || {};
  if (!current.id && !current.clientId && !current.contextId) return null;

  return (
    targets.find(target => target.id === current.id) ||
    targets.find(target => target.clientId === current.clientId && target.contextId === current.contextId) ||
    targets.find(
      target =>
        target.clientId === current.clientId &&
        target.applicationId === current.applicationId &&
        target.name === current.name,
    ) ||
    null
  );
}

function chooseLiveTarget(targets) {
  const selectedPort = selectedDaemonPort(null);
  const portTargets = selectedPort === null ? [] : targets.filter(target => target.port === selectedPort);
  const candidates = portTargets.length ? portTargets : targets;
  if (!state.followLatestTarget) {
    return (
      matchingTargetForCurrentSelection(candidates) ||
      matchingTargetForCurrentSelection(targets) ||
      lastItem(candidates)
    );
  }

  const current = state.snapshot.target || {};
  const appTargets = candidates.filter(
    target =>
      current.clientId && target.clientId === current.clientId && target.applicationId === current.applicationId,
  );
  return lastItem(appTargets.length ? appTargets : candidates);
}

function markSelectedTarget(targets, selectedTarget) {
  return targets.map(target => ({
    ...target,
    state: target.id === selectedTarget.id ? 'attached' : 'available',
  }));
}

function decorateSnapshot(snapshot) {
  snapshot.tree = valdiDebuggerTreeModel.restoreTree(snapshot.tree);
  if (!snapshot.tree) {
    snapshot.target = snapshot.target || { ...emptyTarget };
    snapshot.targets = snapshot.targets || [];
    snapshot.issues = snapshot.issues || [];
    snapshot.logs = snapshot.logs || [];
    state.geometry = null;
    return snapshot;
  }
  decorateNode(snapshot.tree);
  if (!hasAnyRenderableBounds(snapshot.tree)) {
    ensureBounds(snapshot.tree);
  }
  snapshot.issues = mergeIssues(snapshot.issues || [], analyzeTree(snapshot.tree));
  state.geometry = computeGeometry(snapshot.tree);
  return snapshot;
}

function decorateNode(node, path = '0') {
  const records = [];
  const childrenByNode = new Map();
  const paths = new Map();
  valdiDebuggerTreeModel.walk(
    node,
    (current, ancestors, _depth, sourceChildIndex) => {
      const parent = ancestors.at(-1);
      const parentPath = parent ? paths.get(parent) : null;
      const childIndex = parent ? Math.max(0, sourceChildIndex ?? 0) : 0;
      const currentPath = parentPath === null || parentPath === undefined ? path : `${parentPath}.${childIndex}`;
      paths.set(current, currentPath);
      if (parent) {
        const children = childrenByNode.get(parent) || [];
        children.push(current);
        childrenByNode.set(parent, children);
      }
      childrenByNode.set(current, childrenByNode.get(current) || []);

      if (current.element && current.element.frame) {
        current.bounds = normalizeBounds(current.element.frame);
      }
      if (!current.id) {
        if (current.element && current.element.id !== undefined) {
          current.id = String(current.element.id);
        } else if (current.key !== undefined) {
          current.id = `${current.tag}:${current.key}:${currentPath}`;
        } else {
          current.id = `${current.tag}:${currentPath}`;
        }
      }
      records.push(current);
    },
    [],
    0,
  );
  for (let index = records.length - 1; index >= 0; index -= 1) {
    const current = records[index];
    if (!current.bounds) {
      const childBounds = (childrenByNode.get(current) || []).map(child => child.bounds).filter(Boolean);
      if (childBounds.length) current.bounds = unionBounds(childBounds);
    }
  }
}

function normalizeBounds(bounds) {
  return {
    x: Number.parseFloat(bounds.x) || 0,
    y: Number.parseFloat(bounds.y) || 0,
    width: Math.max(0, Number.parseFloat(bounds.width) || 0),
    height: Math.max(0, Number.parseFloat(bounds.height) || 0),
  };
}

function getNodeId(node) {
  return valdiDebuggerTreeModel.id(node);
}

function getNodeKind(node) {
  return node.component ? 'component' : 'element';
}

function normalizeLabelValue(value) {
  if (value === undefined || value === null || value === '') return '';
  return valdiDebuggerTreeModel.formatValue(value, 0).replace(/^"|"$/g, '').trim();
}

function isReadableLabelToken(value) {
  if (!value) return false;
  if (/^-?\d+(\.\d+)?$/.test(value)) return false;
  if (/^#[0-9a-f]{3,8}$/i.test(value)) return false;
  if (/^rgba?\(/i.test(value)) return false;
  if (/^system(-[a-z]+)?\s+\d+$/i.test(value)) return false;
  if (/^(prose|caption|heading|body|title|subtitle|primary|secondary|emphasis)$/i.test(value)) return false;
  return true;
}

function firstReadableValue(sources, keys) {
  for (const source of sources) {
    if (!source || typeof source !== 'object') continue;
    for (const key of keys) {
      const value = normalizeLabelValue(source[key]);
      if (value) return value;
    }
  }
  return '';
}

function truncateLabel(value, maxLength = 58) {
  return value.length > maxLength ? `${value.slice(0, maxLength - 1)}...` : value;
}

function overlayNodeLabel(node) {
  return `${node.tag} #${getNodeId(node)}`;
}

function overlayNodeDetail(node) {
  const attrs = getNodeAttributes(node);
  const text = firstReadableValue(
    [attrs, node.viewModel, node],
    [
      'accessibilityLabel',
      'accessibilityValue',
      'accessibilityHint',
      'accessible',
      'ariaLabel',
      'label',
      'value',
      'title',
      'text',
      'placeholder',
      'name',
      'accessibilityId',
      'id',
    ],
  );
  const style = firstReadableValue(
    [attrs, node],
    ['style', 'font', 'color', 'backgroundColor', 'iosClass', 'androidClass', 'macosClass', 'webClass'],
  );
  return truncateLabel(text || style || '', 72);
}

function describeOverlayNode(node) {
  const label = overlayNodeLabel(node);
  const detail = overlayNodeDetail(node);
  return detail ? `${label}\n${detail}` : label;
}

function getNodeAttributes(node) {
  return valdiDebuggerTreeModel.attributes(node);
}

function getNumericAttribute(node, name) {
  const value = getNodeAttributes(node)[name];
  if (value === undefined || value === null) return 0;
  const parsed = Number.parseFloat(String(value).replace(/^"|"$/g, ''));
  return Number.isFinite(parsed) ? parsed : 0;
}

function getScrollOffset(node) {
  return {
    x: getNumericAttribute(node, 'contentOffsetX'),
    y: getNumericAttribute(node, 'contentOffsetY'),
  };
}

function getTranslation(node) {
  return {
    x: getNumericAttribute(node, 'translationX'),
    y: getNumericAttribute(node, 'translationY'),
  };
}

function hasScrollState(node) {
  const attrs = getNodeAttributes(node);
  return attrs.contentOffsetX !== undefined || attrs.contentOffsetY !== undefined;
}

function hasAnyRenderableBounds(root) {
  let found = false;
  walk(root, node => {
    if (!node.bounds && !(node.element && node.element.frame)) return true;
    found = true;
    return false;
  });
  return found;
}

function hasLocalFrames(root) {
  let found = false;
  walk(root, node => {
    if (!node.element || !node.element.frame) return true;
    found = true;
    return false;
  });
  return found;
}

function unionBounds(boundsList) {
  const bounds = boundsList.filter(Boolean);
  if (!bounds.length) return null;
  const minX = Math.min(...bounds.map(item => item.x));
  const minY = Math.min(...bounds.map(item => item.y));
  const maxX = Math.max(...bounds.map(item => item.x + item.width));
  const maxY = Math.max(...bounds.map(item => item.y + item.height));
  return { x: minX, y: minY, width: maxX - minX, height: maxY - minY };
}

function firstElementWithFrame(root) {
  let result = null;
  walk(root, node => {
    if (node.element && node.element.frame) {
      const bounds = normalizeBounds(node.element.frame);
      if (bounds.width > 0 && bounds.height > 0) {
        result = node;
        return false;
      }
    }
    return true;
  });
  return result;
}

function computeGeometry(root) {
  const localFrames = hasLocalFrames(root);
  const map = new Map();
  const absoluteByNode = new Map();
  const records = [];
  const childrenByNode = new Map();
  const childOffsets = new Map();
  valdiDebuggerTreeModel.walk(
    root,
    (node, ancestors) => {
      const parent = ancestors.at(-1);
      const offset = parent ? childOffsets.get(parent) || { x: 0, y: 0 } : { x: 0, y: 0 };
      if (parent) {
        const children = childrenByNode.get(parent) || [];
        children.push(node);
        childrenByNode.set(parent, children);
      }
      childrenByNode.set(node, childrenByNode.get(node) || []);
      const frame = node.element && node.element.frame ? normalizeBounds(node.element.frame) : null;
      const absoluteCandidate = !localFrames && node.bounds ? normalizeBounds(node.bounds) : null;
      let absolute = null;
      let childOffset = offset;
      if (frame) {
        absolute = {
          x: offset.x + frame.x,
          y: offset.y + frame.y,
          width: frame.width,
          height: frame.height,
        };
        const scrollOffset = getScrollOffset(node);
        const translation = getTranslation(node);
        childOffset = {
          x: absolute.x - scrollOffset.x + translation.x,
          y: absolute.y - scrollOffset.y + translation.y,
        };
      } else if (absoluteCandidate) {
        absolute = absoluteCandidate;
      }
      childOffsets.set(node, childOffset);
      records.push({ absolute, frame, node });
    },
    [],
    0,
  );
  for (let index = records.length - 1; index >= 0; index -= 1) {
    const item = records[index];
    let absolute = item.absolute;
    const childBounds = (childrenByNode.get(item.node) || [])
      .map(child => absoluteByNode.get(child))
      .filter(Boolean);
    if (!absolute || item.node.component) absolute = unionBounds(childBounds) || absolute;
    if (absolute) {
      absoluteByNode.set(item.node, absolute);
      map.set(getNodeId(item.node), {
        local: item.node.bounds ? normalizeBounds(item.node.bounds) : item.frame,
        absolute,
      });
    }
  }

  const rootBounds = absoluteByNode.get(root) || null;
  const viewportNode = localFrames ? firstElementWithFrame(root) : root;
  const viewportGeometry = viewportNode ? map.get(getNodeId(viewportNode)) : null;
  const viewport = viewportGeometry?.absolute || rootBounds || { x: 0, y: 0, width: 390, height: 760 };
  return {
    localFrames,
    map,
    viewport: {
      x: viewport.x,
      y: viewport.y,
      width: Math.max(1, viewport.width),
      height: Math.max(1, viewport.height),
    },
  };
}

function getNodeGeometry(node) {
  return state.geometry?.map.get(getNodeId(node)) || null;
}

function getViewportBounds() {
  return state.geometry?.viewport || { x: 0, y: 0, width: 390, height: 760 };
}

function getElementIdForNode(node) {
  if (!node) return null;
  if (node.element && node.element.id !== undefined) return Number(node.element.id);
  const parsed = Number.parseInt(getNodeId(node), 10);
  return Number.isNaN(parsed) ? null : parsed;
}

function pointFromScreenEvent(event) {
  const viewport = getViewportBounds();
  const screenBounds = elements.screen.getBoundingClientRect();
  const ratioX = Math.min(1, Math.max(0, (event.clientX - screenBounds.left) / Math.max(1, screenBounds.width)));
  const ratioY = Math.min(1, Math.max(0, (event.clientY - screenBounds.top) / Math.max(1, screenBounds.height)));
  return {
    x: viewport.x + ratioX * viewport.width,
    y: viewport.y + ratioY * viewport.height,
    scaleX: viewport.width / Math.max(1, screenBounds.width),
    scaleY: viewport.height / Math.max(1, screenBounds.height),
  };
}

function boundsContainPoint(bounds, point) {
  return (
    bounds &&
    point.x >= bounds.x &&
    point.y >= bounds.y &&
    point.x <= bounds.x + bounds.width &&
    point.y <= bounds.y + bounds.height
  );
}

function findNodeAtPoint(point, predicate = () => true) {
  const hits = [];
  if (!hasSnapshotTree()) return null;
  walk(state.snapshot.tree, (node, _parent, depth) => {
    if (!predicate(node)) return;
    const geometry = getNodeGeometry(node);
    if (!boundsContainPoint(geometry?.absolute, point)) return;
    const bounds = geometry.absolute;
    hits.push({
      node,
      depth,
      area: Math.max(1, bounds.width * bounds.height),
    });
  });

  hits.sort((a, b) => b.depth - a.depth || a.area - b.area);
  return hits[0]?.node || null;
}

function findInputNodeAtEvent(event) {
  const point = pointFromScreenEvent(event);
  const overlayNode = event.target.closest('.overlay-node');
  const overlayTreeNode =
    overlayNode && elements.screen.contains(overlayNode) ? findNode(overlayNode.dataset.nodeId) : null;
  const nodeWithElement =
    overlayTreeNode && getElementIdForNode(overlayTreeNode) !== null
      ? overlayTreeNode
      : findNodeAtPoint(point, node => getElementIdForNode(node) !== null);
  return { node: nodeWithElement, point };
}

function findOverlayNodeAtEvent(event) {
  const overlayNode = event.target.closest('.overlay-node');
  if (!overlayNode || !elements.screen.contains(overlayNode)) return null;
  return findNode(overlayNode.dataset.nodeId);
}

let debuggerInputDispatchTail = Promise.resolve(null);

function captureDebuggerInputTarget() {
  if (state.source !== 'daemon') return null;
  const params = getSelectedTargetParams();
  if (!params.clientId || !params.contextId || !Number.isFinite(params.port)) return null;
  return Object.freeze({
    port: params.port,
    clientId: params.clientId,
    contextId: params.contextId,
  });
}

function debuggerInputTargetKey(target) {
  return JSON.stringify([target.port, target.clientId, target.contextId]);
}

function debuggerInputTargetElementKey(target, elementId) {
  return JSON.stringify([target.port, target.clientId, target.contextId, elementId]);
}

function isSelectedDebuggerInputTarget(target) {
  const selectedTarget = captureDebuggerInputTarget();
  return selectedTarget !== null && debuggerInputTargetKey(selectedTarget) === debuggerInputTargetKey(target);
}

function scheduleInputRefresh(target, delayMs) {
  const key = debuggerInputTargetKey(target);
  const previousTimer = state.inputRefreshTimers.get(key);
  if (previousTimer) window.clearTimeout(previousTimer);
  const timer = window.setTimeout(() => {
    state.inputRefreshTimers.delete(key);
    if (!isSelectedDebuggerInputTarget(target)) return;
    loadRealSnapshot(target, { silent: true, preserveSelection: true });
  }, delayMs);
  state.inputRefreshTimers.set(key, timer);
}

async function dispatchDebuggerInput(target, payload, options) {
  if (!target) {
    addLog('warn', 'input', 'Attach to a live Valdi daemon target before dispatching input.');
    return null;
  }

  try {
    const result = await apiPost('/api/input', target, payload, { timeoutMs: 5000 });
    const input = result.input || {};
    if (input.handled) {
      if (!options.quiet) {
        const action = input.action ? ` via ${input.action}` : '';
        addLog('info', 'input', `${payload.type} handled by #${input.elementId}${action}.`);
      }
      if (options.refresh !== false) {
        scheduleInputRefresh(target, options.refreshDelayMs ?? 120);
      }
    } else if (!options.quiet) {
      addLog('warn', 'input', input.message || `${payload.type} was not handled.`);
    }
    return input;
  } catch (error) {
    addLog('error', 'input', `${payload.type} failed: ${error.message}`);
    return null;
  }
}

function reserveDebuggerInput(target) {
  let releaseReservation;
  let cancelled = false;
  const reservation = new Promise(resolve => {
    releaseReservation = resolve;
  });
  const dispatch = debuggerInputDispatchTail
    .catch(error => {
      addLog('warn', 'input', `Continuing after a queued input failed: ${error?.message || String(error)}`);
      return null;
    })
    .then(() => reservation)
    .then(input => (input && !cancelled ? dispatchDebuggerInput(target, input.payload, input.options) : null));
  debuggerInputDispatchTail = dispatch;
  let released = false;
  return Object.freeze({
    dispatch(payload, options) {
      if (!released) {
        released = true;
        releaseReservation({ payload, options });
      }
      return dispatch;
    },
    cancel() {
      cancelled = true;
      if (!released) {
        released = true;
        releaseReservation(null);
      }
      return dispatch;
    },
  });
}

function enqueueDebuggerInput(target, payload, options) {
  return reserveDebuggerInput(target).dispatch(payload, options);
}

function getPageScrollTarget() {
  return document.scrollingElement || document.documentElement || document.body;
}

function getPageScrollPosition() {
  return {
    x: window.scrollX || getPageScrollTarget().scrollLeft || 0,
    y: window.scrollY || getPageScrollTarget().scrollTop || 0,
  };
}

function restorePageScrollPosition(position) {
  window.scrollTo(position.x, position.y);
  const scrollTarget = getPageScrollTarget();
  scrollTarget.scrollLeft = position.x;
  scrollTarget.scrollTop = position.y;
}

function filterHierarchyToNode(id) {
  elements.treeSearch.value = `#${id}`;
}

function selectPreviewNode(node) {
  const scrollPosition = getPageScrollPosition();
  const id = getNodeId(node);
  filterHierarchyToNode(id);
  selectNode(id, { scrollTree: false });
  restorePageScrollPosition(scrollPosition);
  window.requestAnimationFrame(() => restorePageScrollPosition(scrollPosition));
}

function canScrollBy(element, deltaY) {
  if (!element || !deltaY) return false;
  const maxScrollTop = Math.max(0, element.scrollHeight - element.clientHeight);
  if (maxScrollTop <= 0) return false;
  if (deltaY < 0) return element.scrollTop > 0;
  return element.scrollTop < maxScrollTop;
}

function forwardPreviewWheelToPage(event) {
  if (event.target.closest('.html-preview-root')) return;
  const scrollTarget = getPageScrollTarget();
  if (!canScrollBy(scrollTarget, event.deltaY)) return;

  event.preventDefault();
  scrollTarget.scrollBy({
    top: event.deltaY,
    left: event.deltaX,
    behavior: 'auto',
  });
}

async function dispatchTapInput(event) {
  if (event.target.closest('.html-preview-root')) return;
  const overlaySelection = findOverlayNodeAtEvent(event);
  const { node, point } = findInputNodeAtEvent(event);
  const selectedPreviewNode = overlaySelection || node;
  const elementId = getElementIdForNode(node);

  if (selectedPreviewNode) {
    event.preventDefault();
    event.stopPropagation();
    selectPreviewNode(selectedPreviewNode);
  }

  if (elementId === null) {
    if (!selectedPreviewNode) {
      addLog('warn', 'input', 'Click ignored because no Valdi element was under the cursor.');
    }
    return;
  }

  const target = captureDebuggerInputTarget();
  await enqueueDebuggerInput(
    target,
    {
      type: 'tap',
      elementId,
      x: point.x,
      y: point.y,
    },
    {},
  );
}

function isInteractiveNode(node) {
  const attrs = getNodeAttributes(node);
  return Boolean(attrs.onTap || attrs.onPress || attrs.onClick || attrs.title || node.tag === 'button');
}

function analyzeTree(root) {
  const issues = [];
  const seen = new Set();

  function addIssue(issue) {
    if (seen.has(issue.id)) return;
    seen.add(issue.id);
    issues.push(issue);
  }

  walk(root, node => {
    const id = getNodeId(node);
    const attrs = getNodeAttributes(node);
    const bounds = node.bounds ? normalizeBounds(node.bounds) : null;
    if (!bounds) return;

    if (bounds.width <= 0 || bounds.height <= 0) {
      addIssue({
        id: `bounds-${id}`,
        severity: 'error',
        nodeId: id,
        title: 'Invalid bounds',
        message: `${node.tag} #${id} has non-positive layout bounds.`,
      });
    }

    if (isInteractiveNode(node) && (bounds.width < 44 || bounds.height < 44)) {
      addIssue({
        id: `touch-${id}`,
        severity: 'warn',
        nodeId: id,
        title: 'Small touch target',
        message: `${node.tag} #${id} is ${bounds.width}x${bounds.height}. Native targets usually need at least 44x44.`,
      });
    }

    if (
      node.tag === 'label' &&
      attrs.numberOfLines === '1' &&
      attrs.value &&
      String(attrs.value).length * 6 > bounds.width
    ) {
      addIssue({
        id: `label-${id}`,
        severity: 'warn',
        nodeId: id,
        title: 'Possible label truncation',
        message: `Label #${id} has numberOfLines=1 and text that may exceed its measured width.`,
      });
    }
  });

  return issues.slice(0, 80);
}

function mergeIssues(existing, generated) {
  const byId = new Map();
  for (const issue of [...existing, ...generated]) {
    const id = issue.id || `${issue.title}:${issue.nodeId || ''}:${issue.message}`;
    byId.set(id, { ...issue, id });
  }
  return Array.from(byId.values());
}

function walk(node, visitor, parent = null, depth = 0) {
  return valdiDebuggerTreeModel.walk(
    node,
    (current, ancestors, currentDepth) => {
      return visitor(current, ancestors.at(-1) ?? parent, depth + currentDepth);
    },
    [],
    0,
  );
}

function walkVisible(node, visitor, parent = null, depth = 0) {
  if (!node) return true;
  return valdiDebuggerTreeModel.walkVisible(
    node,
    (current, ancestors, currentDepth) => {
      return visitor(current, ancestors.at(-1) ?? parent, depth + currentDepth);
    },
    current => state.expandedNodeIds.has(getNodeId(current)),
    [],
    0,
  );
}

function findNodeInTree(root, id) {
  return valdiDebuggerTreeModel.findNode(root, id);
}

function findNode(id) {
  if (!hasSnapshotTree() || id === null || id === undefined) return null;
  return findNodeInTree(state.snapshot.tree, id);
}

function getPathToNode(id) {
  return hasSnapshotTree() ? valdiDebuggerTreeModel.pathToNode(state.snapshot.tree, id) : [];
}

function expandPathToNode(id) {
  const path = getPathToNode(id);
  for (const node of path.slice(0, -1)) {
    state.expandedNodeIds.add(getNodeId(node));
  }
}

function collapseTree() {
  state.expandedNodeIds.clear();
}

function collapseTreeNode(node) {
  walk(node, descendant => {
    state.expandedNodeIds.delete(getNodeId(descendant));
  });
}

function toggleTreeNode(id) {
  const node = findNode(id);
  if (!node || !valdiDebuggerTreeModel.hasChildren(node)) return;
  if (state.expandedNodeIds.has(id)) {
    collapseTreeNode(node);
  } else {
    state.expandedNodeIds.add(id);
  }
  renderTree();
}

function ensureBounds(node, depth = 0, index = 0) {
  if (!node) return;
  valdiDebuggerTreeModel.walk(
    node,
    (current, _ancestors, currentDepth, sourceChildIndex) => {
      const currentIndex = sourceChildIndex === null ? index : sourceChildIndex;
      const hasChildren = valdiDebuggerTreeModel.hasChildren(current);
      if (!current.bounds) {
        current.bounds = {
          x: 12 + (depth + currentDepth) * 16,
          y: 24 + currentIndex * 54 + (depth + currentDepth) * 14,
          width: Math.max(80, 360 - (depth + currentDepth) * 28),
          height: hasChildren ? 110 : 42,
        };
      }
    },
    [],
    0,
  );
}

function escapeHtml(value) {
  return String(value)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#039;');
}

function isMacDesktopTarget(target = state.snapshot.target) {
  return target?.applicationId === 'ValdiDesktop' && String(target.platform).toLowerCase() === 'ios';
}

function displayPlatform(target = state.snapshot.target) {
  return isMacDesktopTarget(target) ? 'macOS desktop (iOS class bridge)' : target.platform;
}

function hierarchySourceLabel() {
  if (state.source === 'daemon') return 'Live';
  return 'Empty';
}
