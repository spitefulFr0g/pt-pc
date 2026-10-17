#!/usr/bin/env python3
"""Controlled executable/IPC regression; no Steam or proprietary data."""
import json, os, pathlib, signal, subprocess, sys, tempfile, time
exe = sys.argv[1]
def wait(fn):
    for _ in range(200):
        value = fn()
        if value: return value
        time.sleep(.025)
    raise AssertionError('timed out')
with tempfile.TemporaryDirectory(prefix='pt session ü ') as temp:
    root = pathlib.Path(temp)
    (root/'linux').mkdir(); (root/'CUSA01127').mkdir()
    # Same minimal archive signatures as the public session-operation fixture.
    (root/'CUSA01127/chunk1.psarc').write_bytes(b'PSAR'+b'\0'*60)
    qar = bytearray(64); qar[50:52] = b'aq'
    (root/'CUSA01127/texture.qar').write_bytes(qar)
    game = root/'linux/pt'
    game.write_text('#!/bin/sh\nenv > "$(dirname "$0")/environment"\nsleep 60 & echo $! > "$(dirname "$0")/child.pid"\nwait\n')
    game.chmod(0o700)
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen', PT_SESSION_SENTINEL='original', QT_PLUGIN_PATH='/original/plugin/path')
    proc = subprocess.Popen([exe, '--gaming-mode', temp], env=env)
    def call(op, **kw):
        try:
            p = subprocess.run([exe, '--session-command', temp, json.dumps(dict(v=1, op=op, **kw))], capture_output=True, text=True, timeout=3)
        except subprocess.TimeoutExpired:
            return {}
        response = json.loads(p.stdout) if p.returncode == 0 else {}
        print(op, response, flush=True)
        return response
    try:
        state = wait(lambda: call('status'))
        assert state['supervisor'] == proc.pid
        gui = state['gui']; assert gui > 0
        duplicate = subprocess.run([exe, '--gaming-mode', temp], env=env, timeout=5)
        assert duplicate.returncode == 0 and call('status')['gui'] == gui
        assert call('unknown')['error']
        assert call('job-begin', job='controlled-work')['jobs'] == 1
        assert call('job-end', job='controlled-work')['jobs'] == 0
        assert call('job-begin', job='crashed-gui-work')['jobs'] == 1
        assert call('play', tryAnyway=True)['error']
        os.kill(gui, signal.SIGKILL)
        wait(lambda: call('status').get('gui') == 0)
        assert call('status')['jobs'] == 0
        gui = call('reconnect')['gui']
        launched = call('play', tryAnyway=True)
        assert launched.get('game', 0) > 0, launched
        pid = launched['game']
        assert call('play', tryAnyway=True)['game'] == pid
        wait(lambda: (root/'linux/environment').exists())
        captured = (root/'linux/environment').read_text()
        assert 'PT_SESSION_SENTINEL=original' in captured and 'QT_PLUGIN_PATH=/original/plugin/path' in captured
        assert 'PT_SESSION_BUNDLE=' not in captured
        os.kill(gui, signal.SIGKILL)
        wait(lambda: call('status').get('gui') == 0)
        assert proc.poll() is None and call('status')['game'] == pid
        assert call('status')['jobs'] == 0
        assert call('reconnect')['gui'] != gui
        # Stale identity must never signal a reused/unrelated PID.
        assert call('stop-game', identity='stale')['error']
        os.kill(pid, 0)
        child = int(wait(lambda: (root/'linux/child.pid').read_text() if (root/'linux/child.pid').exists() else ''))
        proc.send_signal(signal.SIGTERM); proc.wait(timeout=8)
        wait(lambda: not pathlib.Path('/proc', str(child)).exists() or ') Z ' in pathlib.Path('/proc', str(child), 'stat').read_text())
        wait(lambda: not pathlib.Path('/proc', str(pid)).exists() or ') Z ' in pathlib.Path('/proc', str(pid), 'stat').read_text())
        # A final game exit with no GUI also releases the root.
        proc = subprocess.Popen([exe, '--gaming-mode', temp], env=env)
        state = wait(lambda: call('status'))
        launched = call('play', tryAnyway=True)
        os.kill(state['gui'], signal.SIGKILL)
        wait(lambda: call('status').get('gui') == 0)
        assert not call('stop-game', identity=launched['identity'])['error']
        proc.wait(timeout=8)
        # Final GUI exit ends an idle root.
        proc = subprocess.Popen([exe, '--gaming-mode', temp], env=env)
        state = wait(lambda: call('status'))
        os.kill(state['gui'], signal.SIGTERM)
        proc.wait(timeout=8)
        # Abrupt root death leaves a stale endpoint/lock; a new root recovers it.
        proc = subprocess.Popen([exe, '--gaming-mode', temp], env=env)
        state = wait(lambda: call('status'))
        proc.kill(); proc.wait(timeout=5); os.kill(state['gui'], signal.SIGKILL)
        proc = subprocess.Popen([exe, '--gaming-mode', temp], env=env)
        state = wait(lambda: call('status'))
        assert state['supervisor'] == proc.pid
        call('shutdown'); proc.wait(timeout=8)
    finally:
        if proc.poll() is None:
            proc.terminate()
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                state = call('status')
                if state.get('gui'): os.kill(state['gui'], signal.SIGKILL)
                proc.kill(); proc.wait(timeout=5)
print('PASS supervisor executable lifecycle, reconnect, environment, stale identity, shutdown')
