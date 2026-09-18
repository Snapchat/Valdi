import { Greeting, IGreeter } from './NativeApi';

class JsGreeter implements IGreeter {
  private _count: number = 0;

  greet(name: string, style: Greeting): string {
    this._count += 1;
    switch (style) {
      case Greeting.CASUAL:
        return 'Hi ' + name;
      case Greeting.FORMAL:
        return 'Hello, ' + name;
    }
  }

  count(): number {
    return this._count;
  }
}

/**
 * @ExportFunction
 */
export function makeGreeter(): IGreeter {
  return new JsGreeter();
}
