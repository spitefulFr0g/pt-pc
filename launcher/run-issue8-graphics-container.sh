#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
evidence_dir="${1:-/tmp/pt-launcher-issue8-graphics-evidence}"
mkdir -p "$evidence_dir"
set +e
"$repo_dir/launcher/run-issue7-settings-container.sh" "$evidence_dir" 2>&1 | tee "$evidence_dir/issue8-harness.log"
status=${PIPESTATUS[0]}
set -e
docker image inspect pt-launcher-preview-build-env-complete:latest --format '{{.Id}}' > "$evidence_dir/container-image-id.txt"
git -C "$repo_dir" rev-parse HEAD > "$evidence_dir/source-commit.txt"
printf 'exit_status=%s\n' "$status" > "$evidence_dir/issue8-result.txt"
find "$evidence_dir" -maxdepth 1 -type f ! -name SHA256SUMS -print0 |
  sort -z | xargs -0 sha256sum > "$evidence_dir/SHA256SUMS"
printf 'Ticket #8 graphics/settings evidence: %s\n' "$evidence_dir"
exit "$status"
