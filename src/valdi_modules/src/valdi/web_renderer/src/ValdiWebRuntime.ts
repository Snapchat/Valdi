import type { ColorPalette, ColorPaletteManager } from './core/Palette';
import type { ViewNodeTree } from './core/ViewNodeTree';
import {
  type IViewNodeAssetTracker,
  type ViewNodeAssetTrackerCallback,
  ViewNodeAssetTrackerEventType,
} from 'valdi_core/src/IViewNodeAssetTracker';
import {
  beginValdiWebTrace,
  endValdiWebTrace,
  instantValdiWebTrace,
  makeValdiWebTraceProxy,
} from './tracing/ValdiWebTracing';
import { setWebRendererLayoutDirection } from './WebRendererRoot';
import { createValdiWebWorker } from './ValdiWebWorker';

declare const require: {
  (id: string): any;
};
declare const __VALDI_API_VERSION__: number;

const path = require('path-browserify');
let cachedColorPaletteManager: ColorPaletteManager | undefined;
let cachedResolveAssetSourceUrl: ((source: unknown) => string | undefined) | undefined;
let cachedBase64FromByteArray: ((bytes: Uint8Array) => string) | undefined;
let cachedDetectImageMimeType: ((bytes: Uint8Array) => string) | undefined;
let cachedGetViewNodeTreeForContextId: ((contextId: string) => ViewNodeTree | undefined) | undefined;

const valdiGlobalThis = globalThis as any;
const originalTimingFunctions = {
  setTimeout: globalThis.setTimeout.bind(globalThis),
  clearTimeout: globalThis.clearTimeout.bind(globalThis),
};

// globalThis is the canonical web global. Keep `global` as a compatibility
// alias for older generated code and third-party modules that still read the
// Node spelling.
valdiGlobalThis.global = valdiGlobalThis;

// To make tests happy
valdiGlobalThis.describe = function (name: string, func: Function) {};

// Eager JS module context removed. Compiled modules now resolve lazily through:
// 1. __valdiBootstrapModules (statically imported essentials)
// 2. moduleLoader factories (registered native module shims)
// 3. generated navigation and worker registries.

function getColorPaletteManager(): ColorPaletteManager {
  if (cachedColorPaletteManager) {
    return cachedColorPaletteManager;
  }

  cachedColorPaletteManager = require('./core/Palette').COLOR_PALETTE_MANAGER as ColorPaletteManager;
  return cachedColorPaletteManager;
}

function stringToUtf8Bytes(value: string): Uint8Array {
  return new TextEncoder().encode(value);
}

function utf8BytesToString(value: Uint8Array): string {
  return new TextDecoder().decode(value);
}

function unwrapWebpackDefault(resourceModule: unknown): unknown {
  const source = resourceModule as { default?: unknown };
  return source && typeof source === 'object' && 'default' in source ? source.default : resourceModule;
}

function resourceModuleToByteArray(resourceModule: unknown): Uint8Array | undefined {
  const value = unwrapWebpackDefault(resourceModule);
  if (value instanceof Uint8Array) {
    return value;
  }
  if (value instanceof ArrayBuffer) {
    return new Uint8Array(value);
  }
  return undefined;
}

// Runtime bootstrap executes before moduleLoader exists, so dependencies are loaded lazily.
function resolveRuntimeAssetSourceUrl(source: unknown): string | undefined {
  if (!cachedResolveAssetSourceUrl) {
    cachedResolveAssetSourceUrl = require('./utils/assetSource').resolveAssetSourceUrl as (
      source: unknown,
    ) => string | undefined;
  }
  return cachedResolveAssetSourceUrl(source);
}

function runtimeBytesToBase64(bytes: Uint8Array): string {
  if (!cachedBase64FromByteArray) {
    cachedBase64FromByteArray = require('coreutils/src/Base64').Base64.fromByteArray as (bytes: Uint8Array) => string;
  }
  return cachedBase64FromByteArray(bytes);
}

function runtimeImageMimeType(bytes: Uint8Array): string {
  if (!cachedDetectImageMimeType) {
    cachedDetectImageMimeType = require('./utils/imageSource').detectImageMimeType as (bytes: Uint8Array) => string;
  }
  return cachedDetectImageMimeType(bytes);
}

function getViewNodeTreeForContextId(contextId: string): ViewNodeTree | undefined {
  if (!cachedGetViewNodeTreeForContextId) {
    cachedGetViewNodeTreeForContextId = require('./core/ViewNodeTree').ViewNodeTree.getForContextId as (
      contextId: string,
    ) => ViewNodeTree | undefined;
  }
  return cachedGetViewNodeTreeForContextId(contextId);
}

function resourceModuleToString(resourceModule: unknown): string {
  const resolved = resolveRuntimeAssetSourceUrl(resourceModule);
  if (resolved) {
    return resolved;
  }
  const bytes = resourceModuleToByteArray(resourceModule);
  if (bytes) {
    return utf8BytesToString(bytes);
  }
  const value = unwrapWebpackDefault(resourceModule);
  return typeof value === 'string' ? value : JSON.stringify(value);
}

function resourceModuleToBytes(resourceModule: unknown): Uint8Array {
  return resourceModuleToByteArray(resourceModule) ?? stringToUtf8Bytes(resourceModuleToString(resourceModule));
}

function getRegisteredModuleEntry(module: string, pathStr: string): unknown | undefined {
  const registry = valdiGlobalThis.__valdiModuleEntryRegistry;
  const factory = registry?.[module]?.[pathStr];
  if (factory) {
    return factory();
  }

  const context = valdiGlobalThis.__valdiModuleEntryContext;
  if (context) {
    const filePath = './' + module + '/' + pathStr;
    return context(filePath);
  }

  return undefined;
}

class Runtime {
  componentPaths = new Map();
  isDebugEnabled = true;
  // ConsoleLogTransformer guards use isLoggingEnabled.
  isLoggingEnabled = true;
  buildType = 'dev';
  // Standalone applications inject the version; npm packages carry the same build metadata.
  apiVersion =
    typeof __VALDI_API_VERSION__ === 'number'
      ? __VALDI_API_VERSION__
      : (require('../../valdi_api_version.json') as number);
  // Map of task IDs to timeout IDs for scheduleWorkItem
  private _taskIdCounter = 1;
  private _scheduledTasks = new Map<number, number>();
  // jsEvaluator for the ModuleLoader. Called when a compiled module's lazy
  // Proxy is first accessed. Resolves via bootstrap modules (Init.js deps),
  // moduleLoader factories (native module shims registered at startup), and
  // generated registries for dynamic module categories.
  loadJsModule(relativePath: string, requireFunc: any, module: any, exports: any) {
    relativePath = path.normalize(relativePath);
    module.path = relativePath;

    // 1. Bootstrap modules — statically imported so webpack always includes
    //    them, but lazily evaluated (the cache stores factories, not exports)
    //    so evaluation order respects Init.js's globalThis setup (e.g. PostInit
    //    needs Long which Init.js installs partway through).
    const bootstrap = valdiGlobalThis.__valdiBootstrapModules;
    if (bootstrap?.[relativePath]) {
      module.exports = bootstrap[relativePath]();
      return;
    }

    // 2. moduleLoader factories — native modules registered via shims or setup.ts
    const ml = valdiGlobalThis.moduleLoader;
    if (ml?.hasModuleFactory?.(relativePath)) {
      module.exports = ml.load(relativePath, true);
      return;
    }

    // 3. Dynamic-module registries — build-time generated maps for modules
    //    that are targets of dynamic require(variable) calls. Each registry
    //    covers one category: NavigationPage components, worker entry points.
    const navPages = valdiGlobalThis.__valdiNavigationPages;
    if (navPages?.[relativePath]) {
      module.exports = navPages[relativePath]();
      return;
    }
    const workers = valdiGlobalThis.__valdiWorkerModules;
    if (workers?.[relativePath]) {
      module.exports = workers[relativePath]();
      return;
    }

    if (valdiGlobalThis.runtime?.isLoggingEnabled) {
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

      const pages = valdiGlobalThis.__valdiNavigationPages;
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
      const ml = valdiGlobalThis.moduleLoader;
      if (ml?.hasModuleFactory?.(filePath)) {
        const mod = ml.load(filePath, true);
        if (mod && mod[symbolName]) {
          this.componentPaths.set(componentName, mod[symbolName]);
          return mod[symbolName];
        }
      }
    }

    if (valdiGlobalThis.runtime?.isLoggingEnabled) {
      console.error('could not find', componentName);
    }
  }

  configureColorPalette(name: string, palette: ColorPalette) {
    getColorPaletteManager().configureColorPalette(name, palette);
  }

  getColorPalette(name?: string) {
    return getColorPaletteManager().getColorPalette(name);
  }

  setActiveColorPalette(name: string) {
    getColorPaletteManager().setActiveColorPalette(name);
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
    return 'contextId';
  }

  setLayoutSpecs(contextId: string, _width: number, _height: number, rtl: boolean): void {
    setWebRendererLayoutDirection(contextId, rtl);
  }

  setViewNodeAssetTracker(contextId: string, callback: ViewNodeAssetTrackerCallback | undefined): void {
    const viewNodeTree = getViewNodeTreeForContextId(contextId);
    if (!viewNodeTree) {
      return;
    }

    const assetTracker: IViewNodeAssetTracker | undefined = callback
      ? {
          onAssetEvent(eventType, nodeId, error): void {
            callback(eventType, nodeId, error);
          },
          onBeganRequestingLoadedAsset(nodeId): void {
            callback(ViewNodeAssetTrackerEventType.beganRequestingLoadedAsset, nodeId, undefined);
          },
          onEndRequestingLoadedAsset(nodeId): void {
            callback(ViewNodeAssetTrackerEventType.endRequestingLoadedAsset, nodeId, undefined);
          },
          onLoadedAssetChanged(nodeId, error): void {
            callback(ViewNodeAssetTrackerEventType.loadedAssetChange, nodeId, error);
          },
        }
      : undefined;
    viewNodeTree.setAssetTracker(assetTracker);
  }

  postMessage(contextId: string, command: string, params: any) {
    // console.log("postMessage", contextId, command, params);
  }

  // Whole-catalog access — used by `loadCatalog(catalogPath)` callers that
  // need every asset in a module's res/. Per-component access flows
  // through __valdiResolveImage (compiler-injected static requires) and
  // bypasses this code path entirely.
  //
  // Resolution tiers (first hit wins):
  //   1. __valdiImageRegistry[catalogPath] — build-time map emitted by
  //      collapse_web_paths.
  //   2. __valdiImageContext — back-compat path for consumers that wire
  //      a webpack require.context directly (pre-PR4 behavior).
  getAssets(catalogPath: string) {
    const registry = valdiGlobalThis.__valdiImageRegistry?.[catalogPath];
    if (registry) {
      return Object.entries(registry).map(([k, v]: [string, any]) => ({
        path: k,
        src: v?.default ?? v,
      }));
    }

    const imgCtx = valdiGlobalThis.__valdiImageContext;
    if (imgCtx) {
      const prefix = `./${catalogPath}/`;
      const allKeys = imgCtx.keys();
      const filteredImages = allKeys.filter((key: string) => key.startsWith(prefix));
      return filteredImages.map((key: string) => ({
        path: path.basename(key).split('.').slice(0, -1).join('.'),
        src: imgCtx(key).default || imgCtx(key),
      }));
    }

    return [];
  }

  makeAssetFromUrl(url: string) {
    return {
      path: url,
      width: 100,
      height: 100,
      src: url,
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
    return createValdiWebWorker(url);
  }

  destroyContext(contextId: string) {}

  measureContext(
    contextId: string,
    maxWidth: number,
    widthMode: number,
    maxHeight: number,
    heightMode: number,
    rtl: boolean,
  ): [number, number] {
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

  getModuleEntry(module: string, pathStr: string, asString: boolean) {
    const resourceModule = getRegisteredModuleEntry(module, pathStr);
    if (resourceModule === undefined) {
      throw new Error(`Valdi module entry not found: ${module}/${pathStr}`);
    }
    return asString ? resourceModuleToString(resourceModule) : resourceModuleToBytes(resourceModule);
  }

  getModuleJsPaths(module: string) {
    return [''];
  }

  beginTrace(tag: string) {
    beginValdiWebTrace(tag);
  }

  endTrace() {
    endValdiWebTrace();
  }

  instantTrace(tag: string, args?: readonly unknown[]) {
    instantValdiWebTrace(tag, args);
  }

  makeTraceProxy(tag: string, callback: Function) {
    return makeValdiWebTraceProxy(tag, callback);
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
    if (typeof globalThis.requestIdleCallback === 'function') {
      globalThis.requestIdleCallback(() => {
        cb();
      });
    } else {
      globalThis.setTimeout(() => {
        cb();
      }, 0);
    }
  }

  makeAssetFromBytes(bytes: ArrayBuffer | Uint8Array) {
    const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    return {
      path: '',
      width: 100,
      height: 100,
      src: `data:${runtimeImageMimeType(view)};base64,${runtimeBytesToBase64(view)}`,
    };
  }

  makeDirectionalAsset(ltrAsset: any, rtlAsset: any) {
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

  makePlatformSpecificAsset(defaultAsset: any, platformAssetOverrides: any) {
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
    asset: any,
    onLoad: Function,
    outputType: any,
    preferredWidth?: number,
    preferredHeight?: number,
  ) {
    return () => {};
  }

  outputLog(type: string, content: string) {
    //This should never be called, web is using the browser's console.log
  }

  scheduleWorkItem(cb: Function, delayMs: number, interruptible: boolean) {
    const taskId = this._taskIdCounter++;
    const delay = delayMs || 0;
    const timeoutId = originalTimingFunctions.setTimeout(() => {
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
      originalTimingFunctions.clearTimeout(timeoutId);
      this._scheduledTasks.delete(taskId);
    }
  }

  getCurrentContext() {
    return '';
  }

  saveCurrentContext() {
    return 0;
  }

  restoreCurrentContext(contextId: number) {}

  onUncaughtError(message: string, error: any) {
    if (valdiGlobalThis.runtime?.isLoggingEnabled) {
      console.log('uncaught error', message, error);
    }
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
}

valdiGlobalThis.runtime = new Runtime();

// Collapsed web packages generate these files for explicit webpack-visible
// runtime lookup. Non-collapsed test environments may omit them.
try {
  require('../../_image_registry');
} catch (error) {}

try {
  require('./_module_entry_registry');
} catch (error) {}

// Bootstrap modules Init.js needs via loadJsModule. Statically required
// so webpack includes them, but wrapped in factories so evaluation is
// deferred until first access. Order matters: PostInit references the `Long`
// that Init.js installs after loading the Long module, so PostInit MUST NOT
// evaluate at bundle-load time.
const _bootstrapModules: Record<string, () => unknown> = {
  'valdi_core/src/ModuleLoader': () => require('valdi_core/src/ModuleLoader'),
  'valdi_core/src/Long': () => require('valdi_core/src/Long'),
  'valdi_core/src/tslib': () => require('valdi_core/src/tslib'),
  'valdi_core/src/PostInit': () => require('valdi_core/src/PostInit'),
  'valdi_core/src/Console': () => require('valdi_core/src/Console'),
  'valdi_core/src/PromisePolyfill': () => require('valdi_core/src/PromisePolyfill'),
  'valdi_core/src/TsnHelper': () => require('valdi_core/src/TsnHelper'),
  // PostInit overwrites globalThis.TextEncoder/TextDecoder when UnicodeNative registers.
  // It loads TextCoding via moduleLoader.load() — must be resolvable here.
  'coreutils/src/unicode/TextCoding': () => require('coreutils/src/unicode/TextCoding'),
};
valdiGlobalThis.__valdiBootstrapModules = _bootstrapModules;

// Init.js creates moduleLoader and runs PostInit (which gates browser-specific
// globals internally). Uses standard module path — webpack resolves via
// resolve.modules config.
require('valdi_core/src/Init');

// Collapsed web packages generate native-module registration next to the
// runtime. Loading it here keeps registration internal to runtime bootstrap so
// host apps can import Valdi components/renderers without explicit setup.
require('../../RegisterNativeModules');

// Patch moduleLoader.onHotReload to handle undefined paths gracefully.
// Webpack's module objects don't have .path, so modules that call
// onHotReload(module, module.path, callback) would fail on web.
if (valdiGlobalThis.moduleLoader) {
  const originalOnHotReload = valdiGlobalThis.moduleLoader.onHotReload.bind(valdiGlobalThis.moduleLoader);
  valdiGlobalThis.moduleLoader.onHotReload = function (module: any, modulePath: string, callback: () => void) {
    if (!modulePath) {
      return () => {};
    }
    return originalOnHotReload(module, modulePath, callback);
  };
}

// Console/timing restoration removed — PostInit now gates console and
// timing overwrites internally (isWeb check), so they're never
// overwritten on web in the first place.
