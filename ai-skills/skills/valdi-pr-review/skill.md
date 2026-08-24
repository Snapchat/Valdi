# Valdi PR Review

Review guidance for Valdi changes, focused on **regression risk, gating, and
blast radius** — the lens a framework maintainer applies on top of ordinary
correctness review. These are heuristics for what to surface; they never gate on
PR size, title, or structure.

When flagging, use a `snake_case` category: `regression` for ungated changes,
`thread_safety` for JS↔native boundary races, `performance` for hot-path or
allocation cost, `resource_leak` for missing cleanup, and `platform_parity` for
one-sided native changes. Use `lint` and `format` for pre-gate tooling failures
(see below).

## When to use

Reviewing a Valdi PR that touches the runtime, renderer, native backends, or
framework modules. Complements correctness review, it doesn't replace it.

## Lint and formatting pre-gate

Before applying the review lens below, run the project's configured linter and
formatter on the changed files and surface any failures as findings — agents routinely
miss these and only discover them when CI rejects the PR.

- **Discover the commands from the repo; do not assume a tool.** Check `AGENTS.md`,
  `README`, `package.json` scripts, `.pre-commit-config.yaml`, and the CI config for
  the linter and formatter this project runs, then run them against the changed files.
  These are often a **single command** (e.g. ESLint running Prettier via a plugin), not
  two separate tools — run whatever the repo defines rather than assuming a standalone
  `prettier`.
- Report a failing formatter as a `format` finding and a failing linter as a `lint`
  finding, quoting the rule and file. These block like a correctness issue would — a
  clean local build with lint failures still fails CI.
- Scan the diff for newly added suppressions (`eslint-disable`, `// prettier-ignore`,
  `nolint`, and equivalents) and flag any not justified by a comment.

This pre-gate runs the tools; it is **not** the "style, naming, or organization pass"
the review lens deliberately avoids (see Material secondary checks). Keep the two
separate — run lint here, review for regression risk below.

## Gating: does the change run for existing consumers?

The primary regression question: does new or changed code run for **existing
consumers**, and if so, is it gated? A gate is any condition that keeps existing
call sites on their prior behavior — a feature flag, an `if (newProperty != null)`
check, or a `hasFeatureData()` predicate.

```typescript
// ❌ Ungated: the new clause runs for every existing caller of this setter
set value(v) {
  if (v instanceof AttributedText || this.parseNeeded(v)) { /* parse work */ }
}

// ✅ Gated: the new work only runs when the feature's data is present
set value(v) {
  if (this.featureEnabled && v instanceof AttributedText) { /* parse work */ }
}
```

Flag when:

- A new clause or branch is added to an existing setter, hot method, or shared
  utility (a text converter, an equality check, an attributed-text parser) that
  runs for existing consumers without such a gate.
- A flag gates feature *data* at one layer, but a shared lower-layer path still
  runs for non-feature callers. A data-layer flag doesn't gate code that executes
  regardless of whether the data is present.
- A condition, exclusion check, fast path, or short-circuit is removed or relaxed
  in a shared or hot method. Audit both perf (every call site now pays) and
  correctness (the condition may have guarded a real bug — a null-deref or a
  double-render, not just the new feature). Trace why it was there before it goes.
- A bypass is removed, making previously unreachable code newly reachable for
  existing callers. Flag missing null checks or unhandled edge cases on the newly
  reachable path, as if it were new code.

## Blast radius

Weight findings by where the change lands.

- **App-wide (highest bar):** the runtime — ViewNode, the renderer diff, and
  marshalling under `valdi/src/valdi/runtime` — and the JS-engine integrations
  (`valdi/src/valdi/jscore`, `quickjs`, `hermes`, `v8`), plus the core TS modules
  every module depends on. The diff/render path is extremely allocation-sensitive:
  one new object allocation there can add hundreds of ms across thousands of nodes.
- **Scoped (trace consumers first):** shared component libraries, navigation,
  dependency injection, module loading, protobuf, sqlite bindings. Flag a change
  to an exported symbol or a native view type here, and note that its direct and
  transitive consumers — wrappers down to the leaf modules — need a breakage
  check; a native view change carries risk through the TS element name it maps to.
- **Developer tooling (lowest bar, no user impact):** debugger, inspector,
  playground, benchmarking/profiling, hot reloader.

## Native review checks

### C++ runtime and the JS ↔ native boundary

Paths: `valdi/src/valdi/{runtime,jscore,quickjs,hermes,v8,snap_drawing}` and
marshalling.

- Operations dispatch between the JS thread and the main/UI thread. Flag missing
  locks, synchronous cross-thread dispatch (deadlock risk), and concurrent access
  to shared state.
- Reference/view types that cross the boundary (e.g. a bytes view) must have
  caller-managed lifetime. Flag a reference held past the caller's scope.
- Flag count-based throttling introduced where time-based throttling would bound
  cost better.
- Flag code that uses `std::function` instead of `Function`, `std::unordered_map`
  instead of a flat map, or fails to use `std::move` for non-trivial types. Also
  flag error paths that return an empty result instead of using `Result<T>`.
- SnapDrawing is an allocation-sensitive render path; flag a new synchronous API
  that races the main thread against draw calls, as it can deadlock.

### Android

Paths: `valdi/src/valdi/android`, `valdi/src/java/com/snap/valdi/views`.

- Tests ≠ perf-safe: JVM/Robolectric tests can't measure JNI roundtrip cost, real
  `onDraw` frame time, native allocation pressure, bitmap accumulation, or
  low-end-device behavior. Flag new perf-critical Android code (per-frame
  `onDraw`, JNI in a hot path, render-path allocation) to note that green CI's
  JVM tests haven't validated its cost — a feature flag is the practical safety net.
- Per-frame predicate cost: when new `onDraw` code calls a predicate to decide
  whether to run feature logic, the predicate cost itself is ungated — every
  frame, every consumer. Cache it at bind time.

```kotlin
// ❌ predicate runs every frame for every consumer
override fun onDraw(canvas: Canvas) {
  if (attributedText.hasAnimationTransform()) { /* ... */ } // O(N JNI) per frame
}

// ✅ computed once at bind time; onDraw reads a cached field
fun setAttributedText(text: AttributedText) {
  this.hasAnimationTransform = text.hasAnimationTransform()
}
override fun onDraw(canvas: Canvas) {
  if (hasAnimationTransform) { /* ... */ }
}
```

- Attributed-text predicates (`hasAnimationTransform()`,
  `hasActiveAnimationTransform()`, `hasRenderableAnimationTransform()`,
  `hasOutline()`, and similar) iterate every part with at least one JNI call per
  part — they look O(1) but are O(N JNI). In `onDraw` or any per-frame entry
  point, flag them unless the result is cached at bind time. A 5-part caption at
  60fps costs ~600 JNI crossings/second per consumer.
- Per-frame / per-flush allocation: in `onDraw`/animation-tick and view-operation
  flush entry points, flag allocations (StringBuilder, String concat, ArrayList,
  lambda) and tree walks — including those in directly-called helpers — that fire
  N times per flush.
- Diagnostic on a hot path: when telemetry, crash metadata, or tracing is added to
  a hot path, flag when the prep feeding the slow/failure branch runs eagerly
  rather than behind the threshold. Hold refs plus an index and stringify lazily;
  the shape to push toward is `if (elapsedMs > THRESHOLD) { /* capture */ }`.
  Verify author cost claims by reading the diff, not the description.
- Flag UI touches in `finalize()` (runs on the finalizer thread). Flag use of
  `LinkedList` in favor of array-backed lists (GC pressure). Flag singleton →
  per-call allocation changes that compound across every view transaction.

### iOS

Paths: `valdi/src/valdi/ios`.

- UIKit is main-thread-only. `@synchronized` plus synchronous dispatch to another
  thread is a known deadlock pattern that has caused production ANRs. Flag code
  called from background contexts (timers, reporters, observers) that contends
  with the main thread.
- `auto &` in an ObjC++ block does not retain — flag a captured reference that
  must be copied.
- Flag a missing `isKindOfClass:` check before casting (`NSNull` is truthy).
- Flag `@synchronized` around immutable state set in the initializer (unnecessary).

## Material secondary checks

Include only when material — a regression and performance lens, not a style,
naming, or organization pass.

- **Lazy over eager:** flag eager static initializers, eager schema registration
  on startup, or eager asset preloading where lazy initialization would work.
- **Resource lifecycle:** when a PR adds a cache or pool of OS-managed resources
  (bitmaps, file handles, native pointers, listeners), flag a missing cleanup path
  (`recycle`/`close`/`dispose`/`clear`).
- **Cross-platform parity:** when a PR changes only one platform's native
  implementation, flag that implementation to ask whether the other platform needs
  an equivalent update. Doesn't apply to platform-specific DI wiring or the shared
  C++ runtime.
- **Runtime isolation:** keep the core runtime free of host-application
  dependencies; flag a new direct dependency from runtime code into app-specific
  modules.
