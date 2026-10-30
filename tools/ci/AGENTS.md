# CI scripts (tools/ci)

How CI works in this repo, and the rules for adding or changing a step. Each unit of CI work is a
script in this directory; the GitHub Actions workflows under `.github/workflows/` run them as jobs.

## The contract

1. **Every unit of CI work is a script in `tools/ci/*.sh`.** No inline `bazel build`/`bazel test`
   in a workflow: a job runs a script. That keeps every job reproducible outside CI.
2. **A step script is self-contained:** runnable from the repo root, in CI and locally, and it
   branches on `uname` itself where platform behavior differs (rather than the workflow deciding).
3. **Helpers are not steps.** Utility/sourced scripts (`setup_linux_env.sh`,
   `bazel_gcs_credential_helper.sh`) are not validation units and don't get their own job.

## Adding or changing a step

1. Write `tools/ci/<step>.sh`. Self-contained; `uname`-branching if platform-specific; safe to run
   locally.
2. Add a job that runs it in the appropriate `.github/workflows/*.yml`.
3. Never replace a script step with an inline `bazel` command: that makes the step impossible to
   run locally or share.

## Environment notes

- Linux jobs `source ./tools/ci/setup_linux_env.sh` first (disk reclaim, system deps, Bazel, Java).
  It also ensures a `bzl` wrapper exists, so scripts can call `bzl` uniformly.
- macOS jobs install Bazel via bazelisk. Some scripts call `bazel` directly and fall back to `bzl`
  when only the wrapper is present.

## Compiler emit golden test

`compiler_golden_test.sh` is a characterization test: it compiles the
`valdi/testdata/resources/modules/**` corpus and diffs the compiler's emitted
TypeScript declarations, generated C++, and compilation metadata against
checked-in goldens under `compiler_goldens/`. It exists to catch **silent**
changes in compiler (`compiler/companion`) output — most importantly from a
TypeScript dependency bump, which type-checks and unit specs do not cover.

- **Runs as its own Linux matrix leg** (`compiler-golden`) in `bzl-changes.yml`.
- **Always built from source companion.** The call site passes
  `GOLDEN_BUILD_FLAGS=--//bzl/valdi:use_prebuilt_companion=false`; otherwise the
  diff would reflect a prebuilt binary, not companion source. (The prebuilt and
  source companions can emit different C++ formatting — this bit us once.)
- **Regenerating goldens** when an emit change is intended:
  `tools/ci/compiler_golden_test.sh --update`. On a Linux/host mismatch, the CI
  leg uploads the freshly-generated set as the `regenerated-compiler-goldens`
  artifact — download it into `compiler_goldens/` instead of rebuilding locally.
- A regenerated golden in a PR triggers a **Sensitive File Notice** comment so
  reviewers confirm the emit change is intended. See `compiler_goldens/README.md`.

## Native implementation check

`native_impl_check.sh` is a static scan (no Bazel) that runs in the `api-surface` job of
`bzl-changes.yml`. Every attribute declared on a `@NativeTemplateElement` in
`NativeTemplateElements.d.ts` must be registered with the attributes binder on iOS
(`bind*Attribute:@"name"`) and on Android (`bind*Attribute("name"`), or by the shared C++
runtime binders. A missing registration fails the job, because otherwise the attribute silently
becomes a no-op on that platform.

- **Intentionally single-platform or runtime-handled attributes** go in
  `native_impl_check/allowlist.txt`, one per line with a reason. Stale entries fail the check.
- **Matching is by attribute name, not per view class.** Registering a name for one view class
  satisfies every element that declares it. It also can't see a registration that exists but
  does nothing.
- **When a binder refactor changes how names are registered**, update the extractor patterns in
  `native_impl_check/check_native_impl.py` and add a fixture case to its unit tests.

## Guardrails

- Reproduce CI's exact invocation when debugging; don't scope a run just to make it pass.
