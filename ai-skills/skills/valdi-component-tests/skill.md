# Write Valdi Component Tests

Write unit tests for a Valdi component using standard Valdi test suite patterns.

## Steps

### 1. Read the source component

Read the component source file to understand:
- What view model properties it accepts
- Which JSX elements have `key` attributes
- What changes based on view model props vs. what's always static
- How "hidden" state is implemented (translationY, opacity, or child component prop)
- Whether the view model contains any **discriminated unions** (type/kind fields)
- Whether any props are **arrays** of items to render

### 2. Add `key` attributes to source elements (only if needed)

Add `key` attributes **only** to elements whose rendering depends on view model properties. Never add keys to static content.

Good candidates for keys:
- The outer container when `hidden` controls `translationY` or `opacity`
- An `<image>` element whose `src` comes from a view model URL prop
- An element inside a `when(condition, ...)` block (to assert presence/absence)
- A badge/overlay that toggles based on a boolean prop
- A spinner shown in a loading/saving state of a union type

Do NOT add keys to:
- Elements with hardcoded asset values (e.g., `src={SIGIcon.xSignStroke}`)
- Label elements with always-present localized strings
- Elements that are always rendered the same regardless of view model

### 3. Determine the test file path

Test files **must mirror the source file hierarchy**. For example:
- Source: `src/categories/CollectionComponent.tsx` → Test: `test/categories/CollectionComponentTest.spec.tsx`
- Source: `src/home_page/OptionPreviewView.tsx` → Test: `test/home_page/OptionPreviewViewTest.spec.tsx`
- Source: `src/MyComponent.tsx` → Test: `test/MyComponentTest.spec.tsx`

### 4. Write the test file

**Imports:**
```tsx
import { MyComponent } from 'my_module/src/MyComponent';
import { componentGetElements } from 'foundation/test/util/componentGetElements';
import { componentTypeFind } from 'foundation/test/util/componentTypeFind';
import { elementKeyFind } from 'foundation/test/util/elementKeyFind';
import { elementTypeFind } from 'foundation/test/util/elementTypeFind';
import { findNodeWithKey } from 'foundation/test/util/findNodeWithKey';
import { tapNodeWithKey } from 'foundation/test/util/tapNodeWithKey';
import 'jasmine/src/jasmine';
import { IRenderedElementViewClass } from 'valdi_test/test/IRenderedElementViewClass';
import { IComponentTestDriver, valdiIt } from 'valdi_test/test/JSXTestUtils';
import { ImageView, View } from 'valdi_tsx/src/NativeTemplateElements';
```

Only import what you actually use. `View` is needed when you call `getAttribute` on a non-image element (e.g., for `onTap`, `onVisibilityChanged`). `ImageView` is needed for `src` attribute access.

**Factory function pattern (always add explicit return type):**
```tsx
const makeViewModel = (): MyComponentViewModel => ({
  imageUrl: 'https://example.com/image.png',
  isVisible: true,
  onTap: fail.bind(null, 'onTap should not be called'),
});
```

Use `fail.bind` for callbacks in the factory default — tests that need to assert on a callback should declare their own spy and pass it explicitly, rather than relying on the factory.

**Render pattern:**
```tsx
const nodes = driver.render(() => {
  <MyComponent {...viewModel} />;
});
```

### 5. Test patterns by assertion type

#### Finding elements
```tsx
// Single-level: component renders native elements directly
elementKeyFind(componentGetElements(nodes[0].component!), 'my-key')[0]

// Cross-boundary: component renders a child component that renders the elements
const inner = componentTypeFind(nodes[0].component as MyComponent, InnerComponent)[0];
elementKeyFind(componentGetElements(inner), 'my-key')[0]

// Typed (for typed attribute access without casting):
elementKeyFind<View>(componentGetElements(nodes[0].component!), 'container')[0]
elementKeyFind<ImageView>(componentGetElements(nodes[0].component!), 'image')[0]

// By element type (e.g. find all Label elements):
// Can pass componentGetElements() result OR an IComponent directly:
const labels = elementTypeFind(componentGetElements(nodes[0].component!), IRenderedElementViewClass.Label);
const labels2 = elementTypeFind(nodes[0].component!, IRenderedElementViewClass.Label); // equivalent
expect(labels[0]?.getAttribute('value')).toBe('Expected text');
```

Use the generic type param to get typed `getAttribute()` results and avoid `@typescript-eslint/no-unsafe-call` errors — prefer this over casting the `getAttribute()` return value.

`elementTypeFind` is useful when elements don't have `key` attributes but you know their type (Label, Image, View, etc.). It returns all elements of that type in render order.

#### Hidden via `translationY`
```tsx
// hidden=false
expect(elementKeyFind<View>(componentGetElements(nodes[0].component!), 'container')[0]?.getAttribute('translationY')).toBe(0);

// hidden=true
expect(elementKeyFind<View>(componentGetElements(nodes[0].component!), 'container')[0]?.getAttribute('translationY')).toBe(850);
```

#### Hidden via `opacity` on native view
```tsx
// visible
expect(elementKeyFind<View>(componentGetElements(nodes[0].component!), 'my-view')[0]?.getAttribute('opacity')).toBe(1);

// hidden
expect(elementKeyFind<View>(componentGetElements(nodes[0].component!), 'my-view')[0]?.getAttribute('opacity')).toBe(0);
```

#### Hidden via `opacity` prop passed to a child Component (component boundary)
```tsx
import { CoreButton } from 'coreui/src/components/button/CoreButton';
const component = nodes[0].component as MyComponent;
const buttons = componentTypeFind(component, CoreButton);
expect(buttons[0].viewModel.opacity).toBe(1); // or 0
```

#### View model URL bound to image `src`
```tsx
expect(elementKeyFind<ImageView>(componentGetElements(nodes[0].component!), 'my-image')[0]?.getAttribute('src')).toBe('https://example.com/image.png');
```

#### Text from view model
```tsx
expect(elementKeyFind(componentGetElements(nodes[0].component!), 'my-label')[0]?.getAttribute('value')).toBe('Expected Text');
```

#### `when()`-conditional element (boolean presence)
```tsx
// condition=true → element exists
expect(elementKeyFind(componentGetElements(nodes[0].component!), 'my-element')[0]).toBeDefined();

// condition=false → element absent
expect(elementKeyFind(componentGetElements(nodes[0].component!), 'my-element')[0]).toBeUndefined();
```

#### `when()`-conditional Component (boolean presence via componentTypeFind)
```tsx
import { BadgeComponent } from 'my_module/src/BadgeComponent';
const component = nodes[0].component as MyComponent;

// condition=true
expect(componentTypeFind(component, BadgeComponent).length).toBe(1);

// condition=false
expect(componentTypeFind(component, BadgeComponent).length).toBe(0);
```

#### Callbacks not expected to be invoked

For callbacks that should never fire in a given test, use `fail.bind(null, '...')` instead of `jasmine.createSpy`. This causes the test to immediately fail with a clear message if the callback is accidentally triggered:

```tsx
<MyComponent
  onSelect={fail.bind(null, 'onSelect should not be called')}
  onLoad={onLoad}
/>
```

`fail.bind(null, 'message')` is a plain function call (not an inline lambda), so it satisfies `jsx-no-lambda`. It is assignable to any callback type since TypeScript allows functions with fewer parameters.

If the same fail callback is used across many tests in the file, extract it to a module-level const to avoid repetition:

```tsx
const failOnSelect = (): void => fail('onSelect should not be called');
```

#### Tap callback (use `tapNodeWithKey` — it's async, always `await` it)

`tapNodeWithKey(component, key, timeoutMs?, intervalMs?)` accepts `IComponent | IRenderedElement`.

```tsx
const onTap = jasmine.createSpy('onTap');
const nodes = driver.render(() => {
  <MyComponent onTap={onTap} />;
});
await tapNodeWithKey(nodes[0].component!, 'my-button');
expect(onTap).toHaveBeenCalled();
```

When you need to find a node without tapping it, use `findNodeWithKey`:
```tsx
import { findNodeWithKey } from 'foundation/test/util/findNodeWithKey';

const node = findNodeWithKey(nodes[0].component!, 'my-button')[0];
expect(node).toBeDefined();
```

When the callback receives arguments (e.g., an index), always assert the exact arguments with `toHaveBeenCalledWith`:
```tsx
const onSelect = jasmine.createSpy('onSelect');
// ... render and trigger ...
expect(onSelect).toHaveBeenCalledWith(1); // not just toHaveBeenCalled()
```

For callbacks invoked via a child component's view model (e.g., through `componentTypeFind`), call the view model method directly:
```tsx
const component = nodes[0].component as MyComponent;
componentTypeFind(component, ItemComponent)[1].viewModel.onTap();
expect(onSelect).toHaveBeenCalledWith(1);
```

#### `onTap` retrieved via `getAttribute` (non-tappable element pattern)
```tsx
// Use elementKeyFind<View> so getAttribute('onTap') is typed — no cast needed
const el = elementKeyFind<View>(componentGetElements(nodes[0].component!), 'my-element')[0];
el?.getAttribute('onTap')?.();
expect(onTap).toHaveBeenCalled();
```

#### `onVisibilityChanged` callback

`View.onVisibilityChanged` signature is `(isVisible: boolean, eventTime: EventTime)` where `EventTime = number`. Always pass both args when invoking:

```tsx
const el = elementKeyFind<View>(componentGetElements(nodes[0].component!), 'container')[0];
el?.getAttribute('onVisibilityChanged')?.(true, 0);
```

**Asserting the callback:** depends on how the component wires the handler:

- If the component **wraps** the viewModel callback (e.g., `onVisibilityChanged={(v) => vm.onVisibilityChanged(v)}`), `toHaveBeenCalledWith(true)` works:
  ```tsx
  expect(onVisibilityChanged).toHaveBeenCalledWith(true);
  ```

- If the component **directly assigns** the viewModel callback (e.g., `onVisibilityChanged={this.viewModel.onVisibilityChanged}`), the spy receives both args. If the spy is typed as `(isVisible: boolean) => void`, `toHaveBeenCalledWith(true, ...)` is a TypeScript error. Check the first arg via `calls`:
  ```tsx
  const spy = viewModel.onVisibilityChanged as jasmine.Spy;
  expect(spy).toHaveBeenCalled();
  expect(spy.calls.mostRecent().args[0]).toBe(true);
  ```
  If the spy is an untyped `jasmine.createSpy()`, you can use `toHaveBeenCalledWith(true, 0)` directly.

### 6. Discriminated union state testing

When a view model contains a discriminated union (e.g., `type: 'LOADING' | 'CONTENT' | 'ERROR'`), **test every branch**. For each state:
1. Assert which elements/components ARE rendered
2. Assert which elements/components from OTHER states are NOT rendered

```tsx
// LOADING state: spinner present, action button absent
valdiIt('Verify spinner is shown in loading state', async driver => {
  const nodes = driver.render(() => {
    <MyComponent button={{ type: ButtonType.LOADING }} />;
  });
  expect(elementKeyFind(componentGetElements(nodes[0].component!), 'loading-spinner')[0]).toBeDefined();
  expect(elementKeyFind(componentGetElements(nodes[0].component!), 'action-button')[0]).toBeUndefined();
});

// CONTENT state: action button present, no spinner
valdiIt('Verify action button is shown in content state', async driver => {
  const nodes = driver.render(() => {
    <MyComponent button={{ type: ButtonType.PURCHASE, price: '$9.99', onTap }} />;
  });
  expect(elementKeyFind(componentGetElements(nodes[0].component!), 'action-button')[0]).toBeDefined();
  expect(elementKeyFind(componentGetElements(nodes[0].component!), 'loading-spinner')[0]).toBeUndefined();
});
```

### 7. Array view model testing

When a component renders a list from an array prop, test three cases:
1. **Empty array** — assert 0 items render
2. **Single item** — assert 1 item renders
3. **Multiple items** — assert item count matches array length

Use `componentTypeFind(component, ItemComponent)` or `elementKeyFind` with indexed keys (e.g., `tile-0`, `tile-1`) to count rendered items.

```tsx
valdiIt('Verify no items render when array is empty', async driver => {
  const emptyOptions: OptionViewModel[] = [];
  const nodes = driver.render(() => {
    <MyComponent items={emptyOptions} />;
  });
  const component = nodes[0].component as MyComponent;
  expect(componentTypeFind(component, ItemComponent).length).toBe(0);
});

valdiIt('Verify item count matches array length', async driver => {
  const threeItems: OptionViewModel[] = [makeItem('a'), makeItem('b'), makeItem('c')];
  const nodes = driver.render(() => {
    <MyComponent items={threeItems} />;
  });
  const component = nodes[0].component as MyComponent;
  expect(componentTypeFind(component, ItemComponent).length).toBe(3);
});
```

Note: Extract array literals to local `const` variables before using in JSX (required by `@snapchat/valdi/jsx-no-lambda`).

### 8. Component boundary traversal

When a component's `onRender()` only renders child components (not native elements), `componentGetElements(component)` returns `[]`. You must get the child component first:

```tsx
import { componentGetElements } from 'foundation/test/util/componentGetElements';
import { componentTypeFind } from 'foundation/test/util/componentTypeFind';
import { elementKeyFind } from 'foundation/test/util/elementKeyFind';

const component = nodes[0].component as OuterComponent;
const inner = componentTypeFind(component, InnerComponent)[0];
const container = elementKeyFind<View>(componentGetElements(inner), 'container')[0];
expect(container?.getAttribute('translationY')).toBe(0);
```

### 9. Extract render and find helpers for readability

For larger test files, extract repeated render + find logic into helper functions. This keeps individual tests focused:

```tsx
// Extract rendering
const renderComponent = (driver: IComponentTestDriver, overrides?: Partial<MyComponentViewModel>) => {
  const vm = { ...makeViewModel(), ...overrides };
  return driver.render(() => { <MyComponent {...vm} />; })[0].component as MyComponent;
};

// Extract finding
const getImage = (component: MyComponent) =>
  elementKeyFind<ImageView>(componentGetElements(component), 'image')[0];

// Tests become clean:
valdiIt('Verify imageUrl is bound', async driver => {
  expect(getImage(renderComponent(driver))?.getAttribute('src')).toBe('https://example.com/image.png');
});
```

### 10. Lint rules to follow

- **`explicit-function-return-type`**: Always add explicit return types to factory functions: `const makeViewModel = (): MyViewModel => ({...})`
- **`jsx-no-lambda`**: Never assign inline array literals directly in JSX props. Extract to a local `const` first:
  ```tsx
  // WRONG
  <MyComponent items={[makeItem('a')]} />;

  // CORRECT
  const items: ItemViewModel[] = [makeItem('a')];
  <MyComponent items={items} />;
  ```
- **`no-unsafe-call`**: Use the generic type parameter on `elementKeyFind<T>` to get typed `getAttribute()` results rather than casting: `elementKeyFind<View>(...)` gives typed access to `onTap`, `onVisibilityChanged`, etc.
- **`import/order`**: Keep imports sorted alphabetically by path.

### 11. Key principle

**Only assert on things that change based on view model props.** Every test should have a clear "when X is Y, then Z" story. If the UI looks the same regardless of the prop, skip it.

For union types, a test that only verifies "the component renders without error" in a given state is not sufficient — assert the meaningful structural difference that state introduces.

## Example test file structure

```tsx
import { MyComponent } from 'my_module/src/MyComponent';
import { MyComponentViewModel } from 'my_module/src/MyComponentViewModel';
import { ChildComponent } from 'my_module/src/ChildComponent';
import { componentGetElements } from 'foundation/test/util/componentGetElements';
import { componentTypeFind } from 'foundation/test/util/componentTypeFind';
import { elementKeyFind } from 'foundation/test/util/elementKeyFind';
import { tapNodeWithKey } from 'foundation/test/util/tapNodeWithKey';
import 'jasmine/src/jasmine';
import { IComponentTestDriver, valdiIt } from 'valdi_test/test/JSXTestUtils';
import { ImageView, View } from 'valdi_tsx/src/NativeTemplateElements';
// elementTypeFind + IRenderedElementViewClass for finding elements by type (no key needed):
// import { elementTypeFind } from 'foundation/test/util/elementTypeFind';
// import { IRenderedElementViewClass } from 'valdi_test/test/IRenderedElementViewClass';

const makeViewModel = (): MyComponentViewModel => ({
  imageUrl: 'https://example.com/image.png',
  isVisible: true,
  onTap: fail.bind(null, 'onTap should not be called'),
});

describe('MyComponentTest', () => {
  valdiIt('Verify visible when isVisible is true', async driver => {
    const nodes = driver.render(() => {
      <MyComponent {...makeViewModel()} />;
    });
    expect(elementKeyFind<View>(componentGetElements(nodes[0].component!), 'container')[0]?.getAttribute('opacity')).toBe(1);
  });

  valdiIt('Verify hidden when isVisible is false', async driver => {
    const nodes = driver.render(() => {
      <MyComponent {...{ ...makeViewModel(), isVisible: false }} />;
    });
    expect(elementKeyFind<View>(componentGetElements(nodes[0].component!), 'container')[0]?.getAttribute('opacity')).toBe(0);
  });

  valdiIt('Verify imageUrl is bound to image src', async driver => {
    const nodes = driver.render(() => {
      <MyComponent {...makeViewModel()} />;
    });
    expect(elementKeyFind<ImageView>(componentGetElements(nodes[0].component!), 'image')[0]?.getAttribute('src')).toBe('https://example.com/image.png');
  });

  valdiIt('Verify onTap is called when tapped', async driver => {
    const onTap = jasmine.createSpy('onTap');
    const nodes = driver.render(() => {
      <MyComponent {...{ ...makeViewModel(), onTap }} />;
    });
    await tapNodeWithKey(nodes[0].component!, 'button');
    expect(onTap).toHaveBeenCalled();
  });

  valdiIt('Verify ChildComponent is present when condition is true', async driver => {
    const nodes = driver.render(() => {
      <MyComponent {...{ ...makeViewModel(), showChild: true }} />;
    });
    const component = nodes[0].component as MyComponent;
    expect(componentTypeFind(component, ChildComponent).length).toBe(1);
  });

  valdiIt('Verify ChildComponent is absent when condition is false', async driver => {
    const nodes = driver.render(() => {
      <MyComponent {...{ ...makeViewModel(), showChild: false }} />;
    });
    const component = nodes[0].component as MyComponent;
    expect(componentTypeFind(component, ChildComponent).length).toBe(0);
  });
});
```
