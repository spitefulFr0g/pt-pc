#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
evidence_dir="${1:-/tmp/pt-launcher-issue7-settings-evidence}"
qt_root="${PT_QT612_ROOT:-/tmp/launcher-qt-6.12.0}"
mkdir -p "$evidence_dir"
chmod 0777 "$evidence_dir"
build_dir="$(mktemp -d "$evidence_dir/build.XXXXXX")"
chmod 0777 "$build_dir"
build_name="$(basename "$build_dir")"
if [[ ! -f "$qt_root/6.12.0/gcc_64/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  echo "Qt 6.12.0 SDK missing at $qt_root; prepare it using launcher/run-issue4-qt612-container.sh first." >&2
  exit 2
fi
docker run --rm \
  --user 65534:65534 \
  -v "$repo_dir:/repo:ro" \
  -v "$qt_root:/opt/qt:ro" \
  -v "$evidence_dir:/evidence" \
  -w /repo pt-launcher-preview-build-env-complete:latest \
  bash -c 'set -euo pipefail
    export PATH=/work/build-tools/bin:$PATH
    export CMAKE_PREFIX_PATH=/opt/qt/6.12.0/gcc_64
    export QT_QPA_PLATFORM=offscreen
    export XDG_CACHE_HOME=/evidence/xdg-cache
    mkdir -p "$XDG_CACHE_HOME"
    printf "image=%s\n" "$(cat /etc/os-release | sed -n "s/^PRETTY_NAME=//p" | tr -d \\\" )"
    cmake -S launcher -B "/evidence/'"$build_name"'" -DCMAKE_CXX_COMPILER=g++-13 -DPT_LAUNCHER_QT_MIN_VERSION=6.12.0 > /evidence/configure.log
    cmake --build "/evidence/'"$build_name"'" -j2 > /evidence/build.log 2>&1
    ctest --test-dir "/evidence/'"$build_name"'" --output-on-failure | tee /evidence/ctest.log'
docker image inspect pt-launcher-preview-build-env-complete:latest --format '{{.Id}}' > "$evidence_dir/container-image-id.txt"
sha256sum "$evidence_dir/configure.log" "$evidence_dir/build.log" "$evidence_dir/ctest.log" "$evidence_dir/container-image-id.txt" > "$evidence_dir/SHA256SUMS"
echo "Ticket #7 evidence: $evidence_dir"
