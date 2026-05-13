/** Performs a deep equality check between two values */
export function deepEqual<T>(a: T, b: T): boolean;
export function deepEqual(a: unknown, b: unknown): boolean {
  if (a === b) {
    return true;
  }

  if (a && b && typeof a === 'object' && typeof b === 'object') {
    if (a.constructor !== b.constructor) {
      return false;
    }

    if (Array.isArray(a) && Array.isArray(b)) {
      if (a.length !== b.length) {
        return false;
      }
      for (let i = 0; i < a.length; i++) {
        if (!deepEqual(a[i], b[i])) {
          return false;
        }
      }
      return true;
    }

    if (a instanceof Map && b instanceof Map) {
      if (a.size !== b.size) {
        return false;
      }
      for (const e of Array.from(a.entries())) {
        if (!b.has(e[0])) {
          return false;
        }
      }
      for (const e of Array.from(a.entries())) {
        if (!deepEqual(e[1], b.get(e[0]))) {
          return false;
        }
      }
      return true;
    }

    if (a instanceof Set && b instanceof Set) {
      if (a.size !== b.size) {
        return false;
      }
      for (const e of Array.from(a.entries())) {
        if (!b.has(e[0])) {
          return false;
        }
      }
      return true;
    }

    if (a instanceof RegExp && b instanceof RegExp) {
      return a.source === b.source && a.flags === b.flags;
    }
    if (a instanceof Long && b instanceof Long) {
      // Handle Long objects before valueOf check, which errors for large values
      return a.compare(b) === 0; // returns 0 if equal
    }
    if (a.valueOf !== Object.prototype.valueOf) {
      return a.valueOf() === b.valueOf();
    }
    if (a.toString !== Object.prototype.toString) {
      return a.toString() === b.toString();
    }

    const keys = Object.keys(a);
    if (keys.length !== Object.keys(b).length) {
      return false;
    }
    for (const key of keys) {
      if (!Object.prototype.hasOwnProperty.call(b, key)) {
        return false;
      }
    }
    for (const key of keys) {
      if (!deepEqual((a as Record<string, unknown>)[key], (b as Record<string, unknown>)[key])) {
        return false;
      }
    }
    return true;
  }

  // true if both NaN, false otherwise
  return a !== a && b !== b;
}

/** Performs a shallow equality check between two values */
export function shallowEqual<T>(a: T, b: T): boolean;
export function shallowEqual(previous: unknown, current: unknown): boolean {
  if (previous && current && typeof previous === 'object' && typeof current === 'object') {
    const previousKeys = Object.keys(previous);
    const currentKeys = Object.keys(current);
    if (previousKeys.length !== currentKeys.length) {
      return false;
    }
    return previousKeys.every(
      key => (previous as Record<string, unknown>)[key] === (current as Record<string, unknown>)[key],
    );
  } else {
    return previous === current;
  }
}
