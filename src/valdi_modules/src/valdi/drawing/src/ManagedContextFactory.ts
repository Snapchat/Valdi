import { ValdiRuntime } from 'valdi_core/src/ValdiRuntime';
import { IRenderer } from 'valdi_core/src/IRenderer';
import { jsx } from 'valdi_core/src/JSXBootstrap';
import { Renderer } from 'valdi_core/src/Renderer';
import { Size } from './DrawingModuleProvider';
import { IBitmap } from './IBitmap';
import { IManagedContext, IManagedContextAssetsLoadResult, IManagedContextDrawResult, IManagedContextFrame, MeasureMode, RasterResult } from './IManagedContext';
import { ManagedContextAssetTracker } from './ManagedContextAssetTracker';
import {
  AssetTrackerEventType,
  SnapDrawingValdiContext,
  SnapDrawingFrameNative,
  createValdiContextWithSnapDrawing,
  destroyValdiContextWithSnapDrawing,
  disposeFrame,
  drawFrame,
  drawFrameSync,
  layoutAsync,
  measureAsync,
  rasterFrame,
} from './ManagedContextNative';

declare const runtime: ValdiRuntime;

class FrameImpl implements IManagedContextFrame {
  constructor(readonly native: SnapDrawingFrameNative) {}

  dispose(): void {
    disposeFrame(this.native);
  }

  rasterInto(bitmap: IBitmap, shouldClearBitmapBeforeDrawing: boolean): RasterResult {
    const damageRects = rasterFrame(this.native, bitmap.native, shouldClearBitmapBeforeDrawing, false);
    return { damageRects };
  }

  rasterDeltaInto(bitmap: IBitmap): RasterResult {
    const damageRects = rasterFrame(this.native, bitmap.native, false, true);
    return { damageRects };
  }
}

class ManagedContextImpl implements IManagedContext {
  renderer: IRenderer;

  private snapDrawingValdiContext: SnapDrawingValdiContext;
  private contextId: string;
  private _renderer: Renderer;
  private assetTracker: ManagedContextAssetTracker;
  private readonly useAsyncPath: boolean;

  constructor(
    useNewExternalSurfaceRasterMethod: boolean,
    enableDeltaRasterization: boolean,
    useAsyncPath: boolean,
  ) {
    this.useAsyncPath = useAsyncPath;
    this.snapDrawingValdiContext = createValdiContextWithSnapDrawing(
      useNewExternalSurfaceRasterMethod,
      enableDeltaRasterization,
      (eventType, nodeId, error) => {
        console.log('Event asset event', eventType, nodeId);
        this.processAssetEvent(eventType, nodeId, error);
      },
    );
    this.contextId = this.snapDrawingValdiContext.contextId;
    this._renderer = jsx.makeRenderer(this.contextId);
    this.renderer = this._renderer;
    this.assetTracker = new ManagedContextAssetTracker();
  }

  private processAssetEvent(eventType: AssetTrackerEventType, nodeId: number, error: string | undefined) {
    switch (eventType) {
      case AssetTrackerEventType.beganRequestingLoadedAsset:
        this.assetTracker.onBeganRequestingLoadedAsset(nodeId);
        break;
      case AssetTrackerEventType.endRequestingLoadedAsset:
        this.assetTracker.onEndRequestingLoadedAsset(nodeId);
        break;
      case AssetTrackerEventType.loadedAssetChange:
        this.assetTracker.onLoadedAssetChanged(nodeId, error);
        break;
    }
  }

  render(renderFunc: () => void): void {
    this._renderer.renderRoot(renderFunc);
  }

  measure(maxWidth: number, widthMode: MeasureMode, maxHeight: number, heightMode: MeasureMode, rtl: boolean): Promise<Size> {
    if (this.useAsyncPath) {
      return new Promise(resolve => {
        measureAsync(
          this.snapDrawingValdiContext.native,
          maxWidth,
          widthMode,
          maxHeight,
          heightMode,
          rtl,
          (width: number, height: number) => {
            resolve({ width, height });
          },
        );
      });
    }
    // Sync path: use runtime.measureContext directly (same as original implementation; no native module dispatch).
    const result = runtime.measureContext(
      this.contextId,
      maxWidth,
      widthMode,
      maxHeight,
      heightMode,
      rtl,
    );
    return Promise.resolve({ width: result[0], height: result[1] });
  }

  layout(width: number, height: number, rtl: boolean): Promise<void> {
    if (this.useAsyncPath) {
      return new Promise(resolve => {
        layoutAsync(this.snapDrawingValdiContext.native, width, height, rtl, () => {
          resolve();
        });
      });
    }
    // Sync path: use runtime.setLayoutSpecs directly (same as original implementation).
    runtime.setLayoutSpecs(this.contextId, width, height, rtl);
    return Promise.resolve();
  }

  draw(): Promise<IManagedContextDrawResult> {
    if (this.useAsyncPath) {
      return new Promise(resolve => {
        drawFrame(
          this.snapDrawingValdiContext.native,
          (frameNative: SnapDrawingFrameNative, mainThreadMs: number) => {
            resolve({ frame: new FrameImpl(frameNative), mainThreadMs });
          },
        );
      });
    }
    const frameNative = drawFrameSync(this.snapDrawingValdiContext.native);
    return Promise.resolve({
      frame: new FrameImpl(frameNative),
      mainThreadMs: 0,
    });
  }

  onAllAssetsLoaded(): Promise<IManagedContextAssetsLoadResult> {
    return new Promise(resolve => {
      this.assetTracker.onAllAssetsLoaded(() => {
        const errors = this.assetTracker.collectErrors();
        resolve({ loadedAssetsCount: this.assetTracker.assetsCount, errors });
      });
    });
  }

  dispose(): void {
    this._renderer.delegate.onDestroyed();
    this._renderer.renderRoot(() => {});
    destroyValdiContextWithSnapDrawing(this.snapDrawingValdiContext.native);
  }
}

/**
 * Defines how embedded platform views are rasterized. Embedded platform views are views
 * that are not natively implemented by SnapDrawing, for example like <textview> or
 * <textfield> or any other native view provided through the <custom-view> element.
 */
export const enum EmbeddedPlatformViewRasterMethod {
  /**
   * The native view will be rasterized using its frame size. If it has a transform (scale, rotation)
   * set on the view or one of its ancestors, the transform will be applied post rasterization.
   * The native view is only redrawn when one of its properties changes. This system results in high
   * draw cache hit rate, but will result in a potentially lower quality image when using transformation.
   */
  FAST = 0,

  /**
   * The native view will be rasterized into the final output buffer with its final transform (scale, rotation)
   * applied. It will be rasterized again every time scale, rotation, translation or frame changes,  or if
   * one of its properties changes. This system results in a highly accurate rasterization of the native view,
   *  but will result in a potentially lower draw cache hit rate and more expensive rasterization.
   */
  ACCURATE = 1,
}

export interface IManagedContextOptions {
  embeddedPlatformViewRasterMethod?: EmbeddedPlatformViewRasterMethod;
  deltaRasterization?: boolean;
  /**
   * When false or unset (default, control), use sync path. When true (treatment), use async path.
   * AB test: compare deadlock rate and performance; CoF MANAGED_CONTEXT_USE_ASYNC.
   */
  useAsyncPath?: boolean;
}

/**
 * Create a new IManagedContext that can be used to render a detached Valdi tree
 */
export function createManagedContext(options?: IManagedContextOptions): IManagedContext {
  const useNewExternalSurfaceRasterMethod =
    options?.embeddedPlatformViewRasterMethod === EmbeddedPlatformViewRasterMethod.ACCURATE;
  const enableDeltaRasterization = options?.deltaRasterization ?? false;
  const useAsyncPath = options?.useAsyncPath ?? false;
  return new ManagedContextImpl(useNewExternalSurfaceRasterMethod, enableDeltaRasterization, useAsyncPath);
}
