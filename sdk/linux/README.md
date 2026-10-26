# Historical Linux archive and consumer experiment

This fork-only prototype predates the revised [RFC 0002](../../rfcs/0002-linux-headless-sdk.md). It is retained as test evidence and reusable packaging/consumer research, **not an implementation of the revised stage-1 contract**. The custom archive/manifest, direct non-Bazel link verifier and TSN/drawing runtime closure are superseded as the proposed upstream design. Do not use this directory to infer a stable SDK, a minimal service profile or production support.

The revised direction is a source-built **Bazel-only** Linux variant of `valdi_exported_library` and `valdi export linux`, based on `valdi_cli_application`/`cli_runner`. It requires a QuickJS-only, TSN-free, drawing-free standalone core and shared embedding lifecycle changes. Those are implementation gaps, not properties this prototype gains by changing a profile label. Compatibility metadata belongs with that existing export pipeline, not a second independently versioned SDK format.

Linux remains **Development only, not supported for production deployment** under [VERSIONING.md](../../VERSIONING.md). A service deployed to users counts as shipping. Compiler/runtime prebuilt publication, a new support tier and any stable C++/C ABI are outside the revised RFC acceptance scope.

## Exact historical evidence

The qualified experiment is implementation commit `b37c808620d64a67a44f43823a824ea02a007f98`, on original RFC commit `304c1b95eaa960add5ea6044298e5c3d7559b160` and public native source `53b70efda8c520037ecba12a7264b777d9e3f4fd`. Later rebases/documentation revisions do **not** requalify a new native source cohort. The original implementation, sealed archives and manifests remain identified by those exact commits/checksums.

- Unsigned local archive SHA256: `5783086d717946215122e1ee43477903b07c63debfa15a5eec81a25d9c7ec634`.
- Manifest SHA256: `78cb190d60492e0052c9a21faefb5be767579ed4abe2f0f89e8e3c8d1a585c64`; 21,914 inventoried files.
- Producer: native Linux x86_64, glibc 2.44, Bazel 8.2.1, LLVM 19.1.7, C++20/libc++, compiler-rt/libunwind/LLD, RTTI and disabled embedding exceptions. Third-party options vary. Both engine and bytecode flags select QuickJS.
- Module generation used the matched source compiler, companion and toolbox. The service fixture uses TSN-native output and generated `@ExportModel`/`@ExportFunction` C++ bindings. It does not qualify the new no-TSN profile.
- An external host compiled against staged headers/archives with the producer checkout/output base inaccessible, display environment/sockets absent and no GPU devices. Success, missing module, corrupt module and throwing entry each passed three runtime cycles (12 total); no engine sources were compiled by the external host command.
- Twelve packaging/compatibility regressions passed; the Linux `//sdk/linux:package_test` passed. These check packaging mechanics, not the revised source-consumer/lifecycle acceptance gates.

The historical `valdi_linux`/TSN closure retains SnapDrawing, Skia and font/raster dependencies. Their retention was recorded honestly and never qualified rendering, layout or GPU behavior. It fails the revised dependency contract. The illustrative generated Bazel import was not executed; the evidence is an explicit compiler/link invocation, not a clean Bzlmod consumer qualification.

## What the experiment exercised

The custom exporter stages transitive headers, archives, resources, link metadata, exact source/profile/tool identities, dependency inputs and discovered notices. Its verifier preserves link order and `alwayslink`, coalesces duplicate archives, and rejects unsupported dynamic/object-only inputs and unsupported link-option semantics. Fixed input bytes package deterministically; independent native builds were not proven byte-for-byte reproducible.

The service fixture invokes generated native bindings, queues native completion onto a host-pumped main queue, cancels delayed host work and releases native objects before runtime teardown. A timeout bounds hangs. This is a bounded host example; it does not establish a general queue subscription API, embedded idle keepalive, admission/completion barrier, JS-exit ownership, cancellation for arbitrary JS work or ASan lifecycle qualification.

Missing and corrupt bytes have distinct preflight checks, but the current loader can normalize runtime failures to a named missing-entry error. Structured load-error codes and underlying cause preservation remain unqualified. Workers, HTTP, storage, ClientSQL, accessibility and IME are not advertised by this fixture.

Copied compiler/companion/toolbox files have exact source/checksum identities, but their standalone runfiles/dynamic closure is **unqualified**. New arbitrary TypeScript compilation must use the matched source build. This does not justify publishing compiler prebuilts; publication needs a separate RFC with an owner, provenance plan and security review.

## Historical reproduction only

Use the exact `b37c8086` source in an isolated Linux checkout to reproduce the recorded cohort, with its documented prerequisites and matching toolchain. These commands describe the old experiment; they are not the new `valdi export linux` interface and were not rerun as native qualification on the rebased documentation branch:

```sh
python3 sdk/linux/package.py build --output /tmp/valdi-linux-sdk
python3 sdk/linux/package.py verify /tmp/valdi-linux-sdk --engine quickjs
python3 sdk/linux/package.py archive /tmp/valdi-linux-sdk --output /tmp/valdi-linux-sdk.tar.gz
```

`package.py` invokes `//sdk/linux:export` with the fixed historical `profile.json`. An owned output base may be supplied for exact-input cache reuse; copying an output base is not the exporter. A fork build requires a resolving public source-repository identity. The scoped Rust/GCC prerequisite option remains available; it cannot override compiler/runtime selection flags. There is no qualified older-glibc, musl, arm64, cross-compile, shared-library or arbitrary libstdc++/libc++ interoperability contract.

The old SDK exposes `@valdi_linux_sdk//:runtime` and a fixture link target, with ordered closure/alwayslink metadata. Those names and the independent manifest schema are prototype interfaces. For the old explicit consumer check, supply matching LLVM executable/library paths and the exact source revision:

```sh
python3 /tmp/valdi-linux-sdk/consumer/verify.py \
  --sdk /tmp/valdi-linux-sdk --cxx /opt/llvm-19.1.7/bin/clang++ \
  --toolchain-lib /opt/llvm-19.1.7/lib \
  --unavailable-source-path /producer/Valdi --unavailable-source-path /producer/bazel-output \
  --source-revision b37c808620d64a67a44f43823a824ea02a007f98 \
  --report /tmp/consumer-report.json
```

Notices were collected with explicit discovery gaps (`asio` and generated Skia configuration); human license/provenance review is pending. Checksums identify bytes, not authenticated releases. No release assets, signing or publication workflow were produced.

## Sequenced work for the revised RFC

No revised runtime/export implementation is claimed by this draft. Follow the RFC review before beginning these bounded slices:

1. **Shared embedding lifecycle.** Define the small experimental create/load/call/shutdown facade, host-owned exit policy, supported cancelable queue subscriptions, weak callbacks, admission rejection and a shutdown completion barrier. Gate repeated queued-work/cancellation/exit races under ASan. Preserve existing CLI/SSR behavior. The window host consumes these common changes; it need not wait for the complete export pipeline.
2. **Minimal source runtime.** Split the optional SnapDrawing provider and the QuickJS-to-TSN coupling. Prove absence of Hermes/JSCore, TSN, Skia, ICU and rendering/font dependencies in the service runtime/export using dependency/action graphs and linked symbols. Measure before/after archive/link-closure sizes. Do not relabel the b37 closure as minimal.
3. **Existing export/CLI extension.** Add the Linux `valdi_exported_library` variant and `valdi export linux`, preserving CcInfo/alwayslink semantics and carrying compatibility metadata with that output. Reuse useful closure/negative-test lessons without retaining a parallel SDK format. Build tools and modules from the same declared source/toolchain profile.
4. **Clean Bazel qualification.** Extend `test_exported_lib.sh`, registry/external-build-graph scripts and the proposed `test_linux_embedding.sh`. Compile a fresh QuickJS module plus generated native bindings without TSN; test clean Bzlmod consumption without producer paths/display/GPU, failures/defaults and focused PR-triggered ASan lifecycle coverage. Qualify Ubuntu 22.04/24.04 separately; record observed libc/C++ requirements and cold/warm CI costs.

Unsupported adapters must return explicit unsupported results in the new embedding facade; existing CLI/SSR no-op behavior is preserved unless separately reviewed. Debugger/hot reload default off in every build flavor, and host environment exposure is explicit allowlist/opt-in. Exact facade/provider shapes and CI ownership/budget remain review decisions. Do not start a new engine cohort, release pipeline or publication stage merely to update these historical docs.

Rendering, window/input integration, consumer protocols and process supervision stay outside this service contract. Binary publication, portable compiler prebuilts, production support and other deferred goals require separate proposals; they are not the next automatic delivery stage of this experiment.
