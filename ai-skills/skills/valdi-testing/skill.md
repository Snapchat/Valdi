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

Add a `ts_project` or `valdi_module` test target. The test target lists test spec files in `srcs` and shares `deps` with the main module:

```python
load("//bzl/valdi:valdi_module.bzl", "valdi_module")

valdi_module(
    name = "my_module_tests",
    srcs = glob(["test/**/*.spec.tsx", "test/**/*.spec.ts"]),
    testonly = True,
    deps = [
        ":my_module",
        "//src/valdi_modules/src/valdi/valdi_test",
        "//src/valdi_modules/src/foundation/test/util",
        "//src/valdi_modules/src/valdi/valdi_tsx",
    ],
)
```

## Platform Tests

For C++, iOS, and Android platform layer tests, see the `valdi-cpp-runtime`, `valdi-ios`, and `valdi-android` skills respectively.
