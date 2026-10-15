# Component events
Before we get started, your `GettingStartedComponent` should look a little something like this.

```tsx
import { StatefulComponent } from 'valdi_core/src/Component';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';

export interface GettingStartedCodeLabViewModel {
    header: string;
    numKittens?: number;
}

interface GettingStartedCodeLabState {
    elapsed: number;
    kitten: boolean;
}

export class GettingStartedCodelab extends StatefulComponent<
    GettingStartedCodeLabViewModel,
    GettingStartedCodeLabState
    > {
    state = {
        elapsed: 0,
        kitten: false,
    };
    private msg?: string;
    private kitten?: boolean;
    private kittens = [
        'https://placecats.com/300/300',
        'https://placecats.com/300/301',
        'https://placecats.com/300/302',
    ];

    private interval?: number;
    onCreate() {
        this.msg = 'Hello valdi on ';
        this.kitten = true;

        this.interval = setInterval(() => {
        this.setState({
            elapsed: this.state.elapsed + 1,
            kitten: this.state.elapsed % 5 == 0,
        });
        }, 1000);
    }

    onDestroy() {
        if (this.interval) {
            clearInterval(this.interval);
        }
    }

    onRenderMessage(message: string) {
        if (this.state.kitten) {
            <image height={50} width={50} src='https://placecats.com/300/300' />;
        } else {
            <label value={message} font={systemFont(12)} />;
        }
    }

    onRender() {
        const fullMsg = this.msg + new Date().toLocaleTimeString();
        <layout>
            <label value={this.viewModel.header} font={systemBoldFont(16)} />
            <label value={`Time Elapsed: ${this.state.elapsed} seconds`} />
            {this.onRenderMessage(fullMsg)}
        </layout>;
    }
}
```

## Touch event callbacks
Some elements expose handlers for working with touch events. You can register callbacks for specific events on the element.

Let's create a button that adds kittens.

First, add a variable to your component's **State** to keep track of the number of **kittens**.

```typescript
interface GettingStartedCodeLabState {
    elapsed: number;
    kitten: boolean;
    numKittens: number;
}
```

Then initialize it to something reasonable.

```typescript
state = {
    elapsed: 0,
    kitten: false,
    numKittens: 1,
};
```

Now, let's consume that variable inside `onRenderMessage`.

```tsx
onRenderMessage(message: string) {
    if (this.state.kitten) {
        for (let i = 0; i < this.state.numKittens; i++) {
            <image height={50} width={50} src='https://placecats.com/300/300' />;
        }
    } else {
        <label value={message} font={TextStyleFont.BODY} />;
    }
}
```

The for loop will create **numKittens** kitten images.

To update **numKittens**, let's add a button in our `onRender` function.

```tsx
<layout>
    <label value={this.viewModel.header} font={TextStyleFont.TITLE_1} />
    <label value={`Time Elapsed: ${this.state.elapsed} seconds`} />
    <view margin={20} backgroundColor='grey' borderRadius={8} padding={16}>
        <label value='Add kitten' />
    </view>
    {this.onRenderMessage(fullMsg)}
</layout>;
```

Yes, the button is a `<view>` but it has all of the callbacks that we need to implement button behavior. There is a whole catalog of **CoreUI** components that contains a stylized button but that's a subject for another code lab.

If you click the button now, nothing happens. That's because we need to register an `onTap` handler.

Let's start by creating the callback function.

```tsx
private buttonTapped = () => {
    this.setState({ numKittens: this.state.numKittens + 1 });
};
```

Interaction callbacks should be implemented as [lambdas](https://www.typescriptlang.org/docs/handbook/2/classes.html#arrow-functions) to make sure that the callback is not recreated every render. Don't worry, the linter will yell at you if you forget.

Now we can hook it up to `onTap`.

```tsx
<view margin={20} backgroundColor='grey' borderRadius={8} padding={16} onTap={this.buttonTapped}>
    <label value='Add kitten' />
</view>
```

Tap the button a few times and see how the UI updates.

## Full solution
Parking the full solution here if you need it.

```tsx
import { StatefulComponent } from 'valdi_core/src/Component';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';

export interface GettingStartedCodeLabViewModel {
    header: string;
    numKittens?: number;
}

interface GettingStartedCodeLabState {
    elapsed: number;
    kitten: boolean;
    numKittens: number;
}

export class GettingStartedCodelab extends StatefulComponent<
    GettingStartedCodeLabViewModel,
    GettingStartedCodeLabState
    > {
state = {
    elapsed: 0,
    kitten: false,
    numKittens: 1,
};
private msg?: string;
private kitten?: boolean;
private kittens = [
    'https://placecats.com/300/300',
    'https://placecats.com/300/301',
    'https://placecats.com/300/302',
];

private interval?: number;
onCreate() {
    this.msg = 'Hello valdi on ';
    this.kitten = true;

    this.interval = setInterval(() => {
        this.setState({
            elapsed: this.state.elapsed + 1,
            kitten: this.state.elapsed % 5 == 0,
        });
    }, 1000);
}

onDestroy() {
    if (this.interval) {
        clearInterval(this.interval);
    }
}

onRenderMessage(message: string) {
    if (this.state.kitten) {
        for (let i = 0; i < this.state.numKittens; i++) {
            <image height={50} width={50} src='https://placecats.com/300/300' />;
        }
    } else {
        <label value={message} font={systemFont(12)} />;
    }
}

onRender() {
    const fullMsg = this.msg + new Date().toLocaleTimeString();
    <layout>
    <label value={this.viewModel.header} font={systemBoldFont(16)} />
    <label value={`Time Elapsed: ${this.state.elapsed} seconds`} />
    <view margin={20} backgroundColor='grey' borderRadius={8} padding={16} onTap={this.buttonTapped}>
        <label value='Add kitten' />
    </view>
    {this.onRenderMessage(fullMsg)}
    </layout>;
}

private buttonTapped = () => {
        this.setState({ numKittens: this.state.numKittens + 1 });
    };
}
```

### [Next >](./9-unittest.md)
