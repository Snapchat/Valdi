import { INativeBitmap } from 'drawing/src/INativeBitmap';

/**
 * @ExportModule
 */

// @ExportModel
export interface NativeImageDiffResult {
  changedPixels: number;
  totalPixels: number;
  dimensionMismatch: boolean;
  image: INativeBitmap;
}

// @ExportFunction
export function diffEncodedImages(
  beforeData: Uint8Array,
  afterData: Uint8Array,
  pixelThreshold: number,
): NativeImageDiffResult;
