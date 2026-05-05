import { makeCalculator } from './NativeCalculator';

export function compute(): number {
  const calculator = makeCalculator();
  calculator.add(42);
  calculator.add(8);
  return calculator.total();
}

