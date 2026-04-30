/**
 * @ExportProxy
 */
export interface ITestObject {
  add(value: number): number;
}

class JsTestObject implements ITestObject {
  private _value: number = 0;

  add(value: number): number {
    this._value += value;
    return this._value;
  }
}

/**
 * @ExportFunction
 */
export function makeTestObject(): ITestObject {
  return new JsTestObject();
}
