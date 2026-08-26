// Live HTML projection for the debugger preview frame.
const HTML_PREVIEW_TEXT_INPUT_DEBOUNCE_MS = 180;
const HTML_PREVIEW_SCROLL_INPUT_DEBOUNCE_MS = 60;
const htmlPreviewTextInputs = new Map();
const htmlPreviewScrollInputs = new Map();
const htmlPreviewInputDispatches = new Map();
const htmlPreviewQueuedInputs = new Set();
const htmlPreviewElementTargets = new WeakMap();
const htmlPreviewElementIncarnations = new WeakMap();
const htmlPreviewElementEditEpochs = new WeakMap();
const htmlPreviewElementFocusEpochs = new WeakMap();
let htmlPreviewIncarnation = 0;

function getHtmlPreviewElementEpoch(epochs, element) {
  return epochs.get(element) || 0;
}

function advanceHtmlPreviewElementEpoch(epochs, element) {
  const epoch = getHtmlPreviewElementEpoch(epochs, element) + 1;
  epochs.set(element, epoch);
  return epoch;
}

function isCurrentHtmlPreviewElement(element, incarnation) {
  return incarnation === htmlPreviewIncarnation && htmlPreviewElementIncarnations.get(element) === incarnation;
}

function associateHtmlPreviewElement(element, target) {
  if (target) htmlPreviewElementTargets.set(element, target);
  htmlPreviewElementIncarnations.set(element, htmlPreviewIncarnation);
}

function cancelPendingHtmlPreviewInputs(pendingInputs) {
  for (const pendingInput of pendingInputs.values()) {
    window.clearTimeout(pendingInput.timer);
    void pendingInput.reservation.cancel();
  }
  pendingInputs.clear();
}

function trackQueuedHtmlPreviewInput(incarnation, reservation, dispatch) {
  const queuedInput = { incarnation, reservation };
  htmlPreviewQueuedInputs.add(queuedInput);
  const clearQueuedInput = () => htmlPreviewQueuedInputs.delete(queuedInput);
  void dispatch.then(clearQueuedInput, clearQueuedInput);
}

function cancelQueuedHtmlPreviewInputs() {
  for (const queuedInput of htmlPreviewQueuedInputs) {
    void queuedInput.reservation.cancel();
  }
  htmlPreviewQueuedInputs.clear();
}

function beginHtmlPreviewIncarnation() {
  htmlPreviewIncarnation += 1;
  cancelPendingHtmlPreviewInputs(htmlPreviewTextInputs);
  cancelPendingHtmlPreviewInputs(htmlPreviewScrollInputs);
  cancelQueuedHtmlPreviewInputs();
  htmlPreviewInputDispatches.clear();
  return htmlPreviewIncarnation;
}

function previewClassName(value) {
  return String(value || 'unknown')
    .replace(/[^a-z0-9_-]+/gi, '-')
    .toLowerCase();
}

function previewValue(value) {
  if (value === undefined || value === null) return '';
  if (typeof value === 'string') return value.replace(/^"|"$/g, '');
  if (typeof value === 'number' || typeof value === 'boolean') return String(value);
  if (typeof value === 'object' && value.path) return String(value.path);
  return normalizeLabelValue(value);
}

function previewBoolean(value, fallback = false) {
  if (value === undefined || value === null || value === '') return fallback;
  if (typeof value === 'boolean') return value;
  const normalized = String(value).toLowerCase();
  if (['1', 'true', 'yes', 'on'].includes(normalized)) return true;
  if (['0', 'false', 'no', 'off'].includes(normalized)) return false;
  return fallback;
}

function previewNumber(value, fallback = 0) {
  if (value === undefined || value === null || value === '') return fallback;
  const parsed = Number.parseFloat(String(value).replace(/^"|"$/g, ''));
  return Number.isFinite(parsed) ? parsed : fallback;
}

function previewCssLength(value) {
  if (value === undefined || value === null || value === '') return '';
  if (typeof value === 'number') return `${value}px`;
  const normalized = String(value).replace(/^"|"$/g, '').trim();
  if (!normalized) return '';
  if (/^-?\d+(\.\d+)?$/.test(normalized)) return `${normalized}px`;
  return normalized;
}

function previewColor(value) {
  const normalized = previewValue(value).trim();
  return normalized || '';
}

function previewText(node, attrs) {
  for (const source of [attrs, node.viewModel, node]) {
    if (!source || typeof source !== 'object') continue;
    for (const key of ['value', 'title', 'text', 'placeholder', 'accessibilityLabel', 'label', 'name']) {
      const value = previewValue(source[key]);
      if (value) return value;
    }
  }
  return '';
}

function previewImageSource(attrs) {
  const source = attrs.src || attrs.source || attrs.url;
  if (!source) return '';
  if (typeof source === 'string') return source.replace(/^"|"$/g, '');
  if (typeof source === 'object') {
    return previewValue(source.src || source.url || source.path || source.default || '');
  }
  return previewValue(source);
}

function previewSafeMediaSource(source) {
  const normalized = String(source || '').trim();
  return /^(data|blob):/i.test(normalized) ? normalized : '';
}

function setStyleIfPresent(style, property, value) {
  if (value === '') return;
  style[property] = value;
}

function applyPreviewFrame(element, node) {
  const frame = node.element?.frame
    ? normalizeBounds(node.element.frame)
    : node.bounds
      ? normalizeBounds(node.bounds)
      : null;
  element.style.position = 'absolute';
  if (!frame) {
    element.style.left = '0';
    element.style.top = '0';
    element.style.width = '100%';
    element.style.height = '100%';
    return;
  }
  element.style.left = `${frame.x}px`;
  element.style.top = `${frame.y}px`;
  element.style.width = `${frame.width}px`;
  element.style.height = `${frame.height}px`;
}

function applyPreviewBoxStyles(element, node) {
  const attrs = getNodeAttributes(node);
  setStyleIfPresent(element.style, 'backgroundColor', previewColor(attrs.backgroundColor || attrs.background));
  setStyleIfPresent(element.style, 'color', previewColor(attrs.color || attrs.textColor));
  setStyleIfPresent(element.style, 'opacity', previewValue(attrs.opacity));
  setStyleIfPresent(element.style, 'borderRadius', previewCssLength(attrs.cornerRadius || attrs.borderRadius));
  setStyleIfPresent(element.style, 'borderColor', previewColor(attrs.borderColor));
  setStyleIfPresent(element.style, 'borderWidth', previewCssLength(attrs.borderWidth));
  if (attrs.borderWidth !== undefined || attrs.borderColor !== undefined) {
    element.style.borderStyle = previewValue(attrs.borderStyle) || 'solid';
  }
  setStyleIfPresent(element.style, 'padding', previewCssLength(attrs.padding));
  setStyleIfPresent(element.style, 'paddingLeft', previewCssLength(attrs.paddingLeft));
  setStyleIfPresent(element.style, 'paddingRight', previewCssLength(attrs.paddingRight));
  setStyleIfPresent(element.style, 'paddingTop', previewCssLength(attrs.paddingTop));
  setStyleIfPresent(element.style, 'paddingBottom', previewCssLength(attrs.paddingBottom));
  const transform = [];
  const translationX = previewNumber(attrs.translationX, 0);
  const translationY = previewNumber(attrs.translationY, 0);
  if (translationX || translationY) transform.push(`translate(${translationX}px, ${translationY}px)`);
  if (attrs.scale !== undefined || attrs.scaleX !== undefined || attrs.scaleY !== undefined) {
    const scale = previewNumber(attrs.scale, 1);
    transform.push(`scale(${previewNumber(attrs.scaleX, scale)}, ${previewNumber(attrs.scaleY, scale)})`);
  }
  if (attrs.rotation !== undefined) transform.push(`rotate(${previewNumber(attrs.rotation, 0)}rad)`);
  if (transform.length) element.style.transform = transform.join(' ');
  if (attrs.hidden !== undefined && previewBoolean(attrs.hidden)) element.style.visibility = 'hidden';
}

function applyPreviewTextStyles(element, attrs) {
  setStyleIfPresent(element.style, 'fontSize', previewCssLength(attrs.fontSize));
  setStyleIfPresent(element.style, 'fontWeight', previewValue(attrs.fontWeight));
  setStyleIfPresent(element.style, 'textAlign', previewValue(attrs.textAlign));
  setStyleIfPresent(element.style, 'lineHeight', previewCssLength(attrs.lineHeight));
  setStyleIfPresent(element.style, 'letterSpacing', previewCssLength(attrs.letterSpacing));
  const font = previewValue(attrs.font);
  const fontSize = font.match(/\b(\d+(?:\.\d+)?)\b/);
  if (fontSize && !element.style.fontSize) element.style.fontSize = `${fontSize[1]}px`;
  if (/bold|semibold|demibold|medium/i.test(font)) element.style.fontWeight = '700';
  if (/mono/i.test(font)) {
    element.style.fontFamily = 'ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
  }
  if (attrs.textDecoration !== undefined) {
    const decoration = previewValue(attrs.textDecoration);
    element.style.textDecoration = decoration.includes('underline') ? 'underline' : decoration;
  }
  const lineCount = previewNumber(attrs.numberOfLines, 0);
  if (lineCount > 0) {
    element.style.display = '-webkit-box';
    element.style.webkitLineClamp = String(lineCount);
    element.style.webkitBoxOrient = 'vertical';
  }
}

function createPreviewElement(node, effectivelyDisabled) {
  const tag = String(node.tag || 'view').toLowerCase();
  const attrs = getNodeAttributes(node);
  if (tag === 'textfield') {
    const input = document.createElement('input');
    input.type = previewBoolean(attrs.secureTextEntry) ? 'password' : 'text';
    input.value = previewValue(attrs.value);
    input.placeholder = previewValue(attrs.placeholder);
    input.disabled = effectivelyDisabled;
    input.readOnly = previewBoolean(attrs.editable, true) === false;
    return input;
  }
  if (tag === 'textview' && previewBoolean(attrs.editable, true)) {
    const textArea = document.createElement('textarea');
    textArea.value = previewValue(attrs.value);
    textArea.placeholder = previewValue(attrs.placeholder);
    textArea.disabled = effectivelyDisabled;
    textArea.readOnly = previewBoolean(attrs.editable, true) === false;
    return textArea;
  }
  if (tag === 'image' || tag === 'animatedimage') {
    const image = document.createElement('img');
    image.alt = previewValue(attrs.accessibilityLabel || attrs.alt || '');
    const source = previewImageSource(attrs);
    const safeSource = previewSafeMediaSource(source);
    if (safeSource) image.src = safeSource;
    else if (source) image.dataset.previewResourceOmitted = 'true';
    image.draggable = false;
    return image;
  }
  if (tag === 'video') {
    const video = document.createElement('video');
    const source = previewImageSource(attrs);
    const safeSource = previewSafeMediaSource(source);
    if (safeSource) video.src = safeSource;
    else if (source) video.dataset.previewResourceOmitted = 'true';
    video.muted = true;
    video.playsInline = true;
    return video;
  }
  if (tag === 'webview') {
    const placeholder = document.createElement('div');
    placeholder.dataset.previewResourceOmitted = 'true';
    placeholder.textContent = 'WebView content omitted';
    return placeholder;
  }
  if (tag === 'button') {
    const button = document.createElement('button');
    button.textContent = previewText(node, attrs);
    button.disabled = effectivelyDisabled;
    return button;
  }
  return document.createElement('div');
}

function finishPreviewElement(element, node, target, effectivelyDisabled) {
  const attrs = getNodeAttributes(node);
  const tag = String(node.tag || 'view').toLowerCase();
  const nodeId = getNodeId(node);
  const elementId = getElementIdForNode(node);
  element.classList.add('valdi-html-node', `valdi-html-${previewClassName(tag)}`);
  if (isInteractiveNode(node) || ['button', 'textfield', 'textview'].includes(tag)) {
    element.classList.add('preview-interactive');
  }
  element.dataset.previewNodeId = nodeId;
  if (elementId !== null) element.dataset.previewElementId = String(elementId);
  associateHtmlPreviewElement(element, target);
  if (effectivelyDisabled) {
    element.classList.add('preview-disabled');
    element.setAttribute('aria-disabled', 'true');
  }
  element.title = describeOverlayNode(node);
  applyPreviewFrame(element, node);
  applyPreviewBoxStyles(element, node);

  if (tag === 'label' || (tag === 'textview' && element.tagName !== 'TEXTAREA')) {
    element.classList.add('valdi-html-text');
    element.textContent = previewText(node, attrs);
    applyPreviewTextStyles(element, attrs);
    element.style.userSelect = previewBoolean(attrs.selectable, false) ? 'text' : 'none';
  } else if (element.tagName === 'INPUT' || element.tagName === 'TEXTAREA' || element.tagName === 'BUTTON') {
    applyPreviewTextStyles(element, attrs);
  } else if (element.tagName === 'IMG' || element.tagName === 'VIDEO') {
    element.style.objectFit = previewValue(attrs.objectFit) || 'cover';
  }

  if (tag === 'scroll') {
    element.classList.add('valdi-html-scroll');
    element.scrollLeft = previewNumber(attrs.contentOffsetX, 0);
    element.scrollTop = previewNumber(attrs.contentOffsetY, 0);
  }
}

function isHtmlPreviewEffectivelyDisabled(node, ancestorDisabled) {
  const attrs = getNodeAttributes(node);
  return (
    ancestorDisabled ||
    previewBoolean(attrs.enabled, true) === false ||
    previewBoolean(attrs.accessibilityStateDisabled, false) ||
    previewBoolean(attrs.touchEnabled, true) === false
  );
}

function appendPreviewNode(node, parent, target, ancestorDisabled) {
  if (!node) return;
  if (node.element) {
    const effectivelyDisabled = isHtmlPreviewEffectivelyDisabled(node, ancestorDisabled);
    const element = createPreviewElement(node, effectivelyDisabled);
    finishPreviewElement(element, node, target, effectivelyDisabled);
    parent.appendChild(element);
    const childParent = canHostPreviewChildren(element) ? element : parent;
    for (const child of node.children || []) appendPreviewNode(child, childParent, target, effectivelyDisabled);
    return;
  }
  for (const child of node.children || []) appendPreviewNode(child, parent, target, ancestorDisabled);
}

function canHostPreviewChildren(element) {
  return !['INPUT', 'IMG', 'TEXTAREA', 'VIDEO'].includes(element.tagName);
}

function updateHtmlPreviewScale() {
  const canvas = elements.htmlPreviewRoot.querySelector('.html-preview-canvas');
  if (!canvas) return;
  const viewport = getViewportBounds();
  const screenBounds = elements.screen.getBoundingClientRect();
  const scaleX = screenBounds.width / Math.max(1, viewport.width);
  const scaleY = screenBounds.height / Math.max(1, viewport.height);
  canvas.style.transform = `scale(${scaleX}, ${scaleY})`;
}

function markHtmlPreviewSelection() {
  elements.htmlPreviewRoot.querySelectorAll('.preview-selected').forEach(element => {
    element.classList.remove('preview-selected');
  });
  if (!state.selectedNodeId) return;
  const selected = Array.from(elements.htmlPreviewRoot.querySelectorAll('[data-preview-node-id]')).find(
    element => element.dataset.previewNodeId === String(state.selectedNodeId),
  );
  selected?.classList.add('preview-selected');
}

function renderHtmlPreview() {
  beginHtmlPreviewIncarnation();
  elements.htmlPreviewRoot.replaceChildren();
  elements.htmlPreviewRoot.classList.toggle('active', false);
  if (!hasSnapshotTree()) return false;

  const viewport = getViewportBounds();
  elements.device.style.setProperty('--device-w', viewport.width);
  elements.device.style.setProperty('--device-h', viewport.height);

  const canvas = document.createElement('div');
  canvas.className = 'html-preview-canvas';
  canvas.style.width = `${viewport.width}px`;
  canvas.style.height = `${viewport.height}px`;
  appendPreviewNode(state.snapshot.tree, canvas, captureDebuggerInputTarget(), false);
  if (!canvas.querySelector('[data-preview-node-id]')) return false;

  elements.htmlPreviewRoot.appendChild(canvas);
  elements.htmlPreviewRoot.classList.toggle('active', true);
  updateHtmlPreviewScale();
  markHtmlPreviewSelection();
  return true;
}

function previewElementNodeFromEvent(event) {
  const element = event.target.closest('[data-preview-node-id]');
  if (!element || !elements.htmlPreviewRoot.contains(element)) {
    return { element: null, node: null, elementId: null, target: null };
  }
  const node = findNode(element.dataset.previewNodeId);
  const parsedElementId = Number.parseInt(element.dataset.previewElementId || '', 10);
  const incarnation = htmlPreviewElementIncarnations.get(element);
  if (incarnation !== htmlPreviewIncarnation) {
    return { element: null, node: null, elementId: null, target: null, incarnation: null };
  }
  return {
    element,
    node,
    elementId: Number.isNaN(parsedElementId) ? null : parsedElementId,
    target: htmlPreviewElementTargets.get(element) || null,
    incarnation,
  };
}

function isEditableHtmlPreviewTarget(target) {
  return target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement || target.isContentEditable;
}

function enqueueHtmlPreviewInput(target, elementId, payload, options) {
  if (!target) return Promise.resolve(null);
  const key = debuggerInputTargetElementKey(target, elementId);
  const dispatch = enqueueDebuggerInput(target, payload, options);
  htmlPreviewInputDispatches.set(key, dispatch);
  const clearDispatch = () => {
    if (htmlPreviewInputDispatches.get(key) === dispatch) {
      htmlPreviewInputDispatches.delete(key);
    }
  };
  void dispatch.then(clearDispatch, clearDispatch);
  return dispatch;
}

function flushHtmlPreviewTextInput(target, elementId) {
  if (!target) return Promise.resolve(null);
  const key = debuggerInputTargetElementKey(target, elementId);
  const pendingInput = htmlPreviewTextInputs.get(key);
  if (!pendingInput) return Promise.resolve(null);
  window.clearTimeout(pendingInput.timer);
  htmlPreviewTextInputs.delete(key);
  if (
    pendingInput.incarnation !== htmlPreviewIncarnation ||
    !isCurrentHtmlPreviewElement(pendingInput.element, pendingInput.incarnation)
  ) {
    return pendingInput.reservation.cancel();
  }
  const dispatch = pendingInput.reservation.dispatch(pendingInput.input, {
    quiet: true,
    refresh: false,
    refreshDelayMs: 180,
  });
  trackQueuedHtmlPreviewInput(pendingInput.incarnation, pendingInput.reservation, dispatch);
  void dispatch.then(input => {
    reconcileHtmlPreviewTextInputResult(
      pendingInput.element,
      pendingInput.input.text,
      input,
      pendingInput.incarnation,
      pendingInput.editEpoch,
    );
  });
  htmlPreviewInputDispatches.set(key, dispatch);
  const clearDispatch = () => {
    if (htmlPreviewInputDispatches.get(key) === dispatch) {
      htmlPreviewInputDispatches.delete(key);
    }
  };
  void dispatch.then(clearDispatch, clearDispatch);
  return dispatch;
}

function dispatchHtmlPreviewFocusInput(event, focused) {
  const { element, node, elementId, target } = previewElementNodeFromEvent(event);
  if (!element || !node || elementId === null || !target || !isEditableHtmlPreviewTarget(event.target)) {
    return Promise.resolve(null);
  }
  selectPreviewNode(node);
  advanceHtmlPreviewElementEpoch(htmlPreviewElementFocusEpochs, element);
  if (!focused) {
    void flushHtmlPreviewTextInput(target, elementId);
  }
  return enqueueHtmlPreviewInput(
    target,
    elementId,
    {
      type: 'focus',
      elementId,
      focused,
    },
    {
      quiet: true,
      refresh: !focused,
      refreshDelayMs: 120,
    },
  );
}

async function dispatchHtmlPreviewTapInput(event) {
  const { element, node, elementId, target } = previewElementNodeFromEvent(event);
  if (!element || !node) return;
  event.stopPropagation();
  selectPreviewNode(node);
  if (elementId === null || !target) return;

  if (isEditableHtmlPreviewTarget(event.target)) return;
  event.preventDefault();
  const point = pointFromScreenEvent(event);
  await enqueueDebuggerInput(
    target,
    {
      type: 'tap',
      elementId,
      x: point.x,
      y: point.y,
    },
    {
      refreshDelayMs: 120,
    },
  );
}

function dispatchHtmlPreviewScrollInput(event) {
  if (!hasSnapshotTree() || state.source !== 'daemon') return;
  const previewElement = event.target.closest('[data-preview-node-id]');
  if (!previewElement || !elements.htmlPreviewRoot.contains(previewElement)) return;
  const target = htmlPreviewElementTargets.get(previewElement) || null;
  const incarnation = htmlPreviewElementIncarnations.get(previewElement);
  if (!target || incarnation !== htmlPreviewIncarnation) return;
  const point = pointFromScreenEvent(event);
  const scrollNode =
    findNodeAtPoint(
      point,
      node => String(node.tag || '').toLowerCase() === 'scroll' && getElementIdForNode(node) !== null,
    ) || findNodeAtPoint(point, node => hasScrollState(node) && getElementIdForNode(node) !== null);
  const node = scrollNode || findNodeAtPoint(point, candidate => getElementIdForNode(candidate) !== null);
  const elementId = getElementIdForNode(node);
  if (elementId === null) return;
  event.preventDefault();
  event.stopPropagation();
  const key = debuggerInputTargetElementKey(target, elementId);
  const pending = htmlPreviewScrollInputs.get(key);
  if (pending) {
    pending.input.deltaX += event.deltaX;
    pending.input.deltaY += event.deltaY;
    pending.input.x = point.x;
    pending.input.y = point.y;
    return;
  }

  const input = {
    type: 'scroll',
    elementId,
    x: point.x,
    y: point.y,
    deltaX: event.deltaX,
    deltaY: event.deltaY,
  };
  const reservation = reserveDebuggerInput(target);
  const timer = window.setTimeout(() => {
    if (incarnation !== htmlPreviewIncarnation || htmlPreviewScrollInputs.get(key)?.reservation !== reservation) {
      void reservation.cancel();
      return;
    }
    htmlPreviewScrollInputs.delete(key);
    const dispatch = reservation.dispatch(input, {
      quiet: true,
      refreshDelayMs: 120,
    });
    trackQueuedHtmlPreviewInput(incarnation, reservation, dispatch);
  }, HTML_PREVIEW_SCROLL_INPUT_DEBOUNCE_MS);
  htmlPreviewScrollInputs.set(key, { incarnation, input, target, reservation, timer });
}

function dispatchHtmlPreviewTextInput(event) {
  const { element, node, elementId, target, incarnation } = previewElementNodeFromEvent(event);
  if (!element || !node || elementId === null || !target) return;
  if (!(event.target instanceof HTMLInputElement) && !(event.target instanceof HTMLTextAreaElement)) return;
  selectPreviewNode(node);
  const editEpoch = advanceHtmlPreviewElementEpoch(htmlPreviewElementEditEpochs, element);
  const text = event.target.value;
  const key = debuggerInputTargetElementKey(target, elementId);
  const pendingInput = htmlPreviewTextInputs.get(key);
  if (pendingInput) {
    window.clearTimeout(pendingInput.timer);
  }
  const input = {
    type: 'text',
    elementId,
    text,
    selectionStart: event.target.selectionStart,
    selectionEnd: event.target.selectionEnd,
  };
  const reservation = pendingInput?.reservation || reserveDebuggerInput(target);
  const timer = window.setTimeout(() => {
    void flushHtmlPreviewTextInput(target, elementId);
  }, HTML_PREVIEW_TEXT_INPUT_DEBOUNCE_MS);
  htmlPreviewTextInputs.set(key, { editEpoch, element, incarnation, input, target, timer, reservation });
}

function reconcileHtmlPreviewTextInputResult(element, expectedValue, input, incarnation, editEpoch) {
  if (!input?.handled || typeof input.value !== 'string') return;
  if (!isCurrentHtmlPreviewElement(element, incarnation)) return;
  if (getHtmlPreviewElementEpoch(htmlPreviewElementEditEpochs, element) !== editEpoch) return;
  if (element.value !== expectedValue) return;
  if (element.value !== input.value) element.value = input.value;
  if (!Number.isInteger(input.selectionStart) || !Number.isInteger(input.selectionEnd)) return;
  const selectionStart = Math.max(0, Math.min(input.value.length, input.selectionStart));
  const selectionEnd = Math.max(selectionStart, Math.min(input.value.length, input.selectionEnd));
  if (element.selectionStart !== selectionStart || element.selectionEnd !== selectionEnd) {
    element.setSelectionRange(selectionStart, selectionEnd);
  }
}

function dispatchHtmlPreviewKeyInput(event) {
  const { element, node, elementId, target, incarnation } = previewElementNodeFromEvent(event);
  if (!element || !node || elementId === null || !target) return Promise.resolve(null);
  if (!(event.target instanceof HTMLInputElement) && !(event.target instanceof HTMLTextAreaElement)) {
    return Promise.resolve(null);
  }
  if (event.key !== 'Enter' && event.key !== 'Escape') return Promise.resolve(null);
  selectPreviewNode(node);
  const attributes = getNodeAttributes(node);
  const editEpoch = advanceHtmlPreviewElementEpoch(htmlPreviewElementEditEpochs, element);
  const focusEpoch = getHtmlPreviewElementEpoch(htmlPreviewElementFocusEpochs, element);
  const selectionStart = event.target.selectionStart;
  const selectionEnd = event.target.selectionEnd;
  if (event.key === 'Enter' && event.target instanceof HTMLTextAreaElement) {
    // The runtime key path owns the newline and onWillChange/onChange/onReturn callbacks. Prevent the
    // browser from also emitting a full-text input, then mirror the pending newline locally so typing
    // stays responsive while the authoritative runtime result is in flight.
    event.preventDefault();
    if (!previewBoolean(attributes.ignoreNewlines, false)) {
      const start = Math.max(0, Math.min(event.target.value.length, selectionStart));
      const end = Math.max(start, Math.min(event.target.value.length, selectionEnd));
      const caret = start + 1;
      event.target.value = event.target.value.slice(0, start) + '\n' + event.target.value.slice(end);
      event.target.setSelectionRange(caret, caret);
    }
  }
  const expectedValue = event.target.value;
  void flushHtmlPreviewTextInput(target, elementId);
  const dispatch = enqueueHtmlPreviewInput(
    target,
    elementId,
    {
      type: 'key',
      elementId,
      key: event.key,
      selectionStart,
      selectionEnd,
    },
    {
      quiet: true,
      refresh: false,
      refreshDelayMs: 180,
    },
  );
  const configuredClose = attributes.closesWhenReturnKeyPressed;
  const closesOnReturn =
    configuredClose === undefined
      ? event.target instanceof HTMLInputElement
      : previewBoolean(configuredClose, event.target instanceof HTMLInputElement);
  const shouldBlurAfterDispatch = event.key === 'Escape' || closesOnReturn;
  return dispatch.then(input => {
    reconcileHtmlPreviewTextInputResult(event.target, expectedValue, input, incarnation, editEpoch);
    if (
      shouldBlurAfterDispatch &&
      input?.handled &&
      isCurrentHtmlPreviewElement(event.target, incarnation) &&
      getHtmlPreviewElementEpoch(htmlPreviewElementFocusEpochs, event.target) === focusEpoch &&
      document.activeElement === event.target
    ) {
      event.target.blur();
    }
    return input;
  });
}
