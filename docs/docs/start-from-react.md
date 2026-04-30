# Valdi for React Developers

Valdi and React are both inspired by Functional Reactive Programming UI patterns.

The "state" of the application is fed into root of the render tree, and components use this state to produce rendered elements. Events emitted by the user interface are then used to change the state of the application in order to produce an updated render tree.

A process compares the new output to the old output and updates the runtime to change the running application.

## Components: Valdi vs React

In React there are two flavors of components: `function` components and `class` components.

Valdi does not have an API equivalent to React’s `function` components.

### React `class` Components

React’s _`class` components_ inherit from `class React.Component<P, S>` and _at least_ implement an `onRender(): JSX.Element`

Props (`P`) are stored as a property (`this.props`) of the `Component` instance and and are provided by the parent of the component via JSX attribute syntax:

```tsx
<Heading title="Hello World" />
```

A component is rendered when:

- It is given new props
- It calls `this.setState({...})` with new state

### Valdi’s `class Component<ViewModel, State>`

Valdi provides a `Component<ViewModel, State, Context>` class similar to React’s `React.Component<P, S>` with the same runtime characteristics for rendering:

- Changes to a component’s props from a parent component will trigger a render pass
- Calling `this.setState({...})` with changed state values will trigger a render pass

In Valdi the concept of `Props` are called `ViewModel`s but are otherwise treated identically. In both Valdi and React, the internal/private set of properties that can trigger a render are called `State`.

> **See also:** [The Mighty Component](./core-component.md) for complete documentation on Valdi components and [Component States](./core-states.md) for state management details.

#### Example React Component ported to Valdi

The following is a trivial React `Component` that uses `Props` and `State`.

```tsx
import { Component } from "react";

interface Props {
  label: string;
  onDoThing: (count: number) => void;
}

interface State {
  counter: number;
}

class MyComponent extends Component<Props, State> {
  state = {
    counter: 0,
  };

  handleTap = () => {
    const counter = this.state.counter + 1;
    this.setState({ counter });
    this.props.onDoThing(counter);
  };

  onRender(): JSX.Element {
    return (
      <view onTap={this.handleTap}>
        <label
          value={`${this.props.label} (click count ${this.state.counter})`}
        />
      </view>
    );
  }
}
```

This component can be used **almost** as is in Valdi:

```tsx
import { StatefulComponent } from "valdi_core/src/Component";

interface ViewModel {
  label: string;
  onDoThing: (count: number) => void;
}

interface State {
  counter: number;
}

class MyComponent extends StatefulComponent<ViewModel, State> {
  state = {
    counter: 0,
  };

  handleTap = () => {
    const counter = this.state.counter + 1;
    this.setState({ counter });
    this.viewModel.onDoThing(counter);
  };

  onRender(): void {
    <view onTap={this.handleTap}>
      <label
        value={$`{this.viewModel.label} (click count ${this.state.counter})`}
      />
    </view>;
  }
}
```

Valdi provides a "stateless" component: `Component` which has no `State` type or `setState` method. Both `Component` and `StatefulComponent` have a third generic parameter: `Context`.

This is not the same as React's `Context` API. It is part of Valdi's native API integration. See _[Component Context](./native-context.md)_ to learn more about Valdi's `Context` API.

### Component `key` in React and Valdi

Each Component can be given a `key` property and it functions the same way as React so the same best practices apply.

If you have a list of items and each item has some intrinsic way of identifying it, using a stable key to reference it will maintain its lifecycle during render passes.

```tsx
    renderListElements() {
        for (const item of STUFF) {
            <ListItem key={item}><label value={item} /></ListItem>
        }
    }
```

## Differences from React

There are some additional differences, like [life-cycle methods](./core-component.md#lifecycle) but the same best practices remain:

- Do not mutate `this.state`, call `this.setState()`. Valdi sets it to `ReadOnly<State>`
- Do not mutate `this.viewModel`, the `viewModel`’s value is defined by the parent. Valdi sets it to `ReadOnly<ViewModel>`
- Inline/lambda functions and non-literal values should be stored as properties of the `class` instance to keep from changing props unintentionally during render. For callback props there is a utility to help with this: [`createReusableCallback`](../../src/valdi_modules/src/valdi/valdi_core/src/utils/Callback.ts#L34)

### Render Output of Valdi vs React.

There is one significant difference to call out between Valdi and React as it pertains to the `onRender()` method and constructing the output tree.

In React, the method is `onRender(): JSX.Element | null`. In Valdi, the method is `onRender(): void`. It returns `void`, nothing is returned from `onRender()`.

Valdi components **do not return rendered elements**.

When `onRender` is called by the Valdi runtime, JSX tags emit the render operations as a side-effect. This means _when_ JSX tags are used is critical to where they end up in the tree.

In React it is common to store a reference to the `JSX.Element` output of a component and then eventually use it in your component’s output.

For example, instead of just allowing a `label: string`, in React it can be specified as `label: JSX.Element | null` and the parent can use any other valid React component.

```tsx
<MyComponent label={<label style={{ color: 'red' }} value="Button label text">}>
```

This cannot be done in Valdi. Not only would this fail to pass TypeScript satic analysis, the `<label>` would be emitted into the render tree at a different location in the tree, not where `MyComponent` uses its `this.viewModel.label` property.

To achieve something similar in Valdi you can use what in the React are sometimes called "render props". It would then be up to `MyComponent` to call the function at the correct time.

```tsx
interface ViewModel {
   label: () => void
}

onRender() {
   <view margin={20}>{this.viewModel.label()}</view>
}
```

Due to the nature of JSX elements being emitted instead of returned, you can also use `for`/`while` loop iterators to generate lists of components.

```tsx
const STUFF = ["beacon", "shovel", "probe", "straps", "harness"];

class SomeComponent extends Component {
  onRender() {
    <List>{this.renderListElements()}</List>;
  }

  renderListElements() {
    for (const item of STUFF) {
      <ListItem>
        <label value={item} />
      </ListItem>;
    }
  }
}
```

In React, this would produce a `<List />` with nothing in it because the `<ListItem />` components are never returned and placed in the component’s output.

```tsx
// Example output of <SomeComponent /> using React
<List />
```

In Valdi, because the `renderListElements()` method is emitting `<ListItem>...</ListItem>` elements after the opening of a `<List>` is emitted, but _before_ the closing `</List>` is emitted, the `<ListItem></ListItem>` elements will be placed inside the `<List />` in the component’s rendered representation:

```tsx
// Example ouptut of <SomeComponent /> using Valdi.
<List>
  <ListItem>
    <label value="beacon" />
  </ListItem>
  <ListItem>
    <label value="shovel" />
  </ListItem>
  <ListItem>
    <label value="probes" />
  </ListItem>
</List>
```

To reiterate, this is because of _when_ the elements were emitted during a render pass which starts when a component’s `onRender` is called and finishes when all leaves are done rendering.

### Component `ref`

Similar to `React`, a `Component`’s `ref` provides a way to access the API of the underlying instance of a child component and interact with it beyond using a `Component`’s `viewModel`.

Ideally the contract between a `Component` and its owner is implemented purely in the component’s `ViewModel` but there are instances where it is less complex to interact with the child’s state with imperative code.

Vald uses the `interface IRenderedElementHolder<T>` and a `ref` is generally assigned to an implementation stored on the instance of the `Component` rendering it.

An example of this is [`ScrollViewHandler`](https://github.com/Snapchat/Valdi_Widgets/blob/main/valdi_modules/widgets/src/components/scroll/ScrollViewHandler.ts#L33) which provides a means of interacting with the view underlying a `<scroll>` component.

This is a trivial example assigns a `ref` to a `<scroll>` native element, and provides a button that scrolls the view down by `10` pixels when pressed.

```tsx

interface ViewModel {
    list: {name: string}[]
}

class SomeComponent<ViewModel> {

    private currentPosition: {x : number, y: number};

    private readonly scrollHandler = new ScrollViewHandler();

    private scrollDownByTen = () => {
        this.scrollHandler.scrollTo(
            this.scrollHandler.scrollX,
            this.scrollHandler.scrollY + 10
        );
    }

    onRender() {
        <layout>
            <view onTap={this.scrollDownByTen}>
                <>
            </view>
            <scroll ref={scrollHandler}>
                {this.viewModel.list.forEach((item) => {
                    <ListItem data={item} />
                })}
            </scroll>
        </layout>
    }
}
```

### Component `children` and the `<slot />` API

In React, a component’s `children` prop is used by a component like any other prop.

In Valdi, a component uses the [`<slot />`](./core-slots.md) API to declare where its children will be rendered.

A Valdi component can specify a different type for its `children` `ViewModel` property. For example the [`MeasuredView` component specifies a function for its children](https://github.com/Snapchat/Valdi_Widgets/blob/main/valdi_modules/widgets/src/components/util/MeasuredView.tsx#L6-L8):

```tsx
export interface MeasuredViewModel extends View {
  children?: (frame: ElementFrame) => void;
}
```

To opt into rendering optimizations, parent componets should use the `$slot` and `$namedSlots` utilities that signal to the rendering system that the children for a component are being changed.

Using `MeasuredView` as an example, the `children` callback is wrapped in `$slot`:

```tsx
import { $slot } from 'valdi_core/src/CompilerIntrinsics';
import { MeasuredView } from 'valdi_widgets/src/components/util/MeasuredView';

onRender() {
    <MeasuredView>
        {$slot(({ widgth, height }) => {
            <view>
            </view>
        })}
    </MeasuredView>
}
```

## Project Setup

A React project built with `create-react-app` or Vite maps to a Valdi project bootstrapped with `valdi bootstrap`.

| React | Valdi |
|---|---|
| `npx create-react-app my-app` | `mkdir my-app && cd my-app && valdi bootstrap` |
| `package.json` deps | `BUILD.bazel` + `valdi_module()` deps |
| `npm start` / `vite dev` | `valdi hotreload` |
| Single `src/index.tsx` entrypoint | Per-module `BUILD.bazel` — modules load lazily |
| `npm run build` | `bazel build //my_module` |

A Valdi module's `BUILD.bazel`:

```python
load("@valdi//bzl/valdi:valdi_module.bzl", "valdi_module")

valdi_module(
    name = "my_module",
    srcs = glob(["src/**/*.ts", "src/**/*.tsx"]),
    deps = [
        "@valdi//src/valdi_modules/src/valdi/valdi_core",
        "@valdi//src/valdi_modules/src/valdi/valdi_http",
    ],
)
```

Unlike a React app's single `main.jsbundle`, each Valdi module compiles to an independent `.valdimodule` archive that is lazy-loaded by the host app.

## Navigation

React Router and React Navigation both map to Valdi's `NavigationController`, accessed through `NavigationRoot`.

```tsx
// React Router
import { BrowserRouter, Route, Link, useNavigate } from 'react-router-dom';

function App() {
  return (
    <BrowserRouter>
      <Route path="/detail/:id" element={<DetailPage />} />
    </BrowserRouter>
  );
}

function HomePage() {
  const navigate = useNavigate();
  return <button onClick={() => navigate('/detail/123')}>Go</button>;
}
```

```tsx
// Valdi
import { NavigationRoot } from 'valdi_navigation/src/NavigationRoot';
import { NavigationPageComponent } from 'valdi_navigation/src/NavigationPageComponent';
import { $slot } from 'valdi_core/src/CompilerIntrinsics';

class App extends Component {
  onRender() {
    <NavigationRoot>
      {$slot(navigationController => {
        <HomePage navigationController={navigationController} />;
      })}
    </NavigationRoot>;
  }
}

@NavigationPage(module)
class HomePage extends NavigationPageComponent<{}> {
  private goToDetail = () => {
    // push = horizontal slide; present = vertical modal
    this.navigationController.push(DetailPage, { id: '123' });
  };

  onRender() {
    <view onTap={this.goToDetail}>
      <label value="Go to detail" />;
    </view>;
  }
}
```

| React Router / React Navigation | Valdi |
|---|---|
| `<BrowserRouter>` / `<NavigationContainer>` | `<NavigationRoot>` |
| `navigate('/path')` / `navigation.navigate('Screen')` | `this.navigationController.push(Page, viewModel)` |
| `navigation.goBack()` | `this.navigationController.pop()` |
| `<Modal>` / bottom sheet | `this.navigationController.present(Page, viewModel)` |
| Dismiss modal | `this.navigationController.dismiss()` |
| `useNavigation()` hook | `this.navigationController` (available on all `NavigationPageComponent`) |

## Lists

React's `Array.map()` and `FlatList` map to a `forEach` loop inside `<scroll>`. The key difference: React's `map()` works because `render()` returns a value. In Valdi, `onRender()` emits JSX as side-effects — `map()` returns an array that is **silently discarded**.

```tsx
// React — map() works because render() returns JSX
function UserList({ users }) {
  return (
    <FlatList
      data={users}
      keyExtractor={u => u.id}
      renderItem={({ item }) => <UserRow user={item} />}
    />
  );
}
```

```tsx
// Valdi — use forEach, not map()
class UserList extends Component<{ users: User[] }> {
  onRender() {
    <scroll>
      {this.viewModel.users.forEach(user => {
        <UserRow key={user.id} data={user} />;
      })}
    </scroll>;
  }
}
```

Valdi's `<scroll>` automatically handles viewport-aware rendering — elements outside the visible area are not inflated. For expensive items, add `lazy={true}` to further limit rendering to the visible viewport.

## Networking

`fetch` and `axios` map to Valdi's `HTTPClient` from the `valdi_http` module. All requests return a `CancelablePromise`.

```tsx
// React
useEffect(() => {
  const controller = new AbortController();
  fetch('/api/users', { signal: controller.signal })
    .then(r => r.json())
    .then(data => setUsers(data));
  return () => controller.abort();
}, []);
```

```tsx
// Valdi
import { HTTPClient } from 'valdi_http/src/HTTPClient';

class UserList extends StatefulComponent<{}, { users: User[] }> {
  state = { users: [] };
  private client = new HTTPClient('https://api.example.com');
  private request?: { cancel(): void };

  onCreate() {
    this.request = this.client.get('/users');
    this.request.then(response => {
      const users = JSON.parse(new TextDecoder().decode(response.body));
      this.setState({ users });
    });
  }

  onDestroy() {
    this.request?.cancel(); // Equivalent to AbortController.abort()
  }

  onRender() {
    <scroll>
      {this.state.users.forEach(u => {
        <label key={u.id} value={u.name} />;
      })}
    </scroll>;
  }
}
```

Add to `BUILD.bazel` deps: `@valdi//src/valdi_modules/src/valdi/valdi_http`

## Styling

`styled-components`, CSS modules, and React Native's `StyleSheet` all map to Valdi's `Style<T>` objects.

```tsx
// React — styled-components
const Card = styled.div`
  background: white;
  border-radius: 8px;
  padding: 16px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
`;

// React Native — StyleSheet
const styles = StyleSheet.create({
  card: { backgroundColor: 'white', borderRadius: 8, padding: 16 },
});
```

```tsx
// Valdi — Style<T> at module level (never inside onRender)
import { Style } from 'valdi_core/src/Style';
import { View } from 'valdi_tsx/src/NativeTemplateElements';

const cardStyle = new Style<View>({
  backgroundColor: '#ffffff',
  borderRadius: 8,
  padding: 16,
  boxShadow: '0 2 8 rgba(0,0,0,0.1)',
});

class Card extends Component<{ title: string }> {
  onRender() {
    <view style={cardStyle}>
      <label value={this.viewModel.title} />;
    </view>;
  }
}
```

> [!Important]
> Never create `new Style()` inside `onRender()`. Valdi assigns each `Style` instance a unique integer ID on first use; creating new instances every render defeats this optimization. Define styles at module level or as class properties.

Combine styles with `Style.merge()` or `.extend()`:

```tsx
const activeCardStyle = cardStyle.extend({ borderColor: '#FFFC00', borderWidth: 2 });
const combinedStyle = Style.merge(baseStyle, overrideStyle);
```

For theming (equivalent to React's `ThemeContext`), use the [Provider pattern](./advanced-provider.md).

## Storage

`localStorage` and `AsyncStorage` (React Native) map to `PersistentStore`.

```tsx
// React / React Native
localStorage.setItem('token', value);
const token = localStorage.getItem('token');
// or AsyncStorage.setItem / getItem
```

```tsx
// Valdi
import { PersistentStore } from 'persistence/src/PersistentStore';

const store = new PersistentStore('my_store', {
  enableEncryption: true, // for sensitive data like tokens
});

await store.storeString('token', value);
const token = await store.fetchString('token');
await store.remove('token');
```

Add to deps: `@valdi//src/valdi_modules/src/valdi/persistence`

## Testing

Jest and React Testing Library map to Valdi's jasmine-based test framework with `valdiIt` and `IComponentTestDriver`.

```tsx
// React Testing Library
test('renders counter', () => {
  render(<Counter label="Clicks" />);
  expect(screen.getByText('Clicks: 0')).toBeInTheDocument();
  fireEvent.click(screen.getByRole('button'));
  expect(screen.getByText('Clicks: 1')).toBeInTheDocument();
});
```

```tsx
// Valdi — test/Counter.spec.tsx
import 'jasmine/src/jasmine';
import { valdiIt } from 'valdi_core/src/testing';

describe('Counter', () => {
  valdiIt('renders initial count', async driver => {
    await driver.render(Counter, { label: 'Clicks' });
    const label = driver.findFirst('label');
    expect(label?.getAttribute('value')).toBe('Clicks: 0');
  });
});
```

Run tests:
```sh
bazel test //my_module:test
```

See [Testing documentation](./workflow-testing.md) for full details on `IComponentTestDriver` and hot-reload iteration.

## Common Pitfalls

| React pattern | Valdi equivalent | Why it matters |
|---|---|---|
| Inline lambda: `onTap={() => fn()}` | Class arrow fn: `private fn = () => {}` | Inline lambdas create new references each render, causing unnecessary child re-renders |
| `items.map(i => <Item />)` | `items.forEach(i => { <Item />; })` | `map()` returns are discarded; `forEach` emits as side-effects |
| `new Style({...})` inside `onRender()` | Module-level `const s = new Style({...})` | Style interning requires stable object identity |
| `label: JSX.Element` prop | `label: () => void` render prop | JSX can't be passed as a value — it must be called at the right time |
| `import { x } from 'react'` | `import { x } from 'valdi_core/src/...'` | No React dependency in Valdi |
| `createReusableCallback` not needed (React) | Use when callback identity must be stable across renders | Prevents spurious viewModel updates in children |
