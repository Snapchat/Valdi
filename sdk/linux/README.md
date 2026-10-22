# Build the experimental Linux headless SDK

This opt-in source build is the first implementation stage of RFC 0002. It
exports the existing Linux runtime and TSN static closure, generated headers,
runtime module resources, a link/compatibility manifest, dependency notices and
an external C++ service fixture. Existing applications do not change.

Linux remains experimental. This is an embedding qualification fixture, not a
stable C++ ABI or a release/support policy. No release publication is included.

## Build and consume

Use a clean committed checkout on native Linux x86_64 with glibc, Bazel 8.2.1,
Python 3.11 or newer, and the repository's documented build prerequisites. The
profile selects production optimization, QuickJS bytecode and LLVM 19.1.7 with
libc++, compiler-rt, libunwind, LLD, C++20, RTTI, and disabled exceptions in the embedding API. Individual
third-party libraries retain their own compilation options. PIC availability is
recorded per archive; a shared-library embedding contract is not qualified.
The consumer also inherits the source build's libc++ compatibility definition
`_LIBCPP_ENABLE_CXX20_REMOVED_TYPE_TRAITS` through the exported target/manifest.

```sh
python3 sdk/linux/package.py build --output /tmp/valdi-linux-sdk
python3 sdk/linux/package.py verify /tmp/valdi-linux-sdk --engine quickjs
python3 sdk/linux/package.py archive /tmp/valdi-linux-sdk --output /tmp/valdi-linux-sdk.tar.gz
```

The builder invokes `//sdk/linux:export` with the fixed flags in `profile.json`.
`--output-base` can reuse an owned Bazel cache; Bazel validates the actual action
inputs. An SDK is never made by copying an output base. A non-upstream build must
pass its public `--source-repository` URL so the exact source revision resolves.
If GCC development libraries are privately extracted instead of system-installed,
the builder accepts the scoped Rust prerequisite option
`--bazel-option=--@rules_rust//rust/settings:extra_exec_rustc_flag=-Lnative=/path/to/gcc-libraries`.
Compiler/runtime selection flags cannot be overridden through this escape hatch.

The SDK's `MODULE.bazel` and `BUILD.bazel` expose `@valdi_linux_sdk//:runtime`.
The separate `:fixture_module` is a complete host link target containing the
runtime plus the example's generated bindings, native TypeScript code and static
resources. The native runtime includes `valdi_core`
resources; it does not require the fixture. The manifest preserves transitive
include paths, definitions, archive order, alwayslink policy and linker options.
Unsupported dynamic/object-only inputs or path-bearing/stateful linker flags fail
staging. The initial profile permits only `-l...` system-library and `-pthread`
options. The raw manifest retains their positions; the Bazel import places these
system dependencies after its archives. The verifier consumes one aggregate link
sequence directly, preserving first archive positions inside a rescan group.
Repeated archives are coalesced with `alwayslink` enabled if any input requires it;
the unmodified export is retained in `provenance/native-export.json`.

Copy the staged SDK to a fresh Linux consumer environment with no Valdi checkout,
display sockets or GPU devices. Supply the matched LLVM executable and library
directory explicitly:

```sh
python3 /tmp/valdi-linux-sdk/consumer/verify.py \
  --sdk /tmp/valdi-linux-sdk --cxx /opt/llvm-19.1.7/bin/clang++ \
  --toolchain-lib /opt/llvm-19.1.7/lib \
  --unavailable-source-path /producer/Valdi --unavailable-source-path /producer/bazel-output \
  --source-revision FULL_SDK_SOURCE_COMMIT --report /tmp/consumer-report.json
```

The verifier compiles exactly one external host source against the staged
headers/libraries. No engine sources or source-tree Bazel rules are available to
that command. The SDK also includes an illustrative Bazel consumer BUILD; its
toolchain registration belongs to the consumer. Qualification uses the explicit
compiler invocation and records the emitted ELF dependency/version requirements.

## Lifecycle and binding fixture

The fixture compiles `sdk_service` in `native` mode through the source-built Valdi
compiler, compiler companion and bytecode toolbox. Its `@ExportModel` request and
`@ExportFunction` trampolines are generated C++ bindings, not a handwritten
JavaScript equivalent. A C++ caller resolves and invokes the service; a native
callback queues completion onto the host's main queue. The host pumps that queue
with a deadline and owns admission, cancellation and shutdown.

The test repeats runtime creation and destruction three times in each scenario:
successful result, missing module, corrupt module, and throwing module entry.
Loader/archive preflight distinguishes absent bytes from malformed bytes. The
current JS module loader can normalize both failures to a named missing-entry
error; the fixture checks that runtime error as well. Structured load-error codes
and preservation of underlying loader causes remain unqualified.
Success also cancels delayed host work, releases generated native objects before
runtime teardown, and rejects work after queue disposal. Debugger and hot reload
are disabled. A subprocess timeout bounds startup, resolution and teardown hangs.
This bounded host example is not a general cancellation API for arbitrary JS work.

## Compatibility and omissions

Treat headers, static libraries, module resources, generated bindings and source
tools as an exact revision/profile set. Rebuild consumers on any change. The
manifest records tool file checksums and the immutable source-toolchain reference;
the copied compiler/companion payload is **not yet qualified for execution outside
Bazel**, including its runfiles and dynamic-library closure. Compiling arbitrary
new TypeScript from only the staged SDK remains a follow-up gate. Use the matched
source build for module compilation in this stage.

The initial native builder's libc is recorded. There is no qualified minimum
glibc, older-distribution compatibility, musl, ARM64, cross-compilation or stable
C++ ABI guarantee. Native outputs have not been proven byte-for-byte reproducible
across independent builders. Packaging fixed input bytes is deterministic.

The existing `valdi_linux` target retains SnapDrawing, Skia, font and raster
dependencies. They are exported and their notices collected; their presence does
not qualify raster, GPU or layout behavior. SDL, GLFW and window hosts are outside
this contract. Workers, HTTP, storage, ClientSQL, accessibility and IME are not
advertised service capabilities. Consumers must check `profile.capabilities` and
reject requirements outside that list before runtime initialization.

Notices are collected from the source and linked external repositories, and
dependency resolution inputs are retained in `provenance/`. Missing discovered
notices are reported explicitly. Human license and release provenance review is
still required; checksums detect payload changes and do not authenticate releases.

## Delivery stages

1. Review this source-build/export seam and qualify the external binding and
   lifecycle fixture on one Linux builder.
2. Package a fully runnable matched compiler/companion toolchain, add arbitrary
   external module compilation, baseline/ABI matrix and sanitizer lifecycle jobs.
3. Agree release ownership, minimum libc/CPU baseline, version naming and artifact
   verification policy. Only then add immutable artifact publication.

Graphical hosts remain separate downstream consumers of the declared closure.
Application protocols, layout-to-compositor bridges and process supervision stay
outside the reusable SDK.
