# RFC 0002: Linux SDK builds for headless Valdi services

- Status: Draft
- Author(s): @sp-ben-dodson
- Discussion: [This pull request](https://github.com/Snapchat/Valdi/pull/197).
- Implementation: Not implemented. The [historical archive experiment](https://github.com/kaboodleLab/Valdi/pull/4) does not meet the revised stage-1 contract.

## Summary

Add a source-built, Bazel-only Linux export to the existing Valdi CLI and library-export pipeline. Stage 1 qualifies a QuickJS-only, TSN-free, drawing-free standalone core, a small experimental embedding facade and a clean external Bazel consumer. Linux remains **Development only, not supported for production deployment**. Acceptance covers this source-consumption stage, not prebuilt publication, a new support tier or a stable C++ ABI.

## Motivation

A Linux host should be able to load compiled Valdi service modules and call generated native bindings without creating a window or rediscovering transitive headers, resources and link requirements. The initial use case does not require native TypeScript execution. Building runtime, modules and bindings together with Bazel is sufficient; direct-link archive experiments do not establish a distribution contract.

### Prior art

- [`valdi_cli_application`](../bzl/valdi/valdi_cli_application.bzl), [`cli_runner`](../valdi/src/valdi/cli_runner/CLIRunner.cpp) and the [CLI application documentation](../docs/docs/workflow-cli-application.md) already provide a source-built command-line host. [SSR #148](https://github.com/Snapchat/Valdi/pull/148) uses that path. `enable_http` optionally links the curl-backed request manager; it is off by default.
- [`valdi_exported_library`](../bzl/valdi/valdi_exported_library.bzl) already exports xcframework, AAR and npm outputs. [`valdi export <platform>`](../npm_modules/cli/src/commands/export.ts) and [`test_exported_lib.sh`](../tools/ci/test_exported_lib.sh) provide the command and validation seam to extend.
- [`validate_registry.sh`](../tools/ci/validate_registry.sh) supplies a clean Bzlmod-consumer pattern; [`test_external_build_graph.sh`](../tools/ci/test_external_build_graph.sh) already checks external engine isolation.
- `valdi_linux_application` takes a root component and links the drawing runtime. It is not the service bootstrap. The current `valdi_standalone_runtime` itself links SnapDrawing and registers its module provider under `SNAP_DRAWING_ENABLED`; changing the Linux alias alone cannot produce a drawing-free core.

These observations were checked against public source `ede1462368bbc233c507dd7136a3602d457a56b8`. They describe starting points, not completed stage-1 behavior.

## Detailed design

### Acceptance scope and shared ownership

Stage 1 supports native Linux x86_64/glibc **Bazel consumers only**, using public source dependencies. Reuse the CLI bootstrap and source compiler/companion/toolbox; do not add another host implementation, SDK archive format or independently versioned manifest contract. The Swift/compiler build requirements remain explicit prerequisites. Copying compiler launchers or successfully linking a runtime does not qualify a portable compiler toolchain.

RFC 0002 owns the drawing-free core and the shared embedding lifecycle. [RFC 0001 (#196)](https://github.com/Snapchat/Valdi/pull/196) consumes those changes and adds optional drawing/window integration; it does not independently implement queue ownership or shutdown. These are separately reviewable proposals with shared prerequisites. Core/lifecycle fixes can land incrementally without waiting for the complete export command. Existing CLI/SSR and platform paths must retain regression coverage.

### Extend the existing export pipeline

Add a Linux variant of `valdi_exported_library` and expose it through `valdi export linux`, including target discovery, output handling and CLI tests. The export carries the complete transitive public/generated headers, static libraries, module resources and Bazel consumer definitions derived from the configured targets. Preserve `CcInfo` include paths, definitions, archive ordering, system link options and `alwayslink` registration semantics. An external fixture imports that output through Bzlmod and builds its host and modules with the same declared source/toolchain configuration.

Compatibility metadata belongs with this existing export output. Record exact source and dependency identities; compiler/companion/toolbox and module-format identities; QuickJS version and bytecode format; LLVM version; target architecture; C++ standard and standard-library implementation/linkage; runtime/unwind linkage; exception and RTTI settings; observed glibc requirements; and static-registration/whole-archive requirements. Reject detectable mismatches before module execution. Do not infer a stable ABI or publish a separate SDK release version from these records. Third-party notices and unresolved notice-discovery gaps accompany the export.

The current open-source toolchain selects LLVM 19.1.7 with no Linux sysroot, C++20 and libc++; runtime compilation disables exceptions. Export the actual configured settings, including libc++ compatibility definitions and per-dependency exceptions/RTTI requirements. A native build inherits its builder's glibc floor unless a separately qualified sysroot/container changes it. Build and consume separately on Ubuntu 22.04 and 24.04; neither success nor an ELF inspection implies arbitrary older-distribution portability. A libstdc++ consumer cannot exchange `std::` objects, `Value` or `StringBox` with a libc++ runtime. Non-Bazel link recipes, including manual `--whole-archive`, are deferred; stage 1 retains those semantics through Bazel.

### Required runtime build changes

QuickJS is the sole stage-1 engine. Desktop engine selection currently chooses a default among enabled bridges rather than proving a single-engine closure; the open-source `.bazelrc` defaults to Hermes. Set both `--//bzl/valdi:js_engine=quickjs` and `--//bzl/valdi:js_bytecode_format=quickjs` explicitly for the runtime and all generated module resources. Reject unsupported engine/native-TSN selections instead of silently substituting them.

The `valdi_quickjs -> //tsn` dependency must be separated so this variant links neither TSN nor Hermes/JSCore. Generated native bindings are part of the service fixture; **native TypeScript execution is not**. A QuickJS capability flag alone does not satisfy this build requirement.

Split the standalone runtime into a drawing-free core plus an optional SnapDrawing provider. The service variant must not register that provider or link Skia, ICU, codecs, skottie, SVG, FreeType or other drawing/font dependencies through another edge. Rendering, layout and offscreen raster are outside this profile. Check configured dependency/action graphs and linked symbols, not only runtime initialization or labels. Report static-archive and complete link-closure sizes before and after the split under the same source/toolchain/optimization settings; do not invent a size target before measurement.

### Embedding facade and lifecycle

Document a small experimental facade with `create`, `load`, `call` and `shutdown` operations. Options describe host queue ownership, exit notification, explicitly enabled adapters and environment exposure. Calls return results or asynchronous completions with cancellation and bounded shutdown; internal `ValdiStandaloneRuntime`/runtime classes are not the SDK API. The facade remains experimental until a separate `VERSIONING.md` C++ policy exists. This is an API-design commitment, not a claim that these operations are implemented today.

The current exit coordinator disposes the main queue when JS and main queues become idle and occupies the single main-queue listener slot. Some standalone callbacks (`destroyAllComponents`, `getRenderedElement`, `exit`) capture raw `this`; component destruction also enqueues directly. Both `process.exit` and `valdiStandalone.exit` can dispose the host queue. The embedding profile requires these shared changes:

1. **Host-owned exit policy.** Embedded services remain alive while idle and awaiting host calls. Keep command-line auto-exit available explicitly for CLI/SSR compatibility. JS exit requests report a status to a host callback; they do not independently dispose an embedded host's loop.
2. **Supported queue subscriptions.** Provide cancelable queue wakeup/idle subscriptions that can coexist with exit coordination. Document callback threading and lock/reentrancy rules; hosts do not steal the listener slot or rely on a testing accessor.
3. **Admission and shutdown barrier.** Close admission to new application work when shutdown begins, reject subsequent submissions with a defined result, and weakly reference or cancel runtime callbacks. Already-admitted teardown work has a bounded drain/cancel path and completion barrier before owned runtime/VM resources are released. A disposed-queue check alone is insufficient. Host pumping after shutdown must not invoke stale runtime callbacks.
4. **Deterministic completion.** Repeated shutdown and cancellation are safe; admitted calls complete or cancel once. Signal handling and process supervision stay with the host. Test shutdown, cancellation and exit races while work is queued, including continued pumping after destroy.

### Capabilities and defaults

The starting matrix is explicit; existing stubs are not evidence of supported service adapters:

| Surface | Current source behavior | Stage-1 embedding contract |
|---|---|---|
| Module loading/generated native bindings | Existing source compilation and module APIs | Qualify matched QuickJS modules and generated bindings through the export fixture; reject missing/corrupt/incompatible modules and entry errors. |
| Device appearance | `observeDarkMode` is a no-op at the inspected base; [#199](https://github.com/Snapchat/Valdi/pull/199) separately fixes host-supplied observation | No OS appearance discovery. Include host-supplied observation only after its implementation/tests land; otherwise report it unsupported. |
| Insets/haptics | `observeDisplayInsetChange` and `performHapticFeedback` are no-ops | Unsupported without a host adapter. |
| Application observers | Background, foreground and keyboard-height observers are no-ops | Unsupported without a host adapter. |
| Linux identity | `DeviceModule::getSystemType()` returns `"ios"` | Correct/define the Linux service identity with a compatibility regression; do not advertise the stub as Linux support. |
| Cache/keychain | In-memory implementations; values do not survive restart | Document ephemeral behavior. Persistent storage/keychain are out of scope. |
| HTTP | Only works with `enable_http`; otherwise requests fail with `No RequestManager set` | Optional curl-backed adapter with an independent local-server test and declared dependencies. Off in the minimal fixture. |
| Workers, ClientSQL and other modules | Not qualified by the CLI bootstrap | Not advertised; require separate adapters/resources/tests. |

For the new embedding facade, requesting an absent adapter returns an explicit unsupported result; it must not silently succeed through a stub. Preserve existing CLI/SSR no-op behavior unless a separately reviewed compatibility change is agreed. Document that distinction rather than changing every caller through a shared default.

The embedding API and service example default debugger and hot reload **off regardless of build flavor**. If a host explicitly enables a debugger, its transport binds loopback only. Existing `CLIRunner` enables debugging in development builds, so this needs an explicit profile option rather than relying on `production`. Host environment exposure is opt-in through an explicit allowlist, empty by default; do not populate the new service's `process.env` from the whole host environment. Preserve existing CLI/SSR behavior pending separately reviewed changes.

### Non-goals

Production deployment/support; non-Bazel or CMake consumers; a stable C ABI or language bindings; TSN; Hermes or Linux JSCore in the baseline; musl, arm64 or cross-compilation; rendering/layout/offscreen raster; default service debugger/hot reload; persistent storage/keychain; prebuilt compiler/runtime publication; and byte-for-byte reproducibility. The export may retain checksums for identity checks without claiming signed/authenticated distribution.

## Automated test strategy

These are **proposed profile merge gates**, not tests already wired or passing and not a new Linux release-blocking support tier. Reuse four-core Linux CI tasks and the Ubuntu 22.04/24.04 source-build matrix pattern from [`test-cli-linux.yml`](../.github/workflows/test-cli-linux.yml). Put execution in `tools/ci/*.sh` so both public and internal CI can call the same scripts; add corresponding `linux-ci` tasks in [`bzl-changes.yml`](../.github/workflows/bzl-changes.yml), not workflow-only inline tests.

| Gate/script | Required regression coverage |
|---|---|
| Extend `tools/ci/test_exported_lib.sh` and CLI export tests | Build the Linux variant through `valdi export linux`, including target discovery/output handling and complete export metadata/resources. Existing xcframework/AAR/npm behavior stays covered. |
| Extend `tools/ci/validate_registry.sh` with a clean Bzlmod fixture | Build a consumer using only the exported Linux target and declared source toolchain prerequisites, without producer checkout/output paths. Load a freshly compiled QuickJS module, call generated native bindings and complete an asynchronous host callback. No TSN execution. Remove display environment/sockets and GPU access; enforce a runtime timeout. |
| Extend `tools/ci/test_external_build_graph.sh` | Inspect configured dependency/action graphs and linked symbols for the **service runtime/consumer closure**, failing on Hermes, JSCore, TSN, Skia, ICU and drawing/font dependencies. Distinguish build-time compiler dependencies from deployed runtime dependencies; assert forbidden libraries are not copied into the runtime export. Record before/after archive and link-closure sizes. |
| Proposed `tools/ci/test_linux_embedding.sh` | Test create/load/call/shutdown, missing/corrupt/mismatched modules, unsupported adapters, entry errors, native registration/alwayslink, environment allowlisting and debugger/hot-reload defaults in both development and production flavors. HTTP opt-in uses a local server; HTTP-off remains explicit. |
| Focused ASan lifecycle harness, invoked by `test_linux_embedding.sh` | Repeat create/call/destroy with queued work; cover idle keepalive, exit callback ownership, subscription removal, admission rejection, cancellation/exit/shutdown races and pumping after destroy. Require correct completion/lifetime assertions in addition to absence of sanitizer diagnostics. |
| Source/ABI matrix and existing platform scripts | Record/check engine/bytecode/tool identities, ELF architecture and glibc/C++ requirements on Ubuntu 22.04/24.04. Existing CLI/SSR, iOS, Android, macOS and web jobs protect shared build/runtime/compiler changes. |

Run the source-consumer and focused ASan lifecycle gates on PRs touching this profile and on matching main changes, with nightly/manual runs as additional coverage. Filters must include `.bazelrc`, toolchain/dependency pins, `bzl/**`, CLI export code/tests, standalone/core/queue/engine sources, relevant modules/compiler inputs, export/consumer fixtures and `tools/ci` scripts/workflow definitions. Include the export fixture path chosen during implementation. No filter should omit a shared dependency that can change the configured closure.

The existing [`valdi-sanitizer-tests.yml`](../.github/workflows/valdi-sanitizer-tests.yml) runs on main/nightly/manual triggers, not PRs; that does not meet the new profile gate. Add focused PR coverage through the shared script. Do not count `//valdi:test_linux_bin` (currently ungated) or enum-only Linux tests as embedding qualification. Avoid depending on Linux `test_runtime` while its unrelated gtest static-destructor failure is unresolved; use the gated integration path or dedicated focused harness. The [normal test script](../tools/ci/run_tests.sh) and sanitizer script remain shared regression references.

Proposed initial caps are **45 minutes per four-core build/test task and five minutes per runtime fixture**. Measure cold/warm CI cost, peak memory and artifact size before maintainers finalize the budget and task ownership; do not claim acceptance assigns CI capacity or an owner. No publication job is added. Compatibility/support-policy and dependency-notice review remain human gates; byte-for-byte independent-build reproducibility and performance guarantees are unqualified.

## Compatibility

Linux stays [Development only](../VERSIONING.md#platform-support), **not supported for production deployment**; a deployed service is a shipping use. A new tier requires a separate `VERSIONING.md` change and agreed ownership/CI commitments. An experimental facade is not an experimental support tier, and undocumented internal C++ APIs gain no stability guarantee.

The new export/profile is opt-in. Preserve CLI/SSR command-line exit and existing adapter semantics; isolate new embedding defaults and guard shared changes with regressions. For the new Linux export, behavior on **iOS, Android, macOS and web is N/A/unchanged**. Their existing source/compiler/export tests must catch unintended shared-path changes. No migration to a new SDK archive is required or proposed.

## Alternatives considered

- **Separate SDK archive/manifest and direct non-Bazel linking:** useful as historical experiments, but duplicates the export pipeline and expands ABI/distribution obligations. Defer it; stage 1 uses the existing Bazel path.
- **Reuse the current desktop runtime unchanged:** saves design work but retains TSN/drawing dependencies and command-line exit behavior. Capability flags alone cannot qualify a minimal embedded service.
- **Publish compiler tools first:** could reduce Linux Swift setup cost, but no portable copied-tool closure has been qualified. Publication needs a follow-up RFC with an owner, provenance plan and security review.
- **Bundle the graphical host or introduce a stable cross-language ABI:** introduces unrelated platform and compatibility commitments before the shared core is qualified.

## Drawbacks and risks

Separating TSN, drawing registration and lifecycle touches shared build/runtime paths. Preserving CLI/SSR and existing platform behavior needs independently reviewable changes and real regressions. Source builds still require a Swift toolchain and may be costly on four-core runners. Toolchain/C++/glibc coupling persists even with Bazel; metadata cannot make arbitrary binary combinations safe. Size reductions, error classifications, capability behavior and shutdown correctness remain to be demonstrated. This RFC creates no binary release, provenance/signing mechanism, distribution owner or release-support obligation.

## Unresolved questions

- Which exact facade/result/cancellation types and supported queue-subscription API fit existing runtime conventions? Confirm them in small shared-core reviews before coupling either host to a new interface.
- Which concrete target/provider split removes all forbidden runtime dependencies while preserving CLI/SSR and optional drawing behavior? Measure the resulting closure rather than assuming the split is sufficient.
- Which existing export provider/output shape should carry Linux Bazel imports and compatibility metadata? Resolve in the export implementation without inventing a parallel versioned SDK format.
- Who reviews/owns the proposed profile gates, and do measured cold/warm costs fit the initial CI caps? Acceptance alone assigns no owner or new budget.
- Final RFC numbering is decided at acceptance; the current RFC 0002 filename/number is retained during review. Publication and any support-tier change require separate proposals.
