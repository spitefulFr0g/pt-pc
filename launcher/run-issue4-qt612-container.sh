#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
evidence_dir="${1:-$repo_dir/launcher/evidence/issue4-qt612}"
qt_root="${PT_QT612_ROOT:-/tmp/launcher-qt-6.12.0}"
mkdir -p "$evidence_dir"
if [[ ! -f "$qt_root/6.12.0/gcc_64/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  python3 -m venv /tmp/pt-launcher-qt-venv
  /tmp/pt-launcher-qt-venv/bin/pip install aqtinstall==3.3.0
  /tmp/pt-launcher-qt-venv/bin/aqt install-qt linux desktop 6.12.0 linux_gcc_64 -O "$qt_root"
fi
docker run --rm -v "$repo_dir:/repo:ro" -v "$qt_root:/opt/qt:ro" -v "$evidence_dir:/evidence" -w /repo ubuntu:24.04 sh -c '
  set -eu
  trap '\''chown -R "$(stat -c %u /repo):$(stat -c %g /repo)" /evidence'\'' EXIT
  apt-get update -qq
  DEBIAN_FRONTEND=noninteractive apt-get install -y -qq cmake g++ zlib1g-dev libgl1-mesa-dev libfontconfig1 libdbus-1-3 libglib2.0-0 libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xfixes0 libxcb-xinerama0 xvfb xauth xdotool openbox > /evidence/packages.log
  dpkg-query -W cmake g++-13 zlib1g-dev libgl1-mesa-dev libglib2.0-0t64 xvfb openbox xdotool libxcb-cursor0 > /evidence/dependencies.txt
  CMAKE_PREFIX_PATH=/opt/qt/6.12.0/gcc_64 QT_QPA_PLATFORM=offscreen /repo/launcher/run-acceptance.sh /evidence
' 2>&1 | tee "$evidence_dir/container.log"
python3 - "$evidence_dir" <<'PY'
from pathlib import Path
import sys
for name in ("packages.log", "configure.log", "build.log", "ctest.log", "container.log"):
    path = Path(sys.argv[1]) / name
    if path.exists():
        path.write_text("\n".join(line.rstrip() for line in path.read_text(errors="replace").splitlines()) + "\n")
PY
