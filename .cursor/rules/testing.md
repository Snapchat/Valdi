# Testing Rules

**Applies to**: Test files in `**/test/`, `**/*.spec.ts`, `**/*.test.ts`

## Overview

All changes to Valdi should include appropriate tests. Valdi uses its own testing framework for component tests, plus platform-specific testing tools.

## Test Framework

### Jasmine for TypeScript Tests

Valdi uses Jasmine as its test framework for TypeScript/component tests:

```typescript
import 'jasmine/src/jasmine';
import { Component } from 'valdi_core/src/Component';

describe('MyComponent', () => {
  it('should render correctly', () => {
    const component = new MyComponent();
    expect(component).toBeDefined();
  });
  
  it('should handle state updates', () => {
    const component = new MyStatefulComponent();
    component.setState({ count: 1 });
    expect(component.state.count).toBe(1);
  });
});
```

Test files use `.spec.ts` extension and are typically in `test/` directories or alongside source files.

## Running Tests

```bash
# Run all tests
bazel test //...

# Run specific test suites
bazel test //valdi:test                    # All C++ runtime tests
bazel test //valdi:test_runtime            # Runtime tests
bazel test //valdi:test_integration        # Integration tests
bazel test //valdi:valdi_ios_objc_test     # iOS Objective-C tests
bazel test //valdi:valdi_ios_swift_test    # iOS Swift tests
bazel test //valdi:test_java               # Android/Java tests

# Run TypeScript module tests
bazel test //src/valdi_modules/src/valdi/valdi_test:valdi_test

# Run with output
bazel test //... --test_output=all

# Run with filter
bazel test //... --test_filter=MyComponentTest
```

## Test Conventions

### File Naming

- `*.spec.ts` or `*.test.ts` for unit tests
- `test/` directory for test files
- Test file should mirror source file name

### Test Structure

```typescript
describe('ComponentName', () => {
  beforeEach(() => {
    // Setup
  });
  
  afterEach(() => {
    // Cleanup
  });
  
  it('should do something specific', () => {
    // Arrange
    const component = new MyComponent();
    
    // Act
    component.doSomething();
    
    // Assert
    expect(component.result).toBe(expected);
  });
});
```

## Component Testing

### Mock Services

```typescript
class MockService {
  getData() { return 'mock data'; }
}

// Use in tests
const component = new MyComponent(mockRenderer, viewModel, mockService);
```

### Testing Lifecycle

```typescript
it('should call onCreate', () => {
  const component = new MyComponent();
  const onCreateSpy = spyOn(component, 'onCreate');
  
  renderer.renderComponent(component);
  
  expect(onCreateSpy).toHaveBeenCalled();
});
```

### Testing State Updates

```typescript
it('should update state correctly', () => {
  const component = new MyStatefulComponent();
  
  component.setState({ count: 1 });
  
  expect(component.state.count).toBe(1);
});
```

## Platform Tests

### C++ Tests

```cpp
TEST(RendererTest, RendersCorrectly) {
  auto renderer = std::make_unique<Renderer>();
  // Test logic
  EXPECT_TRUE(renderer->isValid());
}
```

### iOS Tests

```objc
- (void)testViewCreation {
    ValdiView *view = [[ValdiView alloc] init];
    XCTAssertNotNil(view);
}
```

### Android Tests

```kotlin
@Test
fun testViewCreation() {
    val view = ValdiView(context)
    assertNotNull(view)
}
```

## Coverage

Code coverage is supported via the `bazel coverage` command:

```bash
# Run tests with coverage
bazel coverage //src/valdi_modules/src/valdi/valdi_test:valdi_test
```

Coverage data is written to `$COVERAGE_OUTPUT_FILE` or `$TEST_UNDECLARED_OUTPUTS_DIR/coverage.dat`.

For more details, see `/docs/docs/workflow-testing.md`.

## Important Testing Principles

1. **Test behavior, not implementation** - Focus on what component does, not how
2. **Isolate tests** - Each test should be independent
3. **Mock dependencies** - Use mocks for services and external dependencies
4. **Test error cases** - Don't just test happy path
5. **Performance tests** - For critical rendering paths

## Common Pitfalls

- **Forgetting to clean up** - Use afterEach for cleanup
- **Flaky tests** - Avoid timing-dependent tests
- **Testing implementation details** - Test public API
- **Over-mocking** - Don't mock everything, test real integration when appropriate

## More Information

- Testing docs: `/docs/docs/workflow-testing.md`
- Framework docs: `/AGENTS.md`
