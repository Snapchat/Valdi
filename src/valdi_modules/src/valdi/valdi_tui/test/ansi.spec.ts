import 'jasmine/src/jasmine';
import { parseColor, stripAnsi } from '../src/ansi';

describe('ansi', () => {
  describe('parseColor', () => {
    it('resolves Ink color names to hex', () => {
      expect(parseColor('green')).toBe('#00cd00');
      expect(parseColor('redBright')).toBe('#ff0000');
      expect(parseColor('gray')).toBe('#7f7f7f');
    });

    it('normalizes hex colors', () => {
      expect(parseColor('#ff8800')).toBe('#ff8800');
      expect(parseColor('#f80')).toBe('#ff8800');
      expect(parseColor('#ff8800ff')).toBe('#ff8800');
      expect(parseColor('#ff880000')).toBeUndefined();
      expect(parseColor('#ff888')).toBeUndefined();
    });

    it('parses rgb() and ansi256()', () => {
      expect(parseColor('rgb(1, 2, 3)')).toBe('#010203');
      expect(parseColor('rgba(1, 2, 3, 0)')).toBeUndefined();
      expect(parseColor('ansi256(196)')).toBe('#ff0000');
      expect(parseColor('ansi256(244)')).toBe('#808080');
      expect(parseColor('ansi256(2)')).toBe('#00cd00');
    });

    it('parses CSS names and ignores transparent', () => {
      expect(parseColor('orange')).toBe('#ffa500');
      expect(parseColor('transparent')).toBeUndefined();
      expect(parseColor('not-a-color')).toBeUndefined();
      expect(parseColor(undefined)).toBeUndefined();
    });
  });

  it('strips escape sequences', () => {
    expect(stripAnsi('\x1b[1mbold\x1b[0m plain')).toBe('bold plain');
  });
});
