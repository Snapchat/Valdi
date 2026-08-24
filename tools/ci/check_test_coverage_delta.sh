#!/usr/bin/env bash
#
# Fails when a change removes test coverage from open_source without saying so.
#
# Why this exists: the PR #107 text-rendering import deleted 32 test methods on its way in --
# ValdiEditTextMultilineTest.kt entirely (5 cases covering two shipped Send To crash families) and
# AnimationRichTextTest.kt from 34 cases down to 7 -- and nothing noticed. Both covered code that
# still ships. It was found by hand, months later, after the import had already been rolled back
# once for unrelated breaks. A deleted test is invisible in CI by construction: the suite goes
# green *because* the case is gone.
#
# This is a coarse net on purpose. It counts test functions and test files; it cannot tell a
# legitimate deletion from a bad one. When a deletion is intended, say so in the commit message
# with:
#
#     Allow-test-removal: <reason>
#
# Usage: check_test_coverage_delta.sh [base_ref]        (default: origin/master)
#
# Exits 0 and warns, rather than failing, when the base ref is unavailable (shallow clone, detached
# CI checkout). A check that cannot run must not block a build -- but it says so loudly instead of
# passing silently, because "silently passed" is the failure mode it is here to prevent.

set -uo pipefail

BASE_REF="${1:-origin/master}"

# The tree this guards is two levels up from this script. Capture it before cd'ing to the repo root.
TREE_ABS="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

cd "$(git rev-parse --show-toplevel)" || exit 0

# Express the guarded tree relative to the repo root, so it works whether that tree is nested inside
# a larger repo or is the repo root itself (relative path "."). A wrong path would grep nothing,
# count zero tests, and pass while checking nothing -- the exact failure this script exists to catch.
TREE="$(git -C "$TREE_ABS" rev-parse --show-prefix 2>/dev/null)"
TREE="${TREE%/}"
[[ -z "$TREE" ]] && TREE="."

if ! git rev-parse --verify --quiet "$BASE_REF" > /dev/null; then
    echo "WARNING: base ref '$BASE_REF' not available, skipping the test-coverage delta check."
    echo "         This check is NOT running. Do not read a green build as evidence that no tests"
    echo "         were removed."
    exit 0
fi

MERGE_BASE="$(git merge-base "$BASE_REF" HEAD 2>/dev/null)"
if [[ -z "$MERGE_BASE" ]]; then
    echo "WARNING: no merge base with '$BASE_REF', skipping the test-coverage delta check."
    exit 0
fi

# Counts test functions PER FILE across the languages open_source actually uses: Kotlin/Java @Test,
# GoogleTest TEST/TEST_F/TEST_P, XCTest '- (void)testX', and Jasmine-style it(...) specs.
#
# Per file, not in total, deliberately. A whole-tree total is useless here: a change that adds a
# large new suite while gutting an existing one shows a net *increase*. That is exactly the shape of
# the import that motivated this script -- it added six iOS test files while cutting
# AnimationRichTextTest from 34 cases to 7, so any net count would have waved it through.
count_tests_per_file_at() {
    local rev="$1"
    # NB: no \b. git grep's default POSIX ERE does not support it, and including it silently kills
    # the whole alternation for Kotlin/Java -- it counts zero @Test cases while still matching the
    # C++/ObjC/JS forms, producing a plausible-looking total with every Kotlin test missing.
    git grep -cE '^[[:space:]]*(@Test([^a-zA-Z0-9_]|$)|TEST(_F|_P)?\(|-[[:space:]]*\(void\)test|it\()' \
        "$rev" -- "$TREE" 2>/dev/null | sed "s|^$rev:||"
}

before_counts="$(count_tests_per_file_at "$MERGE_BASE")"
after_counts="$(count_tests_per_file_at HEAD)"

before="$(echo "$before_counts" | awk -F: '{ n += $NF } END { print n+0 }')"
after="$(echo "$after_counts" | awk -F: '{ n += $NF } END { print n+0 }')"

# Files that still exist but lost cases.
shrunk="$(
    join -t: -j1 \
        <(echo "$before_counts" | awk -F: '{ print $1 ":" $NF }' | sort -t: -k1,1) \
        <(echo "$after_counts"  | awk -F: '{ print $1 ":" $NF }' | sort -t: -k1,1) \
        2>/dev/null \
    | awk -F: '$2 > $3 { printf "  %s: %d -> %d (lost %d)\n", $1, $2, $3, $2 - $3 }'
)"

# Test files present at the merge base but gone at HEAD.
deleted_files="$(git diff --diff-filter=D --name-only "$MERGE_BASE" HEAD -- "$TREE" 2>/dev/null \
    | grep -iE '(^|/)(test|tests)/|Test[s]?\.(kt|java|m|mm|swift|ts|tsx)$|_test\.(cpp|cc)$|\.spec\.(ts|tsx)$' \
    || true)"

echo "test functions under $TREE: $before at merge base ($MERGE_BASE), $after at HEAD"

problems=0
if [[ -n "$shrunk" ]]; then
    echo "ERROR: these test files lost cases:"
    echo "$shrunk"
    problems=1
fi
if [[ -n "$deleted_files" ]]; then
    echo "ERROR: test files deleted:"
    echo "$deleted_files" | sed 's/^/  /'
    problems=1
fi

if (( problems == 0 )); then
    exit 0
fi

# An explicit acknowledgement in any commit message on top of the merge base waives the check.
#
# Captured rather than piped into `grep -q`: under `set -o pipefail`, grep -q exits as soon as it
# matches, git log takes SIGPIPE, and the pipeline's status is that failure -- so the waiver looked
# like "no trailer found" every time and the check could not be bypassed at all.
waiver="$(git log --format=%B "$MERGE_BASE"..HEAD | grep -iE '^Allow-test-removal:' || true)"
if [[ -n "$waiver" ]]; then
    echo
    echo "Waived by an 'Allow-test-removal:' trailer:"
    echo "$waiver" | sed 's/^/  /'
    exit 0
fi

cat <<'MESSAGE'

This change removes test coverage from open_source.

If that is deliberate -- the tests are obsolete, or moved, or replaced by something better -- add a
trailer to any commit in the change explaining why, and this check will pass:

    Allow-test-removal: <reason>

If it is not deliberate, restore the tests. If they no longer compile against the new code, retarget
them rather than deleting them; a test that fails is information, a test that is gone is not. When a
test genuinely cannot run in its target any more, keeping it compiling and @Ignore'd with the reason
recorded preserves the coverage intent for whoever can fix it.
MESSAGE

exit 1
