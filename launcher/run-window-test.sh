#!/usr/bin/env bash
set -euo pipefail
xvfb-run -a sh -c 'openbox --sm-disable >/dev/null 2>&1 & sleep 0.2; QT_QPA_PLATFORM=xcb QT_PLUGIN_PATH=/opt/qt/6.12.0/gcc_64/plugins XDG_SESSION_TYPE=x11 "$1" "$2"' sh "$1" "$2"
