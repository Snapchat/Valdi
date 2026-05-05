# Testing

## TypeScript testing

Valdi provides a standalone runtime that can execute Valdi code without requiring integration with a host iOS/Android app. Adding new tests is easy and lightweight; they run very fast and can use the hot reloader for rapid iteration. The runtime leverages the [jasmine](https://jasmine.github.io/) testing framework which is probably already familiar to you.

> [!Note]
> In **Standalone Mode**, Valdi can run on Linux and macOS without any UI, similar to Node.js. This mode is used for running unit tests and scripts, and it evaluates code from `.valdimodule` archives produced by the Valdi compiler.

### How to add new tests?

You can add .ts/.tsx files to the `test/` directory of your Valdi module. All .ts/.tsx files within the `test/` directory are included when running tests. The convention is to have test files be named `FileToTest.spec.ts`.
If you'd like to reference some code from a different module that you don't want to be a dependency of your Valdi module, create a new Valdi module just for your tests.

If using the [jasmine](https://jasmine.github.io/) framework, ensure you import it in your test file `import 'jasmine/src/jasmine';`.

### How do I run my tests?

You can use the `bazel test` Bazel command to test your module. It will compile your files and execute all the tests that were found. All the Valdi modules have an implicit Bazel target named `test`.

```sh
bazel test //src/valdi_modules/src/valdi/your_module_name:test
```

### How to iterate on my tests using the hot reloader?

You can run your unit tests with both the VSCode debugger and the hot reloader. This enables you to iterate on your tests as well as debug them. To enable the debugger and hot reloader service, you need to pass the `--//src/valdi_modules/bzl:hot_reload_enabled` flag to the Bazel invocation. Tests running with the hot reloader should be run using `bazel run` instead of `bazel test`, so that you can get your logs in the console.

- Start the hot reloader (using `valdi hotreload`)
- Run `bazel run //src/valdi_modules/src/valdi/your_module_name:test --//src/valdi_modules/bzl:hot_reload_enabled` to run the standalone test runtime in hot reload mode

Now, whenever you edit the test files in `your_module_name/test` and hit save, the hot reloader will compile and send
the updated tests to the standalone runtime, which will then re-run the tests and show you the results.

### How do I see/get code coverage data?

Refer to `./coverage/lcov-report/index.html`.

In addition to that, code coverage data can be generated using `./scripts/test_module_hotreload_with_code_coverage.sh`

### Example jasmine test

Here's an example test file that tests the `Style` object implementation: [valdi_test/test/Style.spec.ts](https://github.com/Snapchat/Valdi/blob/main/src/valdi_modules/src/valdi/valdi_test/test/Style.spec.ts).

`describe` is used to group test cases. You can add any variables, `beforeEach`, and `beforeAll` calls in this block.
`it` signifies a specific test case. Write your test case code in this block.
`expect` matchers are used to assert expected state at the end of the test case.

You can learn more about jasmine at https://jasmine.github.io/

### How to test Component logic?

We provide test utility APIs to help you drive your Component in your test cases. If you use the `valdiIt` function instead `it` to specify your test case, you'll get access to an `IComponentTestDriver` instance. Here's an example of a test that uses the driver to render a component and verify the resulting virtual node tree:

From [valdi_test/test/ComponentTestExample.tsx](../../src/valdi_modules/src/valdi/valdi_test/test/ComponentTestExample.tsx#L7):

```tsx
valdiIt('can create component', async driver => {
  class MyComponent extends Component {}

  const nodes = driver.render(() => {
    <MyComponent />;
  });

  expect(nodes.length).toBe(1);

  expect(nodes[0].component instanceof MyComponent).toBeTruthy();
});
```

Further down you can see an example of a test that uses the driver to lay out some elements and verifies the resulting frames:

```tsx
valdiIt('can perform layout', async driver => {
  const rootRef = new ElementRef();
  const childRef = new ElementRef();

  driver.render(() => {
    <layout ref={rootRef} width='100%' height='100%' justifyContent='center' alignItems='center'>
      <layout ref={childRef} width={25} height={25} />
    </layout>;
  });

  await driver.performLayout({ width: 100, height: 100 });

  expect(rootRef.single()!.frame).toEqual({
    x: 0,
    y: 0,
    width: 100,
    height: 100,
  });
  expect(childRef.single()!.frame).toEqual({
    x: 37.5,
    y: 37.5,
    width: 25,
    height: 25,
  });
});
```

### UI Tests - How to identify views in tests?

On the TypeScript side you can configure the `accessibiltyId` attribute on any view/label/image element. On iOS this `accessibiltyId` value is applied to the underlying `UIView` and can be retrieved using the usual XCTest APIs. On Android Valdi provides view matcher utilities for finding a reference to a View with a particular accessibilty id.

```xml
<view accessibilityId={this.state.name} />;
```

Ids can also be provided by writing the accessibility ids into an `ids.yaml` file. The ids files will be generated into TypeScript, Kotlin, Swift and Objective-C. Make sure to update [module.yaml](./core-module.md#moduleyaml) to include the new file.

For example, given the following `ids.yaml`:

```yaml:
ids:
  open_page:
    description: Button in the starting demo app page to open sub pages
```

TypeScript definitions:

```ts
export class Ids {
  /**
   * Button in the starting demo app page to open sub pages
   */
  static openPage(): string;
}
```

Android resource file:

```xml
<resources>
  <item type="id" name="yolo:open_page"/>
</resources>
```

Objective-C header and implementation:

```objective-c
#import <Foundation/Foundation.h>

/**
* Button in the starting demo app page to open sub pages
*/
extern NSString *SCValdiIdYoloOpenPage();
```

#### iOS example

<!-- TODO: example for iOS -->

iOS [accessibiltyId example](#todo-example-ios)

#### Android example

<!-- TODO: example for Android -->

[`withAccessibilityId` example](#todo-example-android)

The id itself can be imported using:

```java
import com.snap.valdi.modules.<your_module>.R
```

<!-- TODO: espresso matchers in open source -->

> [!Note]
> Use the Espresso matchers we've provided for working with Valdi _elements_ instead of Android _views_. They're implemented in [`//platform/valdi/test-support`](#todo-espresso-matchers)

For automator performance tests on Android, only `accessibilityId` values that are set through an `ids.yaml` will work

### How to get the Valdi dependencies for tests?

If you're writing a test that requires you to get a reference to a working `SCValdiRuntime`/`ValdiRuntime` to create a view:

- on iOS you can use the `SCValdiServicesForTests()` test utility function available in the `//Features/Valdi/Utilities/SCValdiTestUtilities:SCValdiTestUtilities` module.
- on Android you can use the [`ValdiViewTestRule`](#todo-android-valdiviewtestrule) test utility function
