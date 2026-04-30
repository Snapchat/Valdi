import { CalculatorToStringFormat, ICalculator } from './NativeCalculator';

class JsCalculator implements ICalculator {
  private _value: number = 0;

  add(value: number): void {
    this._value += value;
  }
  sub(value: number): void {
    this._value -= value;
  }
  mul(value: number): void {
    this._value *= value;
  }

  div(value: number): void {
    this._value /= value;
  }

  total(): number {
    return this._value;
  }

  toString(format: CalculatorToStringFormat): string {
    switch (format) {
      case CalculatorToStringFormat.DECIMAL:
        return this._value % 1 === 0 ? this._value.toFixed(1) : this._value.toString();
      case CalculatorToStringFormat.INTEGER:
        return Math.floor(this._value).toString();
    }
  }
}

/**
 * @ExportFunction
 */
export function makeCalculator(): ICalculator {
  return new JsCalculator();
}
