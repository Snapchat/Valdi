#!/usr/bin/env bash
#
# Lints the Valdi modules (src/valdi_modules) and the sample apps (apps/) with the Valdi module
# style rules in tools/lint/.eslintrc.js: ESLint, the Valdi ESLint plugin, and Prettier.
#
#   ./tools/ci/lint_modules.sh                  lint every module and app, minus tools/lint/baseline.txt
#   ./tools/ci/lint_modules.sh --changed <ref>  lint the .ts/.tsx files changed since <ref> (what PRs run)
#   ./tools/ci/lint_modules.sh <paths>          lint only these files or directories
#   ./tools/ci/lint_modules.sh --fix ...        also apply auto-fixes (formatting, import order, unused imports)
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
PATHS=()
CHANGED_BASE=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --fix) FIX=(--fix); shift ;;
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
