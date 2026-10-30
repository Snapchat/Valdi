import { stripAnsi } from './ansi';

/** A user-perceived character (approximate grapheme cluster) and the number of terminal columns it occupies. */
export interface Cluster {
  readonly text: string;
  readonly width: number;
}

const ZERO_WIDTH_RANGES: ReadonlyArray<[number, number]> = [
  [0x0300, 0x036f],
  [0x0483, 0x0489],
  [0x0591, 0x05bd],
  [0x0610, 0x061a],
  [0x064b, 0x065f],
  [0x0e31, 0x0e31],
  [0x0e34, 0x0e3a],
  [0x0e47, 0x0e4e],
  [0x1ab0, 0x1aff],
  [0x1dc0, 0x1dff],
  [0x200b, 0x200f],
  [0x2028, 0x202e],
  [0x2060, 0x2064],
  [0x20d0, 0x20ff],
  [0xfe00, 0xfe0f],
  [0xfe20, 0xfe2f],
  [0xfeff, 0xfeff],
  [0x1f3fb, 0x1f3ff],
  [0xe0100, 0xe01ef],
];

const WIDE_RANGES: ReadonlyArray<[number, number]> = [
  [0x1100, 0x115f],
  [0x231a, 0x231b],
  [0x2329, 0x232a],
  [0x23e9, 0x23ec],
  [0x23f0, 0x23f0],
  [0x23f3, 0x23f3],
  [0x25fd, 0x25fe],
  [0x2614, 0x2615],
  [0x2648, 0x2653],
  [0x267f, 0x267f],
  [0x2693, 0x2693],
  [0x26a1, 0x26a1],
  [0x26aa, 0x26ab],
  [0x26bd, 0x26be],
  [0x26c4, 0x26c5],
  [0x26ce, 0x26ce],
  [0x26d4, 0x26d4],
  [0x26ea, 0x26ea],
  [0x26f2, 0x26f3],
  [0x26f5, 0x26f5],
  [0x26fa, 0x26fa],
  [0x26fd, 0x26fd],
  [0x2705, 0x2705],
  [0x270a, 0x270b],
  [0x2728, 0x2728],
  [0x274c, 0x274c],
  [0x274e, 0x274e],
  [0x2753, 0x2755],
  [0x2757, 0x2757],
  [0x2795, 0x2797],
  [0x27b0, 0x27b0],
  [0x27bf, 0x27bf],
  [0x2b1b, 0x2b1c],
  [0x2b50, 0x2b50],
  [0x2b55, 0x2b55],
  [0x2e80, 0x303e],
  [0x3041, 0x33ff],
  [0x3400, 0x4dbf],
  [0x4e00, 0x9fff],
  [0xa000, 0xa4cf],
  [0xa960, 0xa97f],
  [0xac00, 0xd7a3],
  [0xf900, 0xfaff],
  [0xfe10, 0xfe19],
  [0xfe30, 0xfe6f],
  [0xff00, 0xff60],
  [0xffe0, 0xffe6],
  [0x1f004, 0x1f004],
  [0x1f0cf, 0x1f0cf],
  [0x1f18e, 0x1f18e],
  [0x1f191, 0x1f19a],
  [0x1f200, 0x1f251],
  [0x1f300, 0x1f64f],
  [0x1f680, 0x1f6ff],
  [0x1f7e0, 0x1f7eb],
  [0x1f90c, 0x1f9ff],
  [0x1fa70, 0x1faff],
  [0x20000, 0x3fffd],
];

function inRanges(codePoint: number, ranges: ReadonlyArray<[number, number]>): boolean {
  let low = 0;
  let high = ranges.length - 1;
  while (low <= high) {
    const mid = (low + high) >> 1;
    const range = ranges[mid]!;
    if (codePoint < range[0]) {
      high = mid - 1;
    } else if (codePoint > range[1]) {
      low = mid + 1;
    } else {
      return true;
    }
  }
  return false;
}

const ZERO_WIDTH_JOINER = 0x200d;
const VARIATION_SELECTOR_16 = 0xfe0f;

export function isZeroWidth(codePoint: number): boolean {
  return (
    codePoint < 0x20 ||
    (codePoint >= 0x7f && codePoint < 0xa0) ||
    codePoint === ZERO_WIDTH_JOINER ||
    inRanges(codePoint, ZERO_WIDTH_RANGES)
  );
}

export function codePointWidth(codePoint: number): number {
  if (isZeroWidth(codePoint)) {
    return 0;
  }
  return inRanges(codePoint, WIDE_RANGES) ? 2 : 1;
}

function isRegionalIndicator(codePoint: number): boolean {
  return codePoint >= 0x1f1e6 && codePoint <= 0x1f1ff;
}

/**
 * Splits text into clusters: a base code point followed by its combining marks, variation
 * selectors, skin-tone modifiers and ZWJ sequences. Regional indicator pairs form one flag cluster.
 */
export function clusterize(text: string): Cluster[] {
  const clusters: Cluster[] = [];
  let current = '';
  let currentWidth = 0;
  let joinNext = false;
  let pendingRegional = false;

  const flush = () => {
    if (current !== '') {
      clusters.push({ text: current, width: currentWidth });
    }
    current = '';
    currentWidth = 0;
    pendingRegional = false;
  };

  for (const char of text) {
    const codePoint = char.codePointAt(0)!;
    if (codePoint === ZERO_WIDTH_JOINER) {
      if (current === '') {
        continue;
      }
      current += char;
      joinNext = true;
      continue;
    }
    if (joinNext) {
      current += char;
      joinNext = false;
      continue;
    }
    if (isRegionalIndicator(codePoint) && pendingRegional) {
      current += char;
      currentWidth = 2;
      pendingRegional = false;
      continue;
    }
    if (isZeroWidth(codePoint) && current !== '') {
      current += char;
      if (codePoint === VARIATION_SELECTOR_16 && currentWidth === 1) {
        // Emoji presentation requested for a text-default symbol; terminals render it in two cells.
        currentWidth = 2;
      }
      continue;
    }
    flush();
    current = char;
    currentWidth = codePointWidth(codePoint);
    pendingRegional = isRegionalIndicator(codePoint);
  }
  flush();
  return clusters;
}

/** Number of terminal columns `text` occupies. ANSI escape sequences are ignored. */
export function stringWidth(text: string): number {
  let width = 0;
  for (const cluster of clusterize(stripAnsi(text))) {
    width += cluster.width;
  }
  return width;
}

export function clustersWidth(clusters: ReadonlyArray<Cluster>): number {
  let width = 0;
  for (const cluster of clusters) {
    width += cluster.width;
  }
  return width;
}
