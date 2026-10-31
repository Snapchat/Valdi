import 'jasmine/src/jasmine';
import { clusterize, stringWidth } from '../src/textWidth';

describe('textWidth', () => {
  it('counts ascii as one column each', () => {
    expect(stringWidth('hello')).toBe(5);
    expect(stringWidth('')).toBe(0);
  });

  it('counts CJK and emoji as two columns', () => {
    expect(stringWidth('日本')).toBe(4);
    expect(stringWidth('😀')).toBe(2);
    expect(stringWidth('a😀b')).toBe(4);
  });

  it('ignores combining marks and escape sequences', () => {
    expect(stringWidth('é')).toBe(1);
    expect(stringWidth('\x1b[31mred\x1b[0m')).toBe(3);
  });

  it('keeps ZWJ sequences and flags as one cluster', () => {
    const family = clusterize('👨‍👩‍👧');
    expect(family.length).toBe(1);
    expect(family[0]!.width).toBe(2);

    const flag = clusterize('🇫🇷x');
    expect(flag.length).toBe(2);
    expect(flag[0]!.width).toBe(2);
    expect(flag[1]!.text).toBe('x');
  });

  it('widens text symbols with emoji presentation', () => {
    expect(stringWidth('❤')).toBe(1);
    expect(stringWidth('❤️')).toBe(2);
  });

  it('treats tabs and control characters as zero width', () => {
    expect(stringWidth('\u0007')).toBe(0);
  });
});
