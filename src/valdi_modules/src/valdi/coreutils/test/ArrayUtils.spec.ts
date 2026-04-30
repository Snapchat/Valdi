import { binarySearch, binarySearchRange, exponentialSearch } from 'coreutils/src/ArrayUtils';
import 'jasmine/src/jasmine';

const compareFn = (target: number) => (item: number) => {
  if (item > target) return 1;
  if (item < target) return -1;
  return 0;
};

describe('coreutils > ArrayUtils', () => {
  describe('binarySearch', () => {
    const sorted = [1, 3, 5, 7, 9, 11, 13, 15];

    it('should find an item at the beginning', () => {
      expect(binarySearch(sorted, compareFn(1))).toBe(0);
    });

    it('should find an item at the end', () => {
      expect(binarySearch(sorted, compareFn(15))).toBe(7);
    });

    it('should find an item in the middle', () => {
      expect(binarySearch(sorted, compareFn(7))).toBe(3);
    });

    it('should return -1 for a missing item', () => {
      expect(binarySearch(sorted, compareFn(6))).toBe(-1);
    });

    it('should return -1 for a value below range', () => {
      expect(binarySearch(sorted, compareFn(0))).toBe(-1);
    });

    it('should return -1 for a value above range', () => {
      expect(binarySearch(sorted, compareFn(20))).toBe(-1);
    });

    it('should work with an empty array', () => {
      expect(binarySearch([], compareFn(5))).toBe(-1);
    });

    it('should work with a single-element array (found)', () => {
      expect(binarySearch([10], compareFn(10))).toBe(0);
    });

    it('should work with a single-element array (not found)', () => {
      expect(binarySearch([10], compareFn(5))).toBe(-1);
    });

    it('should respect start and end parameters', () => {
      expect(binarySearch(sorted, compareFn(7), 2, 5)).toBe(3);
      expect(binarySearch(sorted, compareFn(1), 2, 5)).toBe(-1);
    });
  });

  describe('binarySearchRange', () => {
    const sorted = [1, 3, 5, 7, 9, 11, 13, 15];

    it('should return min === max for an exact match', () => {
      const range = binarySearchRange(sorted, compareFn(5));
      expect(range.min).toBe(2);
      expect(range.max).toBe(2);
    });

    it('should return a range for a missing value between elements', () => {
      const range = binarySearchRange(sorted, compareFn(6));
      expect(range.min).toBe(2);
      expect(range.max).toBe(3);
    });

    it('should return a range for a value below all elements', () => {
      const range = binarySearchRange(sorted, compareFn(0));
      expect(range.min).toBe(-1);
      expect(range.max).toBe(0);
    });

    it('should return a range for a value above all elements', () => {
      const range = binarySearchRange(sorted, compareFn(20));
      expect(range.min).toBe(7);
      expect(range.max).toBe(8);
    });

    it('should write into a provided result object', () => {
      const result = { min: 0, max: 0 };
      const returned = binarySearchRange(sorted, compareFn(6), result);
      expect(returned).toBe(result);
      expect(result.min).toBe(2);
      expect(result.max).toBe(3);
    });
  });

  describe('exponentialSearch', () => {
    const sorted = [2, 4, 6, 8, 10, 12, 14, 16, 18, 20];

    it('should find a target searching forward from startIdx', () => {
      expect(exponentialSearch(sorted, compareFn(14), 0)).toBe(6);
    });

    it('should find a target searching backward from startIdx', () => {
      expect(exponentialSearch(sorted, compareFn(4), 8)).toBe(1);
    });

    it('should return the startIdx when it is the target', () => {
      expect(exponentialSearch(sorted, compareFn(10), 4)).toBe(4);
    });

    it('should return -1 for a missing value', () => {
      expect(exponentialSearch(sorted, compareFn(7), 0)).toBe(-1);
    });

    it('should return -1 for an empty array', () => {
      expect(exponentialSearch([], compareFn(5), 0)).toBe(-1);
    });

    it('should find the first element', () => {
      expect(exponentialSearch(sorted, compareFn(2), 0)).toBe(0);
    });

    it('should find the last element', () => {
      expect(exponentialSearch(sorted, compareFn(20), 9)).toBe(9);
    });

    it('should find the first element when starting from the end', () => {
      expect(exponentialSearch(sorted, compareFn(2), 9)).toBe(0);
    });

    it('should find the last element when starting from the beginning', () => {
      expect(exponentialSearch(sorted, compareFn(20), 0)).toBe(9);
    });

    it('should default startIdx to 0', () => {
      expect(exponentialSearch(sorted, compareFn(12))).toBe(5);
    });

    it('should clamp a negative startIdx to 0', () => {
      expect(exponentialSearch(sorted, compareFn(6), -5)).toBe(2);
    });

    it('should clamp a startIdx past the end to the last index', () => {
      expect(exponentialSearch(sorted, compareFn(6), 100)).toBe(2);
    });

    it('should work with a single-element array', () => {
      expect(exponentialSearch([42], compareFn(42), 0)).toBe(0);
      expect(exponentialSearch([42], compareFn(99), 0)).toBe(-1);
    });

    it('should return -1 for a value below all elements', () => {
      expect(exponentialSearch(sorted, compareFn(0), 5)).toBe(-1);
    });

    it('should return -1 for a value above all elements', () => {
      expect(exponentialSearch(sorted, compareFn(100), 5)).toBe(-1);
    });
  });
});
