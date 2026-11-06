import 'jasmine/src/jasmine';
import { ANSI, ColorLevel, detectColorLevel, parseColor, rgbToAnsi256, stripAnsi, styleToSgr } from '../src/ansi';

describe('ansi', () => {
  describe('parseColor', () => {
    it('parses Ink color names', () => {
      expect(parseColor('green')).toEqual({ kind: 'ansi', code: 2 });
      expect(parseColor('redBright')).toEqual({ kind: 'ansi', code: 9 });
      expect(parseColor('gray')).toEqual({ kind: 'ansi', code: 8 });
    });

    it('parses hex colors', () => {
      expect(parseColor('#ff8800')).toEqual({ kind: 'rgb', r: 255, g: 136, b: 0 });
      expect(parseColor('#f80')).toEqual({ kind: 'rgb', r: 255, g: 136, b: 0 });
      expect(parseColor('#ff880000')).toBeUndefined();
    });

    it('parses rgb() and ansi256()', () => {
      expect(parseColor('rgb(1, 2, 3)')).toEqual({ kind: 'rgb', r: 1, g: 2, b: 3 });
      expect(parseColor('rgba(1, 2, 3, 0)')).toBeUndefined();
      expect(parseColor('ansi256(200)')).toEqual({ kind: 'ansi256', code: 200 });
    });

    it('parses CSS names and ignores transparent', () => {
      expect(parseColor('orange')).toEqual({ kind: 'rgb', r: 255, g: 165, b: 0 });
      expect(parseColor('transparent')).toBeUndefined();
      expect(parseColor('not-a-color')).toBeUndefined();
    });
  });

  describe('styleToSgr', () => {
    it('emits nothing without colors', () => {
      expect(styleToSgr({ bold: true, color: 'red' }, ColorLevel.None)).toBe('');
      expect(styleToSgr(undefined, ColorLevel.TrueColor)).toBe('');
    });

    it('downgrades colors to the supported level', () => {
      expect(styleToSgr({ color: '#ff0000' }, ColorLevel.TrueColor)).toBe('\x1b[38;2;255;0;0m');
      expect(styleToSgr({ color: '#ff0000' }, ColorLevel.Ansi256)).toBe(`\x1b[38;5;${rgbToAnsi256(255, 0, 0)}m`);
      expect(styleToSgr({ color: '#ff0000' }, ColorLevel.Basic)).toBe('\x1b[91m');
      expect(styleToSgr({ backgroundColor: 'blue', bold: true }, ColorLevel.Basic)).toBe('\x1b[1;44m');
    });

    it('maps text attributes', () => {
      expect(
        styleToSgr({ dim: true, italic: true, underline: true, inverse: true, strikethrough: true }, ColorLevel.Basic),
      ).toBe('\x1b[2;3;4;7;9m');
    });
  });

  describe('detectColorLevel', () => {
    it('respects NO_COLOR and FORCE_COLOR', () => {
      expect(detectColorLevel({ NO_COLOR: '1', COLORTERM: 'truecolor' }, true)).toBe(ColorLevel.None);
      expect(detectColorLevel({ FORCE_COLOR: '3' }, false)).toBe(ColorLevel.TrueColor);
      expect(detectColorLevel({ FORCE_COLOR: '0' }, true)).toBe(ColorLevel.None);
    });

    it('disables colors when not a tty', () => {
      expect(detectColorLevel({ TERM: 'xterm-256color' }, false)).toBe(ColorLevel.None);
    });

    it('detects truecolor and 256 colors', () => {
      expect(detectColorLevel({ COLORTERM: 'truecolor' }, true)).toBe(ColorLevel.TrueColor);
      expect(detectColorLevel({ TERM: 'xterm-256color' }, true)).toBe(ColorLevel.Ansi256);
      expect(detectColorLevel({ TERM: 'xterm' }, true)).toBe(ColorLevel.Basic);
      expect(detectColorLevel({ TERM: 'dumb' }, true)).toBe(ColorLevel.None);
    });
  });

  it('erases lines upward', () => {
    expect(ANSI.eraseLines(0)).toBe('');
    expect(ANSI.eraseLines(2)).toBe('\x1b[2K\x1b[1A\x1b[2K\x1b[G');
  });

  it('strips escape sequences', () => {
    expect(stripAnsi('\x1b[1mbold\x1b[0m plain')).toBe('bold plain');
  });
});
