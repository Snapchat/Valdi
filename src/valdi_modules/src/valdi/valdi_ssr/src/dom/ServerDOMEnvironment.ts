import {
  ServerDocument,
  ServerElement,
  ServerImageElement,
  ServerNode,
  ServerShadowRoot,
  type ServerDOMMutationTracker,
} from './ServerDOM';

class ServerResizeObserver {
  constructor(_callback: ResizeObserverCallback) {}
  disconnect(): void {}
  observe(_target: Element): void {}
  unobserve(_target: Element): void {}
}

class ServerIntersectionObserver {
  readonly root = null;
  readonly rootMargin = '0px';
  readonly thresholds: ReadonlyArray<number> = [];

  constructor(_callback: IntersectionObserverCallback, _options?: IntersectionObserverInit) {}
  disconnect(): void {}
  observe(_target: Element): void {}
  takeRecords(): IntersectionObserverEntry[] {
    return [];
  }
  unobserve(_target: Element): void {}
}

class ServerWeakRef<T extends object> {
  constructor(private readonly target: T) {}

  deref(): T {
    return this.target;
  }
}

let installedDocument: ServerDocument | undefined;
const installedGlobals = new Map<string, unknown>();
let serverDOMHostCount = 0;

function setGlobalIfMissing(name: string, value: unknown): void {
  const target = globalThis as Record<string, unknown>;
  if (target[name] === undefined) {
    target[name] = value;
    installedGlobals.set(name, value);
  }
}

export function installServerDOMGlobals(): ServerDocument {
  if (installedDocument) {
    return installedDocument;
  }
  const existingDocument = (globalThis as { document?: unknown }).document;
  if (existingDocument !== undefined) {
    if (existingDocument instanceof ServerDocument) {
      installedDocument = existingDocument;
      return existingDocument;
    }
    throw new Error('ValdiHTMLRenderer requires a server environment without an existing browser DOM');
  }

  const document = new ServerDocument();
  installedDocument = document;
  const windowListeners = new Map<string, Array<EventListenerOrEventListenerObject>>();
  const serverWindow: Record<string, any> = {
    devicePixelRatio: 1,
    document,
    innerHeight: 0,
    innerWidth: 0,
    location: {
      href: 'http://localhost/',
      origin: 'http://localhost',
      search: '',
    },
    addEventListener(type: string, listener: EventListenerOrEventListenerObject): void {
      const listeners = windowListeners.get(type) ?? [];
      listeners.push(listener);
      windowListeners.set(type, listeners);
    },
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject): void {
      const listeners = windowListeners.get(type);
      const index = listeners?.indexOf(listener) ?? -1;
      if (index >= 0) {
        listeners!.splice(index, 1);
      }
    },
    getComputedStyle(element: ServerElement): CSSStyleDeclaration {
      return new Proxy(element.style, {
        get(target, property, receiver): unknown {
          if (property === 'direction') {
            return target.getPropertyValue('direction') || document.dir;
          }
          if (property === 'borderLeftWidth' || property === 'borderTopWidth') {
            return target.getPropertyValue(String(property)) || '0px';
          }
          return Reflect.get(target, property, receiver);
        },
      }) as unknown as CSSStyleDeclaration;
    },
    getSelection(): Selection | null {
      return null;
    },
    matchMedia(): MediaQueryList {
      return {
        matches: false,
        media: '',
        onchange: null,
        addEventListener(): void {},
        addListener(): void {},
        dispatchEvent(): boolean {
          return true;
        },
        removeEventListener(): void {},
        removeListener(): void {},
      };
    },
    setInterval: globalThis.setInterval.bind(globalThis),
    clearInterval: globalThis.clearInterval.bind(globalThis),
    setTimeout: globalThis.setTimeout.bind(globalThis),
    clearTimeout: globalThis.clearTimeout.bind(globalThis),
  };
  serverWindow.parent = serverWindow;
  serverWindow.self = serverWindow;
  serverWindow.window = serverWindow;

  const requestAnimationFrame = (callback: FrameRequestCallback): number =>
    globalThis.setTimeout(() => callback(globalThis.performance?.now() ?? Date.now()), 0) as unknown as number;
  const cancelAnimationFrame = (handle: number): void => globalThis.clearTimeout(handle as unknown as number);

  setGlobalIfMissing('document', document);
  setGlobalIfMissing('window', serverWindow);
  setGlobalIfMissing('Document', ServerDocument);
  setGlobalIfMissing('Element', ServerElement);
  setGlobalIfMissing('HTMLElement', ServerElement);
  setGlobalIfMissing('HTMLImageElement', ServerImageElement);
  setGlobalIfMissing('Node', ServerNode);
  setGlobalIfMissing('ShadowRoot', ServerShadowRoot);
  setGlobalIfMissing('ResizeObserver', ServerResizeObserver);
  setGlobalIfMissing('IntersectionObserver', ServerIntersectionObserver);
  // Some CLI JavaScript engines do not implement WeakRef. The renderer only
  // uses it as a lookup cache, so retaining the target for the SSR request is
  // equivalent and the globals are removed when the last renderer is destroyed.
  setGlobalIfMissing('WeakRef', ServerWeakRef);
  setGlobalIfMissing('getComputedStyle', serverWindow.getComputedStyle);
  setGlobalIfMissing('requestAnimationFrame', requestAnimationFrame);
  setGlobalIfMissing('cancelAnimationFrame', cancelAnimationFrame);
  setGlobalIfMissing(
    'Image',
    class extends ServerImageElement {
      constructor() {
        super(document);
      }
    },
  );
  return document;
}

export function createServerDOMHost(mutationTracker: ServerDOMMutationTracker): ServerElement {
  const document = installServerDOMGlobals();
  serverDOMHostCount++;
  const host = document.createElement('div');
  host.setMutationTracker(mutationTracker);
  return host;
}

export function releaseServerDOMHost(): void {
  if (serverDOMHostCount === 0) {
    throw new Error('Unbalanced server DOM host release');
  }
  serverDOMHostCount--;
  if (serverDOMHostCount !== 0) {
    return;
  }
  const target = globalThis as Record<string, unknown>;
  installedGlobals.forEach((value, name) => {
    if (target[name] === value) {
      delete target[name];
    }
  });
  installedGlobals.clear();
  installedDocument = undefined;
}
