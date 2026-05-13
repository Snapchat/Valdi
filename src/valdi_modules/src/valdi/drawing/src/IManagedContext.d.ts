import { IRenderer } from 'valdi_core/src/IRenderer';
import { Size } from './DrawingModuleProvider';
import { IBitmap } from './IBitmap';
import type { Rect } from './ManagedContextNative';

export type { Rect };

export const enum MeasureMode {
  UNSPECIFIED = 0,
  EXACTLY = 1,
  AT_MOST = 2,
}

export interface RasterResult {
  damageRects: Rect[];
}

export interface IManagedContextDrawResult {
  frame: IManagedContextFrame;
  mainThreadMs: number;
}

export interface IManagedContextAssetsLoadResult {
  loadedAssetsCount: number;
  errors: string[] | undefined;
}

export interface IManagedContext {
  /**
   * Return the underlying Renderer managed by this ManagedContext
   */
  renderer: IRenderer;

  /**
   * Starts a render and evaluate the given render function
   */
  render(renderFunc: () => void): void;

  /**
   * Measure the context with the given max size and return
   * a size that fits within the size constraint.
   * When using the async path, measurement is dispatched to the main thread.
   * If the context is disposed before completion (async path), the promise still resolves with { width: 0, height: 0 }.
   */
  measure(maxWidth: number, widthMode: MeasureMode, maxHeight: number, heightMode: MeasureMode, rtl: boolean): Promise<Size>;

  /**
   * Set the layout specifications of the tree.
   * When using the async path, layout is dispatched to the main thread.
   * If the context is disposed before completion (async path), the promise still resolves (no-op).
   */
  layout(width: number, height: number, rtl: boolean): Promise<void>;

  /**
   * Draw the rendered tree with the previously provided layout specs.
   * Return a frame that contains the instructions that represents the scene.
   * When using the async path, draw is dispatched to the main thread.
   * If the context is disposed before completion (async path), the promise still resolves with an empty frame.
   */
  draw(): Promise<IManagedContextDrawResult>;

  /**
   * Schedule a callback to be called when all the assets have been loaded.
   */
  onAllAssetsLoaded(): Promise<IManagedContextAssetsLoadResult>;

  /**
   * Destroy the managed context and its associated resources
   */
  dispose(): void;
}

export interface IManagedContextFrame {
  /**
   * Immediately dispose the frame and its associated resources
   */
  dispose(): void;

  /**
   * Rasterize the frame into a bitmap provided as an array buffer.
   */
  rasterInto(bitmap: IBitmap, shouldClearBitmapBeforeDrawing: boolean): RasterResult;

  /**
   * Rasterize the frame into a bitmap provided as an array buffer.
   * Only the areas that have changed since the last rasterization will be rasterized.
   */
  rasterDeltaInto(bitmap: IBitmap): RasterResult;
}
