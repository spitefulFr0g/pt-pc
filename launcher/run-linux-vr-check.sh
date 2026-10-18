#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
evidence_root="${1:-/home/watermelon/.cache/pt-issue12/evidence}"
qt_sdk="${PT_ISSUE12_QT_SDK:-/tmp/launcher-qt-6.12.0}"
cmake_tools="${PT_ISSUE12_CMAKE_TOOLS:-/home/watermelon/dev/pt-pc-wrapped/build/launcher-preview-local/build-tools}"
image="${PT_ISSUE12_BUILD_IMAGE:-pt-launcher-preview-build-env-complete:latest}"
run_id="${PT_ISSUE12_RUN_ID:-$(date -u +%Y%m%dT%H%M%SZ)-$$}"
run_dir="$evidence_root"
mkdir -p "$run_dir"
printf '%s\n' "$run_dir" > "$evidence_root/latest-run.txt"

if [[ ! -d "$qt_sdk/6.12.0/gcc_64" ]]; then
  printf 'Qt 6.12.0 SDK not found at %s\n' "$qt_sdk" >&2
  exit 2
fi
if [[ ! -x "$cmake_tools/bin/cmake" ]]; then
  printf 'CMake 3.31 toolchain not found at %s\n' "$cmake_tools" >&2
  exit 2
fi

docker run --rm --name "pt-issue12-$run_id" \
  --mount "type=bind,src=$repo_dir,dst=/repo,readonly" \
  --mount "type=bind,src=$run_dir,dst=/evidence" \
  --mount "type=bind,src=$qt_sdk,dst=/opt/qt,readonly" \
  --mount "type=bind,src=$cmake_tools,dst=/work/build-tools,readonly" \
  "$image" bash -lc '
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
export TMPDIR=/evidence/tmp
export PATH=/work/build-tools/bin:$PATH
mkdir -p "$TMPDIR"
apt-get update > /evidence/apt-update.log 2>&1
apt-get install -y --no-install-recommends libvulkan-dev glslc mesa-vulkan-drivers > /evidence/tool-install.log 2>&1
export VULKAN_SDK=/usr
export CMAKE_PREFIX_PATH=/opt/qt/6.12.0/gcc_64
export VK_ICD_FILENAMES="$(find /usr/share/vulkan/icd.d -maxdepth 1 -name "lvp_icd*.json" -print -quit)"
if [[ -z "$VK_ICD_FILENAMES" ]]; then echo "Mesa lavapipe ICD was not installed" >&2; exit 3; fi

{
  cat /etc/os-release
  cmake --version | head -1
  g++-13 --version | head -1
  dpkg-query -W -f="\${Package} \${Version}\n" libvulkan-dev glslc mesa-vulkan-drivers
  /opt/qt/6.12.0/gcc_64/bin/qmake -query QT_VERSION
  printf "Vulkan ICD: %s\n" "$VK_ICD_FILENAMES"
} > /evidence/environment.txt

cmake -S /repo -B /evidence/game-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 \
  -DPT_OPENXR=ON -DPT_UPSCALERS=OFF > /evidence/game-configure.log 2>&1
cmake --build /evidence/game-build --target pt pt_xr_test_runtime --parallel 2 > /evidence/game-build.log 2>&1
runtime_package=/evidence/runtime-stage
mkdir -p "$runtime_package"
cp /evidence/game-build/pt /evidence/game-build/libopenxr_loader.so.1 "$runtime_package/"
test -x "$runtime_package/pt"
test -s "$runtime_package/libopenxr_loader.so.1"
test -s /evidence/game-build/xr_test_runtime/pt_xr_test_runtime.json
test -s /evidence/game-build/xr_test_runtime/libpt_xr_test_runtime.so
readelf -d "$runtime_package/pt" | grep -E "RUNPATH|RPATH" > /evidence/game-runpath.txt
grep -F \$ORIGIN /evidence/game-runpath.txt
ldd "$runtime_package/libopenxr_loader.so.1" > /evidence/openxr-loader-dependencies.txt
sha256sum "$runtime_package/pt" "$runtime_package/libopenxr_loader.so.1" \
  /evidence/game-build/xr_test_runtime/libpt_xr_test_runtime.so \
  /evidence/game-build/xr_test_runtime/pt_xr_test_runtime.json > /evidence/SHA256SUMS

cmake -S /repo/launcher -B /evidence/launcher-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-13 \
  -DCMAKE_PREFIX_PATH=/opt/qt/6.12.0/gcc_64 \
  -DPT_LAUNCHER_VR_ACCEPTANCE_PACKAGE="$runtime_package" \
  -DPT_LAUNCHER_VR_ACCEPTANCE_MANIFEST=/evidence/game-build/xr_test_runtime/pt_xr_test_runtime.json \
  -DPT_LAUNCHER_VR_ACCEPTANCE_INCLUDE=/evidence/game-build/_deps/upscaler-sdks/openxr-1.1.63/package/include \
  > /evidence/launcher-configure.log 2>&1
cmake --build /evidence/launcher-build --parallel 2 > /evidence/launcher-build.log 2>&1
QT_QPA_PLATFORM=offscreen ctest --test-dir /evidence/launcher-build --output-on-failure --timeout 180 -R "^launcher_(inspection|readiness|session|preparation|preparation_ui|preparation_process|settings|settings_dialog|vr_loader_acceptance)$" \
  > /evidence/launcher-ctest.log 2>&1
cat /evidence/launcher-ctest.log
printf "PASS: controlled Linux OpenXR loader boundary\n"
' 2>&1 | tee "$run_dir/driver.log"

printf 'Evidence: %s\n' "$run_dir"
