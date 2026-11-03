// eslint-disable-next-line no-control-regex
const ANSI_PATTERN =
  /[\u001b\u009b][[\]()#;?]*(?:(?:(?:[a-zA-Z\d]*(?:;[-a-zA-Z\d/#&.:=?%@~_]*)*)?\u0007)|(?:(?:\d{1,4}(?:;\d{0,4})*)?[\dA-PR-TZcf-ntqry=><~]))/g;

/** Removes ANSI escape sequences, leaving the visible text. */
export function stripAnsi(text: string): string {
  return text.replace(ANSI_PATTERN, '');
}

/** How many colors an output supports. Mirrors chalk's levels; the memory sink takes it as a number. */
export const enum ColorLevel {
  None = 0,
  Basic = 1,
  Ansi256 = 2,
  TrueColor = 3,
}

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

// The 16 terminal colors as RGB. Terminals theme these, so the exact values only matter for the
// downgrade to a lower color level, which works on RGB.
const ANSI16_RGB: ReadonlyArray<number> = [
  0x000000, 0xcd0000, 0x00cd00, 0xcdcd00, 0x0000ee, 0xcd00cd, 0x00cdcd, 0xe5e5e5, 0x7f7f7f, 0xff0000, 0x00ff00,
  0xffff00, 0x5c5cff, 0xff00ff, 0x00ffff, 0xffffff,
];

function clampChannel(value: number): number {
  return Math.max(0, Math.min(255, Math.round(value)));
}

function toHex(r: number, g: number, b: number): string {
  const channel = (value: number): string => clampChannel(value).toString(16).padStart(2, '0');
  return `#${channel(r)}${channel(g)}${channel(b)}`;
}

function packedToHex(packed: number): string {
  return toHex((packed >> 16) & 0xff, (packed >> 8) & 0xff, packed & 0xff);
}

function ansi256ToHex(code: number): string {
  if (code < 16) {
    return packedToHex(ANSI16_RGB[code]);
  }
  if (code >= 232) {
    const gray = (code - 232) * 10 + 8;
    return toHex(gray, gray, gray);
  }
  const index = code - 16;
  const level = (n: number): number => (n === 0 ? 0 : 55 + n * 40);
  return toHex(level(Math.floor(index / 36)), level(Math.floor((index % 36) / 6)), level(index % 6));
}

/**
 * Resolves a color string to a `#rrggbb` value usable as a Valdi color attribute. Accepts Ink/chalk
 * names (`'green'`, `'redBright'`), CSS names, hex, `rgb()` and `ansi256(n)`. Returns undefined for
 * unknown colors and for fully transparent ones.
 */
export function parseColor(input: string | undefined): string | undefined {
  if (!input) {
    return undefined;
  }
  const value = input.trim();
  if (value === '' || value === 'transparent') {
    return undefined;
  }
  const ansi = ANSI_NAMES[value];
  if (ansi !== undefined) {
    return packedToHex(ANSI16_RGB[ansi]);
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
    return packedToHex(number);
  }
  const fn = /^([a-zA-Z0-9]+)\s*\((.*)\)$/.exec(value);
  if (fn) {
    const name = fn[1].toLowerCase();
    const args = fn[2].split(/[\s,/]+/).filter(part => part.length > 0);
    if ((name === 'rgb' || name === 'rgba') && args.length >= 3) {
      if (args.length >= 4 && parseFloat(args[3]) === 0) {
        return undefined;
      }
      return toHex(parseFloat(args[0]), parseFloat(args[1]), parseFloat(args[2]));
    }
    if ((name === 'ansi256' || name === 'ansi') && args.length >= 1) {
      const code = parseInt(args[0], 10);
      if (isNaN(code)) {
        return undefined;
      }
      return ansi256ToHex(clampChannel(code));
    }
    return undefined;
  }
  const css = CSS_NAMES[value.toLowerCase()];
  if (css !== undefined) {
    return packedToHex(css);
  }
  const lowerAnsi = ANSI_NAMES[value.toLowerCase()];
  if (lowerAnsi !== undefined) {
    return packedToHex(ANSI16_RGB[lowerAnsi]);
  }
  return undefined;
}
