import { BitmapAlphaType, BitmapColorType, BitmapInfo } from '../src/IBitmap';

export interface DecodedBitmap {
  info: BitmapInfo;
  pixels: ArrayBuffer;
}

// Decoding is synchronous and runs on the calling thread, so large images stall it.
const LARGE_IMAGE_PIXELS = 4 * 1024 * 1024;

const PNG_SIGNATURE = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];

const COLOR_GRAY = 0;
const COLOR_RGB = 2;
const COLOR_PALETTE = 3;
const COLOR_GRAY_ALPHA = 4;
const COLOR_RGBA = 6;

interface PngHeader {
  width: number;
  height: number;
  bitDepth: number;
  colorType: number;
  interlace: number;
}

function formatName(data: Uint8Array): string {
  if (data[0] === 0xff && data[1] === 0xd8) return 'JPEG';
  if (data[0] === 0x47 && data[1] === 0x49 && data[2] === 0x46) return 'GIF';
  if (data[8] === 0x57 && data[9] === 0x45 && data[10] === 0x42 && data[11] === 0x50) return 'WebP';
  return 'unknown';
}

function readUint32(data: Uint8Array, offset: number): number {
  return ((data[offset] << 24) | (data[offset + 1] << 16) | (data[offset + 2] << 8) | data[offset + 3]) >>> 0;
}

function channelCount(colorType: number): number {
  switch (colorType) {
    case COLOR_GRAY:
    case COLOR_PALETTE:
      return 1;
    case COLOR_GRAY_ALPHA:
      return 2;
    case COLOR_RGB:
      return 3;
    case COLOR_RGBA:
      return 4;
    default:
      throw new Error(`Invalid PNG color type ${colorType}`);
  }
}

// --- zlib / DEFLATE (RFC 1950, RFC 1951). Synchronous because decodeBitmap is synchronous and
// browsers only offer asynchronous decoders (createImageBitmap, DecompressionStream).

class Huffman {
  readonly counts = new Uint16Array(16);
  readonly symbols = new Uint16Array(288);

  constructor(lengths: ArrayLike<number>, offset: number, count: number) {
    for (let i = 0; i < count; i++) {
      this.counts[lengths[offset + i]]++;
    }
    this.counts[0] = 0;
    const offsets = new Uint16Array(16);
    let sum = 0;
    for (let i = 0; i < 16; i++) {
      offsets[i] = sum;
      sum += this.counts[i];
    }
    for (let i = 0; i < count; i++) {
      const length = lengths[offset + i];
      if (length) {
        this.symbols[offsets[length]++] = i;
      }
    }
  }
}

const LENGTH_BITS = new Uint8Array(30);
const LENGTH_BASE = new Uint16Array(30);
const DIST_BITS = new Uint8Array(30);
const DIST_BASE = new Uint16Array(30);

function buildBitsBase(bits: Uint8Array, base: Uint16Array, delta: number, first: number): void {
  for (let i = 0; i < 30 - delta; i++) {
    bits[i + delta] = Math.floor(i / delta);
  }
  let sum = first;
  for (let i = 0; i < 30; i++) {
    base[i] = sum;
    sum += 1 << bits[i];
  }
}

buildBitsBase(LENGTH_BITS, LENGTH_BASE, 4, 3);
buildBitsBase(DIST_BITS, DIST_BASE, 2, 1);
LENGTH_BITS[28] = 0;
LENGTH_BASE[28] = 258;

const CODE_LENGTH_ORDER = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];

function fixedLiteralTree(): Huffman {
  const lengths = new Uint8Array(288);
  lengths.fill(8, 0, 144);
  lengths.fill(9, 144, 256);
  lengths.fill(7, 256, 280);
  lengths.fill(8, 280, 288);
  return new Huffman(lengths, 0, 288);
}

function fixedDistanceTree(): Huffman {
  return new Huffman(new Uint8Array(30).fill(5), 0, 30);
}

class Inflater {
  private pos: number;
  private tag = 0;
  private bitCount = 0;
  private outPos = 0;

  constructor(private readonly src: Uint8Array, private readonly out: Uint8Array) {
    if (src.length < 2 || (src[0] & 0x0f) !== 8) {
      throw new Error('Invalid PNG image data (bad zlib header)');
    }
    this.pos = 2;
  }

  run(): void {
    let last: number;
    do {
      last = this.bit();
      switch (this.bits(2)) {
        case 0:
          this.stored();
          break;
        case 1:
          this.block(fixedLiteralTree(), fixedDistanceTree());
          break;
        case 2:
          this.dynamic();
          break;
        default:
          throw new Error('Invalid PNG image data (bad deflate block)');
      }
    } while (!last);
    if (this.outPos !== this.out.length) {
      throw new Error('Invalid PNG image data (truncated)');
    }
  }

  private bit(): number {
    if (this.bitCount === 0) {
      if (this.pos >= this.src.length) {
        throw new Error('Invalid PNG image data (truncated)');
      }
      this.tag = this.src[this.pos++]!;
      this.bitCount = 8;
    }
    const value = this.tag & 1;
    this.tag >>>= 1;
    this.bitCount--;
    return value;
  }

  private bits(count: number): number {
    let value = 0;
    for (let i = 0; i < count; i++) {
      value |= this.bit() << i;
    }
    return value;
  }

  private symbol(tree: Huffman): number {
    let sum = 0;
    let cur = 0;
    let len = 0;
    do {
      cur = 2 * cur + this.bit();
      len++;
      if (len > 15) {
        throw new Error('Invalid PNG image data (bad code)');
      }
      sum += tree.counts[len];
      cur -= tree.counts[len];
    } while (cur >= 0);
    return tree.symbols[sum + cur];
  }

  private stored(): void {
    this.bitCount = 0;
    const length = this.src[this.pos] | (this.src[this.pos + 1] << 8);
    this.pos += 4;
    if (this.pos + length > this.src.length || this.outPos + length > this.out.length) {
      throw new Error('Invalid PNG image data (bad stored block)');
    }
    this.out.set(this.src.subarray(this.pos, this.pos + length), this.outPos);
    this.pos += length;
    this.outPos += length;
  }

  private dynamic(): void {
    const hlit = this.bits(5) + 257;
    const hdist = this.bits(5) + 1;
    const hclen = this.bits(4) + 4;
    const lengths = new Uint8Array(320);
    for (let i = 0; i < hclen; i++) {
      lengths[CODE_LENGTH_ORDER[i]] = this.bits(3);
    }
    const codeTree = new Huffman(lengths, 0, 19);
    lengths.fill(0);
    let num = 0;
    while (num < hlit + hdist) {
      const sym = this.symbol(codeTree);
      let repeat: number;
      let value = 0;
      if (sym === 16) {
        if (num === 0) throw new Error('Invalid PNG image data (bad lengths)');
        value = lengths[num - 1]!;
        repeat = this.bits(2) + 3;
      } else if (sym === 17) {
        repeat = this.bits(3) + 3;
      } else if (sym === 18) {
        repeat = this.bits(7) + 11;
      } else {
        lengths[num++] = sym;
        continue;
      }
      if (num + repeat > hlit + hdist) throw new Error('Invalid PNG image data (bad lengths)');
      while (repeat--) lengths[num++] = value;
    }
    this.block(new Huffman(lengths, 0, hlit), new Huffman(lengths, hlit, hdist));
  }

  private block(literals: Huffman, distances: Huffman): void {
    for (;;) {
      let sym = this.symbol(literals);
      if (sym === 256) return;
      if (sym < 256) {
        if (this.outPos >= this.out.length) throw new Error('Invalid PNG image data (overflow)');
        this.out[this.outPos++] = sym;
        continue;
      }
      sym -= 257;
      if (sym >= 29) throw new Error('Invalid PNG image data (bad length)');
      const length = this.bits(LENGTH_BITS[sym]) + LENGTH_BASE[sym];
      const dist = this.symbol(distances);
      if (dist >= 30) throw new Error('Invalid PNG image data (bad distance)');
      const offset = this.bits(DIST_BITS[dist]) + DIST_BASE[dist];
      if (offset > this.outPos || this.outPos + length > this.out.length) {
        throw new Error('Invalid PNG image data (bad back-reference)');
      }
      for (let i = 0; i < length; i++) {
        this.out[this.outPos] = this.out[this.outPos - offset]!;
        this.outPos++;
      }
    }
  }
}

// --- PNG ---

function unfilter(raw: Uint8Array, height: number, rowBytes: number, bytesPerPixel: number): Uint8Array {
  const out = new Uint8Array(height * rowBytes);
  for (let y = 0; y < height; y++) {
    const filter = raw[y * (rowBytes + 1)];
    const src = y * (rowBytes + 1) + 1;
    const dst = y * rowBytes;
    const prev = dst - rowBytes;
    for (let x = 0; x < rowBytes; x++) {
      const a = x >= bytesPerPixel ? out[dst + x - bytesPerPixel] : 0;
      const b = y > 0 ? out[prev + x] : 0;
      const c = y > 0 && x >= bytesPerPixel ? out[prev + x - bytesPerPixel] : 0;
      let predictor: number;
      switch (filter) {
        case 0:
          predictor = 0;
          break;
        case 1:
          predictor = a;
          break;
        case 2:
          predictor = b;
          break;
        case 3:
          predictor = (a + b) >> 1;
          break;
        case 4: {
          const p = a + b - c;
          const pa = Math.abs(p - a);
          const pb = Math.abs(p - b);
          const pc = Math.abs(p - c);
          predictor = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
          break;
        }
        default:
          throw new Error(`Invalid PNG filter type ${filter}`);
      }
      out[dst + x] = (raw[src + x] + predictor) & 0xff;
    }
  }
  return out;
}

function readSample(row: Uint8Array, rowStart: number, index: number, bitDepth: number): number {
  switch (bitDepth) {
    case 8:
      return row[rowStart + index];
    case 16:
      return (row[rowStart + index * 2] << 8) | row[rowStart + index * 2 + 1];
    default: {
      const perByte = 8 / bitDepth;
      const byte = row[rowStart + Math.floor(index / perByte)];
      const shift = 8 - bitDepth * ((index % perByte) + 1);
      return (byte >> shift) & ((1 << bitDepth) - 1);
    }
  }
}

/**
 * Decodes a non-interlaced PNG into RGBA8888 pixels.
 */
export function decodePng(data: Uint8Array): DecodedBitmap {
  for (let i = 0; i < PNG_SIGNATURE.length; i++) {
    if (data[i] !== PNG_SIGNATURE[i]) {
      throw new Error(
        `Failed to decode image: the web renderer can only decode PNG synchronously (got ${formatName(data)} data)`,
      );
    }
  }

  let header: PngHeader | undefined;
  let palette: Uint8Array | undefined;
  let transparency: Uint8Array | undefined;
  const idat: Uint8Array[] = [];

  let pos = 8;
  while (pos + 8 <= data.length) {
    const length = readUint32(data, pos);
    const type = String.fromCharCode(data[pos + 4], data[pos + 5], data[pos + 6], data[pos + 7]);
    const body = data.subarray(pos + 8, pos + 8 + length);
    pos += 12 + length;
    if (type === 'IHDR') {
      header = {
        width: readUint32(body, 0),
        height: readUint32(body, 4),
        bitDepth: body[8],
        colorType: body[9],
        interlace: body[12],
      };
    } else if (type === 'PLTE') {
      palette = body;
    } else if (type === 'tRNS') {
      transparency = body;
    } else if (type === 'IDAT') {
      idat.push(body);
    } else if (type === 'IEND') {
      break;
    }
  }

  if (!header || idat.length === 0) {
    throw new Error('Invalid PNG: missing IHDR or IDAT');
  }
  const { width, height, bitDepth, colorType, interlace } = header;
  if (width === 0 || height === 0) {
    throw new Error('Invalid PNG: empty image');
  }
  if (width * height > LARGE_IMAGE_PIXELS) {
    console.warn(
      `decodeBitmap is decoding a ${width}x${height} PNG synchronously on web, which can block the thread for a noticeable time`,
    );
  }
  if (interlace !== 0) {
    throw new Error('Failed to decode image: interlaced PNGs are not supported on web');
  }

  const channels = channelCount(colorType);
  const bitsPerPixel = channels * bitDepth;
  const rowBytes = Math.ceil((width * bitsPerPixel) / 8);
  const bytesPerPixel = Math.max(1, bitsPerPixel >> 3);

  const compressed = new Uint8Array(idat.reduce((total, chunk) => total + chunk.length, 0));
  let written = 0;
  for (const chunk of idat) {
    compressed.set(chunk, written);
    written += chunk.length;
  }
  const raw = new Uint8Array(height * (rowBytes + 1));
  new Inflater(compressed, raw).run();
  const scanlines = unfilter(raw, height, rowBytes, bytesPerPixel);

  if (colorType === COLOR_PALETTE && !palette) {
    throw new Error('Invalid PNG: missing palette');
  }

  const sampleMax = (1 << bitDepth) - 1;
  const scale = (value: number): number => (bitDepth === 8 ? value : Math.round((value * 255) / sampleMax));
  const hasAlpha = colorType === COLOR_GRAY_ALPHA || colorType === COLOR_RGBA || transparency !== undefined;
  const keyOf = (index: number): number => (transparency![index * 2] << 8) | transparency![index * 2 + 1];

  const pixels = new Uint8Array(width * height * 4);
  for (let y = 0; y < height; y++) {
    const rowStart = y * rowBytes;
    for (let x = 0; x < width; x++) {
      let r: number;
      let g: number;
      let b: number;
      let a = 255;
      const base = x * channels;
      switch (colorType) {
        case COLOR_GRAY: {
          const v = readSample(scanlines, rowStart, base, bitDepth);
          r = g = b = scale(v);
          if (transparency && transparency.length >= 2 && v === keyOf(0)) a = 0;
          break;
        }
        case COLOR_GRAY_ALPHA:
          r = g = b = scale(readSample(scanlines, rowStart, base, bitDepth));
          a = scale(readSample(scanlines, rowStart, base + 1, bitDepth));
          break;
        case COLOR_RGB: {
          const rv = readSample(scanlines, rowStart, base, bitDepth);
          const gv = readSample(scanlines, rowStart, base + 1, bitDepth);
          const bv = readSample(scanlines, rowStart, base + 2, bitDepth);
          r = scale(rv);
          g = scale(gv);
          b = scale(bv);
          if (transparency && transparency.length >= 6 && rv === keyOf(0) && gv === keyOf(1) && bv === keyOf(2)) a = 0;
          break;
        }
        case COLOR_PALETTE: {
          const index = readSample(scanlines, rowStart, base, bitDepth);
          r = palette![index * 3] ?? 0;
          g = palette![index * 3 + 1] ?? 0;
          b = palette![index * 3 + 2] ?? 0;
          if (transparency && index < transparency.length) a = transparency[index]!;
          break;
        }
        default:
          r = scale(readSample(scanlines, rowStart, base, bitDepth));
          g = scale(readSample(scanlines, rowStart, base + 1, bitDepth));
          b = scale(readSample(scanlines, rowStart, base + 2, bitDepth));
          a = scale(readSample(scanlines, rowStart, base + 3, bitDepth));
      }
      const out = (y * width + x) * 4;
      if (a === 255) {
        pixels[out] = r;
        pixels[out + 1] = g;
        pixels[out + 2] = b;
      } else {
        pixels[out] = Math.round((r * a) / 255);
        pixels[out + 1] = Math.round((g * a) / 255);
        pixels[out + 2] = Math.round((b * a) / 255);
      }
      pixels[out + 3] = a;
    }
  }

  return {
    info: {
      width,
      height,
      colorType: BitmapColorType.RGBA8888,
      alphaType: hasAlpha ? BitmapAlphaType.Premul : BitmapAlphaType.OPAQUE,
      rowBytes: width * 4,
    },
    pixels: pixels.buffer,
  };
}
