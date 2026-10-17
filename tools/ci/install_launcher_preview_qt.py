"""Install the pinned Qt SDK, including Qt 6.12's split Windows repository."""

import argparse

from aqt.installer import Cli
from aqt.metadata import QtRepoProperty


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("windows", "linux"), required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    architecture = {"windows": "win64_msvc2022_64", "linux": "linux_gcc_64"}[args.platform]

    # aqtinstall 3.3.0 understands the split Linux repository, but still looks for
    # Windows metadata under qt6_6120/qt6_6120 instead of qt6_6120_msvc2022_64.
    # Only change repository selection; aqt still verifies Qt's archive hashes.
    original_extension = QtRepoProperty.extension_for_arch

    def extension_for_arch(arch, is_version_ge_6):
        if arch == "win64_msvc2022_64" and is_version_ge_6:
            return "msvc2022_64"
        return original_extension(arch, is_version_ge_6)

    QtRepoProperty.extension_for_arch = staticmethod(extension_for_arch)
    command = ["install-qt", args.platform, "desktop", "6.12.0", architecture, "-O", args.output]
    if args.dry_run:
        command.append("--dry-run")
    return Cli().run(command)


if __name__ == "__main__":
    raise SystemExit(main())
