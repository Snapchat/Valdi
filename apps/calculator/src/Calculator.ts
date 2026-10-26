export interface CalculatorState {
  display: string;
  expression: string;
}

type Operator = '+' | '-' | '*' | '/';

/** A small, immediate-execution calculator. No dynamic expression evaluation. */
export class Calculator {
  private display = '0';
  private expression = '';
  private accumulator: number | undefined;
  private operator: Operator | undefined;
  private fresh = true;

  snapshot(): CalculatorState {
    return { display: this.display, expression: this.expression };
  }

  press(key: string): CalculatorState {
    if (key === 'clear' || key === 'Escape') {
      this.display = '0';
      this.expression = '';
      this.accumulator = undefined;
      this.operator = undefined;
      this.fresh = true;
    } else if (/^[0-9]$/.test(key)) {
      if (this.display === 'Error') this.press('clear');
      if (this.fresh || this.display === '0') this.display = key;
      else if (this.display.length < 16) this.display += key;
      this.fresh = false;
    } else if (key === '.') {
      if (this.display === 'Error') this.press('clear');
      if (this.fresh) this.display = '0.';
      else if (!this.display.includes('.')) this.display += '.';
      this.fresh = false;
    } else if (key === 'Backspace') {
      if (this.display === 'Error') return this.press('clear');
      if (!this.fresh) {
        this.display = this.display.slice(0, -1);
        if (!this.display || this.display === '-') this.display = '0';
      }
    } else if (this.display !== 'Error') {
      if (key === 'negate') {
        if (this.display !== '0') this.display = this.display.startsWith('-') ? this.display.slice(1) : '-' + this.display;
      } else if (key === 'percent') {
        this.display = this.format(Number(this.display) / 100);
      } else if (key === '+' || key === '-' || key === '*' || key === '/') {
        if (this.operator && !this.fresh && !this.evaluate()) return this.snapshot();
        this.accumulator = Number(this.display);
        this.operator = key;
        this.expression = `${this.display} ${key}`;
        this.fresh = true;
      } else if ((key === '=' || key === 'Enter') && this.operator && !this.fresh) {
        const expression = `${this.expression} ${this.display} =`;
        if (this.evaluate()) this.expression = expression;
      }
    }
    return this.snapshot();
  }

  private evaluate(): boolean {
    const left = this.accumulator;
    const right = Number(this.display);
    if (left === undefined || !this.operator) return true;
    const value = this.operator === '+' ? left + right : this.operator === '-' ? left - right :
      this.operator === '*' ? left * right : left / right;
    this.display = this.format(value);
    this.operator = undefined;
    this.accumulator = undefined;
    this.fresh = true;
    if (this.display === 'Error') this.expression = '';
    return this.display !== 'Error';
  }

  private format(value: number): string {
    if (!Number.isFinite(value)) return 'Error';
    return String(Number(value.toPrecision(12)));
  }
}
