import { INativeBitmap } from '../src/INativeBitmap';
import {
  SnapDrawingValdiContext,
  SnapDrawingValdiContextNative,
  SnapDrawingFrameNative,
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
  _useNewExternalSurfaceRasterMethod: boolean,
  _enableDeltaRasterization: boolean,
): SnapDrawingValdiContext {
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
