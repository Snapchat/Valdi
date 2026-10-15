# Valdi Roadmap

This document describes where Valdi is headed. It answers common questions about what's coming, what we're looking at, and what we don't plan to do, so you can decide whether Valdi fits your project today.

This roadmap is not a commitment. Priorities shift. If something here matters to you, open a [GitHub Discussion](https://github.com/Snapchat/Valdi/discussions) and tell us why.

---

## Valdi 1.0

Valdi is in beta. We're working toward a 1.0 release.

1.0 is a stability promise, not a feature release. For 1.0 we are focused on:

- **A versioning policy.** What counts as public API, how we version it, and how long deprecated APIs stay around. See [VERSIONING.md](./VERSIONING.md).
- **Supported platforms and toolchains.** A published list of what we test and support. See [VERSIONING.md](./VERSIONING.md).
- **Predictable contribution flow.** Clear expectations for how quickly pull requests get a response ([CONTRIBUTING.md](./CONTRIBUTING.md)), and an [RFC process](./rfcs/README.md) for large changes.
- **Reliable public CI.** `main` stays green, and failures get fixed or tracked quickly.

## In progress

### Web / HTML target
Valdi renders to iOS, Android, and macOS. The web renderer, which targets HTML/CSS, is available to try. It ships as **beta** in 1.0: expect rough edges and API changes.

### Newer Apple toolchains
`main` builds with Xcode 27 on macOS 27, and the minimum iOS deployment target is now 15.0. These changes ship in the next release.

## Done recently

### bzlmod support
Valdi uses bzlmod (`MODULE.bazel`) and Bazel 8. You no longer need a `WORKSPACE`-based setup.

## Not on our current roadmap

These come up often. We're listing them so you don't have to ask.

### Swift / SwiftUI runtime
The iOS runtime is Objective-C. We have no plans to rewrite it in Swift or to add a SwiftUI interop layer. [Polyglot modules](./docs/docs/native-polyglot.md) let you write Swift code that Valdi can call, which covers most integration needs.

### WebSocket API
Valdi doesn't expose a WebSocket API. Implement WebSocket handling in native code and expose it to Valdi through a [polyglot module](./docs/docs/native-polyglot.md).

### macOS Intel (x64) support
We only support Apple Silicon (arm64) for macOS development and as a macOS target.

### Windows development environment
Windows is not supported as a host OS for development.

---

## Questions?

Open a [GitHub Discussion](https://github.com/Snapchat/Valdi/discussions) or [file an issue](https://github.com/Snapchat/Valdi/issues).
