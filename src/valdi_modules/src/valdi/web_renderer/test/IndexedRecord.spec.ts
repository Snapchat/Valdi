import 'jasmine/src/jasmine';
import { IndexedRecord } from '../src/utils/IndexedRecord';

function sorted(values: string[]): string[] {
  return values.slice().sort();
}

describe('IndexedRecord', () => {
  it('stores values and tracks keys without duplicating updated keys', () => {
    const record = new IndexedRecord<number>();

    record.set('width', 10);
    record.set('height', 20);
    record.set('width', 30);

    expect(record.empty).toBe(false);
    expect(record.length).toBe(2);
    expect(record.get('width')).toBe(30);
    expect(record.get('height')).toBe(20);
    expect(sorted(record.keys)).toEqual(['height', 'width']);
  });

  it('removes keys in constant time without preserving key order', () => {
    const record = new IndexedRecord<string>();

    record.set('a', 'first');
    record.set('b', 'second');
    record.set('c', 'third');
    record.remove('b');

    expect(record.length).toBe(2);
    expect(record.get('a')).toBe('first');
    expect(record.get('b')).toBeUndefined();
    expect(record.get('c')).toBe('third');
    expect(sorted(record.keys)).toEqual(['a', 'c']);
  });

  it('ignores removal of missing keys', () => {
    const record = new IndexedRecord<number>();

    record.set('x', 1);
    record.remove('missing');

    expect(record.length).toBe(1);
    expect(record.get('x')).toBe(1);
    expect(record.keys).toEqual(['x']);
  });

  it('clears all keys and can be reused', () => {
    const record = new IndexedRecord<number>();

    record.set('x', 1);
    record.set('y', 2);
    record.clear();

    expect(record.empty).toBe(true);
    expect(record.length).toBe(0);
    expect(record.keys).toEqual([]);
    expect(record.get('x')).toBeUndefined();
    expect(record.get('y')).toBeUndefined();

    record.set('z', 3);
    expect(record.length).toBe(1);
    expect(record.keys).toEqual(['z']);
    expect(record.get('z')).toBe(3);
  });

  it('pops the last stored value and removes it from the record', () => {
    const record = new IndexedRecord<number>();

    expect(record.empty).toBe(true);
    expect(record.pop()).toBeUndefined();

    record.set('x', 1);
    record.set('y', 2);

    expect(record.pop()).toBe(2);
    expect(record.length).toBe(1);
    expect(record.get('x')).toBe(1);
    expect(record.get('y')).toBeUndefined();
    expect(record.keys).toEqual(['x']);

    expect(record.pop()).toBe(1);
    expect(record.empty).toBe(true);
    expect(record.pop()).toBeUndefined();
  });

  it('stores falsy values as present values', () => {
    const record = new IndexedRecord<boolean | number | string>();

    record.set('false', false);
    record.set('zero', 0);
    record.set('empty', '');

    expect(record.length).toBe(3);
    expect(record.get('false')).toBe(false);
    expect(record.get('zero')).toBe(0);
    expect(record.get('empty')).toBe('');
    expect(sorted(record.keys)).toEqual(['empty', 'false', 'zero']);
  });
});
