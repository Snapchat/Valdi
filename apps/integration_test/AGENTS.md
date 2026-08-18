# Integration Test App Guide

This directory contains Valdi's cross-platform snapshot integration test harness. It renders a curated set of Valdi elements and attributes, captures screenshots and node output on each platform, and compares results across platforms or repos.

## What Is Here

- `src/valdi/integration_test_app`: the Valdi app under test.
  - `IntegrationTestCases.tsx` defines the cases.
  - `IntegrationTestApp.tsx` renders one case at a time.
  - `IntegrationTestRunner.ts` captures snapshots, observations, progress, and result JSON.
  - `web/IntegrationTestHost.ts` provides the web host implementation for screenshots, synthetic input, and file writes.
- `src/valdi/integration_test_cli`: the CLI for running captures, comparing outputs, exporting snapshots, and self-testing the comparison logic.
- `src/ios`, `src/android`, and `src/cpp`: native host/module support used by the app and CLI.
- Top-level targets:
  - `//apps/integration_test:integration_test`
  - `//apps/integration_test:integration_test_cli`
  - generated platform app targets such as `integration_test_ios`, `integration_test_android`, `integration_test_macos`
  - web package target `//apps/integration_test:integration_test_web_npm`

## Common Commands

Build the CLI first:

```bash
bazel build //apps/integration_test:integration_test_cli
```

For full saved comparisons or repeated `compare` runs, build/run the CLI with optimizations enabled. The comparison path can be much slower in non-optimized builds, especially when image conversion or resizing goes through SnapDrawing/Skia:

```bash
bazel run -c opt //apps/integration_test:integration_test_cli -- compare \
  --before /private/tmp/valdi-integration-ios.json \
  --after /private/tmp/valdi-integration-web.json \
  --output-dir /private/tmp/valdi-integration-compare
```

Print usage:

```bash
bazel-bin/apps/integration_test/integration_test_cli help
```

Run a web capture:

```bash
bazel-bin/apps/integration_test/integration_test_cli run \
  --platform web \
  --output /private/tmp/valdi-integration-web.json \
  --timeout-ms 240000
```

Run an iOS simulator capture:

```bash
bazel-bin/apps/integration_test/integration_test_cli run \
  --platform ios \
  --device-id booted \
  --output /private/tmp/valdi-integration-ios.json \
  --timeout-ms 240000
```

Compare two result JSON files:

```bash
bazel-bin/apps/integration_test/integration_test_cli compare \
  --before /private/tmp/valdi-integration-ios.json \
  --after /private/tmp/valdi-integration-web.json \
  --output-dir /private/tmp/valdi-integration-compare
```

The compare command writes:

- `index.html`: interactive report
- `summary.json`: machine-readable summary
- `summary.md`: concise Markdown summary
- `before/`, `after/`, `diffs/`: decoded and generated PNGs

Run both sides and compare in one command:

```bash
bazel-bin/apps/integration_test/integration_test_cli full-comparison \
  --before-repo /path/to/repoA \
  --after-repo /path/to/repoB \
  --before-platform ios \
  --after-platform web \
  --output-dir /private/tmp/valdi-integration-full
```

Export snapshots from one result JSON into PNGs plus a contact sheet:

```bash
bazel-bin/apps/integration_test/integration_test_cli export-snapshots \
  --result /private/tmp/valdi-integration-web.json \
  --output-dir /private/tmp/valdi-integration-web-snapshots
```

Run the CLI comparison self-test:

```bash
bazel-bin/apps/integration_test/integration_test_cli self-test
```

## Web Harness Notes

The web run path builds `//apps/integration_test:integration_test_web_npm` with web enabled, creates a temporary webpack harness under `/tmp`, serves it locally, and launches a Chrome-compatible browser in headless mode.

If browser discovery fails, set `CHROME_BIN` to Chrome, Chromium, Chrome for Testing, or `chrome-headless-shell`.

The web harness logs progress lines like:

```text
[web progress] index=12 phase=snapshotting case=view-background-color captured=12
```

Use those progress lines to locate renderer hangs. Avoid changing test cases just to hide a timeout; renderer bugs should usually be fixed in the renderer.

## Native Run Notes

Native `run` uses `valdi install` internally. Useful flags:

- `--device-id`: simulator, emulator, or device id. Use `booted` for the current iOS simulator.
- `--ios-device-build`: build for a physical iOS device.
- `--bazel-args`: pass extra Bazel args through to `valdi install`.
- `--valdi-bin`: use a specific `valdi` executable.

The app writes result JSON from inside the platform host. The CLI waits for completion, copies the result out, and terminates the app.

## Comparison Expectations

When validating a refactor, compare both summary metrics and generated diff PNGs if possible. A good smoke check is:

- same `caseCount`
- same `changedCaseCount`
- same per-case `diffPercent`, `changedPixels`, `totalPixels`, and `dimensionMismatch`
- same diff PNG hashes when comparing an implementation rewrite against a known baseline

Use `--pixel-threshold` for channel tolerance and `--fail-above` when the command should fail if max diff exceeds a percent threshold.

## Editing Guidance

- Add or update cases in `IntegrationTestCases.tsx`.
- Keep case ids stable; reports and comparisons key by id.
- Prefer recording platform limitations in observations over silently skipping behavior.
- If a snapshot can hang a platform, use the explicit skip/expected-failure mechanisms already present in the cases.
- Keep output under `/private/tmp` or another disposable directory; result sets and decoded images can be large.
