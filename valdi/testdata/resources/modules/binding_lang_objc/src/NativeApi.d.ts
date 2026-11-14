/**
 * @ExportModule
 */

/**
 * @ExportEnum
 */
export const enum Greeting {
  CASUAL = 1,
  FORMAL = 2,
}

declare const idBrand: unique symbol;
export type GreetingId = number & { readonly [idBrand]: 'GreetingId' };
export type UserName = string & { __brand: 'UserName' };
export type Formal = boolean & { __brand: 'Formal' };

/**
 * @ExportProxy
 */
export interface IGreeter {
  greet(name: string, style: Greeting): string;
  count(): number;
  greetUser(user: UserName): GreetingId;
  lastGreeting(): GreetingId | undefined;
  lastWasFormal(): Formal;
}

export function makeGreeter(): IGreeter;
