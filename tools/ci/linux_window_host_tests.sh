#!/usr/bin/env bash
# Experimental non-blocking CI job; failures still propagate from this script.
# Builds the separately rooted module and exercises the real SDL offscreen driver.
set -euo pipefail

if [[ $(uname) != Linux ]]; then
    echo "The SDL window host test requires Linux." >&2
    exit 1
fi

cd "$(dirname "$0")/../../linux_window_host"
if command -v bazel >/dev/null 2>&1; then
    bazel_cmd=bazel
elif command -v bzl >/dev/null 2>&1; then
    bazel_cmd=bzl
else
    echo "Install Bazel $(cat .bazelversion) or Bazelisk to run the host test." >&2
    exit 1
fi

# The shared CI cache action exports a cache-only rc for secondary workspaces.
# Do not import the parent workspace's label-based build configuration.
startup_flags=()
if [[ -n "${BAZEL_CACHE_RC:-}" ]]; then
    startup_flags+=("--bazelrc=$BAZEL_CACHE_RC")
fi

# SDL's offscreen driver has a software surface path, so no X11/Wayland server,
# system SDL installation, GPU or Mesa renderer is needed for this first gate.
# Extra arguments let local callers provide machine-specific toolchain options.
"$bazel_cmd" "${startup_flags[@]}" test //valdi_linux_window:framebuffer_test \
    //valdi_linux_window:sdl_window_host_test \
    --test_env=SDL_VIDEODRIVER=offscreen \
    --test_env=SDL_FRAMEBUFFER_ACCELERATION=0 \
    --test_output=errors --test_timeout=60 "$@"
