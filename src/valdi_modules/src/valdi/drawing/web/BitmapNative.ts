import { BitmapAlphaType, BitmapColorType, BitmapInfo, ImageEncoding } from '../src/IBitmap';
import { INativeBitmap } from '../src/INativeBitmap';

type Meta = {
  info: BitmapInfo;
  pixels: ArrayBuffer;
  locked: boolean;
};

const META = new WeakMap<INativeBitmap, Meta>();

function bytesPerPixel(ct: BitmapColorType): number {
  switch (ct) {
    case BitmapColorType.RGBA8888:
    case BitmapColorType.BGRA8888:
      return 4;
    case BitmapColorType.Alpha8:
    case BitmapColorType.Gray8:
      return 1;
    case BitmapColorType.RGBAF16:
      return 8; // 4 channels * 2 bytes
    case BitmapColorType.RGBAF32:
      return 16; // 4 channels * 4 bytes
    case BitmapColorType.UNKNOWN:
    default:
      return 4;
  }
}

function ensureRowBytes(rowBytes: number, width: number, colorType: BitmapColorType): number {
  if (rowBytes && rowBytes >= width * bytesPerPixel(colorType)) return rowBytes;
  return width * bytesPerPixel(colorType);
}

export function createNativeBitmap(
  width: number,
  height: number,
  colorType: BitmapColorType,
  alphaType: BitmapAlphaType,
  rowBytes: number,
  buffer: ArrayBuffer | undefined,
): INativeBitmap {
  const rb = ensureRowBytes(rowBytes, width, colorType);
  const byteLen = rb * height;
  const pixels = buffer && buffer.byteLength >= byteLen ? buffer.slice(0, byteLen) : new ArrayBuffer(byteLen);

  const info: BitmapInfo = { width, height, colorType, alphaType, rowBytes: rb };

  const handle: INativeBitmap = { brand: 'INativeBitmap' };
  META.set(handle, { info, pixels, locked: false });
  return handle;
}

export function decodeNativeBitmap(_data: string | Uint8Array | ArrayBuffer): INativeBitmap {
  // Stub: return a 1x1 transparent RGBA8
  const info: BitmapInfo = {
    width: 1,
    height: 1,
    colorType: BitmapColorType.RGBA8888,
    alphaType: BitmapAlphaType.Premul,
    rowBytes: 4,
  };
  const pixels = new ArrayBuffer(4); // RGBA = 0,0,0,0
  const handle: INativeBitmap = { brand: 'INativeBitmap' };
  META.set(handle, { info, pixels, locked: false });
  return handle;
}

export function disposeNativeBitmap(bitmap: INativeBitmap): void {
  META.delete(bitmap);
}

export function encodeNativeBitmap(bitmap: INativeBitmap, _encoding: ImageEncoding, _quality: number): ArrayBuffer {
  // Stub: return a copy of the pixel buffer
  const meta = META.get(bitmap);
  if (!meta) throw new Error('Invalid INativeBitmap');
  return meta.pixels.slice(0);
}

export function lockNativeBitmap(bitmap: INativeBitmap): ArrayBuffer {
  const meta = META.get(bitmap);
  if (!meta) throw new Error('Invalid INativeBitmap');
  meta.locked = true;
  return meta.pixels;
}

export function unlockNativeBitmap(bitmap: INativeBitmap): void {
  const meta = META.get(bitmap);
  if (!meta) throw new Error('Invalid INativeBitmap');
  meta.locked = false;
}

export function getNativeBitmapInfo(bitmap: INativeBitmap): BitmapInfo {
  const meta = META.get(bitmap);
  if (!meta) throw new Error('Invalid INativeBitmap');
  return meta.info;
}
