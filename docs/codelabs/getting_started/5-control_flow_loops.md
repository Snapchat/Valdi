# Control flow and loops
Before we start, your component should look something like this.

```typescript
import { Component } from "valdi_core/src/Component";
import { systemBoldFont, systemFont } from "valdi_core/src/SystemFont";

export class GettingStartedCodelab extends Component {
    private msg?: string;
    onCreate() {
        this.msg = 'Hello valdi on ';
    }
    onRender() {
        const fullMsg = this.msg + new Date().toLocaleTimeString();
        <layout padding={24} paddingTop={80}>
            <label value='Declarative codelab' font={systemBoldFont(16)} />
            <label value={fullMsg} font={systemFont(12)} />
        </layout>;
    }
}
```

## **`if`** statements
You can conditionally render parts of your UI using if statements.

Add a new **boolean** class variable.

```typescript
export class GettingStartedCodelab extends Component {
    private msg?: string;
    private kitten?: boolean;
    ...
```

Now add an **`if`** statement checking that variable.

```typescript
onRender() {
    if (this.kitten) {
        <image height={50} width={50} src='https://placecats.com/300/300' />
    } else {
        const fullMsg = this.msg + new Date().toLocaleTimeString();
        <layout>
            <label value='Declarative codelab' font={systemBoldFont(16)} />
            <label value={fullMsg} font={systemFont(12)} />
        </layout>;
    }
}
```

You won't see any changes yet. Because **`kitten`** is an optional variable and it is still `undefined`, it evaluates to **`false`**.

Set the value of your boolean in `onCreate`.

```tsx
onCreate() {
    this.msg = 'Hello valdi on ';
    this.kitten = true;
}
```

Now you should see a kitten!

## Inline loops
You can iterate through lists in the middle of the TSX.

Start by removing the **`kitten`** variable we added for the if statement.

Create a list of kittens.

```tsx
export class GettingStartedCodelab extends Component {
    private msg?: string;
    private kittens = [
        'https://placecats.com/300/300',
        'https://placecats.com/300/301',
        'https://placecats.com/300/302',
    ];
    ...
```

The slightly different sizes will give you different kittens.

Then iterate through the **`kittens`** with a **`forEach`**.

```tsx
onRender() {
    const fullMsg = this.msg + new Date().toLocaleTimeString();
    <layout>
        <label value='Declarative codelab' font={systemBoldFont(16)} />
        <label value={fullMsg} font={systemFont(12)} />
        {this.kittens.forEach(kitten => {
            <image height={50} width={50} src={kitten} />;
        })}
    </layout>;
}
```

## Inline **`if`** and **`for`** caveat
You might think, having seen the examples above, that you can do something similar with inline **`if`** and **`for`** loops.

Well, you can't.

The contents of a curly brace block inside of the TSX need to be an expression. In the current TypeScript standard, **`if`** and **`for`** are not expressions.

If you want to do something similar, you have a few options discussed below.

## **`when`** expressions
Going back to our **`if`** implementation.

```tsx
onRender() {
    if (this.kitten) {
        <image height={50} width={50} src='https://placecats.com/300/300' />
    } else {
        const fullMsg = this.msg + new Date().toLocaleTimeString();
        <layout>
            <label value='Declarative codelab' font={systemBoldFont(16)} />
            <label value={fullMsg} font={systemFont(12)} />
        </layout>;
    }
}
```

If you decide you want to keep the **"Declarative codelab"** title and just swap out the contents of the second line, you can do that with a **`when`** expression.

First, import the `when` utility:

```tsx
import { when } from 'valdi_core/src/utils/When';
```

Then use it to conditionally render a component:

```tsx
onRender() {
    const fullMsg = this.msg + new Date().toLocaleTimeString();
    <layout>
        <label value='Declarative codelab' font={systemBoldFont(16)} />
        {when(this.kitten, () => {
            <image height={50} width={50} src='https://placecats.com/300/300' />;
        })}
        <label value={fullMsg} font={systemFont(12)} />
    </layout>;
}
```

The first parameter in **`when`** is the conditional, this can be an expression that evaluates to a **`boolean`** or just a variable. The second parameter is a lambda that executes when your conditional evaluates to **`true`**.

**`when`** has no **`else`** clause, so if you want that kind of thing, you need to implement a `when(!conditional)`.

```typescript
onRender() {
    const fullMsg = this.msg + new Date().toLocaleTimeString();
    <layout>
        <label value='Declarative codelab' font={systemBoldFont(16)} />
        {when(this.kitten, () => {
            <image height={50} width={50} src='https://placecats.com/300/300' />;
        })}
        {when(!this.kitten, () => {
            <label value={fullMsg} font={systemFont(12)} />;
        })}
    </layout>;
}
```

Try changing the value of **`this.kitten`** and watch what happens.

## Nested render functions
You can call out to another function from `onRender`.

Create a new function, `onRenderMessage`, and call it from `onRender`.

```typescript
onRenderMessage(message: string) {
    // Render message
}

onRender() {
    const fullMsg = this.msg + new Date().toLocaleTimeString();
    <layout>
        <label value='Declarative codelab' font={systemFont(12)} />
        {this.onRenderMessage(fullMsg)}
    </layout>;
}
```

Now we can put whatever we want in that `onRenderMessage` function.

Add an **`if`** statement to swap out the kitten.

```typescript
onRenderMessage(message: string) {
    if (this.kitten) {
        <image height={50} width={50} src='https://placecats.com/300/300' />;
    } else {
        <label value={message} font={systemFont(12)} />;
    }
}
```

This isn't just useful for conditionals that are not expressions, it can make your code more readable and reusable.

### [Next >](./6-component_state.md)
