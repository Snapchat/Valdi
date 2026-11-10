# Valdi RFCs

An RFC ("request for comments") is a design doc for a large change. We agree on the design before anyone spends weeks building it. Most changes don't need one: a pull request with a clear description is enough.

## When you need an RFC

Write an RFC if your change does any of these:

- Adds, removes, or changes public API: components, runtime APIs, Bazel rules, CLI commands, or compiler behavior that users depend on. See [VERSIONING.md](../VERSIONING.md) for what counts as public API.
- Breaks compatibility, or changes default behavior.
- Adds a new platform or target, or changes what a platform supports.
- Adds a new dependency to the core runtime or compiler.
- Is large: roughly 1,000 lines or more, or spans several [hot systems](../.github/hot-systems.yml).
- Makes a [significant change](../CONTRIBUTING.md#safe-changes) to a [hot system](../.github/hot-systems.yml), whatever its size. This doesn't apply to the catch-all `framework` entry. A maintainer can waive it on your pull request.

You don't need an RFC for bug fixes, performance work that doesn't change behavior, docs, tests, or internal refactors, as long as they don't make a significant change to a hot system.

## How it works

1. **Pitch.** Describe the problem and your idea in a [Discussion](https://github.com/Snapchat/Valdi/discussions), or on an existing issue about it. A maintainer will tell you whether it needs an RFC, can go straight to a pull request, or isn't a fit for Valdi.
2. **Write the RFC.** Copy [`0000-template.md`](./0000-template.md) to `rfcs/0000-short-name.md` and fill it in, including the automated test strategy. Open a pull request titled `RFC: <title>`.
3. **Review.** Maintainers and the community comment on the pull request. The review period lasts up to **30 days**: maintainers aim to reach a decision within that time, and can decide sooner once the discussion has settled. Leave enough time for people in other time zones and on other schedules to weigh in. You can prototype in a separate draft pull request, but we won't merge it before the RFC is accepted.
4. **Decision.** The maintainers make the call, as described in [GOVERNANCE.md](../GOVERNANCE.md). **An RFC can't be accepted without an automated test strategy the maintainers agree is sufficient.** The outcome is one of:
   - **Accepted.** The RFC gets the next number and merges. Implementation can start. Accepting an RFC means we want the change, not that anyone has committed to building it.
   - **Declined.** The pull request is closed with the reasoning written down.
   - **Postponed.** Good idea, wrong time. Closed with a note on what would need to change.
5. **Implement.** Implementation pull requests link back to the RFC and include the tests its test strategy describes. If the design or the test strategy changes a lot during implementation, update the RFC. When it ships in a release, its status becomes `Implemented in <version>`.

Like every other change, an accepted RFC is merged through the process in [GOVERNANCE.md](../GOVERNANCE.md#how-merged-contributions-reach-this-repository), so its pull request shows as closed rather than merged.

## RFCs

Each RFC's `Status:` line is one of: Draft, Accepted, Implemented in `<version>`, Declined, Postponed, or Superseded by RFC `NNNN`.

| RFC | Status |
|---|---|
| [0001: Optional Linux window host for SnapDrawing](0001-linux-snapdrawing-host.md) ([#196](https://github.com/Snapchat/Valdi/pull/196)) | Accepted |
| [0002: Source-based Linux runtime embedding](0002-linux-headless-sdk.md) ([#197](https://github.com/Snapchat/Valdi/pull/197)) | Accepted |

RFCs in review are [open pull requests with "RFC" in the title](https://github.com/Snapchat/Valdi/pulls?q=is%3Apr+is%3Aopen+RFC+in%3Atitle).
