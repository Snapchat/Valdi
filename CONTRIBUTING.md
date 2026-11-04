# Contributing to Valdi

Thank you for your interest in Valdi. We welcome community contributions and appreciate your time and effort in helping improve the project. Before getting started, please take a moment to review these guidelines.

## 1. [Code of Conduct](./CODE_OF_CONDUCT.md)

We want this project to be a welcoming space for everyone. By contributing, you agree to follow our [Code of Conduct](./CODE_OF_CONDUCT.md) and help keep the community respectful and inclusive.

## 2. How to Contribute

We welcome contributions in various forms, including:

- Bug fixes
- Documentation improvements
- Tests and performance optimizations

If you would like to add new features, please start a [GitHub Discussion](https://github.com/Snapchat/Valdi/discussions) to discuss with the Valdi team first.

Large or cross-cutting changes and changes to public API require the [RFC process](./rfcs/README.md) before implementation. An RFC can't be accepted without an automated test strategy the maintainers agree is sufficient. See [VERSIONING.md](./VERSIONING.md#what-counts-as-public-api) for what counts as public API.

To contribute, follow these steps:

- **Fork** the repository and create a new branch.
- **Make your changes**, ensuring they align with our coding standards (§5 below).
- **Run tests** to ensure your changes do not break existing functionality.
- **Submit a pull request (PR)** with a clear description of the changes.
- A maintainer will review your PR, suggest any necessary changes, and merge it once approved.

All changes are reviewed and approved by the maintainer team. See [GOVERNANCE.md](./GOVERNANCE.md) for how decisions are made and how accepted pull requests are merged.

### What makes a good pull request

- **It solves a concrete problem.** Link the issue or Discussion it addresses, or describe the bug or limitation it fixes.
- **It includes evidence.** Show that the change works: the commands you ran, test output, or screenshots and recordings for visual changes.
- **It is focused.** One logical change per pull request. Smaller pull requests are reviewed faster.

### Changes we generally do not accept

To make the best use of reviewers' time, please do not open pull requests for:

- Refactoring, reformatting, or renaming that does not fix a concrete problem.
- Test-only or CI-only changes that are not tied to a reported bug.
- New features that have not been discussed in a [GitHub Discussion](https://github.com/Snapchat/Valdi/discussions) first.
- Broad changes that touch many unrelated areas of the codebase at once.
- Many near-identical pull requests, such as the same small change applied file by file.

Pull requests like these may be closed with a link to this section. If you are unsure whether a change is welcome, please ask in Discussions before starting.

### Safe changes

Some parts of Valdi have caused most of its serious regressions: bridge marshalling, runtime teardown, threading, module and resource loading, text, images, the renderer, gestures, view recycling, the JS engines, the compiler, the build and OS compatibility. They are listed, with their paths, in [`.github/hot-systems.yml`](./.github/hot-systems.yml), and the rest of the framework source is covered by a catch-all `framework` entry, so the same rules apply to any framework change. When a pull request touches framework code, a bot comments with the systems it touched and a short checklist ([what to do about it](./docs/docs/workflow-hot-systems-comment.md)). Changes that only touch tests or docs don't get the comment. The comment is advisory and never blocks a merge.

A change to a hot system is **significant** if it does any of these:

- Changes when or in what order objects are created, disposed or freed.
- Changes which thread runs code, adds or removes a lock, or adds a synchronous cross-thread call.
- Changes a bridge contract: nullability, return values, error codes, or whether a call raises.
- Turns a silent failure into a raised one, or the reverse.
- Changes a default, a base-class contract, the startup path, or a format produced at build time and read on device.
- Removes code that looks dead, adds logging on a hot path, or refactors hot code. These count even when the intent is no behavior change.

For a significant change:

- **Runtime systems** (gate `flag` in the file): put the new behavior behind a runtime tweak that defaults to the old behavior, and make sure the tweak covers every behavior change in the pull request, including early returns and error paths. See [Putting a change behind a flag](./docs/docs/workflow-flagging-changes.md) for how.
- **Build-time systems** (gate `guard`): a runtime tweak can't cover these, so say so in the pull request and include the test results for the affected area.
- **Teardown and threading changes:** list in the pull request description every object whose lifetime changes and who can still hold it, and the threads and locks before and after.
- **Not significant:** say why in the pull request description. Comments, tests, renames and fixes that only add a null check are not significant.

### Pull request limit

Contributors who are not project collaborators may have up to five pull requests open at a time. Additional pull requests are closed automatically and can be reopened once some of your open pull requests are merged or closed.

### AI-assisted contributions

Contributions made with the help of AI tools are welcome, and disclosure is not required. They are held to the same standard as any other contribution: the pull request must address a concrete problem and include evidence that the change works. As the contributor, you are responsible for understanding and standing behind every line you submit, and for responding to review feedback.

### Commit Messages

Use clear and descriptive commit messages. Follow the conventional commit format when possible:

feat: Add new authentication method  
fix: Resolve issue with session timeout  
docs: Update README with new installation steps  

## 3. What to Expect After You Open a PR

- **First response:** Someone from our team will look at your PR within 3 business days.
- **Review timeline:** We aim to decide on and land implementation PRs within these times, counted in business days. The clock pauses while we're waiting on you.
  - Under 100 lines changed: a decision in 5 business days, landed in 10.
  - 100 to 999 lines: a decision in 7 business days, landed in 15.
  - 1,000 to 4,999 lines: a decision in 10 business days, landed in 25.

  Changes to the C++ runtime can take longer, because fewer maintainers can review them. Changes of roughly 1,000 lines or more usually need an RFC first, which follows its own timeline in the [RFC process](./rfcs/README.md#how-it-works).
- **Keep PRs reviewable:** Prefer small, independently reviewable PRs. If your change spans multiple PRs, link them and note the order they depend on.
- **How changes land:** Accepted PRs are imported into our internal repository, validated, and landed there before being reflected publicly. Your authorship is preserved.
- **Inactivity:** If a PR is waiting on the author with no activity for about a week, we'll follow up. PRs that remain inactive may be closed, and you're welcome to reopen them.
- **Outcomes:** Every PR ends as landed or closed, and we'll explain the reason if we don't accept a change.

## 4.  Legal Terms

By submitting a contribution, you represent and warrant that:

- It is your original work, or you have sufficient rights to submit it.
- You grant the Valdi maintainers and users the right to use, modify, and distribute it under the MIT license (see LICENSE file); and
- To the extent your contribution is covered by patents, you grant a perpetual, worldwide, non-exclusive, royalty-free, irrevocable license to the Valdi maintainers and users to make, use, sell, offer for sale, import, and otherwise transfer your contribution as part of the project.

We do not require a Contributor License Agreement (CLA). However, by contributing, you agree to license your submission under terms compatible with the MIT License and to grant the patent rights described above. If your contribution includes third-party code, you are responsible for ensuring it is MIT-compatible and properly attributed.

Where permitted by law, you waive any moral rights in your contribution (e.g., the right to object to modifications). If such rights cannot be waived, you agree not to assert them in a way that interferes with the project’s use of your contribution.

## 5.  Coding Standards

To maintain a consistent codebase, please follow these guidelines:

- Use the existing coding style and conventions.
- Ensure all code changes are well-documented.
- Write tests for new features and bug fixes.
- Avoid introducing unnecessary dependencies.
- Lint TypeScript changes to modules (`src/valdi_modules`) and sample apps (`apps/`) before you open a pull request: `./tools/ci/lint_modules.sh --changed origin/main` (add `--fix` to apply formatting and import-order fixes). It first builds the compiler-generated TypeScript (`res`, `Strings`, ClientSQL) with Bazel so imports of it type-check; `--skip-generated` skips that step. CI runs the same check, and every file you change is linted in full.

## 6.  Reporting Issues

If you find a bug, please open an issue using one of our [issue forms](https://github.com/Snapchat/Valdi/issues/new/choose), which ask for the details we need to investigate, including steps to reproduce and your Valdi version. Please report one problem per issue and search existing issues first.

- **Questions** belong in [GitHub Discussions](https://github.com/Snapchat/Valdi/discussions). See [SUPPORT.md](./SUPPORT.md).
- **Feature ideas** should start as a [GitHub Discussion](https://github.com/Snapchat/Valdi/discussions).
- **Security vulnerabilities** must be reported privately through [HackerOne](https://hackerone.com/snapchat), never in a public issue.

## 7.  Recognition

We use the All Contributors specification to recognize community members. If your contribution is merged, you will be added to the project's list of contributors. This includes contributions of all kinds—code, documentation, design, testing, and more.
