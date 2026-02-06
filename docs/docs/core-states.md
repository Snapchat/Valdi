# Component State

Components have a unidirectional data flow model, similar to React. 

Data flows from the parent component down to the children and changes are tracked to prevent unnecessary render thrashing. This is handled by passing view models 
and attributes down to children, which can then respond to the changes.


## Unidirectional Data Flow

Consumers can send a view model to a child using the view model shorthand syntax and the receiving component uses the view model to configure the view tree.

The view model is an [immutable](https://en.wikipedia.org/wiki/Immutable_object) object, so you can't modify it in a component or programmatically, it can only be provided in the `onRender()` function of the parent component.

If we need a component to update it's own visual state itself we'll need to use the "State" API.
([more info below on how to use the setState API](#state)).

Let's build a contrived label that takes a color and text property as its view model.

```tsx
import { Component } from 'valdi_core/src/Component';

export interface ColoredLabelViewModel {
  text: string;
  color: string;
}

export class ColoredLabel extends Component<ColoredLabelViewModel> {
  onRender() {
    <label color={this.viewModel.color} value={this.viewModel.text} />;
  }
}
```

To update or render the label, the parent simply specify the view-model for this label.

```tsx
export class HelloWorld extends Component {
  onRender() {
    <layout padding={30}>
      <ColoredLabel color='#FF0000' text='HelloWorld' />;
    </layout>;
  }
}

```

![Screeshot of a component rendering a value provided to its view model](./assets/core-states/IMG_1449.jpg)

## State

**As a best effort rule of thumb, only views that have semantically changed are re-rendered. Components are re-rendered if any top-level properties in their view model changed since the last render from a parent.**

Often times, Components need to track their own state that generally causes a re-render.

To do this, there is state management built into each Component as a special object called `state`.

In other words, state is similar to a view model, except it's private to the Component while view models are [immutable](https://en.wikipedia.org/wiki/Immutable_object) data models known to external users of a given component.

State is also an [immutable](https://en.wikipedia.org/wiki/Immutable_object) object, but you can modify it by using `this.setState({ .. })` API which will cause your component to be re-rendered with that new state. 

> [!Note]
> Under the hood, `setState()` uses the `Renderer.renderComponent()` API. This allows Valdi to re-render only the specific component that changed, rather than the entire tree, ensuring high performance for state updates.


```tsx
// Import the StatefulComponent
import { StatefulComponent } from 'valdi_core/src/Component';

// ViewModel + State interfaces for component
export interface TimerViewModel {
  loop: number;
}
interface TimerState {
  elapsed: number;
}
// Component class
export class Timer extends StatefulComponent<TimerViewModel, TimerState> {
  // Initialize the state
  state = {
    elapsed: 0,
  };
  // When creating the component, start a periodic logic
  private interval?: number;

  // Initialize the setInterval that will update state once a second incrementing
  // the `elapsed` state value.
  onCreate() {
    this.interval = setInterval(() => {
      // Increment the state to trigger a re-render periodically
      this.setState({
        elapsed: (this.state.elapsed + 1) % this.viewModel.loop,
      });
    }, 1000);
  }

  // When component is removed, make sure to cleanup interval logic
  onDestroy() {
    if (this.interval) {
      clearInterval(this.interval);
    }
  }
  // Render visuals will depend both on the state and the view model
  onRender() {
    <view padding={30} backgroundColor='lightblue'>
      <label value={`Time Elapsed: ${this.state.elapsed} seconds`} />;
      <label value={`Time Looping every: ${this.viewModel.loop} seconds`} />;
    </view>;
  }
}
```

This component can then be used like any other component:

```tsx
export class HelloWorld extends Component {
  onRender() {
    <Timer loop={10} />;
  }
}
```

![Component rendering a value provided by its state](./assets/core-states/IMG_1450.jpg)
