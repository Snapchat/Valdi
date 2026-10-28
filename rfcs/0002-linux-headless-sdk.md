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

These observations were checked against public source `6822d68b22192952222c4d8b95a78980c07b9600`. They describe starting points, not completed stage-1 behavior.

## Detailed design

### Acceptance scope and shared ownership

Stage 1 supports native Linux x86_64/glibc **Bazel consumers only**, using public source dependencies. Put the embedding facade below `runValdiStandalone()`; the CLI becomes a client of it, sharing initialization rather than providing a second bootstrap. Reuse the source compiler/companion/toolbox and existing export command. Define the Linux export layout below as a versioned platform output, not an independently published SDK distribution. The Swift/compiler build requirements remain explicit prerequisites. Copying compiler launchers or successfully linking a runtime does not qualify a portable compiler toolchain.

RFC 0002 owns the drawing-free core, shared embedding lifecycle, and optional drawing-provider registration/configuration/access contracts. RFC 0001 owns the SDL host's scheduler, finite cache/resource policy, isolated fontconfig build setting, and display/CI integration. [RFC 0001 (#196)](https://github.com/Snapchat/Valdi/pull/196) consumes those changes and adds optional drawing/window integration; it does not independently implement queue ownership or shutdown. These are separately reviewable proposals with shared prerequisites. Core/lifecycle fixes can land incrementally without waiting for the complete export command. Existing CLI/SSR and platform paths must retain regression coverage.

### Extend the existing export pipeline

The Linux export is an optional handoff/reuse layer over a source build. It gives a second Bazel workspace a declared import of an already-built runtime/module set, carrying the transitive headers, archive/alwayslink semantics and resource registration together. It does not add service capabilities over a direct source dependency or promise a build-speed improvement over Bazel caching. Building new modules still requires the matched Valdi source, compiler/companion/toolbox and Swift prerequisites. Consumers already using the Valdi source graph can use the profile/facade directly without this export; it is not a standalone compiler SDK.

Add a Linux variant of `valdi_exported_library` and expose it through `valdi export linux`, including target discovery, output handling and CLI tests. Its archive output is a `.tar.gz` containing one source-built Bzlmod import module named `valdi_linux_export`, with this relative layout:

```text
MODULE.bazel
BUILD.bazel
include/...
lib/*.a
resources/<module>/*.valdimodule
metadata/export.json
notices/...
```

This **is a versioned Linux export format**: `metadata/export.json` begins with `format_version: 1`. It is reviewed here as a new platform output of the existing pipeline, not deferred as an unspecified format or given a separate binary release/publication policy. Stage 1 uses local source-built outputs; the module can be imported with a Bzlmod `local_path_override` in the clean consumer fixture. Any future publication or registry delivery requires the deferred policy work.

`MODULE.bazel` declares the resolved dependencies/toolchain references needed by the import, including the exact source identity of the Valdi build rules and source compiler tools used for module generation. It does not silently register a different compiler or embed unqualified copied compiler launchers. The consumer declares its matching toolchain prerequisites. `BUILD.bazel` provides a public `:runtime` target for the experimental facade and complete native runtime closure, plus `:module_<name>` targets mapping each exported module to its generated binding headers, native/resource-registration archives and runtime resources. The target-to-module mapping is recorded in metadata. Imported `cc_import` archives and `cc_library` wrappers preserve configured `CcInfo` include paths, compile definitions, archive ordering, system link options and `alwayslink` registration semantics. Resources and generated registration code are declared Bazel inputs/dependencies rather than ambient runfiles.

All required public/generated/transitive headers, static archives and module resources must be in this output or explicitly declared as supported toolchain/system prerequisites. Paths inside BUILD/metadata are relative; reject producer checkout/output paths, undeclared libraries/resources and escaping symlinks. `metadata/export.json` records the layout version, public targets and module mapping, ordered archive/link/alwayslink requirements, include paths/definitions, file identities, capabilities and omissions, and dependency versions/notices. The format is experimental; schema changes require an explicit format-version change and matching import validation. It does not imply stable C++ ABI or publication support.

Compatibility fields record exact source/configuration identities; compiler/companion/toolbox and module-format identities; QuickJS version/ABI and bytecode-format version; LLVM version; target architecture; C++ standard and standard-library implementation/linkage; runtime/unwind linkage; exception and RTTI settings; observed glibc requirements; and static-registration/whole-archive requirements. Notice discovery gaps remain explicit.

Stage 1 keeps the existing `.valdimodule` format and loader behavior. Qualification uses the matched source/profile recorded in `metadata/export.json`; no new runtime/module compatibility stamp or pre-execution identity-checking mechanism is required by this RFC.

The current open-source toolchain selects LLVM 19.1.7 with no Linux sysroot, C++20 and libc++; runtime compilation disables exceptions. Export the actual configured settings, including libc++ compatibility definitions and per-dependency exceptions/RTTI requirements. A native build inherits its builder's glibc floor unless a separately qualified sysroot/container changes it. Build and consume separately on Ubuntu 22.04 and 24.04; neither success nor an ELF inspection implies arbitrary older-distribution portability. A libstdc++ consumer cannot exchange `std::` objects, `Value` or `StringBox` with a libc++ runtime. Non-Bazel link recipes, including manual `--whole-archive`, are deferred; stage 1 retains those semantics through Bazel.

### Required runtime build changes

QuickJS is the sole stage-1 engine. Desktop engine selection currently chooses a default among enabled bridges rather than proving a single-engine closure; the open-source `.bazelrc` defaults to Hermes. Set both `--//bzl/valdi:js_engine=quickjs` and `--//bzl/valdi:js_bytecode_format=quickjs` explicitly for the runtime and all generated module resources. Reject unsupported engine/native-TSN selections instead of silently substituting them.

The `valdi_quickjs -> //tsn` dependency must be separated so this variant links neither TSN nor Hermes/JSCore. Generated native bindings are part of the service fixture; **native TypeScript execution is not**. A QuickJS capability flag alone does not satisfy this build requirement.

Split the standalone runtime into a drawing-free core plus an optional SnapDrawing provider. The service variant must not register that provider or link Skia, ICU, codecs, skottie, SVG, FreeType or other drawing/font dependencies through another edge. Rendering, layout and offscreen raster are outside this profile. Check configured dependency/action graphs and linked symbols, not only runtime initialization or labels. Report static-archive and complete link-closure sizes before and after the split under the same source/toolchain/optimization settings; do not invent a size target before measurement.

### Embedding facade and lifecycle

Document a small experimental facade with `create`, `load`, `call` and `shutdown` operations **below `runValdiStandalone()`**. `createValdiStandaloneRuntime()` and `runValdiStandalone()`/`CLIRunner` become clients of the same facade and shared initialization: the CLI supplies command-line signal/exit policy and pumps its loop; an embedded host supplies its own loop and lifetime. The facade does not call a loop-owning CLI bootstrap. These proposed refactor boundaries avoid two bootstraps while preserving existing CLI/SSR behavior. Options describe host queue ownership, exit notification, explicitly enabled adapters and environment exposure. Calls return results or asynchronous completions with cancellation and bounded shutdown; internal `ValdiStandaloneRuntime`/runtime classes are not the SDK API. The facade remains experimental until a separate `VERSIONING.md` C++ policy exists. This is an API-design commitment, not a claim that these operations are implemented today.

**Optional provider contract (provisional experimental names).** The drawing-free facade exposes the existing generic `registerModuleFactoriesProvider(...)` seam before setup/load, typed only in the generic module-provider interface. In the optional SnapDrawing provider, `SnapDrawingModuleFactoriesProvider::configure(options)` is called before first use, then `getOrCreateRuntime()` lazily returns the same configured drawing runtime. Late reconfiguration is rejected. The provider owns the configuration and runtime lifetime; SnapDrawing options/types and accessors stay in that optional provider/library, not the drawing-free facade. RFC0002 owns these registration/configuration/access semantics; RFC0001 supplies the SDL scheduler and finite cache/resource options and its isolated fontconfig integration. These names are proposed for coordination, not already available or stable API.

The current exit coordinator disposes the main queue when JS and main queues become idle and occupies the single main-queue listener slot. Some standalone callbacks (`destroyAllComponents`, `getRenderedElement`, `exit`) capture raw `this`; component destruction also enqueues directly. Both `process.exit` and `valdiStandalone.exit` can dispose the host queue. The embedding profile requires these shared changes:

1. **Host-owned exit policy.** Embedded services remain alive while idle and awaiting host calls. Keep command-line auto-exit available explicitly for CLI/SSR compatibility. JS exit requests report a status to a host callback; they do not independently dispose an embedded host's loop.
2. **Supported queue subscriptions.** Provide cancelable queue wakeup/idle subscriptions that coexist with exit coordination and include next-task-deadline changes. Provisional names are `getNextTaskDeadline()` for a monotonic `steady_clock` snapshot (or none) and `onNextTaskDeadlineChanged(deadline)` for notification. Cover earlier, later and none transitions, including rescheduling/cancellation while the queue remains nonempty. Subscribe and obtain a versioned/current snapshot with a race-safe protocol so a host can arm/rearm its wait without losing a wakeup; notifications and the latest snapshot must agree. Document callback threading and lock/reentrancy rules without requiring callbacks to reenter a held queue lock. Hosts do not steal a listener slot or rely on a testing accessor.
3. **Admission and shutdown barrier.** `beginShutdown` closes admission to new application work and rejects subsequent application submissions, but does **not** close the generation-scoped teardown capability. That capability admits context destruction and dependent cleanup on both main and JS queues, including descendants submitted **after shutdown begins**; it is not a general application-work bypass. Weakly reference or cancel runtime callbacks. Bound/drain or cancel this teardown generation, seal its admission only when dependent cleanup is complete, and signal its completion barrier before releasing runtime/VM resources. A disposed-queue check or a barrier placed before later teardown submissions is insufficient. The host keeps pumping both queues while refusing new application work; after the barrier, no stale runtime callback may run.
4. **Deterministic completion.** The provisional asynchronous operation `destroyAllContextsAsync(completion)` follows the existing `ContextManager::destroyAllContexts` naming. Completion is delivered exactly once through the still-live host queue **after all dependent generation-scoped main and JS teardown work completes**, including cleanup admitted after `beginShutdown`. It does not merely acknowledge enqueueing the old `destroyAllComponents` callback. The host must retain the queue, provider and required resources until completion, then finalize shutdown/release; no synchronous cross-thread wait is required. Repeated shutdown and cancellation are safe; admitted calls complete or cancel once. Signal handling and process supervision stay with the host. Test shutdown, cancellation and exit races, later teardown descendants, continued queue pumping after destroy and no callbacks after the final barrier.

### Capabilities and defaults

The starting matrix is explicit; existing stubs are not evidence of supported service adapters:

| Surface | Current source behavior | Stage-1 embedding contract |
|---|---|---|
| Module loading/generated native bindings | Existing source compilation and module APIs | Qualify matched QuickJS modules and generated bindings through the export fixture; cover missing/corrupt module failures and entry errors. |
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
| Extend `tools/ci/test_exported_lib.sh` and CLI export tests | Build the Linux Bzlmod import module through `valdi export linux`; validate format version, declared targets/headers/archives/resources/metadata and relocatable paths. Reject incomplete or unsupported layouts. Existing xcframework/AAR/npm behavior stays covered. |
| Extend `tools/ci/validate_registry.sh` with a clean Bzlmod fixture | Build a consumer using only the exported Linux target and declared source toolchain prerequisites, without producer checkout/output paths. Load a freshly compiled QuickJS module, call generated native bindings and complete an asynchronous host callback. No TSN execution. Remove display environment/sockets and GPU access; enforce a runtime timeout. |
| Extend `tools/ci/test_external_build_graph.sh` | Inspect configured dependency/action graphs and linked symbols for the **service runtime/consumer closure**, failing on Hermes, JSCore, TSN, Skia, ICU and drawing/font dependencies. Distinguish build-time compiler dependencies from deployed runtime dependencies; assert forbidden libraries are not copied into the runtime export. Record before/after archive and link-closure sizes. |
| Proposed `tools/ci/test_linux_embedding.sh` | Test the common facade below the CLI and embedded clients, missing/corrupt modules, export-metadata/profile validation, unsupported adapters, entry errors, native registration/alwayslink, environment allowlisting and debugger/hot-reload defaults in both build flavors. HTTP opt-in uses a local server; HTTP-off remains explicit. |
| Focused ASan lifecycle harness, invoked by `test_linux_embedding.sh` | Repeat create/call/destroy with queued work; cover idle keepalive, exit ownership, deadline earlier/later/none transitions and snapshot/notification races, subscription removal, rejection of new application work, and teardown descendants admitted on main/JS queues after shutdown begins. Assert `destroyAllContextsAsync` completion runs once on the live host queue only after dependent cleanup and before resource release. Include cancellation/exit races and pumping after destroy, with real lifetime assertions as well as sanitizer checks. |
| Source/ABI matrix and existing platform scripts | Record/check engine/bytecode/tool identities, ELF architecture and glibc/C++ requirements on Ubuntu 22.04/24.04. Existing CLI/SSR, iOS, Android, macOS and web jobs protect shared build/runtime/compiler changes. |

Run the source-consumer and focused ASan lifecycle gates on PRs touching this profile and on matching main changes, with nightly/manual runs as additional coverage. Filters must include `.bazelrc`, toolchain/dependency pins, `bzl/**`, CLI export code/tests, standalone/core/queue/engine sources, relevant modules/compiler inputs, export/consumer fixtures and `tools/ci` scripts/workflow definitions. Include the export fixture path chosen during implementation. No filter should omit a shared dependency that can change the configured closure.

The existing [`valdi-sanitizer-tests.yml`](../.github/workflows/valdi-sanitizer-tests.yml) runs on main/nightly/manual triggers, not PRs; that does not meet the new profile gate. Add focused PR coverage through the shared script. Do not count `//valdi:test_linux_bin` (currently ungated) or enum-only Linux tests as embedding qualification. Avoid depending on Linux `test_runtime` while its unrelated gtest static-destructor failure is unresolved; use the gated integration path or dedicated focused harness. The [normal test script](../tools/ci/run_tests.sh) and sanitizer script remain shared regression references.

Proposed initial caps are **45 minutes per four-core build/test task and five minutes per runtime fixture**. Measure cold/warm CI cost, peak memory and artifact size before maintainers finalize the budget and task ownership; do not claim acceptance assigns CI capacity or an owner. No publication job is added. Compatibility/support-policy and dependency-notice review remain human gates; byte-for-byte independent-build reproducibility and performance guarantees are unqualified.

## Compatibility

Linux stays [Development only](../VERSIONING.md#platform-support), **not supported for production deployment**; a deployed service is a shipping use. A new tier requires a separate `VERSIONING.md` change and agreed ownership/CI commitments. An experimental facade is not an experimental support tier, and undocumented internal C++ APIs gain no stability guarantee.

The new export/profile is opt-in. Preserve CLI/SSR command-line exit and existing adapter semantics; isolate new embedding defaults and guard shared changes with regressions. For the new Linux export, behavior on **iOS, Android, macOS and web is N/A/unchanged**. Their existing source/compiler/export tests must catch unintended shared-path changes. No migration to a new SDK archive is required or proposed.

## Alternatives considered

- **Independently published SDK distribution and direct non-Bazel linking:** useful as historical experiments but expands ABI/release obligations. Stage 1 instead defines a versioned Linux Bzlmod output of the existing source-built export pipeline; publication and non-Bazel consumption remain deferred.
- **Reuse the current desktop runtime unchanged:** saves design work but retains TSN/drawing dependencies and command-line exit behavior. Capability flags alone cannot qualify a minimal embedded service.
- **Publish compiler tools first:** could reduce Linux Swift setup cost, but no portable copied-tool closure has been qualified. Publication needs a follow-up RFC with an owner, provenance plan and security review.
- **Bundle the graphical host or introduce a stable cross-language ABI:** introduces unrelated platform and compatibility commitments before the shared core is qualified.

## Drawbacks and risks

Separating TSN, drawing registration and lifecycle touches shared build/runtime paths. Preserving CLI/SSR and existing platform behavior needs independently reviewable changes and real regressions. Source builds still require a Swift toolchain and may be costly on four-core runners. Toolchain/C++/glibc coupling persists even with Bazel; metadata cannot make arbitrary binary combinations safe. Size reductions, error classifications, capability behavior and shutdown correctness remain to be demonstrated. This RFC creates no binary release, provenance/signing mechanism, distribution owner or release-support obligation.

## Unresolved questions

- Finalize facade/result/cancellation types and the provisional provider/deadline/context-completion signatures in small shared-core reviews, preserving the ownership, ordering and admission contracts above.
- Which concrete target/provider split removes all forbidden runtime dependencies while preserving CLI/SSR and optional drawing behavior? Measure the resulting closure rather than assuming the split is sufficient.
- Validate the specified format-v1 Bzlmod import layout and export metadata in implementation reviews; the output shape and declared compatibility fields above are acceptance scope, not deferred design.
- Who reviews/owns the proposed profile gates, and do measured cold/warm costs fit the initial CI caps? Acceptance alone assigns no owner or new budget.
- Final RFC numbering is decided at acceptance; the current RFC 0002 filename/number is retained during review. Publication and any support-tier change require separate proposals.
