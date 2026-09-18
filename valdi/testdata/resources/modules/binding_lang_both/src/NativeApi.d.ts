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

/**
 * @ExportProxy
 */
export interface IGreeter {
  greet(name: string, style: Greeting): string;
  count(): number;
}

export function makeGreeter(): IGreeter;
