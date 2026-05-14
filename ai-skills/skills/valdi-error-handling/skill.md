# Valdi Error Handling

Error handling, promise rejection, and lifecycle safety patterns.

## When to use

When dealing with async operations, network requests, component lifecycle boundaries, or any code that can fail at runtime.

## Key concepts

Valdi runs on a JavaScript runtime (Hermes/V8) with native platform integration. Unhandled errors crash the app. The primary error sources are:

1. **Async operations** completing after component destruction
2. **Network failures** from HTTPClient
3. **Uncaught promise rejections**
4. **Type mismatches** from native/JS boundary

## CancelablePromise — the first line of defense

Most Valdi errors stem from async work completing after a component is destroyed. `CancelablePromise` + `registerDisposable` prevents this entirely:

```tsx
import { promiseToCancelablePromise } from 'valdi_core/src/CancelablePromise';

// ✅ Safe async — cancelled on component destruction
onCreate() {
  const cancelable = promiseToCancelablePromise(this.fetchData(), () => {});
  this.registerDisposable(() => cancelable.cancel?.());
  cancelable.then(
    data => { this.setState({ data, loading: false }); },
    error => { this.setState({ error: error.message, loading: false }); },
  );
}
```

```tsx
// ❌ WRONG — Raw promise with no lifecycle safety
onCreate() {
  this.fetchData().then(data => {
    this.setState({ data });  // Crashes if component is destroyed
  });
}
```

See the `valdi-async` skill for comprehensive CancelablePromise patterns.

## HTTPClient error handling

`HTTPClient` is an instance, not static. Methods return `CancelablePromise<HTTPResponse>`.

```tsx
import { HTTPClient } from 'valdi_http/src/HTTPClient';

private httpClient = new HTTPClient('https://api.example.com');

// ✅ Handle network errors with lifecycle safety
private fetchData = () => {
  const cancelable = this.httpClient.get('/data');
  this.registerDisposable(() => cancelable.cancel?.());
  cancelable.then(
    response => {
      if (response.statusCode !== 200) {
        this.setState({ error: `HTTP ${response.statusCode}`, loading: false });
        return;
      }
      const decoder = new TextDecoder();
      const data = JSON.parse(decoder.decode(response.body));
      this.setState({ data, loading: false });
    },
    error => {
      this.setState({ error: (error as Error).message, loading: false });
    },
  );
};
```

`HTTPResponse` shape: `{ statusCode: number, headers: StringMap<string>, body?: Uint8Array }`. Decode `body` with `TextDecoder` before parsing.

## isDestroyed() guard

For callbacks that can't use CancelablePromise (e.g., native callbacks, timers):

```tsx
// ✅ Guard against use-after-destroy
private onNativeCallback = (result: string) => {
  if (this.isDestroyed()) return;
  this.setState({ result });
};
```

`isDestroyed()` is a method — always call with parentheses.

## try/catch in async methods

```tsx
import { promiseToCancelablePromise } from 'valdi_core/src/CancelablePromise';

// ✅ Catch errors from async operations
private loadProfile = () => {
  const cancelable = promiseToCancelablePromise(
    (async () => {
      try {
        const profile = await this.api.getProfile();
        return { profile, error: undefined };
      } catch (e) {
        return { profile: undefined, error: (e as Error).message };
      }
    })(),
    () => {},
  );
  this.registerDisposable(() => cancelable.cancel?.());

  cancelable.then(result => {
    this.setState({
      profile: result.profile,
      error: result.error,
      loading: false,
    });
  });
};
```

## Error states in UI

```tsx
// ✅ Show error state to the user
onRender(): void {
  if (this.state.loading) {
    <AnimationShimmer width="100%" height={200} />;
    return;
  }

  if (this.state.error) {
    <view alignItems="center" justifyContent="center" padding={16}>
      <label value="Something went wrong" font={systemBoldFont(16)} />
      <label value={this.state.error} color="#999" />
      <CoreButton text="Retry" onTap={this.handleRetry} />
    </view>;
    return;
  }

  <label value={this.state.data.name} />;
}
```

## Defensive ViewModel access

ViewModels come from parent components and may have optional fields:

```tsx
// ✅ Defensive access with defaults
onRender(): void {
  const title = this.viewModel.title ?? 'Untitled';
  const items = this.viewModel.items ?? [];

  <view>
    <label value={title} />
    {items.forEach(item => { <label value={item.name} />; })}
  </view>;
}
```

## Timer cleanup

```tsx
// ✅ Use setTimeoutDisposable for automatic cleanup
onCreate() {
  this.setTimeoutDisposable(this.tick, 1000);
}
```

```tsx
// ✅ Manual cleanup for setInterval
private timerId?: number;

onCreate() {
  this.timerId = setInterval(this.tick, 1000);
}

onDestroy() {
  if (this.timerId !== undefined) {
    clearInterval(this.timerId);
  }
}
```

Or use `registerDisposable` with a disposable wrapper — see `valdi-async` skill.

## Common mistakes

```tsx
// ❌ WRONG — HTTPClient is not static
const data = await HTTPClient.send(request);  // No static send() method

// ✅ Correct — instantiate then call methods
const client = new HTTPClient('https://api.example.com');
const cancelable = client.get('/data');

// ❌ WRONG — .catch() is not available on CancelablePromise
cancelable.catch(error => { ... });  // CancelablePromise only has .then()

// ✅ Correct — use second argument of .then() for error handling
cancelable.then(
  response => { /* success */ },
  error => { /* failure */ },
);

// ❌ WRONG — React error boundaries don't exist
<ErrorBoundary fallback={<Error />}>  // Not a Valdi concept
  <MyComponent />
</ErrorBoundary>

// ❌ WRONG — window.onerror / global error handlers
window.addEventListener('error', handler);  // No window object

// ❌ WRONG — isDestroyed without parentheses
if (this.isDestroyed) return;  // It's a method, not a property

// ✅ Correct
if (this.isDestroyed()) return;

// ❌ WRONG — Passing CancelablePromise directly to registerDisposable
this.registerDisposable(cancelable);  // Wrong type

// ✅ Correct — wrap cancel in a callback
this.registerDisposable(() => cancelable.cancel?.());
```

## Summary

| Pattern | When to use |
|---------|------------|
| `promiseToCancelablePromise` + `registerDisposable` | Any async work in a component |
| `isDestroyed()` guard | Native callbacks, external event handlers |
| `try/catch` | Operations that can throw |
| Error state in UI | User-facing failure recovery |
| `onDestroy` cleanup | Timers, subscriptions, manual resources |
| `setTimeoutDisposable` | One-shot timers (auto-cleaned) |
