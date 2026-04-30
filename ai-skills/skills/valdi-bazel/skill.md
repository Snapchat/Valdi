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

1. **Use `bazel`** for all build commands
2. **The CLI wraps Bazel** - `valdi` commands use bazel under the hood
3. **Cache is important** - Don't suggest `bazel clean` unless necessary

## Build Rules

### Valdi-Specific Rules

- `/bzl/valdi/` - Valdi build rules and macros
- Custom rules for compiling .tsx to .valdimodule
- Platform-specific build transitions

### Common Targets

```python
load("@aspect_rules_ts//ts:defs.bzl", "ts_project")

# Valdi application
valdi_application(
    name = "my_app",
    root_component_path = "App@my_app/src/MyApp",
    title = "My App",
    version = "1.0.0",
    deps = ["//apps/my_app/src/valdi/my_app"],
)

# Web polyglot views — MUST be ts_project, never filegroup
ts_project(
    name = "my_module_web",
    srcs = glob(
        ["web/**/*.ts", "src/**/*.d.ts"],
        exclude = ["web/**/*.d.ts"],
    ),
    allow_js = True,
    composite = True,
    transpiler = "tsc",
    tsconfig = "web/tsconfig.json",
)

# Valdi module with platform deps
valdi_module(
    name = "my_module",
    srcs = glob(["src/**/*.ts", "src/**/*.tsx"]) + ["tsconfig.json"],
    ios_deps = [":my_ios_views"],          # objc_library
    macos_deps = [":my_macos_views"],      # objc_library (or omit to share ios_deps)
    android_deps = [":my_android_views"],  # valdi_android_library
    web_deps = [":my_module_web"],         # ts_project (never filegroup)
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
