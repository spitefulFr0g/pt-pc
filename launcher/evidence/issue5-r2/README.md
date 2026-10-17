# Issue 5 Linux escalation evidence — 2026-10-17

Command: `launcher/run-issue5-acceptance.sh /tmp/pt-issue5-r2-final`

Pass: all six launcher CTest cases and actual free-space preflight on Docker's
64 MiB tmpfs (67,076,096 available bytes). Ubuntu 24.04 container,
GCC 13.3.0, CMake 3.28.3, Qt 6.12.0; host Debian 13.7/WSL2.
Image: `sha256:763f56146b3927b7e85b7b7197e0539fbddbb0856bb430020c54def74a5fa549`.
Based on local integration/launcher 854178b (merge reported already up to date).

`ctest.log` records actual bounded SIGKILL during copying and after successful
archive-placement, settings-backup, and settings-placement renames. Before each
kill, concurrent public PlayFlat, PrepareGame and WithStoppedBundle writes are
rejected by the held preparation lock. Public RecoverPreparation restores the
original archive, metadata, settings, source, player and unowned bytes; public
PrepareGame then succeeds. No journals are handcrafted in these process cases.
The library adapter is loaded only into disposable test children. Production
has no environment-variable fault hook. Existing import/adoption and Qt tests
remain in the full suite.

Red/green slice 1: `unknown-activity-red.log` shows the new real non-dumpable
process regression failed before session.cpp changed. `/proc/PID/exe` is denied
while cmdline remains readable. Previously this was misreported as Stopped;
now public inspection returns Uncertain and Play/preparation/recovery/writes
fail closed. The test asserts the real permission error, so an environment
that cannot establish it fails rather than silently skipping coverage.

Red/green slice 2: a full-suite run aborted in launcher_session with
`basic_filebuf::underflow error reading the file: No such process` while reading
procfs. `proc-race-red.log` records a deterministic syscall ESRCH reproduction
failing before the second fix. Inspection now handles vanished processes and
returns Uncertain on other read exceptions. The final regression asserts the
adapter actually consumed the injected read failure.

These are synthetic recognition archives and controlled Linux processes, not
playable data. This does not claim native Windows, Wayland/SteamOS, or full
packaged GUI/helper crash coverage; those remain in #24/#26/#27. No unavoidable
Linux coverage blocker remains for the escalation's requested cases. No
push/release/ticket closure was performed. Full build/setup logs remain in the
external evidence directory.
