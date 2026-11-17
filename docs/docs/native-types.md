# Type Conversions

# Native Type Conversions

Valdi supports extensible type conversions between TypeScript and Android/iOS environments, including built-in support for several key types. Custom type converters are registered at runtime in Android/iOS and can handle any non-primitive TypeScript types.

## Available Types

The following type conversions are available in Valdi:

## The `Valdi::Value` Container

Under the hood, all data passed between TypeScript and native environments is encapsulated in a unified C++ container called `Valdi::Value`.

### Characteristics of `Valdi::Value`
- **128-bit Variant**: It uses 64 bits for data (or a pointer) and 64 bits for type information.
- **Zero-Copy Marshalling**: Designed to minimize heap allocations when passing data across the bridge.
- **Reference Counted**: Complex types like maps and arrays are ref-counted to ensure memory safety across threads.
- **Immutable**: Once created, a `Valdi::Value` is immutable and thread-safe.

This unified representation allows Valdi to handle complex data structures efficiently without repeated conversions as they move between JavaScript, Objective-C, and Java.

| TypeScript  | Android (Kotlin) | iOS (Objective-C)  |
| ------------- | ------------- | ------------- |
| `boolean` | `Boolean` | `BOOL`, or `NSNumber*` if optional |
| `number` |  `Double`  |  `double`, or `NSNumber*` if optional |
| `string` | `String`   | `NSString*` |
| `Uint8Array` | `ByteArray` | `NSData*` |
| `Long` |  `Long`  |  `int64_t`, or `NSNumber*` if optional |
| `bigint` | `com.snap.valdi.BigInt` | `SCValdiBigInt*` |
| `T[]` or `Array<T>` | `Array<T>` | `NSArray<T *>*` |
| arrow function or regular function |  lambda  | block |
| [Annotated type](native-annotations.md) | Generated type | Generated type |
| `Promise<T>`  | [`Promise<T>`][Promise-android] or [`CancelableResolvablePromise<T>`][Cancelable-Promise-android]  | [`SCValdiPromise`][Promise-ios] or [`SCValdiResolvablePromise`][Cancelable-Promise-ios]
| `Observable<T>`  | [`BridgeObservable<T>`][Observable-ts]  | [`SCBridgeObservable`][Observable-ts]
| `Subject<T>` | [`BridgeSubject`][BridgeSubject] | [`SCBridgeSubject`][BridgeSubject]
| [`Asset`][Asset] | `com.snapchat.client.valdi_core.Asset` | `SCNValdiCoreAsset` use in combination with `SCValdiAssetFromImage` to pass `UIImage` to Valdi |

[Promise-android]: ../../valdi/src/java/com/snap/valdi/promise/Promise.kt
[Cancelable-Promise-android]: ../../valdi/src/java/com/snap/valdi/promise/CancelableResolvablePromise.kt
[Promise-ios]: ../../valdi_core/src/valdi_core/ios/valdi_core/SCValdiPromise.m
[Cancelable-Promise-ios]: ../../valdi_core/src/valdi_core/ios/valdi_core/SCValdiResolvablePromise.mm
[Observable-ts]: ../../src/valdi_modules/src/valdi/bridge_observables/src/types/BridgeObservable.ts
[BridgeSubject]: ../../src/valdi_modules/src/valdi/bridge_observables/src/types/BridgeSubject.d.ts
[Asset]: ../../src/valdi_modules/src/valdi/valdi_tsx/src/Asset.d.ts




## Custom Type Converters
Custom type converters can be registered at the platform-level and made available to all Valdi features. Type converters are registered at runtime to provide a mapping between a platform-specific type and its TypeScript counterpart using a factory method that creates a `TypeConverter`.

For example, the following method converts to and from RxJS Observables:

```typescript
import { TypeConverter } from 'valdi_core/src/TypeConverter';
import { Observable } from 'rxjs/src/Observable';
import { BridgeObservable } from '../types/BridgeObservable';
import { convertBridgeObservableToObservable } from './convertBridgeObservableToObservable';
import { convertObservableToBridgeObservable } from './convertObservableToBridgeObservable';

// @NativeTypeConverter
export function makeTypeConverter<T>(): TypeConverter<Observable<T>, BridgeObservable<T>> {
  return {
    toIntermediate: convertObservableToBridgeObservable,
    toTypeScript: convertBridgeObservableToObservable,
  };
}
```

<!-- TODO: publicly available instructions on how to register converters -->

### Big integers

JavaScript `bigint` values cross native boundaries without losing precision. C++ bindings use
`Valdi::Ref<Valdi::BigInt>`, and Swift uses `SCValdiBigInt`. Native wrappers accept signed
64-bit integers and decimal strings. Formatting returns decimal text, and arithmetic uses the
JavaScript engine. Exact int64 conversion returns an optional value in C++ and Kotlin. Objective-C and Swift use
`SCValdiBigIntInt64Result`, whose `success` field indicates whether `value` contains the
exact integer. When conversion fails, `success` is false and `value` is zero.

Public Valdi targets ES2016, so TypeScript rejects literals such as `123n`. To construct
a big integer, include the BigInt type declaration and pass its decimal digits as a string:

```typescript
/// <reference lib="es2020.bigint" />

const value = BigInt('18446744073709551616');
```

On Apple platforms, native `bigint` bindings with JavaScriptCore require iOS 18 or later
or macOS 15 or later. This requirement applies to the bindings, not to JavaScript arithmetic.

`Valdi::BigInt::make(int64_t)` returns a retained immutable integer. The decimal overload accepts
`std::string_view` and an `ExceptionTracker`, reporting malformed input through the tracker.
Its `toInt64()` returns `std::optional<int64_t>` and `toString()` returns `std::string`.
Large integers keep their decimal payload in the same allocation as the C++ object. Kotlin
keeps int64-sized values directly, without a native allocation or JNI call for construction,
conversion, or formatting.

Valdi's native Value-to-JSON helpers encode BigInts as decimal strings; decoding that JSON
produces strings. JavaScript's built-in JSON behavior is unchanged.
