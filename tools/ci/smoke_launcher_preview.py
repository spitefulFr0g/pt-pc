#!/usr/bin/env python3
"""Check an extracted portable preview and launch its real Qt event loop."""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path

FORBIDDEN = {"chunk1.psarc", "texture.qar", "pathid_list_ps4.bin", "pt-updater", "pt-updater.exe"}
FORBIDDEN_SUFFIXES = {".pkg", ".psarc", ".qar", ".self", ".pdb", ".ilk", ".pem", ".p12", ".pfx", ".key"}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def extract(archive, destination, platform):
    if platform == "windows":
        with zipfile.ZipFile(archive) as source:
            names = source.namelist()
            require(all(not name.startswith("/") and ".." not in Path(name).parts for name in names), "unsafe ZIP path")
            source.extractall(destination)
    else:
        with tarfile.open(archive, "r:gz") as source:
            members = source.getmembers()
            require(all(not member.name.startswith("/") and ".." not in Path(member.name).parts
                        and (member.isfile() or member.isdir()) for member in members), "unsafe tar member")
            source.extractall(destination, filter="data")
    roots = [p for p in destination.iterdir() if p.is_dir()]
    require(len(roots) == 1, "archive must contain one bundle root")
    return roots[0]


def check(root, platform):
    inventory_path = root / "preview-inventory.json"
    require(inventory_path.is_file(), "preview-inventory.json missing")
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    require(inventory.get("format") == 1 and inventory.get("kind") == "unsigned-portable-preview"
            and inventory.get("platform") == platform, "wrong preview inventory format/platform")
    expected = {}
    for item in inventory["files"]:
        relative = Path(item["path"])
        require(not relative.is_absolute() and ".." not in relative.parts, "unsafe inventory path")
        require(relative.as_posix() not in expected, f"duplicate inventory path: {relative}")
        expected[relative.as_posix()] = item
    actual = {p.relative_to(root).as_posix(): p for p in root.rglob("*") if p.is_file() and p != inventory_path}
    require(set(actual) == set(expected), f"inventory file set differs: missing={set(expected)-set(actual)}, extra={set(actual)-set(expected)}")
    for name, path in actual.items():
        item = expected[name]
        require(path.stat().st_size == item["size"] and digest(path) == item["sha256"], f"hash/size mismatch: {name}")
        require(path.name.lower() not in FORBIDDEN and path.suffix.lower() not in FORBIDDEN_SUFFIXES
                and not any(part.lower() == "cusa01127" for part in Path(name).parts),
                f"proprietary/unsupported file: {name}")
        require(not any(part.lower() in ("tests", "test", "evidence", "keys", ".git") for part in Path(name).parts),
                f"development file: {name}")
    runtime = root / platform / ("pt.exe" if platform == "windows" else "pt")
    launcher = root / ("pt-launcher.exe" if platform == "windows" else "libexec/pt-launcher")
    asset = root / ("assets" if platform == "windows" else "libexec/assets")
    require(runtime.is_file() and launcher.is_file(), "runtime or launcher missing")
    require(digest(runtime) == inventory["runtime_source_sha256"] == inventory["supported_runtime_sha256"],
            "runtime hash differs from source/supported hash")
    origin = json.loads((root / platform / "runtime-origin.json").read_text(encoding="utf-8"))
    require(origin == inventory["runtime_origin"] and origin["executable_sha256"] == digest(runtime),
            "upstream runtime origin differs from the inventory or executable")
    configured = dict(line.split("=", 1) for line in (root / "supported-runtime-sha256.txt").read_text(encoding="ascii").splitlines()
                      if "=" in line)
    require(configured.get(platform) == inventory["supported_runtime_sha256"],
            "installed launcher platform hash differs from inventory")
    require(inventory["supported_runtime_sha256"].encode("ascii") in launcher.read_bytes(),
            "launcher does not embed this runtime hash")
    require((asset / "spooky-forest-LICENSE.md").is_file() and (asset / "PirataOne-OFL.txt").is_file(),
            "launcher asset notices missing")
    require((root / "PREVIEW-NOTICES.md").is_file() and (root / "README-preview.md").is_file()
            and (root / "LICENSE").is_file(), "preview instructions or project license missing")
    for name in ("LGPL-3.0-only.txt", "GPL-3.0-only.txt", "Qt-GPL-exception-1.0.txt"):
        require((root / "notices/qt" / name).is_file(), f"Qt license text missing: {name}")
    require((asset / "spooky-forest.jpg").is_file() and (asset / "PirataOne-Regular.ttf").is_file(),
            "launcher assets missing")
    require((root / platform / "licenses").is_dir(), "runtime license directory missing")
    if platform == "linux":
        require(os.access(runtime, os.X_OK), "Linux game runtime is not executable")
        for name in ("libqxcb.so", "libqwayland.so"):
            require((root / "libexec/plugins/platforms" / name).is_file(), f"missing Qt platform plugin {name}")
        require((root / "libexec/qt.conf").is_file() and (root / "lib").is_dir(), "private Qt configuration missing")
        require((root / "pt-launcher").is_file(), "Linux entry script missing")
    else:
        require((root / "platforms/qwindows.dll").is_file() and (root / "Qt6Core.dll").is_file(),
                "Windows Qt deployment missing")
        require((root / "vcruntime140.dll").is_file() and (root / "msvcp140.dll").is_file(),
                "Windows app-local compiler runtime missing")
    return root / ("pt-launcher.exe" if platform == "windows" else "pt-launcher")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--archive", type=Path)
    source.add_argument("--artifacts", type=Path, help="directory with exactly one preview archive for this platform")
    parser.add_argument("--platform", choices=("linux", "windows"), required=True)
    parser.add_argument("--evidence", type=Path, help="directory for extracted-launcher screenshots")
    parser.add_argument("--screenshot", type=Path, help="save screenshot outside the archive")
    args = parser.parse_args()
    if args.artifacts:
        suffix = ".zip" if args.platform == "windows" else ".tar.gz"
        matches = list(args.artifacts.glob(f"pt-launcher-preview-*-{args.platform}{suffix}"))
        require(len(matches) == 1, f"expected one {args.platform} archive in {args.artifacts}, found {len(matches)}")
        archive = matches[0].resolve()
    else:
        archive = args.archive.resolve()
    require(archive.is_file(), f"archive missing: {archive}")
    sidecar = archive.with_name(archive.name + ".sha256")
    require(sidecar.is_file(), f"archive SHA-256 sidecar missing: {sidecar}")
    match = re.fullmatch(r"([0-9a-f]{64})  (.+)\n?", sidecar.read_text(encoding="ascii"))
    require(match and match.group(2) == archive.name and match.group(1) == digest(archive), "archive SHA-256 mismatch")
    with tempfile.TemporaryDirectory(prefix="pt preview smoke ") as temporary:
        work = Path(temporary)
        root = extract(archive, work / "extracted with spaces", args.platform)
        launcher = check(root, args.platform)
        external_inventory = archive.with_name(archive.name.removesuffix(".tar.gz").removesuffix(".zip") + "-inventory.json")
        require(external_inventory.is_file() and external_inventory.read_bytes() == (root / "preview-inventory.json").read_bytes(),
                "external inventory missing or differs from bundle")
        shot = args.screenshot.resolve() if args.screenshot else (args.evidence.resolve() / "launcher.png" if args.evidence else work / "launcher screenshot.png")
        require(not shot.is_relative_to(root), "screenshot output must be outside the bundle")
        shot.parent.mkdir(parents=True, exist_ok=True)
        environment = os.environ.copy()
        for key in list(environment):
            if key.startswith("QT_") or key in ("QTDIR", "CMAKE_PREFIX_PATH", "QT_ROOT", "LD_LIBRARY_PATH", "LD_PRELOAD", "PYTHONPATH"):
                environment.pop(key)
        if args.platform == "linux":
            environment["PATH"] = "/usr/bin:/bin"
        else:
            system_root = environment.get("SystemRoot") or environment.get("WINDIR")
            require(system_root, "SystemRoot missing for clean Windows PATH")
            environment["PATH"] = os.pathsep.join((str(Path(system_root) / "System32"), system_root))
        if args.platform == "linux":
            require(shutil.which("xvfb-run"), "xvfb-run is required for xcb smoke")
            command = ["xvfb-run", "-a", "-s", "-screen 0 1280x800x24", str(launcher), "--screenshot", str(shot)]
        else:
            command = [str(launcher), "--screenshot", str(shot)]
        subprocess.run(command, cwd=root, env=environment, check=True, timeout=60)
        require(shot.is_file() and shot.stat().st_size > 1000, "launcher did not save a screenshot")
        narrow = Path(str(shot) + ".narrow.png")
        require(narrow.is_file() and narrow.stat().st_size > 1000, "launcher did not save narrow screenshot")
        print(f"preview smoke passed: {archive}")
        if args.screenshot:
            print(f"screenshots: {shot}, {narrow}")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, TypeError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        sys.exit(str(error))
