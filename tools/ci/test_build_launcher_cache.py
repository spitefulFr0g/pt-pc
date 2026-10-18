"""Exercise native build orchestration without compiling the game."""
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import build_launcher_preview as build


class ConfigureReached(Exception):
    pass


class CompilerCacheTest(unittest.TestCase):
    def commands(self, target, cache):
        commands = []
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            runtime = work / 'runtime-stage'
            runtime.mkdir()
            (runtime / 'libopenxr_loader.so.1').write_bytes(b'loader')
            (runtime / ('pt.exe' if target == 'windows' else 'pt')).write_bytes(b'game')
            (runtime / 'runtime-origin.json').write_text(json.dumps({'executable_sha256': 'old'}))
            (work / 'game').mkdir()
            (work / 'game/pt').write_bytes(b'game')
            (work / 'game/libopenxr_loader.so.1').write_bytes(b'loader')
            (work / 'zlib-install/lib').mkdir(parents=True)
            (work / 'zlib-install/lib/zs.lib').touch()

            def popen(command, **kwargs):
                commands.append((command, kwargs['env'].copy()))
                if command[:2] == ['cmake', '-S'] and str(build.REPO / 'launcher') in command:
                    raise ConfigureReached()
                process = unittest.mock.MagicMock()
                process.__enter__.return_value = process
                process.stdout = io.StringIO('')
                process.wait.return_value = 0
                return process

            with patch.dict(os.environ, {'CCACHE_DIR': str(work / 'cache')} if cache else {}, clear=True), \
                 patch('sys.argv', ['build', '--platform', target, '--version', '0.1.0', '--work', tmp]), \
                 patch.object(build.platform, 'system', return_value='Linux' if target == 'linux' else 'Windows'), \
                 patch.object(build.platform, 'machine', return_value='x86_64'), \
                 patch.object(build.subprocess, 'Popen', side_effect=popen), \
                 patch.object(build.subprocess, 'check_output', return_value='fixture'), \
                 patch.object(build.subprocess, 'run'), \
                 patch.object(build.shutil, 'which', return_value='/usr/bin/ccache'), \
                 patch.object(build.shutil, 'rmtree'), patch.object(build.shutil, 'copy2'), \
                 patch.object(build.shutil, 'copytree'):
                with self.assertRaises(ConfigureReached):
                    build.main()
                timings = json.loads((work / 'evidence/timings.json').read_text())
                self.assertEqual(timings[-1]['phase'], 'launcher-configure')
                self.assertEqual(timings[-1]['status'], 'failed')
                self.assertGreaterEqual(timings[-1]['seconds'], 0)
        return commands

    def test_linux_game_and_launcher_use_cache(self):
        commands = self.commands('linux', True)
        configure = [(cmd, env) for cmd, env in commands if cmd[:2] == ['cmake', '-S']]
        self.assertEqual(len(configure), 2)
        for cmd, env in configure:
            self.assertIn('-DCMAKE_C_COMPILER_LAUNCHER=/usr/bin/ccache', cmd)
            self.assertIn('-DCMAKE_CXX_COMPILER_LAUNCHER=/usr/bin/ccache', cmd)
            self.assertEqual(env['CCACHE_COMPILERCHECK'], 'content')
            self.assertEqual(env['CCACHE_BASEDIR'], str(build.REPO))
            self.assertEqual(env['CCACHE_MAXSIZE'], '2G')

    def test_optional_cache_and_windows(self):
        for target, cache in [('linux', False), ('windows', False), ('windows', True)]:
            with self.subTest(target=target, cache=cache):
                for cmd, _ in self.commands(target, cache):
                    self.assertFalse(any('COMPILER_LAUNCHER' in part for part in cmd))


class WorkflowContractTest(unittest.TestCase):
    def test_only_superseded_pr_runs_are_cancelled(self):
        workflow = (build.REPO / '.github/workflows/launcher-preview.yml').read_text()
        self.assertIn("group: launcher-preview-${{ github.event_name }}-${{ github.ref }}", workflow)
        self.assertIn("cancel-in-progress: ${{ github.event_name == 'pull_request' }}", workflow)

    def test_cache_restore_and_successful_save_accumulate_inputs(self):
        workflow = (build.REPO / '.github/workflows/launcher-preview.yml').read_text()
        self.assertIn('uses: actions/cache/restore@v4', workflow)
        self.assertIn('uses: actions/cache/save@v4', workflow)
        self.assertIn("if: success() && runner.os == 'Linux'", workflow)
        self.assertIn('${{ github.sha }}-${{ github.run_id }}-${{ github.run_attempt }}', workflow)
        self.assertIn('restore-keys: |\n            ${{ steps.compiler-cache.outputs.prefix }}-', workflow)
        self.assertIn('key: ${{ steps.ccache-restore.outputs.cache-primary-key }}', workflow)
        self.assertEqual(workflow.count('path: ${{ runner.temp }}/launcher-ccache'), 2)
