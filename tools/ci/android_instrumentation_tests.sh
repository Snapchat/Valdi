#!/usr/bin/env bash

# Builds the HelloWorld instrumented tests (//apps/helloworld:hello_world_instrumentation_test)
# and runs them when an Android device or emulator is available.
#
# The tests exercise the open-source Espresso bindings (//valdi:valdi_android_test_support)
# and the test runtime that mounts views for them (//valdi:valdi_android_test_runtime).
#
# Two modes, so the same script covers both CIs:
#   * build only  - the default, and what internal CI runs: SnapCI VMs have no emulator, but
#                   building the APK still gates the test sources, the bindings, and the
#                   cross-workspace @test_mvn wiring.
#   * build + run - external CI (GitHub Actions Linux runner, KVM available) sets
#                   VALDI_START_EMULATOR=1 and the tests actually execute. Same thing happens
#                   locally when you have a device attached or an emulator already running.
#
# Environment:
#   VALDI_ANDROID_ABI     arm64-v8a (default) or x86_64. Defaults to arm64-v8a because the
#                         rest of CI already builds arm64 native libs, so the build gate
#                         reuses that cache. Emulators on x86_64 hosts need x86_64.
#   VALDI_START_EMULATOR  1 to create and boot a headless emulator via the Android SDK.
#   VALDI_EMULATOR_API    API level for the emulator image (default 34).
#   VALDI_BAZEL_OUTPUT_USER_ROOT
#                         Bazel --output_user_root, for hosts where a second, larger disk
#                         is mounted. Not used by GitHub CI: a cold build needs ~25GB and
#                         df there reports / and /mnt as the same device, so the space has
#                         to come from setup_linux_env.sh's reclaim instead.
#   VALDI_RECLAIM_BUILD_SPACE
#                         1 to delete the Bazel output base after the APK is built and
#                         before the emulator boots. A GitHub runner's 72GB disk cannot
#                         hold both a cold build and the emulator's userdata partition.
#                         Off by default: a local run should keep its build state.

set -euo pipefail
set -x

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
OPEN_SOURCE_DIR="$(cd "$SCRIPT_DIR/../../"; pwd)"

ABI="${VALDI_ANDROID_ABI:-arm64-v8a}"
API_LEVEL="${VALDI_EMULATOR_API:-34}"
TEST_TARGET="//apps/helloworld:hello_world_instrumentation_test"
TEST_APK="bazel-bin/apps/helloworld/hello_world_instrumentation_test.apk"
TEST_PACKAGE="com.snap.valdi.hello_world.test"
TEST_RUNNER="androidx.test.runner.AndroidJUnitRunner"
AVD_NAME="valdi_instrumentation"
EMULATOR_PID=""

# The emulator sizes its userdata partition at 7372.80MB and refuses to boot with a FATAL
# if the filesystem cannot hold it. Round up for the system image and the installed APK.
EMULATOR_DISK_MB=8192

# Only kills an emulator this script started. A device or emulator that was already
# running when the script began is left alone. Without this a local run leaves a
# headless emulator behind, and the next run's `avdmanager create --force` fights it
# for the AVD directory.
cleanup() {
    if [ -n "$EMULATOR_PID" ]; then
        kill "$EMULATOR_PID" 2> /dev/null || true
        wait "$EMULATOR_PID" 2> /dev/null || true
    fi
}
trap cleanup EXIT

case "$ABI" in
    arm64-v8a)
        REPO_DEFINE="--define=client_repo_arm64=true"
        ANDROID_PLATFORM="@snap_platforms//os:android_arm64"
        ;;
    x86_64)
        REPO_DEFINE="--define=client_repo_x86_64=true"
        ANDROID_PLATFORM="@snap_platforms//os:android_x86_64"
        ;;
    *)
        echo "[ERROR] Unsupported VALDI_ANDROID_ABI: $ABI (expected arm64-v8a or x86_64)"
        exit 1
        ;;
esac

# Deadlines on the adb/sdkmanager steps, so a stuck prompt or a wedged emulator fails
# instead of burning the CI job's whole budget. `timeout` is coreutils: always there on
# Linux, not in a stock macOS install, so degrade to running the command unbounded.
if command -v timeout > /dev/null; then
    with_timeout() { timeout "$@"; }
else
    with_timeout() { shift; "$@"; }
fi

BAZEL_STARTUP_FLAGS=()
if [ -n "${VALDI_BAZEL_OUTPUT_USER_ROOT:-}" ]; then
    BAZEL_STARTUP_FLAGS+=("--output_user_root=$VALDI_BAZEL_OUTPUT_USER_ROOT")
fi

run_bzl() {
    if [ "${#BAZEL_STARTUP_FLAGS[@]}" -gt 0 ]; then
        bzl "${BAZEL_STARTUP_FLAGS[@]}" "$@"
    else
        bzl "$@"
    fi
}

# Available space in MB on the filesystem holding $1. -P forces one line per filesystem so
# a long device name cannot wrap and shift the columns.
free_mb() {
    df -Pm "$1" | awk 'NR==2 {print $4}'
}

# Nothing past the build needs Bazel, only the APK, so the output base is dead weight that
# the emulator needs the space back from. Stage the APK somewhere Bazel does not own, then
# expunge. The disk cache and repository cache live outside the output base and survive, so
# the next run still restores from them rather than rebuilding from source.
reclaim_build_space() {
    local staged_dir staged_apk
    staged_dir="$(mktemp -d)"
    cp "$TEST_APK" "$staged_dir/"
    staged_apk="$staged_dir/$(basename "$TEST_APK")"

    run_bzl clean --expunge

    TEST_APK="$staged_apk"
    echo "[OK] Reclaimed the Bazel output base; $(free_mb "$staged_dir")MB now free"
}

sdk_root() {
    local root="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
    if [ -z "$root" ]; then
        echo "[ERROR] Neither ANDROID_HOME nor ANDROID_SDK_ROOT is set; cannot manage an emulator" >&2
        exit 1
    fi
    echo "$root"
}

# Bazel downloads its own hermetic SDK, so adb is only needed for install/run.
adb_bin() {
    if command -v adb > /dev/null; then
        command -v adb
    else
        echo "$(sdk_root)/platform-tools/adb"
    fi
}

start_emulator() {
    local root
    root="$(sdk_root)"
    local image="system-images;android-${API_LEVEL};default;${ABI}"

    # Pin where the AVD lives so avdmanager and the emulator cannot disagree. Left unset,
    # avdmanager writes to $ANDROID_SDK_HOME/.android/avd while the emulator searches
    # $ANDROID_SDK_HOME/avd, and a mismatch presents as "Unknown AVD name" right after a
    # create that looked like it worked.
    export ANDROID_AVD_HOME="${ANDROID_AVD_HOME:-$HOME/.android/avd}"
    mkdir -p "$ANDROID_AVD_HOME"

    # Check before the SDK install rather than after: out of space, the emulator exits with
    # "Not enough space to create userdata partition" and never opens a port, which then
    # surfaces 300s later as `adb wait-for-device` timing out. That reads as an adb or KVM
    # problem and sends the next person looking in the wrong place.
    local free
    free="$(free_mb "$ANDROID_AVD_HOME")"
    if [ "$free" -lt "$EMULATOR_DISK_MB" ]; then
        echo "[ERROR] Only ${free}MB free at $ANDROID_AVD_HOME; the emulator needs ~${EMULATOR_DISK_MB}MB for its userdata partition"
        echo "  Set VALDI_RECLAIM_BUILD_SPACE=1 to drop the Bazel output base before booting."
        df -h "$ANDROID_AVD_HOME" 2>&1 | sed 's/^/    /' || true
        exit 1
    fi

    # Every prompt gets an answer and every step gets a deadline: sdkmanager asks for
    # licence acceptance on a cold SDK, and an unanswered prompt hangs the job until the
    # runner's own timeout rather than failing.
    yes | "$root/cmdline-tools/latest/bin/sdkmanager" --licenses > /dev/null || true

    # `yes` is still writing when sdkmanager exits, so it dies of SIGPIPE and, under
    # `set -o pipefail`, fails the whole pipeline even on a clean install. Read the status of
    # the tool out of PIPESTATUS instead of the pipeline's, at both prompt-fed steps.
    # `set +e` rather than `|| true`: appending `|| true` would run another command and reset
    # PIPESTATUS to 0, masking every failure here.
    local sdkmanager_status avdmanager_status

    set +e +o pipefail
    yes | with_timeout 900 "$root/cmdline-tools/latest/bin/sdkmanager" \
        --install "platform-tools" "emulator" "$image"
    sdkmanager_status="${PIPESTATUS[1]}"
    set -e -o pipefail
    if [ "$sdkmanager_status" -ne 0 ]; then
        echo "[ERROR] sdkmanager exited $sdkmanager_status installing platform-tools, emulator and $image"
        exit 1
    fi

    set +e +o pipefail
    echo no | with_timeout 300 "$root/cmdline-tools/latest/bin/avdmanager" create avd \
        --name "$AVD_NAME" --package "$image" --force
    avdmanager_status="${PIPESTATUS[1]}"
    set -e -o pipefail
    if [ "$avdmanager_status" -ne 0 ]; then
        echo "[ERROR] avdmanager exited $avdmanager_status creating AVD '$AVD_NAME'"
        exit 1
    fi

    # avdmanager's exit code does not mean the AVD exists. Measured against
    # cmdline-tools: with the target filesystem nearly full it exits 0 having written a
    # truncated .ini, and it has been seen on GitHub's runners to exit 0 writing nothing at
    # all. Only a loud failure (no space at all, unwritable directory) exits non-zero with
    # "Error: AVD not created.". So ask the emulator, which is the thing that has to find it.
    # -list-avds alone is not enough: it lists an AVD whose .ini is present but truncated,
    # which is exactly what a nearly-full disk produces. Follow the .ini's path= to the
    # config.ini the emulator actually boots from.
    local avd_ini="$ANDROID_AVD_HOME/$AVD_NAME.ini"
    local avd_dir=""
    if [ -f "$avd_ini" ]; then
        avd_dir="$(sed -n 's/^path=//p' "$avd_ini" | head -1)"
    fi

    if ! "$root/emulator/emulator" -list-avds 2> /dev/null | grep -qx "$AVD_NAME" \
        || [ -z "$avd_dir" ] || [ ! -f "$avd_dir/config.ini" ]; then
        echo "[ERROR] avdmanager reported success but AVD '$AVD_NAME' is missing or incomplete"
        echo "  expected .ini: $avd_ini"
        echo "  path= resolved to: ${avd_dir:-<empty or no path= line>}"
        echo "  config.ini present: $([ -n "$avd_dir" ] && [ -f "$avd_dir/config.ini" ] && echo yes || echo no)"
        echo "  ANDROID_AVD_HOME=${ANDROID_AVD_HOME:-<unset>}"
        echo "  ANDROID_SDK_HOME=${ANDROID_SDK_HOME:-<unset>}"
        echo "  HOME=$HOME"
        echo "  AVDs the emulator can see:"
        "$root/emulator/emulator" -list-avds 2>&1 | sed 's/^/    /' || true
        echo "  contents of $ANDROID_AVD_HOME:"
        ls -la "$ANDROID_AVD_HOME" 2>&1 | sed 's/^/    /' || true
        echo "  other places an AVD may have landed:"
        find "$HOME/.android" "${ANDROID_SDK_HOME:-/nonexistent}" -maxdepth 3 -name '*.ini' \
            2> /dev/null | sed 's/^/    /' || true
        echo "  free space (avdmanager exits 0 having written a partial AVD when this is tight):"
        df -h "$ANDROID_AVD_HOME" 2>&1 | sed 's/^/    /' || true
        exit 1
    fi

    # Headless, software GL: no display or GPU on a CI runner.
    "$root/emulator/emulator" -avd "$AVD_NAME" \
        -no-window -no-audio -no-boot-anim -no-snapshot -gpu swiftshader_indirect \
        -read-only -wipe-data &
    EMULATOR_PID=$!

    local adb
    adb="$(adb_bin)"
    if ! with_timeout 300 "$adb" wait-for-device; then
        echo "[ERROR] No device appeared within 300s of starting the emulator"
        exit 1
    fi

    echo "Waiting for the emulator to finish booting..."
    local waited=0
    until [ "$("$adb" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = "1" ]; do
        if [ "$waited" -ge 600 ]; then
            echo "[ERROR] Emulator did not boot within 600s"
            exit 1
        fi
        sleep 5
        waited=$((waited + 5))
    done

    "$adb" shell input keyevent 82 || true
}

has_device() {
    local adb
    adb="$(adb_bin)"
    [ -x "$adb" ] || return 1
    "$adb" devices | grep -qw "device"
}

# An arm64 phone cannot run an x86_64 APK, and vice versa. Checking up front turns a
# confusing INSTALL_FAILED_NO_MATCHING_ABIS into a clear message.
device_supports_abi() {
    local adb abilist
    adb="$(adb_bin)"
    abilist="$("$adb" shell getprop ro.product.cpu.abilist 2> /dev/null | tr -d '\r')"
    case ",$abilist," in
        *",$ABI,"*) return 0 ;;
        *) return 1 ;;
    esac
}

pushd "$OPEN_SOURCE_DIR"

# --android_platforms, not --fat_apk_cpu or --android_cpu: aar_import resolves the AAR's
# native libs from the target platform's CPU constraint (rules_android aar_import/impl.bzl,
# ctx.target_platform_has_constraint), so without it the build asks the AAR for whatever
# ABI the host implies and dies with "missing native libs for requested architecture".
# The --define picks the ABI the AAR is built for; these two have to agree.
run_bzl build "$TEST_TARGET" "$REPO_DEFINE" "--android_platforms=$ANDROID_PLATFORM"

if [ ! -f "$TEST_APK" ]; then
    echo "[ERROR] Test APK not found at $TEST_APK"
    exit 1
fi

echo "[OK] Built $TEST_TARGET for $ABI"

if [ "${VALDI_START_EMULATOR:-0}" = "1" ]; then
    if [ "${VALDI_RECLAIM_BUILD_SPACE:-0}" = "1" ] \
        && [ "$(free_mb "$OPEN_SOURCE_DIR")" -lt "$EMULATOR_DISK_MB" ]; then
        reclaim_build_space
    fi
    start_emulator
fi

SKIP_REASON=""
if ! has_device; then
    SKIP_REASON="no Android device or emulator is connected"
elif ! device_supports_abi; then
    SKIP_REASON="the connected device does not support $ABI (set VALDI_ANDROID_ABI to match it)"
fi

if [ -n "$SKIP_REASON" ]; then
    # Whoever asked for an emulator wants the tests to actually run, so a missing or
    # mismatched device is a failure there rather than a silent green.
    if [ "${VALDI_START_EMULATOR:-0}" = "1" ]; then
        echo "[FAILED] Expected to run the tests but $SKIP_REASON"
        exit 1
    fi

    echo "================================================================"
    echo "[SKIPPED] Built the test APK but did not run it: $SKIP_REASON."
    echo "Attach a matching device, boot an emulator, or set VALDI_START_EMULATOR=1."
    echo "================================================================"
    popd
    exit 0
fi

ADB="$(adb_bin)"

with_timeout 600 "$ADB" install -r -g "$TEST_APK"

# am instrument exits 0 even when tests fail, so scan the output for the JUnit verdict.
INSTRUMENT_LOG="$(mktemp)"
set +e
with_timeout 1800 "$ADB" shell am instrument -w -r "$TEST_PACKAGE/$TEST_RUNNER" 2>&1 | tee "$INSTRUMENT_LOG"
INSTRUMENT_STATUS=$?
set -e

if [ "$INSTRUMENT_STATUS" -ne 0 ]; then
    echo "[FAILED] adb instrument exited with $INSTRUMENT_STATUS"
    exit 1
fi

if grep -q "INSTRUMENTATION_RESULT: shortMsg" "$INSTRUMENT_LOG"; then
    echo "[FAILED] The instrumentation process crashed"
    exit 1
fi

if ! grep -q "^INSTRUMENTATION_STATUS_CODE: 0" "$INSTRUMENT_LOG" && \
   ! grep -q "OK (" "$INSTRUMENT_LOG"; then
    echo "[FAILED] Instrumentation did not report a passing run"
    exit 1
fi

if grep -qE "FAILURES!!!|INSTRUMENTATION_STATUS: stack=" "$INSTRUMENT_LOG"; then
    echo "[FAILED] Instrumented tests reported failures"
    exit 1
fi

rm -f "$INSTRUMENT_LOG"

echo "================================================================"
echo "[PASSED] HelloWorld instrumented tests"
echo "================================================================"

popd
