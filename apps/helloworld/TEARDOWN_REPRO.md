# Bridge-function resolution teardown repro

A minimal, self-contained demonstration of what happens when a native → JS **bridge function is
resolved while the JS runtime is being torn down** (the situation a Valdi session hits on logout /
account switch), wired into the Hello World app (iOS).

## The behavior

`+[SCValdiBridgeFunction functionWithJSRuntime:]` resolves a generated bridge function by pushing
its module onto a marshaller. If the JS runtime is disposed between a call being queued and
executed, the JS-thread task that would populate the marshaller is skipped.

- **Without the resolution-teardown degrade**, the marshaller is left empty and
  `SCValdiMarshallerCheck` raises an ObjC `SCValdiError` NSException. Because the resolver is reached
  from a Swift (or unguarded ObjC) frame, the exception is uncatchable and the process aborts
  (SIGABRT).
- **With the degrade** (`VALDI_ENABLE_RESOLUTION_TEARDOWN_DEGRADE`, on by default), the skipped task
  is stamped with a distinguishable teardown error code; `functionWithJSRuntime:` recognizes it and
  returns a non-nil **no-op** bridge function so the dying session unwinds quietly instead of
  aborting.

## What this repro does (`src/ios/SCValdiTeardownRepro.mm`)

1. Stands up an **isolated** `SCValdiRuntimeManager` (so teardown doesn't kill the app's own UI).
2. Captures the main runtime's `jsRuntime` strongly, so the disposed runtime stays addressable.
3. Forces `VALDI_ENABLE_RESOLUTION_TEARDOWN_DEGRADE` to a known state on that runtime by injecting a
   fixed `ITweakValueProvider` (the value is cached on the JS runtime before teardown detaches the
   listener, so it survives disposal).
4. On a background queue (resolution on the main thread is forbidden by `async_strict_mode`):
   - resolves the probe on the **live** runtime — succeeds (sanity, logged);
   - drops the manager (`manager = nil`) → dealloc → `fullTeardown` → runtime disposed;
   - resolves the probe **again**, and either degrades (no crash) or aborts, depending on the mode.

The probe is a trivial exported function, `makeTeardownProbe` in
`src/valdi/teardown_probe/src/TeardownReproProbe.ts`, which codegen turns into the bridge-function
class `SCCTeardownProbeMakeTeardownProbe` (only its resolution is exercised; the body never runs).

## How to trigger

Run the Hello World app on iOS. Two debug-only buttons at the bottom of the screen:

- **"✅ Resolve after teardown (degrade ON — no crash)"** (accessibilityId `teardown-degrade-button`)
  — the shipped default. Resolution after teardown returns a no-op function; the final `NSLog`
  (`degrade ON: resolved after teardown without crashing`) prints and the app keeps running.
- **"⚠️ Resolve after teardown (degrade OFF — SIGABRT)"** (accessibilityId `teardown-crash-button`)
  — disables the kill switch first. The app aborts within a second; the crash matches the original
  teardown crash (`SCValdiErrorThrowWithStacktrace` → `SCValdiMarshallerCppCheck` →
  `-[SCValdiJSRuntimeImpl pushModuleAtPath:inMarshaller:]` → `+[…Probe functionWithJSRuntime:]`).

Android and web wire both buttons to no-ops — this behavior is iOS-specific.

## Note

This demonstrates the resolution-teardown degrade as a pair: the crash it removes (degrade off) and
the graceful unwind it provides (degrade on). It exercises the **raising**
`functionWithJSRuntime:` entry point; the non-raising `resolve(jsRuntime:)` resolver instead reports
teardown as `nil` + `NSError` so its adopters can run their own degrade path.
