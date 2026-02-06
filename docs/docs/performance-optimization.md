# Optimization

Valdi was designed for high performance from the ground up. There are a few key points that makes it fast:
- It avoids allocations on render when possible.
- It avoids marshalling between JS and native code as much as possible.
- Component's rendering are local, a component can re-render without triggering a render from its parent. Additionally, in cascading renders between parent to a nested child, the renderer can skip ancestors in between them in many cases. This allows for quick incremental updates.
- Child Components are only re-rendered whenever their view model changed.
- Native views are re-used across all components.
- Logic which runs in the iOS/Android main thread is all C++ code.

This document covers some additional features that can be used to keep the view inflation fast.

## Layout nodes

Sometimes, you may use a `view` to center or align elements. In that case, you can use a `layout` node instead. It works like a `view`, but won't be backed by a native view. As such it cannot display anything on screen and cannot receive touches, it is just used for laying out its children. `layout` nodes are cheaper than `view` to create, destroy and render.

> **See also:** [`<layout>` and `<view>`](./core-views.md) for complete documentation on layout and view elements.

```tsx
<view>
   <layout>
      <label />
      <label />
   </layout>
</view>
```

## Optimizing rendering

Valdi will render your Component in the following cases:
- Your Component's state changed
- Your Component's view model instance changed
- You explicitly called `render()` or `animate()`

Renders in Valdi are a lot cheaper than React, but they still have a cost that can add up if many components need to be rendered. As such, you should try to make sure the framework only render when something actually changed. Here are a few common pitfalls and how to fix them.

### View model always changing

```tsx
class Parent extends Component {
  onRender() {
    <Child items={['Item1', 'Item2']}/>
  }
}
```
This code creates a new view model for `Child`, every time `Parent` renders. This is because the `items` view model property in this example is an array that gets created on every render. This means that the child will always re-render whenever the parent renders. You can solve this by creating the array once and passing it to the child component:

```tsx
class Parent extends Component {
  items = ['Item1', 'Item2'];

  onRender() {
    <Child items={this.items}/>
  }
}
```

If the view model property you need to pass to your child component depends on a property passed from a parent, you can leverage the `onViewModelUpdate()` callback to compute and store the fields to pass down to your child components once:

> **See also:** [Component Lifecycle](./core-component.md#lifecycle) for details on `onViewModelUpdate()` and other lifecycle methods.

```tsx
interface ViewModel {
  numberOfItems: number;
}

class Parent extends Component<ViewModel> {
  items = [];

  onViewModelUpdate() {
    if (this.items.length !== this.viewModel.numberOfItems) {
      this.items = [];
      for (let i = 0; i < this.viewModel.numberOfItems; i++) {
        this.items.push(`Item${i + 1}`);
      }
    }
  }

  onRender() {
    <Child items={this.items}/>
  }
}
```

This solution will ensure that `Child` will re-render only when necessary, making incremental updates faster.

### Using callbacks in elements

> **See also:** [Component Events](./core-events.md) for more on handling touch events and callbacks.

**❌ BAD - Don't do this:**

```tsx
class MyComponent extends Component {
  onRender() {
    <view onTap={() => {
      // Handle click
    }}/>
  }
}
```
This code sets the `onTap` attribute to an inline lambda. The issue with that code is that whenever the Component renders, a callback function will be created for the `onTap` every time. Functions in TypeScript cannot be compared by value, they are just compared by reference. As such, the framework cannot know if the function has actually changed or not, unless the reference stays the same. This means that for every render, the diff algorithm will detect a change for the `onTap` attribute which triggers an update of the tap handler to the backing view element. The solution is to store the callback as a lambda on the component itself, and pass down the created lambda:

**✅ GOOD - Do this instead:**

```tsx
class MyComponent extends Component {
  onRender() {
    <view onTap={this.onTap}/>
  }

  private onTap = () => {
    // Handle click
  }
}
```


### Component Keys

To correctly identify which elements have been been removed/added/moved between renders the framework has a concept of _keys_. In cases where there are multiple siblings of the same node type the `key` attribute is used to determine the identity of an element. If you don't provide a key manually, the framework assigns one automatically. This works fine for shorter lists of views that, but it's a good idea to provide the `key` yourself, especially since it's trivial as the data backing the elements in your list usually has some sort of unique identifier. For cases such as rearranging arrays or drag and drop, the items can remain consistently rendered instead of destroyed and recreated.

```tsx
class MyComponent extends Component {
  onRender() {
    <view>
      <view key="1" />
      <view key="2" />
    </view>
  }
}
```

## Advanced Optimizations

### Bypassed Rendering
Valdi avoids unnecessary work by skipping the `onRender` call of any component whose `viewModel` hasn't changed. This prevents updates from cascading through the entire tree.

### Slot Re-rendering
Valdi can re-render a slot's content without re-rendering the component that contains the slot. This is particularly useful for high-level layout components. See [Renderer Internals](./internals-renderer.md) for details.

### Lazy Module Loading
Valdi modules are loaded lazily using ES6 Proxies. Code is only evaluated when it's actually used, which keeps cold start times fast even as your codebase grows.

### Interned Strings and Styles
The bridge between TypeScript and C++ is optimized using interning:
- **Strings**: Common strings (attribute names, view classes) are interned as integers.
- **Styles**: `Style<>` objects are interned on first use. Reusing style objects in your code allows Valdi to pass a single ID across the bridge instead of a full dictionary of properties.
