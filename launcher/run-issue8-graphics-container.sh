#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
evidence_dir="${1:-/tmp/pt-launcher-issue8-graphics-evidence}"
mkdir -p "$evidence_dir"
"$repo_dir/launcher/run-issue7-settings-container.sh" "$evidence_dir"
printf 'Ticket #8 graphics/settings evidence: %s\n' "$evidence_dir"
