"""Stage the unchanged, checksum-pinned upstream v1.0.5 portable runtime."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import urllib.request
import zipfile

REPO = Path(__file__).resolve().parents[2]
RELEASE_SOURCE_REVISION = "60fe8b324aca24c264e89378ae0d2c61f3a674af"
ARCHIVES = {
    "windows": "e84209e1c46e3e0e1d09b6e1eb0cfdcfabecee7c8ed010887466be9ade4488e8",
    "linux": "45a762170299bb535f62c6c53b9549a5fac1620fbae6e16b439b4d22d9814fe5",
}


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=ARCHIVES, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--archive", type=Path, help="An already downloaded archive; the same pinned hash is required")
    args = parser.parse_args()
    output = args.out.resolve()
    if output.exists():
        raise ValueError(f"Runtime staging directory already exists: {output}")
    url = ("https://github.com/LoreanXavier/pt-pc/releases/download/v1.0.5/"
           f"P.T.PC.Port-portable-{args.platform}.zip")
    output.parent.mkdir(parents=True, exist_ok=True)
    archive = args.archive.resolve() if args.archive else output.parent / f"upstream-v1.0.5-{args.platform}.zip"
    if not archive.exists():
        if args.archive:
            raise ValueError(f"Archive not found: {archive}")
        partial = archive.with_suffix(".zip.part")
        with urllib.request.urlopen(url, timeout=60) as response, partial.open("wb") as destination:
            shutil.copyfileobj(response, destination)
        if sha256(partial) != ARCHIVES[args.platform]:
            partial.unlink()
            raise ValueError("Downloaded upstream runtime archive SHA-256 does not match the pin")
        partial.replace(archive)
    if sha256(archive) != ARCHIVES[args.platform]:
        raise ValueError(f"Upstream runtime archive SHA-256 does not match the pin: {archive}")
    if args.platform == "windows":
        sys.path.insert(0, str(REPO / "tools"))
        from package import validate_upscaler_archive
        validate_upscaler_archive(archive)

    with zipfile.ZipFile(archive) as source:
        entries = source.infolist()
        roots, seen = set(), set()
        for entry in entries:
            path = PurePosixPath(entry.filename)
            if (path.is_absolute() or ".." in path.parts or "\\" in entry.filename
                    or ":" in entry.filename or not path.parts):
                raise ValueError(f"Unsafe upstream archive member: {entry.filename}")
            key = path.as_posix().casefold()
            if key in seen or stat.S_ISLNK(entry.external_attr >> 16):
                raise ValueError(f"Duplicate path or symlink in upstream archive: {entry.filename}")
            seen.add(key)
            roots.add(path.parts[0])
        if len(roots) != 1:
            raise ValueError("Upstream runtime ZIP must have a single root folder")
        archive_root = roots.pop()
        # Extract only the pinned runtime's files; retain Unix executable modes.
        for entry in entries:
            relative = PurePosixPath(entry.filename).relative_to(archive_root)
            if not relative.parts:
                continue
            destination = output.joinpath(*relative.parts)
            if entry.is_dir():
                destination.mkdir(parents=True, exist_ok=True)
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            with source.open(entry) as src, destination.open("wb") as dst:
                shutil.copyfileobj(src, dst)
            if entry.create_system == 3:
                destination.chmod(stat.S_IMODE(entry.external_attr >> 16) & 0o777)
            if args.platform == "linux":
                # Upstream also packages on Windows, which loses Unix execute
                # bits. Restore them for the native programs and script tools.
                with destination.open("rb") as binary:
                    magic = binary.read(4)
                if magic == b"\x7fELF" or magic.startswith(b"#!"):
                    destination.chmod(destination.stat().st_mode | 0o111)

    # In-game updating cannot maintain the new paired layout. Preview images
    # captured from the original game are also excluded from our test bundles.
    for name in ("pt-updater", "pt-updater.exe", "pt-files.json"):
        (output / name).unlink(missing_ok=True)
    shutil.rmtree(output / "loop-previews", ignore_errors=True)
    executable = output / ("pt.exe" if args.platform == "windows" else "pt")
    if not executable.is_file():
        raise ValueError(f"Upstream archive lacks {executable.name}")
    if args.platform == "linux" and not (executable.stat().st_mode & stat.S_IXUSR):
        raise ValueError("Upstream Linux runtime has no executable permission")
    origin = {
        "repository": "LoreanXavier/pt-pc", "release": "v1.0.5",
        "release_source_revision": RELEASE_SOURCE_REVISION,
        "archive_root": archive_root, "archive_url": url,
        "archive_sha256": ARCHIVES[args.platform], "executable_sha256": sha256(executable),
    }
    (output / "runtime-origin.json").write_text(json.dumps(origin, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(origin, indent=2))


if __name__ == "__main__":
    main()
