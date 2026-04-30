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

export const enum AssetTrackerEventType {
  beganRequestingLoadedAsset = 1,
  endRequestingLoadedAsset = 2,
  loadedAssetChange = 3,
}

export type AssetTrackerCallback = (
  eventType: AssetTrackerEventType,
  nodeId: number,
  error: string | undefined,
) => void;

export function createValdiContextWithSnapDrawing(
  useNewExternalSurfaceRasterMethod: boolean,
  enableDeltaRasterization: boolean,
  assetTrackerCallback: AssetTrackerCallback,
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

export function disposeFrame(native: SnapDrawingFrameNative): void;

/**
 * @ExportModel({
 *   ios: 'SCDrawingRect',
 *   android: 'com.snap.drawing.Rect'
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
