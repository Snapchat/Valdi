# Valdi Performance Patterns

Valdi re-renders a child component whenever its viewModel reference changes. Most
unnecessary re-renders come from creating new object/array/function references in
`onRender()`. Fix the reference — fix the re-render.

## ViewModel Identity Stability

**The #1 performance problem in Valdi apps.** Every object or array literal created
inside `onRender()` is a new reference. Child components will always re-render, even
when the actual values haven't changed.

```typescript
// ❌ New object every render — child always re-renders
onRender(): void {
  <UserRow vm={{ name: this.viewModel.user.name, age: this.viewModel.user.age }} />;
}

// ❌ New array every render
onRender(): void {
  <TabBar tabs={['Home', 'Profile', 'Settings']} />;
}

// ✅ Stable class property for constants
private tabs = ['Home', 'Profile', 'Settings'];
onRender(): void {
  <TabBar tabs={this.tabs} />;
}

// ✅ Pre-compute derived viewModels in onViewModelUpdate
private userRowVM: UserRowViewModel = { name: '', age: 0 };

onViewModelUpdate(): void {
  this.userRowVM = { name: this.viewModel.user.name, age: this.viewModel.user.age };
}

onRender(): void {
  <UserRow vm={this.userRowVM} />;
}
```

Only update the pre-computed VM when the relevant input actually changes:

```typescript
onViewModelUpdate(previous?: UserProfileViewModel): void {
  if (this.viewModel.userId !== previous?.userId) {
    this.userRowVM = buildUserRowVM(this.viewModel.user);
  }
}
```

## Navigation Callbacks

Navigation callbacks passed into child viewModels have the same identity problem:
`() => this.navigationController.push(...)` creates a new function each render.
Use a class arrow function — it is defined once and has a stable reference:

```typescript
// ❌ New function every render
onRender(): void {
  <UserCard onTap={() => this.navigationController.push(DetailPage, { id: this.viewModel.userId })} />;
}

// ✅ Class arrow function — stable reference, viewModel.userId read at tap time
private goToDetail = (): void => {
  this.navigationController.push(DetailPage, { id: this.viewModel.userId });
};

onRender(): void {
  <UserCard onTap={this.goToDetail} />;
}
```

## `<layout>` vs `<view>`

`<view>` allocates a native platform view. `<layout>` is virtual — it participates in
flexbox layout but creates no native view, which is faster and uses less memory.

```typescript
// ❌ Native view wasted on an invisible spacer
<view height={16} />

// ✅ No native view allocated
<layout height={16} />

// ❌ Wrapper with no visual properties or tap handler
<view flexDirection="column">
  <label value="A" />;
  <label value="B" />;
</view>;

// ✅ Virtual layout node
<layout flexDirection="column">
  <label value="A" />;
  <label value="B" />;
</layout>;
```

**Use `<view>` when you need:** `onTap`, `backgroundColor`, `borderRadius`, `style`,
`overflow`, `opacity`, or any visual/interactive property.
**Use `<layout>` for everything else:** spacers, invisible wrappers, structural containers.

## Keys in Lists

Keys determine element identity across re-renders. Without a key (or with an index
key), reordering or inserting items causes the wrong component instances to receive
the wrong viewModels.

```typescript
// ❌ No key — identity lost on reorder
{this.viewModel.items.forEach(item => {
  <ItemRow value={item.name} />;
})}

// ❌ Index key — breaks on insert/remove
{this.viewModel.items.forEach((item, index) => {
  <ItemRow key={String(index)} value={item.name} />;
})}

// ✅ Stable data ID
{this.viewModel.items.forEach(item => {
  <ItemRow key={item.id} value={item.name} />;
})}
```

## Render Props as Class Arrow Functions

When a parent needs to pass a render function to a child (e.g. a list row renderer),
define it as a class arrow function property so it has a stable reference:

```typescript
// ❌ New function every render — child's renderItem prop always changes
onRender(): void {
  <List renderItem={(item) => { <Row data={item} />; }} />;
}

// ✅ Stable class arrow function
private renderItem = (item: Item): void => {
  <Row data={item} />;
};

onRender(): void {
  <List renderItem={this.renderItem} />;
}
```

For loop closures that must capture a loop variable, use `createReusableCallback`
inline in `onRender()`. Valdi's diffing engine recognises `Callback` objects and
updates the internal function reference without treating it as a prop change, so the
child does not re-render:

```typescript
import { createReusableCallback } from 'valdi_core/src/utils/Callback';

// ❌ New plain function every render — child always re-renders
onRender(): void {
  {this.viewModel.sections.forEach((section, i) => {
    <Section onTap={() => this.handleTap(i)} />;
  })}
}

// ✅ Inline Callback — identity-merged by Valdi's diffing engine
onRender(): void {
  {this.viewModel.sections.forEach((section, i) => {
    <Section onTap={createReusableCallback(() => this.handleTap(i))} />;
  })}
}
```

## Renderer memoization with `remember`

`remember(factory, ...keys)` (from `valdi_core/src/Remember.ts`)
caches the result of `factory()` across renders and only recomputes it when one of
the trailing keys changes; otherwise it returns the cached value. Reach for it when
a value is expensive to derive inside `onRender()` but depends on just a few inputs.

```typescript
import { remember } from 'valdi_core/src/Remember';

// ❌ Rebuilds the index every render
onRender(): void {
  const index = buildIndex(this.viewModel.items);
  <SearchResults index={index} />;
}

// ✅ Rebuilt only when `items` changes; cached otherwise
onRender(): void {
  const index = remember(() => buildIndex(this.viewModel.items), this.viewModel.items);
  <SearchResults index={index} />;
}
```

How it relates to the other primitives here:
- **vs. pre-computing derived viewModels in `onViewModelUpdate`** — same goal (recompute
  only on real input changes), but `remember` keeps the derivation inline in `onRender()`
  keyed by its dependencies, instead of hoisting it into a lifecycle method plus a guarded
  field. Use `remember` for render-local values; keep the `onViewModelUpdate` pattern when
  the result is a child viewModel whose *reference identity* must stay stable to avoid
  re-rendering the child.
- **vs. `createReusableCallback`** — `createReusableCallback` stabilises a *function's*
  identity across renders; `remember` caches a computed *value*.
- **vs. `cacheObservable`** — `cacheObservable` shares one subscription across a producer
  chain; `remember` is synchronous, render-scoped memoization with no Observable involved.

## Style Objects at Module Level

Create each `Style<T>` **once at module level and reuse the instance**. A `Style` is
not value-interned — two `new Style({...})` calls with identical properties are still
two distinct objects. What a `Style` caches is its own native serialization:
`toNative()` is memoized per instance. So a module-level `Style` pays that marshalling
cost once and reuses it, while a `Style` constructed inside `onRender()` re-allocates
and re-serializes on every render.

```typescript
// ❌ New allocation + re-serialization every render
onRender(): void {
  const s = new Style<View>({ backgroundColor: '#fff', borderRadius: 8 });
  <view style={s} />;
}

// ✅ Constructed once at module level; native serialization cached on the instance
import { View } from 'valdi_tsx/src/NativeTemplateElements';

const styles = {
  card: new Style<View>({ backgroundColor: '#fff', borderRadius: 8 }),
};

class MyCard extends Component<MyViewModel> {
  onRender(): void {
    <view style={styles.card} />;
  }
}
```

Group styles in a `const styles = {}` object after the class definition.

## Observable Subscriptions in `onCreate()` Block First Paint

Calling `.subscribe()` on a BridgeObservable during `onCreate()` creates the
native bridge and runs the initial value flow synchronously, delaying first
paint. Each subscription compounds the delay, and `combineLatest` over several
sources makes it worse because every bridge is established before the
component can paint.

```typescript
// ❌ Each subscription adds synchronous work before first paint
onCreate(): void {
  this.registerDisposable(this.context.dataUpdates.subscribe(d => this.setState({ data: d })));
  this.registerDisposable(this.context.statusUpdates.subscribe(s => this.setState({ status: s })));
}
```

Defer subscriptions whose data isn't needed for first paint by wrapping in
`setTimeout(0)`, pushing them to the next tick:

```typescript
// ✅ Critical stays synchronous; non-critical deferred to next tick
onCreate(): void {
  this.registerDisposable(this.context.essentialData.subscribe(d => this.setState({ data: d })));

  const tag = setTimeout(() => {
    this.registerDisposable(this.context.badgeCount.subscribe(c => this.setState({ badgeCount: c })));
  }, 0);
  this.registerDisposable({ unsubscribe: () => clearTimeout(tag) });
}
```

Only defer subscriptions whose data isn't needed for the initial render.
Deferring something the first paint depends on (primary content, user identity)
causes a visible empty-then-populated flash.

## Moving heavy computation to a Worker Service

TypeScript in Valdi runs on a single-threaded event loop. Inherently expensive
TypeScript work competes with Valdi rendering on that same thread. A Worker
Service moves the work to a separate Worker thread with its own JS heap, so the
host thread stays free.

**When to reach for it:** business logic that involves expensive synchronous
TypeScript (parsing, transforms, in-language computation) and the caller can
tolerate an asynchronous API. All host↔worker calls are promise-based.

**Why it isn't free:**
- Workers have their own memory heap. Arguments and return values are deep-copied
  across the boundary on every call. Design APIs to send infrequent, coarse-grained
  batches rather than many small calls.
- Bridged TypeScript functions cannot return values synchronously. Anything that
  needs an inline value cannot live on a worker.
- Only own properties of an object are copied across the boundary. Instance
  methods on plain TypeScript classes are stripped; use `@ExportProxy`-backed
  native objects when methods must survive the crossing.
- Each `WorkerServiceExecutors` value is its own thread + heap. Adding executors
  raises overall memory usage; passing objects across different executors also
  clones.
- Mutex-protected shared mutable state across threads (as in C++/Swift/Kotlin)
  is not supported.

**Don't confuse with `setTimeout(0)`.** `setTimeout(0)` reschedules work to a
later tick on the same JS thread, so the work still runs on the rendering
thread, just later. A Worker Service moves the work to a different thread.

**API entry points:** `startWorkerService`, `useOrStartWorkerService`,
`startWorkerServiceInForeground`, `useWorkerService`, `useWorkerServiceSingleton`.
A service is declared by a `WorkerServiceEntryPoint` subclass annotated with
`@workerService(executor, module)`. See the Valdi docs for the full API.
Always call `dispose()` on the returned client; the service otherwise stays in
memory.

**Structured messaging with `MessageChannel` (recent).** For richer
host↔worker communication than one-shot request/response, the runtime installs the
web-standard, native-backed `MessageChannel` / `MessagePort` globals. `new
MessageChannel()` gives a linked `port1` / `port2` pair; hand one port to the worker
and keep the other. Each port has `postMessage<T>(data, transfer?)` and an `onmessage`
handler; messages queue in FIFO order until the receiving port is started (assigning
`onmessage` or calling `start()`). Ports are themselves transferable — pass them in
the `transfer` array to move a channel across an existing port. Reach for this when a
feature needs multiple independent, long-lived message streams to a worker rather than
the single promise-based call surface above.

```typescript
const channel = new MessageChannel();
channel.port1.onmessage = (event) => this.handle(event.data);
worker.postMessage({ cmd: 'attach' }, [channel.port2]);  // transfer port2 to the worker
```

**Idle workers are now torn down aggressively.** The runtime reclaims
resources by terminating idle Worker Services, so a worker may be gone between uses and
re-spun on the next call. Factor the (re)spin-up cost into the offloading tradeoff:
infrequent, bursty work now pays a cold-start penalty each time, weakening the case for
a worker versus keeping cheap work on the host thread. Steady, frequent workloads keep
the worker warm and still win.

## Deferring construction until it's needed

Three Valdi-supported ways to push work past startup, from a single field to
a whole module:

### Field-level: `Lazy<T>`

Wrap construction of an expensive instance field in `Lazy<T>` from
`foundation/src/Lazy`. The thunk runs once on first read of `.target`, then
caches the result and drops itself so captured closure refs become GC-eligible.
Use for fields needed only on conditional code paths (taps, modal openings,
error flows).

```typescript
import { Lazy } from 'foundation/src/Lazy';

// ❌ Pays construction cost at instantiation, even if never read
private readonly defaultStatus = this.statusProvider.getDefaultStatus();

// ✅ Constructed on first .target read, then cached
private readonly defaultStatus = new Lazy(() => this.statusProvider.getDefaultStatus());

// Read site:
const status = this.defaultStatus.target;
```

Good fits: expensive dependency resolution, derived collections, formatters,
parsers, anything you would otherwise build in the constructor "just in case".

### Context dependencies: `Provider<T>`

Type a context field as `Provider<T>` (from `foundation/src/Provider`) instead
of `T` to defer native DI construction until the component calls `.get()`. Use
for context dependencies that are expensive to construct natively and only
needed on conditional code paths.

```typescript
import { Provider } from 'foundation/src/Provider';

export interface MyContext {
  storyPlayer: IStoryPlayer;            // ❌ Built at context creation
  storyPlayer: Provider<IStoryPlayer>;  // ✅ Built on first .get() call
}

// Read site:
const player = this.context.storyPlayer.get();
```

Wrap with `providerToLazy` for a TS-side cache when `.get()` is read repeatedly:

```typescript
import { providerToLazy } from 'foundation/src/Provider';

private readonly storyPlayer = providerToLazy(this.context.storyPlayer);
const player = this.storyPlayer.target;   // first read invokes Provider.get(), cached
```

### Whole modules: dynamic `import()`

The Valdi compiler supports `await import('module/path')` for code-splitting.
The imported module is parsed and initialised on first use, then cached.

```typescript
// ❌ Top-of-file: pays parse + init cost at startup, even if never used
import { HeavyScreen } from './HeavyScreen';
this.navigationController.target?.present(HeavyScreen, ...);

// ✅ Dynamic: module loads on first call, then cached
const { HeavyScreen } = await import('./HeavyScreen');
this.navigationController.target?.present(HeavyScreen, ...);
```

**Pays off when:** the module is large or has expensive top-level initialisation
and the code path is conditional (modals, edit flows, error views) where most
users never hit it.

**Skip when:** the module is needed on the initial render path (dynamic import
adds an async tick) or the module is tiny (savings are negligible).

### Whole modules, synchronously: `lazyImport` (recently added)

When you want to defer a module's parse/init cost but can't afford the async tick
of `import()`, use `lazyImport` from `valdi_core/src/LazyImport`.
It returns a `LazyImport<TModule>` whose `.get` synchronously requires the module on
first access and caches it thereafter — no `await`, no render-path tick.

```typescript
import { lazyImport } from 'valdi_core/src/LazyImport';

// Pass the current module's `require` so the relative path resolves from here.
const heavy = lazyImport<typeof import('./HeavyUtil')>(require, './HeavyUtil');

// First `.get` parses + initialises the module; later reads are cached.
heavy.get.doWork();
```

**Prefer over `import()`** when the consumer is synchronous (can't await) but the
module is still only needed conditionally. **Prefer `import()`** when you genuinely
want the module off the startup bundle / loaded asynchronously.

## Excessive `setState` calls

`setState` runs synchronously: each call merges state and immediately
re-renders. There is no frame-level batching, so N calls in a row produce
N renders.

```typescript
// ❌ Three renders for one logical update
this.setState({ items });
this.setState({ loading: false });
this.setState({ lastFetchedAt: now });

// ✅ One render
this.setState({ items, loading: false, lastFetchedAt: now });
```

Common high-frequency sources:
- **Frequently-emitting Observables (especially BridgeObservables).** A single
  `setState` inside `.subscribe()` looks innocent but renders N times if the
  Observable emits N times for the same logical change. Fix upstream of the
  subscribe call: `distinctUntilChanged(arrayEquals)` / `distinctUntilChanged(setsEquivalent)`
  for object emissions (default identity check won't dedup freshly-allocated
  arrays/sets), or `debounceTime(...)` for chatty streams that would cause
  visible flicker. For a BridgeObservable, also check whether the native
  producer is emitting per-field instead of batching related changes; the real
  fix may live there.
- Scroll, drag, and animation-tick callbacks. Throttle, debounce, or use
  `setStateAnimated` rather than per-frame `setState`.
- Loops that emit items as they arrive. Accumulate locally and `setState` once
  at the end of the batch.
- `onViewModelUpdate` that calls `setState` unconditionally. Guard with
  `if (this.viewModel.x !== previous?.x)` so unrelated viewModel updates don't
  trigger work.

## Re-subscribing to expensive observables

Each `.subscribe()` runs the producer chain from scratch (HTTP, query, expensive
transform). Two callsite patterns make this costly:
- Multiple subscribers to the same source.
- A `switchMap` that swaps inner observables. Each switch tears down the
  previous subscription and starts a fresh producer chain.

`cacheObservable(disposable)` from
`rxjs_extensions/src/operators/cacheObservable` shares a single subscription
behind `shareReplaySafe`. The disposable (usually the owning class's lifetime)
keeps the cache alive; all subscribers and `switchMap` re-subscriptions get
the cached emission instead of re-running the producer.

```typescript
import { cacheObservable } from 'rxjs_extensions/src/operators/cacheObservable';
import { startWithTimeout } from 'rxjs_extensions/src/operators/startWithTimeout';

class FriendSearcher {
  // ❌ Every subscriber re-runs the whole producer chain
  private readonly results$ = this.source$.pipe(
    map(friends => buildIndex(friends)),
  );

  // ✅ Producer runs once; downstream subscribers and switchMap
  //    re-subscriptions hit the cache.
  private readonly results$ = this.source$.pipe(
    map(friends => buildIndex(friends)),
    startWithTimeout([], 500),
    cacheObservable(this.disposable),
  );
}
```

Smell: a `switchMap` whose inner observable does HTTP or expensive computation
with no caching upstream. Each switch tears down the previous subscription
and starts a fresh producer chain.

## Native sticky headers (experimental)

Sticky headers implemented in JS lag the scroll: the scroll offset round-trips to
the JS thread, the header position is recomputed, and the update lands a frame or
more behind the gesture. Native sticky headers reposition the header in the native
scroll pass — the same VSYNC as the scroll gesture — eliminating that round-trip.

Enable it on the `<scroll>` element and tag the descendant that should pin:

```typescript
// ✅ Repositioning happens natively, in sync with the gesture
<scroll nativeStickyEnabled={true}>
  <layout>
    <view stickyPosition="top">...header...</view>
    ...section content...
  </layout>
</scroll>
```

- `nativeStickyEnabled` (default `false`) — turns on native repositioning for the
  scroll view.
- `stickyPosition` (`'none' | 'top'`, default `'none'`) — tags a descendant as a
  sticky header within the nearest `nativeStickyEnabled` scroll view.
- `nativeStickyCover` (default `0`) — pixels of visual overhang above sticky headers,
  extending the effective header height so a bar rendered above the header does not
  overlap the next section's header.
- `nativeStickyOffset` (default `0`) — pixels below the viewport top where headers
  pin (matches CSS `position: sticky; top: N`); use when a floating page header has a
  visual footprint extending below its Yoga bounds.

All four attributes are `@experimental` (may change). Prefer this over a JS-side
sticky implementation on scroll-heavy screens where header lag is visible.
