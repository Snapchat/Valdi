# valdi-async skill tests

Compile check for the async lifecycle safety patterns taught in `skill.md`.

## What's tested

`src/reference.tsx` exercises:
- `HTTPClient` + `CancelablePromise`: store in field, cancel in `onDestroy`, cancel before restart in `onViewModelUpdate`
- `registerDisposable` with `setInterval` for auto-cleanup subscriptions
- `isDestroyed()` guard for plain `Promise` / `async/await`
- `setTimeoutInterruptible` for debounce — returns `number`, cancel with `clearTimeout()`
- `promiseToCancelablePromise` wrapping a third-party `Promise`

## Compile check

```bash
cd client/src/open_source
bazel build //ai-skills/skills/valdi-async/tests:tests
```

Expected: `Build completed successfully`

## Updating

When Valdi APIs change (import paths, type signatures), update `src/reference.tsx` to match and keep it compiling. Also update `../skill.md` with any corrected patterns.
