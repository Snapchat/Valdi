# Versioning and Support

This document says what Valdi promises to keep stable, how version numbers work, and which platforms and toolchains we support.

## Version numbers

Valdi will follow [Semantic Versioning](https://semver.org) starting at 1.0.0: `MAJOR.MINOR.PATCH`.

- **MAJOR** releases may break public API. Each one comes with a migration guide.
- **MINOR** releases add features without breaking public API. They may deprecate things.
- **PATCH** releases fix bugs. No new features and no API changes.

Until 1.0, releases are tagged `beta-X.Y.Z`, and any beta release may include breaking changes.

The framework and the `valdi` command-line tool are versioned separately. Each CLI release notes which framework versions it supports.

## What counts as public API

These are covered by the compatibility promise:

- TypeScript APIs exported from Valdi's documented modules.
- Bazel rules and macros documented in [`docs/`](./docs), such as `valdi_module`.
- CLI commands and flags documented in the CLI help or in [`docs/`](./docs).
- Generated native APIs that apps call from Swift, Objective-C, Kotlin, or Java, such as polyglot modules.
- The documented behavior of all of the above.

These are **not** public API and can change in any release:

- Anything marked experimental, beta, or internal-only, and anything undocumented.
- Anything under an `internal` path or prefixed with `_`.
- Compiler output format and generated code layout, beyond what apps call directly.
- Exact error message text, log output, and performance characteristics.
- The web target while it is in beta.

## Deprecation

Before we remove or change public API in a major release:

1. We mark it deprecated in a minor release, with a compiler or runtime warning where we can.
2. The deprecation note says what to use instead.
3. It stays available for at least **one minor release and 90 days** before removal.

Security fixes are the exception. If keeping an API means keeping a vulnerability, we may change it sooner and will say why in the release notes.

## Supported releases

We fix bugs on the latest release only. Fixes land on `main` and ship in the next release. We don't backport fixes to older releases. See also [SECURITY.md](./SECURITY.md).

## Platform support

Support tiers describe what we commit to, not just what CI happens to run.

| Tier | Meaning | Platforms |
|---|---|---|
| **Tier 1** | Supported. Tested in CI on every change. Bugs block a release. | iOS, Android |
| **Tier 2** | Beta. Tested in CI, but APIs and behavior may change. Bugs are fixed on a best-effort basis. | Web (HTML/CSS), macOS (as an app target) |
| **Development only** | Works for building and testing. Not supported for shipping apps. | Linux |
| **Unsupported** | Not supported and not planned. | Windows, macOS on Intel (x64) |

## Toolchain support

| Tool | Supported |
|---|---|
| Development host | macOS on Apple Silicon; Linux for Android builds and tests |
| Xcode | 26.5 and 27 |
| iOS deployment target | 15.0 and later |
| Android | API 23 (Android 6.0) and later |
| Node.js | 20 LTS or later |
| Bazel | 8.x, with bzlmod. The exact version is pinned in [`.bazelversion`](./.bazelversion). |
