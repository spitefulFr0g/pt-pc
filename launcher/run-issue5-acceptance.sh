#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
evidence_dir="${1:-/tmp/pt-launcher-issue5-evidence}"
qt_root="${PT_QT612_ROOT:-/tmp/launcher-qt-6.12.0}"
image="${PT_LAUNCHER_BUILD_IMAGE:-pt-launcher-preview-build-env-complete:latest}"
mkdir -p "$evidence_dir"
if [[ ! -f "$qt_root/6.12.0/gcc_64/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  python3 -m venv /tmp/pt-launcher-qt-venv
  /tmp/pt-launcher-qt-venv/bin/pip install aqtinstall==3.3.0
  /tmp/pt-launcher-qt-venv/bin/aqt install-qt linux desktop 6.12.0 linux_gcc_64 -O "$qt_root"
fi
docker run --rm \
  -v "$repo_dir:/repo:ro" \
  -v "$qt_root:/opt/qt:ro" \
  -v "$evidence_dir:/evidence" \
  -w /repo "$image" sh -lc '
    set -eu
    { date -u; g++-13 --version; cmake --version; } > /evidence/environment.txt
    CMAKE_PREFIX_PATH=/opt/qt/6.12.0/gcc_64 \
      cmake -S launcher -B /evidence/build \
        -DPT_LAUNCHER_QT_MIN_VERSION=6.12.0 -DCMAKE_CXX_COMPILER=g++-13 \
        > /evidence/configure.log 2>&1
    cmake --build /evidence/build -j2 > /evidence/build.log 2>&1
    QT_QPA_PLATFORM=offscreen ctest --test-dir /evidence/build --output-on-failure \
      > /evidence/ctest.log 2>&1
    cat /evidence/ctest.log
  ' 2>&1 | tee "$evidence_dir/container.log"
