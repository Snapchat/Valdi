import { StringMap } from 'coreutils/src/StringMap';
import { Range } from './Range';

/**
 * Check whether two arrays contains the same items. *
 */
export function arrayEquals<T>(left: T[], right: T[]): boolean {
  const length = left.length;
  if (length !== right.length) {
    return false;
  }

  for (let i = 0; i < length; i++) {
    if (left[i] !== right[i]) {
      return false;
    }
  }

  return true;
}

/**
 * Like Array.map() but lazy. It will allocate a new array
 * only when the visitor function returns a different item.
 * @param array
 * @param visitor
 */
export function lazyMap<T>(array: T[] | undefined, visitor: (item: T) => T): T[] {
  if (!array) {
    return [];
  }

  let outputArray = array;

  let index = 0;
  for (const item of array) {
    const newItem = visitor(item);

    if (newItem !== item) {
      if (outputArray === array) {
        outputArray = array.slice(0, array.length);
      }

      outputArray[index] = newItem;
    }

    index++;
  }

  return outputArray;
}

/**
 * Search an exact match item in a sorted array.
 *
 * the compareFn should return >0 to indicate that the given item is "higher"
 * than the searched index, <0 to indicate that the given item is "lower",
 * 0 to indicate a perfect match.
 */
const binarySearchRangeResult: Range = { min: 0, max: 0 };
export function binarySearch<T>(
  array: T[],
  compareFn: (item: T) => number,
  start?: number,
  end?: number,
): number {
  const range = binarySearchRange(array, compareFn, binarySearchRangeResult, start, end);
  if (range.min === range.max) {
    return range.min;
  }
  return -1;
}

/**
 * Search the range that would be a suitable match within an array
 *
 * the compareFn should return >0 to indicate that the given item is "higher"
 * than the searched index, <0 to indicate that the given item is "lower",
 * 0 to indicate a perfect match.
 */
export function binarySearchRange<T>(
  array: T[],
  compareFn: (item: T) => number,
  result?: Range,
  start?: number,
  end?: number,
): Range {
  let lo = start ?? 0;
  let hi = end ?? array.length - 1;

  // Iterate while start not meets end
  while (lo <= hi) {
    // Find the mid index
    const mid = Math.floor((lo + hi) / 2);

    const diff = compareFn(array[mid]);
    if (diff === 0) {
      return {
        min: mid,
        max: mid,
      };
    } else if (diff > 0) {
      hi = mid - 1;
    } else {
      lo = mid + 1;
    }
  }

  const min = Math.min(lo, hi);
  const max = Math.max(lo, hi);
  if (result) {
    result.min = min;
    result.max = max;
    return result;
  } else {
    return {
      min: min,
      max: max,
    };
  }
}

/**
 * Exponential search from a start index within a sorted array.
 * O(log d) where d is the distance from startIdx to the target.
 *
 * the compareFn should return >0 to indicate that the given item is "higher"
 * than the searched index, <0 to indicate that the given item is "lower",
 * 0 to indicate a perfect match.
 */
export function exponentialSearch<T>(
  array: T[],
  compareFn: (item: T) => number,
  startIdx: number = 0,
): number {
  const len = array.length;
  if (len === 0) {
    return -1;
  }

  startIdx = Math.max(0, Math.min(startIdx, len - 1));

  const diff = compareFn(array[startIdx]);
  if (diff === 0) {
    return startIdx;
  }

  let lo: number;
  let hi: number;

  if (diff < 0) {
    // Entry is "lower" than target — search forward.
    let bound = 1;
    while (startIdx + bound < len) {
      const d = compareFn(array[startIdx + bound]);
      if (d === 0) return startIdx + bound;
      if (d > 0) break;
      bound *= 2;
    }
    lo = startIdx + Math.floor(bound / 2) + 1;
    hi = Math.min(startIdx + bound - 1, len - 1);
  } else {
    // Entry is "higher" than target — search backward.
    let bound = 1;
    while (startIdx - bound >= 0) {
      const d = compareFn(array[startIdx - bound]);
      if (d === 0) return startIdx - bound;
      if (d < 0) break;
      bound *= 2;
    }
    lo = Math.max(startIdx - bound + 1, 0);
    hi = startIdx - Math.floor(bound / 2) - 1;
  }

  return lo <= hi ? binarySearch(array, compareFn, lo, hi) : -1;
}

/**
 * Given an array, group the items by the given keySelector into a map.
 */
export function groupBy<T>(array: T[], keySelector: (item: T) => string): StringMap<T[]> {
  const output: StringMap<T[]> = {};

  for (const item of array) {
    const key = keySelector(item);
    let itemsForKey = output[key];
    if (!itemsForKey) {
      itemsForKey = [];
      output[key] = itemsForKey;
    }
    itemsForKey.push(item);
  }

  return output;
}

/**
 * Remove an item from the given array.
 * Return whether the item was succesfully removed.
 */
export function remove<T>(array: T[], item: T): boolean {
  const index = array.indexOf(item);
  if (index < 0) {
    return false;
  }
  array.splice(index, 1);
  return true;
}
