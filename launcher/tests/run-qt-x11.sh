#!/usr/bin/env bash
set -euo pipefail
# CTest must be usable under an inherited offscreen GUI environment too.
# Each test gets its own server/WM; do not reuse a caller's display or focus.
xvfb-run -a bash -c '
  set -euo pipefail
  openbox --sm-disable </dev/null >/dev/null 2>&1 &
  wm=$!
  trap '\''kill "$wm" 2>/dev/null || true; wait "$wm" 2>/dev/null || true'\'' EXIT
  for attempt in {1..50}; do
    if xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q "window id"; then break; fi
    sleep 0.02
  done
  export QT_QPA_PLATFORM=xcb XDG_SESSION_TYPE=x11
  "$@"
' bash "$@"
