import 'jasmine/src/jasmine';
import { ColorLevel } from '../src/ansi';
import { Screen } from '../src/Screen';

describe('Screen', () => {
  it('writes text and trims trailing blanks', () => {
    const screen = new Screen(10, 2);
    screen.writeText(1, 0, 'hi', undefined, screen.bounds);
    expect(screen.toLines(ColorLevel.None)).toEqual([' hi', '']);
  });

  it('clips to the given rectangle', () => {
    const screen = new Screen(10, 1);
    screen.writeText(0, 0, 'abcdef', undefined, { x: 0, y: 0, width: 3, height: 1 });
    expect(screen.toLines(ColorLevel.None)).toEqual(['abc']);
  });

  it('handles wide characters and partial overwrites', () => {
    const screen = new Screen(6, 1);
    screen.writeText(0, 0, '日本', undefined, screen.bounds);
    expect(screen.toLines(ColorLevel.None)).toEqual(['日本']);
    screen.writeText(1, 0, 'x', undefined, screen.bounds);
    expect(screen.toLines(ColorLevel.None)).toEqual([' x本']);
  });

  it('replaces a wide character that does not fit the clip with a space', () => {
    const screen = new Screen(3, 1);
    screen.writeText(2, 0, '日', undefined, screen.bounds);
    expect(screen.getRowText(0, 0, 3)).toBe('   ');
  });

  it('emits minimal SGR changes', () => {
    const screen = new Screen(6, 1);
    screen.writeText(0, 0, 'ab', { color: 'red' }, screen.bounds);
    screen.writeText(2, 0, 'cd', { color: 'red' }, screen.bounds);
    screen.writeText(4, 0, 'e', undefined, screen.bounds);
    expect(screen.toLines(ColorLevel.Basic)).toEqual(['\x1b[31mabcd\x1b[0me']);
  });

  it('fills backgrounds under existing text', () => {
    const screen = new Screen(4, 1);
    screen.writeText(0, 0, 'ab', { color: 'red' }, screen.bounds);
    screen.fillBackground({ x: 0, y: 0, width: 4, height: 1 }, 'blue', screen.bounds);
    expect(screen.toLines(ColorLevel.Basic)).toEqual(['\x1b[31;44mab\x1b[0m\x1b[44m  \x1b[0m']);
  });

  it('keeps per-column styles when replacing a row', () => {
    const screen = new Screen(5, 1);
    screen.writeText(0, 0, 'abc', { bold: true }, screen.bounds);
    screen.setRowText(0, 0, 5, 'ABCDE', screen.bounds);
    expect(screen.toLines(ColorLevel.Basic)).toEqual(['\x1b[1mABC\x1b[0mDE']);
  });
});
