import { Formal, Greeting, GreetingId, IGreeter, UserName } from './NativeApi';

class JsGreeter implements IGreeter {
  private _count: number = 0;
  private _lastGreeting: GreetingId | undefined;
  private _lastWasFormal = false as Formal;

  greet(name: string, style: Greeting): string {
    this._count += 1;
    this._lastWasFormal = (style === Greeting.FORMAL) as Formal;
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

  greetUser(user: UserName): GreetingId {
    this.greet(user, Greeting.CASUAL);
    this._lastGreeting = this._count as GreetingId;
    return this._lastGreeting;
  }

  lastGreeting(): GreetingId | undefined {
    return this._lastGreeting;
  }

  lastWasFormal(): Formal {
    return this._lastWasFormal;
  }
}

/**
 * @ExportFunction
 */
export function makeGreeter(): IGreeter {
  return new JsGreeter();
}
