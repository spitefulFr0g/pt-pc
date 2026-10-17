#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
work="${1:-$HOME/.cache/pt-issue13/acceptance}"
qt_root="${PT_QT612_ROOT:-/tmp/launcher-qt-6.12.0}"
build_tools="${PT_BUILD_TOOLS:-/home/watermelon/dev/pt-pc-wrapped/build/launcher-preview-local/build-tools}"
image="${PT_LAUNCHER_BUILD_IMAGE:-pt-launcher-preview-build-env-complete:latest}"
mkdir -p "$work/tmp"
work="$(cd "$work" && pwd)"
docker image inspect "$image" --format '{{.Id}}' > "$work/image-id.txt"
docker run --rm \
  -v "$repo:/repo:ro" -v "$work:/evidence" \
  -v "$qt_root:/opt/qt:ro" -v "$build_tools:/work/build-tools:ro" \
  -e TMPDIR=/evidence/tmp -w /repo "$image" bash -lc '
    set -euo pipefail
    export PATH=/work/build-tools/bin:$PATH QT_QPA_PLATFORM=offscreen
    { date -u; uname -a; g++-13 --version; cmake --version; } > /evidence/environment.txt
    cmake -S launcher -B /evidence/build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_PREFIX_PATH=/opt/qt/6.12.0/gcc_64 \
      2>&1 | tee /evidence/configure.log
    cmake --build /evidence/build --parallel 4 2>&1 | tee /evidence/build.log
    # Plain full CTest is intentional: display-dependent tests own their setup.
    ctest --test-dir /evidence/build --output-on-failure --timeout 90 \
      2>&1 | tee /evidence/ctest.log
    sha256sum /evidence/build/pt-launcher /evidence/build/pt_launcher_controller*_test \
      > /evidence/artifact-sha256.txt
  ' 2>&1 | tee "$work/controller-acceptance.log"
