#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
evidence_dir="${1:-/tmp/pt-launcher-issue6-evidence}"
sdk_root="${PT_ISSUE6_DOTNET_ROOT:-/tmp/pt-issue6-sdk}"
source_dir="${PT_ISSUE6_LIBORBIS_SOURCE:-/tmp/pt-issue6-src/LibOrbisPkg}"
revision="643477263b2644e0803e0f58b8726ea4e3f3b7d4"
mkdir -p "$evidence_dir"
rm -rf "$evidence_dir/build" "$evidence_dir/staged" "$evidence_dir/synthetic"
if [[ ! -x "$sdk_root/dotnet" ]]; then
  mkdir -p "$sdk_root"
  curl -fsSL https://dot.net/v1/dotnet-install.sh -o "$evidence_dir/dotnet-install.sh"
  bash "$evidence_dir/dotnet-install.sh" --version 10.0.402 --install-dir "$sdk_root" > "$evidence_dir/dotnet-install.log"
fi
if [[ ! -d "$source_dir/.git" ]]; then
  mkdir -p "$(dirname "$source_dir")"
  git clone https://github.com/maxton/LibOrbisPkg.git "$source_dir"
fi
git -C "$source_dir" checkout --detach "$revision" > "$evidence_dir/liborbis-checkout.log"
test "$(git -C "$source_dir" rev-parse HEAD)" = "$revision"
export PATH="$sdk_root:$PATH"
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1 DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
mkdir -p "$evidence_dir/build" "$evidence_dir/staged/linux" "$evidence_dir/staged/windows"
python3 "$repo_dir/tools/ci/publish_extractor.py" --liborbis "$source_dir" --rid linux-x64 \
  --out "$evidence_dir/build/linux" --notices "$evidence_dir/build/linux-notices" > "$evidence_dir/publish-linux.log" 2>&1
python3 "$repo_dir/tools/ci/publish_extractor.py" --liborbis "$source_dir" --rid win-x64 \
  --out "$evidence_dir/build/windows" --notices "$evidence_dir/build/windows-notices" > "$evidence_dir/publish-windows.log" 2>&1
cp -a "$evidence_dir/build/linux/." "$evidence_dir/staged/linux/"
cp -a "$evidence_dir/build/windows/." "$evidence_dir/staged/windows/"
cat > "$evidence_dir/environment.txt" <<EOF
UTC: $(date -u +%FT%TZ)
Linux: $(uname -srvmo)
.NET SDK: $(dotnet --version)
LibOrbisPkg repository: https://github.com/maxton/LibOrbisPkg.git
LibOrbisPkg revision: $revision
Windows: $(/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -NonInteractive -Command '[Environment]::OSVersion.VersionString' | tr -d '\r')
EOF
fixture_root="$evidence_dir/synthetic"
rm -rf "$fixture_root"
test_source="$evidence_dir/liborbis-test"
rm -rf "$test_source"
cp -a "$source_dir" "$test_source"
python3 - "$test_source/LibOrbisPkg.Core/LibOrbisPkg.Core.csproj" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text(encoding="utf-8-sig").replace("netcoreapp3.0", "net10.0").replace(
    "<LangVersion>7.3</LangVersion>", "<LangVersion>latest</LangVersion>").replace(
    "<GenerateSerializationAssemblies>Auto</GenerateSerializationAssemblies>",
    "<GenerateSerializationAssemblies>Off</GenerateSerializationAssemblies>")
p.write_text(s, encoding="utf-8")
PY
python3 "$repo_dir/tools/patch_liborbis_readers.py" "$test_source" > "$evidence_dir/patch-test-source.log"
dotnet run --project "$repo_dir/installer/Tests/Tests.csproj" -c Release \
  -p:LibOrbisSource="$test_source" -- "$fixture_root" > "$evidence_dir/synthetic-generator.log" 2>&1
mkdir -p "$evidence_dir/staged/linux/extractor" "$evidence_dir/staged/windows/extractor"
cp -a "$evidence_dir/build/linux/." "$evidence_dir/staged/linux/extractor/"
cp -a "$evidence_dir/build/windows/." "$evidence_dir/staged/windows/extractor/"
cp "$evidence_dir/build/linux-notices/"* "$evidence_dir/staged/linux/extractor/"
cp "$evidence_dir/build/windows-notices/"* "$evidence_dir/staged/windows/extractor/"
python3 - "$evidence_dir" <<'PY'
import pathlib, shutil, sys
root = pathlib.Path(sys.argv[1])
fixtures = root / "synthetic"
for platform in ("linux", "windows"):
    for name in ("fixture.pkg", "Europe.pkg", "Japan.pkg", "moved.pkg"):
        src = fixtures / name
        if not src.is_file():
            raise SystemExit(f"synthetic fixture missing: {src}")
        shutil.copy2(src, root / "staged" / platform / name)
PY
for platform in linux windows; do
  stage="$evidence_dir/staged/$platform"
  if [[ "$platform" == linux ]]; then helper="$stage/extractor/PT.PkgExtract"; else helper="$stage/extractor/PT.PkgExtract.exe"; fi
  for fixture in fixture Europe Japan moved; do
    out="$stage/output-$fixture"
    if [[ "$platform" == linux ]]; then
      "$helper" "$stage/$fixture.pkg" "$out" > "$evidence_dir/$platform-$fixture.log" 2>&1
    else
      pkg_win="$(wslpath -w "$stage/$fixture.pkg")"; out_win="$(wslpath -w "$out")"; helper_win="$(wslpath -w "$helper")"
      /mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -NonInteractive \
        -Command "& '$helper_win' '$pkg_win' '$out_win'" > "$evidence_dir/$platform-$fixture.log" 2>&1
    fi
    python3 - "$fixture_root" "$out" "$fixture" <<'PY'
import hashlib, pathlib, sys
fixtures, out = map(pathlib.Path, sys.argv[1:3])
name = sys.argv[3]
for file in ("chunk1.psarc", "texture.qar", "pathid_list_ps4.bin"):
    a, b = fixtures / "extracted" / file, out / file
    if hashlib.sha256(a.read_bytes()).digest() != hashlib.sha256(b.read_bytes()).digest():
        raise SystemExit(f"{name}: extracted bytes differ for {file}")
text = (out / "source.txt").read_text(errors="replace")
if name == "Europe" and "region=Europe" not in text:
    raise SystemExit("European release identity was not reported")
if name == "Japan" and "region=Japan" not in text:
    raise SystemExit("Japanese release identity was not reported")
print(f"PASS {name}: extracted archive bytes match synthetic source")
PY
  done
done
for platform in linux windows; do
  stage="$evidence_dir/staged/$platform"
  if [[ "$platform" == linux ]]; then helper="$stage/extractor/PT.PkgExtract"; else helper="$stage/extractor/PT.PkgExtract.exe"; fi
  for case_name in invalid other retail; do
    out="$stage/refusal-$case_name"
    if [[ "$platform" == linux ]]; then
      if "$helper" "$fixture_root/$case_name.pkg" "$out" > "$evidence_dir/$platform-$case_name.log" 2>&1; then
        echo "$platform helper unexpectedly accepted $case_name fixture" >&2; exit 1
      fi
    else
      pkg_win="$(wslpath -w "$fixture_root/$case_name.pkg")"; out_win="$(wslpath -w "$out")"; helper_win="$(wslpath -w "$helper")"
      if /mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -NonInteractive \
        -Command "& '$helper_win' '$pkg_win' '$out_win'; exit \$LASTEXITCODE" > "$evidence_dir/$platform-$case_name.log" 2>&1; then
        echo "$platform helper unexpectedly accepted $case_name fixture" >&2; exit 1
      fi
    fi
    test ! -e "$out"
    case "$case_name" in
      invalid) expected="This is not a PS4 PKG." ;;
      other) expected="This package does not hold P.T.'s game data" ;;
      retail) expected="This PKG is a retail" ;;
    esac
    rg -Fq "$expected" "$evidence_dir/$platform-$case_name.log"
  done
done
launcher_evidence="$evidence_dir/launcher-$(date +%s)"
mkdir -p "$launcher_evidence"
cp -a "$evidence_dir/staged/linux/extractor" "$launcher_evidence/staged-extractor"
cp "$fixture_root/fixture.pkg" "$launcher_evidence/synthetic-fixture.pkg"
export PT_LAUNCHER_STAGED_EXTRACTOR="$launcher_evidence/staged-extractor"
export PT_LAUNCHER_TEST_PKG="$launcher_evidence/synthetic-fixture.pkg"
"$repo_dir/launcher/run-issue5-acceptance.sh" "$launcher_evidence" > "$evidence_dir/launcher.log" 2>&1
sha256sum "$evidence_dir/staged/linux/extractor/PT.PkgExtract" \
  "$evidence_dir/staged/windows/extractor/PT.PkgExtract.exe" > "$evidence_dir/helper-sha256.txt"
echo "Issue #6 helper and public preparation checks passed. Evidence: $evidence_dir"
