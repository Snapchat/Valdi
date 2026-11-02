/* eslint @typescript-eslint/no-unused-vars: ["error", { "argsIgnorePattern": "^_" }] */
import type { IModuleLoader } from 'valdi_core/src/IModuleLoader';
import type { ColorPalette, NativeWorker } from 'valdi_core/src/ValdiRuntime';
import { beginValdiWebTrace, endValdiWebTrace, makeValdiWebTraceProxy } from './tracing/ValdiWebTracing';

declare const require: (id: string) => unknown;

interface WebAsset {
  path: string;
  src: string;
  width?: number;
  height?: number;
}

type ModuleExports = Record<string, unknown>;
type ModuleRegistry = Record<string, () => ModuleExports>;
type BrowserConsole = Pick<Console, 'log' | 'warn' | 'error' | 'info' | 'debug' | 'dir' | 'trace' | 'assert'>;

type WebGlobals = Record<
  '__valdiBootstrapModules' | '__valdiNavigationPages' | '__valdiWorkerModules',
  ModuleRegistry | undefined
> &
  Record<'__valdiImageRegistry', Record<string, Record<string, string | { default: string }>> | undefined> &
  Record<'__valdiImageContext', { (path: string): string | { default: string }; keys(): string[] } | undefined> &
  Record<'__originalConsole__', BrowserConsole> &
  Record<'__valdiLogToConsole__', (...args: unknown[]) => void> &
  Record<'__originalTimingFunctions__', Pick<Window, 'setTimeout' | 'clearTimeout' | 'setInterval' | 'clearInterval'>>;

const globalObject = globalThis as typeof globalThis &
  WebGlobals & {
    global: typeof globalThis;
    describe: (name: string, callback: () => void) => void;
    runtime: Runtime;
    moduleLoader?: IModuleLoader;
    currentPalette?: ColorPalette;
  };
const path = require('path-browserify') as { normalize(path: string): string; basename(path: string): string };

// Valdi runtime assumes global instead of globalThis
globalObject.global = globalThis;

// To make tests happy
globalObject.describe = function (_name: string, _callback: () => void): void {};

// Eager context removed — modules are now resolved lazily via:
// 1. __valdiBootstrapModules (statically imported essentials)
// 2. moduleLoader factories (registerModule'd native modules)

class Runtime {
  componentPaths = new Map<string, unknown>();
  isDebugEnabled = true;
  // ConsoleLogTransformer guards use runtime.isLoggingEnabled
  isLoggingEnabled = true;
  buildType = 'debug';
  // Map of task IDs to timeout IDs for scheduleWorkItem
  private _taskIdCounter = 1;
  private _scheduledTasks = new Map<number, number>();
  // Named color palettes registered via configureColorPalette (see PR #121's runtime API).
  private colorPalettes: Record<string, ColorPalette> = {};

  // jsEvaluator for the ModuleLoader. Called when a compiled module's lazy
  // Proxy is first accessed. Resolves via bootstrap modules (Init.js deps)
  // then moduleLoader factories (native module shims registered at startup).
  // Most modules are loaded by webpack's own require — loadJsModule only
  // handles modules requested via valdiRequire / moduleLoader.load().
  loadJsModule(
    relativePath: string,
    _requireFunc: unknown,
    module: { path: string; exports: unknown },
    _exports: unknown,
  ): void {
    relativePath = path.normalize(relativePath);
    module.path = relativePath;

    // 1. Bootstrap modules — statically imported so webpack always includes
    //    them, but lazily evaluated (the cache stores factories, not exports)
    //    so evaluation order respects Init.js's global setup (e.g. PostInit
    //    needs global.Long which Init.js installs partway through).
    const bootstrap = globalObject.__valdiBootstrapModules;
    if (bootstrap?.[relativePath]) {
      module.exports = bootstrap[relativePath]();
      return;
    }

    // 2. moduleLoader factories — native modules registered via shims or setup.ts
    const ml = globalObject.moduleLoader;
    if (ml?.hasModuleFactory?.(relativePath)) {
      module.exports = ml.load(relativePath, true) as unknown;
      return;
    }

    // 3. Dynamic-module registries — build-time generated maps for modules
    //    that are targets of dynamic require(variable) calls. Each registry
    //    covers one category: NavigationPage components, worker entry points.
    const navPages = globalObject.__valdiNavigationPages;
    if (navPages?.[relativePath]) {
      module.exports = navPages[relativePath]();
      return;
    }
    const workers = globalObject.__valdiWorkerModules;
    if (workers?.[relativePath]) {
      module.exports = workers[relativePath]();
      return;
    }

    if (globalObject.runtime?.isLoggingEnabled) {
      console.warn(`[ValdiWebRuntime] Module not found: ${relativePath}`);
    }
  }

  // NavigationPage component resolution. Parses "Symbol@FilePath" format
  // and resolves via the generated _navigation_registry.js (collapse_web_paths
  // greps compiled output for NavigationPage usage and emits explicit requires).
  requireByComponent(componentName: string): unknown {
    if (this.componentPaths.has(componentName)) {
      return this.componentPaths.get(componentName);
    }

    const atIdx = componentName.indexOf('@');
    if (atIdx >= 0) {
      const symbolName = componentName.substring(0, atIdx);
      const filePath = componentName.substring(atIdx + 1);

      const pages = globalObject.__valdiNavigationPages;
      if (pages?.[filePath]) {
        try {
          const mod = pages[filePath]();
          if (mod && mod[symbolName]) {
            this.componentPaths.set(componentName, mod[symbolName]);
            return mod[symbolName];
          }
        } catch (e) {
          // fall through to moduleLoader fallback
        }
      }

      // Fallback: try moduleLoader (for native module components)
      const ml = globalObject.moduleLoader;
      if (ml?.hasModuleFactory?.(filePath)) {
        const mod = ml.load(filePath, true) as ModuleExports;
        if (mod && mod[symbolName]) {
          this.componentPaths.set(componentName, mod[symbolName]);
          return mod[symbolName];
        }
      }
    }

    if (globalObject.runtime?.isLoggingEnabled) {
      console.error('could not find', componentName);
    }
    return undefined;
  }

  configureColorPalette(name: string, palette: ColorPalette): void {
    this.colorPalettes[name] = palette;
    globalObject.currentPalette = palette;
  }

  setActiveColorPalette(name: string): void {
    const palette = this.colorPalettes[name];
    if (palette !== undefined) {
      globalObject.currentPalette = palette;
    }
  }

  /** @deprecated Use configureColorPalette() and setActiveColorPalette() instead. */
  setColorPalette(palette: ColorPalette): void {
    this.configureColorPalette('default', palette);
    this.setActiveColorPalette('default');
  }

  getColorPalette(): ColorPalette | undefined {
    return globalObject.currentPalette;
  }

  getCurrentPlatform(): number {
    // 1 = Android, 2 = iOS, 3 = MacOS, 4 = Web
    return 4;
  }

  submitRawRenderRequest(_renderRequest: unknown): void {
    // console.log("submitRawRenderRequest", renderRequest);
  }

  createContext(_manager: unknown): string {
    // console.log("createContext", manager);
    return 'contextId';
  }

  setLayoutSpecs(_contextId: string, _width: number, _height: number, _rtl: boolean): void {
    // console.log("setLayoutSpecs", contextId, width, height, rtl);
  }

  // This renderer does not report asset loads; callers see no tracked assets.
  setViewNodeAssetTracker(_contextId: string, _callback: unknown): void {}

  postMessage(_contextId: string, _command: string, _params: unknown): void {
    // console.log("postMessage", contextId, command, params);
  }

  // Whole-catalog access — used by `loadCatalog(catalogPath)` callers that
  // need every asset in a module's res/. Per-component access flows
  // through __valdiResolveImage (compiler-injected static requires) and
  // bypasses this code path entirely.
  //
  // Resolution tiers (first hit wins):
  //   1. __valdiImageRegistry[catalogPath] — build-time map emitted by
  //      collapse_web_paths, populated when consumers `require` the
  //      module's _image_registry.js.
  //   2. __valdiImageContext — back-compat path for consumers that wire
  //      a webpack require.context directly (pre-PR4 behavior).
  getAssets(catalogPath: string): WebAsset[] {
    const registry = globalObject.__valdiImageRegistry?.[catalogPath];
    if (registry) {
      return Object.entries(registry).map(([key, value]) => ({
        path: key,
        src: (value as { default?: string } | undefined)?.default ?? (value as string),
      }));
    }
    const imgCtx = globalObject.__valdiImageContext;
    if (!imgCtx) {
      return [];
    }
    const prefix = `./${catalogPath}/`;
    const allKeys = imgCtx.keys();
    const filteredImages = allKeys.filter((key: string) => key.startsWith(prefix));
    return filteredImages.map((key: string) => ({
      path: path.basename(key).split('.').slice(0, -1).join('.'),
      src: (imgCtx(key) as { default?: string }).default || (imgCtx(key) as string),
    }));
  }

  makeAssetFromUrl(url: string): WebAsset {
    return {
      path: url,
      src: url,
      width: 100,
      height: 100,
    };
  }

  pushCurrentContext(_contextId: string | undefined): void {
    // console.log("pushCurrentContext", contextId);
  }

  popCurrentContext(): void {}

  getFrameForElementId(_contextId: string, _elementId: number, callback: (value: undefined) => void): void {
    callback(undefined);
  }

  getNativeViewForElementId(_contextId: string, _elementId: number, callback: (value: undefined) => void): void {
    callback(undefined);
  }

  getNativeNodeForElementId(_contextId: string, _elementId: number): undefined {
    return undefined;
  }

  makeOpaque<T>(object: T): T {
    return object;
  }

  configureCallback(_options: unknown, _func: unknown): void {}

  withCallbackAttribution<T>(_attribution: string, callback: () => T): T {
    return callback();
  }

  getViewNodeDebugInfo(_contextId: string, _elementId: number, callback: (value: undefined) => void): void {
    callback(undefined);
  }

  takeElementSnapshot(_contextId: string, _elementId: number, callback: (value: undefined) => void): void {
    callback(undefined);
  }

  getLayoutDebugInfo(_contextId: string, _elementId: number, callback: (value: undefined) => void): void {
    callback(undefined);
  }

  performSyncWithMainThread(func: () => void): void {
    func();
  }

  createWorker(_url: string): NativeWorker {
    return {
      postMessage(_data: unknown): void {},
      setOnMessage(_callback: unknown): void {},
      terminate(): void {},
    };
  }

  destroyContext(_contextId: string): void {}

  measureContext(
    _contextId: string,
    _maxWidth: number,
    _widthMode: number,
    _maxHeight: number,
    _heightMode: number,
    _rtl: boolean,
  ): [number, number] {
    return [0, 0];
  }

  getCSSModule(_path: string): { getRule(name: string): undefined } {
    return {
      getRule(_name: string): undefined {
        return undefined;
      },
    };
  }

  createCSSRule(_attributes: unknown): number {
    return 0;
  }

  internString(_str: string): number {
    return 0;
  }

  getAttributeId(_attributeName: string): number {
    return 0;
  }

  protectNativeRefs(_contextId: string): () => void {
    return () => {};
  }

  getBackendRenderingTypeForContextId(_contextId: string): number {
    return 1;
  }

  isModuleLoaded(_module: string): boolean {
    return true;
  }

  loadModule(_module: string, completion?: () => void): void {
    if (completion) completion();
  }

  // Stubbed — was using jsonContext (removed). Localized strings now use
  // _strings_preload.js generated by collapse_web_paths instead.
  getModuleEntry(_module: string, _pathStr: string, _asString: boolean): string {
    return '{}';
  }

  getModuleJsPaths(_module: string): string[] {
    return [''];
  }

  trace<T>(tag: string, callback: () => T): T {
    const handle = beginValdiWebTrace(tag);
    try {
      return callback();
    } finally {
      endValdiWebTrace(handle);
    }
  }

  makeTraceProxy(tag: string, callback: () => unknown): () => unknown {
    return makeValdiWebTraceProxy(tag, callback) as () => unknown;
  }

  startTraceRecording(): number {
    return 0;
  }

  stopTraceRecording(_id: number): unknown[] {
    return [];
  }

  callOnMainThread(method: (parameters: unknown) => void, parameters: unknown): void {
    method(parameters);
  }

  onMainThreadIdle(cb: () => void): void {
    requestIdleCallback(() => {
      cb();
    });
  }

  makeAssetFromBytes(bytes: ArrayBuffer | Uint8Array): WebAsset {
    const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    let binary = '';
    for (let i = 0; i < view.length; i++) {
      binary += String.fromCharCode(view[i]);
    }
    const src = 'data:image/png;base64,' + btoa(binary);
    return {
      path: '',
      src,
      width: 100,
      height: 100,
    };
  }

  makeDirectionalAsset(ltrAsset: string | WebAsset, rtlAsset: string | WebAsset): WebAsset {
    const isRtl = typeof document !== 'undefined' && document.dir === 'rtl';
    const asset = isRtl ? rtlAsset : ltrAsset;
    if (typeof asset === 'string') {
      return { path: asset, src: asset, width: 100, height: 100 };
    }
    return {
      path: asset?.path ?? '',
      src: asset?.src ?? asset?.path ?? '',
      width: asset?.width ?? 100,
      height: asset?.height ?? 100,
    };
  }

  makePlatformSpecificAsset(defaultAsset: string | WebAsset, _platformAssetOverrides: unknown): WebAsset {
    const asset = defaultAsset;
    if (typeof asset === 'string') {
      return { path: asset, src: asset, width: 100, height: 100 };
    }
    return {
      path: asset?.path ?? '',
      src: asset?.src ?? asset?.path ?? '',
      width: asset?.width ?? 100,
      height: asset?.height ?? 100,
    };
  }

  addAssetLoadObserver(
    _asset: unknown,
    _onLoad: unknown,
    _outputType: unknown,
    _preferredWidth?: number,
    _preferredHeight?: number,
  ): () => void {
    return () => {};
  }

  outputLog(_type: string, _content: string): void {
    //This should never be called, web is using the browser's console.log
  }

  scheduleWorkItem(cb: () => void, delayMs: number, _interruptible: boolean): number {
    const taskId = this._taskIdCounter++;
    const delay = delayMs || 0;
    const timing = globalObject.__originalTimingFunctions__;
    // Use the same native setTimeout/clearTimeout pair so cancellation always works
    // (window.setTimeout may be monkey-patched by Zone.js or others, returning IDs
    // that native clearTimeout would not recognize)
    const timeoutId = timing.setTimeout(() => {
      this._scheduledTasks.delete(taskId);
      try {
        cb();
      } catch (err) {
        this.onUncaughtError('scheduleWorkItem', err);
      }
    }, delay);
    this._scheduledTasks.set(taskId, timeoutId);
    return taskId;
  }

  unscheduleWorkItem(taskId: number): void {
    const timeoutId = this._scheduledTasks.get(taskId);
    if (timeoutId !== undefined) {
      globalObject.__originalTimingFunctions__.clearTimeout(timeoutId);
      this._scheduledTasks.delete(taskId);
    }
  }

  getCurrentContext(): string {
    return '';
  }

  saveCurrentContext(): number {
    return 0;
  }

  restoreCurrentContext(_contextId: number): void {}

  onUncaughtError(message: string, error: unknown): void {
    if (globalObject.runtime?.isLoggingEnabled) console.log('uncaught error', message, error);
  }

  setUncaughtExceptionHandler(_cb: unknown): void {}

  setUnhandledRejectionHandler(_cb: unknown): void {}

  dumpMemoryStatistics(): { memoryUsageBytes: number; objectsCount: number } {
    return {
      memoryUsageBytes: 0,
      objectsCount: 0,
    };
  }

  performGC(): void {
    // Not a thing on the web
  }

  dumpHeap(): ArrayBuffer {
    // Not a thing on the web
    return new ArrayBuffer(0);
  }

  bytesToString(bytes: ArrayBuffer | Uint8Array): string {
    const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    return new TextDecoder().decode(view);
  }

  submitDebugMessage(_level: string, _message: string): void {
    // Unused, should go through console.log
  }
}

globalObject.runtime = new Runtime();

// Capture original console before Init overwrites it
globalObject.__originalConsole__ = {
  log: console.log.bind(console),
  warn: console.warn.bind(console),
  error: console.error.bind(console),
  info: console.info.bind(console),
  debug: console.debug.bind(console),
  dir: console.dir.bind(console),
  trace: console.trace.bind(console),
  assert: console.assert.bind(console),
};
Object.freeze(globalObject.__originalConsole__);

/** Log to the real browser console even when Init has replaced global.console (e.g. during startup). */
globalObject.__valdiLogToConsole__ = function (...args: unknown[]): void {
  const c = globalObject.__originalConsole__ || globalObject.console;
  if (c?.log) c.log(...args);
};

// Capture native browser setTimeout/clearTimeout before Valdi replaces them (like we do for console)
// Bind to window so they can be called without a receiver (e.g. timing.setTimeout(...)) without
// throwing "Illegal invocation" on native functions that require window as `this`.
globalObject.__originalTimingFunctions__ = {
  setTimeout: window.setTimeout.bind(window),
  clearTimeout: window.clearTimeout.bind(window),
  setInterval: window.setInterval.bind(window),
  clearInterval: window.clearInterval.bind(window),
};
Object.freeze(globalObject.__originalTimingFunctions__);

// Bootstrap modules Init.js needs via loadJsModule. Statically required
// so webpack includes them, but wrapped in factories so evaluation is
// deferred until first access. Order matters: PostInit references the
// global `Long` that Init.js installs after loading the Long module, so
// PostInit MUST NOT evaluate at bundle-load time.
const bootstrapModules: ModuleRegistry = {
  ['valdi_core/src/ModuleLoader']: () => require('valdi_core/src/ModuleLoader') as ModuleExports,
  ['valdi_core/src/Long']: () => require('valdi_core/src/Long') as ModuleExports,
  ['valdi_core/src/tslib']: () => require('valdi_core/src/tslib') as ModuleExports,
  ['valdi_core/src/PostInit']: () => require('valdi_core/src/PostInit') as ModuleExports,
  // PostInit overwrites global.TextEncoder/TextDecoder when UnicodeNative registers.
  // It loads TextCoding via moduleLoader.load() — must be resolvable here.
  ['coreutils/src/unicode/TextCoding']: () => require('coreutils/src/unicode/TextCoding') as ModuleExports,
};
globalObject.__valdiBootstrapModules = bootstrapModules;

// Init.js creates moduleLoader and runs PostInit (which gates browser-specific
// globals internally). Uses standard module path — webpack resolves via
// resolve.modules config.
require('valdi_core/src/Init');

// Patch moduleLoader.onHotReload to handle undefined paths gracefully.
// Webpack's module objects don't have .path, so modules that call
// onHotReload(module, module.path, callback) would fail on web.
if (globalObject.moduleLoader) {
  const originalOnHotReload = globalObject.moduleLoader.onHotReload.bind(globalObject.moduleLoader);
  globalObject.moduleLoader.onHotReload = function (
    module: { path: string },
    modulePath: string,
    callback: () => void,
  ): () => void {
    if (!modulePath) {
      return () => {};
    }
    return originalOnHotReload(module, modulePath, callback);
  };
}

// Console/timing restoration removed — PostInit now gates console and
// timing overwrites internally (isBrowser check), so they're never
// overwritten on web in the first place.

export {};
