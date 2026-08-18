interface IntegrationTestWebFileStore {
  files: Record<string, string>;
  marks: Record<string, boolean>;
}

type NativeHtmlNode = HTMLElement & {
  shadowRoot?: ShadowRoot | null;
  value?: string;
  disabled?: boolean;
  focus?: () => void;
  blur?: () => void;
  setSelectionRange?: (start: number, end: number) => void;
};

interface HtmlToImageModule {
  toPng(element: HTMLElement, options: {
    width: number;
    height: number;
    pixelRatio: number;
    skipAutoScale: boolean;
    skipFonts: boolean;
  }): Promise<string>;
}

declare const require: (module: string) => unknown;
const htmlToImage = require('html-to-image') as HtmlToImageModule;
const SNAPSHOT_PIXEL_RATIO = 3;
const PNG_DATA_URL_PREFIX_PATTERN = /^data:image\/png;base64,/;
let pendingPost: Promise<void> = Promise.resolve();

declare global {
  interface Window {
    __valdiIntegrationTest?: IntegrationTestWebFileStore;
    __valdiTakeElementSnapshot?: (element: HTMLElement) => Promise<string>;
  }
}

function store(): IntegrationTestWebFileStore {
  if (!window.__valdiIntegrationTest) {
    window.__valdiIntegrationTest = {
      files: {},
      marks: {},
    };
  }
  return window.__valdiIntegrationTest;
}

function dispatchMouse(node: HTMLElement, type: string, x: number, y: number): void {
  node.dispatchEvent(new MouseEvent(type, {
    bubbles: true,
    cancelable: true,
    clientX: x,
    clientY: y,
    buttons: type === 'mouseup' ? 0 : 1,
  }));
}

function inputTarget(node: NativeHtmlNode): NativeHtmlNode {
  const shadowInput = node.shadowRoot?.querySelector('input,textarea') as NativeHtmlNode | null;
  return shadowInput ?? node;
}

function enqueuePost(label: string, endpoint: string, payload: unknown): void {
  pendingPost = pendingPost
    .then(async () => {
      const response = await fetch(endpoint, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify(payload),
      });
      if (!response.ok) {
        throw new Error(`${endpoint} returned ${response.status}`);
      }
    })
    .catch(error => {
      console.error(`[integration-test-host] failed to post ${label}`, error);
    });
}

function prepareScrollableSnapshot(element: HTMLElement): () => void {
  const cleanups: Array<() => void> = [];
  const nodes = [element, ...Array.from(element.querySelectorAll<HTMLElement>('*'))];

  for (const node of nodes) {
    const isScrollable = node.scrollWidth > node.clientWidth || node.scrollHeight > node.clientHeight;
    if (!isScrollable) {
      continue;
    }

    const previousScrollbarWidth = node.style.getPropertyValue('scrollbar-width');
    const previousScrollbarPriority = node.style.getPropertyPriority('scrollbar-width');
    const hadHideVerticalScrollbar = node.classList.contains('hide-v-scrollbar');
    const hadHideHorizontalScrollbar = node.classList.contains('hide-h-scrollbar');
    node.style.setProperty('scrollbar-width', 'none');
    node.classList.add('hide-v-scrollbar', 'hide-h-scrollbar');
    cleanups.push(() => {
      node.style.setProperty('scrollbar-width', previousScrollbarWidth, previousScrollbarPriority);
      node.classList.toggle('hide-v-scrollbar', hadHideVerticalScrollbar);
      node.classList.toggle('hide-h-scrollbar', hadHideHorizontalScrollbar);
    });

    const scrollLeft = node.scrollLeft;
    const scrollTop = node.scrollTop;
    if (scrollLeft === 0 && scrollTop === 0) {
      continue;
    }

    for (const child of Array.from(node.children)) {
      if (!(child instanceof HTMLElement)) {
        continue;
      }
      const previousTransform = child.style.transform;
      const offsetTransform = `translate(${-scrollLeft}px, ${-scrollTop}px)`;
      child.style.transform = previousTransform ? `${offsetTransform} ${previousTransform}` : offsetTransform;
      cleanups.push(() => {
        child.style.transform = previousTransform;
      });
    }
  }

  return () => {
    for (let i = cleanups.length - 1; i >= 0; i--) {
      cleanups[i]();
    }
  };
}

window.__valdiTakeElementSnapshot = async function(element: HTMLElement): Promise<string> {
  const rect = element.getBoundingClientRect();
  const width = Math.round(rect.width);
  const height = Math.round(rect.height);
  if (width <= 0 || height <= 0) {
    throw new Error('Cannot snapshot empty element');
  }

  const restoreScrollableSnapshot = prepareScrollableSnapshot(element);
  try {
    const dataUrl = await htmlToImage.toPng(element, {
      width,
      height,
      pixelRatio: SNAPSHOT_PIXEL_RATIO,
      skipAutoScale: true,
      skipFonts: true,
    });
    return dataUrl.replace(PNG_DATA_URL_PREFIX_PATTERN, '');
  } finally {
    restoreScrollableSnapshot();
  }
};

export function getPlatform(): string {
  return 'web';
}

export function getOutputPath(): string {
  return '/tmp/valdi-integration-test/results.json';
}

export function markFinished(path: string): void {
  store().marks[path] = true;
  console.log(`[integration-test-host] mark ${path}`);
  enqueuePost('mark', '/integration-test-mark', { path });
}

export function writeTextFile(path: string, contents: string): void {
  store().files[path] = contents;
  console.log(`[integration-test-host] write ${path} (${contents.length} bytes)`);
  enqueuePost('file', '/integration-test-file', { path, contents });
}

export function submitTouchSequence(node: NativeHtmlNode, sequenceJson: string): string {
  if (!node) {
    return 'web touch dispatch skipped: node unavailable';
  }

  const rect = node.getBoundingClientRect();
  const sequence = JSON.parse(sequenceJson) as { events?: Array<{ action: string; x: number; y: number }> };
  for (const event of sequence.events ?? []) {
    const x = rect.left + rect.width * event.x;
    const y = rect.top + rect.height * event.y;
    if (event.action === 'down') {
      dispatchMouse(node, 'mousedown', x, y);
    } else if (event.action === 'move') {
      dispatchMouse(node, 'mousemove', x, y);
    } else if (event.action === 'up') {
      dispatchMouse(node, 'mouseup', x, y);
      dispatchMouse(node, 'click', x, y);
    }
  }
  return 'web dispatched mouse sequence';
}

export function focusTextInput(node: NativeHtmlNode): string {
  const target = inputTarget(node);
  target.focus?.();
  return 'web focus dispatched';
}

export function replaceText(node: NativeHtmlNode, value: string): string {
  const target = inputTarget(node);
  target.value = value;
  target.dispatchEvent(new InputEvent('input', { bubbles: true, cancelable: true, data: value }));
  return `web replaceText length=${value.length}`;
}

export function pressReturn(node: NativeHtmlNode): string {
  const target = inputTarget(node);
  target.dispatchEvent(new KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: 'Enter' }));
  target.dispatchEvent(new KeyboardEvent('keyup', { bubbles: true, cancelable: true, key: 'Enter' }));
  return 'web return dispatched';
}

export function pressBackspace(node: NativeHtmlNode): string {
  const target = inputTarget(node);
  target.dispatchEvent(new KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: 'Backspace' }));
  if (typeof target.value === 'string' && target.value.length > 0) {
    target.value = target.value.slice(0, target.value.length - 1);
    target.dispatchEvent(new InputEvent('input', { bubbles: true, cancelable: true, inputType: 'deleteContentBackward' }));
  }
  target.dispatchEvent(new KeyboardEvent('keyup', { bubbles: true, cancelable: true, key: 'Backspace' }));
  return 'web backspace dispatched';
}
