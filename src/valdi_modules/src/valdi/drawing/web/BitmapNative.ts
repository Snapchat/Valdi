import { BitmapAlphaType, BitmapColorType, BitmapInfo, ImageEncoding } from '../src/IBitmap';
import { INativeBitmap } from '../src/INativeBitmap';
import { decodePng } from './PngDecoder';

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

function toBytes(data: string | Uint8Array | ArrayBuffer): Uint8Array {
  if (typeof data === 'string') {
    const binary = atob(data);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) {
      bytes[i] = binary.charCodeAt(i);
    }
    return bytes;
  }
  return data instanceof Uint8Array ? data : new Uint8Array(data);
}

export function decodeNativeBitmap(data: string | Uint8Array | ArrayBuffer): INativeBitmap {
  const decoded = decodePng(toBytes(data));
  const handle: INativeBitmap = { brand: 'INativeBitmap' };
  META.set(handle, { info: decoded.info, pixels: decoded.pixels, locked: false });
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
