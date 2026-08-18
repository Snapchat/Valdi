import { Base64 } from 'coreutils/src/Base64';
import { createBitmap, createBitmapWithBuffer, decodeBitmap } from 'drawing/src/BitmapFactory';
import { BitmapAlphaType, BitmapColorType, ImageEncoding } from 'drawing/src/IBitmap';
import type { BitmapInfo, IBitmap } from 'drawing/src/IBitmap';

export type Pixel = number;

function asBytes(value: Uint8Array | ArrayBuffer): Uint8Array {
  return value instanceof Uint8Array ? value : new Uint8Array(value);
}

function asArrayBuffer(data: Uint8Array): ArrayBuffer {
  if (data.byteOffset === 0 && data.byteLength === data.buffer.byteLength) {
    return data.buffer;
  }
  return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
}

export function createRgbaBitmap(width: number, height: number, data?: Uint8Array): IBitmap {
  const info = {
    width,
    height,
    colorType: BitmapColorType.RGBA8888,
    alphaType: BitmapAlphaType.Unpremul,
    rowBytes: width * 4,
  };
  if (data === undefined) {
    return createBitmap(info);
  }
  return createBitmapWithBuffer(info, asArrayBuffer(data));
}

export class LockedBitmapPixels {
  constructor(
    readonly info: BitmapInfo,
    private readonly pixels: Uint8Array,
  ) {
    if (info.colorType !== BitmapColorType.RGBA8888) {
      throw new Error(`Expected RGBA8888 bitmap pixels, got color type ${info.colorType}`);
    }
    if (info.rowBytes < info.width * 4) {
      throw new Error(`Expected bitmap rowBytes to be at least width * 4, got ${info.rowBytes}`);
    }
  }

  static rgba(red: number, green: number, blue: number, alpha: number): Pixel {
    return (((red & 0xff) << 24) | ((green & 0xff) << 16) | ((blue & 0xff) << 8) | (alpha & 0xff)) >>> 0;
  }

  static red(pixel: Pixel): number {
    return (pixel >>> 24) & 0xff;
  }

  static green(pixel: Pixel): number {
    return (pixel >>> 16) & 0xff;
  }

  static blue(pixel: Pixel): number {
    return (pixel >>> 8) & 0xff;
  }

  static alpha(pixel: Pixel): number {
    return pixel & 0xff;
  }

  getPixel(x: number, y: number): Pixel {
    if (x < 0 || y < 0 || x >= this.info.width || y >= this.info.height) {
      return 0;
    }

    const offset = y * this.info.rowBytes + x * 4;
    return LockedBitmapPixels.rgba(
      this.pixels[offset] ?? 0,
      this.pixels[offset + 1] ?? 0,
      this.pixels[offset + 2] ?? 0,
      this.pixels[offset + 3] ?? 0,
    );
  }

  setPixel(x: number, y: number, pixel: Pixel): void {
    if (x < 0 || y < 0 || x >= this.info.width || y >= this.info.height) {
      return;
    }

    const offset = y * this.info.rowBytes + x * 4;
    this.pixels[offset] = (pixel >>> 24) & 0xff;
    this.pixels[offset + 1] = (pixel >>> 16) & 0xff;
    this.pixels[offset + 2] = (pixel >>> 8) & 0xff;
    this.pixels[offset + 3] = pixel & 0xff;
  }
}

export function accessBitmapPixels<T>(bitmap: IBitmap, callback: (locked: LockedBitmapPixels) => T): T {
  const pixels = asBytes(bitmap.lockPixels());
  try {
    return callback(new LockedBitmapPixels(bitmap.getInfo(), pixels));
  } finally {
    bitmap.unlockPixels();
  }
}

export function decodePng(data: Uint8Array): IBitmap {
  return decodeBitmap(data);
}

export function decodeBase64Png(value: string): IBitmap {
  const normalized = value.replace(/^data:image\/png;base64,/, '');
  return decodePng(asBytes(Base64.toByteArray(normalized)));
}

export function encodePng(bitmap: IBitmap): Uint8Array {
  return asBytes(bitmap.encode(ImageEncoding.PNG, 1));
}

export function encodeBase64Png(bitmap: IBitmap): string {
  return Base64.fromByteArray(encodePng(bitmap));
}
