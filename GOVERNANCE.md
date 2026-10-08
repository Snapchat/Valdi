# Governance

Valdi is developed and maintained by Snap Inc. This document describes how decisions are made and how contributions are reviewed.

## Maintainers

The project is maintained by the Valdi team at Snap, listed in [MAINTAINERS.md](./MAINTAINERS.md). The maintainer team sets the project's direction, decides what is in scope for Valdi core, and is responsible for every change that is merged.

## How changes are accepted

- Every change, from maintainers and outside contributors alike, is reviewed and approved by the maintainer team before it is merged.
- Approval is at the maintainers' discretion. A pull request may be declined even if it is correct, for example if it falls outside the project's direction or would be costly to maintain.
- New features begin as a [GitHub Discussion](https://github.com/Snapchat/Valdi/discussions) so the approach can be agreed on before code is written. Features that fit better outside of Valdi core may be redirected to a separate package or module.
- The project's direction is described in the [roadmap](./ROADMAP.md).

## How merged contributions reach this repository

Valdi's source of truth is Snap's internal repository, which is mirrored here. When a pull request is accepted, a maintainer imports it into the internal repository, where it runs through additional testing before being mirrored back. The original pull request is then closed automatically with a reference to the resulting commit, and the contributor is credited as its author.

## Triage and automation

To keep the project manageable for a small team, some triage is automated:

- Contributors who are not project collaborators may have up to five pull requests open at a time.
- Maintainers use labels to close issues and pull requests with a standard explanation, such as redirecting a question to Discussions.
- Every automated action can be overridden by a maintainer. If you believe automation acted on your issue or pull request in error, please reply on the thread.

## Code of Conduct

All participants are expected to follow the [Code of Conduct](./CODE_OF_CONDUCT.md). The maintainers are responsible for enforcing it.

## Changes to this document

The maintainer team may update this document as the project evolves.
