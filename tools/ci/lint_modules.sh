#!/usr/bin/env bash
#
# Lints the Valdi modules (src/valdi_modules) and the sample apps (apps/) with the Valdi module
# style rules in tools/lint/.eslintrc.js: ESLint, the Valdi ESLint plugin, and Prettier.
#
#   ./tools/ci/lint_modules.sh                  lint every module and app, minus tools/lint/baseline.txt
#   ./tools/ci/lint_modules.sh --changed <ref>  lint the .ts/.tsx files changed since <ref> (what PRs run)
#   ./tools/ci/lint_modules.sh <paths>          lint only these files or directories
#   ./tools/ci/lint_modules.sh --fix ...        also apply auto-fixes (formatting, import order, unused imports)
#   ./tools/ci/lint_modules.sh --skip-generated ...  don't build the compiler-generated TypeScript (no Bazel)
#
# By default the script first builds the modules' <module>_projectsync targets with Bazel, the same
# step `valdi projectsync` runs for editors, so imports of compiler-generated TypeScript (res.ts,
# Strings.d.ts, ClientSQL sources) type-check. With --skip-generated those imports don't resolve,
# and files that use them report type findings that CI would not.
#
# Changed files and explicit paths are linted in full, including files listed in the baseline.
# Warnings fail the run too (--max-warnings 0).
#
# One CI unit = one script (see AGENTS.md).

set -euo pipefail

# Intended to be run from open_source/
cd "$(dirname "$0")/../.."

LINT_DIR=tools/lint
LINT_ROOTS=(src/valdi_modules/src/valdi apps)

FIX=()
GENERATED=1
PATHS=()
CHANGED_BASE=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --fix) FIX=(--fix); shift ;;
        --skip-generated) GENERATED=0; shift ;;
        --changed)
            [[ $# -ge 2 ]] || { echo "--changed needs a git ref" >&2; exit 2; }
            CHANGED_BASE="$2"; shift 2 ;;
        *) PATHS+=("$1"); shift ;;
    esac
done

# npm ci from the committed lockfile, like npm_modules/cli: reproducible versions, and the
# registry.npmjs.org URLs in it are redirected to whatever registry npm is configured with.
if [[ ! -x "$LINT_DIR/node_modules/.bin/eslint" ]]; then
    npm ci --prefix "$LINT_DIR" --no-audit --no-fund
fi

if [[ "$GENERATED" -eq 1 ]]; then
    BAZEL=bzl
    command -v bzl >/dev/null 2>&1 || BAZEL=bazel
    if ! command -v "$BAZEL" >/dev/null 2>&1; then
        echo "Bazel not found. It builds the compiler-generated TypeScript; pass --skip-generated to lint without it." >&2
        exit 2
    fi
    PROJECTSYNC_QUERY="kind(valdi_projectsync, $(printf '//%s/... + ' "${LINT_ROOTS[@]}" | sed 's/ + $//'))"
    PROJECTSYNC_TARGETS=()
    while IFS= read -r t; do
        [[ -n "$t" ]] && PROJECTSYNC_TARGETS+=("$t")
    done <<< "$("$BAZEL" query "$PROJECTSYNC_QUERY")"
    echo "Building compiler-generated TypeScript for ${#PROJECTSYNC_TARGETS[@]} module(s)."
    "$BAZEL" build "${PROJECTSYNC_TARGETS[@]}"
    # The explicit target set, not the kind() query: cquery would configure every target under the roots.
    GENERATED_FILES="$("$BAZEL" cquery --output=files "set(${PROJECTSYNC_TARGETS[*]})")"
    printf '%s\n' "$GENERATED_FILES" | { grep -E '/generated_ts/.*\.tsx?$' || true; } | node "$LINT_DIR/generated_tsconfig.js"
    export VALDI_LINT_TSCONFIG=./tsconfig.generated.json
fi

IGNORES=()
if [[ -n "$CHANGED_BASE" ]]; then
    # Captured in a variable, not a process substitution, so a bad or unfetched ref fails the run
    # instead of reading as "nothing changed". --relative keeps paths relative to this directory,
    # whichever repository it is checked out in.
    CHANGED="$(git diff --name-only --relative --diff-filter=ACMR "$CHANGED_BASE...HEAD" -- "${LINT_ROOTS[@]}")"
    # Drop files the config ignores: ESLint warns on an explicitly passed ignored file, which
    # --max-warnings 0 would turn into a failure. Asks ESLint itself so the config stays the only list.
    FILTERED="$(printf '%s\n' "$CHANGED" | { grep -E '\.tsx?$' || true; } | LINT_DIR="$LINT_DIR" node -e '
        const { ESLint } = require(require.resolve("eslint", { paths: [process.env.LINT_DIR] }));
        const eslint = new ESLint({
            useEslintrc: false,
            overrideConfigFile: `${process.env.LINT_DIR}/.eslintrc.js`,
            resolvePluginsRelativeTo: process.env.LINT_DIR,
        });
        const files = require("fs").readFileSync(0, "utf8").split("\n").filter(Boolean);
        (async () => {
            for (const f of files) {
                if (!(await eslint.isPathIgnored(f))) console.log(f);
            }
        })().catch(e => { console.error(e); process.exit(1); });
    ' || exit 1)"
    while IFS= read -r f; do
        [[ -n "$f" ]] && PATHS+=("$f")
    done <<< "$FILTERED"
    if [[ ${#PATHS[@]} -eq 0 ]]; then
        echo "No changed .ts/.tsx files under ${LINT_ROOTS[*]}; nothing to lint."
        exit 0
    fi
elif [[ ${#PATHS[@]} -eq 0 ]]; then
    PATHS=("${LINT_ROOTS[@]}")
    while IFS= read -r f; do
        IGNORES+=(--ignore-pattern "$f")
    done < <(grep -v -e '^#' -e '^[[:space:]]*$' "$LINT_DIR/baseline.txt")
fi

echo "Linting ${#PATHS[@]} path(s)."
"$LINT_DIR/node_modules/.bin/eslint" \
    --no-eslintrc \
    --config "$LINT_DIR/.eslintrc.js" \
    --resolve-plugins-relative-to "$LINT_DIR" \
    --ext .ts,.tsx \
    --max-warnings 0 \
    ${FIX[@]+"${FIX[@]}"} \
    ${IGNORES[@]+"${IGNORES[@]}"} \
    "${PATHS[@]}"
