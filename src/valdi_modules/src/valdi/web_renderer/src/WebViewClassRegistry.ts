/**
 * Registry for polyglot <custom-view> implementations on web.
 * When a custom-view has webClass="SomeClassName", the web renderer looks up
 * the class name here and calls the factory to create the DOM content.
 * Modules (e.g. valdi_polyglot) register via addRegistrationCallback(); the
 * registry and pending callbacks are stored on globalThis so all chunks share one registry.
 */

export type WebViewClassAttributeHandler = { changeAttribute: (name: string, value: unknown) => void };
export type WebViewClassFactory = (container: HTMLElement) => WebViewClassAttributeHandler | void;
export type WebViewClassRegistry = Map<string, WebViewClassFactory>;
export type WebViewClassRegistrationCallback = (reg: WebViewClassRegistry) => void;
export type WebViewClassRegistryModule = {
  addRegistrationCallback?: (fn: WebViewClassRegistrationCallback) => void;
};

const REGISTRY_KEY = '__valdiWebViewClassRegistry';
const CALLBACKS_KEY = '__valdiWebViewClassRegistryCallbacks';

function getGlobal(): typeof globalThis | undefined {
  return typeof globalThis !== 'undefined' ? globalThis : undefined;
}

function getSharedCallbacks(): Array<WebViewClassRegistrationCallback> {
  const g = getGlobal();
  if (!g) return [];
  let cbs = (g as any)[CALLBACKS_KEY];
  if (!Array.isArray(cbs)) {
    cbs = [];
    (g as any)[CALLBACKS_KEY] = cbs;
  }
  return cbs;
}

function getSharedRegistry(): WebViewClassRegistry | null {
  const g = getGlobal();
  if (!g) return null;
  const reg = (g as any)[REGISTRY_KEY];
  return reg instanceof Map ? reg : null;
}

function setSharedRegistry(reg: WebViewClassRegistry): void {
  const g = getGlobal();
  if (g) (g as any)[REGISTRY_KEY] = reg;
}

/**
 * Register a callback that runs when the registry is (or was) ready.
 * If the registry already exists, the callback runs immediately; otherwise it runs
 * when getRegistry() is first called (e.g. from ValdiWebRendererDelegate constructor).
 */
export function addRegistrationCallback(fn: WebViewClassRegistrationCallback): void {
  const existing = getSharedRegistry();
  if (existing !== null) {
    fn(existing);
  } else {
    const cbs = getSharedCallbacks();
    cbs.push(fn);
  }
}

export function getRegistry(): WebViewClassRegistry {
  const existing = getSharedRegistry();
  if (existing !== null) {
    return existing;
  }
  const registry: WebViewClassRegistry = new Map<string, WebViewClassFactory>();
  const cbs = getSharedCallbacks();
  cbs.forEach((cb) => cb(registry));
  cbs.length = 0;
  setSharedRegistry(registry);
  return registry;
}

export function registerWebViewClass(className: string, factory: WebViewClassFactory): void {
  const reg = getRegistry();
  reg.set(className, factory);
}

export function getWebViewClassFactory(className: string): WebViewClassFactory | undefined {
  const reg = getRegistry();
  return reg.get(className);
}
