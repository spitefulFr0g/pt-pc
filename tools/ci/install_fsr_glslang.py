#!/usr/bin/env python3
"""Install the Windows glslang executable used by FidelityFX_SC under Wine."""

import argparse
import hashlib
import io
import urllib.request
import zipfile
from pathlib import Path


URL = (
    "https://github.com/KhronosGroup/glslang/releases/download/16.6.0/"
    "glslang-16.6.0-windows-x86_64-release.zip"
)
SHA256 = "82bf434e69b9bb4829de7e2b4bc2c5e7a7861e53d66cf75e5cc70f5f694a8d9b"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="Directory to add to Wine's WINEPATH")
    args = parser.parse_args()

    with urllib.request.urlopen(URL, timeout=60) as response:
        archive = response.read()
    actual = hashlib.sha256(archive).hexdigest()
    if actual != SHA256:
        raise RuntimeError(f"glslang archive SHA256 mismatch: {actual}")

    with zipfile.ZipFile(io.BytesIO(archive)) as zip_file:
        executable = zip_file.read("bin/glslang.exe")

    args.destination.mkdir(parents=True, exist_ok=True)
    output = args.destination / "glslangValidator.exe"
    output.write_bytes(executable)
    print(output.resolve())


if __name__ == "__main__":
    main()
