/**
 * Create an array of arbitrary size that contains x times the same value
 */
export function arrayRepeat<T>(size: number, value: T): T[] {
  return arrayGenerate(size, () => value);
}

/**
 * Create an array of arbitrary size by calling a function x times and saving its return value in the array
 */
export function arrayGenerate<T>(size: number, call: (index: number) => T): T[] {
  const result: T[] = [];
  for (let i = 0; i < size; i++) {
    result.push(call(i));
  }
  return result;
}

/*
 * Immutable insert function
 */
export function insert<T>(arr: T[], index: number, newItem: T): T[] {
  return [...arr.slice(0, index), newItem, ...arr.slice(index)];
}

/*
 * Immutable multiInsert function that inserts values into an array at given indeces
 */
export function multiInsert<T>(arr: T[], values: { [index: number]: T }): T[] {
  let numInsertions = 0;
  const clonedArr: T[] = Object.assign([], arr);
  arr.forEach((_, index) => {
    if (values[index]) {
      const clonedIndex = index + numInsertions;
      clonedArr.splice(clonedIndex, 0, values[index]);
      numInsertions += 1;
    }
  });
  return clonedArr;
}

/**
 * Partitions an array based on a predicate, and returns a tuple of two arrays.
 * The first array contains elements that tested positive, the second negative
 */
export function partition<T>(arr: readonly T[], predicate: (element: T) => boolean): [T[], T[]] {
  return arr.reduce<[T[], T[]]>(
    (splitArrays, currentValue) => {
      splitArrays[predicate(currentValue) ? 0 : 1].push(currentValue);
      return splitArrays;
    },
    [[], []],
  );
}

/**
 * Maps every element of an iterable with a transform, and leaves out the elements it returns undefined for
 */
export function compactMap<T, U>(elements: Iterable<T>, transform: (element: T) => U | undefined): U[] {
  const result: U[] = [];
  for (const element of elements) {
    const transformed = transform(element);
    if (transformed !== undefined) {
      result.push(transformed);
    }
  }
  return result;
}
