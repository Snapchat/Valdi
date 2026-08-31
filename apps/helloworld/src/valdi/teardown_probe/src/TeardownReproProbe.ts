// Probe for the bridge-function resolution teardown behavior.
//
// The native repro (SCValdiTeardownRepro on iOS) resolves this factory through the RAISING path —
// +[SCCTeardownProbeMakeTeardownProbe functionWithJSRuntime:] — after the JS runtime has been torn
// down. Only the resolution is exercised; the returned object is never used.
//
// The factory returns an exported proxy (not an export-model and not void) so codegen emits a
// SCValdiBridgeFunction subclass with +functionWithJSRuntime: — the resolution path that raises on
// teardown — rather than a plain callable block. Mirrors makeTestObject in valdi_test's
// FunctionTest.ts. The module is built with async_strict_mode so that class is generated.

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
