# Valdi

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](./LICENSE.md)
[![Platforms](https://img.shields.io/badge/platform-iOS%20%7C%20Android%20%7C%20macOS-lightgrey)](./docs/INSTALL.md)
[![Status](https://img.shields.io/badge/status-beta-yellow)]()
[![Discord](https://img.shields.io/discord/1285677307163574322?color=7289da&label=Discord&logo=discord&logoColor=white)](https://discord.gg/uJyNEeYX2U)
[![TypeScript](https://img.shields.io/badge/TypeScript-5.x-blue?logo=typescript)](https://www.typescriptlang.org/)
[![Documentation](https://img.shields.io/badge/docs-available-brightgreen)](./docs/README.md)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](./CONTRIBUTING.md)

> [!NOTE]
> **Beta Status:** Valdi has been widely used in Snap's production apps for the last 8 years. We're calling this a beta because our tools and documentation need more battle testing in the open source world. Valdi will exit beta when we're happy with the developer experience.

**Valdi is a cross-platform UI framework that delivers native performance without sacrificing developer velocity.** Write your UI once in declarative TypeScript, and it compiles directly to native views on iOS, Android, and macOS—no web views, no JavaScript bridges. 

## Quick Example

A basic Valdi component:

```tsx
import { Component } from 'valdi_core/src/Component';

class HelloWorld extends Component {
  onRender() {
    const message = 'Hello World! 👻';
    <view backgroundColor='#FFFC00' padding={30}>
      <label color='black' value={message} />
    </view>;
  }
}
```

<p align="center">
  <img src="./docs/docs/assets/start-about/IMG_1445.jpg" width="400" alt="Hello World example running on iOS" />
</p>

## Quick Links

- [Getting Started Guide](./docs/INSTALL.md)
- [Documentation](./docs/README.md)
- [Codelabs](./docs/docs/start-code-lab.md)
- [API Reference](./docs/api/api-quick-reference.md)
- [Frequently Asked Questions](./docs/docs/faq.md)
- [Component Library](https://github.com/Snapchat/Valdi_Widgets)

## Why Choose Valdi?

Valdi is a cross-platform UI framework designed to solve the fundamental problem of cross-platform development: velocity vs. runtime performance. For 8 years, it has powered a large portion of Snap's production apps.

### True Native Performance

Unlike frameworks that rely on web views or JavaScript bridges, Valdi compiles declaratively rendered TypeScript components into platform-native views. Valdi also includes several other performance advantages:

- **[Automatic view recycling](./docs/docs/performance-view-recycling.md)** - Global view pooling system reuses native views across all screens, dramatically reducing inflation latency
- **Optimized component rendering** - Components re-render independently without triggering parent re-renders, enabling fast incremental updates
- **Optimized layout engine** - C++ layout engine runs on the main thread with minimal marshalling overhead
- **Viewport-aware rendering** - Only visible views are inflated, making infinite scrolling performant by default

Learn more in our [Performance Optimization Guide](./docs/docs/performance-optimization.md).

### Developer Experience Built for Speed

Valdi eliminates the traditional compile-test-debug cycle that slows native development:

- **Instant hot reload** - See changes in milliseconds on iOS, Android, or desktop without recompiling
- **[Full VSCode debugging](./docs/docs/workflow-hermes-debugger.md)** - Set breakpoints, inspect variables, profile performance, and capture heap dumps directly in VSCode
- **Familiar syntax** - TSX components with TypeScript for type safety

### Flexible Adoption Model

Valdi integrates easily into existing apps - start small and scale as needed:

- **[Embed Valdi in native](./docs/docs/native-bindings.md)** - Drop Valdi components into existing UIKit or Android view hierarchies
- **[Embed native in Valdi](./docs/docs/native-customviews.md)** - Use platform-specific views within Valdi layouts via `<custom-view>`
- **[Polyglot modules](./docs/docs/native-polyglot.md)** - Write performance-critical code in C++, Swift, Kotlin, or Objective-C with type-safe bindings to TypeScript
- **[Full-stack architecture](./docs/docs/advanced-full-stack.md)** - Build entire features in Valdi with worker threads for background processing, eliminating platform-specific bridge code

### Deep Native Integration

Valdi generates type-safe bindings between TypeScript and native platforms:

- **[Automatic code generation](./docs/docs/native-annotations.md)** - TypeScript interfaces compile to Kotlin, Objective-C, and Swift bindings
- **[Native API access](./docs/docs/native-polyglot.md)** - Direct access to platform APIs and third-party native libraries through polyglot modules
- **Bidirectional communication** - Pass complex data structures and callbacks between TypeScript and native code safely
- **[Native protobuf support](./docs/docs/advanced-protobuf.md)** - Seamless integration with protobuf for efficient data serialization

### Proven at Scale

- Powers critical features in production Snap apps.
- Supports [advanced animations](./docs/docs/advanced-animations.md), real-time rendering, and [complex gesture systems](./docs/docs/core-touches.md)

### Feature Highlights

- **[Flexbox layout system](./docs/docs/core-flexbox.md)** with automatic RTL support
- **[Worker threads](./docs/docs/advanced-worker-service.md)** for multi-threaded JavaScript execution
- **[Native animations](./docs/docs/advanced-animations.md)** for native look and feel
- **[Advanced gesture recognition](./docs/docs/core-touches.md)** with platform-native handling
- **[Built-in testing framework](./docs/docs/workflow-testing.md)** with component-level unit tests
- **[Bazel integration](./docs/docs/workflow-bazel.md)** for reproducible, incremental builds

## Need Help?

Join our [Discord](https://discord.gg/uJyNEeYX2U) for support.

## Contributing

Please follow the [contributing](./CONTRIBUTING.md) guidelines.

## License

Valdi is made available under the MIT [License](./LICENSE.md).
