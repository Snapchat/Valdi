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

## Style Objects at Module Level

`new Style<T>({...})` interns style objects — the same property values always produce
the same cached object. This interning only works at module initialization time.
Inside `onRender()` the cache is bypassed and a new allocation happens every render.

```typescript
// ❌ Defeats interning — new allocation every render
onRender(): void {
  const s = new Style<View>({ backgroundColor: '#fff', borderRadius: 8 });
  <view style={s} />;
}

// ✅ Interned at module level
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
