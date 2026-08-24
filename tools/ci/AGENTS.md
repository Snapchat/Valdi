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

## Guardrails

- Reproduce CI's exact invocation when debugging; don't scope a run just to make it pass.
