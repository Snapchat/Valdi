# Bazel Build System Rules

**Applies to**: `BUILD.bazel`, `*.bzl` files in `/bzl/`, `WORKSPACE`, `MODULE.bazel`

## Overview

Valdi uses Bazel as its build system. Bazel provides reproducible, incremental builds across all platforms.

## Key Commands

```bash
# Build everything
bazel build //...

# Build specific target
bazel build //apps/helloworld:helloworld

# Run tests
bazel test //...

# Clean (use sparingly - cache is valuable!)
bazel clean
```

## Important Notes

1. **`bzl` is an alias for `bazel`** - Both commands work
2. **The CLI wraps Bazel** - `valdi` commands use bazel under the hood
3. **Cache is important** - Don't suggest `bazel clean` unless necessary

## Build Rules

### Valdi-Specific Rules

- `/bzl/valdi/` - Valdi build rules and macros
- Custom rules for compiling .tsx to .valdimodule
- Platform-specific build transitions

### Common Targets

```python
# Valdi application
valdi_application(
    name = "my_app",
    root_component_path = "App@my_app/src/MyApp",
    title = "My App",
    version = "1.0.0",
    deps = ["//apps/my_app/src/valdi/my_app"],
)

# Valdi module
valdi_module(
    name = "my_module",
    srcs = glob(["src/**/*.ts", "src/**/*.tsx"]),
    deps = [
        "//src/valdi_modules/src/valdi/valdi_core",
    ],
)
```

## Conventions

### File Naming

- `BUILD.bazel` not `BUILD` (explicit extension)
- `.bzl` for Starlark macros and rules

### Targets

- Use descriptive target names
- One main target per BUILD file usually matches directory name

### Dependencies

- Be explicit about dependencies
- Don't rely on transitive deps implicitly
- Use visibility to control access

## Platform Builds

```bash
# Build and install iOS app
valdi install ios

# Build and install Android app
valdi install android

# Or use bazel directly with configs
bazel build //apps/helloworld:hello_world --config=ios
bazel build //apps/helloworld:hello_world --config=android
```

## Configuration

- `.bazelrc` - Build flags and configurations
- `MODULE.bazel` - Bazel module dependencies
- `WORKSPACE` - Legacy workspace configuration (being migrated to MODULE.bazel)

## Common Issues

1. **Missing dependencies** - Add to `deps` in BUILD.bazel
2. **Cache issues** - Try `bazel clean --expunge` (last resort)
3. **Platform transitions** - Use correct config flags

## Testing

```bash
# Run all tests
bazel test //...

# Run specific test
bazel test //valdi/test:renderer_test

# Run with coverage
bazel coverage //...
```

## More Information

- Bazel docs: https://bazel.build
- Valdi build rules: `/bzl/valdi/README.md`
- Framework docs: `/AGENTS.md`
