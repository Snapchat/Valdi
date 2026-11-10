# Core Utilities (coreutils)

The `coreutils` module provides a collection of essential utility functions and data structures for Valdi applications.

## Overview

Core utilities are foundational tools extracted from `valdi_core` to be available for modules that `valdi_core` depends on. These utilities cover common programming tasks like array manipulation, encoding, caching, and text processing.

## Installation

The `coreutils` module is a standard Valdi module. Add it to your `BUILD.bazel` dependencies:

```python
valdi_module(
    name = "my_module",
    deps = [
        "@valdi//src/valdi_modules/src/valdi/coreutils",
    ],
)
```

## Array Utilities

Functions for working with arrays efficiently.

### `arrayEquals<T>(left: T[], right: T[]): boolean`

Checks whether two arrays contain the same items in the same order.

```typescript
import { arrayEquals } from 'coreutils/src/ArrayUtils';

const arr1 = [1, 2, 3];
const arr2 = [1, 2, 3];
const arr3 = [1, 3, 2];

console.log(arrayEquals(arr1, arr2)); // true
console.log(arrayEquals(arr1, arr3)); // false
```

### `lazyMap<T>(array: T[] | undefined, visitor: (item: T) => T): T[]`

Like `Array.map()` but lazy - allocates a new array only when the visitor function returns a different item. Useful for optimization.

```typescript
import { lazyMap } from 'coreutils/src/ArrayUtils';

const numbers = [1, 2, 3, 4, 5];
const result = lazyMap(numbers, n => n); // Returns original array, no allocation
const doubled = lazyMap(numbers, n => n * 2); // Returns new array
```

### `binarySearch<T>(array: T[], compareFn: (item: T) => number): number`

Searches for an exact match in a sorted array using binary search.

**Parameters:**
- `array`: A sorted array to search
- `compareFn`: Function that returns:
  - `>0` if the given item is "higher" than the searched index
  - `<0` if the given item is "lower"
  - `0` for a perfect match

**Returns:** Index of the found item, or `-1` if not found.

```typescript
import { binarySearch } from 'coreutils/src/ArrayUtils';

const sortedNumbers = [1, 3, 5, 7, 9, 11, 13];
const index = binarySearch(sortedNumbers, item => {
    return item - 7; // Looking for 7
});
console.log(index); // 3
```

### `binarySearchRange<T>(array: T[], compareFn: (item: T) => number, result?: Range): Range`

Searches for a range that would be a suitable match within a sorted array. Returns a `Range` object with `min` and `max` properties.

```typescript
import { binarySearchRange } from 'coreutils/src/ArrayUtils';

const items = [1, 3, 3, 3, 5, 7, 9];
const range = binarySearchRange(items, item => item - 3);
// Returns range of all items matching 3
```

### Other Array Functions

- `arrayGroupBy<T, K>(array: T[], keyFn: (item: T) => K): StringMap<T[]>` - Groups array items by a key
- `arrayUniqueBy<T, K>(array: T[], keyFn: (item: T) => K): T[]` - Returns unique items based on a key function
- `arrayIntersection<T>(left: T[], right: T[]): T[]` - Returns items present in both arrays
- `arrayDifference<T>(left: T[], right: T[]` - Returns items in left but not in right

## Base64 Encoding

Encode and decode Base64 strings, including URL-safe variants.

```typescript
import { fromByteArray, toByteArray } from 'coreutils/src/Base64';

// Encode
const data = new Uint8Array([72, 101, 108, 108, 111]);
const encoded = fromByteArray(data);
console.log(encoded); // "SGVsbG8="

// Decode
const decoded = toByteArray(encoded);
console.log(decoded); // Uint8Array [72, 101, 108, 108, 111]
```

**Functions:**
- `byteLength(b64: string): number` - Get byte length of a Base64 string
- `toByteArray(b64: string): Uint8Array` - Decode Base64 to bytes
- `fromByteArray(uint8: Uint8Array): string` - Encode bytes to Base64

## LRU Cache

A Least Recently Used (LRU) cache that automatically evicts old items when capacity is reached.

```typescript
import { LRUCache } from 'coreutils/src/LRUCache';

// Create a cache that holds up to 100 items
const cache = new LRUCache<string>(100);

// Insert items
cache.insert('user:1', 'Alice');
cache.insert('user:2', 'Bob');

// Get items (marks as recently used)
const user = cache.get('user:1'); // 'Alice'

// Set max size (evicts old items if needed)
cache.setMaxSize(50);

// Clear all items
cache.clear();

// Listen to evictions
cache.listener = {
    onItemEvicted(key: string, item: string) {
        console.log(`Evicted ${key}: ${item}`);
    }
};
```

**Methods:**
- `get(key: string): T | undefined` - Get an item, marking it as recently used
- `insert(key: string, item: T): void` - Insert or update an item
- `remove(key: string): T | undefined` - Remove and return an item
- `setMaxSize(maxSize: number): void` - Change cache capacity
- `clear(): void` - Remove all items
- `size: number` - Current number of items

## MD5 Hashing

Compute MD5 hashes of strings.

```typescript
import { md5 } from 'coreutils/src/md5';

const hash = md5('Hello, World!');
console.log(hash); // "65a8e27d8879283831b664bd8b7f0ad4"
```

## UUID Utilities

Convert between UUID string and byte representations.

```typescript
import { 
    uuidStringToBytes, 
    uuidBytesToString,
    uuidStringToBytesList,
    uuidBytesToStringList
} from 'coreutils/src/uuidUtils';

// String to bytes
const uuid = '550e8400-e29b-41d4-a716-446655440000';
const bytes = uuidStringToBytes(uuid);

// Bytes to string
const uuidString = uuidBytesToString(bytes);
console.log(uuidString); // "550e8400-e29b-41d4-a716-446655440000"

// Batch conversions
const uuids = ['550e8400-e29b-41d4-a716-446655440000', 'another-uuid-here'];
const bytesList = uuidStringToBytesList(uuids);
const stringList = uuidBytesToStringList(bytesList);
```

## URL Utilities

Parse and manipulate URLs.

```typescript
import { parseURL, URLComponents } from 'coreutils/src/url';

const url = 'https://example.com:8080/path?query=value#fragment';
const components: URLComponents = parseURL(url);

console.log(components.scheme);    // "https"
console.log(components.host);      // "example.com"
console.log(components.port);      // 8080
console.log(components.path);      // "/path"
console.log(components.query);     // "query=value"
console.log(components.fragment);  // "fragment"
```

## Unicode Text Processing

Utilities for working with Unicode text, including encoding and decoding.

```typescript
import { UnicodeString } from 'coreutils/src/unicode/UnicodeString';
import { TextEncoder, TextDecoder } from 'coreutils/src/unicode/TextCoding';

// Unicode string manipulation
const str = new UnicodeString('Hello 👋 World');
console.log(str.length); // Character count including emoji

// Text encoding/decoding
const encoder = new TextEncoder();
const bytes = encoder.encode('Hello');

const decoder = new TextDecoder();
const text = decoder.decode(bytes);
```

## Serial Task Queue

Execute asynchronous tasks serially, one at a time.

```typescript
import { SerialTaskQueue } from 'coreutils/src/SerialTaskQueue';

const queue = new SerialTaskQueue();

// Add tasks
queue.enqueue(async () => {
    console.log('Task 1');
    await someAsyncOperation();
});

queue.enqueue(async () => {
    console.log('Task 2');
    await anotherAsyncOperation();
});

// Tasks execute serially: Task 1 completes before Task 2 starts
```

## String Map and String Set

Efficient map and set implementations optimized for string keys.

```typescript
import { StringMap } from 'coreutils/src/StringMap';
import { StringSet } from 'coreutils/src/StringSet';

// StringMap - object-based map for better performance with string keys
const map: StringMap<number> = {};
map['key1'] = 42;
map['key2'] = 100;

// StringSet - object-based set for string membership testing
const set: StringSet = {};
set['apple'] = true;
set['banana'] = true;

if (set['apple']) {
    console.log('Has apple');
}
```

## Uint8Array Utilities

Helper functions for working with `Uint8Array`.

```typescript
import { 
    areUint8ArraysEqual,
    concatUint8Arrays 
} from 'coreutils/src/Uint8ArrayUtils';

const arr1 = new Uint8Array([1, 2, 3]);
const arr2 = new Uint8Array([1, 2, 3]);
const arr3 = new Uint8Array([4, 5, 6]);

console.log(areUint8ArraysEqual(arr1, arr2)); // true
console.log(areUint8ArraysEqual(arr1, arr3)); // false

const combined = concatUint8Arrays([arr1, arr3]);
console.log(combined); // Uint8Array [1, 2, 3, 4, 5, 6]
```

## Range Type

Simple interface for representing a numeric range.

```typescript
import { Range } from 'coreutils/src/Range';

const range: Range = { min: 0, max: 100 };
```

## Best Practices

1. **Use LRU Cache for bounded caches**: When you need a cache with automatic eviction, prefer `LRUCache` over unbounded `Map`.

2. **Prefer lazyMap for conditional transformations**: When mapping an array where many items might not change, `lazyMap` avoids unnecessary allocations.

3. **Use binary search for sorted data**: If you maintain sorted arrays, `binarySearch` is much faster than linear search for large datasets.

4. **StringMap for performance**: For maps with string keys, `StringMap` (plain objects) can be faster than `Map<string, T>` in some JavaScript engines.

## See Also

- [File System](./stdlib-filesystem.md) - File I/O operations
- [Performance Optimization](./performance-optimization.md) - Tips for efficient code

## Testing

The `coreutils` module includes comprehensive unit tests. To run them:

```bash
bazel test //src/valdi_modules/src/valdi/coreutils/test/...
```

