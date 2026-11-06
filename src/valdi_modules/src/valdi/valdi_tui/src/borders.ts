/** Characters used to draw a box border. Same shapes as Ink's `borderStyle` names. */
export interface BorderCharacters {
  readonly topLeft: string;
  readonly top: string;
  readonly topRight: string;
  readonly right: string;
  readonly bottomRight: string;
  readonly bottom: string;
  readonly bottomLeft: string;
  readonly left: string;
}

export type BorderStyleName =
  'single' | 'double' | 'round' | 'bold' | 'singleDouble' | 'doubleSingle' | 'classic' | 'arrow';

export type BorderStyle = BorderStyleName | BorderCharacters;

const BORDER_STYLES: { readonly [name in BorderStyleName]: BorderCharacters } = {
  single: {
    topLeft: '┌',
    top: '─',
    topRight: '┐',
    right: '│',
    bottomRight: '┘',
    bottom: '─',
    bottomLeft: '└',
    left: '│',
  },
  double: {
    topLeft: '╔',
    top: '═',
    topRight: '╗',
    right: '║',
    bottomRight: '╝',
    bottom: '═',
    bottomLeft: '╚',
    left: '║',
  },
  round: {
    topLeft: '╭',
    top: '─',
    topRight: '╮',
    right: '│',
    bottomRight: '╯',
    bottom: '─',
    bottomLeft: '╰',
    left: '│',
  },
  bold: {
    topLeft: '┏',
    top: '━',
    topRight: '┓',
    right: '┃',
    bottomRight: '┛',
    bottom: '━',
    bottomLeft: '┗',
    left: '┃',
  },
  singleDouble: {
    topLeft: '╓',
    top: '─',
    topRight: '╖',
    right: '║',
    bottomRight: '╜',
    bottom: '─',
    bottomLeft: '╙',
    left: '║',
  },
  doubleSingle: {
    topLeft: '╒',
    top: '═',
    topRight: '╕',
    right: '│',
    bottomRight: '╛',
    bottom: '═',
    bottomLeft: '╘',
    left: '│',
  },
  classic: {
    topLeft: '+',
    top: '-',
    topRight: '+',
    right: '|',
    bottomRight: '+',
    bottom: '-',
    bottomLeft: '+',
    left: '|',
  },
  arrow: {
    topLeft: '↘',
    top: '↓',
    topRight: '↙',
    right: '←',
    bottomRight: '↖',
    bottom: '↑',
    bottomLeft: '↗',
    left: '→',
  },
};

export function resolveBorderStyle(style: unknown): BorderCharacters | undefined {
  if (typeof style === 'string') {
    return BORDER_STYLES[style as BorderStyleName];
  }
  if (style && typeof style === 'object' && typeof (style as BorderCharacters).top === 'string') {
    return style as BorderCharacters;
  }
  return undefined;
}
