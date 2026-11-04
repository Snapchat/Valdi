## Description
<!-- Provide a clear and concise description of your changes -->

## Type of Change
- [ ] Bug fix (non-breaking change that fixes an issue)
- [ ] Documentation improvement
- [ ] Performance optimization
- [ ] Test improvement
- [ ] Other (please describe)

## Testing
- [ ] Tests pass locally (`bazel test //...`)
- [ ] Added/updated tests for changes (if applicable)
- [ ] Tested on multiple platforms (iOS/Android/Web/macOS as applicable)
- [ ] Manual testing performed (describe below)

### Evidence
<!-- Required: show that the change works. Paste the commands you ran and their output, or attach screenshots/recordings for visual changes. -->

## Checklist
- [ ] Code follows project style guidelines
- [ ] Documentation updated (if needed)
- [ ] No breaking changes (or documented in description)
- [ ] Commit messages follow [conventional format](../CONTRIBUTING.md#commit-messages)
- [ ] No secrets, API keys, or internal URLs included
- [ ] If this touches a path in [`.github/hot-systems.yml`](hot-systems.yml): significant changes are behind a runtime tweak that defaults to the old behavior, or the description says why they aren't needed ([Safe changes](../CONTRIBUTING.md#safe-changes))

## Related Issues
<!-- Link related issues using: Closes #(issue), Fixes #(issue), Relates to #(issue) -->

Reverts / partially reverts: <!-- #PR, or "none". Count a forward fix that restores earlier behavior too. -->

## Additional Context
<!-- Add any other context, screenshots, or information that would help reviewers -->
