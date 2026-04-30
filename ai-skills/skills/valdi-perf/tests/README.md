# valdi-perf skill tests

Compile check for the performance patterns taught in `skill.md`.

## What's tested

`src/reference.tsx` exercises:
- Pre-computed viewModels in `onViewModelUpdate` (stable child references)
- `createReusableCallback` for memoized callbacks by argument
- `<layout>` vs `<view>` for invisible containers and spacers
- Stable class arrow functions for render props
- Module-level `Style` objects (not created inside `onRender`)
- `key={item.id}` with stable data IDs in lists (not array indices)

## Compile check

```bash
cd client/src/open_source
bazel build //ai-skills/skills/valdi-perf/tests:tests
```

Expected: `Build completed successfully`

## Updating

When Valdi APIs change (import paths, type signatures), update `src/reference.tsx` to match and keep it compiling. Also update `../skill.md` with any corrected patterns.
