import 'jasmine/src/jasmine';
import {
  consumeCssNumber,
  isPlainCssNumber,
  parseCssFunction,
  parseCssFunctionCall,
  readPreviousWhitespaceSeparatedToken,
  readWhitespaceSeparatedToken,
} from '../src/utils/cssScanner';

describe('cssScanner', () => {
  it('consumes plain CSS numbers', () => {
    expect(consumeCssNumber('-12.5px', 0)).toBe(5);
    expect(consumeCssNumber('.5', 0)).toBe(2);
    expect(isPlainCssNumber('12.5')).toBeTrue();
    expect(isPlainCssNumber('12px')).toBeFalse();
  });

  it('reads forward and backward whitespace-separated tokens', () => {
    expect(readWhitespaceSeparatedToken('  one two ', 0)).toEqual({ token: 'one', startIndex: 2, nextIndex: 5 });
    expect(readPreviousWhitespaceSeparatedToken('  one two ', 10)).toEqual({
      token: 'two',
      startIndex: 6,
      nextIndex: 9,
    });
  });

  it('parses CSS function calls with nested parameters', () => {
    const call = parseCssFunctionCall('  repeat(2, minmax(40px, 1fr) 20px) trailing', 0);

    expect(call?.name).toBe('repeat');
    expect(call?.parameters).toEqual(['2', 'minmax(40px, 1fr) 20px']);
    expect(call?.nextIndex).toBe(35);
    expect(parseCssFunction('rgba(1, 2, calc(3 + 4))')).toEqual({
      name: 'rgba',
      parameters: ['1', '2', 'calc(3 + 4)'],
    });
  });
});
