# Migrating from Flutter to Valdi

Valdi and Flutter share the same high-level goal: write UI once, run natively on iOS and Android without web views. If you're coming from Flutter, many concepts will feel familiar—but the implementation language, build system, and component model are different.

This guide walks through how Flutter concepts map to Valdi equivalents.

## Mental Model

| | Flutter | Valdi |
|---|---|---|
| **Language** | Dart | TypeScript |
| **Build system** | `pubspec.yaml` + `flutter` CLI | `BUILD.bazel` + `valdi` CLI |
| **UI primitive** | Widget | Component (class-based) |
| **Renders to** | Native views via Skia/Impeller | Native views directly |
| **Hot reload** | `flutter run` | `valdi hotreload` |
| **Package manager** | `pub.dev` | npm + Bazel deps |

Both frameworks compile declarative UI code to true native views—no WebView, no JS bridge at runtime.

## Project Setup

### Creating a new project

```bash
# Flutter
flutter create my_app
cd my_app
flutter run

# Valdi
mkdir my_app && cd my_app
valdi bootstrap
valdi install ios    # or android
valdi hotreload
```

### Build configuration

Flutter uses `pubspec.yaml` to declare dependencies. Valdi uses a `BUILD.bazel` file with the `valdi_module()` rule:

```python
# Flutter: pubspec.yaml
# dependencies:
#   http: ^1.0.0

# Valdi: BUILD.bazel
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

## Component Model

### StatelessWidget → Component

```dart
// Flutter
class Greeting extends StatelessWidget {
  final String name;
  const Greeting({required this.name});

  @override
  Widget build(BuildContext context) {
    return Text('Hello, $name');
  }
}
```

```tsx
// Valdi
import { Component } from 'valdi_core/src/Component';

interface GreetingViewModel {
  name: string;
}

class Greeting extends Component<GreetingViewModel> {
  onRender() {
    <label value={`Hello, ${this.viewModel.name}`} />;
  }
}
```

Key differences:
- Props are declared as a `ViewModel` interface, accessed via `this.viewModel` (not `this.widget` or constructor params)
- `build(BuildContext context)` → `onRender(): void` — **no return statement**, JSX is emitted as a side-effect
- No `BuildContext` parameter — context injection uses the [Provider pattern](#provider-pattern) instead

### StatefulWidget → StatefulComponent

Flutter's `StatefulWidget` + `State<T>` pair becomes a single `StatefulComponent<ViewModel, State>` class in Valdi. The `setState()` API is nearly identical.

```dart
// Flutter
class Counter extends StatefulWidget {
  final String label;
  const Counter({required this.label});
  @override
  State<Counter> createState() => _CounterState();
}

class _CounterState extends State<Counter> {
  int _count = 0;

  void _increment() {
    setState(() { _count++; });
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: _increment,
      child: Text('${widget.label}: $_count'),
    );
  }
}
```

```tsx
// Valdi
import { StatefulComponent } from 'valdi_core/src/Component';

interface CounterViewModel {
  label: string;
}

interface CounterState {
  count: number;
}

class Counter extends StatefulComponent<CounterViewModel, CounterState> {
  state = { count: 0 };

  // Class arrow function — never use inline lambdas in JSX
  private increment = () => {
    this.setState({ count: this.state.count + 1 });
  };

  onRender() {
    <view onTap={this.increment}>
      <label value={`${this.viewModel.label}: ${this.state.count}`} />;
    </view>;
  }
}
```

> [!Important]
> Event handlers must be class arrow functions (e.g. `private increment = () => {}`), not inline lambdas in JSX. Inline lambdas create a new function reference on every render, causing unnecessary re-renders of child components.

## Lifecycle Methods

| Flutter (`State<T>`) | Valdi |
|---|---|
| `initState()` | `onCreate()` |
| `dispose()` | `onDestroy()` |
| `didUpdateWidget(oldWidget)` | `onViewModelUpdate(previous?)` |
| `build(context)` | `onRender(): void` |
| `didChangeDependencies()` | `onViewModelUpdate(previous?)` (for Provider changes) |

```tsx
class MyComponent extends StatefulComponent<MyViewModel, MyState> {
  state = { data: null };

  onCreate() {
    // Like initState() — runs once after component mounts
    this.loadData();
  }

  onViewModelUpdate(previous?: MyViewModel) {
    // Like didUpdateWidget — runs when viewModel changes
    if (this.viewModel.id !== previous?.id) {
      this.loadData();
    }
  }

  onDestroy() {
    // Like dispose() — clean up subscriptions, timers, etc.
  }

  private async loadData() {
    const data = await fetchData(this.viewModel.id);
    this.setState({ data });
  }

  onRender() {
    if (!this.state.data) {
      <spinner />;
      return;
    }
    <label value={this.state.data.title} />;
  }
}
```

## Layout

Flutter uses `Row`, `Column`, and `Stack` widgets. Valdi uses flexbox (the CSS Web Flexbox spec, via Facebook's Yoga engine), applied directly to `<view>` and `<layout>` elements.

| Flutter | Valdi |
|---|---|
| `Column` | `<view flexDirection="column">` (default) |
| `Row` | `<view flexDirection="row">` |
| `Stack` | `<view>` with children using `position="absolute"` |
| `Container` (visual) | `<view>` |
| `SizedBox` / `Padding` (layout only) | `<layout>` — no native view, use when no visual styling needed |
| `Expanded` | `flexGrow={1}` on child |
| `Flexible` | `flexShrink={1}` on child |
| `Spacer` | `<layout flexGrow={1} />` |
| `Center` | `<view justifyContent="center" alignItems="center">` |
| `Padding(padding: EdgeInsets.all(8))` | `<view padding={8}>` |
| `AspectRatio` | `<view aspectRatio={16/9}>` |

```tsx
// Flutter Column with two centered items:
// Column(
//   mainAxisAlignment: MainAxisAlignment.center,
//   children: [Text('Hello'), Text('World')],
// )

// Valdi:
onRender() {
  <view flexDirection="column" justifyContent="center" alignItems="center">
    <label value="Hello" />;
    <label value="World" />;
  </view>;
}
```

```tsx
// Flutter Stack with positioned overlay:
// Stack(children: [
//   Image.network(url),
//   Positioned(bottom: 8, right: 8, child: Text('Caption')),
// ])

// Valdi:
onRender() {
  <view>
    <image src={this.viewModel.imageUrl} width="100%" height="100%" objectFit="cover" />;
    <label
      value="Caption"
      position="absolute"
      bottom={8}
      right={8}
    />;
  </view>;
}
```

## Native Elements

| Flutter Widget | Valdi Element | Notes |
|---|---|---|
| `Text` | `<label>` | `value` prop for string content |
| `TextField` | `<textfield>` | Single-line input |
| `TextFormField` | `<textview>` | Multi-line input |
| `Image.network` / `Image.asset` | `<image>` | `src` accepts URL string or resource |
| `VideoPlayer` | `<video>` | |
| `ListView` / `GridView` | `<scroll>` + `forEach` | See [Lists](#lists) |
| `SingleChildScrollView` | `<scroll>` | |
| `PageView` | `<scroll pagingEnabled={true}>` | |
| `CircularProgressIndicator` | `<spinner>` | |
| `Lottie` | `<animatedimage>` | |
| `ClipRRect` / `ClipOval` | `<view clipToBounds={true} borderRadius={...}>` | |
| `BackdropFilter` | `<blur>` | iOS only |
| `UiKitView` / `AndroidView` | `<custom-view iosClass="..." androidClass="...">` | |

## Lists

Flutter's `ListView.builder` uses index-based construction. Valdi uses `forEach` inside a `<scroll>` element. Viewport-aware rendering is automatic — only visible elements are inflated.

```dart
// Flutter
ListView.builder(
  itemCount: items.length,
  itemBuilder: (context, index) {
    return ListTile(title: Text(items[index].name));
  },
)
```

```tsx
// Valdi
onRender() {
  <scroll>
    {this.viewModel.items.forEach(item => {
      <ListItem key={item.id} data={item} />;
    })}
  </scroll>;
}
```

> [!Important]
> Do **not** use `map()` — Valdi JSX is emitted as side-effects, not return values. `map()` returns an array that is silently discarded. Use `forEach` or `for...of` loops instead.

For performance with very large lists, use `lazy={true}` on children to enable viewport-limited rendering:

```tsx
<scroll>
  {this.viewModel.items.forEach(item => {
    <ExpensiveRow key={item.id} data={item} lazy={true} />;
  })}
</scroll>
```

## Navigation

| Flutter | Valdi |
|---|---|
| `MaterialApp` / `CupertinoApp` | `NavigationRoot` |
| `Navigator.push(context, route)` | `navigationController.push(Page, viewModel, context)` |
| `Navigator.pop(context)` | `navigationController.pop()` |
| `Navigator.pushReplacement` | `navigationController.push` + `pop` |
| `showModalBottomSheet` | `navigationController.present(Page, viewModel, context)` |
| `Navigator.of(context).pop()` from modal | `navigationController.dismiss()` |

```tsx
import { NavigationRoot } from 'valdi_navigation/src/NavigationRoot';
import { NavigationPageComponent } from 'valdi_navigation/src/NavigationPageComponent';

// App root
class App extends Component {
  onRender() {
    <NavigationRoot>
      {$slot(navigationController => {
        <HomePage navigationController={navigationController} />;
      })}
    </NavigationRoot>;
  }
}

// Page — annotate with @NavigationPage
@NavigationPage(module)
class HomePage extends NavigationPageComponent<HomeViewModel> {
  private goToDetail = () => {
    this.navigationController.push(DetailPage, { id: '123' });
  };

  onRender() {
    <view onTap={this.goToDetail}>
      <label value="Go to detail" />;
    </view>;
  }
}
```

## Networking

| Flutter | Valdi |
|---|---|
| `http.get(uri)` | `client.get('/path')` |
| `http.post(uri, body: body)` | `client.post('/path', body)` |
| `dio.get(url)` | `client.get('/path')` |
| `Future<T>` | `Promise<T>` / `CancelablePromise<T>` |
| `CancelToken` (dio) | `.cancel()` on the returned promise |

```tsx
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
    // Cancel in-flight request like Flutter's CancelToken
    this.request?.cancel();
  }

  onRender() {
    <scroll>
      {this.state.users.forEach(user => {
        <label key={user.id} value={user.name} />;
      })}
    </scroll>;
  }
}
```

Add `valdi_http` to your `BUILD.bazel` deps:
```python
deps = ["@valdi//src/valdi_modules/src/valdi/valdi_http"]
```

## Storage

| Flutter | Valdi |
|---|---|
| `SharedPreferences.getString(key)` | `await store.fetchString(key)` |
| `SharedPreferences.setString(key, value)` | `await store.storeString(key, value)` |
| `FlutterSecureStorage` | `new PersistentStore(name, { enableEncryption: true })` |

```tsx
import { PersistentStore } from 'persistence/src/PersistentStore';

const store = new PersistentStore('my_store', { enableEncryption: true });

// Store
await store.storeString('user_token', token);

// Fetch
const token = await store.fetchString('user_token');

// Remove
await store.remove('user_token');
```

Add to deps: `@valdi//src/valdi_modules/src/valdi/persistence`

## Styling

Flutter uses `ThemeData` and decoration objects. Valdi uses `Style<T>` — typed objects created at module initialization time (never inside `onRender()`).

```dart
// Flutter
Container(
  decoration: BoxDecoration(
    color: Colors.white,
    borderRadius: BorderRadius.circular(12),
    boxShadow: [BoxShadow(blurRadius: 8, color: Colors.black12)],
  ),
  padding: EdgeInsets.all(16),
  child: Text('Card'),
)
```

```tsx
// Valdi — Style defined at module level, not inside onRender()
import { Style } from 'valdi_core/src/Style';
import { View } from 'valdi_tsx/src/NativeTemplateElements';

const cardStyle = new Style<View>({
  backgroundColor: '#ffffff',
  borderRadius: 12,
  boxShadow: '0 2 8 rgba(0,0,0,0.12)',
  padding: 16,
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
> Always create `Style` objects at the module level or as class properties — **never** inside `onRender()`. Valdi interns styles by assigning an integer ID on first use; creating new instances each render defeats this optimization.

For theming (equivalent to Flutter's `ThemeData`), use the [Provider pattern](./advanced-provider.md):

```tsx
const ThemeProvider = createProviderComponentWithKeyName<ThemeService>('ThemeProvider');
```

## Provider Pattern

Flutter's `InheritedWidget` / `provider` package maps to Valdi's `createProviderComponentWithKeyName` + `withProviders` HOC.

```dart
// Flutter
class ThemeNotifier extends ChangeNotifier { ... }
ChangeNotifierProvider(create: (_) => ThemeNotifier(), child: MyApp())
// Consumer: context.watch<ThemeNotifier>()
```

```tsx
// Valdi
import { createProviderComponentWithKeyName } from 'valdi_core/src/provider/createProvider';
import { ProvidersValuesViewModel, withProviders } from 'valdi_core/src/provider/withProviders';

class ThemeService { primary = '#FFFC00'; }

const ThemeProvider = createProviderComponentWithKeyName<ThemeService>('ThemeProvider');

// Root provides value
class AppRoot extends Component {
  private theme = new ThemeService();
  onRender() {
    <ThemeProvider value={this.theme}><App /></ThemeProvider>;
  }
}

// Consumer
interface ThemedViewModel extends ProvidersValuesViewModel<[ThemeService]> {}
class ThemedButton extends Component<ThemedViewModel> {
  onRender() {
    const [theme] = this.viewModel.providersValues;
    <view backgroundColor={theme.primary}><label value="Click me" /></view>;
  }
}
const ThemedButtonWithProvider = withProviders(ThemeProvider)(ThemedButton);
```

See [Provider documentation](./advanced-provider.md) for full details.

## Key Differences to Watch

| Flutter behavior | Valdi behavior |
|---|---|
| `build(BuildContext context)` receives context | `onRender()` takes no parameters — use Provider for DI |
| `const` constructor for widget optimization | No `const` constructors — style interning handles optimization |
| `Widget` is immutable; `State` is mutable | `viewModel` is `ReadOnly<VM>`; `state` is `ReadOnly<State>` |
| `map()` works naturally in `build()` (returns Widget list) | `map()` is **silent no-op** in `onRender()` — use `forEach` |
| Render props via `WidgetBuilder` (`(ctx) => Widget`) | Render props via `() => void` callback called inside `onRender()` |
| `children: [Widget, Widget]` in constructor | Children projected via `<slot />` — see [Slots](./core-slots.md) |
| `GlobalKey` for cross-widget communication | `ElementRef` / `IRenderedElementHolder<T>` |

## See Also

- [The Mighty Component](./core-component.md)
- [Component States](./core-states.md)
- [FlexBox Layout](./core-flexbox.md)
- [Navigation](./navigation.md)
- [Provider](./advanced-provider.md)
- [Working with AI Assistants](./ai-tooling.md)
