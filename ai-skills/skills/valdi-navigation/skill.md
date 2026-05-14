# Valdi Navigation

Multi-screen navigation patterns using the `valdi_navigation` module.

## When to use

When building apps with multiple screens/pages, modal presentations, or any push/pop navigation flow.

## Key concepts

Valdi navigation follows an **iOS-style model**: a `NavigationRoot` manages a stack of pages. Pages are pushed (horizontal slide) or presented (vertical modal). Each page component must be annotated with `@NavigationPage(module)`.

**Not React Router.** There is no `<Route>`, no URL-based routing, no `useNavigate()`. Navigation is imperative via `NavigationController`.

## Setup

Add `valdi_navigation` to your module's `BUILD.bazel` deps:

```python
deps = [
    "//src/valdi_modules/src/valdi/valdi_navigation",
]
```

## NavigationRoot

The root of your navigation tree. Exposes a `NavigationController` via a slot:

```tsx
// ✅ Correct — NavigationRoot with slot pattern
import { NavigationRoot } from 'valdi_navigation/src/NavigationRoot';
import { NavigationController } from 'valdi_navigation/src/NavigationController';

export class App extends Component {
  private navController?: NavigationController;

  onRender(): void {
    <NavigationRoot>
      {$slot(navController => {
        this.navController = navController;
        <view width="100%" height="100%">
          <CoreButton text="Go to Settings" onTap={this.openSettings} />
        </view>;
      })}
    </NavigationRoot>;
  }

  private openSettings = () => {
    this.navController?.push(SettingsPage, { title: 'Settings' }, {});
  };
}
```

```tsx
// ❌ WRONG — React Router patterns don't exist
<Router>
  <Route path="/settings" component={SettingsPage} />  // ❌
</Router>
const navigate = useNavigate();  // ❌
navigate('/settings');            // ❌
```

## NavigationPageComponent

Pages must extend `NavigationPageComponent` or `NavigationPageStatefulComponent` and use the `@NavigationPage` decorator:

```tsx
// ✅ Correct — page with @NavigationPage decorator
import { NavigationPage } from 'valdi_navigation/src/NavigationPage';
import { NavigationPageStatefulComponent } from 'valdi_navigation/src/NavigationPageComponent';

interface SettingsViewModel {
  title: string;
}

interface SettingsState {
  darkMode: boolean;
}

@NavigationPage(module)
export class SettingsPage extends NavigationPageStatefulComponent<SettingsViewModel, SettingsState> {
  state = { darkMode: false };

  onRender(): void {
    <view width="100%" height="100%" backgroundColor="white">
      <label value={this.viewModel.title} />
      <CoreButton text="Back" onTap={this.handleBack} />
      <CoreButton text="Open Detail" onTap={this.handleOpenDetail} />
    </view>;
  }

  private handleBack = () => {
    this.navigationController.pop();
  };

  private handleOpenDetail = () => {
    this.navigationController.push(DetailPage, { id: '123' }, {});
  };
}
```

## Navigation operations

| Method | Animation | Use case |
|--------|-----------|----------|
| `push(Component, viewModel, context, options?)` | Horizontal slide right | Drill-down navigation |
| `pop(animated?)` | Horizontal slide left | Go back one level |
| `popToSelf(animated?)` | Unwinds stack | Return to this page |
| `popToRoot(animated?)` | Unwinds full stack | Return to root |
| `present(Component, viewModel, context, options?)` | Vertical slide up | Modal presentation |
| `dismiss(animated)` | Vertical slide down | Close modal |

## Navigation options

```tsx
// Push with custom options
this.navController?.push(DetailPage, viewModel, context, {
  animated: true,                // default: true
  pageBackgroundColor: '#FFFFFF', // page container color
});

// Present as modal with nested push/pop support (iOS)
this.navController?.present(ModalPage, viewModel, context, {
  animated: true,
  wrapInPlatformNavigationController: true, // default: true, enables push/pop inside modal
});
```

## Android back button

```tsx
// ✅ Handle Android hardware back button
onCreate() {
  this.unregisterBack = this.navigationController.registerBackButtonObserver(() => {
    // Custom back behavior
    this.saveAndGoBack();
  });
}

onDestroy() {
  this.unregisterBack?.();
}
```

## Page visibility

```tsx
// ✅ Know when page becomes visible/hidden (e.g., after pop returns to this page)
onCreate() {
  this.navigationController.addPageVisibilityObserver(this.onVisibilityChange);
}

private onVisibilityChange = (visibility: INavigatorPageVisibility) => {
  // React to page becoming visible again
};

onDestroy() {
  this.navigationController.removePageVisibilityObserver(this.onVisibilityChange);
}
```

## Common mistakes

```tsx
// ❌ WRONG — Forgetting @NavigationPage decorator
export class MyPage extends NavigationPageComponent<{}> { ... }
// Runtime error: "Component has not been decorated with @NavigationPage"

// ❌ WRONG — Using NavigationPageComponent outside a NavigationRoot
// Pages must be rendered inside a NavigationRoot's stack

// ❌ WRONG — Passing navigator in context manually
navController.push(Page, vm, { navigator: navController }); // ❌
// Navigator is injected automatically — don't pass it in context

// ✅ Correct — pass empty context if page needs no extra context
navController.push(Page, vm, {});
```

## Disable interactive dismissal

```tsx
// Prevent swipe-to-dismiss on a modal (e.g., during form editing)
const reenable = this.navigationController.disableDismissalGesture();
// Later, when safe to dismiss:
reenable();
```
