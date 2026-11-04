# Your pull request got a "Valdi hot systems touched" comment

A bot comments on pull requests that change Valdi framework code. This page says what to do about it. The rules behind it are in [CONTRIBUTING.md, "Safe changes"](../../CONTRIBUTING.md#safe-changes).

**The comment never blocks a merge.** It lists the hot systems your change touched, with a short checklist. Reviewers will look for your answer to that checklist in the pull request description.

## Why it's there

A few parts of Valdi have caused most of its serious regressions: bridge marshalling, runtime teardown, threading, module and resource loading, text, images, the renderer, gestures, view recycling, the JS engines, the compiler, the build and OS compatibility. They're listed, with their paths, in [`.github/hot-systems.yml`](../../.github/hot-systems.yml). Bugs there tend to show up only in production, on some devices, after a change has shipped. The checklist is how we catch them in review instead.

## What to do

### 1. Decide whether the change is significant

It's **significant** if it does any of these:

- Changes when or in what order objects are created, disposed or freed.
- Changes which thread runs code, adds or removes a lock, or adds a synchronous cross-thread call.
- Changes a bridge contract: nullability, return values, error codes, or whether a call raises.
- Turns a silent failure into a raised one, or the reverse.
- Changes a default, a base-class contract, the startup path, or a format produced at build time and read on device.
- Removes code that looks dead, adds logging on a hot path, or refactors hot code. These count even when you don't intend any behavior change.

It's **not significant** if it only changes comments, tests or names, or only adds a null check. Tick "Not significant" and say why in one sentence in the pull request description. That's all you need to do.

### 2. If it is significant

Look at the **gate** for each system in the comment:

| Gate | What to do |
|---|---|
| `flag` (runtime code) | Put the new behavior behind a runtime tweak that defaults to the old behavior: [Putting a change behind a flag](./workflow-flagging-changes.md). The tweak must cover every behavior change, including early returns and error paths. |
| `guard` (build-time code: the compiler, JS engines, the build, OS compatibility, layout) | A runtime tweak can't cover these. Say so in the pull request, and include the test results for that area. |

### 3. For teardown, threading and marshalling changes

Add a short audit to the pull request description:

- **Teardown:** each object whose lifetime changes, and who can still hold it.
- **Threading:** which threads and locks are involved before and after, and any new synchronous cross-thread call.
- **Marshalling:** what changes in nullability or error behavior, on each platform.

A few lines each is enough. The point is that a reviewer can check them.

## What happens next

- A reviewer checks your answers. They may ask you to add a flag, an audit or a test before the change is merged.
- Once merged, a flagged change ships turned off. Apps that embed Valdi turn it on gradually and can turn it off again without a new release. Keep the old path until the flag is removed.
- If your change is a fix for a regression that already shipped, say so. A revert or a flag flip is often safer than a new fix.

## Questions

Ask in [Discussions](https://github.com/Snapchat/Valdi/discussions), or reply on the comment in your pull request.
