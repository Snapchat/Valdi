import { RuntimeBase } from 'coreutils/src/RuntimeBase';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { NativeNode } from 'valdi_tsx/src/NativeNode';
import { NativeView } from 'valdi_tsx/src/NativeView';
import { Asset, PlatformAssetOverrides } from './Asset';
import { ElementId } from './IRenderedElement';
import { IRootComponentsManager } from './IRootComponentsManager';
import { RenderRequest } from './RenderRequest';
import { SubmitDebugMessageFunc } from './debugging/DebugMessage';
import { AnyFunction } from './utils/Callback';
import { PropertyList } from './utils/PropertyList';

export type CSSRuleNative = number;
export interface CSSModuleNative {
  getRule(name: string): CSSRuleNative | undefined;
}

export interface NativeViewNodeInfo {
  id: string;
  view: NativeView;
  attributes: any;
  includedInLayout: boolean;
  frame: ElementFrame;
  children: NativeViewNodeInfo[];
}

export interface ColorPalette {
  [key: string]: string;
}

interface MessageEvent<T = unknown> {
  readonly data: T;
}

export type OnMessageFunc<T> = (msg: MessageEvent<T>) => void;

export interface NativeWorker {
  postMessage<T>(data: T): void;
  setOnMessage<T>(f: OnMessageFunc<T>): void;
  terminate(): void;
}

export interface RuntimeMemoryStatistics {
  memoryUsageBytes: number;
  objectsCount: number;
}

export const enum ExceptionHandlerResult {
  NOTIFY = 0,
  IGNORE = 1,
  CRASH = 2,
}

export const enum BackendRenderingType {
  IOS = 1,
  ANDROID = 2,
  SNAP_DRAWING = 3,
}

export type AssetEntry = Asset | string;

export interface LoadedAsset {
  brand?: 'LoadedAsset';
}

export interface LoadedAssetImageMetadata {
  type: 'image';
  width: number;
  height: number;
}

export interface LoadedAssetLottieMetadata {
  type: 'lottie';
  width: number;
  height: number;
  durationMs: number;
}

export interface LoadedAssetSkCodecMetadata {
  type: 'skcodec';
  width: number;
  height: number;
  numberOfFrames: number;
  durationMs: number;
}

export type LoadedAssetMetadata = LoadedAssetImageMetadata | LoadedAssetLottieMetadata | LoadedAssetSkCodecMetadata;

export interface ValdiRuntime extends RuntimeBase {
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
  getViewNodeDebugInfo(
    contextId: string,
    elementId: ElementId,
    callback: (viewNode: NativeViewNodeInfo | undefined) => void,
  ): void;
  takeElementSnapshot(contextId: string, elementId: ElementId, callback: (snapshot: string | undefined) => void): void;
  getLayoutDebugInfo(contextId: string, elementId: ElementId, callback: (layoutInfo: string | undefined) => void): void;
  performSyncWithMainThread(func: () => void): void;
  createWorker(url: string): NativeWorker;

  // Rendering
  submitRawRenderRequest(renderRequest: RenderRequest, callback: () => void): void;

  createContext(manager?: IRootComponentsManager): string;
  destroyContext(contextId: string): void;
  setLayoutSpecs(contextId: string, width: number, height: number, rtl: boolean): void;
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
  /**
   * Load a Valdi Module with the given module name.
   * If the completion is null, the operation will be considered
   * a preload operation and it might be ignored based on the
   * configuration of the runtime.
   */
  loadModule(module: string, completion?: (error?: string) => void): void;
  getModuleEntry(module: string, path: string, asString: boolean): Uint8Array | string;
  getModuleJsPaths(module: string): string[];

  trace(tag: string, callback: () => any): any;
  makeTraceProxy(tag: string, callback: () => void): () => void;

  startTraceRecording(): number;
  stopTraceRecording(id: number): any[];

  submitDebugMessage: SubmitDebugMessageFunc;

  callOnMainThread(method: any, parameters: any[]): void;
  onMainThreadIdle(cb: () => void): void;

  makeAssetFromUrl(url: string): Asset;
  makeAssetFromBytes(bytes: ArrayBuffer | Uint8Array): Asset;
  makeDirectionalAsset(ltrAsset: string | Asset, rtlAsset: string | Asset): Asset;
  makePlatformSpecificAsset(defaultAsset: string | Asset, platformAssetOverrides: PlatformAssetOverrides): Asset;
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

  outputLog(type: number, content: string): void;

  scheduleWorkItem(cb: () => void, delayMs?: number, interruptible?: boolean): number;
  unscheduleWorkItem(taskId: number): void;

  pushCurrentContext(contextId: string | undefined): void;
  popCurrentContext(): void;
  getCurrentContext(): string;

  saveCurrentContext(): number;
  restoreCurrentContext(contextId: number): void;

  onUncaughtError(message: string, error: Error): void;
  /**
   * Registers an uncaught exception handler that will be called by the runtime
   * when an uncaught exception is detected
   */
  setUncaughtExceptionHandler(cb: ((error: unknown, contextId: string) => ExceptionHandlerResult) | undefined): void;
  /**
   * Registers an unhandled rejection handler that will be called by the runtime
   * when an unhandled rejected promise is detected
   */
  setUnhandledRejectionHandler(
    cb: ((promiseResult: unknown, contextId: string) => ExceptionHandlerResult) | undefined,
  ): void;

  dumpMemoryStatistics(): RuntimeMemoryStatistics;

  performGC(): void;

  dumpHeap?(): ArrayBuffer;

  isDebugEnabled: boolean;

  /**
   * Whether JS logging (console.log/warn/error/info/debug) is enabled.
   * In non-appstore builds this is always true.
   * In appstore builds this is controlled by the VALDI_DISABLE_JS_LOGGING COF key.
   */
  isLoggingEnabled: boolean;

  /**
   * When true, the renderer emits moves in top-down order (parent before children) to reduce ANR risk.
   * Gated by VALDI_MAX_VIEW_OPERATIONS_PROCESSING_TIME (same COF as view-op throttling); true when that value > 0.
   */
  useTopDownMoveOrder: boolean;

  buildType: string;
}
