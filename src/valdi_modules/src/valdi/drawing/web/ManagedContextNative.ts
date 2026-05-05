import { INativeBitmap } from '../src/INativeBitmap';
import {
  SnapDrawingValdiContext,
  SnapDrawingValdiContextNative,
  SnapDrawingFrameNative,
  AssetTrackerCallback,
  AssetTrackerEventType,
} from '../src/ManagedContextNative';
import type { Rect } from '../src/ManagedContextNative';

/** Internal no-op tracker state */
let _nextContextId = 1;

/** Make a brandy placeholder object */
function makeNativeContext(): SnapDrawingValdiContextNative {
  return { brand: 'SnapDrawingValdiContextNative' };
}

/** Make a brandy frame placeholder object */
function makeFrame(): SnapDrawingFrameNative {
  return { brand: 'SnapDrawingFrameNative' };
}

export function createValdiContextWithSnapDrawing(
  useNewExternalSurfaceRasterMethod: boolean,
  assetTrackerCallback: AssetTrackerCallback,
): SnapDrawingValdiContext {
  // Fire a benign "started" and "ended" sequence to exercise the callback.
  try {
    assetTrackerCallback(AssetTrackerEventType.beganRequestingLoadedAsset, 0, undefined);
    assetTrackerCallback(AssetTrackerEventType.endRequestingLoadedAsset, 0, undefined);
  } catch {
    // Intentionally swallow in stub
  }

  const id = String(_nextContextId++);
  return {
    contextId: id,
    native: makeNativeContext(),
  };
}

export function destroyValdiContextWithSnapDrawing(_native: SnapDrawingValdiContextNative): void {
  // no-op
}

export function drawFrame(_native: SnapDrawingValdiContextNative): SnapDrawingFrameNative {
  // Return a placeholder frame
  return makeFrame();
}

export function disposeFrame(_native: SnapDrawingFrameNative): void {
  // no-op
}

export function rasterFrame(
  _native: SnapDrawingFrameNative,
  _bitmapNative: INativeBitmap,
  _shouldClearBitmapBeforeDrawing: boolean,
  _deltaRasterization: boolean,
): Rect[] {
  // no-op - return empty damage rects array
  return [];
}
