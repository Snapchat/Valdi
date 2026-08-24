// Live HTML projection for the debugger preview frame.
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

function createPreviewElement(node) {
  const tag = String(node.tag || 'view').toLowerCase();
  const attrs = getNodeAttributes(node);
  if (tag === 'textfield') {
    const input = document.createElement('input');
    input.type = previewBoolean(attrs.secureTextEntry) ? 'password' : 'text';
    input.value = previewValue(attrs.value);
    input.placeholder = previewValue(attrs.placeholder);
    input.disabled = previewBoolean(attrs.enabled, true) === false;
    input.readOnly = true;
    return input;
  }
  if (tag === 'textview' && previewBoolean(attrs.editable, true)) {
    const textArea = document.createElement('textarea');
    textArea.value = previewValue(attrs.value);
    textArea.placeholder = previewValue(attrs.placeholder);
    textArea.disabled = previewBoolean(attrs.enabled, true) === false;
    textArea.readOnly = true;
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
    return button;
  }
  return document.createElement('div');
}

function finishPreviewElement(element, node) {
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

function appendPreviewNode(node, parent) {
  if (!node) return;
  if (node.element) {
    const element = createPreviewElement(node);
    finishPreviewElement(element, node);
    parent.appendChild(element);
    const childParent = canHostPreviewChildren(element) ? element : parent;
    for (const child of node.children || []) appendPreviewNode(child, childParent);
    return;
  }
  for (const child of node.children || []) appendPreviewNode(child, parent);
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
  appendPreviewNode(state.snapshot.tree, canvas);
  if (!canvas.querySelector('[data-preview-node-id]')) return false;

  elements.htmlPreviewRoot.appendChild(canvas);
  elements.htmlPreviewRoot.classList.toggle('active', true);
  updateHtmlPreviewScale();
  markHtmlPreviewSelection();
  return true;
}

function previewElementNodeFromEvent(event) {
  const element = event.target.closest('[data-preview-node-id]');
  if (!element || !elements.htmlPreviewRoot.contains(element)) return { element: null, node: null };
  const node = findNode(element.dataset.previewNodeId);
  return { element, node };
}

function selectHtmlPreviewNodeAtEvent(event) {
  const { element, node } = previewElementNodeFromEvent(event);
  if (!element || !node) return;
  event.preventDefault();
  event.stopPropagation();
  selectPreviewNode(node);
}
