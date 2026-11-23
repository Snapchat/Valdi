# Valdi

Valdi is a cross-platform UI framework that compiles declarative TypeScript components to native views on iOS, Android, and macOS. There are no web views and no JavaScript bridge. It has powered a large portion of Snap's production apps for 8 years.

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

## Get started

- [What is Valdi?](./start-about.md): what the framework does and how it fits with native code.
- [Choosing Valdi](./choosing-valdi.md): how it compares to React Native, Flutter, and native, including current gaps.
- [Quick Start](./start-install.md): install the CLI and run your first app.
- [Code Labs](./start-code-lab.md): self-paced tutorials, in the recommended order.

## Coming from another framework

- [React](./start-from-react.md)
- [Flutter](./migrate-from-flutter.md)
- [Jetpack Compose](./migrate-from-compose.md)

## Learn the basics

- [The Mighty Component](./core-component.md), [Component States](./core-states.md), and [Component Events](./core-events.md)
- [FlexBox Layout](./core-flexbox.md) and [Styling](./core-styling.md)
- [API Reference](../api/api-reference-elements.md) and the [Quick Reference Guide](../api/api-quick-reference.md)

## Go further

- [Native Integration](./native-bindings.md): embed Valdi in an existing iOS or Android app, or native views in Valdi.
- [Performance](./performance-optimization.md): view recycling, tracing, and memory.
- [Debugging](./workflow-hermes-debugger.md): breakpoints, profiling, and heap dumps in VS Code.

## Get help

- [FAQ](./faq.md) and [Troubleshooting](./help-troubleshooting.md)
- [GitHub Discussions](https://github.com/Snapchat/Valdi/discussions) for questions
- [GitHub Issues](https://github.com/Snapchat/Valdi/issues) for bugs and feature requests
