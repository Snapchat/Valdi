/**
 * Used only by AsyncStrictModeSyncCallAssertsOnMainThread test.
 * This module has async_strict_mode=True; compute() is intentionally not @AllowSyncCall
 * so that a sync call from the main thread triggers the assertion.
 */
export function compute(): number {
  return 42;
}
