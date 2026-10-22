#!/usr/bin/env bash
# Run an already-built executable under private X11 and Wayland displays.
# Extra arguments precede the per-display snapshot path.
set -euo pipefail

host_test=$(realpath "${1:?Usage: run-displays.sh /path/to/executable [output-directory [arguments...]]}")
output=$(realpath -m "${2:-window-host-results}")
command_args=("${@:3}")
mkdir -p "$output"
runtime=$(mktemp -d)
display_pid=
cleanup() {
    if [[ -n "$display_pid" ]]; then
        kill "$display_pid" 2>/dev/null || true
        wait "$display_pid" 2>/dev/null || true
    fi
    rm -rf "$runtime"
}
trap cleanup EXIT

Xvfb -displayfd 3 -screen 0 1280x900x24 -nolisten tcp -noreset \
    3>"$runtime/x11-display" >"$output/xvfb.log" 2>&1 &
display_pid=$!
for _ in {1..100}; do
    [[ -s "$runtime/x11-display" ]] && break
    kill -0 "$display_pid"
    sleep 0.05
done
DISPLAY=":$(cat "$runtime/x11-display")" SDL_VIDEODRIVER=x11 SDL_FRAMEBUFFER_ACCELERATION=0 \
    timeout 30 "$host_test" "${command_args[@]}" "$output/x11.bmp" >"$output/x11.log" 2>&1
kill "$display_pid"
wait "$display_pid" 2>/dev/null || true
display_pid=

for scale in 1 2; do
    mkdir -m 700 "$runtime/wayland-$scale"
    XDG_RUNTIME_DIR="$runtime/wayland-$scale" weston --backend=headless --renderer=pixman --fake-seat \
        --width=$((1280 * scale)) --height=$((900 * scale)) --scale="$scale" \
        --socket=window-host-test --no-config --idle-time=0 \
        --log="$output/weston-$scale.log" >"$output/weston-$scale-stdio.log" 2>&1 &
    display_pid=$!
    for _ in {1..100}; do
        [[ -S "$runtime/wayland-$scale/window-host-test" ]] && break
        kill -0 "$display_pid"
        sleep 0.05
    done
    # Keep SDL's default surface presentation path: disabling its acceleration
    # is not supported by every Wayland video backend. Mesa software rendering
    # makes this a CPU/VM check, not a physical GPU qualification.
    env -u SDL_FRAMEBUFFER_ACCELERATION XDG_RUNTIME_DIR="$runtime/wayland-$scale" \
        WAYLAND_DISPLAY=window-host-test SDL_VIDEODRIVER=wayland LIBGL_ALWAYS_SOFTWARE=1 \
        timeout 30 "$host_test" "${command_args[@]}" "$output/wayland-$scale.bmp" >"$output/wayland-$scale.log" 2>&1
    kill "$display_pid"
    wait "$display_pid" 2>/dev/null || true
    display_pid=
done
cat "$output/x11.log" "$output/wayland-1.log" "$output/wayland-2.log"
