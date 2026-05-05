/**
 * @ExportModule
 */

/**
 * @ExportEnum
 */
export const enum CalculatorToStringFormat {
  DECIMAL = 1,
  INTEGER = 2,
}

/**
 * @ExportProxy
 */
export interface ICalculator {
  add(value: number): void;
  sub(value: number): void;
  mul(value: number): void;
  div(value: number): void;
  total(): number;
  toString(format: CalculatorToStringFormat): string;
}

export function makeCalculator(): ICalculator;
