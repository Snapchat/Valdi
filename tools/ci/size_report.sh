#!/usr/bin/env bash
#
# Warn-only size report for Valdi's shipped outputs: per-module Android release .valdimodule files
# (compressed, plus uncompressed via zstd) and the native .so per ABI in the exported AAR.
#
# Why this exists: size regressions land one small change at a time, and nothing in CI looks at
# size, so the first signal is a downstream app noticing months later. This puts the number on the
# PR while the change is still in review.
#
# Flow:
#   1. Build every //src/valdi_modules/... and //modules/... :android.release.valdimodule target and
#      measure it. Modules in tools/ci/size_ignore.txt (test-only) are skipped.
#   2. Measure jni/<abi>/*.so in the exported AAR. Pass the AAR test_exported_lib.sh produced via
#      VALDI_SIZE_AAR; without it, the AAR is built here with `valdi export android` when the CLI is
#      installed, and native sizes are skipped otherwise.
#   3. Write size-report.json (schema: tools/ci/size_report.py).
#   4. In GitHub Actions, find a baseline: the size-report artifact of the newest push-to-main run of
#      this workflow whose commit is VALDI_SIZE_BASE_SHA or one of its 50 nearest ancestors (the
#      workflow is path-filtered, so not every main commit has a run). Uses the read-only
#      GITHUB_TOKEN, so it works for fork PRs. The artifact is untrusted input: only one member is
#      extracted, size-capped, and schema-validated before any of it is rendered.
#   5. Write a step summary (top changes + totals) and, when VALDI_SIZE_ANNOTATE=1, a ::warning per
#      threshold crossing. A `Size-change: <reason>` trailer in the PR head commit acknowledges the
#      growth: the table stays, the annotations go.
#
# Never fails the build. A measurement it cannot make is reported as a warning, and a missing
# baseline is reported loudly instead of passing silently: "no warnings" must not be readable as
# "nothing grew" when nothing was compared.
#
# Environment (all optional):
#   VALDI_SIZE_AAR          exported AAR to measure native libraries from
#   VALDI_SIZE_OUT          copy the final size-report.json here (the main-push artifact upload)
#   VALDI_SIZE_BASE_SHA     commit to find a baseline for (PR base / push "before")
#   VALDI_SIZE_HEAD_SHA     PR head commit, checked for a Size-change: trailer
#   VALDI_SIZE_ANNOTATE     1 to emit ::warning annotations (default 0: report only)
#   VALDI_SIZE_ARTIFACT     baseline artifact name (default valdi-size-report)
#   GH_TOKEN, GITHUB_REPOSITORY, GITHUB_STEP_SUMMARY   provided by GitHub Actions
#
# Usage: tools/ci/size_report.sh

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OPEN_SOURCE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
HELPER="$SCRIPT_DIR/size_report.py"
IGNORE_FILE="$SCRIPT_DIR/size_ignore.txt"

ARTIFACT_NAME="${VALDI_SIZE_ARTIFACT:-valdi-size-report}"
WORKFLOW_PATH=".github/workflows/bzl-changes.yml"
BASELINE_BRANCH="main"
MAX_ANCESTORS=50
MAX_ZIP_BYTES=$((4 * 1024 * 1024))
MAX_REPORT_BYTES=$((1024 * 1024))

# Mirrored repo: open_source is the workspace root. Internal: it is the @valdi repo.
POTENTIAL_ROOT="$(cd "$SCRIPT_DIR/../../../../" && pwd)"
if [[ -d "$POTENTIAL_ROOT/Jenkins" ]]; then
    ROOT_DIR="$POTENTIAL_ROOT"
    TARGET_PREFIX="@valdi//"
else
    ROOT_DIR="$OPEN_SOURCE_DIR"
    TARGET_PREFIX="//"
fi

if command -v bzl > /dev/null 2>&1; then
    BAZEL="${BAZEL:-bzl}"
else
    BAZEL="${BAZEL:-bazel}"
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
REPORT="$WORK_DIR/size-report.json"
# Keep the source tree clean of __pycache__ from the helper and its tests.
export PYTHONDONTWRITEBYTECODE=1

warn() {
    # One line, no workflow-command injection from interpolated text.
    local msg="${1//'%'/%25}"
    msg="${msg//$'\n'/%0A}"
    msg="${msg//$'\r'/%0D}"
    if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
        echo "::warning title=Valdi size::$msg"
    else
        echo "WARNING: $1" >&2
    fi
}

cd "$ROOT_DIR" || { warn "cannot cd to $ROOT_DIR"; exit 0; }

# The helper's delta/threshold/validation logic has unit tests; run them (well under a second) so
# they cannot rot. A failure does not stop the report, but it is called out.
if ! python3 "$SCRIPT_DIR/size_report_test.py" > "$WORK_DIR/selftest.log" 2>&1; then
    cat "$WORK_DIR/selftest.log" >&2
    warn "size_report.py unit tests failed; treat this size report with suspicion."
fi

# --- 1. JS bytecode format ------------------------------------------------------------------
# The build_setting_default of //bzl/valdi:js_bytecode_format. Sizes under different formats are
# not comparable, so the format is recorded and a mismatched baseline is refused.
BYTECODE_FORMAT="$(awk '
    /name = "js_bytecode_format",/ { found = 1; next }
    found && /build_setting_default/ { gsub(/[^a-z0-9_]/, "", $3); print $3; exit }
' "$OPEN_SOURCE_DIR/bzl/valdi/BUILD.bazel")"
BYTECODE_FORMAT="${BYTECODE_FORMAT:-unknown}"
echo "JS bytecode format: $BYTECODE_FORMAT"

# --- 2. .valdimodule sizes ------------------------------------------------------------------
QUERY="filter('android\\.release\\.valdimodule\$', ${TARGET_PREFIX}src/valdi_modules/... + ${TARGET_PREFIX}modules/...)"
if ! TARGETS="$("$BAZEL" query "$QUERY" 2> "$WORK_DIR/query.log")" || [[ -z "$TARGETS" ]]; then
    cat "$WORK_DIR/query.log" >&2
    warn "could not list .valdimodule targets; no size report this run."
    exit 0
fi
echo "Measuring $(echo "$TARGETS" | wc -l | tr -d ' ') .valdimodule targets"

# shellcheck disable=SC2086 # word-splitting the target list is intended
if ! "$BAZEL" build $TARGETS; then
    warn "building the .valdimodule targets failed; no size report this run."
    exit 0
fi

EXEC_ROOT="$("$BAZEL" info execution_root 2> /dev/null)"
# One "<label>\t<path>" line per output file. Labels are normalized to the plain //pkg:name form so
# reports stay comparable across Bazel versions that print the main repo as @@//.
# cquery takes ONE expression, so the targets go in a set(); as separate words it fails to parse.
TARGET_SET="set($(tr '\n' ' ' <<< "$TARGETS"))"
if ! "$BAZEL" cquery "$TARGET_SET" --output=starlark \
    --starlark:expr='"\n".join([str(target.label) + "\t" + f.path for f in target.files.to_list() if f.path.endswith(".valdimodule")])' \
    2> "$WORK_DIR/cquery.log" \
    | sed -E 's#^@@?//#//#' \
    | awk -F'\t' -v root="$EXEC_ROOT" 'NF == 2 { print $1 "\t" root "/" $2 }' \
    > "$WORK_DIR/valdimodules.tsv"; then
    cat "$WORK_DIR/cquery.log" >&2
    warn "could not locate the built .valdimodule files; no size report this run."
    exit 0
fi

# --- 3. Native library sizes ----------------------------------------------------------------
AAR="${VALDI_SIZE_AAR:-}"
if [[ -z "$AAR" || ! -f "$AAR" ]]; then
    if command -v valdi > /dev/null 2>&1; then
        AAR="$WORK_DIR/hello_world_export.aar"
        echo "No VALDI_SIZE_AAR; building the exported AAR (same invocation as test_exported_lib.sh)"
        valdi export android \
            --library "${TARGET_PREFIX}apps/helloworld:hello_world_export_android" \
            --output_path "$AAR" \
            --bazel_args="--fat_apk_cpu=arm64-v8a --android_cpu=arm64-v8a" || AAR=""
    else
        AAR=""
    fi
fi
: > "$WORK_DIR/native.tsv"

# Stripped size is what an app ships (release packaging strips native libraries), so measure a
# stripped copy when an ELF-capable llvm-strip is around: VALDI_SIZE_STRIP, then PATH, then the
# NDK the hosted runner image preinstalls. Without one, sizes are as packaged in the AAR, and the
# report records which so a stripped/packaged pair is never compared.
find_strip() {
    local candidate
    for candidate in "${VALDI_SIZE_STRIP:-}" "$(command -v llvm-strip 2> /dev/null)" \
        "${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-/nonexistent}}"/toolchains/llvm/prebuilt/*/bin/llvm-strip; do
        [[ -n "$candidate" && -x "$candidate" ]] && { echo "$candidate"; return; }
    done
}
STRIP_TOOL="$(find_strip)"
NATIVE_MEASURE="packaged"
[[ -n "$STRIP_TOOL" ]] && NATIVE_MEASURE="stripped"

if [[ -n "$AAR" && -f "$AAR" ]]; then
    mkdir -p "$WORK_DIR/aar"
    if unzip -q -o "$AAR" 'jni/*' -d "$WORK_DIR/aar"; then
        # Sorted for a stable report order; arm64-v8a is the only ABI CI exports today.
        find "$WORK_DIR/aar/jni" -mindepth 2 -maxdepth 2 -type f -name '*.so' | LC_ALL=C sort \
            | while read -r so; do
                rel="${so#"$WORK_DIR/aar/jni/"}"
                if [[ -n "$STRIP_TOOL" ]]; then
                    if ! "$STRIP_TOOL" --strip-unneeded -o "$so.stripped" "$so"; then
                        touch "$WORK_DIR/strip_failed"
                        continue
                    fi
                    so="$so.stripped"
                fi
                printf '%s\t%s\n' "$rel" "$so"
            done > "$WORK_DIR/native.tsv"
    fi
    if [[ -e "$WORK_DIR/strip_failed" ]]; then
        # A partial list would read as libraries being removed. Report none instead.
        : > "$WORK_DIR/native.tsv"
        warn "$STRIP_TOOL failed on a native library; native sizes not measured this run."
    fi
    echo "Native libraries measured: $NATIVE_MEASURE${STRIP_TOOL:+ (with $STRIP_TOOL)}"
    [[ -s "$WORK_DIR/native.tsv" || -e "$WORK_DIR/strip_failed" ]] || warn "no jni/*/*.so in $AAR; native sizes not measured."
else
    warn "no exported AAR available; native library sizes not measured this run."
fi

if ! python3 "$HELPER" collect \
    --valdimodules "$WORK_DIR/valdimodules.tsv" \
    --native "$WORK_DIR/native.tsv" \
    --ignore "$IGNORE_FILE" \
    --bytecode-format "$BYTECODE_FORMAT" \
    --native-measure "$NATIVE_MEASURE" \
    --commit "$(git -C "$OPEN_SOURCE_DIR" rev-parse HEAD 2> /dev/null || echo unknown)" \
    --out "$REPORT"; then
    warn "could not write the size report."
    exit 0
fi

if [[ -n "${VALDI_SIZE_OUT:-}" ]]; then
    mkdir -p "$(dirname "$VALDI_SIZE_OUT")"
    cp "$REPORT" "$VALDI_SIZE_OUT"
fi

# --- 4. Baseline ----------------------------------------------------------------------------
BASELINE=""
BASELINE_NOTE=""

find_baseline() {
    local repo="${GITHUB_REPOSITORY:-}" base="${VALDI_SIZE_BASE_SHA:-}"
    if [[ -z "${GITHUB_ACTIONS:-}" || -z "$repo" || -z "${GH_TOKEN:-}" ]]; then
        BASELINE_NOTE="Not running in GitHub Actions with a token, so no baseline was looked up."
        return
    fi
    if ! [[ "$base" =~ ^[0-9a-f]{40}$ ]]; then
        BASELINE_NOTE="No base commit for this event."
        return
    fi
    if ! command -v gh > /dev/null 2>&1; then
        BASELINE_NOTE="The gh CLI is not installed, so no baseline was looked up."
        return
    fi

    local ancestors artifacts
    if ! ancestors="$(gh api "repos/$repo/commits?sha=$base&per_page=$MAX_ANCESTORS" --jq '.[].sha' 2> /dev/null)"; then
        BASELINE_NOTE="Could not list the base commit's ancestors."
        return
    fi
    # Newest first. Only unexpired artifacts from runs on the baseline branch.
    if ! artifacts="$(gh api "repos/$repo/actions/artifacts?name=$ARTIFACT_NAME&per_page=100" \
        --jq ".artifacts[] | select(.expired == false and .workflow_run.head_branch == \"$BASELINE_BRANCH\")
              | [.id, .workflow_run.id, .workflow_run.head_sha, .size_in_bytes] | @tsv" 2> /dev/null)"; then
        BASELINE_NOTE="Could not list baseline artifacts."
        return
    fi

    local sha line art_id run_id art_size run_info
    for sha in $ancestors; do
        [[ "$sha" =~ ^[0-9a-f]{40}$ ]] || continue
        line="$(awk -F'\t' -v s="$sha" '$3 == s { print; exit }' <<< "$artifacts")"
        [[ -n "$line" ]] || continue
        IFS=$'\t' read -r art_id run_id _ art_size <<< "$line"
        [[ "$art_id" =~ ^[0-9]+$ && "$run_id" =~ ^[0-9]+$ && "$art_size" =~ ^[0-9]+$ ]] || continue

        # The artifact must come from a push to the baseline branch of THIS repo running THIS
        # workflow. A PR run cannot pass for one: its event is pull_request.
        run_info="$(gh api "repos/$repo/actions/runs/$run_id" \
            --jq '[.event, .head_repository.full_name, .head_branch, .path] | @tsv' 2> /dev/null)" || continue
        if [[ "$run_info" != "push"$'\t'"$repo"$'\t'"$BASELINE_BRANCH"$'\t'"$WORKFLOW_PATH" ]]; then
            continue
        fi
        if (( art_size > MAX_ZIP_BYTES )); then
            BASELINE_NOTE="The baseline artifact for ${sha:0:12} is implausibly large; ignored."
            return
        fi

        mkdir -p "$WORK_DIR/baseline"
        if ! gh api "repos/$repo/actions/artifacts/$art_id/zip" 2> /dev/null \
            | head -c "$((MAX_ZIP_BYTES + 1))" > "$WORK_DIR/baseline/artifact.zip"; then
            BASELINE_NOTE="Downloading the baseline artifact for ${sha:0:12} failed."
            return
        fi
        # Extract exactly one member to stdout: no paths from the archive touch the filesystem.
        unzip -p "$WORK_DIR/baseline/artifact.zip" size-report.json 2> /dev/null \
            | head -c "$((MAX_REPORT_BYTES + 1))" > "$WORK_DIR/baseline/size-report.json"
        if [[ ! -s "$WORK_DIR/baseline/size-report.json" ]]; then
            BASELINE_NOTE="The baseline artifact for ${sha:0:12} has no size-report.json."
            return
        fi
        BASELINE="$WORK_DIR/baseline/size-report.json"
        echo "Baseline: $ARTIFACT_NAME from run $run_id at ${sha:0:12}"
        return
    done
    BASELINE_NOTE="No $ARTIFACT_NAME artifact from a $BASELINE_BRANCH push within $MAX_ANCESTORS commits of ${base:0:12} (expired, or that range never ran this workflow)."
}
find_baseline

# --- 5. Size-change trailer -----------------------------------------------------------------
MSG_FILE="$WORK_DIR/head_message.txt"
: > "$MSG_FILE"
HEAD_SHA="${VALDI_SIZE_HEAD_SHA:-}"
if [[ "$HEAD_SHA" =~ ^[0-9a-f]{40}$ ]]; then
    if ! git -C "$OPEN_SOURCE_DIR" log -1 --format=%B "$HEAD_SHA" > "$MSG_FILE" 2> /dev/null \
        && [[ -n "${GITHUB_REPOSITORY:-}" && -n "${GH_TOKEN:-}" ]] && command -v gh > /dev/null 2>&1; then
        # Shallow checkouts do not have the PR head commit; the API does, fork PRs included.
        gh api "repos/$GITHUB_REPOSITORY/commits/$HEAD_SHA" --jq '.commit.message' > "$MSG_FILE" 2> /dev/null || : > "$MSG_FILE"
    fi
fi

# --- 6. Compare and report ------------------------------------------------------------------
COMPARE_ARGS=(compare --current "$REPORT" --commit-message-file "$MSG_FILE")
[[ -n "$BASELINE" ]] && COMPARE_ARGS+=(--baseline "$BASELINE")
[[ -n "$BASELINE_NOTE" ]] && COMPARE_ARGS+=(--baseline-note "$BASELINE_NOTE")
[[ -n "${GITHUB_STEP_SUMMARY:-}" ]] && COMPARE_ARGS+=(--summary "$GITHUB_STEP_SUMMARY")
[[ "${VALDI_SIZE_ANNOTATE:-0}" == "1" ]] && COMPARE_ARGS+=(--annotate)

python3 "$HELPER" "${COMPARE_ARGS[@]}" || warn "size comparison failed."
exit 0
