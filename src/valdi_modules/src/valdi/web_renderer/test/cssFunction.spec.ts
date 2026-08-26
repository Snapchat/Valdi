import 'jasmine/src/jasmine';
import { parseCssFunction } from '../src/utils/cssFunction';

describe('cssFunction', () => {
  it('parses CSS functions with nested functions and quoted commas', () => {
    const parsed = parseCssFunction(' linear-gradient(45deg, rgba(10, 20, 30, 0.5), "literal, comma") ');

    expect(parsed).toEqual({
      name: 'linear-gradient',
      parameters: ['45deg', 'rgba(10, 20, 30, 0.5)', '"literal, comma"'],
    });
  });

  it('rejects malformed CSS function text', () => {
    expect(parseCssFunction('rgba(1, 2, 3')).toBeUndefined();
    expect(parseCssFunction('rgba(1, 2, 3))')).toBeUndefined();
    expect(parseCssFunction('not a function')).toBeUndefined();
  });
});
