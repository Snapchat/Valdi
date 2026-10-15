# Declarative rendering
In an imperative UI (traditional Android and iOS), you create the UI once. If you want to make changes to the UI, you modify the views that are already there.

Valdi is a declarative UI framework. In declarative UIs, you don't manage the UI yourself, you create a declaration of what the UI should look like based on some current state and the framework handles the changes to get the UI there from its previous layout.

Valdi components override the **`onRender`** function. This function runs every time the UI needs to update.

In our example **`GettingStartedComponent`**, update the **`value`** of the first **`<label>`** to `'Declarative codelab'` in the `onRender` function.

```typescript
onRender() {
    <layout padding={24} paddingTop={80}>
        <label value='DeclarativeCodelab!' font={systemBoldFont(16)} />
        <label value='github.com/Snapchat/Valdi' font={systemFont(12)} />
    </layout>;
}
```

When you save the file, the hotreloader does its thing, compiles the TypeScript, and rebuilds the page.

Valdi does a lot of work under the hood to optimize UIs and changes in UIs, but you should avoid doing any significant work in the **`onRender`** function.

## TSX/JSX
These `<layout>` and `<label>` tags may look like HTML, but they are actually TSX.

TSX is an embeddable XML shaped syntax that is transformed into valid TypeScript by our toolchain. In Valdi, you can only use TSX within the call stack of **`onRender`**; that means inside of **`onRender`** or functions called from inside **`onRender`**.

You can read more about TSX and the JavaScript counterpart, JSX, in the official [TypeScript documentation](https://www.typescriptlang.org/docs/handbook/jsx.html)

## Valdi native elements
Valdi provides a handful of native elements that map to a native implementation. These can be thought of as the primitives of a UI.

`<layout>` doesn't do any rendering on its own, instead it is used to group child elements together and specify how they should be laid out.

`<label>` is a text label that displays text.

`<view>` is a simple rectangular container that can have a background color, border, and respond to gesture events. It's not used much on its own.

`<image>`, `<spinner>`, and `<scroll>` should be self-explanatory.

You can even define your own native views, but that's a subject for a different codelab.

> **Want more details?** See the [API Reference](../../api/api-reference-elements.md) for a complete list of all available elements and their properties.

## Properties
We're not restricted to strings and constants in UI rendering, you can also define your own properties.

Define a new `msg` property inside your component.

```typescript
export class GettingStartedCodelab extends Component {
    private msg?: string;  
```

If you're not familiar with TypeScript or you need a refresher, this is a new private property called **`msg`** of type **`string`**. The question mark `?` indicates that this is an [**optional** property](https://www.typescriptlang.org/docs/handbook/2/objects.html#optional-properties) and may be **`undefined`**.

Now implement the `onCreate` function inside the component and assign a value to the `msg` property.

```typescript
onCreate() {
    this.msg = 'Hello Valdi on ' + new Date().toLocaleString();
}
```

`onCreate` is part of a Component's lifecycle and is called once when the component is created.

Update `<label>` in the `onRender` function to consume your new variable.

```typescript
onRender() {
    <layout>
        <label value='Declarative codelab' font={systemBoldFont(16)} />
        <label value={this.msg} font={systemFont(12)} />
    </layout>;
}
```

The curly braces are required to access the variable within the TSX.

## Local variables
You can also define local variables.

Update **msg** in the `onCreate` function to only contain the first part of your message.

```typescript
onCreate() {
    this.msg = 'Hello Valdi on ';
}
```

Create a local variable in the `onRender` function to build your full message string.

```typescript
onRender() {
    const fullMsg = this.msg + new Date().toLocaleString();
    // ...
```

Then update the label value to use the new variable.

```typescript
onRender() {
    const fullMsg = this.msg + new Date().toLocaleTimeString();
    <layout>
        <label value='Declarative codelab' font={systemBoldFont(16)} />
        <label value={fullMsg} font={systemFont(12)} />
    </layout>;
}
```

**fullMsg** will be created every time `onRender` runs and will update the date.

### [Next >](./5-control_flow_loops.md)