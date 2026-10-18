#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
evidence_dir="${1:-$repo_dir/launcher/evidence}"
mkdir -p "$evidence_dir"
: "${QT_QPA_PLATFORM:=offscreen}"
export QT_QPA_PLATFORM
python3 -m unittest discover -s "$repo_dir/tools/ci" -p 'test_launcher_release.py' -v 2>&1 | tee "$evidence_dir/release-contract.log"
cmake -S "$repo_dir/launcher" -B "$evidence_dir/build" -DPT_LAUNCHER_QT_MIN_VERSION=6.12.0 2>&1 | tee "$evidence_dir/configure.log"
cmake --build "$evidence_dir/build" -j2 2>&1 | tee "$evidence_dir/build.log"
# Retained desktop/player-data boundaries; controller, window-focus and Gaming Mode
# supervisor suites remain available via unfiltered CTest for optional development.
ctest --test-dir "$evidence_dir/build" --output-on-failure -R '^launcher_(inspection|readiness|session|preparation|preparation_ui|preparation_process|settings|settings_dialog)$' 2>&1 | tee "$evidence_dir/ctest.log"
