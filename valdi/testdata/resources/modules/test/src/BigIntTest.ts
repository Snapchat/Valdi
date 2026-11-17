/// <reference lib="es2020.bigint" />

/**
 * @ExportModel({ios: 'SCBigIntModel', android: 'com.snap.modules.test.BigIntModel', cpp: 'ValdiTest::BigIntModel'})
 * @Version(__PLACEHOLDER__)
 */
export interface BigIntModel {
  value: bigint;
  optional?: bigint;
  values: bigint[];
  echo: (value: bigint) => bigint;
  promise: Promise<bigint>;
}

/** @Version(__PLACEHOLDER__) */
export function getModel(): BigIntModel {
  const value = BigInt('18446744073709551616');
  return { value, values: [value, -value], echo: input => input, promise: Promise.resolve(value) };
}

export function echo(value: bigint): bigint {
  return value;
}

export function add(left: bigint, right: bigint): bigint {
  return left + right;
}
