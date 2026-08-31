// Probe for the bridge-function teardown behavior (resolution and invocation).
//
// The native repro (SCValdiTeardownRepro on iOS) exercises this module two ways after the JS runtime
// has been torn down:
//   - RESOLUTION: +[SCCTeardownProbeMakeTeardownProbe functionWithJSRuntime:] (the raising path).
//   - INVOCATION: invoking getTeardownPath after teardown, where the degrade returns a null value in
//     a nonnull-typed slot.
//
// makeTeardownProbe returns an exported proxy (not an export-model and not void) so codegen emits a
// SCValdiBridgeFunction subclass with +functionWithJSRuntime: (the resolution path). getTeardownPath
// returns a string so its degraded invocation yields a null in a `_Nonnull` NSString. The module is
// built with async_strict_mode so those classes are generated.

/**
 * @ExportProxy
 */
export interface TeardownProbeApi {
  ping(): void;
}

class JsTeardownProbe implements TeardownProbeApi {
  ping(): void {}
}

/**
 * @ExportFunction
 */
export function makeTeardownProbe(): TeardownProbeApi {
  return new JsTeardownProbe();
}

/**
 * @ExportFunction
 */
// String return: invoking this after teardown yields a null string in a `_Nonnull` slot. Passed to a
// nonnull-requiring API (e.g. +[NSURL fileURLWithPath:]), the null crashes — the invocation-teardown
// nil-in-nonnull crash.
export function getTeardownPath(): string {
  return 'teardown_probe/ok';
}
