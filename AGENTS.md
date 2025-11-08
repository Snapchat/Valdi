# AGENTS.md - Guide for AI Coding Assistants

This document provides context and guidelines for AI coding assistants working with the Valdi codebase.

## Overview

Valdi is a cross-platform UI framework that compiles declarative TypeScript components to native views on iOS, Android, and macOS. Write your UI once, and it runs natively on multiple platforms without web views or JavaScript bridges.

Valdi has been used in production at Snap for 8 years and is now available as open source under the MIT license.

### How Valdi Works

The Valdi compiler takes TypeScript source files (using TSX/JSX syntax) and compiles them into `.valdimodule` files. These compiled modules are read by the Valdi runtime on each platform to render native views. **This is not TypeScript rendered in a WebView** - Valdi generates true native UI components.

## Key Technologies

- **Valdi**: TypeScript-based declarative UI framework that compiles to native code
- **TSX/JSX**: React-like syntax for declarative UI (but this is not React - Valdi compiles to native)
- **Bazel**: Primary build system for reproducible builds
- **TypeScript/JavaScript**: Application and UI layer
- **C++**: Cross-platform runtime and layout engine
- **Swift**: Compiler implementation
- **Kotlin/Java**: Android runtime
- **Objective-C/C++**: iOS runtime
- **Flexbox**: Layout system with automatic RTL support

## Directory Structure

### `/apps/`
Example applications demonstrating Valdi features:
- `helloworld/` - Basic getting started example
- `valdi_gpt/` - More complex demo application
- `benchmark/` - Performance testing app
- `*_example/` - Various feature demonstrations (navigation, managed context, etc.)

### `/compiler/`
The Valdi compiler and companion tools:
- `compiler/` - Swift-based main compiler that transforms TypeScript to native code
- `companion/` - TypeScript-based companion tools for the build process

### `/valdi/`, `/valdi_core/`, `/valdi_protobuf/`
Core Valdi runtime implementations:
- Platform-specific implementations (iOS, Android, macOS)
- Cross-platform C++ core with layout engine
- Protobuf integration for efficient serialization
- Generated code from Djinni interfaces for cross-language bindings

### `/src/valdi_modules/`
Core Valdi TypeScript modules and standard library:
- `valdi_core/` - Core component and runtime APIs (Component, Provider, etc.)
- `valdi_protobuf/` - Protobuf serialization support
- `valdi_http/` - HTTP client module (promise-based network requests)
- `valdi_navigation/` - Navigation utilities
- `valdi_rxjs/` - RxJS integration for reactive programming
- `persistence/` - Key-value storage with encryption and TTL support
- `drawing/` - Managed context for graphics and drawing operations
- `file_system/` - Low-level file I/O operations
- `valdi_web/`, `web_renderer/` - Web runtime implementations
- `foundation/`, `coreutils/` - Common utilities (arrays, Base64, LRU cache, UUID, etc.)
- `worker/` - Worker service support for background JavaScript execution
- Other standard library modules

### `/npm_modules/`
Node.js packages:
- `cli/` - Command-line interface for Valdi development (`valdi` command)
- `eslint-plugin-valdi/` - ESLint rules for Valdi code

### `/bzl/`
Bazel build rules and macros for the Valdi build system

### `/docs/`
Comprehensive documentation:
- Codelabs for learning
- API documentation
- Setup and installation guides

### `/third-party/`
External dependencies and their Bazel build configurations

## Important Conventions

### Build System

1. **Bazel is the primary build system** - Use `bazel build`, `bazel test`, etc.
   - Note: `bzl` is an alias for `bazel` - both commands work interchangeably
   - The CLI looks for `bazel`, `bzl`, or `bazelisk` executables
2. **MODULE.bazel and WORKSPACE** - Bazel module system is in use
3. **Cross-platform builds** - Code must work on iOS, Android, Linux, and macOS
4. **Platform transitions** - Build rules handle platform-specific compilation automatically

### Code Style

1. **C++**: Follow the project's C++ style conventions
2. **TypeScript**: Use ESLint with Valdi-specific rules
3. **Swift**: Follow Swift conventions for compiler code
4. **Kotlin**: Follow Kotlin conventions for Android runtime
5. **Indentation**: Always match existing file conventions

### Testing

1. Test files are typically in `test/` subdirectories
2. Run tests with `bazel test //path/to:target`
3. All changes should include appropriate tests
4. Use the built-in Valdi testing framework for component tests

### Generated Code

1. **Djinni interfaces** - Some code is generated from `.djinni` files for cross-language bindings
2. **Don't modify generated code** - Change the source `.djinni` file instead
3. Generated files are typically in `generated-src/` directories

## Common Tasks

### Building the Compiler

```bash
# Build the compiler
bazel build //compiler/compiler:valdi-compiler

# After building, move the binary to bin/ directory for use by the toolchain
# The exact path depends on your platform (macos/linux and architecture)
# Example for macOS ARM64:
cp bazel-bin/compiler/compiler/valdi-compiler bin/compiler/macos/arm64/
```

Note: Pre-built compiler binaries are checked in to `/bin/compiler/` for convenience, but you can build and use your own version during development.

### Running Tests

```bash
# Run all tests
bazel test //...

# Run specific test
bazel test //valdi/test:some_test
```

### Installing and Using the CLI

```bash
cd npm_modules/cli

# Install the valdi command-line tool globally
npm run cli:install

# After installation, use the CLI
valdi --help
```

### Creating New Examples

Use existing apps in `/apps/` as templates. Each app needs:
- `BUILD.bazel` file defining build targets
- `package.json` for npm dependencies
- Entry point file (typically `.tsx` for TypeScript JSX)
- Source files in `src/` directory

## Important Files to Review

- `/README.md` - Main project documentation
- `/docs/INSTALL.md` - Installation and setup instructions
- `/docs/DEV_SETUP.md` - Developer environment setup
- `/CONTRIBUTING.md` - Contribution guidelines
- `/CODE_OF_CONDUCT.md` - Community standards
- `/LICENSE.md` - MIT License information

## Toolchain Locations

Pre-built binaries are stored in `/bin/`:
- Compiler binaries for Linux/macOS
- SQLite compiler for data persistence
- Other build tools

## Platform-Specific Notes

### iOS
- Uses Objective-C++ bridge layer for TypeScript-native communication
- Metal for GPU-accelerated rendering
- See `/valdi/src/ios/` for platform implementations

### Android
- Kotlin/Java implementations
- Uses Android NDK for C++ integration
- See `/valdi/src/android/` for platform implementations

### Web
- TypeScript runtime for web targets
- See `/src/valdi_modules/src/valdi/web_renderer/` for web implementation

### Desktop (macOS)
- Native macOS implementation
- See `/valdi/src/valdi/macos/` for desktop implementations

## Development Workflow

1. **Setup environment** - Follow `/docs/DEV_SETUP.md`
2. **Make changes** in appropriate directory
3. **Build locally** with Bazel
4. **Run tests** to verify changes
5. **Run linters** with appropriate tools
6. **Test on multiple platforms** - Changes may affect iOS, Android, and web
7. **Update documentation** if adding features

## Common Patterns

### Component Development

Valdi components follow a class-based pattern with lifecycle methods:

```typescript
import { Component } from 'valdi_core/src/Component';

class MyComponent extends Component {
  // Required: Render the component's UI
  onRender() {
    <view>
      <label value="Hello" />
    </view>;
  }
  
  // Optional lifecycle methods:
  // onMount() - Called when component is first mounted
  // onUnmount() - Called before component is removed
  // onUpdate(prevProps) - Called when component updates
}
```

**Key Valdi Concepts:**
- Components use TSX/JSX syntax (similar to React but compiles to native)
- State management via component properties
- Event handlers for user interactions
- Flexbox layout system for positioning

### Native Polyglot Modules

For performance-critical code, write native implementations with TypeScript bindings:
- Define interfaces in TypeScript
- Specify polyglot modules in build files (BUILD.bazel)
- Implement in C++, Swift, Kotlin, or Objective-C
- Compiler generates type-safe bindings

### Worker Services

For background processing:
- Create worker services that run in separate JavaScript contexts
- Communicate via message passing
- See `/docs/docs/advanced-worker-service.md`

### Component Context & Native Integration

Pass data and services between native code and Valdi:
- **Component Context**: Pass native data to Valdi components when instantiating them
- **Native Annotations**: Use TypeScript comments to export components to native platforms
- **Example**: `@Component` and `@ExportModel` annotations define how components are exposed
- See `/docs/docs/native-annotations.md` and `/docs/docs/native-context.md`

### Provider Pattern

Dependency injection for Valdi components:
- Use `Provider` to pass services and data down the component tree
- Similar to React Context but Valdi-specific
- Enables loose coupling and testability
- See `/docs/docs/advanced-provider.md`

### Localization

String management for multi-language support:
- String resources defined in JSON files
- Automatic locale switching based on device settings
- See `/docs/docs/advanced-localization.md`

## Common Pitfalls

1. **Don't skip cross-platform testing** - Changes affect multiple platforms
2. **Don't modify generated code** - Change the source instead
3. **Don't ignore Bazel cache** - Use `bazel clean` sparingly
4. **Don't hardcode platform assumptions** - Use appropriate abstractions
5. **Performance matters** - Valdi is a UI framework where rendering performance is critical

## Architecture Overview

### Compilation Pipeline

1. **TypeScript source** → Valdi compiler (Swift)
2. **Compiler output** → Platform-specific code generation
3. **Native builds** → iOS/Android/macOS apps
4. **Runtime** → C++ layout engine + platform-specific renderers

### Hot Reload System

- Valdi includes instant hot reload during development
- Changes to TypeScript components are reflected in milliseconds
- No need to recompile native code for UI changes
- Use `valdi hotreload` command

### Performance Features

- **View recycling** - Global view pooling reuses native views
- **Viewport-aware rendering** - Only visible views are inflated
- **Independent component rendering** - Components update without parent re-renders
- **Optimized layout** - C++ layout engine with minimal marshalling

## Key Points for AI Assistants

1. **Cross-platform compatibility is critical** - Test implications across iOS, Android, and web
2. **Bazel is non-negotiable** - Don't suggest alternative build systems
3. **Generated code exists** - Some files are auto-generated from Djinni interfaces
4. **Performance is paramount** - This is a production UI framework used at scale
5. **Follow existing patterns** - This is a mature codebase with established conventions
6. **TypeScript is compiled** - Unlike React Native, this doesn't run JavaScript at runtime
7. **Native integration is deep** - Direct access to platform APIs via polyglot modules

## Quick Reference Commands

### For App Development

```bash
# Install Valdi CLI (first time)
cd npm_modules/cli && npm run cli:install

# Setup development environment
valdi dev_setup

# Bootstrap a new project
mkdir my_project && cd my_project
valdi bootstrap

# Install dependencies and build
valdi install ios    # or android

# Start hot reload
valdi hotreload
```

### For Platform Development (Contributing to Valdi)

```bash
# Setup development environment (first time)
scripts/dev_setup.sh

# Build everything
bazel build //...

# Run all tests
bazel test //...

# Build and run example app
cd apps/helloworld
valdi install ios    # or android
```

## Testing Framework

Valdi includes a built-in testing framework:
- Component-level unit tests
- Mock services and dependencies
- See `/docs/docs/workflow-testing.md`

```typescript
import { TestRunner } from 'valdi_core/src/TestRunner';

TestRunner.test('component renders correctly', () => {
  const component = new MyComponent();
  // Test assertions
});
```

## Debugging

- **VSCode integration** - Full debugging support with breakpoints
- **Hermes debugger** - For JavaScript debugging
- **Native debugging** - Xcode/Android Studio for platform-specific issues
- See `/docs/docs/workflow-hermes-debugger.md`

## Related Documentation

For more details on specific topics, see the `/docs/` directory:
- Architecture overview
- API reference
- Codelabs for hands-on learning
- Advanced features (animations, gestures, protobuf)
- Native bindings and custom views

## Contributing

This is an open-source project. When contributing:
1. Follow the style guides
2. Include tests for new features
3. Update documentation
4. Ensure cross-platform compatibility
5. See `CONTRIBUTING.md` for full guidelines

## Community & Support

- **Discord**: Join the [Valdi Discord community](https://discord.gg/uJyNEeYX2U) for support and discussions
- **Documentation**: Comprehensive docs in `/docs/` directory
- **Examples**: Working examples in `/apps/` directory
- **Issues**: Report bugs and request features via GitHub issues
- **Discussions**: Ask questions and share ideas in GitHub Discussions

---

*This document is intended for AI coding assistants to quickly understand the structure and conventions of the Valdi codebase. For human developers, please refer to the main README.md and comprehensive documentation in `/docs/`.*
