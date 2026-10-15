# Valdi RFCs

An RFC ("request for comments") is how we agree on a large change before anyone spends weeks building it. Most changes don't need one. A good pull request with a clear description is enough.

## When you need an RFC

Write an RFC if your change does any of these:

- Adds, removes, or changes public API: components, runtime APIs, Bazel rules, CLI commands, or compiler behavior that users depend on. See [VERSIONING.md](../VERSIONING.md) for what counts as public API.
- Breaks compatibility, or changes default behavior.
- Adds a new platform or target, or changes what a platform supports.
- Adds a new dependency to the core runtime or compiler.
- Is large: roughly more than 1,000 lines, or touches several subsystems.

You don't need an RFC for bug fixes, performance work that doesn't change behavior, docs, tests, or internal refactors.

Not sure? Open a [Discussion](https://github.com/Snapchat/Valdi/discussions) and ask. A maintainer will tell you.

## How it works

1. **Pitch.** Open a [Discussion](https://github.com/Snapchat/Valdi/discussions) with a short description of the problem and your idea. This is where you find out early whether it's a fit. If you skip this step, you risk writing an RFC that gets declined.
2. **Write the RFC.** Copy [`0000-template.md`](./0000-template.md) to `rfcs/0000-short-name.md` and fill it in, including the automated test strategy. Open a pull request.
3. **Review.** Maintainers and the community comment on the pull request. The review period is at least **30 days**. That gives the maintainer team time to discuss the RFC, and gives people in other time zones and on other schedules time to weigh in.
4. **Decision.** The maintainers make the call, as described in [GOVERNANCE.md](../GOVERNANCE.md). **An RFC can't be accepted without an automated test strategy the maintainers agree is sufficient.** The outcome is one of:
   - **Accepted.** The RFC gets a number and merges. Implementation can start. Accepting an RFC means we want the change, not that anyone has committed to building it.
   - **Declined.** The pull request is closed with the reasoning written down.
   - **Postponed.** Good idea, wrong time. Closed with a note on what would need to change.
5. **Implement.** Implementation pull requests link back to the RFC and include the tests its test strategy describes. If the design or the test strategy changes a lot during implementation, update the RFC.

Like every other change, an accepted RFC is merged through the process in [GOVERNANCE.md](../GOVERNANCE.md#how-merged-contributions-reach-this-repository).

## Status

| Status | Meaning |
|---|---|
| Draft | Pull request open, under review |
| Accepted | Merged, ready to implement |
| Implemented | Shipped in a release (note the version) |
| Declined / Postponed | Closed, with reasoning |

## Accepted RFCs

None yet.
