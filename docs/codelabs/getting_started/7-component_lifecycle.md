# Component lifecycle
Let's take a break from the code for a minute and talk about the **Component** lifecycle.

We touched on this a bit when the previous examples overrode `onCreate` and `onRender`. These functions, and a few others, are called by the **Valdi** runtime to indicate specific parts of the component lifecycle. You can override them in your **Component** to take advantage of that state of the lifecycle.

## onCreate
`onCreate` is called once when the component is first created after the **ViewModel** and **State** have been set but rendering hasn't started yet. This is a good place to setup local variables and kick off any work.

## `onViewModelUpdate(previousViewModel?: ViewModelType)`
This is called when the **ViewModel** is set and when the parent has updated it since the last `onRender` call. It is also called directly after `onCreate` if the parent provided a **ViewModel** at initialization. `onRender` is always called right after this.

## onRender
`onRender` is called when the component needs to be rendered. TSX tags will only have any effect if they are inside the call stack of this function.

There are three scenarios in which onRender is called:
- Right after `onCreate` during setup.
- Right after `onViewModelUpdate` is called.
- In a **StatefulComponent**, right after `setState` is called.

## onDestroy
This is called once when the component is destroyed. A component gets destroyed if it's parent no longer renders it in the parent's `onRender`.

`onDestroy` is a good place to cleanup long running tasks and resources.

### [Next >](./8-component_events.md)
