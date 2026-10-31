import { TextStyle } from './TextStyle';

const ESC = '\x1b[';

export const ANSI = {
  cursorHide: `${ESC}?25l`,
  cursorShow: `${ESC}?25h`,
  cursorLeft: `${ESC}G`,
  eraseLine: `${ESC}2K`,
  eraseDown: `${ESC}J`,
  clearTerminal: `${ESC}2J${ESC}3J${ESC}H`,
  reset: `${ESC}0m`,
  cursorUp(count: number): string {
    return count > 0 ? `${ESC}${count}A` : '';
  },
  /** Erases `count` lines ending at the cursor line and leaves the cursor at the start of the first one. */
  eraseLines(count: number): string {
    let out = '';
    for (let i = 0; i < count; i++) {
      out += ANSI.eraseLine;
      if (i < count - 1) {
        out += ANSI.cursorUp(1);
      }
    }
    if (count > 0) {
      out += ANSI.cursorLeft;
    }
    return out;
  },
};

// eslint-disable-next-line no-control-regex
const ANSI_PATTERN =
  /[\u001b\u009b][[\]()#;?]*(?:(?:(?:[a-zA-Z\d]*(?:;[-a-zA-Z\d/#&.:=?%@~_]*)*)?\u0007)|(?:(?:\d{1,4}(?:;\d{0,4})*)?[\dA-PR-TZcf-ntqry=><~]))/g;

export function stripAnsi(text: string): string {
  return text.replace(ANSI_PATTERN, '');
}

/** How many colors the output supports. Mirrors chalk's levels. */
export const enum ColorLevel {
  None = 0,
  Basic = 1,
  Ansi256 = 2,
  TrueColor = 3,
}

export interface ColorEnv {
  readonly [name: string]: string | undefined;
}

export function detectColorLevel(env: ColorEnv, isTTY: boolean): ColorLevel {
  if (env.NO_COLOR !== undefined && env.NO_COLOR !== '') {
    return ColorLevel.None;
  }
  const force = env.FORCE_COLOR;
  if (force !== undefined) {
    if (force === '' || force === 'true') {
      return ColorLevel.Basic;
    }
    if (force === 'false') {
      return ColorLevel.None;
    }
    const level = parseInt(force, 10);
    if (!isNaN(level)) {
      return Math.max(0, Math.min(3, level)) as ColorLevel;
    }
  }
  if (!isTTY) {
    return ColorLevel.None;
  }
  const term = env.TERM ?? '';
  if (term === 'dumb') {
    return ColorLevel.None;
  }
  const colorTerm = env.COLORTERM ?? '';
  if (colorTerm === 'truecolor' || colorTerm === '24bit') {
    return ColorLevel.TrueColor;
  }
  const program = env.TERM_PROGRAM ?? '';
  if (program === 'iTerm.app' || program === 'vscode' || program === 'WezTerm' || program === 'ghostty') {
    return ColorLevel.TrueColor;
  }
  if (program === 'Apple_Terminal' || /-256(color)?$/.test(term)) {
    return ColorLevel.Ansi256;
  }
  if (/^(xterm|screen|tmux|vt100|vt220|rxvt|linux|cygwin|ansi)/.test(term)) {
    return ColorLevel.Basic;
  }
  return term ? ColorLevel.Basic : ColorLevel.None;
}

export type ParsedColor =
  | { readonly kind: 'ansi'; readonly code: number }
  | { readonly kind: 'ansi256'; readonly code: number }
  | { readonly kind: 'rgb'; readonly r: number; readonly g: number; readonly b: number };

const ANSI_NAMES: { readonly [name: string]: number } = {
  black: 0,
  red: 1,
  green: 2,
  yellow: 3,
  blue: 4,
  magenta: 5,
  cyan: 6,
  white: 7,
  gray: 8,
  grey: 8,
  blackBright: 8,
  redBright: 9,
  greenBright: 10,
  yellowBright: 11,
  blueBright: 12,
  magentaBright: 13,
  cyanBright: 14,
  whiteBright: 15,
};

// CSS named colors commonly used with Valdi components, so the same strings work on device and in a terminal.
const CSS_NAMES: { readonly [name: string]: number } = {
  aqua: 0x00ffff,
  beige: 0xf5f5dc,
  brown: 0xa52a2a,
  chocolate: 0xd2691e,
  coral: 0xff7f50,
  crimson: 0xdc143c,
  darkblue: 0x00008b,
  darkgray: 0xa9a9a9,
  darkgreen: 0x006400,
  darkgrey: 0xa9a9a9,
  darkorange: 0xff8c00,
  darkred: 0x8b0000,
  deeppink: 0xff1493,
  deepskyblue: 0x00bfff,
  dimgray: 0x696969,
  dodgerblue: 0x1e90ff,
  fuchsia: 0xff00ff,
  gold: 0xffd700,
  hotpink: 0xff69b4,
  indigo: 0x4b0082,
  ivory: 0xfffff0,
  khaki: 0xf0e68c,
  lavender: 0xe6e6fa,
  lightblue: 0xadd8e6,
  lightgray: 0xd3d3d3,
  lightgreen: 0x90ee90,
  lightgrey: 0xd3d3d3,
  lightyellow: 0xffffe0,
  lime: 0x00ff00,
  maroon: 0x800000,
  navy: 0x000080,
  olive: 0x808000,
  orange: 0xffa500,
  orchid: 0xda70d6,
  pink: 0xffc0cb,
  plum: 0xdda0dd,
  purple: 0x800080,
  salmon: 0xfa8072,
  silver: 0xc0c0c0,
  skyblue: 0x87ceeb,
  slategray: 0x708090,
  snow: 0xfffafa,
  steelblue: 0x4682b4,
  tan: 0xd2b48c,
  teal: 0x008080,
  tomato: 0xff6347,
  turquoise: 0x40e0d0,
  violet: 0xee82ee,
  wheat: 0xf5deb3,
};

function clampChannel(value: number): number {
  return Math.max(0, Math.min(255, Math.round(value)));
}

/**
 * Parses a color string. Returns undefined for unknown colors and for fully transparent ones.
 */
export function parseColor(input: string | undefined): ParsedColor | undefined {
  if (!input) {
    return undefined;
  }
  const value = input.trim();
  if (value === '' || value === 'transparent') {
    return undefined;
  }
  const ansi = ANSI_NAMES[value];
  if (ansi !== undefined) {
    return { kind: 'ansi', code: ansi };
  }
  if (value.charAt(0) === '#') {
    let hex = value.substring(1);
    if (hex.length === 3 || hex.length === 4) {
      hex = hex
        .split('')
        .map(c => c + c)
        .join('');
    }
    if (hex.length !== 6 && hex.length !== 8) {
      return undefined;
    }
    const number = parseInt(hex.substring(0, 6), 16);
    if (isNaN(number)) {
      return undefined;
    }
    if (hex.length === 8 && parseInt(hex.substring(6, 8), 16) === 0) {
      return undefined;
    }
    return { kind: 'rgb', r: (number >> 16) & 0xff, g: (number >> 8) & 0xff, b: number & 0xff };
  }
  const fn = /^([a-zA-Z0-9]+)\s*\((.*)\)$/.exec(value);
  if (fn) {
    const name = fn[1]!.toLowerCase();
    const args = fn[2]!.split(/[\s,/]+/).filter(part => part.length > 0);
    if ((name === 'rgb' || name === 'rgba') && args.length >= 3) {
      if (args.length >= 4 && parseFloat(args[3]!) === 0) {
        return undefined;
      }
      return {
        kind: 'rgb',
        r: clampChannel(parseFloat(args[0]!)),
        g: clampChannel(parseFloat(args[1]!)),
        b: clampChannel(parseFloat(args[2]!)),
      };
    }
    if ((name === 'ansi256' || name === 'ansi') && args.length >= 1) {
      const code = parseInt(args[0]!, 10);
      if (isNaN(code)) {
        return undefined;
      }
      return name === 'ansi' && code < 16 ? { kind: 'ansi', code } : { kind: 'ansi256', code: clampChannel(code) };
    }
    return undefined;
  }
  const css = CSS_NAMES[value.toLowerCase()];
  if (css !== undefined) {
    return { kind: 'rgb', r: (css >> 16) & 0xff, g: (css >> 8) & 0xff, b: css & 0xff };
  }
  const lowerAnsi = ANSI_NAMES[value.toLowerCase()];
  if (lowerAnsi !== undefined) {
    return { kind: 'ansi', code: lowerAnsi };
  }
  return undefined;
}

export function rgbToAnsi256(r: number, g: number, b: number): number {
  if (r === g && g === b) {
    if (r < 8) {
      return 16;
    }
    if (r > 248) {
      return 231;
    }
    return Math.round(((r - 8) / 247) * 24) + 232;
  }
  return 16 + 36 * Math.round((r / 255) * 5) + 6 * Math.round((g / 255) * 5) + Math.round((b / 255) * 5);
}

export function rgbToAnsi16(r: number, g: number, b: number): number {
  const value = Math.max(r, g, b) / 255;
  if (value === 0) {
    return 0;
  }
  let code = (Math.round(b / 255) << 2) | (Math.round(g / 255) << 1) | Math.round(r / 255);
  if (value > 0.5) {
    code += 8;
  }
  return code;
}

export function ansi256ToAnsi16(code: number): number {
  if (code < 16) {
    return code;
  }
  if (code >= 232) {
    const gray = ((code - 232) * 10 + 8) & 0xff;
    return rgbToAnsi16(gray, gray, gray);
  }
  const index = code - 16;
  const r = Math.floor(index / 36) * 51;
  const g = Math.floor((index % 36) / 6) * 51;
  const b = (index % 6) * 51;
  return rgbToAnsi16(r, g, b);
}

function colorCodes(color: ParsedColor, level: ColorLevel, background: boolean): string {
  const base = background ? 40 : 30;
  const extended = background ? 48 : 38;
  let resolved = color;
  if (level < ColorLevel.TrueColor && resolved.kind === 'rgb') {
    resolved = { kind: 'ansi256', code: rgbToAnsi256(resolved.r, resolved.g, resolved.b) };
  }
  if (level < ColorLevel.Ansi256 && resolved.kind === 'ansi256') {
    resolved = { kind: 'ansi', code: ansi256ToAnsi16(resolved.code) };
  }
  switch (resolved.kind) {
    case 'rgb':
      return `${extended};2;${resolved.r};${resolved.g};${resolved.b}`;
    case 'ansi256':
      return `${extended};5;${resolved.code}`;
    case 'ansi':
      return resolved.code < 8 ? `${base + resolved.code}` : `${base + 60 + resolved.code - 8}`;
  }
}

/** The SGR sequence that enables `style`, or '' when nothing is set or colors are disabled. */
export function styleToSgr(style: TextStyle | undefined, level: ColorLevel): string {
  if (!style || level === ColorLevel.None) {
    return '';
  }
  const codes: string[] = [];
  if (style.bold) {
    codes.push('1');
  }
  if (style.dim) {
    codes.push('2');
  }
  if (style.italic) {
    codes.push('3');
  }
  if (style.underline) {
    codes.push('4');
  }
  if (style.inverse) {
    codes.push('7');
  }
  if (style.strikethrough) {
    codes.push('9');
  }
  const fg = parseColor(style.color);
  if (fg) {
    codes.push(colorCodes(fg, level, false));
  }
  const bg = parseColor(style.backgroundColor);
  if (bg) {
    codes.push(colorCodes(bg, level, true));
  }
  return codes.length > 0 ? `${ESC}${codes.join(';')}m` : '';
}

export function styledText(text: string, style: TextStyle | undefined, level: ColorLevel): string {
  const open = styleToSgr(style, level);
  return open ? `${open}${text}${ANSI.reset}` : text;
}
