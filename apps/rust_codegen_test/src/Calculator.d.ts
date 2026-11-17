/**
 * @ExportModule
 */

/**
 * @ExportModel
 */
export interface Point {
  x: number;
  label: string;
  enabled?: boolean;
}

/**
 * @ExportModel
 */
export interface Boxed<T> {
  value: T;
}

/**
 * @ExportModel
 */
export interface KeywordModel {
  type: string;
  match: number;
  async?: boolean;
}

/**
 * @ExportEnum
 */
export enum Operation {
  ADD = 4,
  SUBTRACT = 9,
}

/**
 * @ExportEnum
 */
export enum Unit {
  POINTS = 'points',
  PIXELS = 'pixels',
}

/**
 * @ExportProxy
 */
export interface Accumulator {
  add(value: number): number;
  label(): string;
}

export const add: (left: number, right: number) => number;
export const negate: (value: boolean) => boolean;
export const greet: (name: string) => string;
export const copyPoint: (point: Point) => Point;
export const echoOptionalPoint: (point?: Point) => Point | undefined;
export const echoStringBox: (value: Boxed<string>) => Boxed<string>;
export const echoPointBox: (value: Boxed<Point>) => Boxed<Point>;
export const echoNestedBox: (value: Boxed<Boxed<string>>) => Boxed<Boxed<string>>;
export const echoNumberArrayBox: (value: Boxed<number[]>) => Boxed<number[]>;
export const echoBytesBox: (value: Boxed<Uint8Array>) => Boxed<Uint8Array>;
export const echoKeyword: (value: KeywordModel) => KeywordModel;
export const storePoint: (point: Point) => void;
export const storedPointX: () => number;
export const callProxy: (proxy: Accumulator, value: number) => number;
export const echoOptionalProxy: (proxy?: Accumulator) => Accumulator | undefined;
export const storeProxy: (proxy: Accumulator) => void;
export const callStoredProxy: (value: number) => number;
export const makeProxy: (seed: number) => Accumulator;
export const echoOperation: (operation: Operation) => Operation;
export const echoOptionalOperation: (operation?: Operation) => Operation | undefined;
export const echoUnit: (unit: Unit) => Unit;
/** @Version(__PLACEHOLDER__) */
export const echoBigInt: (value: bigint) => bigint;
/** @Version(__PLACEHOLDER__) */
export const echoOptionalBigInt: (value?: bigint) => bigint | undefined;
/** @Version(__PLACEHOLDER__) */
export const echoBigInts: (values: bigint[]) => bigint[];

export const echoLong: (value: Long) => Long;
export const echoBytes: (value: Uint8Array) => Uint8Array;
export const echoOptionalBytes: (value?: Uint8Array) => Uint8Array | undefined;
export const storeBytes: (value: Uint8Array) => void;
export const storedBytes: () => Uint8Array;
export const echoNumbers: (values: number[]) => number[];
export const echoOptionalNumbers: (values?: number[]) => number[] | undefined;
export const echoFlags: (values: boolean[]) => boolean[];
export const echoLabels: (values: string[]) => string[];
export const echoPoints: (values: Point[]) => Point[];
export const echoAny: (value: object) => object;
export const echoOptionalAny: (value?: object) => object | undefined;
export const makeAnyString: () => object;
export const readAnyNumber: (value: object) => number;
export const echoMap: (value: any) => any;
export const echoNumberMap: (value: Map<string, number>) => Map<string, number>;
export const makeMap: (value: object) => any;
export const readMap: (value: any, key: string) => object;
export const makeResolvedFuture: (value: number) => Promise<number>;
export const makeBackgroundFuture: (value: number) => Promise<number>;
export const makeResolvedStringFuture: (value: string) => Promise<string>;
export const makeResolvedVoidFuture: () => Promise<void>;
export const makeRejectedFuture: (message: string) => Promise<number>;
export const observeFuture: (value: Promise<number>) => void;
export const echoOptionalFuture: (value?: Promise<number>) => Promise<number> | undefined;
export const lastFutureValue: () => number;
export const applyCallback: (value: number, callback: (value: number) => number) => number;
export const callOptionalCallback: (value: number, callback?: (value: number) => number) => number | undefined;
export const makeCallback: (offset: number) => (value: number) => number;
export const storeCallback: (callback: (value: number) => number) => void;
export const callStored: (value: number) => number;
export const fail: () => void;
export const panicCall: () => void;
