# Putting a change behind a flag

Some parts of Valdi have caused most of its serious regressions. They're listed, with their paths, in [`.github/hot-systems.yml`](../../.github/hot-systems.yml). The rest of the framework source is covered by a catch-all `framework` entry, and a bot comments on pull requests that touch any of it. A **significant** change to framework code should ship behind a runtime tweak that defaults to the old behavior, so the app embedding Valdi can turn it on gradually and turn it off without a new release. [CONTRIBUTING.md](../../CONTRIBUTING.md#safe-changes) explains what counts as significant.

This page covers how to add that tweak. The rules behind it are in [AGENTS.md, "Runtime Configuration"](../../AGENTS.md#runtime-configuration-flags-and-tweaks).

## When you need one

| Gate in `hot-systems.yml` | What to do for a significant change |
|---|---|
| `flag` (runtime systems: marshalling, teardown, threading, resource loading, images, text, the renderer, gestures, view recycling, and other framework code) | Add a runtime tweak that defaults to the old behavior. |
| `guard` (build-time systems: the compiler, JS engines, the build, OS compatibility, the layout engine) | A runtime tweak can't cover these. Say so in the pull request and include the test results for the affected area. |

Comments, tests, renames and fixes that only add a null check aren't significant and don't need a flag. Say why in the pull request description.

## 1. Name it

```
VALDI_<ENABLE|DISABLE>_<AREA>_<CHANGE>[_IOS|_ANDROID]
```

- `ENABLE` turns on new behavior and defaults to `false`. Use it for new changes.
- `DISABLE` is a kill switch for behavior that has already shipped. It also defaults to `false`; `true` falls back to the old path.
- `AREA` is the system's `id` from `hot-systems.yml` in upper case, for example `TEARDOWN`, `MARSHAL`, `THREAD`, `TEXT`.
- `CHANGE` is two to four words describing the new behavior.
- Add `_IOS` or `_ANDROID` only when the flag exists on one platform.
- Values that tune behavior (sizes, timeouts) aren't gates. Name them `VALDI_<AREA>_<PARAM>`.

Don't rename a flag once it has shipped: hosts that already set the old name would silently lose the value.

## 2. Add the tweak

Behavior decided in the C++ runtime goes in `ValdiRuntimeTweaks`. Use the platform config, `ValdiTweaks` (Android) or `SCValdiConfiguration` (iOS), only when the decision can't be made in C++; see the AGENTS.md rules.

1. Declare the getter in `valdi/src/valdi/runtime/ValdiRuntimeTweaks.hpp`, with a comment giving the key, the default and what `true` does:

   ```cpp
   // VALDI_ENABLE_TEARDOWN_STOP_ORPHANED_WORKERS (default false). When true, ...
   bool enableTeardownStopOrphanedWorkers() const;
   ```

2. Implement it in `ValdiRuntimeTweaks.cpp`. The fallback is the **old** behavior:

   ```cpp
   bool ValdiRuntimeTweaks::enableTeardownStopOrphanedWorkers() const {
       auto configKey =
           StringCache::getGlobal().makeStringFromLiteral(std::string_view("VALDI_ENABLE_TEARDOWN_STOP_ORPHANED_WORKERS"));
       return _tweakValueProvider->getBool(configKey, false);
   }
   ```

3. **Null-check at every call site.** The tweaks object is null until a host attaches a provider, and stays null in hosts that never do. Fall back to the same value as the getter's default.
4. **If the value is needed early**, apply it when the provider attaches (`RuntimeManager::setTweakValueProvider`) or read it when it's used. Don't move a C++ setting to the platform config just for timing.
5. **If the value must hold through teardown**, keep a copy on the object that needs it. The provider may be gone by the time teardown runs.
6. **Cover the whole change.** Every behavior change in the pull request goes behind the flag: early returns, error paths and new defaults included. A common failure is a flag that guards the new code but not an early return added next to it, so turning the flag off doesn't remove the regression.

## 3. Test both states

- In `valdi/test/integration/Runtime_tests.cpp`, set the value through the test tweak module:

  ```cpp
  tweakModule->config.setMapValue("VALDI_ENABLE_...", Valdi::Value(static_cast<bool>(true)));
  ```

- Run the affected tests with the flag on and with it off until the flag is removed.
- Tests default to the old behavior, the same as apps. Don't turn the flag on for the whole test harness; only the tests for the new behavior set it.
- If the flag is read during startup, also test with no value set.

## 4. How a host supplies values

A host app passes an `ITweakValueProvider` (`valdi/src/valdi/runtime/Interfaces/ITweakValueProvider.hpp`) to the runtime. It answers `getBool`, `getInt`, `getFloat`, `getString` and `getBinary` for a key, with a fallback.

- **C++ and the standalone runtime:** pass it to `RuntimeManager::setTweakValueProvider`, or as the `tweakValueProvider` argument of `ValdiStandaloneRuntime::create`.
- **iOS and Android apps** don't have a public setter yet. Until they do, a new flag stays at its default in those apps, which is the old behavior.

Any key the provider doesn't answer returns the getter's fallback, so a host that knows nothing about your flag keeps today's behavior.

## 5. Roll out and remove

These are recommendations for the app that embeds Valdi:

- Ship with the flag off. Turn it on for internal users first, then for a growing share of users, and advance only while crash and error rates stay flat.
- Watch the crashes and errors whose stack frames are in the changed code, not just the overall crash rate.
- Remember that a flag read at startup only changes after the app restarts with the new value.
- After the new behavior has run for everyone for a full release, flip the default and remove the flag and the old code path in a follow-up pull request.
