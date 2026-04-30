# Migrating from Jetpack Compose to Valdi

Jetpack Compose and Valdi both use a declarative, component-based model for building native UI. If you're coming from Compose, many concepts translate directly — but Valdi uses TypeScript instead of Kotlin, a class-based component model instead of composable functions, and Bazel instead of Gradle.

## Mental Model

| | Jetpack Compose | Valdi |
|---|---|---|
| **Language** | Kotlin | TypeScript |
| **Build system** | Gradle + `build.gradle` | Bazel + `BUILD.bazel` |
| **UI primitive** | `@Composable` function | Class extending `Component` |
| **Platforms** | Android (+ multiplatform beta) | iOS, Android, macOS |
| **Hot reload** | Android Studio Live Edit | `valdi hotreload` |
| **State** | `remember { mutableStateOf() }` | `StatefulComponent` + `this.setState()` |

## Component Model

The biggest structural difference: Compose uses annotated functions, Valdi uses classes.

```kotlin
// Compose — function component
@Composable
fun Greeting(name: String) {
    Text("Hello, $name")
}
```

```tsx
// Valdi — class component
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

### Stateful components

Compose's `remember { mutableStateOf() }` maps to `StatefulComponent` with `this.setState()`:

```kotlin
// Compose
@Composable
fun Counter(label: String) {
    var count by remember { mutableStateOf(0) }
    Button(onClick = { count++ }) {
        Text("$label: $count")
    }
}
```

```tsx
// Valdi
import { StatefulComponent } from 'valdi_core/src/Component';

interface CounterViewModel { label: string; }
interface CounterState { count: number; }

class Counter extends StatefulComponent<CounterViewModel, CounterState> {
  state = { count: 0 };

  // Class arrow function — never inline lambda in JSX props
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
> Valdi does not have hooks. There is no `remember`, `mutableStateOf`, `derivedStateOf`, or `collectAsState`. All state lives in the class via `this.setState()`.

## Lifecycle

| Compose | Valdi |
|---|---|
| `LaunchedEffect(Unit) { ... }` | `onCreate()` |
| `DisposableEffect { onDispose { ... } }` | `onDestroy()` |
| `LaunchedEffect(key) { ... }` | `onViewModelUpdate(previous?)` with key comparison |
| Recomposition triggered by state/remember | `onRender()` triggered by `setState()` or viewModel change |
| `SideEffect { ... }` | Logic inside `onViewModelUpdate` or `onCreate` |

```tsx
class DataComponent extends StatefulComponent<{ userId: string }, { data: Data | null }> {
  state = { data: null };

  onCreate() {
    // Like LaunchedEffect(Unit) — runs once on mount
    this.loadData(this.viewModel.userId);
  }

  onViewModelUpdate(previous?: { userId: string }) {
    // Like LaunchedEffect(userId) — re-runs when userId changes
    if (this.viewModel.userId !== previous?.userId) {
      this.loadData(this.viewModel.userId);
    }
  }

  onDestroy() {
    // Like DisposableEffect onDispose — clean up
  }

  private async loadData(id: string) {
    const data = await fetchData(id);
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

Compose's `Column`, `Row`, and `Box` map to Valdi's flexbox-based `<view>` and `<layout>` elements.

| Compose | Valdi |
|---|---|
| `Column` | `<view flexDirection="column">` (default) |
| `Row` | `<view flexDirection="row">` |
| `Box` | `<view>` with children using `position="absolute"` |
| `Spacer(Modifier.weight(1f))` | `<layout flexGrow={1} />` |
| `Modifier.fillMaxWidth()` | `width="100%"` |
| `Modifier.fillMaxHeight()` | `height="100%"` |
| `Modifier.padding(16.dp)` | `padding={16}` (on any element) |
| `Modifier.background(Color.White)` | `backgroundColor="#ffffff"` |
| `Modifier.clip(RoundedCornerShape(8.dp))` | `borderRadius={8} clipToBounds={true}` |
| `Modifier.clickable { }` | `onTap={this.handler}` on `<view>` |
| `Arrangement.Center` / `Alignment.Center` | `justifyContent="center" alignItems="center"` |

```kotlin
// Compose
Column(
    modifier = Modifier.fillMaxWidth().padding(16.dp),
    verticalArrangement = Arrangement.spacedBy(8.dp)
) {
    Text("Title", style = MaterialTheme.typography.h5)
    Text("Subtitle")
}
```

```tsx
// Valdi
// import { systemFont } from 'valdi_core/src/SystemFont';
onRender() {
  <view flexDirection="column" width="100%" padding={16}>
    <label value="Title" font={systemFont(20)} />;
    <layout height={8} />;
    <label value="Subtitle" />;
  </view>;
}
```

> [!Note]
> Valdi doesn't have `Arrangement.spacedBy`. Use margin/padding on children or a `<layout height={gap} />` spacer.

## Native Elements

| Compose | Valdi |
|---|---|
| `Text(text)` | `<label value={text}>` |
| `TextField` / `OutlinedTextField` | `<textfield>` (single-line) |
| `BasicTextField` | `<textfield>` |
| `Image(painter)` | `<image src={url}>` |
| `AsyncImage` (Coil) | `<image src={url}>` (built-in async loading) |
| `LazyColumn` / `LazyRow` | `<scroll>` + `forEach` (see [Lists](#lists)) |
| `CircularProgressIndicator` | `<spinner>` |
| `LottieAnimation` | `<animatedimage>` |
| `AndroidView` | `<custom-view androidClass="...">` |
| `VideoPlayer` | `<video>` |

## Lists

Compose's `LazyColumn` / `LazyRow` with `items { }` maps to `<scroll>` with `forEach`. Valdi handles viewport-aware rendering automatically.

```kotlin
// Compose
LazyColumn {
    items(users, key = { it.id }) { user ->
        UserRow(user = user)
    }
}
```

```tsx
// Valdi
onRender() {
  <scroll>
    {this.viewModel.users.forEach(user => {
      <UserRow key={user.id} data={user} />;
    })}
  </scroll>;
}
```

> [!Important]
> Do not use `map()`. In Valdi, `onRender()` emits JSX as side-effects — `map()` returns an array that is silently discarded. Use `forEach` or `for...of`.

For horizontal lists:
```tsx
<scroll horizontal={true}>
  {this.viewModel.items.forEach(item => {
    <Card key={item.id} data={item} />;
  })}
</scroll>
```

## State Hoisting & CompositionLocal → Provider

Compose's `CompositionLocalProvider` and state hoisting pattern map to Valdi's Provider pattern.

```kotlin
// Compose — CompositionLocal
val LocalTheme = compositionLocalOf { LightTheme }

CompositionLocalProvider(LocalTheme provides DarkTheme) {
    MyScreen()
}

// Consumer
val theme = LocalTheme.current
```

```tsx
// Valdi — Provider
import { createProviderComponentWithKeyName } from 'valdi_core/src/provider/createProvider';
import { ProvidersValuesViewModel, withProviders } from 'valdi_core/src/provider/withProviders';

class ThemeService { primary = '#FFFC00'; }
const ThemeProvider = createProviderComponentWithKeyName<ThemeService>('ThemeProvider');

// Root
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
    <view backgroundColor={theme.primary}><label value="Click" /></view>;
  }
}
const ThemedButtonWithProvider = withProviders(ThemeProvider)(ThemedButton);
```

See [Provider documentation](./advanced-provider.md).

## Navigation

| Compose (Navigation component) | Valdi |
|---|---|
| `NavHost` | `<NavigationRoot>` |
| `navController.navigate("detail")` | `this.navigationController.push(DetailPage, viewModel)` |
| `navController.popBackStack()` | `this.navigationController.pop()` |
| `ModalBottomSheet` / `Dialog` | `this.navigationController.present(Page, viewModel)` |
| `rememberNavController()` | `this.navigationController` (available on `NavigationPageComponent`) |

## Networking

```kotlin
// Compose / Kotlin coroutines
viewModelScope.launch {
    val response = api.getUsers()
    _users.value = response
}
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
    this.request?.cancel();
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

## Styling

Compose's `Modifier` chain and `MaterialTheme` map to Valdi's `Style<T>` objects and Provider-based theming.

```kotlin
// Compose
Box(
    modifier = Modifier
        .background(Color.White, RoundedCornerShape(12.dp))
        .shadow(elevation = 4.dp)
        .padding(16.dp)
)
```

```tsx
// Valdi — Style at module level, not inside onRender()
import { Style } from 'valdi_core/src/Style';
import { View } from 'valdi_tsx/src/NativeTemplateElements';

const cardStyle = new Style<View>({
  backgroundColor: '#ffffff',
  borderRadius: 12,
  boxShadow: '0 2 8 rgba(0,0,0,0.15)',
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

## Storage

| Kotlin / Android | Valdi |
|---|---|
| `SharedPreferences` | `PersistentStore` |
| `DataStore<Preferences>` | `PersistentStore` |
| `EncryptedSharedPreferences` | `new PersistentStore(name, { enableEncryption: true })` |

## Key Differences to Watch

| Compose | Valdi |
|---|---|
| `@Composable` function annotation | No annotation — class extends `Component` |
| `remember { }` for local state | `state = {}` field on `StatefulComponent` |
| `mutableStateOf` / `collectAsState` | `this.setState({})` |
| `Modifier` chain for styling/layout | Direct attributes on elements (`padding={16}`, `backgroundColor="#fff"`) |
| `LaunchedEffect(key)` for side effects | `onViewModelUpdate(prev)` with key comparison |
| `items { }` in `LazyColumn` | `forEach` inside `<scroll>` — no `map()` |
| `LocalContext.current` | No direct equivalent — use Provider pattern |
| `stringResource(R.string.title)` | Valdi localization module — see [Localization](./advanced-localization.md) |
| `rememberCoroutineScope()` | `onCreate()` / async class methods |

## See Also

- [The Mighty Component](./core-component.md)
- [Component States](./core-states.md)
- [FlexBox Layout](./core-flexbox.md)
- [Navigation](./navigation.md)
- [Provider](./advanced-provider.md)
- [Working with AI Assistants](./ai-tooling.md)
