import 'jasmine/src/jasmine';
import { layoutText, linesToPlainText, measureLines } from '../src/wrap';

describe('wrap', () => {
  it('returns a single line when unconstrained', () => {
    const lines = layoutText([{ text: 'hello world' }], undefined);
    expect(linesToPlainText(lines)).toEqual(['hello world']);
    expect(measureLines(lines)).toEqual([11, 1]);
  });

  it('wraps between words', () => {
    const lines = layoutText([{ text: 'the quick brown fox' }], 10);
    expect(linesToPlainText(lines)).toEqual(['the quick', 'brown fox']);
    expect(measureLines(lines)).toEqual([9, 2]);
  });

  it('hard-breaks words wider than the line', () => {
    expect(linesToPlainText(layoutText([{ text: 'abcdefghij' }], 4))).toEqual(['abcd', 'efgh', 'ij']);
  });

  it('honors hard line breaks', () => {
    expect(linesToPlainText(layoutText([{ text: 'a\nb\r\nc' }], 10))).toEqual(['a', 'b', 'c']);
    expect(measureLines(layoutText([{ text: '' }], 10))).toEqual([0, 1]);
    expect(measureLines(layoutText([{ text: '\n' }], 10))).toEqual([0, 2]);
  });

  it('does not split wide characters', () => {
    expect(linesToPlainText(layoutText([{ text: '日本語' }], 4))).toEqual(['日本', '語']);
  });

  it('truncates with an ellipsis', () => {
    const text = [{ text: 'hello wonderful world' }];
    expect(linesToPlainText(layoutText(text, 10, 'truncate'))).toEqual(['hello won…']);
    expect(linesToPlainText(layoutText(text, 10, 'truncate-start'))).toEqual(['…ful world']);
    expect(linesToPlainText(layoutText(text, 10, 'truncate-middle'))).toEqual(['hello…orld']);
    expect(linesToPlainText(layoutText(text, 1, 'truncate'))).toEqual(['…']);
    expect(linesToPlainText(layoutText([{ text: 'short' }], 10, 'truncate'))).toEqual(['short']);
  });

  it('keeps styles per segment and merges equal neighbors', () => {
    const red = { color: 'red' };
    const lines = layoutText([{ text: 'a', style: red }, { text: 'b', style: red }, { text: 'c' }], 10);
    expect(lines.length).toBe(1);
    expect(lines[0]!.segments.map(segment => segment.text)).toEqual(['ab', 'c']);
    expect(lines[0]!.segments[0]!.style).toEqual(red);
    expect(lines[0]!.segments[1]!.style).toBeUndefined();
  });

  it('wraps across runs with different styles', () => {
    const lines = layoutText([{ text: 'Status: ' }, { text: 'all good', style: { color: 'green' } }], 10);
    expect(linesToPlainText(lines)).toEqual(['Status:', 'all good']);
  });

  it('produces empty lines for zero width', () => {
    expect(measureLines(layoutText([{ text: 'abc' }], 0))).toEqual([0, 1]);
  });
});
