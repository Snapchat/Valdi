import { INativeBitmap } from './INativeBitmap';

export interface SnapDrawingValdiContextNative {
  brand?: 'SnapDrawingValdiContextNative';
}

export interface SnapDrawingFrameNative {
  brand?: 'SnapDrawingFrameNative';
}

export interface SnapDrawingValdiContext {
  contextId: string;
  native: SnapDrawingValdiContextNative;
}

export function createValdiContextWithSnapDrawing(
  useNewExternalSurfaceRasterMethod: boolean,
  enableDeltaRasterization: boolean,
): SnapDrawingValdiContext;

export function destroyValdiContextWithSnapDrawing(native: SnapDrawingValdiContextNative): void;

export function measureAsync(
  native: SnapDrawingValdiContextNative,
  maxWidth: number,
  widthMode: number,
  maxHeight: number,
  heightMode: number,
  rtl: boolean,
  callback: (width: number, height: number) => void,
): void;

export function layoutAsync(
  native: SnapDrawingValdiContextNative,
  width: number,
  height: number,
  rtl: boolean,
  callback: () => void,
): void;

export function drawFrame(
  native: SnapDrawingValdiContextNative,
  callback: (frame: SnapDrawingFrameNative, mainThreadMs: number) => void,
): void;

export function drawFrameSync(native: SnapDrawingValdiContextNative): SnapDrawingFrameNative;

export function processFrame(native: SnapDrawingValdiContextNative, deltaMs: number): void;

export function disposeFrame(native: SnapDrawingFrameNative): void;

/**
 * @ExportModel({
 *   ios: 'SCDrawingRect',
 *   android: 'com.snap.modules.drawing.Rect'
 * })
 */
export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

export function rasterFrame(
  native: SnapDrawingFrameNative,
  bitmapNative: INativeBitmap,
  shouldClearBitmapBeforeDrawing: boolean,
  deltaRasterization: boolean,
): Rect[];
