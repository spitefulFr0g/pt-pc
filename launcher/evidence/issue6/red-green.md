# Ticket #6 red/green notes

The existing public preparation operation already had the guarded extraction
and recovery behavior. The work here made that path reproducible with a real
staged self-contained helper and added public-operation and controlled-helper
coverage. Red runs exposed acceptance harness defects before the final green
run:

- CTest initially could not find the staged helper because Docker received a
  host `/tmp` path instead of its `/evidence` mount path. The harness now maps
  evidence paths into the container and stages the helper directory and
  synthetic package inside that mount.
- The first public-operation run copied only the Linux apphost. The process
  correctly reported its adjacent managed assembly missing. The fixture now
  copies the full published helper directory, including its self-contained
  runtime and LGPL source/notices.
- The separate insufficient-space run initially attempted to copy that helper
  into its 64 MiB tmpfs. The helper/package environment is now unset for that
  isolated space-only process.

After those harness fixes, the final merged run passed both staged helper
platforms and their refusal diagnostics, public `PrepareGame`, all 9 CTest
cases, and the real insufficient-space preflight. Logs for the final run are in
this directory. No game or engine source, player-data format, or proprietary
fixture was changed or added.
