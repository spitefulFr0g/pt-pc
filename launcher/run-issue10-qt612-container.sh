#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
evidence_dir="${1:-/tmp/pt-launcher-issue10-evidence}"
qt_root="${PT_QT612_ROOT:-/tmp/launcher-qt-6.12.0}"
build_tools="${PT_BUILD_TOOLS:-/home/watermelon/dev/pt-pc-wrapped/build/launcher-preview-local/build-tools}"
image="${PT_LAUNCHER_BUILD_IMAGE:-pt-launcher-preview-build-env-complete:latest}"
mkdir -p "$evidence_dir"
test -f "$qt_root/6.12.0/gcc_64/lib/cmake/Qt6/Qt6Config.cmake"
test -x "$build_tools/bin/cmake"
docker run --rm \
  -v "$repo_dir:/repo:ro" \
  -v "$qt_root:/opt/qt:ro" \
  -v "$build_tools:/work/build-tools:ro" \
  -v "$evidence_dir:/evidence" \
  -w /repo "$image" bash -lc '
    set -euo pipefail
    export PATH=/work/build-tools/bin:$PATH
    export QT_QPA_PLATFORM=offscreen
    /work/build-tools/bin/cmake -S launcher -B /evidence/build \
      -DCMAKE_CXX_COMPILER=g++-13 \
      -DCMAKE_PREFIX_PATH=/opt/qt/6.12.0/gcc_64 \
      -DPT_LAUNCHER_QT_MIN_VERSION=6.12.0 \
      2>&1 | tee /evidence/configure.log
    /work/build-tools/bin/cmake --build /evidence/build -j2 \
      2>&1 | tee /evidence/build.log
    /work/build-tools/bin/ctest --test-dir /evidence/build --output-on-failure \
      2>&1 | tee /evidence/ctest.log
  ' 2>&1 | tee "$evidence_dir/container.log"
printf 'Checkout: %s\nImage: %s\nCompiler: g++-13\nQt: 6.12.0\nCommand: launcher/run-issue10-qt612-container.sh %s\n' \
  "$repo_dir" "$image" "$evidence_dir" > "$evidence_dir/reproduction.txt"
