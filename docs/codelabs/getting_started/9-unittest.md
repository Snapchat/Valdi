# Unit testing
Now that we have some UI, let's make sure that it does what it's supposed to do.

Valdi uses the [Jasmine](https://jasmine.github.io/) testing framework.

## Where tests live
Tests generally live in your module but in their own test directory.

The tests for this codelab will live in `./apps/helloworld/src/valdi/hello_world/test`

Create a test file for our **GettingStarted** Component.

```
cd apps/helloworld/src/valdi/hello_world
mkdir test
touch test/GettingStartedCodelab.spec.tsx
```

This just sets up the structure and creates a file. Open your new file.

## Test structure
Let's create a barebones test file.

Replace the contents of `GettingStartedCodelabTest.tsx` with one test.

```typescript
import 'jasmine/src/jasmine';
import { valdiIt } from 'valdi_test/test/JSXTestUtils';

describe('GettingStartedCodelab', () => {
    valdiIt('creates component', async driver => {
        // Test logic goes here.
        throw new Error('not implemented');
    });
});
```

This imports the framework, sets up a new group of tests called **GettingStartedCodelab** and creates one test called **creates component**. **valdiIt** is a Valdi utility that provides some functionality for rendering components.

It's good practice to keep unit tests small and the names descriptive.

## Update `BUILD.bazel`

Running tests requires the test files to be added to the module. Open `apps/helloworld/src/valdi/hello_world/BUILD.bazel`.

Update the `srcs` to include `test` files:

```bazel
valdi_module(
    name = "hello_world",
    srcs = glob([
        "src/**/*.ts",
        "src/**/*.tsx",
        "test/**/*.ts",
        "test/**/*.tsx",
    ]) + [
        "tsconfig.json",
    ],
```

Upate the `deps` to include the `valdi_test` dependency:

```bazel
    deps = [
        "//src/valdi_modules/src/valdi/valdi_core",
        "//src/valdi_modules/src/valdi/valdi_tsx",
        "//src/valdi_modules/src/valdi/valdi_test",
        "//src/valdi_modules/src/valdi/foundation",
    ],
```

## Run the tests
Let's make sure you can run these new tests. 

```
bazel build //apps/helloworld/src/valdi/hello_world:test
```

The initial build may take some time to pull in the test dependencies.


```
//apps/helloworld/src/valdi/hello_world:test                             FAILED in 0.2s
Executing tests from //apps/helloworld/src/valdi/hello_world:test
-----------------------------------------------------------------------------
Randomized with seed 40852
Started
[0.065: INFO] [JS] - Jasmine execution started (1 specs)
[0.065: INFO] [JS] -------------------------------------
[0.065: INFO] [JS] -- Suite started: GettingStartedCodelab
[0.065: INFO] [JS] ---- Spec started: creates component
F[0.072: INFO] [JS] -- Suite passed: GettingStartedCodelab
[0.072: INFO] [JS] -------------------------------------


-------------------- Failures start:
1) GettingStartedCodelab creates component
  Message:
    Error: Not implemented
```


## Render a layout
Now that we can run these tests, we need to get our test subject loaded up.

Inside of that **creates component** test, load up your Component.

```tsx
describe('GettingStartedCodelab', () => {
    valdiIt('creates component', async driver => {
        const nodes = driver.render(() => {
            <GettingStartedCodelab header='Stuff and things' />;
        });
    });
});
```

This will render your component and give you back a reference to the output. You can use **driver.render** to render any arbitrary TSX similar to `onRender`. This is useful if you want to test how your component layout behaves when inside various sized containers.

Now we can check on that output and make sure it is what we think it is.

```typescript
expect(nodes.length).toBe(1);

expect(nodes[0].component instanceof GettingStartedCodelab).toBeTruthy();
```

Here we're checking that there was only one thing rendered and that that one thing was a **GettingStarteCodelab**.

Save the file and watch the hotreloader script rerun your tests.

## Render a single component and check the header
We only care about our component so there are easier ways to create it for testing.

Create another test and load up a component with **createComponent**.

```typescript
valdiIt('sets header', async driver => {
    const instrumentedComponent = createComponent(GettingStartedCodelab, { header: 'Something' });
    const component = instrumentedComponent.getComponent();
});
```

**createComponent** is another utility that creates an instrumented component and we can get a reference to our component from there.

Let's fetch the `<label>`s so we can make sure they are what we think they are.

```typescript
const labels = elementTypeFind(componentGetElements(component), IRenderedElementViewClass.Label);
```

VSCode might struggle to find the imports for this one, so here they are if you need them.

```typescript
import { IRenderedElementViewClass } from 'valdi_test/test/IRenderedElementViewClass';
import { componentGetElements } from 'foundation/test/util/componentGetElements';
import { elementTypeFind } from 'foundation/test/util/elementTypeFind';
```

**elementTypeFind** finds all of the elements of a particular type. Remember, elements are the natively supported types in Valdi. There is a similar function for Components: **componentTypeFind**. 

So we're filtering through all of the child elements of the **GettingStartedCodelab** component and getting back any that match **IRenderedElementViewClass.Label** which is the native type of `<label>`.

We know, from our implementation of the **GettingStartedCodelab** that we want the first `<label>` so let's check it.

```typescript
const header = labels[0].getAttribute('value');
expect(header).toBe('Something');
```

We grab the first element in the array of labels and check to verify that its label is what we set when we created the object.

Let's change the value and check it again.

```typescript
instrumentedComponent.setViewModel({ header: 'Something else' });
const header2 = labels[0].getAttribute('value');
expect(header2).toBe('Something else');
```

We set a new **ViewModel** on the instrumentedComponent then checked the label again to make sure it is what we think it should be.

## Test button logic
Let's make sure that clicking on the button gives us more kittens.

The setup for this test starts the same way.

```typescript
valdiIt('adds kittens', async driver => {
    const instrumentedComponent = createComponent(GettingStartedCodelab, { header: 'Kittens' });
    const component = instrumentedComponent.getComponent();
});
```

Let's double check that we start out with the right number of kittens.

```typescript
expect(component.state.numKittens).toBe(1);
```

Our button is a view so we want to fetch all of them and then grab the first one.

```typescript
const button = elementTypeFind(componentGetElements(component), IRenderedElementViewClass.View)[0];
```

**onTap** is just another attribute on the element so we can fetch it and then, because it's a function pointer, we can execute it.

```typescript    
button.getAttribute('onTap')?.();
```

The question mark (?) is a null check and that final set of parentheses is the function call.

Now we can check and make sure we have the right number of kittens.

```typescript
expect(component.state.numKittens).toBe(2);
```

Save the file and let the hotreloader run the tests.

## Full solution
Here's the full testing solution if you need it.

```typescript
import 'jasmine/src/jasmine';
import { IRenderedElementViewClass } from 'valdi_test/test/IRenderedElementViewClass';
import { valdiIt, createComponent } from 'valdi_test/test/JSXTestUtils';
import { componentGetElements } from 'foundation/test/util/componentGetElements';
import { elementTypeFind } from 'foundation/test/util/elementTypeFind';
import { GettingStartedCodelab } from 'playground/src/GettingStartedCodelab';

describe('GettingStartedCodelab', () => {
    valdiIt('creates component', async driver => {
        const nodes = driver.render(() => {
        <GettingStartedCodelab header='Stuff and things' />;
        });

        expect(nodes.length).toBe(1);

        expect(nodes[0].component instanceof GettingStartedCodelab).toBeTruthy();
    });

    valdiIt('sets header', async driver => {
        const instrumentedComponent = createComponent(GettingStartedCodelab, { header: 'Something' });
        const component = instrumentedComponent.getComponent();

        const labels = elementTypeFind(componentGetElements(component), IRenderedElementViewClass.Label);

        const header = labels[0].getAttribute('value');
        expect(header).toBe('Something');

        instrumentedComponent.setViewModel({ header: 'Something else' });
        const header2 = labels[0].getAttribute('value');
        expect(header2).toBe('Something else');
    });

    valdiIt('adds kittens', async driver => {
        const instrumentedComponent = createComponent(GettingStartedCodelab, { header: 'Kittens' });
        const component = instrumentedComponent.getComponent();

        expect(component.state.numKittens).toBe(1);

        const button = elementTypeFind(componentGetElements(component), IRenderedElementViewClass.View)[0];

        button.getAttribute('onTap')?.();

        expect(component.state.numKittens).toBe(2);
    });
});
```

# Next 

Dive in to the [documentation](https://github.com/Snapchat/Valdi/tree/main/docs#the-basics) to learn more about how to build your own app.