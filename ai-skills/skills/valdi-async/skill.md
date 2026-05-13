# Valdi Async & Lifecycle Safety

Async operations that complete after a component is destroyed will call `setState()`
on a dead component — the framework throws an error. The fix is always to cancel or
guard before the callback fires.

## HTTPClient: Store, Cancel, Repeat

`HTTPClient` methods return `CancelablePromise<HTTPResponse>`. Store it in a typed
field so you can cancel it in `onDestroy()` and before starting a new request in
`onViewModelUpdate()`.

```typescript
import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { CancelablePromise } from 'valdi_core/src/CancelablePromise';
import { HTTPClient } from 'valdi_http/src/HTTPClient';
import { HTTPResponse } from 'valdi_http/src/HTTPTypes';

interface UserState {
  user: { name: string } | null;
  loading: boolean;
}

class UserProfile extends StatefulComponent<UserProfileViewModel, UserState> {
  state: UserState = { user: null, loading: true };

  private client = new HTTPClient('https://api.example.com');
  private request?: CancelablePromise<HTTPResponse>;

  onCreate(): void {
    this.fetchUser(this.viewModel.userId);
  }

  onViewModelUpdate(previous?: UserProfileViewModel): void {
    if (this.viewModel.userId !== previous?.userId) {
      this.request?.cancel?.();          // Cancel in-flight before starting new one
      this.fetchUser(this.viewModel.userId);
    }
  }

  onDestroy(): void {
    this.request?.cancel?.();            // Always clean up
  }

  private fetchUser(id: string): void {
    this.request = this.client.get(`/users/${id}`);
    this.request.then(response => {
      const user = JSON.parse(new TextDecoder().decode(response.body));
      this.setState({ user, loading: false });
    });
  }

  onRender(): void {
    if (this.state.loading) {
      <spinner />;
      return;
    }
    <label value={this.state.user?.name ?? ''} />;
  }
}
```

**Important:** `cancel` is an optional method on `CancelablePromise` — always use
`?.cancel?.()`, not `?.cancel()`.

## registerDisposable: Timers and Subscriptions

For anything that emits over time (timers, event emitters, observables), use
`registerDisposable()`. The framework calls `cancel()` on all registered disposables
in `onDestroy()` automatically — you don't need to override `onDestroy()` for these.

```typescript
class LiveClock extends Component<{}> {
  onCreate(): void {
    // ❌ Timer leaks if component is destroyed
    setInterval(() => this.tick(), 1000);

    // ✅ Automatically cancelled on destroy
    const id = setInterval(() => this.tick(), 1000);
    this.registerDisposable({ cancel: () => clearInterval(id) });
  }

  private tick = () => { /* ... */ };
}
```

Same pattern for any subscription-style API:

```typescript
onCreate(): void {
  const unsubscribe = eventEmitter.on('change', this.handleChange);
  this.registerDisposable({ cancel: unsubscribe });
}
```

## isDestroyed() Guard for Plain Promises

When using `async/await` or plain `Promise` chains that can't be cancelled, guard
`setState()` with `isDestroyed()` before calling it:

```typescript
private async loadData(): Promise<void> {
  const result = await someAsyncOperation();

  if (this.isDestroyed()) return;   // Component unmounted while awaiting

  this.setState({ data: result });
}
```

Prefer `CancelablePromise` + `registerDisposable` over this pattern where possible —
they make cancellation explicit and don't rely on the check being remembered.

## onViewModelUpdate: Cancel Before Restart

When a viewModel update requires fetching new data, always cancel the previous
request before starting a new one. Skipping this means two requests can be in flight
simultaneously and the earlier one can resolve last, overwriting newer data.

```typescript
onViewModelUpdate(previous?: MyViewModel): void {
  if (this.viewModel.query !== previous?.query) {
    this.searchRequest?.cancel?.();           // ← required
    this.searchRequest = this.client.get(`/search?q=${this.viewModel.query}`);
    this.searchRequest.then(response => {
      this.setState({ results: parse(response) });
    });
  }
}
```

## Observable Subscriptions

For RxJS or observable-based APIs, prefer `registerDisposable` for one-off subscriptions
or the `Subscription` class for managing multiple subscriptions as a group:

```typescript
import { Subscription } from 'rxjs';

class FeedComponent extends StatefulComponent<FeedViewModel, FeedState> {
  private subscription = new Subscription();

  onCreate(): void {
    // Add multiple subscriptions to the group
    this.subscription.add(
      this.viewModel.feedItems$.subscribe({ next: items => this.setState({ items }) })
    );
    this.subscription.add(
      this.viewModel.loading$.subscribe({ next: loading => this.setState({ loading }) })
    );
  }

  onDestroy(): void {
    this.subscription?.unsubscribe();   // Cancels all at once
  }
}
```

For a single subscription, `registerDisposable` is simpler:

```typescript
onCreate(): void {
  this.registerDisposable(
    this.viewModel.counter$.subscribe({ next: count => this.setState({ count }) })
  );
  // No onDestroy() override needed
}
```

### Preventing redundant re-renders with distinctUntilChanged

Subscribe with `distinctUntilChanged()` to skip emissions where the value hasn't
actually changed — avoids a `setState` call and a re-render for each identical value:

```typescript
import { distinctUntilChanged } from 'rxjs/operators';

this.subscription.add(
  this.viewModel.title$.pipe(distinctUntilChanged()).subscribe({
    next: title => this.setState({ title }),
  })
);
```

## setTimeoutInterruptible for Debounce / Race Conditions

When a delayed action must be cancelled if conditions change (e.g. search debounce,
retry delay), use `setTimeoutInterruptible` rather than a bare `setTimeout`:

```typescript
import { setTimeoutInterruptible } from 'valdi_core/src/SetTimeout';

// setTimeoutInterruptible returns a number (timer ID) — cancel with clearTimeout()
class SearchBar extends StatefulComponent<SearchViewModel, SearchState> {
  private debounceId?: number;

  onViewModelUpdate(previous?: SearchViewModel): void {
    if (this.viewModel.query !== previous?.query) {
      clearTimeout(this.debounceId);   // Cancel any pending debounce
      this.debounceId = setTimeoutInterruptible(() => {
        this.fetchResults(this.viewModel.query);
      }, 300);
    }
  }
}
```

## promiseToCancelablePromise

When you have a plain `Promise` (e.g. from a third-party library) that needs to participate in Valdi's cancellation system, wrap it with `promiseToCancelablePromise`:

```typescript
import { CancelablePromise, promiseToCancelablePromise } from 'valdi_core/src/CancelablePromise';

class MyComponent extends StatefulComponent<MyViewModel, MyState> {
  private request?: CancelablePromise<string>;

  onCreate(): void {
    const rawPromise: Promise<string> = thirdPartyApi.fetchData();
    this.request = promiseToCancelablePromise(rawPromise, () => {
      // optional cleanup on cancel
    });
    this.request.then(data => {
      if (this.isDestroyed()) return;
      this.setState({ data });
    });
  }

  onDestroy(): void {
    this.request?.cancel?.();
  }
}
```

Prefer `CancelablePromise` directly (e.g. `HTTPClient`) when available — `promiseToCancelablePromise` is the bridge for external APIs.

## Import Paths

```typescript
import { CancelablePromise, promiseToCancelablePromise } from 'valdi_core/src/CancelablePromise';
import { HTTPClient } from 'valdi_http/src/HTTPClient';
import { HTTPResponse } from 'valdi_http/src/HTTPTypes';
import { setTimeoutInterruptible } from 'valdi_core/src/SetTimeout';
import { Subscription } from 'rxjs';
import { distinctUntilChanged } from 'rxjs/operators';
```
