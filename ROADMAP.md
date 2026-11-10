# Valdi Roadmap

This document describes where Valdi is headed. It answers common questions about what's coming, what we're looking at, and what we don't plan to do, so you can decide whether Valdi fits your project today.

This roadmap is not a commitment. Priorities shift. If something here matters to you, open a [GitHub Discussion](https://github.com/Snapchat/Valdi/discussions) and tell us why.

---

## Valdi 1.0

Valdi is in beta. We're working toward a 1.0 release.

1.0 is a stability promise, not a feature release. It comes with:

- **A versioning policy.** Done. What counts as public API, how we version it, and how long deprecated APIs stay around. See [VERSIONING.md](./VERSIONING.md).
- **Supported platforms and toolchains.** Done. A published list of what we test and support. See [VERSIONING.md](./VERSIONING.md#platform-support).
- **Predictable contribution flow.** Done. How quickly pull requests get a response and land ([CONTRIBUTING.md](./CONTRIBUTING.md#3-what-to-expect-after-you-open-a-pr)), and an [RFC process](./rfcs/README.md) for large changes.
- **Reliable public CI.** In progress. `main` stays green, and failures get fixed or tracked quickly.

1.0 does not include:

- **Web feature parity.** Web ships as beta. See [Web / HTML target](#web--html-target).
- **Full support for macOS or Linux.** macOS stays beta and Linux stays development only. See [VERSIONING.md](./VERSIONING.md#platform-support).

## In progress

### Web / HTML target
Valdi renders to iOS, Android, and macOS. The web renderer targets HTML/CSS. Web ships as **beta** in 1.0: expect rough edges and API changes. To try it, see [Web development](./docs/docs/workflow-web-development.md).

### Linux app host
Two accepted RFCs let you run Valdi on Linux from source: [embedding the runtime](./rfcs/0002-linux-headless-sdk.md) in a Linux program, and an [optional window host](./rfcs/0001-linux-snapdrawing-host.md) that draws Valdi components in an SDL3 window. Contributors have started implementing them. Linux stays development only, not supported for shipping apps. See [#6](https://github.com/Snapchat/Valdi/issues/6).

## Under discussion

### Terminal renderer
[RFC 0003](https://github.com/Snapchat/Valdi/pull/202) proposes a renderer for command-line apps. It's in review.

## Done recently

### Newer Apple toolchains
Valdi builds with Xcode 27 on macOS 27, and the minimum iOS deployment target is now 15.0. These changes shipped in beta-0.2.1.

### bzlmod support
Valdi uses bzlmod (`MODULE.bazel`) and Bazel 8. You no longer need a `WORKSPACE`-based setup.

## Not on our current roadmap

These come up often. We're listing them so you don't have to ask.

### Swift / SwiftUI runtime
The iOS runtime is Objective-C. We have no plans to rewrite it in Swift or to add a SwiftUI interop layer. [Polyglot modules](./docs/docs/native-polyglot.md) let you write Swift code that Valdi can call, which covers most integration needs.

### WebSocket API
Valdi doesn't expose a WebSocket API. Implement WebSocket handling in native code and expose it to Valdi through a [polyglot module](./docs/docs/native-polyglot.md).

### macOS Intel (x64) support
We only support Apple Silicon (arm64) for macOS development and as a macOS target. Exported iOS frameworks no longer include an x86_64 simulator slice.

### Windows development environment
Windows is not supported as a host OS for development.

---

## Questions?

Open a [GitHub Discussion](https://github.com/Snapchat/Valdi/discussions) or [file an issue](https://github.com/Snapchat/Valdi/issues).
