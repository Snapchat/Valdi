# C++ Runtime Rules

**Applies to**: C++ files in `/valdi/`, `/valdi_core/`, `/snap_drawing/`

## Overview

Valdi's runtime and layout engine are implemented in C++ for cross-platform performance. This code runs on iOS, Android, macOS, but not web.

**Note**: `/libs/` contains shared utility libraries (crypto, logging, image processing) used across the codebase, but not the core runtime itself.

## Code Style

- **4-space indentation**
- Follow existing C++ style in the codebase
- Use smart pointers appropriately
- Prefer const correctness

## Key Concepts

### Layout Engine

- Uses **Yoga** (Facebook's Flexbox implementation) for layout
- Cross-platform layout calculations
- Performance-critical code
- RTL (right-to-left) support built-in
- Yoga source: `/third-party/yoga/`

### Memory Management

- Be careful with memory ownership
- Use RAII patterns
- Consider platform-specific memory constraints (mobile)

### Platform Abstractions

- Code must work on iOS, Android, macOS
- Use platform-agnostic APIs where possible
- Platform-specific code goes in appropriate subdirectories

## Common Patterns

### Djinni Generated Code

- Some C++ code is generated from `.djinni` interface files
- **Don't modify generated code** - change the .djinni file instead
- Generated files typically in `generated-src/` directories

### Performance

- This is performance-critical code
- Profile before optimizing
- Consider cache locality
- Be mindful of allocations in hot paths

## Testing

```bash
# Run all C++ runtime tests
bazel test //valdi:test

# Run specific test suites
bazel test //valdi:test_runtime       # Runtime tests
bazel test //valdi:test_integration   # Integration tests
bazel test //valdi:test_snap_drawing  # Snap drawing tests
```

## Platform-Specific Notes

### iOS
- Objective-C++ bridge in `/valdi/src/valdi/ios/`
- Metal for GPU rendering
- UIKit view integration

### Android
- JNI bridge in `/valdi/src/valdi/android/`
- NDK integration
- Native view rendering

### macOS
- Platform-specific code in `/valdi/src/valdi/macos/`
- Desktop runtime support

### Build

```bash
# Build runtime for specific platform
bazel build //valdi:valdi_ios
bazel build //valdi:valdi_android
bazel build //valdi:valdi_macos
```

## Important

1. **Cross-platform first** - Code must work on all platforms
2. **Performance critical** - UI rendering and layout
3. **Memory efficiency** - Mobile devices have constraints
4. **Thread safety** - Consider concurrency

## More Information

- Runtime overview: `/valdi/README.md`
- Core bindings: `/valdi_core/README.md`
- Framework docs: `/AGENTS.md`
