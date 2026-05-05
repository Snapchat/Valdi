/**
 * Web implementation of the Calculator bridge module.
 *
 * Registered automatically by RegisterNativeModules.js via moduleLoader.
 */

function multiply(left: number, right: number): number {
  return left * right;
}

export = { multiply };
