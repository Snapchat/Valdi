#!/usr/bin/env bash
#
# Hot-reload smoke test.
#
# Proves the compiler's --monitor mode recompiles when a watched source file
# changes: build a valdi_hotreload target, start it, edit a .tsx, and confirm a
# recompilation pass fires. This exercises the CLI hotreload path + the
# compiler's file watcher (watchman) + the AutoRecompiler, none of which any
# other CI job covers.
#
# Scope: compiler-side recompile only. A true app-applied reload is macOS-only
# (the Linux standalone runtime hard-disables the reloader), so this does not
# assert an app received the update — only that the recompile pass ran.
#
# Requires: watchman on PATH, a Swift toolchain (built from source on Linux via
# use_local_compiler, which Copybara flips on for the public repo).
set -uo pipefail

# Run from the open_source workspace root so `//apps/helloworld:...` resolves and
# the reloader's workspace-relative paths (see the run_hotreloader note below)
# line up, regardless of the caller's cwd. Every sibling tools/ci script does
# this; hotreload_smoke.sh previously relied on the caller, which broke the
# internal macOS leg (it invoked from client/, where //apps/helloworld is not a
# package). Matches compiler_tests.sh et al.
cd "$(dirname "$0")/../.."

TARGET="${HOTRELOAD_TARGET:-//apps/helloworld:hello_world_hotreload}"
EDIT_FILE="${HOTRELOAD_EDIT_FILE:-apps/helloworld/src/valdi/hello_world/src/HelloWorldApp.tsx}"
UP_TIMEOUT="${HOTRELOAD_UP_TIMEOUT:-360}"
RECOMPILE_TIMEOUT="${HOTRELOAD_RECOMPILE_TIMEOUT:-120}"
# Extra bazel build flags. Empty on the public repo (Copybara defaults
# use_local_compiler=true there); set to --//bzl/valdi:use_local_compiler=true
# to run against the internal monorepo checkout, which otherwise reaches for the
# GCS-gated prebuilt compiler.
BUILD_FLAGS="${HOTRELOAD_BUILD_FLAGS:-}"

log() { echo "[hotreload-smoke] $*"; }

# macOS only. The reloader only reaches a ready state on macOS; on Linux the
# standalone runtime hard-disables it (see header), so it never signals ready
# and the wait below always times out. Skip rather than fail there.
if [ "$(uname -s)" != "Darwin" ]; then
  log "SKIP: hotreload smoke is macOS-only (Linux standalone runtime disables the reloader)."
  exit 0
fi

if ! command -v watchman >/dev/null 2>&1; then
  log "FAILED: watchman not on PATH (required by the compiler's file watcher)"
  exit 1
fi

# The reloader talks to watchman through a plain `watchman get-sockname`, which
# auto-spawns a daemon at watchman's default socket. On the internal SnapCI macOS
# VM two things break that: (1) the default socket lives under a per-user state
# dir the VM never creates, and (2) macOS watchman auto-spawns via the launchd
# "site spawner", which a headless CI VM has no user session for -- so the spawn
# fails ("unable to talk to your watchman ... No such file or directory") and the
# reloader never comes up. Point watchman at a private, writable state dir and
# spawn the daemon ourselves with --no-site-spawner (a direct fork, no launchd);
# the reloader -- same binary, inherited env -- then finds this running daemon
# instead of trying to spawn its own. Setting both XDG_STATE_HOME (newer watchman)
# and WATCHMAN_SOCK (older) covers both socket-resolution schemes. This is
# deterministic on the internal macOS runner, the external GitHub runner, and dev
# boxes, and never touches a shared daemon (so tearing it down in cleanup is safe).
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

# The internal monorepo runners expose the Bazel wrapper as `bzl` (plain `bazel`
# is not on PATH there); the public GitHub Actions runner has `bazel`. Pick
# whichever is present so the script works in both, run_tests.sh (internal) and
# the standalone hotreload-smoke job (external).
if command -v bzl >/dev/null 2>&1; then
  BAZEL=bzl
elif command -v bazel >/dev/null 2>&1; then
  BAZEL=bazel
elif command -v bazelisk >/dev/null 2>&1; then
  BAZEL=bazelisk
else
  log "FAILED: no bzl/bazel/bazelisk on PATH"
  exit 1
fi

log "Building $TARGET"
# run_hotreloader.sh execs the compiler (and its companion/toolbox) by their
# bazel-out paths, but those are inputs to the script-generating action, not
# outputs of this target. Under the runners' default --remote_download_minimal a
# cache-hit compiler stays remote and the script dies with "No such file"
# (surfaces on any compiler-source change, which re-keys the action). Force all
# build outputs local so the referenced binaries are materialized.
# Placed after $BUILD_FLAGS so this download setting wins if a caller's flags
# also set --remote_download_* (Bazel takes the last value).
# shellcheck disable=SC2086
"$BAZEL" build $BUILD_FLAGS --remote_download_outputs=all "$TARGET" || { log "FAILED: could not build $TARGET"; exit 1; }

# shellcheck disable=SC2086
SCRIPT="$("$BAZEL" cquery --output=files $BUILD_FLAGS "$TARGET" 2>/dev/null | head -n1)"
if [ -z "$SCRIPT" ] || [ ! -f "$SCRIPT" ]; then
  log "FAILED: could not resolve the built hotreloader script for $TARGET"
  exit 1
fi
log "Hotreloader script: $SCRIPT"

if [ ! -f "$EDIT_FILE" ]; then
  log "FAILED: edit target $EDIT_FILE does not exist"
  exit 1
fi

LOG="$(mktemp)"
BACKUP="$(mktemp)"
# Guard the backup: without it, a failed cp (no set -e here) would leave BACKUP
# as the empty mktemp file, and the cleanup trap would then overwrite the real
# source with that 0-byte file.
cp "$EDIT_FILE" "$BACKUP" || { log "FAILED: could not back up $EDIT_FILE"; exit 1; }

# run_hotreloader.sh uses paths relative to the workspace root and $PWD, so it
# must run from here (the CLI runs it with cwd = workspace root).
bash "$SCRIPT" > "$LOG" 2>&1 &
HRPID=$!

cleanup() {
  kill "$HRPID" 2>/dev/null || true
  pkill -f local_valdi_compiler 2>/dev/null || true
  pkill -f run_hotreloader 2>/dev/null || true
  # Shut down the private watchman daemon started above. Safe because it has its
  # own state dir (XDG_STATE_HOME/WATCHMAN_SOCK point into WM_PRIVATE_STATE), so
  # this is not the shared daemon a dev box may run for other projects -- unlike
  # a bare shutdown-server, it cannot wipe their watches.
  if [ -n "${WM_PRIVATE_STATE:-}" ]; then
    watchman shutdown-server >/dev/null 2>&1 || true
    rm -rf "$WM_PRIVATE_STATE"
  fi
  if [ -s "$BACKUP" ]; then
    cp "$BACKUP" "$EDIT_FILE" 2>/dev/null || true
  fi
  rm -f "$BACKUP" "$LOG"
}
trap cleanup EXIT

log "Waiting up to ${UP_TIMEOUT}s for the reloader to come up..."
UP=0
for ((i=1; i<=UP_TIMEOUT; i++)); do
  if grep -qE "Reloader listening on port|waiting for file changes" "$LOG"; then UP=1; break; fi
  if ! kill -0 "$HRPID" 2>/dev/null; then log "reloader exited before it was ready"; break; fi
  sleep 1
done
if [ "$UP" != 1 ]; then
  log "FAILED: reloader never became ready in ${UP_TIMEOUT}s"
  echo "--- last 40 log lines ---"; tail -40 "$LOG"
  exit 1
fi
log "Reloader up after ${i}s."

# A real content change (not a no-op touch) so watchman reliably fires.
printf '\n// hotreload smoke marker %s\n' "$$" >> "$EDIT_FILE"
log "Edited $EDIT_FILE; waiting up to ${RECOMPILE_TIMEOUT}s for a recompilation pass..."

RECOMPILED=0
for ((i=1; i<=RECOMPILE_TIMEOUT; i++)); do
  if grep -qE "Files changed - starting recompilation pass" "$LOG"; then RECOMPILED=1; break; fi
  if ! kill -0 "$HRPID" 2>/dev/null; then log "reloader died during the recompile wait"; break; fi
  sleep 1
done
if [ "$RECOMPILED" != 1 ]; then
  log "FAILED: no recompilation pass within ${RECOMPILE_TIMEOUT}s of the edit"
  echo "--- last 40 log lines ---"; tail -40 "$LOG"
  exit 1
fi

log "PASS: file change triggered a recompilation pass."
exit 0
