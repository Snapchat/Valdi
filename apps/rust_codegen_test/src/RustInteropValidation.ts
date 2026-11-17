/// <reference lib="es2020.bigint" />

import { isVersionAtLeast } from 'valdi_core/src/CompilerIntrinsics';
import {
  Operation,
  Unit,
  add,
  applyCallback,
  callOptionalCallback,
  callProxy,
  callStored,
  callStoredProxy,
  copyPoint,
  echoBytes,
  echoBytesBox,
  echoFlags,
  echoKeyword,
  echoLabels,
  echoLong,
  echoBigInt,
  echoOptionalBigInt,
  echoBigInts,
  echoNestedBox,
  echoNumberArrayBox,
  echoNumberMap,
  echoNumbers,
  echoOperation,
  echoOptionalAny,
  echoOptionalBytes,
  echoOptionalFuture,
  echoOptionalNumbers,
  echoOptionalOperation,
  echoOptionalPoint,
  echoOptionalProxy,
  echoPointBox,
  echoPoints,
  echoStringBox,
  echoUnit,
  greet,
  makeAnyString,
  makeBackgroundFuture,
  makeCallback,
  makeProxy,
  makeRejectedFuture,
  makeResolvedFuture,
  makeResolvedStringFuture,
  makeResolvedVoidFuture,
  readAnyNumber,
  storeCallback,
  storeBytes,
  storePoint,
  storeProxy,
  storedPointX,
  storedBytes,
} from './Calculator';

function assertEqual<T>(actual: T, expected: T, description: string): void {
  if (actual !== expected) {
    throw new Error(`${description}: expected ${String(expected)}, got ${String(actual)}`);
  }
}

function assertDeepEqual(actual: unknown, expected: unknown, description: string): void {
  const actualJson = JSON.stringify(actual);
  const expectedJson = JSON.stringify(expected);
  if (actualJson !== expectedJson) {
    throw new Error(`${description}: expected ${expectedJson}, got ${actualJson}`);
  }
}

export function validateSynchronousRustInterop(): void {
  if (isVersionAtLeast(__PLACEHOLDER__)) {
    const integer = BigInt('18446744073709551616');
    assertEqual(echoBigInt(integer), integer, 'bigint');
    assertEqual(echoBigInt(-integer), -integer, 'negative bigint');
    assertEqual(echoOptionalBigInt(undefined), undefined, 'absent bigint');
    assertEqual(echoOptionalBigInt(BigInt(0)), BigInt(0), 'zero bigint');
    const integers = echoBigInts([integer, -integer, BigInt(0)]);
    assertEqual(integers[0], integer, 'bigint array');
    assertEqual(integers[1], -integer, 'negative bigint array');
    assertEqual(integers[2], BigInt(0), 'zero bigint array');
  }

  assertEqual(add(20, 22), 42, 'primitive call');
  assertEqual(greet('TypeScript'), 'Hello, TypeScript', 'string call');

  const point = copyPoint({ x: 42, label: 'answer', enabled: true });
  assertDeepEqual(point, { x: 42, label: 'answer', enabled: true }, 'model');
  assertDeepEqual(echoOptionalPoint(point), point, 'present optional model');
  assertEqual(echoOptionalPoint(undefined), undefined, 'absent optional model');
  assertDeepEqual(echoStringBox({ value: 'boxed' }), { value: 'boxed' }, 'generic string model');
  assertDeepEqual(echoPointBox({ value: point }), { value: point }, 'generic model');
  assertDeepEqual(
    echoNestedBox({ value: { value: 'nested' } }),
    { value: { value: 'nested' } },
    'nested generic model',
  );
  assertDeepEqual(echoNumberArrayBox({ value: [1, 2, 39] }), { value: [1, 2, 39] }, 'generic array model');
  assertDeepEqual(Array.from(echoBytesBox({ value: new Uint8Array([4, 2]) }).value), [4, 2], 'generic bytes model');
  assertDeepEqual(
    echoKeyword({ type: 'keyword', match: 42, async: true }),
    { type: 'keyword', match: 42, async: true },
    'Rust keyword fields',
  );

  assertEqual(echoOperation(9 as Operation), 9 as Operation, 'numeric enum');
  assertEqual(echoOptionalOperation(9 as Operation), 9 as Operation, 'present optional enum');
  assertEqual(echoOptionalOperation(undefined), undefined, 'absent optional enum');
  assertEqual(echoUnit('pixels' as Unit), 'pixels' as Unit, 'string enum');
  assertEqual(echoLong(Long.fromString('9223372036854775000')).toString(), '9223372036854775000', 'Long');
  assertDeepEqual(echoNumbers([1, 2, 39]), [1, 2, 39], 'primitive array');
  assertDeepEqual(echoOptionalNumbers([1, 2, 39]), [1, 2, 39], 'present optional array');
  assertEqual(echoOptionalNumbers(undefined), undefined, 'absent optional array');
  assertDeepEqual(echoFlags([true, false, true]), [true, false, true], 'boolean array');
  assertDeepEqual(echoLabels(['forty', 'two']), ['forty', 'two'], 'string array');
  assertDeepEqual(echoPoints([point]), [point], 'model array');

  assertDeepEqual(Array.from(echoBytes(new Uint8Array([0, 1, 2, 255]))), [0, 1, 2, 255], 'bytes');
  assertDeepEqual(Array.from(echoOptionalBytes(new Uint8Array([4, 2]))!), [4, 2], 'present optional bytes');
  assertEqual(echoOptionalBytes(undefined), undefined, 'absent optional bytes');
  assertEqual(readAnyNumber(42 as any), 42, 'any number');
  assertEqual(readAnyNumber(echoOptionalAny(42 as any) as any), 42, 'present optional any');
  assertEqual(echoOptionalAny(undefined), undefined, 'absent optional any');
  assertEqual(makeAnyString() as any, 'forty-two', 'returned any');

  const numberMap = { answer: 42 } as any as Map<string, number>;
  assertEqual((echoNumberMap(numberMap) as any).answer, 42, 'map');
}

export function validateRetainedRustInterop(): void {
  storeBytes(new Uint8Array([4, 2]));
  assertDeepEqual(Array.from(storedBytes()), [4, 2], 'retained bytes');

  storePoint({ x: 42, label: 'retained', enabled: true });
  assertEqual(storedPointX(), 42, 'retained model');

  storeCallback(value => value + 40);
  assertEqual(callStored(2), 42, 'retained callback');
  assertEqual(
    applyCallback(2, value => value + 40),
    42,
    'borrowed callback',
  );
  assertEqual(makeCallback(40)(2), 42, 'Rust callback');

  const tsProxy = {
    add(value: number): number {
      return value + 40;
    },
    label(): string {
      return 'TypeScript proxy';
    },
  };
  assertEqual(callProxy(tsProxy, 2), 42, 'borrowed TypeScript proxy');
  assertEqual(echoOptionalProxy(tsProxy)?.add(2), 42, 'present optional proxy');
  assertEqual(echoOptionalProxy(undefined), undefined, 'absent optional proxy');
  assertEqual(
    callOptionalCallback(2, value => value + 40),
    42,
    'present optional callback',
  );
  assertEqual(callOptionalCallback(2, undefined), undefined, 'absent optional callback');
  storeProxy(tsProxy);
  assertEqual(callStoredProxy(2), 42, 'retained TypeScript proxy');
  assertEqual(makeProxy(40).add(2), 42, 'Rust proxy');
}

export function validateRustFutures(): Promise<void> {
  assertEqual(echoOptionalFuture(undefined), undefined, 'absent optional future');
  const echoedFuture = echoOptionalFuture(Promise.resolve(42));
  if (echoedFuture === undefined) {
    throw new Error('present optional future returned undefined');
  }

  const validations: Promise<void>[] = [
    makeResolvedFuture(42).then(value => assertEqual(value, 42, 'resolved future')),
    makeBackgroundFuture(42).then(value => assertEqual(value, 42, 'background future')),
    makeResolvedStringFuture('forty-two').then(value => assertEqual(value, 'forty-two', 'string future')),
    makeResolvedVoidFuture().then(value => assertEqual(value, undefined, 'void future')),
    echoedFuture.then(value => assertEqual(value, 42, 'echoed TypeScript future')),
    makeRejectedFuture('future rejected').then(
      () => {
        throw new Error('rejected future resolved');
      },
      error => {
        if (!(error instanceof Error)) {
          throw new Error(`future rejected with a non-Error value: ${String(error)}`);
        }
        if (!error.message.includes('future rejected')) {
          throw new Error(`unexpected future rejection: ${error.message}`);
        }
      },
    ),
  ];
  return Promise.all(validations).then(() => undefined);
}

export function validateRustInterop(): Promise<void> {
  validateSynchronousRustInterop();
  validateRetainedRustInterop();
  return validateRustFutures();
}
