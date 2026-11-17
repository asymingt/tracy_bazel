#!/usr/bin/env bash
# Starts tracy-profiler under a headless display server and checks that it is
# still running (i.e. initialised its window, GL context and fonts) after a few
# seconds. Linux only; needs weston (Wayland) or Xvfb (X11) plus Mesa on the
# host -- see the "Headless profiler tests" step in .github/workflows/bazel.yaml.
#
# usage: run_headless.sh <tracy-profiler> wayland|x11
set -uo pipefail

profiler="$1"
backend="$2"
seconds=8

runtime="$(mktemp -d "${TEST_TMPDIR:-/tmp}/xdg.XXXXXX")"
chmod 700 "$runtime"
export XDG_RUNTIME_DIR="$runtime"
export HOME="${TEST_TMPDIR:-$runtime}"
# Mesa software rendering: no GPU on CI runners.
export LIBGL_ALWAYS_SOFTWARE=1
unset WAYLAND_DISPLAY DISPLAY XDG_SESSION_TYPE

server_pid=""
cleanup() { [ -n "$server_pid" ] && kill "$server_pid" 2>/dev/null; }
trap cleanup EXIT

case "$backend" in
  wayland)
    socket="tracy-test-$$"
    /usr/bin/weston --backend=headless --renderer=pixman --socket="$socket" \
      --idle-time=0 >"$runtime/server.log" 2>&1 &
    server_pid=$!
    export WAYLAND_DISPLAY="$socket"
    for _ in $(seq 50); do [ -S "$runtime/$socket" ] && break; sleep 0.1; done
    [ -S "$runtime/$socket" ] || { echo "weston did not start:"; cat "$runtime/server.log"; exit 1; }
    ;;
  x11)
    display=":$(( 90 + $$ % 900 ))"
    /usr/bin/Xvfb "$display" -screen 0 1280x800x24 -nolisten tcp \
      >"$runtime/server.log" 2>&1 &
    server_pid=$!
    export DISPLAY="$display"
    for _ in $(seq 50); do [ -S "/tmp/.X11-unix/X${display#:}" ] && break; sleep 0.1; done
    ;;
  *) echo "unknown backend $backend"; exit 2 ;;
esac

"$profiler" >"$runtime/profiler.log" 2>&1 &
pid=$!
sleep "$seconds"
if kill -0 "$pid" 2>/dev/null; then
  kill "$pid"; wait "$pid" 2>/dev/null
  echo "tracy-profiler ($backend) ran for ${seconds}s"
  cat "$runtime/profiler.log"
  exit 0
fi
wait "$pid"; rc=$?
echo "tracy-profiler ($backend) exited early with status $rc"
echo "--- profiler log"; cat "$runtime/profiler.log"
echo "--- server log"; cat "$runtime/server.log"
exit 1
