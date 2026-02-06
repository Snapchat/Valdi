# Android Runtime Rules

**Applies to**: Kotlin/Java files in `/valdi/src/valdi/android/` and related Android runtime code

## Overview

The Valdi Android runtime bridges TypeScript/Valdi components to native Android views. It's implemented in Kotlin and C++ (via JNI).

## Code Style

- **4-space indentation** for Kotlin/Java
- Follow Kotlin coding conventions
- Use Kotlin features (data classes, extension functions, etc.)

## Key Concepts

### View Rendering

- Valdi components map to Android Views
- View recycling for performance
- Efficient view updates

### Platform Bridge

- JNI bridge to C++ runtime
- Kotlin/Java to native code communication
- Performance-critical paths

## Common Patterns

### Bazel Android Targets

```python
android_library(
    name = "valdi_android",
    srcs = glob(["**/*.kt", "**/*.java"]),
)
```

### View Binding

- Custom view implementations
- Attribute binding for Valdi properties
- Event handling

## Testing

```bash
# Run Android runtime tests
bazel test //valdi:test_java

# Run with coverage
bazel coverage //valdi:test_java
```

Test files are in `/valdi/test/java/`

## Building

```bash
# Build Android runtime library
bazel build //valdi:valdi_android

# Test with hello world app
valdi install android
cd apps/helloworld
valdi install android
```

## Platform-Specific Notes

1. **API Level** - Be mindful of minimum API level
2. **Android NDK** - C++ integration via NDK
3. **Permissions** - Handle runtime permissions appropriately
4. **Lifecycle** - Android Activity/Fragment lifecycle

## Important

- **Performance** - View creation and updates are critical
- **Memory** - Be careful with leaks (Activity context)
- **Threading** - UI thread vs background threads
- **Compatibility** - Support multiple Android versions

## More Information

- Runtime source: `/valdi/src/valdi/android/`
- Runtime tests: `/valdi/test/java/`
- Build config: `/valdi/BUILD.bazel`
- Framework docs: `/AGENTS.md`
