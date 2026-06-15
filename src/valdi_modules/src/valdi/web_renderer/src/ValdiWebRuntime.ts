// Declare webpack require.context
declare const require: {
  (id: string): any;
  // 'mode' param enables webpack 'weak' context (lookup-only, no bundling)
  context(directory: string, useSubdirectories: boolean, regExp: RegExp, mode?: string): any;
};

// Declare global for Node-like environment
declare const global: any;

// Declare global timing functions
declare global {
  var __originalTimingFunctions__: {
    setTimeout: typeof setTimeout;
    clearTimeout: typeof clearTimeout;
    setInterval: typeof setInterval;
    clearInterval: typeof clearInterval;
  };
}

const path = require('path-browserify');

// Valdi runtime assumes global instead of globalThis
(globalThis as any).global = globalThis;

// To make tests happy
(globalThis as any).describe = function(name: string, func: Function) {};

// Eager context removed — modules are now resolved lazily via:
// 1. __valdiBootstrapModules (statically imported essentials)
// 2. moduleLoader factories (registerModule'd native modules)

class Runtime {
  componentPaths = new Map();
  isDebugEnabled = true;
  // ConsoleLogTransformer guards use runtime.isLoggingEnabled
  isLoggingEnabled = true;
  buildType = "debug";
  // Map of task IDs to timeout IDs for scheduleWorkItem
  private _taskIdCounter = 1;
  private _scheduledTasks = new Map<number, number>();

  // jsEvaluator for the ModuleLoader. Called when a compiled module's lazy
  // Proxy is first accessed. Resolves via bootstrap modules (Init.js deps)
  // then moduleLoader factories (native module shims registered at startup).
  // Most modules are loaded by webpack's own require — loadJsModule only
  // handles modules requested via valdiRequire / moduleLoader.load().
  loadJsModule(relativePath: string, requireFunc: any, module: any, exports: any) {
    relativePath = path.normalize(relativePath);
    module.path = relativePath;

    // 1. Bootstrap modules — statically imported so webpack always includes
    //    them, but lazily evaluated (the cache stores factories, not exports)
    //    so evaluation order respects Init.js's global setup (e.g. PostInit
    //    needs global.Long which Init.js installs partway through).
    const bootstrap = (globalThis as any).__valdiBootstrapModules;
    if (bootstrap?.[relativePath]) {
      module.exports = bootstrap[relativePath]();
      return;
    }

    // 2. moduleLoader factories — native modules registered via shims or setup.ts
    const ml = (global as any).moduleLoader;
    if (ml?.hasModuleFactory?.(relativePath)) {
      module.exports = ml.load(relativePath, true);
      return;
    }

    // 3. Dynamic-module registries — build-time generated maps for modules
    //    that are targets of dynamic require(variable) calls. Each registry
    //    covers one category: NavigationPage components, worker entry points.
    const navPages = (globalThis as any).__valdiNavigationPages;
    if (navPages?.[relativePath]) {
      module.exports = navPages[relativePath]();
      return;
    }
    const workers = (globalThis as any).__valdiWorkerModules;
    if (workers?.[relativePath]) {
      module.exports = workers[relativePath]();
      return;
    }

    if ((globalThis as any).runtime?.isLoggingEnabled) {
      console.warn(`[ValdiWebRuntime] Module not found: ${relativePath}`);
    }
  }

  // NavigationPage component resolution. Parses "Symbol@FilePath" format
  // and resolves via the generated _navigation_registry.js (collapse_web_paths
  // greps compiled output for NavigationPage usage and emits explicit requires).
  requireByComponent(componentName: string) {
    if (this.componentPaths.has(componentName)) {
      return this.componentPaths.get(componentName);
    }

    const atIdx = componentName.indexOf('@');
    if (atIdx >= 0) {
      const symbolName = componentName.substring(0, atIdx);
      const filePath = componentName.substring(atIdx + 1);

      const pages = (globalThis as any).__valdiNavigationPages;
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
      const ml = (global as any).moduleLoader;
      if (ml?.hasModuleFactory?.(filePath)) {
        const mod = ml.load(filePath, true);
        if (mod && mod[symbolName]) {
          this.componentPaths.set(componentName, mod[symbolName]);
          return mod[symbolName];
        }
      }
    }

    if ((globalThis as any).runtime?.isLoggingEnabled) {
      console.error("could not find", componentName);
    }
  }

  setColorPalette(palette: any) {
    (global as any).currentPalette = palette;
  }

  getColorPalette() {
    return (global as any).currentPalette;
  }

  getCurrentPlatform() {
    // 1 = Android, 2 = iOS, 3 = MacOS, 4 = Web
    return 4;
  }

  submitRawRenderRequest(renderRequest: any) {
    // console.log("submitRawRenderRequest", renderRequest);
  }

  createContext(manager: any) {
    // console.log("createContext", manager);
    return "contextId";
  }

  setLayoutSpecs(contextId: string, width: number, height: number, rtl: boolean) {
    // console.log("setLayoutSpecs", contextId, width, height, rtl);
  }

  postMessage(contextId: string, command: string, params: any) {
    // console.log("postMessage", contextId, command, params);
  }

  // Image context moved from hardcoded require.context to __valdiImageContext,
  // set by playground setup.ts. Decouples image bundling from ValdiWebRuntime.
  getAssets(catalogPath: string) {
    const imgCtx = (globalThis as any).__valdiImageContext;
    if (!imgCtx) {
      return [];
    }
    const prefix = `./${catalogPath}/`;
    const allKeys = imgCtx.keys();
    const filteredImages = allKeys.filter((key: string) => key.startsWith(prefix));
    return filteredImages.map((key: string) => ({
      path: path.basename(key).split('.').slice(0, -1).join('.'),
      src: imgCtx(key).default || imgCtx(key),
    }));
  }

  makeAssetFromUrl(url: string) {
    return {
      path: url,
      src: url,
      width: 100,
      height: 100,
    };
  }

  pushCurrentContext(contextId: string) {
    // console.log("pushCurrentContext", contextId);
  }

  popCurrentContext() {}

  getFrameForElementId(contextId: string, elementId: number, callback: Function) {
    callback(undefined);
  }

  getNativeViewForElementId(contextId: string, elementId: number, callback: Function) {
    callback(undefined);
  }

  getNativeNodeForElementId(contextId: string, elementId: number) {
    return undefined;
  }

  makeOpaque(object: any) {
    return object;
  }

  configureCallback(options: any, func: Function) {}

  getViewNodeDebugInfo(contextId: string, elementId: number, callback: Function) {
    callback(undefined);
  }

  takeElementSnapshot(contextId: string, elementId: number, callback: Function) {
    callback(undefined);
  }

  getLayoutDebugInfo(contextId: string, elementId: number, callback: Function) {
    callback(undefined);
  }

  performSyncWithMainThread(func: Function) {
    func();
  }

  createWorker(url: string) {
    return {
      postMessage(data: any) {},
      setOnMessage(f: Function) {},
      terminate() {},
    };
  }

  destroyContext(contextId: string) {}

  measureContext(contextId: string, maxWidth: number, widthMode: number, maxHeight: number, heightMode: number, rtl: boolean): [number, number] {
    return [0, 0];
  }

  getCSSModule(path: string) {
    return {
      getRule(name: string) {
        return undefined;
      },
    };
  }

  createCSSRule(attributes: any) {
    return 0;
  }

  internString(str: string) {
    return 0;
  }

  getAttributeId(attributeName: string) {
    return 0;
  }

  protectNativeRefs(contextId: string) {
    return () => {};
  }

  getBackendRenderingTypeForContextId(contextId: string) {
    return 1;
  }

  isModuleLoaded(module: string) {
    return true;
  }

  loadModule(module: string, completion?: Function) {
    if (completion) completion();
  }

  // Stubbed — was using jsonContext (removed). Localized strings now use
  // _strings_preload.js generated by collapse_web_paths instead.
  getModuleEntry(module: string, pathStr: string, asString: boolean) {
    return '{}';
  }

  getModuleJsPaths(module: string) {
    return [""];
  }

  trace(tag: string, callback: Function) {
    return callback();
  }

  makeTraceProxy(tag: string, callback: Function) {
    // Return callback directly to avoid adding stack frames
    return callback;
  }

  startTraceRecording() {
    return 0;
  }

  stopTraceRecording(id: number) {
    return [];
  }

  callOnMainThread(method: Function, parameters: any) {
    method(parameters);
  }

  onMainThreadIdle(cb: Function) {
    requestIdleCallback(() => {
      cb();
    });
  }

  makeAssetFromBytes(bytes: ArrayBuffer) {
    const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    let binary = '';
    for (let i = 0; i < view.length; i++) {
      binary += String.fromCharCode(view[i]);
    }
    const src = 'data:image/png;base64,' + btoa(binary);
    return {
      path: "",
      src,
      width: 100,
      height: 100,
    };
  }

  makeDirectionalAsset(ltrAsset: any, rtlAsset: any) {
    const isRtl = typeof document !== 'undefined' && document.dir === 'rtl';
    const asset = isRtl ? rtlAsset : ltrAsset;
    if (typeof asset === 'string') {
      return { path: asset, src: asset, width: 100, height: 100 };
    }
    return {
      path: asset?.path ?? "",
      src: asset?.src ?? asset?.path ?? "",
      width: asset?.width ?? 100,
      height: asset?.height ?? 100,
    };
  }

  makePlatformSpecificAsset(defaultAsset: any, platformAssetOverrides: any) {
    const asset = defaultAsset;
    if (typeof asset === 'string') {
      return { path: asset, src: asset, width: 100, height: 100 };
    }
    return {
      path: asset?.path ?? "",
      src: asset?.src ?? asset?.path ?? "",
      width: asset?.width ?? 100,
      height: asset?.height ?? 100,
    };
  }

  addAssetLoadObserver(asset: any, onLoad: Function, outputType: any, preferredWidth?: number, preferredHeight?: number) {
    return () => {};
  }

  outputLog(type: string, content: string) {
    //This should never be called, web is using the browser's console.log
  }

  scheduleWorkItem(cb: Function, delayMs: number, interruptible: boolean) {
    const taskId = this._taskIdCounter++;
    const delay = delayMs || 0;
    const timing = (globalThis as any).__originalTimingFunctions__;
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

  unscheduleWorkItem(taskId: number) {
    const timeoutId = this._scheduledTasks.get(taskId);
    if (timeoutId !== undefined) {
      (globalThis as any).__originalTimingFunctions__.clearTimeout(timeoutId);
      this._scheduledTasks.delete(taskId);
    }
  }

  getCurrentContext() {
    return "";
  }

  saveCurrentContext() {
    return 0;
  }

  restoreCurrentContext(contextId: number) {}

  onUncaughtError(message: string, error: any) {
    if ((globalThis as any).runtime?.isLoggingEnabled) console.log("uncaught error", message, error);
  }

  setUncaughtExceptionHandler(cb: Function) {}

  setUnhandledRejectionHandler(cb: Function) {}

  dumpMemoryStatistics() {
    return {
      memoryUsageBytes: 0,
      objectsCount: 0,
    };
  }

  performGC() {
    // Not a thing on the web
  }

  dumpHeap() {
    // Not a thing on the web
    return new ArrayBuffer(0);
  }

  bytesToString(bytes: ArrayBuffer | Uint8Array) {
    const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    return new TextDecoder().decode(view);
  }

  submitDebugMessage(level: string, message: string) {
    // Unused, should go through console.log
  }
};

const globalAny = globalThis as any;
globalAny.runtime = new Runtime();

// Capture original console before Init overwrites it
globalAny.__originalConsole__ = {
  log: console.log.bind(console),
  warn: console.warn.bind(console),
  error: console.error.bind(console),
  info: console.info.bind(console),
  debug: console.debug.bind(console),
  dir: console.dir.bind(console),
  trace: console.trace.bind(console),
  assert: console.assert.bind(console),
};
Object.freeze(globalAny.__originalConsole__);

/** Log to the real browser console even when Init has replaced global.console (e.g. during startup). */
globalAny.__valdiLogToConsole__ = function (...args: unknown[]) {
  const c = globalAny.__originalConsole__ || globalAny.console;
  if (c?.log) c.log.apply(c, args);
};

// Capture native browser setTimeout/clearTimeout before Valdi replaces them (like we do for console)
// Bind to window so they can be called without a receiver (e.g. timing.setTimeout(...)) without
// throwing "Illegal invocation" on native functions that require window as `this`.
(globalThis as any).__originalTimingFunctions__ = {
  setTimeout: window.setTimeout.bind(window),
  clearTimeout: window.clearTimeout.bind(window),
  setInterval: window.setInterval.bind(window),
  clearInterval: window.clearInterval.bind(window),
};
Object.freeze((globalThis as any).__originalTimingFunctions__);

// Bootstrap modules Init.js needs via loadJsModule. Statically required
// so webpack includes them, but wrapped in factories so evaluation is
// deferred until first access. Order matters: PostInit references the
// global `Long` that Init.js installs after loading the Long module, so
// PostInit MUST NOT evaluate at bundle-load time.
const _bootstrapModules: Record<string, () => unknown> = {
  'valdi_core/src/ModuleLoader': () => require('valdi_core/src/ModuleLoader'),
  'valdi_core/src/Long': () => require('valdi_core/src/Long'),
  'valdi_core/src/tslib': () => require('valdi_core/src/tslib'),
  'valdi_core/src/PostInit': () => require('valdi_core/src/PostInit'),
  // PostInit overwrites global.TextEncoder/TextDecoder when UnicodeNative registers.
  // It loads TextCoding via moduleLoader.load() — must be resolvable here.
  'coreutils/src/unicode/TextCoding': () => require('coreutils/src/unicode/TextCoding'),
};
(globalThis as any).__valdiBootstrapModules = _bootstrapModules;

// Init.js creates moduleLoader and runs PostInit (which gates browser-specific
// globals internally). Uses standard module path — webpack resolves via
// resolve.modules config.
const initModule = require("valdi_core/src/Init");

// Patch moduleLoader.onHotReload to handle undefined paths gracefully.
// Webpack's module objects don't have .path, so modules that call
// onHotReload(module, module.path, callback) would fail on web.
if (globalAny.moduleLoader) {
  const originalOnHotReload = globalAny.moduleLoader.onHotReload.bind(globalAny.moduleLoader);
  globalAny.moduleLoader.onHotReload = function(module: any, modulePath: string, callback: () => void) {
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
