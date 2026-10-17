# Ticket #7 display settings checks

Run the noninteractive disposable-bundle checks from the repository root:

```sh
launcher/run-issue7-settings-container.sh /tmp/pt-launcher-issue7-settings-evidence
```

The harness uses the recorded local Ubuntu 24.04 launcher build image and Qt 6.12.0 SDK, mounts source read-only, creates disposable Unicode/spaced bundle paths inside tests, builds the native launcher and test targets as an unprivileged UID, runs the complete launcher CTest set, and writes logs plus hashes outside the checkout. Each run gets its own build folder. A nonzero configure, build, or CTest result fails the command. If Qt is missing, prepare it with `launcher/run-issue4-qt612-container.sh`; the required image name and SDK root are also in the implementation run notes.

Ticket coverage includes public Linux/Windows settings reads; unchanged runtime defaults, malformed-value fallback, and value clamping; latest-text patching of only edited display keys; comments, unknown entries, spacing, and CRLF preservation; same-key conflict detection; Unicode and spaces; stopped-session guarding; a second activity check immediately before atomic replacement; absent settings defaults; and a failed replacement path. A real Qt event loop drives the Settings dialog through Apply/readback, Discard, Reload, conflict reconciliation, and safe-default Keep editing on close/back.

Evidence records the exact local container image ID and test logs. The Qt dialog runs with Qt's offscreen platform plugin; this establishes real widget/event-loop behavior and observable fixture files, not physical display-driver mode switching, window-manager fullscreen behavior, or Windows process activity. Windows platform parsing/writes are exercised on Linux fixtures; native Windows execution remains a paired CI check.
