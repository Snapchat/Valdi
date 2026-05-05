# How `onRender()` Works

Every Valdi component implements `onRender(): void`. The return type is `void` — not `JSX.Element`, not `null`, not anything. This is one of the most fundamental differences between Valdi and React, and it shapes how you write every component.

## JSX as side-effects, not return values

In React, JSX is an expression that produces a value. `render()` returns a tree of `React.Element` objects and React's reconciler compares new trees to old ones to compute a minimal diff.

In Valdi, JSX tags are **imperative calls to the native renderer**. When the Valdi runtime calls your `onRender()`, it opens a render transaction. Each JSX tag — `<view>`, `<label>`, `<MyComponent />` — immediately emits a native instruction into that transaction. When `onRender()` returns, the transaction is committed.

There is no virtual DOM. There is no tree to diff. There is no return value because the output has already been emitted.

```tsx
// React — JSX produces a value that is returned
render(): JSX.Element {
  return (
    <view>
      <label value="hello" />
    </view>
  );
}

// Valdi — JSX emits native calls; nothing is returned
onRender(): void {
  <view>
    <label value="hello" />
  </view>;
}
```

## Common mistakes

### ❌ Using `return`

```tsx
// WRONG — nothing is returned, this is silently discarded
onRender(): void {
  return <view><label value="hello" /></view>;
}
```

TypeScript will catch this if your return type annotation is `void`, but the real problem is that `return` exits `onRender()` before the JSX is evaluated. The native renderer never receives the instruction.

```tsx
// CORRECT
onRender(): void {
  <view><label value="hello" /></view>;
}
```

### ❌ Storing JSX in a variable

```tsx
// WRONG — the <label> is emitted into the tree when the variable is assigned,
// not when it's "used" — it ends up in the wrong place
onRender(): void {
  const title = <label value={this.viewModel.title} />;
  <view>
    {title}  // this does NOT insert the label here
  </view>;
}
```

Because JSX emits at the point it is evaluated, storing a JSX expression in a variable and using it later does not work the way it does in React. The `<label>` is emitted at the assignment site, not at the usage site.

```tsx
// CORRECT — call a method that emits at the right time
onRender(): void {
  <view>
    {this.renderTitle()}
  </view>;
}

private renderTitle(): void {
  <label value={this.viewModel.title} />;
}
```

### ❌ Passing JSX as a prop

```tsx
// WRONG — the <label> is emitted into the wrong location in the tree
<MyComponent header={<label value="Title" />} />
```

JSX cannot be passed as a value. Instead, pass a function (sometimes called a "render prop") that the child calls at the correct time:

```tsx
// CORRECT — write children as inline JSX; the compiler wraps them in a render function
interface MyComponentViewModel {
  children?: () => void;
}

// Parent
class ParentComponent extends Component<ParentViewModel> {
  onRender(): void {
    <MyComponent>
      <label value="Title" />;
    </MyComponent>;
  }
}

// Child — call this.viewModel.children() to render the slot
class MyComponent extends Component<MyComponentViewModel> {
  onRender(): void {
    <view>
      {this.viewModel.children?.()}
    </view>;
  }
}
```

When the child component needs to pass a value back into the render function (for example, a measured size), use `$slot()` to pass a parametric callback:

```tsx
import { $slot } from 'valdi_core/src/CompilerIntrinsics';

interface MeasuredViewModel {
  children?: (frame: { width: number; height: number }) => void;
}

class ParentComponent extends Component<ParentViewModel> {
  onRender(): void {
    <MeasuredView>
      {$slot(({ width, height }) => {
        <view height={Math.min(width, height)} width={Math.min(width, height)} />;
      })}
    </MeasuredView>;
  }
}
```

### ❌ Using `Array.map()` to render lists

```tsx
// WRONG — map() returns an array; the return value is discarded
onRender(): void {
  <scroll>
    {this.viewModel.items.map(item => <Row data={item} />)}
  </scroll>;
}
```

`map()` collects return values into an array. But JSX in Valdi doesn't return — it emits. The `<Row>` elements are emitted immediately, then `map()` returns an array of `void`s which is silently discarded, leaving the `<scroll>` empty.

```tsx
// CORRECT — forEach emits at the right time
onRender(): void {
  <scroll>
    {this.viewModel.items.forEach(item => {
      <Row data={item} />;
    })}
  </scroll>;
}
```

## Why there is no `export default`

A common source of confusion when starting with Valdi: components are not exported as default exports.

In React, a component file typically ends with `export default MyComponent`. In Valdi, components are referenced by class identity — the host app loads modules by their compiled `.valdimodule` archive, and the Valdi runtime instantiates the root component class directly. There is no module registry that needs a default export.

```tsx
// React
export default class MyComponent extends React.Component<Props> { ... }

// Valdi — no default export needed; class identity is the reference
export class MyComponent extends Component<ViewModel> { ... }
```

If you're using `EntryPointComponent` to register a component as a module entry point, class identity is sufficient — no default export required.

## Performance implications

Because there is no virtual DOM, Valdi's render path is direct:

- `onRender()` is called
- Native view instructions are serialized into a `RenderRequest` buffer as side-effects
- At the end of the render pass, the `RenderRequest` is submitted to the C++ runtime in one batch

Valdi uses a **fine-grained reactivity model**: only the components whose `ViewModel` or `State` changed are re-rendered. A component re-render does not trigger its subtree unless child components also receive updated `ViewModel`s.

This means:
- Creating inline lambdas or new objects inside `onRender()` can cause unnecessary child re-renders (changed prop identity even if the value is semantically the same)
- `new Style({...})` inside `onRender()` defeats style interning — define styles at module level
- Methods like `renderTitle()` are fine since they emit, not return

> **See also:** [Performance Optimization](./performance-optimization.md), [Component States](./core-states.md), [The Mighty Component](./core-component.md)
