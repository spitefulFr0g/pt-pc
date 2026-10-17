#!/usr/bin/env bash
set -euo pipefail
app="$1"
fixture="$(mktemp -d "${TMPDIR:-/tmp}/pt-preparation-ui-XXXXXX")"
trap 'status=$?; if [[ $status -ne 0 ]]; then cat "$fixture/ui.log" 2>/dev/null || true; fi; rm -rf "$fixture"; exit $status' EXIT
bundle="$fixture/portable bundle"
source="$fixture/dump folder 日本語"
mkdir -p "$bundle/linux" "$source"
printf 'synthetic paired runtime' > "$bundle/linux/pt"
python3 - "$source/chunk1.psarc" <<'PY'
from pathlib import Path
import sys
with Path(sys.argv[1]).open('wb') as f:
    f.write(b'PSAR')
    f.truncate(32 * 1024 * 1024)
PY
python3 - "$source/texture.qar" "$source/pathid_list_ps4.bin" <<'PY'
from pathlib import Path
import sys
qar = bytearray(0x24)
qar[0x16:0x18] = b'aq'
Path(sys.argv[1]).write_bytes(qar)
Path(sys.argv[2]).write_bytes(b'pt14_ synthetic path list fixture data ............')
PY
hash="$(sha256sum "$bundle/linux/pt" | cut -d' ' -f1)"
QT_QPA_PLATFORM=xcb PT_LAUNCHER_TEST_SOURCE="$source" \
  xvfb-run -a "$app" --test-preparation-prompt "$bundle" --test-supported-sha256 "$hash" \
  > "${fixture}/ui.log" 2>&1
cat "${fixture}/ui.log"
