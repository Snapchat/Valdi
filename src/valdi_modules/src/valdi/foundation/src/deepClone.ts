/**
 * Deep clone the object.
 *
 * Note that we should use structuredClone(), once valdi upgrade to Node.js 17.
 */
const deepClone = <T>(val: T, seen = new WeakMap()): T => {
  // Handle primitive types
  if (val === null || typeof val !== 'object') {
    return val;
  }

  // Handle circular references
  if (seen.has(val)) {
    return seen.get(val) as T;
  }

  // handle Array
  if (Array.isArray(val)) {
    const result = val.slice(0) as T;
    seen.set(val, result);
    val.forEach((item: unknown, i) => {
      // No easy way to type this, so we use any
      // eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/no-unsafe-member-access
      (result as any)[i] = deepClone(item, seen);
    });
    return result;
  }

  // handle Date
  if (val instanceof Date) {
    return new Date(val.valueOf()) as T;
  }
  // handle RegExp
  if (val instanceof RegExp) {
    return new RegExp(val) as T;
  }
  // handle Map
  if (val instanceof Map) {
    const result = new Map();
    seen.set(val, result);
    val.forEach((value, key) => {
      result.set(key, deepClone(value, seen));
    });
    return result as T;
  }
  // handle Set
  if (val instanceof Set) {
    const result = new Set();
    seen.set(val, result);
    val.forEach(value => {
      result.add(deepClone(value, seen));
    });
    return result as T;
  }
  // handle ArrayBuffer
  if (val instanceof ArrayBuffer) {
    return val.slice(0) as T;
  }
  // handle Long
  if (val instanceof Long) {
    return Long.fromValue(val) as T;
  }
  // handle plain object
  if (val.constructor === Object) {
    return deepClonePlainObject(val, seen) as T;
  }
  // handle Uint8Array
  if (val instanceof Uint8Array) {
    return new Uint8Array(val) as T;
  }
  // handle protobuf Message
  if (isClonableProtobufMessage(val)) {
    const cloned = val.clone() as T;
    seen.set(val, cloned);
    return cloned;
  }

  throw new Error(`Unsupported type in deepClone ${typeof val} ${val.constructor.name}`);
};

const deepClonePlainObject = <T extends object>(obj: T, seen: WeakMap<object, unknown>): T => {
  const cloned = {} as T;
  seen.set(obj, cloned);
  for (const key in obj) {
    const val = obj[key];
    cloned[key] = deepClone(val, seen);
  }
  return cloned;
};

export default deepClone;

// Object with a clone method (Protobuf Message)
interface Cloneable {
  clone(): unknown;
}

// Check if value is a protobuf Message (has $arena property and clone method)
function isClonableProtobufMessage(val: object): val is Cloneable {
  return '$arena' in val && 'clone' in val && typeof val.clone === 'function';
}
