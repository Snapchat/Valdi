/**
 * Typed access to the globals Jest injects, scoped to this module.
 *
 * Declaring these ambiently (in a .d.ts) leaks them into every other module's type-check,
 * where a narrower `expect` shadows the jasmine declarations the rest of the repo relies
 * on. Depending on @types/jest instead puts an npm target in coreutils_web's dependency
 * graph, which some consumers cannot resolve. Keeping the types inside a module avoids
 * both: nothing here is visible outside the files that import it.
 */

interface Matchers {
  toBe(expected: unknown): void;
  toEqual(expected: unknown): void;
  toThrow(expected?: string | RegExp): void;
}

interface JestGlobals {
  describe(name: string, fn: () => void): void;
  it(name: string, fn: () => void): void;
  expect(actual: unknown): Matchers;
}

const jestGlobals = globalThis as unknown as JestGlobals;

export const describe = jestGlobals.describe;
export const it = jestGlobals.it;
export const expect = jestGlobals.expect;
