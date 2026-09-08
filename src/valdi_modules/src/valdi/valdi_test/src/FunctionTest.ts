/**
 * @ExportProxy
 */
export interface ITestObject {
  add(value: number): number;
  // Throws synchronously when invoked. Used to exercise the bridge trampoline's behaviour when a
  // bridged invocation raises a synchronous JS error: the crossing must report the error and return
  // a default value rather than raise an uncatchable NSException.
  throwSynchronously(): number;
}

class JsTestObject implements ITestObject {
  private _value: number = 0;

  add(value: number): number {
    this._value += value;
    return this._value;
  }

  throwSynchronously(): number {
    throw new Error('throwSynchronously: synchronous JS failure');
  }
}

/**
 * @ExportFunction
 */
export function makeTestObject(): ITestObject {
  return new JsTestObject();
}

// Returns a non-null string. Used to exercise the invocation-teardown boundary: after the runtime is
// torn down the raising resolver degrades to a no-op function whose invocation returns a null value
// in this non-null-typed return slot.
/**
 * @ExportFunction
 */
export function getTestString(): string {
  return 'ok';
}
