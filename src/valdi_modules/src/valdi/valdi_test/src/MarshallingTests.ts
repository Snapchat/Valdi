import { Optional } from 'valdi_core/src/utils/Optional';

/**
 * @ExportModel
 */
export interface TestObject {
  i: number;
  s: string;
  b: boolean;
}

/**
 * @ExportEnum
 */
export enum TestIntEnum {
  FIRST = 1,
  SECOND = 2,
}

/**
 * @ExportEnum
 */
export enum TestStringEnum {
  FIRST = 'one',
  SECOND = 'two',
}

/**
 * @ExportProxy
 */
export interface TestProxyObject {
  getString(): string;
}

/**
 * @ExportProxy
 */
export interface ProxyCallbackObject {
  receiveString(string: string): void;
  receiveError(message: string): void;
}

/**
 * @ExportProxy
 */
export interface MarshallingTestService {
  getObject(i: number, s: string, b: boolean): TestObject;
  receiveObject(object: TestObject): string;
  receiveProxy(proxy: TestProxyObject): string;

  getList(p0: string, p1: string, p2: string): string[];
  receiveList(l: string[]): string;

  getMap(k1: string, v1: number, k2: string, v2: number): Map<string, number>;
  receiveMap(m: Map<string, number>): string;

  getFuture(s: string): Promise<string>;
  receiveFuture(future: Promise<string>, callback: ProxyCallbackObject): void;

  receiveBinary(binary: Uint8Array): string;
  getBinary(base64: string): Uint8Array;

  receiveLong(l: Long): string;
  getLong(str: string): Long;

  receiveFunction(cb: () => string): string;
  getFunction(value: string): () => string;

  receiveIntEnum(enumValue: TestIntEnum): string;
  getIntEnum(enumIndex: number): TestIntEnum;

  receiveStringEnum(enumValue: TestStringEnum): string;
  getStringEnum(enumIndex: number): TestStringEnum;

  receiveGeneric(optional: Optional<TestObject>): string;
  getGeneric(i: number, s: string, b: boolean): Optional<TestObject>;
}

function assertEquals(a: any, b: any): void {
  if (a !== b) {
    throw new Error(`Expected ${a} to be equal to ${b}`);
  }
}

async function basicTests(testService: MarshallingTestService): Promise<void> {
  console.info('Testing basic object operations...');
  const object = testService.getObject(42, 'Hello World', true);
  assertEquals(object.i, 42);
  assertEquals(object.s, 'Hello World');
  assertEquals(object.b, true);

  const toStringRepr = testService.receiveObject({
    i: 43,
    s: 'Hello World 2',
    b: false,
  });

  assertEquals(toStringRepr, '43-Hello World 2-false');
}

async function collectionTests(testService: MarshallingTestService): Promise<void> {
  console.info('Testing collection operations...');
  const list = testService.getList('Hello', 'World', '!');
  assertEquals(list.length, 3);
  assertEquals(list[0], 'Hello');
  assertEquals(list[1], 'World');
  assertEquals(list[2], '!');

  const toStringListRepr = testService.receiveList(['This', 'is', 'great!']);
  assertEquals(toStringListRepr, 'This,is,great!');

  const map = testService.getMap('Hello', 1, 'World', 2);
  assertEquals(map.size, 2);
  assertEquals(map.get('Hello'), 1);
  assertEquals(map.get('World'), 2);

  const toStringMapRepr = testService.receiveMap(
    new Map([
      ['This', 1],
      ['is', 2],
      ['great!', 3],
    ]),
  );
  assertEquals(toStringMapRepr, 'This:1,great!:3,is:2');
}

function asciiToUint8Array(str: string): Uint8Array {
  const arr = new Uint8Array(str.length);
  for (let i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i) & 0x7f;
  }
  return arr;
}

function binaryToAscii(binary: Uint8Array): string {
  let str = '';
  for (let i = 0; i < binary.length; i++) {
    str += String.fromCharCode(binary[i]);
  }
  return str;
}

async function longTests(testService: MarshallingTestService): Promise<void> {
  console.info('Testing long operations...');
  const long = testService.getLong('9223372036854775806');
  assertEquals(true, long instanceof Long);
  assertEquals(long.toString(), '9223372036854775806');

  const toStringLongRepr = testService.receiveLong(Long.fromString('-9223372036854775800'));
  assertEquals(toStringLongRepr, '-9223372036854775800');

  const smallLong = testService.getLong('42');
  const smallLong2 = testService.getLong('42');

  assertEquals(smallLong.toNumber(), 42);

  // Should be cached
  assertEquals(smallLong, smallLong2);
}

async function binaryTests(testService: MarshallingTestService): Promise<void> {
  console.info('Testing binary operations...');

  const base64 = testService.receiveBinary(asciiToUint8Array('Hello world!'));
  assertEquals(base64, 'SGVsbG8gd29ybGQh');

  const binary = testService.getBinary('R29vZGJ5ZT8=');
  assertEquals(binaryToAscii(binary), 'Goodbye?');
}

async function functionTests(testService: MarshallingTestService): Promise<void> {
  console.info('Testing function operations...');
  const functionResult = testService.receiveFunction(() => 'Hello World');
  assertEquals(functionResult, 'Hello World');

  const fn = testService.getFunction('Hello World');
  assertEquals(fn(), 'Hello World');
}

async function genericTests(testService: MarshallingTestService): Promise<void> {
  console.info('Testing generic operations...');

  const genericResult = testService.receiveGeneric({ data: { i: 4200, s: 'Hello World', b: false } });
  assertEquals(genericResult, '4200-Hello World-false');

  const generic = testService.getGeneric(4200, 'Hello World', false);
  assertEquals(generic.data?.i, 4200);
  assertEquals(generic.data?.s, 'Hello World');
  assertEquals(generic.data?.b, false);
}

async function proxyTests(service: MarshallingTestService): Promise<void> {
  console.info('Testing proxy operations...');
  class ProxyObject implements TestProxyObject {
    getString(): string {
      return 'Hello World';
    }
  }
  const stringResult = service.receiveProxy(new ProxyObject());
  assertEquals(stringResult, 'Hello World');
}

async function enumTests(service: MarshallingTestService): Promise<void> {
  console.info('Testing enum operations...');
  const intEnumResult = service.receiveIntEnum(TestIntEnum.SECOND);
  assertEquals(intEnumResult, '2');
  const intEnum = service.getIntEnum(1);
  assertEquals(intEnum, TestIntEnum.SECOND);

  const stringEnumResult = service.receiveStringEnum(TestStringEnum.SECOND);
  assertEquals(stringEnumResult, 'two');
  const stringEnum = service.getStringEnum(1);
  assertEquals(stringEnum, TestStringEnum.SECOND);
}

/**
 * @ExportFunction
 */
export async function runMarshallingTests(service: MarshallingTestService): Promise<void> {
  try {
    console.info('Running marshalling sanity checks...');
    await basicTests(service);
    await longTests(service);
    await collectionTests(service);
    await binaryTests(service);
    await proxyTests(service);
    await functionTests(service);
    await genericTests(service);
    await enumTests(service);

    console.info('Marshalling sanity checks passed');
  } catch (error) {
    console.error('Marshalling sanity checks failed', error);
    throw new Error('Marshalling sanity checks failed');
  }
}
