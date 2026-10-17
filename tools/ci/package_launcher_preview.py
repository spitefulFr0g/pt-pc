#!/usr/bin/env python3
"""Assemble an unsigned, paired launcher/game preview from finished builds."""
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

REPO = Path(__file__).resolve().parents[2]
EXCLUDED_LINUX = re.compile(r"^(lib(c|m|pthread|dl|rt|resolv|util|nss_.*|anl)\.so|ld-linux|linux-vdso)")
FORBIDDEN = {"chunk1.psarc", "texture.qar", "pathid_list_ps4.bin", "pt-updater", "pt-updater.exe"}
FORBIDDEN_SUFFIXES = {".pkg", ".psarc", ".qar", ".self", ".pdb", ".ilk", ".pem", ".p12", ".pfx", ".key"}
QT_LICENSES = ("LGPL-3.0-only.txt", "GPL-3.0-only.txt", "Qt-GPL-exception-1.0.txt")


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def cmake_value(build, key):
    cache = build / "CMakeCache.txt"
    require(cache.is_file(), f"missing {cache}")
    for line in cache.read_text(errors="replace").splitlines():
        if line.startswith(key + ":"):
            return line.split("=", 1)[1]
    return ""


def qt_root(build):
    config = cmake_value(build, "Qt6_DIR")
    require(config, "launcher build has no Qt6_DIR in CMakeCache.txt")
    root = Path(config).resolve().parents[2]
    require((root / "plugins" / "platforms").is_dir(), f"Qt plugins missing under {root}")
    return root


def qt_version(root):
    config = root / "lib/cmake/Qt6/Qt6ConfigVersionImpl.cmake"
    if not config.is_file():
        config = root / "lib/cmake/Qt6/Qt6ConfigVersion.cmake"
    match = re.search(r'set\(PACKAGE_VERSION "([0-9.]+)"\)', config.read_text(errors="replace"))
    require(match, f"cannot determine Qt version from {config}")
    return match.group(1)


def linux_deploy(root, qt):
    require(shutil.which("patchelf"), "patchelf is required to deploy private Linux Qt libraries")
    launcher = root / "libexec/pt-launcher"
    require(launcher.is_file(), f"missing installed launcher {launcher}")
    plugins = root / "libexec/plugins"
    for category in ("platforms", "imageformats", "platforminputcontexts", "xcbglintegrations", "wayland-decoration-client", "wayland-graphics-integration-client", "wayland-shell-integration"):
        source = qt / "plugins" / category
        if source.is_dir():
            selected = list(source.glob("*.so"))
            if category == "platforms":
                selected = [p for p in selected if p.name in ("libqxcb.so", "libqwayland.so")]
            if selected:
                target = plugins / category
                target.mkdir(parents=True, exist_ok=True)
                for item in selected:
                    shutil.copy2(item, target / item.name)
    for name in ("libqxcb.so", "libqwayland.so"):
        require((plugins / "platforms" / name).is_file(), f"Qt platform plugin missing: {name}")
    (root / "libexec/qt.conf").write_text("[Paths]\nPlugins=plugins\n", encoding="utf-8")
    libdir = root / "lib"
    libdir.mkdir(exist_ok=True)
    distro_packages = set()
    queue = [launcher, *plugins.rglob("*.so")]
    seen = set()
    environment = os.environ.copy()
    environment["LD_LIBRARY_PATH"] = str(libdir) + ":" + str(qt / "lib")
    while queue:
        binary = queue.pop()
        if binary in seen:
            continue
        seen.add(binary)
        output = subprocess.run(["ldd", str(binary)], env=environment, text=True, capture_output=True, check=True).stdout
        require("not found" not in output, f"unresolved Linux dependency for {binary}: {output}")
        for line in output.splitlines():
            match = re.match(r"\s*(\S+)\s+=>\s+(/\S+)", line)
            if not match:
                continue
            soname, source = match.groups()
            if EXCLUDED_LINUX.match(soname) or soname.startswith(("libGL", "libEGL", "libvulkan", "libdrm", "libgbm")):
                continue
            target = libdir / soname
            if not target.exists():
                shutil.copy2(Path(source).resolve(), target)
                queue.append(target)
                if not Path(source).resolve().is_relative_to(qt):
                    package = None
                    for candidate in (source, str(Path(source).resolve()), str(Path(source).resolve()).replace("/usr/lib/", "/lib/", 1)):
                        query = subprocess.run(["dpkg-query", "-S", candidate], text=True, capture_output=True)
                        if query.returncode == 0:
                            package = query.stdout.split(": ", 1)[0].split(",", 1)[0].split(":", 1)[0]
                            break
                    require(package, f"no distribution license record for {source}")
                    distro_packages.add(package)
    subprocess.run(["patchelf", "--set-rpath", "$ORIGIN/../lib", str(launcher)], check=True)
    for plugin in plugins.rglob("*.so"):
        subprocess.run(["patchelf", "--set-rpath", "$ORIGIN/../../../lib", str(plugin)], check=True)
    for library in libdir.iterdir():
        subprocess.run(["patchelf", "--set-rpath", "$ORIGIN", str(library)], check=True)
    notices = root / "notices/linux-dependencies"
    notices.mkdir(parents=True)
    for package in sorted(distro_packages):
        copyright_file = Path("/usr/share/doc") / package / "copyright"
        require(copyright_file.is_file(), f"missing copyright notice for {package}")
        shutil.copy2(copyright_file, notices / f"{package}.copyright")
    entry = root / "pt-launcher"
    entry.write_text('#!/bin/sh\nset -eu\nROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$ROOT/libexec/pt-launcher" "$@"\n', encoding="utf-8")
    entry.chmod(0o755)


def windows_deploy(root, qt):
    launcher = root / "pt-launcher.exe"
    require(launcher.is_file(), f"missing installed launcher {launcher}")
    deploy = qt / "bin/windeployqt.exe"
    require(deploy.is_file(), f"missing {deploy}")
    subprocess.run([str(deploy), "--no-translations", "--compiler-runtime", "--dir", str(root), str(launcher)], check=True)
    redist = Path(os.environ["VCToolsRedistDir"]) if os.environ.get("VCToolsRedistDir") else None
    candidates = [redist / "x64/Microsoft.VC143.CRT"] if redist else []
    if os.environ.get("VCINSTALLDIR"):
        candidates += sorted((Path(os.environ["VCINSTALLDIR"]) / "Redist/MSVC").glob("*/x64/Microsoft.VC143.CRT"), reverse=True)
    candidates = [folder for folder in candidates if folder.is_dir()]
    require(candidates, "MSVC x64 app-local CRT not found in VCToolsRedistDir or VCINSTALLDIR")
    for library in candidates[0].glob("*.dll"):
        shutil.copy2(library, root / library.name)
    require((root / "vcruntime140.dll").is_file() and (root / "msvcp140.dll").is_file(),
            "MSVC app-local CRT is incomplete")
    require((root / "platforms/qwindows.dll").is_file(), "windeployqt omitted qwindows.dll")
    require(any(root.glob("Qt6Core.dll")), "windeployqt omitted Qt6Core.dll")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("linux", "windows"), required=True)
    parser.add_argument("--runtime-build", type=Path, required=True)
    launcher = parser.add_mutually_exclusive_group(required=True)
    launcher.add_argument("--launcher-stage", type=Path)
    launcher.add_argument("--launcher-build", type=Path)
    parser.add_argument("--qt-root", type=Path, help="Qt SDK prefix; defaults to QT_ROOT or CMAKE_PREFIX_PATH")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--revision", required=True)
    args = parser.parse_args()
    require(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", args.version), "invalid version")
    require(re.fullmatch(r"[0-9a-fA-F]{7,40}", args.revision), "revision must be a git hash")
    runtime_build = args.runtime_build.resolve()
    launcher_build = args.launcher_build.resolve() if args.launcher_build else None
    stage = args.launcher_stage.resolve() if args.launcher_stage else None
    environment_qt = os.environ.get("QT_ROOT") or os.environ.get("CMAKE_PREFIX_PATH", "").split(os.pathsep)[0]
    require(launcher_build or args.qt_root or environment_qt,
            "--launcher-stage requires --qt-root, QT_ROOT, or CMAKE_PREFIX_PATH")
    qt = args.qt_root.resolve() if args.qt_root else qt_root(launcher_build) if launcher_build else Path(environment_qt).resolve()
    executable = runtime_build / ("pt_release.exe" if args.platform == "windows" else "pt_release")
    require(executable.is_file(), f"missing release runtime {executable}")
    runtime_sha = digest(executable)
    name = f"pt-launcher-preview-{args.version}-{args.platform}"
    suffix = ".zip" if args.platform == "windows" else ".tar.gz"
    args.out.mkdir(parents=True, exist_ok=True)
    archive = args.out / (name + suffix)
    require(not archive.exists(), f"output exists: {archive}")
    with tempfile.TemporaryDirectory(prefix="pt-preview-") as scratch:
        scratch = Path(scratch)
        if launcher_build:
            stage = scratch / "stage"
            subprocess.run(["cmake", "--install", str(launcher_build), "--prefix", str(stage)], check=True)
        root = scratch / name
        shutil.copytree(stage, root)
        hashes = root / "supported-runtime-sha256.txt"
        require(hashes.is_file(), f"missing installed launcher hash record {hashes}")
        configured = dict(line.split("=", 1) for line in hashes.read_text(encoding="ascii").splitlines() if "=" in line)
        require(configured.get(args.platform) == runtime_sha,
                f"launcher {args.platform} supported hash does not match {executable}")
        package_out = scratch / "runtime"
        no_game = scratch / "no-game-files"
        subprocess.run([sys.executable, str(REPO / "tools/package.py"), "--build", str(runtime_build),
                        "--exe", str(executable), "--out", str(package_out), "--game", str(no_game)], check=True)
        runtime_roots = [p for p in package_out.iterdir() if p.is_dir()]
        require(len(runtime_roots) == 1, "runtime package did not produce exactly one folder")
        destination = root / args.platform
        shutil.copytree(runtime_roots[0], destination)
        for stale in ("pt-updater", "pt-updater.exe", "pt-files.json"):
            (destination / stale).unlink(missing_ok=True)
        require(digest(destination / ("pt.exe" if args.platform == "windows" else "pt")) == runtime_sha,
                "packaged runtime differs from compiled release runtime")
        launcher_exe = root / ("pt-launcher.exe" if args.platform == "windows" else "libexec/pt-launcher")
        require(launcher_exe.is_file(), f"missing staged launcher {launcher_exe}")
        require(runtime_sha.encode("ascii") in launcher_exe.read_bytes(),
                "launcher does not embed the actual matching runtime SHA-256; configure PT_SUPPORTED_*_SHA256")
        if args.platform == "linux":
            linux_deploy(root, qt)
        else:
            windows_deploy(root, qt)
        shutil.copy2(REPO / "LICENSE", root / "LICENSE")
        qt_notices = root / "notices/qt"
        qt_notices.mkdir(parents=True, exist_ok=True)
        for name in QT_LICENSES:
            source = next((candidate for candidate in (
                qt / "licenses" / name, qt.parent.parent / "Licenses" / name,
                REPO / "tools/ci/licenses" / name) if candidate.is_file()), None)
            require(source, f"Qt license text missing: {name}")
            shutil.copy2(source, qt_notices / name)
        (root / "README-preview.md").write_text(
            "# Portable launcher preview\n\n"
            "Extract this unsigned test bundle. Put your own extracted `chunk1.psarc`, "
            "`texture.qar`, and `pathid_list_ps4.bin` under `CUSA01127/` at this "
            "bundle root. Start `pt-launcher.exe` (Windows) or `./pt-launcher` (Linux), "
            "then use **Game files** to choose this bundle root. "
            "PlayFlat disables upstream update checks. If you start the game runtime "
            "directly, pass `--no-update-check`; upstream in-game updates are not "
            "supported for this mixed preview bundle. Inspection and flat launch "
            "are the preview scope; gameplay and platform compositor behavior need "
            "manual testing. See `PREVIEW-NOTICES.md` for source and licenses.\n",
            encoding="utf-8")
        (root / "PREVIEW-NOTICES.md").write_text(
            f"# Unsigned portable preview\n\n"
            f"This test bundle contains the pt-pc launcher and game runtime from revision {args.revision}. "
            "It is unsigned and has no secure updater or automatic preview update path. "
            "Game archives are supplied by the player.\n\n"
            f"Qt {qt_version(qt)} shared libraries and plugins are included under the "
            "[Qt LGPLv3 terms](https://www.qt.io/terms-conditions/#licenses). "
            f"Corresponding Qt source: https://download.qt.io/archive/qt/{'.'.join(qt_version(qt).split('.')[:2])}/{qt_version(qt)}/src/ . "
            "The launcher source is at https://github.com/spitefulFr0g/pt-pc . "
            "Runtime third-party notices are in the platform runtime's licenses/ directory. "
            "Linux distribution library copyright records, when applicable, are under notices/linux-dependencies/. "
            "Windows compiler runtime licensing is governed by the "
            "[Microsoft Visual C++ redistributable terms](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files).\n",
            encoding="utf-8")
        require((root / ("assets" if args.platform == "windows" else "libexec/assets") / "spooky-forest-LICENSE.md").is_file(),
                "artwork license missing")
        require((root / ("assets" if args.platform == "windows" else "libexec/assets") / "PirataOne-Regular.ttf").is_file(),
                "launcher font missing")
        for path in root.rglob("*"):
            if path.is_file():
                relative = path.relative_to(root)
                require(path.name.lower() not in FORBIDDEN and path.suffix.lower() not in FORBIDDEN_SUFFIXES
                        and not any(part.lower() == "cusa01127" for part in relative.parts),
                        f"proprietary or unsupported file in preview: {relative}")
                require(not any(part.lower() in ("tests", "test", "evidence", "keys", ".git") for part in relative.parts),
                        f"development file in preview: {relative}")
        files = [{"path": p.relative_to(root).as_posix(), "size": p.stat().st_size, "sha256": digest(p)}
                 for p in sorted(root.rglob("*")) if p.is_file()]
        inventory = {"format": 1, "kind": "unsigned-portable-preview", "platform": args.platform,
                     "version": args.version, "revision": args.revision, "qt_version": qt_version(qt),
                     "runtime_source_sha256": runtime_sha, "supported_runtime_sha256": runtime_sha, "files": files}
        (root / "preview-inventory.json").write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
        external_inventory = args.out / (name + "-inventory.json")
        require(not external_inventory.exists(), f"output exists: {external_inventory}")
        external_inventory.write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
        if args.platform == "windows":
            with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
                for path in sorted(root.rglob("*")):
                    if path.is_file():
                        output.write(path, Path(name) / path.relative_to(root))
        else:
            with tarfile.open(archive, "w:gz") as output:
                output.add(root, arcname=name)
    sidecar = archive.with_name(archive.name + ".sha256")
    sidecar.write_text(f"{digest(archive)}  {archive.name}\n", encoding="ascii")
    print(archive)
    print(sidecar)
    print(external_inventory)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
