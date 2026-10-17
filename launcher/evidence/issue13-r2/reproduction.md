# #13 repair validation

Source implementation b80b9f9, integrated with b6907be3400873b6267ee91c81bda8b789dfcafb at dcb8530. Final evidence commit adds optional fixture forwarding and documentation; launcher source is unchanged from that merge.

Final command (exit 0):

```sh
PT_LAUNCHER_STAGED_EXTRACTOR=/home/watermelon/.cache/pt-issue6/evidence-final/staged/linux/extractor \
PT_LAUNCHER_TEST_PKG=/home/watermelon/.cache/pt-issue6/evidence-final/synthetic/fixture.pkg \
launcher/tests/run-controller-test.sh /home/watermelon/.cache/pt-issue13/r2
```

11/11 CTests pass in Release, 22.78 seconds, including conditional real helper/package preparation cases from merged #6. CTest starts with inherited offscreen; controller/launch/window/preparation UI tests provision separate Xvfb/Openbox/xcb displays. Qt6.12.0, SDL release-3.4.16, g++13, recorded container image. No process safety guards changed. Real denied-write/uninspectable-process checks remain active. `LastTest.log` in the external build also records vanishing-process, SIGKILL/recovery, lock/launch races and conservative uninspectable-process refusal.

The unmodified candidate passes 10/10 in a fresh container with the known-good harness (`baseline-ctest.log`); prior worker 3/10 was not reproduced. Retained `*-red.log` files show concrete regression failures before their fixes. Green verdict is the final combined suite; intermediate per-fix logs and full build/configure logs are external at the command's evidence directory. See `launcher/tests/controller-testing.md` and shared `progress-13-r2.md` for diagnosis and corrected fixture/setup observations.

Limits: controlled Linux/X11/virtual SDL, synthetic archives and shell game-process fixtures. Existing window fixture additionally verifies public ReturnToGame against a real Qt game window. No native Windows controller execution, Steam profile delivery/client, Gamescope, Wayland, physical device or complete packaged-release acceptance is asserted. #14/#15/#17 remain separate slices.
