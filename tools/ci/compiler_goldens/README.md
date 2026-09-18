# Compiler emit goldens

Checked-in reference output of the Valdi TSX→native compiler (`compiler/companion`)
for the `valdi/testdata/resources/modules/**` corpus, used by
[`../compiler_golden_test.sh`](../compiler_golden_test.sh).

## What this guards

The companion embeds the TypeScript compiler as a library and drives its
AST/checker APIs, including undocumented internals (`EmitResolver`,
`ts.matchFiles`). A change to the pinned TypeScript version — or to a transform —
can therefore change emitted output **silently**: type-checking still passes and
the companion's unit specs still pass, yet generated `.d.ts` / C++ changes.

This golden diff over real module output is the safety net that makes such a
change **visible and reviewable** (notably, it gates the TypeScript 5.3.3 → 5.9
bump).

## What is captured

Per module, keyed by the config-independent logical path `<module>/<rest>`:

- **TypeScript declarations** — `.valdi_build/compile/typescript/output/<mod>/**/*.d.ts`
- **Generated C++** — `cpp/release/src/valdi_modules/<mod>/<mod>.{cpp,hpp}` (the NativeCompiler C++ emitter; release only — see below)
- **Generated native C** — the per-flavor `*_native.c` (the TSN-atom emitter path)
- **Compilation metadata** — `.valdi_build/compile/typescript/dumped_symbols/<mod>/compilation-metadata.json`
- **Platform bindings (release)** — ObjC `ios/release/src/**/*.{h,m}` (the module
  and its `<Mod>Types` sibling), Swift `ios/release/src/**/*.swift`, and Kotlin
  `android/release/src/*.kt`. These are declaration-driven single-file outputs
  (`single_file_codegen`), so the Android output is a plain `.kt`, not a
  `.srcjar` archive. The `binding_lang_{objc,swift,both}` trio shares one minimal
  bridgeable API and differs only in `ios_language`, so its goldens characterize
  the ObjC, Swift, and hybrid emit paths from identical input. The rest of the
  corpus exports nothing bridgeable and stays objc-only (empty stubs).

Each file is stored with a `.golden` suffix (e.g. `test.cpp.golden`) so no
language formatter (clang-format, eslint) matches and rewrites these verbatim
compiler outputs.

The captured content was verified deterministic and portable — free of absolute
paths, `bazel-out` references, timestamps, and arch/host strings, and
byte-identical between macOS and Linux CI.

**Not captured:** the `.valdimodule` bytecode blob (brittle binary), `.map.json`
source maps (embed absolute paths), and **web-transpiled JS**
(`web/release/assets/<mod>/**/*.js`). The web/Vue codegen is not reproducible
across hosts today (macOS and Linux emit different generated member references
for some `.vue` files), so it can't be a stable golden yet — tracked as a
follow-up. Likewise the **debug-flavor C++** (`cpp/debug`): the complex `test`
module's debug C++ diverges macOS vs Linux (while `cpp/release` is byte-identical
cross-host), so only the release flavor is pinned.

## Where it runs

Its own `Compiler Golden Diff` leg (Linux) in `.github/workflows/bzl-changes.yml`.
It builds the **source** companion (`--//bzl/valdi:use_prebuilt_companion=false`),
so the diff reflects companion source rather than a prebuilt binary (the two can
emit different C++ formatting).

## Updating

Goldens are **generated**, not hand-edited. When an emit change is intended,
regenerate and review the diff in your PR. Two ways:

```
# 1. Locally, from open_source/ (must be on the same platform CI uses — Linux):
tools/ci/compiler_golden_test.sh --update

# 2. From CI: when the leg fails on drift it uploads the freshly-generated set as
#    the `regenerated-compiler-goldens` artifact. Download and unzip it into
#    tools/ci/compiler_goldens/ — no local rebuild needed, and guaranteed to
#    match the CI host.
```

`--update` wipes and regenerates only the per-module subtrees, so this README
survives a regeneration.

> Portability note: content was verified free of absolute paths, `bazel-out`
> references, and timestamps, so goldens should be identical across host
> configs. The first Linux CI run validates this against the macOS-seeded set;
> if any file proves host-dependent, regenerate on Linux (or drop it from the
> captured surface in `compiler_golden_test.sh`).
