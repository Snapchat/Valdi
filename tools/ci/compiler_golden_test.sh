#!/usr/bin/env bash
#
# Compiler emit golden-diff test.
#
# Characterizes the Valdi TSX->native compiler (compiler/companion) by compiling
# a fixed corpus of testdata modules and diffing the emitted TypeScript
# declarations, generated C++, native C, platform bindings (ObjC/Kotlin), and
# compilation metadata against checked-in goldens under tools/ci/compiler_goldens/.
#
# Why this exists: the companion embeds the TypeScript compiler as a library and
# drives its AST/checker APIs (including undocumented internals such as
# EmitResolver and ts.matchFiles). A TypeScript dependency bump can therefore
# change emitted output *silently* -- neither type-checking nor the companion's
# unit specs would catch it. This diff over real module output is the safety net
# that gates such a bump.
#
# Usage (run from open_source/):
#   tools/ci/compiler_golden_test.sh              # verify against goldens (CI mode)
#   tools/ci/compiler_golden_test.sh --update     # regenerate goldens after an
#                                                  # intended emit change
#   tools/ci/compiler_golden_test.sh --no-build   # skip the build (reuse outputs)
#
# Environment:
#   GOLDEN_BUILD_FLAGS   Extra flags appended to `bzl build` (space-separated).
#                        Used by CI to force the source-built companion
#                        (--//bzl/valdi:use_prebuilt_companion=false) so the test
#                        exercises companion source, not a prebuilt binary.
#   GOLDEN_REGEN_DIR     If set, the freshly-captured emit surface is written
#                        here (perms-normalized, ready to commit) in verify mode.
#                        CI uploads this as an artifact on drift so a developer on
#                        any host can download the correct goldens without a
#                        local rebuild.
#
set -euo pipefail

# Run from the open_source/ workspace root regardless of caller CWD.
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

MODE="verify"
DO_BUILD=1
for arg in "$@"; do
  case "$arg" in
    --update) MODE="update" ;;
    --no-build) DO_BUILD=0 ;;
    *) echo "Unknown argument: $arg" >&2; exit 2 ;;
  esac
done

# The corpus: self-contained testdata modules (deps only on in-repo
# valdi_core / valdi_tsx / worker), already compiled elsewhere in CI.
MODULES=(test test2 local remote remote_assets startup_benchmark test_async_strict binding_lang_objc binding_lang_swift binding_lang_both)
MODULE_PKG="valdi/testdata/resources/modules"
GOLDEN_ROOT="tools/ci/compiler_goldens"

if [[ "$DO_BUILD" == "1" ]]; then
  targets=()
  for m in "${MODULES[@]}"; do targets+=("//${MODULE_PKG}/${m}"); done
  # Every Bazel ValdiCompile action already runs the companion with
  # --disable-disk-cache, so this is a clean, reproducible compile.
  #
  # --remote_download_all forces the emit outputs to be materialized on local
  # disk. The capture below reads the generated .d.ts / *_native.c / .cpp /
  # metadata straight from bazel-bin, but CI runs Bazel with
  # Build-without-the-Bytes, so on a warm remote cache those cache-hit
  # intermediates are never downloaded and the capture finds nothing. A
  # command-line flag overrides that download policy.
  # shellcheck disable=SC2086 # GOLDEN_BUILD_FLAGS is intentionally word-split.
  bzl build --remote_download_all ${GOLDEN_BUILD_FLAGS:-} "${targets[@]}"
fi

# Pass GOLDEN_BUILD_FLAGS to `info` too: a build setting can transition the
# configuration, moving outputs to a different bazel-out dir. Without the flags
# here, `info` could report the default dir and we'd capture stale outputs.
# shellcheck disable=SC2086 # GOLDEN_BUILD_FLAGS is intentionally word-split.
BB="$(bzl info ${GOLDEN_BUILD_FLAGS:-} bazel-bin)"
SRC_ROOT="${BB}/${MODULE_PKG}"

STAGING="$(mktemp -d)"
DIFF_BRIEF="$(mktemp)"
trap 'rm -rf "$STAGING" "$DIFF_BRIEF"' EXIT

# Copy the golden surface for one module into STAGING, keyed by the
# config-independent logical path <module>/<rest> (strips the
# bazel-out/<config>/bin/ prefix so goldens are portable across host configs,
# e.g. darwin_arm64-fastbuild locally vs k8-fastbuild on Linux CI).
#
# Golden surface (verified deterministic and portable across build configs --
# free of absolute paths, timestamps, and arch/host strings, and byte-identical
# between macOS and Linux CI):
#   - TypeScript declaration files (.d.ts)
#   - generated C++ (cpp/release; the NativeCompiler C++ emitter). The debug
#     flavor is NOT captured: the debug C++ emit is not host-reproducible (the
#     complex `test` module's cpp/debug diverges macOS vs Linux while cpp/release
#     is byte-identical), so it can't be a stable golden.
#   - generated native C (the per-flavor *_native.c; the TSN-atom emitter)
#   - compilation metadata JSON
#   - generated platform bindings, release flavor: ObjC (.h/.m under
#     ios/release/src, both the module and its <Mod>Types sibling) and Kotlin
#     (.kt under android/release/src). These are declaration-driven single-file
#     outputs (single_file_codegen is always on), so despite the build rule's
#     "srcjar" field name the Android output is a plain .kt, not an archive. We
#     pin the release flavor for the bindings. Swift and the objc/swift/hybrid
#     split are exercised by the purpose-built binding_lang_{objc,swift,both}
#     trio: they share one minimal bridgeable API (an @ExportProxy interface +
#     @ExportEnum, so the Types module is non-empty) and differ only in
#     ios_language, so their goldens characterize each emit path from identical
#     input. The rest of the corpus exports nothing bridgeable (empty ObjC
#     stubs; swift codegen fails on them outright), so those modules stay
#     objc-only.
# Excluded:
#   - the .valdimodule bytecode blob (a brittle binary)
#   - .map.json source maps (can embed absolute paths)
#   - web-transpiled JavaScript (web/release/assets/<mod>/**/*.js): the web/Vue
#     codegen is NOT reproducible across hosts today -- macOS and Linux emit
#     different generated member references for some .vue files -- so it cannot
#     be a stable golden yet. Tracked as a follow-up (investigate the compiler
#     non-determinism, then add it back).
collect_module() {
  local mod="$1"
  local base="${SRC_ROOT}/${mod}"
  local f rel
  while IFS= read -r f; do
    [[ -z "$f" ]] && continue
    rel="${f#"${SRC_ROOT}"/}"
    mkdir -p "${STAGING}/$(dirname "$rel")"
    # Store with a .golden suffix so no language formatter (clang-format,
    # eslint, ...) matches and rewrites these verbatim compiler outputs.
    cp "$f" "${STAGING}/${rel}.golden"
  done < <(
    find "${base}/.valdi_build/compile/typescript/output/${mod}" -name '*.d.ts' 2>/dev/null || true
    find "${base}/cpp/release" \( -name '*.cpp' -o -name '*.hpp' \) 2>/dev/null || true
    find "${base}" -path '*/native/*' -name '*_native.c' 2>/dev/null || true
    find "${base}/.valdi_build/compile/typescript/dumped_symbols/${mod}" -name 'compilation-metadata.json' 2>/dev/null || true
    find "${base}/ios/release/src" \( -name '*.h' -o -name '*.m' -o -name '*.swift' \) 2>/dev/null || true
    find "${base}/android/release/src" -name '*.kt' 2>/dev/null || true
  )
}

# Publish the captured emit surface (STAGING) into a destination golden tree:
# wipe only the per-module subtrees (so hand-written root docs such as README.md
# survive), copy, and normalize Bazel's read-only 0555 outputs to plain perms.
publish_capture() {
  local dest="$1"
  mkdir -p "$dest"
  local m
  for m in "${MODULES[@]}"; do rm -rf "${dest:?}/${m}"; done
  cp -R "${STAGING}/." "${dest}/"
  find "$dest" -type d -exec chmod 755 {} +
  find "$dest" -type f -exec chmod 644 {} +
}

for m in "${MODULES[@]}"; do collect_module "$m"; done

captured_count="$(find "$STAGING" -type f | wc -l | tr -d ' ')"
if [[ "$captured_count" == "0" ]]; then
  echo "ERROR: captured 0 emit artifacts -- did the build produce outputs?" >&2
  exit 1
fi

if [[ "$MODE" == "update" ]]; then
  publish_capture "$GOLDEN_ROOT"
  echo "Updated goldens under ${GOLDEN_ROOT} (${captured_count} files)."
  exit 0
fi

# verify mode
if [[ ! -d "$GOLDEN_ROOT" ]]; then
  echo "ERROR: no goldens at ${GOLDEN_ROOT}. Seed with: tools/ci/compiler_golden_test.sh --update" >&2
  exit 1
fi

# Stage the would-be-new goldens for CI to upload on drift (see GOLDEN_REGEN_DIR).
if [[ -n "${GOLDEN_REGEN_DIR:-}" ]]; then
  publish_capture "$GOLDEN_REGEN_DIR"
fi

# Exclude hand-written root docs (README.md) -- they live in the golden root but
# are not part of the captured emit surface.
DIFF_EXCLUDE=(-x 'README.md')
if diff -rqN "${DIFF_EXCLUDE[@]}" "$GOLDEN_ROOT" "$STAGING" > "$DIFF_BRIEF"; then
  echo "Compiler emit goldens match (${captured_count} files across ${#MODULES[@]} modules)."
  exit 0
fi

echo "::error::Compiler emit differs from checked-in goldens -- the companion compiler produced different output." >&2
echo "If this change is intended (e.g. a TypeScript version bump), regenerate and commit the goldens:" >&2
echo "    tools/ci/compiler_golden_test.sh --update      # regenerate locally, or" >&2
echo "    download the 'regenerated-compiler-goldens' CI artifact into tools/ci/compiler_goldens/" >&2
echo "then review the emit diff in your PR." >&2
echo "----- files differing (golden vs actual) -----" >&2
cat "$DIFF_BRIEF" >&2
echo "----- unified diff (truncated to 400 lines) -----" >&2
diff -ruN "${DIFF_EXCLUDE[@]}" "$GOLDEN_ROOT" "$STAGING" | head -n 400 >&2
exit 1
