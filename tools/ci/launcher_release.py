"""Shared identity for stable and preview portable launcher releases."""
import argparse
import os
from pathlib import Path
import re

REPO = Path(__file__).resolve().parents[2]


def identity(version):
    base = (REPO / 'launcher/VERSION').read_text(encoding='ascii').strip()
    number = r'(?:0|[1-9][0-9]*)'
    match = re.fullmatch(rf'({number}\.{number}\.{number})(-preview\.{number})?', version)
    if not match or match[1] != base:
        raise ValueError(f'Use {base} or {base}-preview.N (no leading zeros)')
    preview = bool(match[2])
    return {'version': version, 'channel': 'preview' if preview else 'stable',
            'tag': ('launcher-v' if preview else 'v') + version,
            'prerelease': 'true' if preview else 'false'}


def bundle_name(version, platform):
    release = identity(version)
    return f"pt-launcher-{'preview-' if release['channel'] == 'preview' else ''}{version}-{platform}"


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default='')
    args = parser.parse_args()
    base = (REPO / 'launcher/VERSION').read_text(encoding='ascii').strip()
    release = identity(args.version or f"{base}-preview.{os.environ['GITHUB_RUN_NUMBER']}")
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        for key, value in release.items():
            output.write(f'{key}={value}\n')
