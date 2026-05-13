# Valdi Testing

## Component Tests (most common)

Use `valdiIt` from `valdi_test/test/JSXTestUtils` as the test wrapper. It provides a `driver` that renders components synchronously.

See **`valdi-component-tests`** skill for the full guide on:
- `elementKeyFind`, `elementTypeFind`, `componentTypeFind`, `componentGetElements`
- `tapNodeWithKey` for tap callbacks
- Discriminated union and array view model testing
- Lint rules (`jsx-no-lambda`, `explicit-function-return-type`, etc.)

## Running Tests

```bash
# Run a specific module's tests
bazel test //modules/my_module:tests

# Run with output on failure
bazel test //modules/my_module:tests --test_output=errors

# Run all tests in the repo
bazel test //...

# Filter to a specific test class
bazel test //modules/my_module:tests --test_filter=MyComponentTest
```

## Test File Location

Test files must mirror the source file hierarchy:

```
my_module/
├── src/
│   └── categories/
│       └── CollectionComponent.tsx
└── test/
    └── categories/
        └── CollectionComponentTest.spec.tsx
```

## BUILD.bazel for Tests

Tests are included in the same `valdi_module` target as source files. The `valdi_module` rule auto-generates a test target from `test/` files:

```python
load("//bzl/valdi:valdi_module.bzl", "valdi_module")

valdi_module(
    name = "my_module",
    srcs = glob([
        "src/**/*.ts",
        "src/**/*.tsx",
        "test/**/*.ts",
        "test/**/*.tsx",
    ]) + ["tsconfig.json"],
    deps = [
        "//src/valdi_modules/src/valdi/valdi_core",
        "//src/valdi_modules/src/valdi/valdi_tsx",
        "//src/valdi_modules/src/valdi/valdi_test",
        "//src/valdi_modules/src/valdi/foundation/test/util",
    ],
)
```

## Platform Tests

For C++, iOS, and Android platform layer tests, see the `valdi-cpp-runtime`, `valdi-ios`, and `valdi-android` skills respectively.

## Combined Validation with agent-check

Instead of running build and test separately, use `agent-check`:

```bash
valdi agent-check --module my_module                   # build + lint + test
valdi agent-check --module my_module --quick           # lint + test only (hot reloader running)
valdi agent-check --module my_module --quick --json    # machine-readable output
```

See the **valdi-setup** skill for the full hot reloader + agent-check iteration workflow.
