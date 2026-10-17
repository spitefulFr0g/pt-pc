#!/usr/bin/env bash
set -euo pipefail
exec "$(dirname "$0")/tests/run-qt-x11.sh" "$@"
