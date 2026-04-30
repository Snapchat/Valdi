# valdi-tsx skill tests

Compile check for the core TSX component patterns taught in `skill.md`.

## What's tested

`src/reference.tsx` exercises:
- Stateless `Component<VM>` with `onRender()` void pattern
- `StatefulComponent` with `state`, `setState`, and all lifecycle methods
- Class arrow functions for event handlers (not inline lambdas)
- `setTimeoutDisposable` for auto-cleanup timers
- `forEach` for list rendering (not `map`)
- `<layout>` spacer (no native view)
- Module-level `Style` objects (`new Style<View>(...)`)
- Provider pattern: `createProviderComponentWithKeyName`, `withProviders`
- `Device.isIOS()` / `isAndroid()` platform detection

## Compile check

```bash
cd client/src/open_source
bazel build //ai-skills/skills/valdi-tsx/tests:tests
```

Expected: `Build completed successfully`

## Updating

When Valdi APIs change (import paths, type signatures), update `src/reference.tsx` to match and keep it compiling. Also update `../skill.md` with any corrected patterns.
