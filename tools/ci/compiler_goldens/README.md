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

Web JS is captured from `web/debug/assets/<mod>`, the declared web outputs,
including the Vue template render code (`*.vue.generated.js`).

**Not captured:** the `.valdimodule` bytecode blob (brittle binary), `.map.json`
source maps (embed absolute paths), and files the build rule never declares:
web JS under `web/release` and debug-flavor C++ (`cpp/debug`). Undeclared files show up or not, with
whatever content, depending on how the compile action happened to run.

## Where it runs

Its own `Compiler Golden Diff` leg (Linux) in `.github/workflows/bzl-changes.yml`.
It builds the **source** companion (`--//bzl/valdi:use_prebuilt_companion=false`),
so the `.d.ts` / native-C / C++ emit reflects companion source rather than a
prebuilt binary (the two can emit different C++ formatting).

### Compiler provenance (matters for the platform bindings)

The ObjC/Swift/Kotlin platform bindings are emitted by the Valdi **compiler**
binary, not the companion. The harness forces the *companion* from source, but
the *compiler* follows the build's default: from source here, but potentially a
**prebuilt binary** in environments that ship one. A prebuilt that lags the
compiler source will emit stale bindings, so the binding goldens track whatever
compiler the build actually uses. When a landed compiler change affects binding
emit, the prebuilt must be rebuilt to include it **before** these goldens are
regenerated — otherwise the checked-in goldens characterize an emit path that no
longer exists in source, and a from-source run (like this leg) will diff against
them.

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
