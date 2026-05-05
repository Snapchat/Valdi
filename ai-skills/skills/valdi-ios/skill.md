# iOS Runtime Rules

**Applies to**: Objective-C/C++ files in `/valdi/src/valdi/ios/` and related iOS runtime code

## Overview

The Valdi iOS runtime bridges TypeScript/Valdi components to native UIKit views. It's implemented in Objective-C, Objective-C++, and Swift.

## Code Style

- **4-space indentation** for Objective-C
- Follow Apple's Objective-C conventions
- Use modern Objective-C features (properties, blocks, etc.)

## Key Concepts

### View Rendering

- Valdi components map to UIViews
- View recycling for performance
- UIKit integration

### Platform Bridge

- Objective-C++ bridge to C++ runtime
- Memory management (ARC)
- iOS-specific APIs

## Common Patterns

### Bazel iOS Targets

```python
objc_library(
    name = "valdi_ios",
    srcs = glob(["**/*.m", "**/*.mm"]),
    hdrs = glob(["**/*.h"]),
)
```

### View Implementation

- Custom UIView subclasses
- CALayer for advanced rendering
- Metal for GPU acceleration

## Testing

```bash
# Run iOS runtime tests (Objective-C)
bazel test //valdi:valdi_ios_objc_test

# Run iOS runtime tests (Swift)
bazel test //valdi:valdi_ios_swift_test

# Run all iOS tests
bazel test //valdi:valdi_ios_objc_test //valdi:valdi_ios_swift_test
```

Test files are in `/valdi/test/ios/` and `/valdi/test/ios_swift/`

## Building

```bash
# Build iOS runtime library
bazel build //valdi:valdi_ios

# Test with hello world app
cd apps/helloworld
valdi install ios
```

## Platform-Specific Notes

1. **iOS Version** - Be mindful of minimum iOS version
2. **ARC** - Automatic Reference Counting (memory management)
3. **Auto Layout** - Valdi uses flexbox, not Auto Layout
4. **Metal** - GPU rendering for advanced graphics

## Important

- **Performance** - UIView creation and layout are critical
- **Memory** - Understand retain cycles and weak references
- **Threading** - Main thread for UI, background for heavy work
- **Lifecycle** - UIViewController lifecycle

## More Information

- Runtime source: `/valdi/src/valdi/ios/`
- Runtime tests: `/valdi/test/ios/` and `/valdi/test/ios_swift/`
- Core iOS code: `/valdi_core/src/valdi_core/ios/`
- Build config: `/valdi/BUILD.bazel`
- Framework docs: `/AGENTS.md`
