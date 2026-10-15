# Component state
Up until now, we've been playing with local variables and relying on the hotreloader to rerender our components. But when you've got a component running in production, how does the view update?

Valdi will automatically rerender a component when specific kinds of state change. Components have two main kinds of state: the **ViewModel**, and the appropriately named **State**.

## ViewModel and unidirectional data flow
In Valdi, data propagates down the tree; from parent to child. The way that parents pass data down to children is through the **ViewModel**.

To the child, the **ViewModel** is immutable; you can't modify it, only read the variables. When the parent updates the child's **ViewModel**, the child can get the new values in the next `onRender` or in a lifecycle callback `onViewModelUpdate`.

Let's build a **ViewModel** for our component.

**ViewModels** are defined as interfaces and specify a generic type on the `Component`.

Define your **ViewModel** just below the imports, outside of the `GettingStartedCodelab` component definition.

```typescript
export interface GettingStartedCodeLabViewModel {
    header: string;
}
```

The recommended naming convention for these objects is **`NameOfComponentViewModel`**.

Then specify the **ViewModel** type on the **Component**.

```typescript
export class GettingStartedCodelab extends Component<GettingStartedCodeLabViewModel> {
```

Then in our `onRender` function, we can consume the new **ViewModel**.

```typescript
onRender() {
    const fullMsg = this.msg + new Date().toLocaleTimeString();
    <layout>
        <label value={this.viewModel.header} font={systemBoldFont(16)} />
        {this.onRenderMessage(fullMsg)}
    </layout>;
}
```

When the hotreloader refreshes, you'll either see an error or a missing header. This is because we didn't define the **`header`** parameter where the `GettingStartedCodelab` component was added in its parent.

Open [`HelloWorldApp.tsx`](../../../apps/helloworld/src/valdi/hello_world/src/HelloWorldApp.tsx) and find where the `<GettingStartedCodelab>` component is included in the `onRender` function.

VSCode should give you some angry red lines and when you hover over the error it'll tell you `"Property 'header' is missing in type '{}' but required in type 'Readonly<GettingStartedCodeLabViewModel>'"`

This is happening because the parent is not passing the **`header`** value to the **ViewModel** of the child.

We can fix this by adding a value to the **`header`** tag.

```typescript
<GettingStartedCodelab header='State codelab'></GettingStartedCodelab>
```

When the hotreloader refreshes the UI, your new header should be there.

If you want **ViewModel** variables that aren't always required, make them optional in the **ViewModel** interface definition.

```typescript
export interface GettingStartedCodeLabViewModel {
    header: string;
    numKittens?: number;
}
```

## State
**State** objects are used to track a **Component**'s internal state. Like **ViewModels**, **State** objects are immutable. But unlike **ViewModels**, they can be modified with the component's `setState()` function, which will trigger the component to re-render.

Define a **State** interface for your component just after the **ViewModel**.

```typescript
interface GettingStartedCodeLabState {
    elapsed: number;
    kitten: boolean;
}
```

The **State** interface does not need to be **exported** because it's not going to be used outside of this file.

We have a special component that handles **State**. Update your component to extend the **StatefulComponent** and specify the **State** interface as a generic.

```typescript
import { StatefulComponent } from "valdi_core/src/Component";

/**
 * @Component
 */
export class GettingStartedCodelab extends StatefulComponent<
    GettingStartedCodeLabViewModel,
    GettingStartedCodeLabState
> {
```

Then we can initialize the **state** variable.

```typescript
/**
 * @Component
 */
export class GettingStartedCodelab extends StatefulComponent<
    GettingStartedCodeLabViewModel,
    GettingStartedCodeLabState
> {
    state = {
        elapsed: 0,
        kitten: false,
    };
```

Now, let's do something with this **`elapsed`** state property. Set up a timer in `onCreate` and an `interval` property to hold on to a reference to it.

```typescript
private interval?: number;
onCreate() {
    this.msg = 'Hello valdi on ';
    this.kitten = true;

    this.interval = setInterval(() => {
        this.setState({ elapsed: this.state.elapsed + 1 });
    }, 1000);
}
```

`setInterval` is a JavaScript utility that calls a function, repeatedly, after a set delay.

The first parameter is the function to be called, we're creating a lambda that calls `setState` to increment **`elapsed`**. `setState` does a partial update, so we're not replacing the whole object here, we're just updating **elapsed**, the value of **kitten** will remain the same.

The second parameter is a length of time, specified in milliseconds.

`setInterval` will run forever, so we need to clean it up when the component gets destroyed.

```typescript
onDestroy() {
    if (this.interval) {
        clearInterval(this.interval);
    }
}
```

`clearInterval` clears the interval set in `setInterval`, and `onDestroy` is a **Component** lifecycle function that will be called as the **Component** is being destroyed.

Now, let's read this new **State** variable that we've created and are updating. Add another `<label>` in `onRender`.

```typescript
onRender() {
    const fullMsg = this.msg + new Date().toLocaleTimeString();
    <layout>
        <label value={this.viewModel.header} font={systemBoldFont(16)} />
        <label value={`Time Elapsed: ${this.state.elapsed} seconds`} />
        {this.onRenderMessage(fullMsg)}
    </layout>;
}
```

The value of our new `<label>` is a template literal string: a formatted string. Pay attention to the quotes here, they're not quotes, they're back ticks.

Let the hotreloader do its thing and then watch the timer tick up.

Let's play with that other **State** variable we created. Update the `setInterval` function to update the value of **state.kitten** based on `elapsed % 5`.

```typescript
this.interval = setInterval(() => {
    this.setState({
        elapsed: this.state.elapsed + 1,
        kitten: this.state.elapsed % 5 == 0,
    });
}, 1000);
```

This sets **kitten** to **true** when a multiple of 5 seconds have elapsed.

Then we can consume **state.kitten** in `onRenderMessage` and clean up the kitten local variable.

```typescript
onRenderMessage(message: string) {
    if (this.state.kitten) {
        <image height={50} width={50} src='https://placecats.com/300/300' />;
    } else {
        <label value={message} font={systemFont(12)} />;
    }
}
```

Take a look at the UI, you should see the hello message some of the time and a kitten every 5 seconds.

### [Next >](./7-component_lifecycle.md)
