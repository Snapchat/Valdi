import 'jasmine/src/jasmine';
import { Asset, PlatformAssetOverrides, ThemableAssetMap } from 'valdi_core/src/Asset';
import { ElementId } from 'valdi_core/src/IRenderedElement';
import { IRootComponentsManager } from 'valdi_core/src/IRootComponentsManager';
import { ViewNodeAssetTrackerCallback } from 'valdi_core/src/IViewNodeAssetTracker';
import { RenderRequest } from 'valdi_core/src/RenderRequest';
import {
  AssetEntry,
  BackendRenderingType,
  ColorPalette,
  CSSModuleNative,
  CSSRuleNative,
  ExceptionHandlerResult,
  LoadedAsset,
  LoadedAssetMetadata,
  NativeViewNodeInfo,
  NativeWorker,
  RuntimeMemoryStatistics,
  ValdiRuntime,
} from 'valdi_core/src/ValdiRuntime';
import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';
import { SubmitDebugMessageFunc } from 'valdi_core/src/debugging/DebugMessage';
import { AnyFunction } from 'valdi_core/src/utils/Callback';
import { PropertyList } from 'valdi_core/src/utils/PropertyList';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { NativeNode } from 'valdi_tsx/src/NativeNode';
import { NativeView } from 'valdi_tsx/src/NativeView';

/**
 * The public `runtime` API, pinned. Module authors outside this repo call these members, so
 * removing one or changing its signature is a breaking change even when nothing here uses it.
 *
 * If `RuntimeApiMismatch` below fails to compile, it names the members that differ. For an
 * intentional change: update this interface, cover the member in the specs below, and treat
 * removals and signature changes as breaking.
 */
// `any` is what the declaration uses; the exact-equality check below tells it apart from `unknown`.
/* eslint-disable @typescript-eslint/no-explicit-any */
interface PinnedValdiRuntime {
  apiVersion: number;
  submitDebugMessage: SubmitDebugMessageFunc;
  isDebugEnabled: boolean;
  isLoggingEnabled: boolean;
  useTopDownMoveOrder: boolean;
  enableModuleUnloadWorklist?: boolean;
  buildType: string;

  // RuntimeBase
  bytesToString(bytes: Uint8Array | ArrayBuffer): string;

  postMessage(contextId: string, command: string, params: any): void;
  getFrameForElementId(
    contextId: string,
    elementId: ElementId,
    callback: (frame: ElementFrame | undefined) => void,
  ): void;
  getNativeViewForElementId(
    contextId: string,
    elementId: ElementId,
    callback: (view: NativeView | undefined) => void,
  ): void;
  getNativeNodeForElementId(contextId: string, elementId: ElementId): NativeNode | undefined;
  makeOpaque(object: any): any;
  configureCallback<F extends AnyFunction>(options: number, func: F): void;
  makeANRAttributionProxy?<F extends AnyFunction>(attribution: string, func: F): F;
  withCallbackAttribution<T>(attribution: string, callback: () => T): T;
  getViewNodeDebugInfo(
    contextId: string,
    elementId: ElementId,
    callback: (viewNode: NativeViewNodeInfo | undefined) => void,
  ): void;
  takeElementSnapshot(contextId: string, elementId: ElementId, callback: (snapshot: string | undefined) => void): void;
  getLayoutDebugInfo(contextId: string, elementId: ElementId, callback: (layoutInfo: string | undefined) => void): void;
  performSyncWithMainThread(func: () => void): void;
  createWorker(url: string): NativeWorker;

  submitRawRenderRequest(renderRequest: RenderRequest, callback: () => void): void;

  createContext(manager?: IRootComponentsManager): string;
  destroyContext(contextId: string): void;
  setLayoutSpecs(contextId: string, width: number, height: number, rtl: boolean): void;
  setViewNodeAssetTracker(contextId: string, callback: ViewNodeAssetTrackerCallback | undefined): void;
  measureContext(
    contextId: string,
    maxWidth: number,
    widthMode: number,
    maxHeight: number,
    heightMode: number,
    rtl: boolean,
  ): [number, number];

  getCSSModule(path: string): CSSModuleNative;
  createCSSRule(attributes: PropertyList): CSSRuleNative;

  getCurrentPlatform(): number;
  internString(str: string): number;
  getAttributeId(attributeName: string): number;

  protectNativeRefs(contextId: string | number): () => void;

  getBackendRenderingTypeForContextId(contextId: string | number): BackendRenderingType;

  isModuleLoaded(module: string): boolean;
  loadModule(module: string, completion?: (error?: string) => void): void;
  getModuleEntry(module: string, path: string, asString: boolean): Uint8Array | string;
  getModuleJsPaths(module: string): string[];

  trace(tag: string, callback: () => any): any;
  makeTraceProxy(tag: string, callback: () => void): () => void;
  beginTrace?(tag: string): void;
  endTrace?(): void;
  instantTrace?(tag: string, args?: readonly unknown[]): void;

  startTraceRecording(): number;
  stopTraceRecording(id: number): any[];
  stopTraceRecordingWithStats?(id: number): any[];

  callOnMainThread(method: any, parameters: any[]): void;
  onMainThreadIdle(cb: () => void): void;

  makeAssetFromUrl(url: string): Asset;
  makeAssetFromBytes(bytes: ArrayBuffer | Uint8Array): Asset;
  makeDirectionalAsset(ltrAsset: string | Asset, rtlAsset: string | Asset): Asset;
  makePlatformSpecificAsset(defaultAsset: string | Asset, platformAssetOverrides: PlatformAssetOverrides): Asset;
  makeThemableAsset(assetsByColorPalette: ThemableAssetMap): Asset;
  getAssets(catalogPath: string): AssetEntry[];
  addAssetLoadObserver(
    asset: string | Asset,
    onLoad: (loadedAsset: LoadedAsset | Uint8Array | undefined, error: string | undefined) => void,
    outputType: number,
    preferredWidth: number | undefined,
    preferredHeight: number | undefined,
  ): () => void;
  getLoadedAssetMetadata(loadedAsset: LoadedAsset): LoadedAssetMetadata | undefined;

  setColorPalette(colorPalette: ColorPalette): void;
  configureColorPalette(name: string, colorPalette: ColorPalette): void;
  setActiveColorPalette(name: string): void;

  outputLog(type: number, content: string): void;

  scheduleWorkItem(cb: () => void, delayMs?: number, interruptible?: boolean): number;
  unscheduleWorkItem(taskId: number): void;

  pushCurrentContext(contextId: string | undefined): void;
  popCurrentContext(): void;
  getCurrentContext(): string;

  saveCurrentContext(): number;
  restoreCurrentContext(contextId: number): void;

  onUncaughtError(message: string, error: Error): void;
  setUncaughtExceptionHandler(cb: ((error: unknown, contextId: string) => ExceptionHandlerResult) | undefined): void;
  setUnhandledRejectionHandler(
    cb: ((promiseResult: unknown, contextId: string) => ExceptionHandlerResult) | undefined,
  ): void;

  dumpMemoryStatistics(): RuntimeMemoryStatistics;

  performGC(): void;

  newWeakRef(target: object): unknown;
  derefWeakRef(handle: unknown): object | undefined;

  dumpHeap?(): ArrayBuffer;
}
/* eslint-enable @typescript-eslint/no-explicit-any */

// Exact type equality: unlike assignability, it catches dropped parameters, widened return types
// and `any` creeping in.
type Equals<A, B> = (<T>() => T extends A ? 1 : 2) extends <T>() => T extends B ? 1 : 2 ? true : false;

type AllMembers = keyof ValdiRuntime | keyof PinnedValdiRuntime;

// Pick keeps optionality and readonly, so making a member optional also counts as a change.
type RuntimeApiMismatch = {
  [K in AllMembers]: K extends keyof ValdiRuntime
    ? K extends keyof PinnedValdiRuntime
      ? Equals<Pick<ValdiRuntime, K>, Pick<PinnedValdiRuntime, K>> extends true
        ? never
        : K
      : K
    : K;
}[AllMembers];

// A compile error here reads "Type 'true' is not assignable to type '<member names>'".
const runtimeApiMatchesPinnedApi: [RuntimeApiMismatch] extends [never] ? true : RuntimeApiMismatch = true;

// Members whose binding is compiled out of some native builds or not implemented natively at all.
// They stay pinned above; the specs only check them when present.
const OPTIONAL_AT_RUNTIME: ReadonlySet<AllMembers> = new Set<AllMembers>([
  // Only bound when tracing is compiled in.
  'trace',
  'makeTraceProxy',
  // Declared optional; only some runtimes implement them.
  'makeANRAttributionProxy',
  'beginTrace',
  'endTrace',
  'instantTrace',
  'stopTraceRecordingWithStats',
  'dumpHeap',
  'enableModuleUnloadWorklist',
]);

// Every member, so the runtime check below can't drift from the pinned interface: a key missing
// here or an extra one fails to compile.
const ALL_MEMBERS: { [K in keyof PinnedValdiRuntime]-?: 'function' | 'number' | 'boolean' | 'string' } = {
  bytesToString: 'function',
  apiVersion: 'number',
  postMessage: 'function',
  getFrameForElementId: 'function',
  getNativeViewForElementId: 'function',
  getNativeNodeForElementId: 'function',
  makeOpaque: 'function',
  configureCallback: 'function',
  makeANRAttributionProxy: 'function',
  withCallbackAttribution: 'function',
  getViewNodeDebugInfo: 'function',
  takeElementSnapshot: 'function',
  getLayoutDebugInfo: 'function',
  performSyncWithMainThread: 'function',
  createWorker: 'function',
  submitRawRenderRequest: 'function',
  createContext: 'function',
  destroyContext: 'function',
  setLayoutSpecs: 'function',
  setViewNodeAssetTracker: 'function',
  measureContext: 'function',
  getCSSModule: 'function',
  createCSSRule: 'function',
  getCurrentPlatform: 'function',
  internString: 'function',
  getAttributeId: 'function',
  protectNativeRefs: 'function',
  getBackendRenderingTypeForContextId: 'function',
  isModuleLoaded: 'function',
  loadModule: 'function',
  getModuleEntry: 'function',
  getModuleJsPaths: 'function',
  trace: 'function',
  makeTraceProxy: 'function',
  beginTrace: 'function',
  endTrace: 'function',
  instantTrace: 'function',
  startTraceRecording: 'function',
  stopTraceRecording: 'function',
  stopTraceRecordingWithStats: 'function',
  submitDebugMessage: 'function',
  callOnMainThread: 'function',
  onMainThreadIdle: 'function',
  makeAssetFromUrl: 'function',
  makeAssetFromBytes: 'function',
  makeDirectionalAsset: 'function',
  makePlatformSpecificAsset: 'function',
  makeThemableAsset: 'function',
  getAssets: 'function',
  addAssetLoadObserver: 'function',
  getLoadedAssetMetadata: 'function',
  setColorPalette: 'function',
  configureColorPalette: 'function',
  setActiveColorPalette: 'function',
  outputLog: 'function',
  scheduleWorkItem: 'function',
  unscheduleWorkItem: 'function',
  pushCurrentContext: 'function',
  popCurrentContext: 'function',
  getCurrentContext: 'function',
  saveCurrentContext: 'function',
  restoreCurrentContext: 'function',
  onUncaughtError: 'function',
  setUncaughtExceptionHandler: 'function',
  setUnhandledRejectionHandler: 'function',
  dumpMemoryStatistics: 'function',
  performGC: 'function',
  newWeakRef: 'function',
  derefWeakRef: 'function',
  dumpHeap: 'function',
  isDebugEnabled: 'boolean',
  isLoggingEnabled: 'boolean',
  useTopDownMoveOrder: 'boolean',
  enableModuleUnloadWorklist: 'boolean',
  buildType: 'string',
};

const MODULE_NAME = 'runtime_api_surface';

function waitFor(register: (done: () => void) => void): Promise<void> {
  return new Promise<void>(resolve => register(resolve));
}

describe('ValdiRuntime public API', () => {
  const runtime: ValdiRuntime = getValdiRuntime();

  it('matches the pinned API at compile time', () => {
    expect(runtimeApiMatchesPinnedApi).toBeTrue();
  });

  it('installs every pinned member on the native runtime', () => {
    const missing: string[] = [];
    for (const key of Object.keys(ALL_MEMBERS) as (keyof PinnedValdiRuntime)[]) {
      const actual = typeof (runtime as unknown as Record<string, unknown>)[key];
      if (actual === 'undefined' && OPTIONAL_AT_RUNTIME.has(key)) {
        continue;
      }
      if (actual !== ALL_MEMBERS[key]) {
        missing.push(`${key}: expected ${ALL_MEMBERS[key]}, got ${actual}`);
      }
    }
    expect(missing).toEqual([]);
  });

  describe('properties', () => {
    it('exposes runtime configuration', () => {
      const apiVersion: number = runtime.apiVersion;
      const isDebugEnabled: boolean = runtime.isDebugEnabled;
      const isLoggingEnabled: boolean = runtime.isLoggingEnabled;
      const useTopDownMoveOrder: boolean = runtime.useTopDownMoveOrder;
      const enableModuleUnloadWorklist: boolean | undefined = runtime.enableModuleUnloadWorklist;
      const buildType: string = runtime.buildType;

      expect(apiVersion).toBeGreaterThanOrEqual(0);
      expect(typeof isDebugEnabled).toBe('boolean');
      expect(typeof isLoggingEnabled).toBe('boolean');
      expect(typeof useTopDownMoveOrder).toBe('boolean');
      expect(enableModuleUnloadWorklist === undefined || typeof enableModuleUnloadWorklist === 'boolean').toBeTrue();
      expect(buildType.length).toBeGreaterThan(0);
    });
  });

  describe('strings and values', () => {
    it('bytesToString', () => {
      expect(runtime.bytesToString(new Uint8Array([104, 105]))).toBe('hi');
      expect(runtime.bytesToString(new Uint8Array([104, 105]).buffer)).toBe('hi');
    });

    it('internString and getAttributeId', () => {
      const id: number = runtime.internString('runtime_api_surface');
      expect(runtime.internString('runtime_api_surface')).toBe(id);
      const attributeId: number = runtime.getAttributeId('opacity');
      expect(runtime.getAttributeId('opacity')).toBe(attributeId);
    });

    it('makeOpaque', () => {
      const opaque: unknown = runtime.makeOpaque({ value: 1 });
      expect(opaque).toBeDefined();
    });

    it('newWeakRef and derefWeakRef', () => {
      const target = { value: 1 };
      const handle: unknown = runtime.newWeakRef(target);
      const deref: object | undefined = runtime.derefWeakRef(handle);
      expect(deref).toBe(target);
    });

    it('getCurrentPlatform', () => {
      const platform: number = runtime.getCurrentPlatform();
      expect(typeof platform).toBe('number');
    });
  });

  describe('callbacks', () => {
    it('configureCallback', () => {
      const cb = (value: number): number => value + 1;
      runtime.configureCallback<typeof cb>(0, cb);
      expect(cb(1)).toBe(2);
    });

    it('withCallbackAttribution', () => {
      const result: number = runtime.withCallbackAttribution<number>('runtime_api_surface', () => 42);
      expect(result).toBe(42);
    });

    it('makeANRAttributionProxy', () => {
      if (!runtime.makeANRAttributionProxy) {
        return;
      }
      const proxy: (value: number) => number = runtime.makeANRAttributionProxy(
        'runtime_api_surface',
        (v: number) => v * 2,
      );
      expect(proxy(21)).toBe(42);
    });
  });

  describe('contexts', () => {
    it('creates, configures, measures and destroys a context', () => {
      const contextId: string = runtime.createContext();
      try {
        runtime.setLayoutSpecs(contextId, 100, 200, false);
        runtime.setViewNodeAssetTracker(contextId, undefined);
        const tracker: ViewNodeAssetTrackerCallback = () => {};
        runtime.setViewNodeAssetTracker(contextId, tracker);

        const measured: [number, number] = runtime.measureContext(contextId, 100, 0, 200, 0, false);
        expect(measured === undefined || measured.length === 2).toBeTrue();

        const release: () => void = runtime.protectNativeRefs(contextId);
        release();

        const node: NativeNode | undefined = runtime.getNativeNodeForElementId(contextId, 1);
        expect(node).toBeUndefined();
      } finally {
        runtime.destroyContext(contextId);
      }
    });

    // The standalone test runtime creates contexts without a view manager, so this throws there.
    // Hosts that render return the backend type.
    it('getBackendRenderingTypeForContextId', () => {
      const contextId = runtime.createContext();
      try {
        let renderingType: BackendRenderingType | undefined;
        try {
          renderingType = runtime.getBackendRenderingTypeForContextId(contextId);
        } catch (error) {
          expect(String(error)).toContain('ViewManagerContext');
          return;
        }
        expect(typeof renderingType).toBe('number');
      } finally {
        runtime.destroyContext(contextId);
      }
    });

    it('pushCurrentContext, getCurrentContext and popCurrentContext', () => {
      const contextId = runtime.createContext();
      try {
        runtime.pushCurrentContext(contextId);
        const current: string = runtime.getCurrentContext();
        expect(String(current)).toBe(String(contextId));
        runtime.popCurrentContext();

        runtime.pushCurrentContext(undefined);
        runtime.popCurrentContext();
      } finally {
        runtime.destroyContext(contextId);
      }
    });

    // These resolve the context synchronously and throw when it is gone, so calling them on a
    // destroyed context checks the native binding without needing a rendered tree.
    it('element queries and postMessage reject a destroyed context', () => {
      const contextId = runtime.createContext();
      runtime.destroyContext(contextId);

      expect(() => runtime.getFrameForElementId(contextId, 1, (_frame: ElementFrame | undefined) => {})).toThrow();
      expect(() => runtime.getNativeViewForElementId(contextId, 1, (_view: NativeView | undefined) => {})).toThrow();
      expect(() =>
        runtime.getViewNodeDebugInfo(contextId, 1, (_viewNode: NativeViewNodeInfo | undefined) => {}),
      ).toThrow();
      expect(() => runtime.takeElementSnapshot(contextId, 1, (_snapshot: string | undefined) => {})).toThrow();
      expect(() => runtime.getLayoutDebugInfo(contextId, 1, (_layoutInfo: string | undefined) => {})).toThrow();
      // A callAction message without an action name is rejected before anything is dispatched.
      expect(() => runtime.postMessage(contextId, 'callAction', {})).toThrow();
    });

    it('saveCurrentContext and restoreCurrentContext', () => {
      const saved: number = runtime.saveCurrentContext();
      runtime.restoreCurrentContext(saved);
    });
  });

  describe('modules', () => {
    it('isModuleLoaded', () => {
      expect(runtime.isModuleLoaded(MODULE_NAME)).toBeTrue();
    });

    it('getModuleJsPaths and getModuleEntry', () => {
      const paths: string[] = runtime.getModuleJsPaths(MODULE_NAME);
      expect(paths.length).toBeGreaterThan(0);
      const missingEntry = (): Uint8Array | string => runtime.getModuleEntry(MODULE_NAME, 'does/not/exist.js', true);
      expect(missingEntry).toThrow();
    });

    it('loadModule', async () => {
      let loadError: string | undefined = 'not called';
      await waitFor(done =>
        runtime.loadModule('valdi_core', (error?: string) => {
          loadError = error;
          done();
        }),
      );
      expect(loadError).toBeUndefined();
    });
  });

  describe('styles', () => {
    it('createCSSRule', () => {
      // Style.toNative passes the object form; the native side rejects the array form.
      const rule: CSSRuleNative = runtime.createCSSRule({ opacity: 0.5 });
      expect(typeof rule).toBe('number');
    });

    it('getCSSModule', () => {
      const missing = (): CSSModuleNative => runtime.getCSSModule(`${MODULE_NAME}/does_not_exist.css`);
      expect(missing).toThrow();
    });

    it('setColorPalette, configureColorPalette and setActiveColorPalette', () => {
      const palette: ColorPalette = { 'runtime-api-surface': '#ff0000' };
      runtime.setColorPalette(palette);
      runtime.configureColorPalette('runtime_api_surface', palette);
      runtime.setActiveColorPalette('runtime_api_surface');
      runtime.setActiveColorPalette('default');
    });
  });

  describe('assets', () => {
    // A file URL so that nothing can reach the network; creating an asset doesn't load it.
    const url = 'file:///runtime_api_surface/does_not_exist.png';

    it('makeAssetFromUrl and makeAssetFromBytes', () => {
      const fromUrl: Asset = runtime.makeAssetFromUrl(url);
      const fromBytes: Asset = runtime.makeAssetFromBytes(new Uint8Array([1, 2, 3]));
      expect(fromUrl).toBeDefined();
      expect(fromBytes).toBeDefined();
    });

    it('makeDirectionalAsset, makePlatformSpecificAsset and makeThemableAsset', () => {
      const ltr = runtime.makeAssetFromUrl(url);
      const rtl = runtime.makeAssetFromUrl(`${url}?rtl`);
      const directional: Asset = runtime.makeDirectionalAsset(ltr, rtl);
      const platformSpecific: Asset = runtime.makePlatformSpecificAsset(ltr, { ios: rtl, android: rtl });
      const themable: Asset = runtime.makeThemableAsset({ light: ltr, dark: rtl });
      expect(directional).toBeDefined();
      expect(platformSpecific).toBeDefined();
      expect(themable).toBeDefined();
    });

    it('getAssets', () => {
      const missing = (): AssetEntry[] => runtime.getAssets(`${MODULE_NAME}/does_not_exist`);
      expect(missing).toThrow();
    });
  });

  describe('tracing and diagnostics', () => {
    it('trace and makeTraceProxy', () => {
      if (typeof runtime.trace !== 'function' || typeof runtime.makeTraceProxy !== 'function') {
        return;
      }
      expect(runtime.trace('runtime_api_surface', () => 42)).toBe(42);
      let called = false;
      const proxy: () => void = runtime.makeTraceProxy('runtime_api_surface', () => {
        called = true;
      });
      proxy();
      expect(called).toBeTrue();
    });

    it('beginTrace, endTrace and instantTrace', () => {
      if (runtime.beginTrace && runtime.endTrace) {
        runtime.beginTrace('runtime_api_surface');
        runtime.endTrace();
      }
      runtime.instantTrace?.('runtime_api_surface', ['key', 'value']);
    });

    it('startTraceRecording, stopTraceRecording and stopTraceRecordingWithStats', () => {
      const id: number = runtime.startTraceRecording();
      const entries: unknown[] = runtime.stopTraceRecording(id);
      expect(Array.isArray(entries)).toBeTrue();
      if (runtime.stopTraceRecordingWithStats) {
        const statsId = runtime.startTraceRecording();
        expect(Array.isArray(runtime.stopTraceRecordingWithStats(statsId))).toBeTrue();
      }
    });

    it('submitDebugMessage and outputLog', () => {
      runtime.submitDebugMessage(0, 'runtime_api_surface debug message');
      runtime.outputLog(1, 'runtime_api_surface log message');
    });

    it('dumpMemoryStatistics and performGC', () => {
      runtime.performGC();
      const stats: RuntimeMemoryStatistics = runtime.dumpMemoryStatistics();
      expect(typeof stats.memoryUsageBytes).toBe('number');
      expect(typeof stats.objectsCount).toBe('number');
    });
  });

  describe('scheduling', () => {
    it('scheduleWorkItem and unscheduleWorkItem', async () => {
      const cancelled: number = runtime.scheduleWorkItem(() => fail('unscheduled work item ran'), 60 * 1000, true);
      runtime.unscheduleWorkItem(cancelled);
      await waitFor(done => runtime.scheduleWorkItem(done, 0));
    });

    it('performSyncWithMainThread', async () => {
      await waitFor(done => runtime.performSyncWithMainThread(done));
    });

    it('callOnMainThread', async () => {
      const args = await new Promise<unknown[]>(resolve =>
        runtime.callOnMainThread((...received: unknown[]) => resolve(received), [1, 'two']),
      );
      expect(args).toEqual([1, 'two']);
    });

    it('onMainThreadIdle', async () => {
      await waitFor(done => runtime.onMainThreadIdle(done));
    });
  });

  // Not called: the exception handlers and onUncaughtError would replace or trip the handlers the
  // test runner installs, createWorker starts another JS thread, submitRawRenderRequest needs an
  // encoded render tree, and the asset-load members need a platform image loader. The pinned
  // interface already fixes their signatures; these typed references also fail to compile if the
  // declarations change.
  it('references members that are not called', () => {
    const createWorker: (url: string) => NativeWorker = runtime.createWorker;
    const submitRawRenderRequest: (renderRequest: RenderRequest, callback: () => void) => void =
      runtime.submitRawRenderRequest;
    const addAssetLoadObserver: (
      asset: string | Asset,
      onLoad: (loadedAsset: LoadedAsset | Uint8Array | undefined, error: string | undefined) => void,
      outputType: number,
      preferredWidth: number | undefined,
      preferredHeight: number | undefined,
    ) => () => void = runtime.addAssetLoadObserver;
    const getLoadedAssetMetadata: (loadedAsset: LoadedAsset) => LoadedAssetMetadata | undefined =
      runtime.getLoadedAssetMetadata;
    const onUncaughtError: (message: string, error: Error) => void = runtime.onUncaughtError;
    const setUncaughtExceptionHandler: (
      cb: ((error: unknown, contextId: string) => ExceptionHandlerResult) | undefined,
    ) => void = runtime.setUncaughtExceptionHandler;
    const setUnhandledRejectionHandler: (
      cb: ((promiseResult: unknown, contextId: string) => ExceptionHandlerResult) | undefined,
    ) => void = runtime.setUnhandledRejectionHandler;
    const dumpHeap: (() => ArrayBuffer) | undefined = runtime.dumpHeap;

    const references: unknown[] = [
      createWorker,
      submitRawRenderRequest,
      addAssetLoadObserver,
      getLoadedAssetMetadata,
      onUncaughtError,
      setUncaughtExceptionHandler,
      setUnhandledRejectionHandler,
    ];
    for (const reference of references) {
      expect(typeof reference).toBe('function');
    }
    expect(dumpHeap === undefined || typeof dumpHeap === 'function').toBeTrue();
  });
});
