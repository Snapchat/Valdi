#!/usr/bin/env bash
#
# Builds the iOS app of an already-bootstrapped Valdi project, installs it on a fresh
# simulator, launches it, and fails if the process is not still running afterwards.
#
# A build can't catch launch-time failures that UIKit raises for the SDK in use (for
# example "UIScene life cycle is required for apps built with this SDK"): the app compiles,
# then exits before any Valdi code runs.
#
# Usage: ios_launch_smoke.sh <bootstrapped app dir>
# Env:
#   SMOKE_LAUNCH_SECONDS  how long the app must stay alive after launch (default 10)
#   SMOKE_IOS_TARGET      the ios_application label to build (default: the first one in the workspace)

set -euo pipefail

APP_DIR="$(cd "${1:?usage: ios_launch_smoke.sh <bootstrapped app dir>}" && pwd)"
LAUNCH_SECONDS="${SMOKE_LAUNCH_SECONDS:-10}"

if [[ $(uname) != Darwin ]]; then
    echo "ios_launch_smoke.sh needs macOS and the iOS simulator" >&2
    exit 1
fi

BAZEL=bazel
command -v bazel >/dev/null || BAZEL=bzl
BUILD_FLAGS=(--repo_env=VALDI_PLATFORM_DEPENDENCIES=ios --ios_multi_cpus=sim_arm64)

WORK_DIR="$(mktemp -d)"
SIM_UDID=""
cleanup() {
    if [[ -n "$SIM_UDID" ]]; then
        xcrun simctl shutdown "$SIM_UDID" >/dev/null 2>&1 || true
        xcrun simctl delete "$SIM_UDID" >/dev/null 2>&1 || true
    fi
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

cd "$APP_DIR"

TARGET="${SMOKE_IOS_TARGET:-$("$BAZEL" query 'kind("ios_application", //...)' | head -n 1)}"
if [[ -z "$TARGET" ]]; then
    echo "No ios_application target found in $APP_DIR" >&2
    exit 1
fi
echo "Building $TARGET"
"$BAZEL" build "${BUILD_FLAGS[@]}" "$TARGET"

IPA="$("$BAZEL" cquery "${BUILD_FLAGS[@]}" --output=files "$TARGET" | grep -E '\.ipa$' | head -n 1)"
if [[ -z "$IPA" ]]; then
    echo "No .ipa among the outputs of $TARGET" >&2
    exit 1
fi
unzip -q "$APP_DIR/$IPA" -d "$WORK_DIR/ipa"
APP_BUNDLE="$(find "$WORK_DIR/ipa/Payload" -maxdepth 1 -name '*.app' | head -n 1)"
BUNDLE_ID="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$APP_BUNDLE/Info.plist")"
echo "Built $BUNDLE_ID"

# The newest installed iOS runtime, so the app runs against the SDK generation it was built with,
# and a modern iPhone that runtime supports (the global device type list includes phones it rejects).
read -r RUNTIME DEVICE_TYPE < <(xcrun simctl list runtimes available -j | python3 -I -c '
import json, re, sys
runtimes = [r for r in json.load(sys.stdin)["runtimes"] if r["identifier"].startswith("com.apple.CoreSimulator.SimRuntime.iOS-")]
if not runtimes:
    print("", "")
    sys.exit(0)
runtimes.sort(key=lambda r: [int(p) for p in r["version"].split(".")])
runtime = runtimes[-1]
phones = [t for t in runtime.get("supportedDeviceTypes", []) if "iPhone" in t["name"]]
def rank(t):
    m = re.match(r"iPhone (\d+)( Pro)?$", t["name"])
    return (int(m.group(1)), bool(m.group(2))) if m else (-1, False)
phones.sort(key=rank)
print(runtime["identifier"], phones[-1]["identifier"] if phones else "")
')
if [[ -z "${RUNTIME:-}" || -z "${DEVICE_TYPE:-}" ]]; then
    echo "No iOS simulator runtime with an iPhone device type is installed" >&2
    exit 1
fi
echo "Creating a simulator ($DEVICE_TYPE on $RUNTIME)"
SIM_UDID="$(xcrun simctl create valdi-launch-smoke "$DEVICE_TYPE" "$RUNTIME")"
xcrun simctl boot "$SIM_UDID"
xcrun simctl bootstatus "$SIM_UDID" -b >/dev/null

xcrun simctl install "$SIM_UDID" "$APP_BUNDLE"
LAUNCH_OUTPUT="$(xcrun simctl launch "$SIM_UDID" "$BUNDLE_ID")"
echo "$LAUNCH_OUTPUT"

sleep "$LAUNCH_SECONDS"

# `launchctl list` prints "<pid> <status> <label>"; a pid of "-" means the job is no longer running.
APP_PID="$(xcrun simctl spawn "$SIM_UDID" launchctl list | awk -v id="$BUNDLE_ID" '$3 ~ id { print $1 }' | head -n 1)"
if [[ -z "$APP_PID" || "$APP_PID" == "-" ]]; then
    echo "FAIL: $BUNDLE_ID is not running ${LAUNCH_SECONDS}s after launch (simulator $RUNTIME, app built with $(/usr/libexec/PlistBuddy -c 'Print :DTSDKName' "$APP_BUNDLE/Info.plist" 2>/dev/null || echo unknown))" >&2
    APP_EXECUTABLE="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$APP_BUNDLE/Info.plist")"
    echo "Log lines from $APP_EXECUTABLE:" >&2
    xcrun simctl spawn "$SIM_UDID" log show --info --debug --last 2m --style compact \
        --predicate "process == \"$APP_EXECUTABLE\"" 2>/dev/null | tail -n 40 >&2 || true
    exit 1
fi
echo "PASS: $BUNDLE_ID still running (pid $APP_PID) ${LAUNCH_SECONDS}s after launch"
