/**
 * Visual attributes of a run of terminal text. Colors accept the same strings as
 * {@link parseColor}: Ink/chalk names (`'green'`, `'redBright'`), CSS names, hex,
 * `rgb()` and `ansi256(n)`.
 */
export interface TextStyle {
  readonly color?: string;
  readonly backgroundColor?: string;
  readonly bold?: boolean;
  readonly dim?: boolean;
  readonly italic?: boolean;
  readonly underline?: boolean;
  readonly strikethrough?: boolean;
  readonly inverse?: boolean;
}

const STYLE_KEYS: ReadonlyArray<keyof TextStyle> = [
  'color',
  'backgroundColor',
  'bold',
  'dim',
  'italic',
  'underline',
  'strikethrough',
  'inverse',
];

export function isEmptyStyle(style: TextStyle | undefined): boolean {
  if (!style) {
    return true;
  }
  for (const key of STYLE_KEYS) {
    if (style[key] !== undefined && style[key] !== false) {
      return false;
    }
  }
  return true;
}

export function stylesEqual(a: TextStyle | undefined, b: TextStyle | undefined): boolean {
  if (a === b) {
    return true;
  }
  const left = a ?? {};
  const right = b ?? {};
  for (const key of STYLE_KEYS) {
    if ((left[key] ?? false) !== (right[key] ?? false)) {
      return false;
    }
  }
  return true;
}

/** Overlays `overlay` on `base`; defined overlay properties win. */
export function mergeStyles(base: TextStyle | undefined, overlay: TextStyle | undefined): TextStyle | undefined {
  if (isEmptyStyle(overlay)) {
    return base;
  }
  if (isEmptyStyle(base)) {
    return overlay;
  }
  const out: { -readonly [K in keyof TextStyle]: TextStyle[K] } = { ...base };
  for (const key of STYLE_KEYS) {
    const value = overlay![key];
    if (value !== undefined) {
      (out as any)[key] = value;
    }
  }
  return out;
}
