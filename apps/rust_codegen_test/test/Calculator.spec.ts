/// <reference lib="es2020.bigint" />

import { isVersionAtLeast } from 'valdi_core/src/CompilerIntrinsics';
import { echoBigInt, echoOptionalBigInt, echoBigInts } from '../src/Calculator';
import 'jasmine/src/jasmine';
import {
  validateRetainedRustInterop,
  validateRustFutures,
  validateSynchronousRustInterop,
} from '../src/RustInteropValidation';

describe('Rust generated native module', () => {
  if (isVersionAtLeast(__PLACEHOLDER__)) {
    it('round trips bigint primitives, optional values, and collections', () => {
      const value = BigInt('18446744073709551616');
      expect(echoBigInt(value)).toBe(value);
      expect(echoBigInt(-value)).toBe(-value);
      expect(echoOptionalBigInt(undefined)).toBeUndefined();
      expect(echoOptionalBigInt(BigInt(0))).toBe(BigInt(0));
      expect(echoBigInts([value, -value, BigInt(0)])).toEqual([value, -value, BigInt(0)]);
    });
  }

  it('marshals primitives, models, generics, enums, collections, bytes, Long, any, and maps', () => {
    validateSynchronousRustInterop();
  });

  it('preserves retained model, callback, and proxy lifetimes in both directions', () => {
    validateRetainedRustInterop();
  });

  it('resolves and rejects Rust futures', (): Promise<void> => {
    return validateRustFutures();
  });
});
