# Valdi Swift bindings

Swift-facing half of the Valdi iOS bindings. The Objective-C bindings live in
`../valdi_core/` and are a separate, parallel implementation — not a layer this code sits on top of.

## Invariant: no Objective-C exception may cross into Swift

Swift cannot catch `NSException`. When one is raised below a Swift frame, one of two things
happens, and neither is acceptable:

- No Objective-C `@try` exists further up the stack — the unwinder finds no handler and the process
  terminates.
- Some Objective-C caller further up does have a `@try` — the exception is caught, but the unwind
  passed through Swift frames that emit no cleanup, so ARC never runs and whatever those frames
  owned is leaked. A `ValdiMarshaller` leaks its C++ marshaller this way.

So this directory reports errors, it does not raise them:

- C++ is reached through the `SwiftValdiMarshaller_*` shim in `cpp/`, whose functions record onto the
  marshaller's `ExceptionTracker` and return normally.
- `ValdiMarshaller.checkError()` converts a recorded error into a Swift `ValdiError`.

The Objective-C bindings use the opposite convention: `SCValdiMarshallerCheck()` *extracts* the
pending error and raises it as an `SCValdiError` (an `NSException` subclass). Any Objective-C API
that runs `SCValdiMarshallerCheck` internally is therefore unsafe to call from here, even though it
compiles fine and looks harmless at the callsite.

**When you need such an API, add a non-raising variant on the Objective-C side and call that** —
do not wrap the call in an exception-catching trampoline, which leaves the leak in place.
`SCValdiJSRuntime`'s `pushModuleAtPath:reportingErrorOnMarshaller:` is the reference example: it
leaves the failure on the marshaller so the existing `try marshaller.checkError()` observes it, while
`pushModuleAthPath:inMarshaller:` keeps raising for the Objective-C callers that expect it.

Note that this rule is about *runtime* failures. Programming errors that are deliberately fatal on
both sides — the main-thread `NSAssert` in `inflateView:owner:cppMarshaller:`, the
`async_strict_mode` resolution check — are meant to abort and should stay as they are.

## Known gap: Swift callers of Objective-C generated bridge functions

The invariant above holds for the bindings in this directory. It does **not** yet cover Swift code
that resolves an *Objective-C generated* bridge function, which is a common shape for bundles built
with `ios_language = ["objc", "swift"]`. Those go through
`SCValdiMakeBridgeFunctionFromJSRuntime` in `SCValdiBridgeFunction.m`, which runs
`SCValdiMarshallerCheck` internally and raises.

This is easy to miss at the callsite because Swift imports the generated
`+functionWithJSRuntime:` factory as an initializer, so it reads like ordinary Swift and carries no
`try` — e.g. `SCCBusinessDeeplinkHandleBusinessDeeplink(jsRuntime: jsRuntime)` in
`SnapPromoteDeeplinkPlugin.swift`. A module-resolution failure there still terminates the process.

Closing that gap means giving the Objective-C generated factory a non-raising path too; until then,
prefer `ios_language = "swift"` for bundles you intend to consume from Swift.
