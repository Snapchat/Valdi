# The Mighty Component

## Why Component?

"Component" is the basic building block for all valdi UIs.

There are two basic entities used by Valdi:
- Valdi `elements` which are basic native elements such as `<image>` or `<view>`
- Valdi `components` which are implemented in TypeScript and contain business logic.

Valdi elements have lowercase naming and are always included by the valdi runtime.

Additionally, it is possible to directly include a native view using the [`<custom-view>`](./native-customviews.md) tag.

To simplify:
- Valdi `elements` are implemented by the native code
- Valdi `components` are implemented in TypeScript (cross-platform)

We can define TypeScript components that use both elements and other sub-components. A typical Valdi feature has a root component which calls sub-components who themselves use native elements.

## Native Elements

| Element | Android View | iOS View | Description |
| ------- | ------------ | ---------|-------------|
| `<view>` | `ViewGroup`       | `UIView`      | A simple view that can have layout properties (ie: flexbox) and view properties (ie: background color, touch events, etc.)  |
| `<layout>` | N/A       | N/A      | Supports layout properties such as flexbox but does not support view properties. Use a `<layout>` instead of `<view>` whenever possible, since these nodes will not be backed by a platform view when rendering |
| `<label>` | `TextView`       | `UITextView`      | For displaying text |
| `<textfield>` | `EditText`       | `UITextField`      | For editing single line text |
| `<textview>` | `EditText`       | `UITextView`      | For editing multi-line text |
| `<image>` | `ValdiImageView`       | `UIImageView`      | Renders an image |
| `<video>` | `ValdiVideoView`       | `SCValdiVideoView`      | Renders a video |
| `<scroll>` | `ValdiScrollView`       | `SCValdiScrollView`      | A container of views that can scroll either vertically or horizontally  |
| `<blur>` | N/A       |    `UIVisualEffectView`   | (iOS only) A container that blurs the background  |
| `<shape>` | `View`       |   `UIView`    | Supports drawing shapes in a view |

## Example Component

```tsx
/**
 * Make sure to import the component parent class
 */
import { Component } from 'valdi_core/src/Component';
/**
 * We can (optionally) define an interface that declares all the components parameters
 * Any parent component that uses this component will be able to modify those values
 * Any change in those values will trigger a render of the component
 */
export interface ReusableComponentViewModel {
  myParam: string;
  myOtherParam: number;
}
/**
 * This is a typical class that defines the component logic
 * Note that it has an optional generic type for its view model
 */
export class ReusableComponent extends Component<ReusableComponentViewModel> {
  onCreate() {
    // Called when the component is created in the render-tree
    // Used to prepare some resources, precomputed values or subscribe to observables
  }
  onDestroy() {
    // Called when the component is no longer in the render-tree
    // Used to dispose of resources and observables
  }
  onViewModelUpdate(previous?: ReusableComponentViewModel) {
    // Called every time the view model changes
    // Used to trigger some computations, typically used for asynchronous operations,
    // that would then impact the internal component state
  }
  onRender() {
    // Called every time the component needs to be rendered
    // Used to define child components and child render-tree
    // Can access "this.viewModel" that contains the current component parameters values
    const myVariable = 20;
    <view backgroundColor='lightblue' padding={myVariable}>
      <label value={'myParam: ' + this.viewModel.myParam} />
      <label value={'myOtherParam: ' + this.viewModel.myOtherParam.toString()} />
    </view>;
  }
}
```

An example how to use the above component would be:

```tsx
export class HelloWorld extends Component {
  onRender() {
    <ReusableComponent myParam='Hello this the parameter' myOtherParam={42} />;
  }
}
```

![''](./assets/core-component/IMG_1448.jpg)


> [!Note]
>  We are using the component **from** within a component.


## Lifecycle
Each component has several lifecycle methods that can be overridden to run code at particular times in the application process.

```ts
onCreate()
```
Called when the component is first created and its attributes, view model, and state have been set, but it has not yet been rendered. The component will be rendered right after the call.

```ts
onViewModelUpdate(previousViewModel?: ViewModelType)
```
Called when the `viewModel` of the component is set and updated. Also called directly after `onCreate()` if a view model was provided at initialization. The component will be rendered right after the call.

Note: if you need to update your state as part of a view model change call `setState()` with the new partial state directly.

```ts
onRender()
```
Called when the component is rendering.
TSX tags will only have an effect INSIDE of the call stack of an onRender() function

```ts
onDestroy()
```
Called when the component was destroyed

```ts
onError(error: Error)
```
Called when a render error occurs in a child component. This allows the component to act as an **Error Boundary**, catching exceptions and potentially rendering a fallback UI instead of crashing the entire tree.

```ts
registerDisposable(disposable: (() => void) | Unsubscribable)
```
Registers a function or unsubscribable (like an RxJS Subscription) which will be called automatically when the component is destroyed. This is useful for cleaning up subscriptions, timers, or other resources.

**Example with RxJS:**
```tsx
import { Component } from 'valdi_core/src/Component';
import { interval } from 'valdi_rxjs/src/interval';

export class MyComponent extends Component {
  onCreate() {
    const subscription = interval(1000).subscribe(() => {
      console.log('Tick');
    });
    
    // Subscription will be automatically cleaned up when component is destroyed
    this.registerDisposable(subscription);
  }
}
```

**Example with custom cleanup:**
```tsx
export class MyComponent extends Component {
  onCreate() {
    const timerId = setInterval(() => {
      console.log('Timer fired');
    }, 1000);
    
    // Clear timer when component is destroyed
    this.registerDisposable(() => {
      clearInterval(timerId);
    });
  }
}
```

> **See also:** [RxJS Integration](./client-libraries-rxjs.md) for more examples of subscription management
