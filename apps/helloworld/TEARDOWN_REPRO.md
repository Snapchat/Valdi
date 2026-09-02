# Bridge-function teardown repro

A minimal, self-contained demonstration of what happens when a native → JS **bridge crossing races
JS-runtime teardown** (the situation a Valdi session hits on logout / account switch), wired into the
Hello World app (iOS). It covers both crossings that crash — **resolution** and **invocation** — and
the resolution-teardown degrade that sits between them.

## Which crash each button reproduces

Three debug-only buttons at the bottom of the Hello World screen. Each stands up an **isolated**
`SCValdiRuntimeManager`, disposes it, and then crosses the bridge on the disposed runtime off the
main thread (`async_strict_mode` forbids resolution on the main thread):

| Button (accessibilityId) | Crossing | Degrade | Outcome |
|---|---|---|---|
| ✅ `teardown-degrade-button` | resolution | on (default) | **No crash** — resolution returns a no-op function and unwinds quietly. |
| ⚠️ `teardown-crash-button` | resolution | off (kill switch) | **SIGABRT** — the raising resolver throws an uncatchable `SCValdiError` below the calling frame. |
| 💥 `teardown-invocation-button` | invocation | on (default) | **SIGABRT** — resolution degrades to a no-op, but **invoking** it returns a null value in a `_Nonnull` slot; passing that null to a non-null-requiring API (`+[NSURL fileURLWithPath:]`) raises `NSInvalidArgumentException`. |

Android and web wire all three to no-ops — this behavior is iOS-specific.

## The behavior

### Resolution (buttons 1 & 2)

`+[SCValdiBridgeFunction functionWithJSRuntime:]` resolves a generated bridge function by pushing its
module onto a marshaller. If the JS runtime is disposed between a call being queued and executed, the
JS-thread task that would populate the marshaller is skipped.

- **Without the degrade** (`VALDI_ENABLE_RESOLUTION_TEARDOWN_DEGRADE` off), the marshaller is left
  empty and `SCValdiMarshallerCheck` raises an ObjC `SCValdiError` NSException. Reached from a Swift
  (or unguarded ObjC) frame, the exception is uncatchable and the process aborts (**SIGABRT**).
- **With the degrade** (on by default), the skipped task is stamped with a distinguishable teardown
  error code; `functionWithJSRuntime:` recognizes it and returns a non-nil **no-op** bridge function
  so the dying session unwinds quietly instead of aborting.

### Invocation (button 3)

The degrade prevents the resolution abort, but the no-op function it returns is still `_Nonnull` and
still callable. **Invoking** it (`-[…GetTeardownPath getTeardownPath]` → the no-op `callBlock`)
returns a **null value** — and the generated return type is `_Nonnull` (here `NSString * _Nonnull`),
so callers trust it is non-null. Passing that null to an API with a non-null precondition,
`+[NSURL fileURLWithPath:]`, raises `NSInvalidArgumentException` (`nil string parameter`) and aborts
(**SIGABRT**).

This is the invocation-teardown nil-in-`nonnull` crash: the degrade turned a resolution abort into a
null value that flows past the type system and detonates in whatever downstream code trusts the
non-null contract. (The exact downstream varies — a nonnull-arg API like the `NSURL` case here, a
value the caller force-unwraps, etc.)

## What this repro does

- `src/valdi/teardown_probe/src/TeardownReproProbe.ts` — an `@ExportFunction makeTeardownProbe`
  returning an `@ExportProxy` (for the resolution buttons) and an `@ExportFunction getTeardownPath`
  returning a `string` (for the invocation button), built with `async_strict_mode` so codegen emits
  the `SCValdiBridgeFunction` subclasses. The bodies never run — only resolution/invocation are
  exercised.
- `src/ios/SCValdiTeardownRepro.mm` — owns the runtime lifecycle: creates the isolated runtime,
  (for buttons 1 & 2) pins the degrade kill switch via an injected `ITweakValueProvider`, tears the
  runtime down, and performs the resolution (buttons 1 & 2) or the invocation + nonnull-API use
  (button 3).

## How to trigger

Run the Hello World app on iOS and tap a button. Buttons 2 and 3 abort within a second; the crash
matches the corresponding teardown signature:
- button 2 (SIGABRT): `SCValdiErrorThrowWithStacktrace` → `SCValdiMarshallerCppCheck` →
  `-[SCValdiJSRuntimeImpl pushModuleAtPath:inMarshaller:]` → `+[…MakeTeardownProbe functionWithJSRuntime:]`.
- button 3 (SIGABRT): `NSInvalidArgumentException` (`nil string parameter`) ← `+[NSURL fileURLWithPath:]`
  ← `+[SCValdiTeardownRepro reproduceInvocationCrash]`.

Tools can trigger button 3 without a tap: launch with the env var
`SIMCTL_CHILD_TEARDOWN_REPRO_AUTORUN=invocation` (debug-only hook; a normal launch does nothing).
