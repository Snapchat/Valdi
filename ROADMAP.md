# Valdi Roadmap

This document reflects the current direction of the Valdi project. It's meant to answer common questions about what's coming, what we're investigating, and what's not on our current plan—so you can make informed decisions about whether Valdi is the right fit for your project today.

This roadmap is not a commitment. Priorities shift. If something here matters to you, open a [GitHub Discussion](https://github.com/Snapchat/Valdi/discussions) and tell us why.

---

## In Progress

### Web / HTML target
Valdi already renders to iOS, Android, and macOS. We're actively working on a web renderer that targets HTML/CSS, which would allow the same component to render in a browser without a native shell. The web renderer is available to try today—use it at your own risk and expect rough edges.

### bzlmod support
Valdi currently requires a `WORKSPACE`-based setup. We're migrating to bzlmod (`MODULE.bazel`), which is the modern Bazel dependency model and a prerequisite for publishing to the Bazel Central Registry. Once complete, you'll be able to consume Valdi as a standard Bazel module without managing `WORKSPACE` entries manually.

---

## Not on Our Current Roadmap

These are things that come up often. We're documenting them explicitly so you don't have to ask.

### Swift / SwiftUI runtime
The iOS runtime is currently Objective-C. We have no plans to rewrite it in Swift or to provide a SwiftUI interop layer at this time. [Polyglot modules](./docs/docs/native-polyglot.md) let you write Swift code that Valdi can call, which covers most integration needs.

### WebSocket API
Valdi doesn't expose a WebSocket API. The workaround is to implement WebSocket handling in native code and expose it to Valdi via a [polyglot module](./docs/docs/native-polyglot.md). We don't have plans to add first-class WebSocket support to the runtime.

### macOS Intel (x64) support
We only support Apple Silicon (arm64) for macOS development and as a macOS target. We don't have plans to add x64 support.

### Windows development environment
Valdi does not support Windows as a host OS for development.

---

## Questions?

Open a [GitHub Discussion](https://github.com/Snapchat/Valdi/discussions) or reach us on [Discord](https://discord.gg/uJyNEeYX2U).
