import { readCssNumber, skipCssWhitespace } from './cssScanner';

const CHAR_COMMA = 44;
const CHAR_LINE_FEED = 10;
const CHAR_SPACE = 32;
const GIF_87A_MAGIC_BYTES: readonly number[] = [0x47, 0x49, 0x46, 0x38, 0x37, 0x61];
const GIF_89A_MAGIC_BYTES: readonly number[] = [0x47, 0x49, 0x46, 0x38, 0x39, 0x61];
const JPEG_MAGIC_BYTES: readonly number[] = [0xff, 0xd8, 0xff];
const PNG_MAGIC_BYTES: readonly number[] = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
const RIFF_MAGIC_BYTES: readonly number[] = [0x52, 0x49, 0x46, 0x46];
const SVG_MAGIC_BYTES: readonly number[] = [0x3c, 0x73, 0x76, 0x67];
const WEBP_MAGIC_BYTES: readonly number[] = [0x57, 0x45, 0x42, 0x50];
const WEBP_MAGIC_BYTES_OFFSET = 8;
const XML_MAGIC_BYTES: readonly number[] = [0x3c, 0x3f, 0x78, 0x6d, 0x6c];

function bytesMatch(bytes: Uint8Array, offset: number, expected: readonly number[]): boolean {
  if (bytes.length < offset + expected.length) {
    return false;
  }
  for (let index = 0; index < expected.length; index++) {
    if (bytes[offset + index] !== expected[index]) {
      return false;
    }
  }
  return true;
}

function isSvg(bytes: Uint8Array): boolean {
  let offset = 0;
  while (bytes[offset] === CHAR_SPACE || bytes[offset] === CHAR_LINE_FEED) {
    offset++;
  }
  return bytesMatch(bytes, offset, SVG_MAGIC_BYTES) || bytesMatch(bytes, offset, XML_MAGIC_BYTES);
}

export function detectImageMimeType(bytes: Uint8Array): string {
  if (bytesMatch(bytes, 0, PNG_MAGIC_BYTES)) {
    return 'image/png';
  }
  if (bytesMatch(bytes, 0, JPEG_MAGIC_BYTES)) {
    return 'image/jpeg';
  }
  if (bytesMatch(bytes, 0, GIF_87A_MAGIC_BYTES) || bytesMatch(bytes, 0, GIF_89A_MAGIC_BYTES)) {
    return 'image/gif';
  }
  if (bytesMatch(bytes, 0, RIFF_MAGIC_BYTES) && bytesMatch(bytes, WEBP_MAGIC_BYTES_OFFSET, WEBP_MAGIC_BYTES)) {
    return 'image/webp';
  }
  if (isSvg(bytes)) {
    return 'image/svg+xml';
  }
  return 'application/octet-stream';
}

function decodeBase64Text(value: string): string {
  const binary = atob(value);
  const bytes = new Uint8Array(binary.length);
  for (let index = 0; index < binary.length; index++) {
    bytes[index] = binary.charCodeAt(index);
  }
  return new TextDecoder().decode(bytes);
}

export function decodeTextDataUrl(value: string, expectedPrefix: string): string | undefined {
  if (!value.startsWith(expectedPrefix)) {
    return undefined;
  }
  const commaIndex = value.indexOf(',');
  if (commaIndex < 0) {
    return undefined;
  }
  const metadata = value.slice(0, commaIndex);
  const payload = value.slice(commaIndex + 1);
  return metadata.includes(';base64') ? decodeBase64Text(payload) : decodeURIComponent(payload);
}

function skipSvgNumberSeparators(value: string, index: number): number {
  let nextIndex = skipCssWhitespace(value, index);
  if (value.charCodeAt(nextIndex) === CHAR_COMMA) {
    nextIndex = skipCssWhitespace(value, nextIndex + 1);
  }
  return nextIndex;
}

function parseSvgNumberList(value: string): number[] | undefined {
  const numbers: number[] = [];
  let index = skipCssWhitespace(value, 0);
  while (index < value.length) {
    const number = readCssNumber(value, index);
    if (!number) {
      return undefined;
    }
    numbers.push(number.value);
    index = skipSvgNumberSeparators(value, number.nextIndex);
  }
  return numbers;
}

export function svgViewBoxIntrinsicSize(src: string): { width: number; height: number } | undefined {
  const text = decodeTextDataUrl(src, 'data:image/svg+xml');
  if (text === undefined) {
    return undefined;
  }
  const viewBoxIndex = text.indexOf('viewBox=');
  if (viewBoxIndex < 0) {
    return undefined;
  }
  const quote = text.charAt(viewBoxIndex + 8);
  if (quote !== '"' && quote !== "'") {
    return undefined;
  }
  const start = viewBoxIndex + 9;
  const end = text.indexOf(quote, start);
  if (end < 0) {
    return undefined;
  }
  const parts = parseSvgNumberList(text.slice(start, end));
  if (!parts || parts.length !== 4 || parts[2] <= 0 || parts[3] <= 0) {
    return undefined;
  }
  return { width: parts[2], height: parts[3] };
}
