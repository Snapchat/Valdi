#!/usr/bin/env bash
#
# Sourced by run_tests.sh and test_modules.sh (not a CI step on its own). Fills TEST_TARGETS with
# the test targets one of those jobs should run, so that every test target in the repo is run by
# some job without anyone wiring it in by hand:
#
#   select_tests cpp      every test target except valdi_test modules (run_tests.sh)
#   select_tests modules  every valdi_test module test, plus //modules/... (test_modules.sh)
#
# Both exclude `manual` tests, Apple test rules on Linux, and the entries in
# tools/ci/test_skiplist.txt. Skip-list entries are one of:
#
#   <label> all|linux|macos # <reason>        skipped on those platforms
#   <label> run_by:<path>   # <reason>        run by another CI job; <path> must name the label
#
# Fails if an entry is no longer a test target, or if a run_by entry's file doesn't mention the
# label, so the list can't go stale.

select_tests() {
    local scope="$1"
    local platform platform_except
    if [[ $(uname) == Linux ]] ; then
        platform=linux
        # Apple test rules need Xcode.
        platform_except=' except kind("(ios|macos)_(unit|ui|build)_test", tests(//...))'
    else
        platform=macos
        platform_except=''
    fi

    local skiplist=tools/ci/test_skiplist.txt
    local skipped=()
    local label where _
    # `|| [[ -n ... ]]` keeps the last entry when the file has no trailing newline.
    while read -r label where _ || [[ -n "$label" ]] ; do
        [[ -z "$label" || "$label" == \#* ]] && continue
        if [[ "$where" == run_by:* ]] ; then
            if ! grep -qF -- "$label" "${where#run_by:}" ; then
                echo "error: $skiplist says ${where#run_by:} runs $label, but that file doesn't mention it" >&2
                return 1
            fi
            skipped+=("$label")
        elif [[ "$where" == all || "$where" == "$platform" ]] ; then
            skipped+=("$label")
        fi
        # A stale entry (renamed or deleted target) fails here.
        if [[ -z "$(bzl query "tests($label)" --output=label 2>/dev/null)" ]] ; then
            echo "error: $skiplist lists $label, which is not a test target" >&2
            return 1
        fi
    done < "$skiplist"
    # Internal CI runs //valdi:test_java in a separate step, so only run it here on GitHub Actions.
    if [[ "${GITHUB_ACTIONS:-}" != "true" ]] ; then
        skipped+=("//valdi:test_java")
    fi

    local all='tests(//...) except attr(tags, "\bmanual\b", tests(//...))'"$platform_except"
    if (( ${#skipped[@]} )) ; then
        all="($all) except set(${skipped[*]})"
    fi
    local query
    case "$scope" in
        # valdi_test targets run on a host-built (exec configuration) copy of the runtime. Keeping
        # them out of the C++ job means it doesn't build the whole native stack a second time.
        cpp) query="$all except kind(valdi_test, tests(//...))" ;;
        # Intersect with $all so the skip list and platform exclusions apply to //modules/... too.
        modules) query="(kind(valdi_test, tests(//...)) + tests(//modules/...)) intersect ($all)" ;;
        *) echo "error: select_tests: unknown scope '$scope'" >&2 ; return 1 ;;
    esac

    # Not a process substitution: set -e ignores its exit status, so a failed query (e.g. a broken
    # BUILD file) would pass CI with a partial target list.
    local targets
    targets="$(bzl query "$query" --output=label)" || return 1
    TEST_TARGETS=()
    local t
    while IFS= read -r t ; do
        [[ -n "$t" ]] && TEST_TARGETS+=("$t")
    done <<< "$targets"
    if (( ${#TEST_TARGETS[@]} == 0 )) ; then
        echo "error: select_tests $scope: the query matched no test targets" >&2
        return 1
    fi
    echo "select_tests $scope: ${#TEST_TARGETS[@]} test targets (${#skipped[@]} skipped by $skiplist)"
}
