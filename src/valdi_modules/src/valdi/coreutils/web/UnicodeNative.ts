import { FromCodepointsOutput, Encoding } from '../src/unicode/UnicodeNative';

export function strToCodepoints(input: string, normalize: boolean, _disableCategorization: boolean): ArrayBuffer {
  const s = normalize ? input.normalize('NFC') : input;
  const cps = toCodePoints(s);
  const out = new ArrayBuffer(cps.length * 4);
  const view = new DataView(out);
  for (let i = 0; i < cps.length; i++) view.setUint32(i * 4, cps[i], true);
  return out;
}

export function codepointsToStr(
  codepoints: number[],
  normalize: boolean,
  _disableCategorization: boolean,
): FromCodepointsOutput {
  const sanitized = codepoints.map(sanitizeCodePoint);
  const str = String.fromCodePoint(...sanitized);
  const s = normalize ? str.normalize('NFC') : str;

  // Return a Uint32-per-codepoint buffer (LE) mirroring the input array
  const buf = new ArrayBuffer(sanitized.length * 4);
  const dv = new DataView(buf);
  for (let i = 0; i < sanitized.length; i++) dv.setUint32(i * 4, sanitized[i], true);
  return { str: s, buffer: buf };
}

export function encodeString(str: string, encoding: Encoding): ArrayBuffer {
  switch (encoding) {
    case Encoding.utf8:
      return encodeUtf8(str).buffer;
    case Encoding.utf16:
      return encodeUtf16LE(str).buffer; // no BOM
    case Encoding.utf32:
      return encodeUtf32LE(str).buffer; // no BOM
    default:
      throw new TypeError('Unknown encoding');
  }
}

export function decodeIntoString(buffer: ArrayBufferLike, encoding: Encoding): string {
  switch (encoding) {
    case Encoding.utf8:
      return decodeUtf8(new Uint8Array(buffer));
    case Encoding.utf16:
      return decodeUtf16(new Uint8Array(buffer)); // BOM aware
    case Encoding.utf32:
      return decodeUtf32(new Uint8Array(buffer)); // BOM aware
    default:
      throw new TypeError('Unknown encoding');
  }
}

/* ===== Helpers ===== */

const REPLACEMENT = 0xfffd;

function forEachCodePoint(s: string, fn: (cp: number, i: number) => void) {
  for (let i = 0; i < s.length; ) {
    const cp = s.codePointAt(i)!;
    fn(cp, i);
    i += cp > 0xffff ? 2 : 1;
  }
}

/** Turn a JS string into Unicode scalar values (code points). */
function toCodePoints(s: string): number[] {
  const cps: number[] = [];
  forEachCodePoint(s, cp => {
    const ch = String.fromCodePoint(cp);
    // ...
  });
  return cps;
}

function sanitizeCodePoint(cp: number): number {
  // valid ranges: 0..0xD7FF, 0xE000..0x10FFFF
  if (cp >= 0 && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff)) return cp >>> 0;
  return REPLACEMENT;
}

/* ===== UTF-8 ===== */

function encodeUtf8(str: string): Uint8Array {
  if (typeof TextEncoder !== 'undefined') {
    return new TextEncoder().encode(str);
  }
  // Manual fallback (sufficient for BMP + astral)
  const cps = toCodePoints(str);
  // worst case 4 bytes/code point
  const out: number[] = [];
  for (const cp of cps) {
    if (cp <= 0x7f) {
      out.push(cp);
    } else if (cp <= 0x7ff) {
      out.push(0b11000000 | (cp >> 6), 0b10000000 | (cp & 0b00111111));
    } else if (cp <= 0xffff) {
      out.push(0b11100000 | (cp >> 12), 0b10000000 | ((cp >> 6) & 0b00111111), 0b10000000 | (cp & 0b00111111));
    } else {
      out.push(
        0b11110000 | (cp >> 18),
        0b10000000 | ((cp >> 12) & 0b00111111),
        0b10000000 | ((cp >> 6) & 0b00111111),
        0b10000000 | (cp & 0b00111111),
      );
    }
  }
  return Uint8Array.from(out);
}

function decodeUtf8(bytes: Uint8Array): string {
  if (typeof TextDecoder !== 'undefined') {
    try {
      return new TextDecoder('utf-8', { fatal: false }).decode(bytes);
    } catch {
      // fall through to manual
    }
  }
  const cps: number[] = [];
  for (let i = 0; i < bytes.length; ) {
    const b0 = bytes[i++];
    if (b0 < 0x80) {
      cps.push(b0);
      continue;
    }
    let needed = 0,
      cp = 0;
    if ((b0 & 0b11100000) === 0b11000000) {
      needed = 1;
      cp = b0 & 0b00011111;
    } else if ((b0 & 0b11110000) === 0b11100000) {
      needed = 2;
      cp = b0 & 0b00001111;
    } else if ((b0 & 0b11111000) === 0b11110000) {
      needed = 3;
      cp = b0 & 0b00000111;
    } else {
      cps.push(REPLACEMENT);
      continue;
    }

    if (i + needed > bytes.length) {
      cps.push(REPLACEMENT);
      break;
    }
    let valid = true;
    for (let k = 0; k < needed; k++) {
      const bx = bytes[i++];
      if ((bx & 0b11000000) !== 0b10000000) {
        valid = false;
        break;
      }
      cp = (cp << 6) | (bx & 0b00111111);
    }
    // overlong & range checks (simple)
    if (
      !valid ||
      (needed === 1 && cp < 0x80) ||
      (needed === 2 && cp < 0x800) ||
      (needed === 3 && cp < 0x10000) ||
      cp > 0x10ffff ||
      (cp >= 0xd800 && cp <= 0xdfff)
    ) {
      cps.push(REPLACEMENT);
    } else {
      cps.push(cp);
    }
  }
  return String.fromCodePoint(...cps);
}

/* ===== UTF-16 (LE on encode; BOM-aware on decode) ===== */

function encodeUtf16LE(str: string): Uint8Array {
  // Use UTF-16 code units directly (JS internal)
  const out = new Uint8Array(str.length * 2);
  const dv = new DataView(out.buffer);
  for (let i = 0; i < str.length; i++) {
    dv.setUint16(i * 2, str.charCodeAt(i), true); // little-endian
  }
  return out;
}

function decodeUtf16(bytes: Uint8Array): string {
  if (bytes.length === 0) return '';
  // Detect BOM
  let be = false,
    offset = 0;
  if (bytes.length >= 2) {
    const b0 = bytes[0],
      b1 = bytes[1];
    if (b0 === 0xfe && b1 === 0xff) {
      be = true;
      offset = 2;
    } else if (b0 === 0xff && b1 === 0xfe) {
      be = false;
      offset = 2;
    }
  }
  const dv = new DataView(bytes.buffer, bytes.byteOffset + offset, bytes.byteLength - offset);
  const len = dv.byteLength & ~1;
  const codeUnits: number[] = new Array(len / 2);
  for (let i = 0; i < len; i += 2) {
    codeUnits[i >> 1] = dv.getUint16(i, !be);
  }
  // Construct string in chunks to avoid argument limits
  let out = '';
  const CHUNK = 0x8000;
  for (let i = 0; i < codeUnits.length; i += CHUNK) {
    out += String.fromCharCode(...codeUnits.slice(i, i + CHUNK));
  }
  return out;
}

/* ===== UTF-32 (LE on encode; BOM-aware on decode) ===== */

function encodeUtf32LE(str: string): Uint8Array {
  const cps = toCodePoints(str);
  const out = new Uint8Array(cps.length * 4);
  const dv = new DataView(out.buffer);
  for (let i = 0; i < cps.length; i++) {
    dv.setUint32(i * 4, cps[i], true);
  }
  return out;
}

function decodeUtf32(bytes: Uint8Array): string {
  if (bytes.length === 0) return '';
  // Detect BOM
  let be = false,
    offset = 0;
  if (bytes.length >= 4) {
    const b0 = bytes[0],
      b1 = bytes[1],
      b2 = bytes[2],
      b3 = bytes[3];
    // UTF-32BE BOM: 00 00 FE FF
    if (b0 === 0x00 && b1 === 0x00 && b2 === 0xfe && b3 === 0xff) {
      be = true;
      offset = 4;
    }
    // UTF-32LE BOM: FF FE 00 00
    else if (b0 === 0xff && b1 === 0xfe && b2 === 0x00 && b3 === 0x00) {
      be = false;
      offset = 4;
    }
  }
  if (((bytes.length - offset) & 3) !== 0) {
    // truncate to a multiple of 4
    bytes = bytes.slice(0, bytes.length - ((bytes.length - offset) & 3));
  }
  const dv = new DataView(bytes.buffer, bytes.byteOffset + offset, bytes.byteLength - offset);
  const cps: number[] = [];
  for (let i = 0; i < dv.byteLength; i += 4) {
    const cp = dv.getUint32(i, !be);
    cps.push(sanitizeCodePoint(cp));
  }
  return String.fromCodePoint(...cps);
}
