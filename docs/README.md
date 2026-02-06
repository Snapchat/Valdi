# Valdi Docs

 - [What is Valdi?](./docs/start-about.md)
 - [Getting Started with Valdi](./docs/start-install.md)
 - [Valdi Code Labs](./docs/start-code-lab.md)
 - [Valdi Command Line References](./docs/command-line-references.md)

## API Reference

 - [API Reference](./api/api-reference-elements.md) - Comprehensive API documentation for all native elements
 - [Style Attributes Reference](./api/api-style-attributes.md) - Complete guide to all style attributes for each element type
 - [Quick Reference Guide](./api/api-quick-reference.md) - Quick lookup for commonly used properties

## The Basics

 - [Valdi Module](./docs/core-module.md)
 - [The Mighty Component](./docs/core-component.md)
 - [Component States](./docs/core-states.md)
 - [Component Events](./docs/core-events.md)
 - [Control Flow](./docs/control-flow.md)
 - [FlexBox Layout](./docs/core-flexbox.md)
 - [`<layout>` and `<view>`](./docs/core-views.md)
 - [`<label>` and Text](./docs/core-text.md)
 - [`<image>`](./docs/core-images.md)
 - [`<video>`](./docs/core-video.md)
 - [`<scroll>`](./docs/core-scrolls.md)
 - [`<slot>`](./docs/core-slots.md)
 - [`Style<>`](./docs/core-styling.md)
 - [Touches and Gestures](./docs/core-touches.md)
 - [Navigation](./docs/navigation.md)
 
 ## Internals
 
  - [Runtime Internals](./docs/internals-runtime.md) - Deep dive into `Valdi::Value`, memory management, and the binary protocol
  - [Renderer Internals](./docs/internals-renderer.md) - Detailed explanation of the diffing algorithm and slot optimizations
  - [Compiler Internals](./docs/internals-compiler.md) - Pipeline architecture and TSX transformation details
  - [Native Integration Internals](./docs/internals-native-integration.md) - View factories, pooling, and attribute binding
  - [API Design & Extensibility](./docs/internals-api-design.md) - Philosophy, bridge modules, and layered architecture
 
 ## Native Integration

 - [Annotations](./docs/native-annotations.md)
 - [Native Bindings](./docs/native-bindings.md)
 - [Polyglot Modules](./docs/native-polyglot.md)
 - [Component Context](./docs/native-context.md)
 - [View Model](./docs/native-view-model.md)
 - [Type Conversions](./docs/native-types.md)
 - [The `<custom-view>`](./docs/native-customviews.md)
 - [Native CollectionViews](./docs/native-collectionview.md)

## Client Libraries
 - [Protobuf](./docs/advanced-protobuf.md)
 - [RxJS](./docs/client-libraries-rxjs.md)

## Standard Library
 - [Core Utilities](./docs/stdlib-coreutils.md) - Array utilities, Base64, LRU Cache, MD5, UUID, and more
 - [HTTP Client](./docs/stdlib-http.md) - Promise-based HTTP client for network requests
 - [Persistent Storage](./docs/stdlib-persistence.md) - Key-value storage with encryption and TTL support
 - [File System](./docs/stdlib-filesystem.md) - Low-level file I/O operations (internal use)
 - [Glossary](./docs/glossary.md) - Comprehensive guide to Valdi terminology

## Debugging Tools
 - [Hermes Debugger](./docs/workflow-hermes-debugger.md)
 - [Valdi Inspector](./docs/workflow-inspector.md)

## Advanced Topics
 - [Animations](./docs/advanced-animations.md)
 - [Custom Image Loader](./docs/advanced-images.md)
 - [Localization](./docs/advanced-localization.md)
 - [Element References](./docs/advanced-element-references.md)
 - [Provider](./docs/advanced-provider.md)
 - [Native References](./docs/advanced-native-references.md)
 - [Worker Service](./docs/advanced-worker-service.md)
 - [Full stack Valdi](./docs/advanced-full-stack.md)

## Performance
 - [Optimization](./docs/performance-optimization.md)
 - [Tracing](./docs/performance-tracing.md)
 - [Memory Leaks](./docs/performance-memory-leaks.md)
 - [View Recycling](./docs/performance-view-recycling.md)

## Workflow
 - [SwiftPM, CocoaPods, Xcode and Gradle integration](./docs/workflow-external-build-system.md)
 - [Releasing to app stores](./docs/workflow-appstore-release.md)
 - [Building a CLI application](./docs/workflow-cli-application.md)
 - [Valdi rules for Bazel](./docs/workflow-bazel.md)
 - [Valdi Style Guide](./docs/workflow-style-guide.md)
 - [Valdi Inspector](./docs/workflow-inspector.md)
 - [Testing](./docs/workflow-testing.md)
 - [Hermes Debugger](./docs/workflow-hermes-debugger.md)
 - [Disk Management](./docs/workflow-disk.md)

 ## Misc
 - [Third party dependencies](./docs/third-party-dependencies.md)
 - [Frequently Asked Questions](./docs/faq.md)

## Help
 - [Support](./docs/help-support.md)
 - [Troubleshooting](./docs/help-troubleshooting.md)
