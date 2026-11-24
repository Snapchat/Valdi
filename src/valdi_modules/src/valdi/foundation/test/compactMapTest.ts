import { compactMap } from 'foundation/src/array';
import 'jasmine/src/jasmine';

describe('compactMap', () => {
  it('maps the elements and leaves out the ones the transform returns undefined for', () => {
    expect(compactMap([1, 2, 3, 4], value => (value % 2 === 0 ? `${value}` : undefined))).toEqual(['2', '4']);
  });

  it('keeps null and other falsy values', () => {
    expect(compactMap([0, 1, 2], value => [null, 0, ''][value])).toEqual([null, 0, '']);
  });

  it('accepts any iterable', () => {
    expect(compactMap(new Set(['a', 'b', 'c']), value => (value === 'b' ? undefined : value.toUpperCase()))).toEqual([
      'A',
      'C',
    ]);
  });

  it('returns an empty array for an empty input', () => {
    expect(compactMap([], value => value)).toEqual([]);
  });
});
