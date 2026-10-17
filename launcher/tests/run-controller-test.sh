#!/usr/bin/env bash
set -euo pipefail
# Build the native launcher with pinned Qt/SDL, then exercise real SDL virtual-device
# events through the QApplication loop. The supplied directory keeps binaries and logs.

repo="$(cd "$(dirname "$0")/../.." && pwd)"
work="${1:-/tmp/pt-controller-acceptance-13}"
mkdir -p "$work"
docker run --rm \
  -v "$repo:/repo:ro" \
  -v "$work:/evidence" \
  -v /tmp/launcher-qt-6.12.0:/opt/qt:ro \
  -w /repo \
  pt-launcher-preview-build-env-complete \
  bash -lc 'set -euo pipefail
    cmake -S launcher -B /evidence/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_PREFIX_PATH=/opt/qt/6.12.0/gcc_64
    cmake --build /evidence/build --parallel 4
    xvfb-run -a sh -c "openbox --sm-disable </dev/null >/dev/null 2>&1 & sleep 0.2; QT_QPA_PLATFORM=xcb QT_PLUGIN_PATH=/opt/qt/6.12.0/gcc_64/plugins ctest --test-dir /evidence/build --output-on-failure --timeout 90 -R launcher_controller"
  ' 2>&1 | tee "$work/controller-acceptance.log"
