#!/usr/bin/env bash
#
# Hot-reload *delivery* smoke test.
#
# Goes one step past hotreload_smoke.sh: it doesn't just prove the compiler recompiles
# on a file change, it stands up a mock client that speaks the daemon wire protocol
# (npm_modules/cli/test/helpers/MockAppClient.ts) and asserts the recompiled module is
# actually DELIVERED. The reloader runs in --usb mode (hardcoded by the valdi_hotreload
# rule), so it never opens a TCP acceptor; its simulator auto-connector dials
# localhost:13591 (Ports.reloaderStandalone) looking for an app. We mock that device:
# LISTEN on 13591, let the daemon connect to us, edit the app's .tsx, and assert the
# recompiled module arrives.
#
# Why apps/helloworld (and not a `valdi bootstrap`ed app): a bootstrapped app has @valdi
# as an EXTERNAL repo, so the companion's live recompile reads
# external/valdi~/modules/tsconfig.json from the app execroot, which isn't materialized
# -> ENOENT -> recompile aborts -> nothing delivered. helloworld builds from THIS
# workspace, where @valdi is in-tree, so its recompile initializes cleanly. This mirrors
# hotreload_smoke.sh's target + build path; we add the mock + delivery assertion on top.
#
# Runs on both sides (macOS only): the external "Valdi Smoke Tests" GitHub job and the
# internal client-validate-valdi-open-source-macos leg. Requires watchman + Bazel.
set -uo pipefail

# Run from the open_source workspace root so //apps/helloworld resolves and the
# reloader's workspace-relative paths line up, matching every sibling tools/ci script.
cd "$(dirname "$0")/../.."
OPEN_SOURCE_DIR="$(pwd)"

TARGET="${HOTRELOAD_TARGET:-//apps/helloworld:hello_world_hotreload}"
EDIT_FILE="${HOTRELOAD_EDIT_FILE:-apps/helloworld/src/valdi/hello_world/src/HelloWorldApp.tsx}"
RELOADER_PORT="${RELOADER_PORT:-13591}"
UP_TIMEOUT="${HOTRELOAD_UP_TIMEOUT:-600}"
# Empty on the public repo (Copybara defaults use_local_compiler=true there); set to
# --//bzl/valdi:use_local_compiler=true for the internal monorepo if the prebuilt
# reloader ever misbehaves. --remote_download_outputs=all materializes the
# compiler/companion/toolbox binaries run_hotreloader.sh execs by bazel-out path
# (otherwise --remote_download_minimal leaves a cache hit remote -> "No such file").
BUILD_FLAGS="${HOTRELOAD_BUILD_FLAGS:-} --remote_download_outputs=all"

log() { echo "[hotreload-delivery-smoke] $*"; }

# macOS only, matching hotreload_smoke.sh: the reloader needs a real toolchain and the
# Linux standalone runtime disables it. Skip (not fail) on Linux.
if [ "$(uname -s)" != "Darwin" ]; then
  log "SKIP: hot-reload delivery smoke is macOS-only."
  exit 0
fi

if ! command -v watchman >/dev/null 2>&1; then
  log "FAILED: watchman not on PATH (required by the compiler's file watcher)"
  exit 1
fi

# Private watchman daemon (same dance as hotreload_smoke.sh) so it comes up on a
# headless CI VM. Private state dir means teardown never touches a shared daemon.
WM_PRIVATE_STATE="$(mktemp -d)"
export XDG_STATE_HOME="$WM_PRIVATE_STATE"
export WATCHMAN_SOCK="$WM_PRIVATE_STATE/sock"
if ! watchman --no-site-spawner \
      --logfile="$WM_PRIVATE_STATE/log" \
      --pidfile="$WM_PRIVATE_STATE/pid" \
      get-sockname > "$WM_PRIVATE_STATE/start.out" 2>&1; then
  log "FAILED: watchman on PATH but no daemon could be started (state dir $WM_PRIVATE_STATE)"
  echo "--- watchman get-sockname output ---"; cat "$WM_PRIVATE_STATE/start.out" 2>/dev/null
  echo "--- watchman daemon log ---"; tail -40 "$WM_PRIVATE_STATE/log" 2>/dev/null
  exit 1
fi

# Internal runners expose `bzl`; the public GitHub runner has `bazel`. Pick whichever.
if command -v bzl >/dev/null 2>&1; then BAZEL=bzl
elif command -v bazel >/dev/null 2>&1; then BAZEL=bazel
elif command -v bazelisk >/dev/null 2>&1; then BAZEL=bazelisk
else log "FAILED: no bzl/bazel/bazelisk on PATH"; exit 1
fi

log "Building $TARGET"
# shellcheck disable=SC2086
"$BAZEL" build $BUILD_FLAGS "$TARGET" || { log "FAILED: could not build $TARGET"; exit 1; }
# shellcheck disable=SC2086
SCRIPT="$("$BAZEL" cquery --output=files $BUILD_FLAGS "$TARGET" 2>/dev/null | head -n1)"
if [ -z "$SCRIPT" ] || [ ! -f "$SCRIPT" ]; then
  log "FAILED: could not resolve the built hotreloader script for $TARGET"; exit 1
fi

# Absolute path: the restore in cleanup() runs after we cd into npm_modules/cli, so a
# workspace-relative EDIT_FILE would resolve against the wrong dir and silently fail to
# restore, leaving the marker edit in the tree.
EDIT_FILE_ABS="$OPEN_SOURCE_DIR/$EDIT_FILE"
if [ ! -f "$EDIT_FILE_ABS" ]; then
  log "FAILED: edit target $EDIT_FILE_ABS does not exist"; exit 1
fi

RELOADER_LOG="$(mktemp)"
BACKUP="$(mktemp)"
cp "$EDIT_FILE_ABS" "$BACKUP" || { log "FAILED: could not back up $EDIT_FILE_ABS"; exit 1; }

# Clear the field immediately before launch. The companion's JS-debugger proxy binds a
# hardcoded port 9010 and crashes the whole companion (killing recompiles) if it's taken.
# A prior reloader in the same job (hotreload_smoke.sh) can still be alive and RESPAWN its
# companion onto 9010 during our build window, so freeing 9010 earlier isn't enough: kill
# the respawner (reloader/compiler/companion) AND free 9010 + the sim dial-in port here,
# right before we start our own reloader. Only prior-smoke processes exist at this point.
pkill -9 -f run_hotreloader 2>/dev/null || true
pkill -9 -f local_valdi_compiler 2>/dev/null || true
pkill -9 -f ts_bin_wrapper 2>/dev/null || true
pkill -9 -f remotedebug-ios-webkit-adapter 2>/dev/null || true
for port in 9010 "$RELOADER_PORT"; do
  pids="$(lsof -ti tcp:"$port" 2>/dev/null || true)"
  if [ -n "$pids" ]; then
    log "Freeing port $port (killing stale PIDs: $pids)"
    # shellcheck disable=SC2086
    kill -9 $pids 2>/dev/null || true
  fi
done

# run_hotreloader.sh uses paths relative to the workspace root, so run it from here.
bash "$SCRIPT" > "$RELOADER_LOG" 2>&1 &
HRPID=$!

cleanup() {
  kill "$HRPID" 2>/dev/null || true
  pkill -f local_valdi_compiler 2>/dev/null || true
  pkill -f run_hotreloader 2>/dev/null || true
  if [ -s "$BACKUP" ]; then cp "$BACKUP" "$EDIT_FILE_ABS" 2>/dev/null || true; fi
  if [ -n "${WM_PRIVATE_STATE:-}" ]; then
    watchman shutdown-server >/dev/null 2>&1 || true
    rm -rf "$WM_PRIVATE_STATE"
  fi
  rm -f "$BACKUP"
}
trap cleanup EXIT

log "Waiting up to ${UP_TIMEOUT}s for the reloader to come up..."
for ((i=1; i<=UP_TIMEOUT; i++)); do
  if grep -qE "Reloader listening on port|waiting for file changes|Localhost reloader connection" "$RELOADER_LOG"; then break; fi
  if ! kill -0 "$HRPID" 2>/dev/null; then
    log "FAILED: reloader exited before coming up"; echo "--- last 40 reloader log lines ---"; tail -40 "$RELOADER_LOG"; exit 1
  fi
  sleep 1
done

# Install the CLI package deps (ts-node) the client script needs. install_cli.sh runs
# `npm run cli:install` which provisions npm_modules/cli; cheap if already present.
if [ ! -d "$OPEN_SOURCE_DIR/npm_modules/cli/node_modules/ts-node" ]; then
  mkdir -p ~/.npm-global/lib; npm config set prefix "$HOME/.npm-global"; export PATH="$HOME/.npm-global/bin:$PATH"
  ./tools/ci/install_cli.sh
fi

MARKER="valdi_delivery_${RANDOM}${RANDOM}"
log "Driving the mock client (listening on 127.0.0.1:${RELOADER_PORT}, marker ${MARKER})"
cd "$OPEN_SOURCE_DIR/npm_modules/cli"
RELOADER_PORT="$RELOADER_PORT" \
EDIT_FILE="$EDIT_FILE_ABS" \
MARKER="$MARKER" \
DIAL_TIMEOUT_MS=120000 \
  node -r ts-node/register test/hotreloadDeliveryClient.ts
STATUS=$?

if [ "$STATUS" != 0 ]; then
  log "FAILED: mock client did not observe delivery (exit $STATUS)"
  echo "--- last 60 reloader log lines ---"; tail -60 "$RELOADER_LOG"
fi
exit "$STATUS"
