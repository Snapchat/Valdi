# valdi-component-tests skill tests

Reference spec file demonstrating all test utility patterns from `skill.md`.

## Files

| File | Description |
|------|-------------|
| `src/ProfileCard.tsx` | Component under test — has an image, labels, conditional child, tap handler |
| `test/ProfileCardTest.spec.tsx` | Spec file exercising all test utilities |

## What's demonstrated

`test/ProfileCardTest.spec.tsx` exercises:
- `elementKeyFind<ImageView>` — typed `getAttribute('src')` on image element
- `elementKeyFind` — untyped string attribute access
- `elementKeyFind<View>` — typed `getAttribute('onTap')` for direct invocation
- `elementTypeFind` — find elements by type (`IRenderedElementViewClass.Label`)
- `componentTypeFind` — conditional child component presence/absence
- `tapNodeWithKey` — async tap triggering a spy callback
- `fail.bind(null, '...')` — factory function default for callbacks that must not fire
- `valdiIt` — test runner with driver

## Run tests

```bash
cd client/src/open_source
bazel test //ai-skills/skills/valdi-component-tests/tests:tests
```

Expected: all jasmine specs pass.

## Updating

When test utility APIs change, update `test/ProfileCardTest.spec.tsx` to match and keep it passing. Also update `../skill.md` with any corrected patterns.
