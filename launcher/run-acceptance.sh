#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
evidence_dir="${1:-$repo_dir/launcher/evidence}"
mkdir -p "$evidence_dir"
: "${QT_QPA_PLATFORM:=offscreen}"
export QT_QPA_PLATFORM
cmake -S "$repo_dir/launcher" -B "$evidence_dir/build" -DPT_LAUNCHER_QT_MIN_VERSION=6.12.0 2>&1 | tee "$evidence_dir/configure.log"
cmake --build "$evidence_dir/build" -j2 2>&1 | tee "$evidence_dir/build.log"
ctest --test-dir "$evidence_dir/build" --output-on-failure 2>&1 | tee "$evidence_dir/ctest.log"
