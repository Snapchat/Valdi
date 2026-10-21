# RFC 0002: Linux SDK builds for headless Valdi services

- Status: Draft
- Author(s): @sp-ben-dodson
- Discussion: This pull request.
- Implementation: None yet.

## Summary

Provide reproducible Linux SDK builds for embedding Valdi in headless services and command-line hosts. Package the runtime, required native dependencies, headers, module resources and compatible build tools behind a documented consumer contract. Start with Linux x86_64 on glibc and an experimental support tier. This RFC covers SDK production and consumption; a graphical Linux host is a separate proposal.

## Motivation

Applications may want to reuse Valdi modules, native TypeScript execution and shared services on Linux without creating a window. Each consumer should not have to discover the runtime's transitive link requirements, copy generated headers or assemble a matching compiler/runtime combination.

Upstream already has a public [Linux runtime target](https://github.com/Snapchat/Valdi/blob/53b70efda8c520037ecba12a7264b777d9e3f4fd/valdi/BUILD.bazel), a [Linux application macro](https://github.com/Snapchat/Valdi/blob/53b70efda8c520037ecba12a7264b777d9e3f4fd/bzl/valdi/valdi_linux_application.bzl), and a [bootstrap](https://github.com/Snapchat/Valdi/blob/53b70efda8c520037ecba12a7264b777d9e3f4fd/valdi/src/valdi/linux/Bootstrap/ValdiLinuxMain.cpp) that evaluates a script and runs the main queue. These are starting points, not evidence of a qualified distributable SDK. Linux is currently [development only](https://github.com/Snapchat/Valdi/blob/53b70efda8c520037ecba12a7264b777d9e3f4fd/VERSIONING.md).

## Detailed design

**Build and artifact contract.** Add an opt-in SDK packaging target using upstream Bazel/Bzlmod and public, pinned dependencies. Reuse existing runtime/compiler targets rather than maintain a second engine build. Initially provide a native Linux x86_64/glibc build; agree on the minimum glibc, CPU instruction set and compiler/C++ standard library baseline before the first artifact. Cross-compilation, musl and additional architectures require their own qualification.

An SDK contains:

- The documented embedding headers, generated binding headers and complete native static-library closure, including TSN when native TypeScript modules require it. Export include paths, compile definitions, link order/options and required system libraries through a Bazel consumer target and a machine-readable manifest.
- The selected JavaScript engine and its dependencies. Qualify one explicit engine configuration first; do not silently substitute engines. Declare RTTI, exception, C++ ABI and position-independent-code settings.
- Required runtime module/resource bundles and matching Linux compiler tools, or immutable references to separately published matching tools. Distinguish tools that execute during a build from libraries that execute in the deployed service.
- A manifest recording the SDK version, upstream source revision, compiler/module-format compatibility, target baseline, engine configuration, dependency versions, file checksums and third-party notices. Include a minimal external-consumer example.

Treat headers, libraries, generated modules and tools as a matched set. Do not promise a stable binary ABI for internal C++ interfaces or mix arbitrary compiler/runtime releases. Document the supported embedding surface and rebuild requirements; language-specific wrappers and a new stable C ABI are outside this initial proposal.

**Headless behavior and capabilities.** The baseline starts a runtime, loads a module, invokes a service entry point and exchanges data through documented native bindings. Specify queue/thread ownership, asynchronous completion, error propagation, cancellation and deterministic shutdown. Keep process supervision and signal handling in the host. Teardown must stop accepting work, cancel or drain bounded pending work and release runtime resources without stale callbacks.

No display server, window, GPU context or desktop session is required. Keep SDL, GLFW and window-host dependencies outside the headless consumer path. The current Linux target includes SnapDrawing: audit and declare any retained raster/font dependencies rather than claim a minimal dependency closure without evidence. Offscreen rendering or layout can be separately named, tested capabilities; they are not prerequisites for the service baseline.

Publish a capability matrix. Begin with module loading, native bindings, native TypeScript execution and queue/lifecycle behavior. Add workers, serialization, HTTP, storage or other modules only with their native adapters, resources and integration tests. An absent capability must fail explicitly rather than appear to work through an empty stub. Debugger and hot-reload services are disabled by default in the service example; hosts opt in explicitly.

**Delivery stages.** First land the source build/packaging seam, manifest and external-consumer fixture. Then add a release job that builds on a pinned Linux environment, tests the staged archive in a clean consumer environment, and publishes immutable, checksummed artifacts with provenance through the agreed release verification mechanism. Release naming and verification policy must be agreed before publication. Expand capabilities and architectures in separately tested changes. This RFC itself publishes no SDK binaries.

## Automated test strategy

These are proposed additions; existing source tests do not by themselves qualify an SDK archive.

| Coverage | Proposed CI job and regression caught |
|---|---|
| SDK build and completeness | Linux SDK build job builds from public dependencies and stages an archive. Compile/link an external fixture against only that SDK and declared toolchain/system prerequisites; fail on missing transitive headers, generated bindings, symbols or resources, and leaked checkout paths. |
| Headless service integration | Clean Linux consumer job, without display sockets or GPU devices, loads a compiled module, calls native bindings, exercises native TypeScript execution and asynchronous completion, and checks results. Timeouts catch hangs; no display initialization may be required. |
| Lifecycle and failures | Linux runtime tests cover missing/corrupt modules, unsupported capabilities, entry-point errors, cancellation, repeated create/destroy and shutdown with queued work. Bounded-time assertions and sanitizer runs catch deadlocks, leaks and callbacks after teardown. |
| Artifact compatibility | Linux baseline/consumer matrix checks ELF architecture, declared glibc/C++ symbol requirements, engine identity, manifest/checksums and compiler/runtime compatibility. Negative fixtures reject mismatched tools/modules where detectable; unsupported combinations remain explicitly unsupported. |
| Optional modules | Separate Linux integration jobs test each advertised capability, using local HTTP fixtures and temporary storage where relevant. Missing adapters fail the capability's qualification. |
| Release and platform isolation | Release job verifies the exact staged artifact and provenance before publication. Existing iOS, Android, macOS and web jobs guard shared build/compiler changes; the SDK target remains opt-in. |

Build reproducibility means pinned inputs and a repeatable build procedure initially. Compare independent builds and document nondeterministic outputs before claiming byte-for-byte reproducibility. Record artifact size, startup and steady-state resource use without setting an unsupported performance promise. Human review is still needed for notices and the documented compatibility/support policy.

## Compatibility

Additive; existing applications and Linux application targets require no migration. New SDK rules and documented embedding APIs follow the public API policy. Packaging an experimental SDK does not promote Linux to a supported production platform. Any support-tier change needs an explicit policy update and agreed release-blocking CI. Other platforms retain their current behavior.

## Alternatives considered

- **Build the entire source tree in every consumer:** remains available, but repeats integration work and does not qualify a reusable artifact.
- **Distribute only compiler/CLI binaries:** helps module compilation but leaves service embedding and native dependencies unresolved.
- **Bundle a graphical host:** adds unrelated platform dependencies; the headless SDK should be independently consumable.
- **Introduce a separate runtime fork or stable cross-language ABI immediately:** increases maintenance and compatibility commitments before the existing embedding path is qualified.

## Drawbacks and risks

SDK packaging creates a release-maintenance obligation. Native dependency size, licensing, C++ ABI/toolchain coupling and compiler/module compatibility need ongoing validation. Existing targets may carry more graphics dependencies than a service requires. Shipping libraries does not establish that every Valdi feature works on Linux, and checksums alone do not authenticate an artifact.

## Unresolved questions

- Who owns SDK releases, CI and the experimental support policy?
- Which glibc/toolchain baseline and initial JavaScript engine should be qualified?
- Which embedding APIs and initial module capabilities should be documented as consumable?
- What archive layout, matching compiler distribution, release version naming and provenance verification fit upstream's release process?
- Which graphics dependencies can safely be removed from the service profile, and when should ARM64 or additional libc variants be added?
