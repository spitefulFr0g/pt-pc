"""Regression checks for the release identities consumed by native packaging."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from launcher_release import identity, bundle_name
from smoke_launcher_preview import check


class ReleaseContractTest(unittest.TestCase):
    def test_version_metadata_cannot_shadow_cpp_version_header(self):
        from launcher_release import REPO
        # MSVC resolves <version> case-insensitively through launcher include dirs.
        self.assertFalse(any(path.name.casefold() == 'version'
                             for path in (REPO / 'launcher').iterdir()))
        self.assertEqual((REPO / 'launcher/launcher-version.txt').read_text().strip(),
                         '0.1.0')

    def test_stable_and_preview(self):
        for version, channel, tag, name in (
            ('0.1.0', 'stable', 'v0.1.0', 'pt-launcher-0.1.0-windows'),
            ('0.1.0-preview.11', 'preview', 'launcher-v0.1.0-preview.11',
             'pt-launcher-preview-0.1.0-preview.11-windows'),
        ):
            with self.subTest(version=version):
                release = identity(version)
                self.assertEqual((release['channel'], release['tag']), (channel, tag))
                self.assertEqual(release['prerelease'], str(channel == 'preview').lower())
                self.assertEqual(bundle_name(version, 'windows'), name)

    def test_linux_archive_names(self):
        self.assertEqual(bundle_name('0.1.0', 'linux'), 'pt-launcher-0.1.0-linux')
        self.assertEqual(bundle_name('0.1.0-preview.11', 'linux'),
                         'pt-launcher-preview-0.1.0-preview.11-linux')

    def test_reject_invalid_or_different_source_version(self):
        for version in ('1.0.0', '0.1.1-preview.1', 'v0.1.0', '00.1.0',
                        '0.1.0-preview.01', '0.1.0-rc.1', '0.1.0+build', '../0.1.0', ''):
            with self.subTest(version=version), self.assertRaises(ValueError):
                identity(version)

    def test_smoke_rejects_identity_mismatches_before_launch(self):
        for version in ('0.1.0', '0.1.0-preview.11'):
            for field in ('tag', 'channel', 'root', 'launcher-version'):
                with self.subTest(version=version, field=field), tempfile.TemporaryDirectory() as tmp:
                    release = identity(version)
                    root = Path(tmp) / (bundle_name(version, 'windows') if field != 'root' else 'wrong')
                    root.mkdir()
                    data = dict(release, format=1, platform='windows',
                                kind='unsigned-portable-' + release['channel'])
                    if field in ('tag', 'channel'):
                        data[field] = 'wrong'
                    inventory = 'preview-inventory.json' if release['channel'] == 'preview' else 'launcher-inventory.json'
                    (root / inventory).write_text(json.dumps(data))
                    (root / 'launcher-version.txt').write_text('wrong' if field == 'launcher-version' else version)
                    with self.assertRaisesRegex(ValueError, 'identity differs|root/version differs|version differs'):
                        check(root, 'windows')

    def test_packaging_keeps_archive_root_separate_from_license_loop(self):
        # Exercise archive writing through main with synthetic staged inputs and
        # mocked deployment, retaining real archive/inventory/checksum creation.
        import package_launcher_preview as package
        import hashlib
        import sys
        import zipfile
        for version in ('0.1.0', '0.1.0-preview.11'):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as tmp:
                base = Path(tmp)
                stage, runtime, qt, out = [base / x for x in ('stage', 'runtime', 'qt', 'out')]
                for folder in (stage, runtime, qt / 'licenses'):
                    folder.mkdir(parents=True)
                sha = hashlib.sha256(b'runtime').hexdigest()
                (runtime / 'pt.exe').write_bytes(b'runtime')
                (runtime / 'runtime-origin.json').write_text(json.dumps({'executable_sha256': sha}))
                helper = runtime / 'extractor'
                helper.mkdir()
                for name in ('PT.PkgExtract.exe', 'LICENSE-LibOrbisPkg.txt', 'LibOrbisPkg-corresponding-source.zip'):
                    (helper / name).write_text('fixture')
                (stage / 'pt-launcher.exe').write_text(sha)
                (stage / 'supported-runtime-sha256.txt').write_text('windows=' + sha)
                (stage / 'launcher-version.txt').write_text(version)
                (stage / 'assets').mkdir()
                for name in ('spooky-forest-LICENSE.md', 'PirataOne-Regular.ttf'):
                    (stage / 'assets' / name).write_text('fixture')
                for name in package.QT_LICENSES:
                    (qt / 'licenses' / name).write_text('fixture')
                argv = ['package', '--platform', 'windows', '--runtime-stage', str(runtime),
                        '--launcher-stage', str(stage), '--qt-root', str(qt), '--out', str(out),
                        '--version', version, '--revision', 'a' * 40]
                with patch.object(sys, 'argv', argv), patch.object(package, 'windows_deploy'), patch.object(package, 'qt_version', return_value='6.12.0'):
                    package.main()
                name = bundle_name(version, 'windows')
                archive = out / (name + '.zip')
                with zipfile.ZipFile(archive) as zipped:
                    self.assertTrue(all(p.startswith(name + '/') for p in zipped.namelist()))
                    inventory_name = 'preview-inventory.json' if '-preview.' in version else 'launcher-inventory.json'
                    readme_name = 'README-preview.md' if '-preview.' in version else 'README-launcher.md'
                    readme = zipped.read(name + '/' + readme_name).decode('utf-8')
                    self.assertIn('**Choose .pkg and prepare**', readme)
                    self.assertNotIn('**Game files**', readme)
                    internal = zipped.read(name + '/' + inventory_name)
                    self.assertEqual(internal, (out / (name + '-inventory.json')).read_bytes())
                    data = json.loads(internal)
                    self.assertEqual((data['version'], data['tag']), (version, identity(version)['tag']))
                self.assertEqual((out / (archive.name + '.sha256')).read_text(),
                                 hashlib.sha256(archive.read_bytes()).hexdigest() + '  ' + archive.name + '\n')
