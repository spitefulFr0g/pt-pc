# SDL controller test

Run the controller acceptance harness from the repository root:

```sh
launcher/tests/run-controller-test.sh [evidence-directory]
```

It uses the `pt-launcher-preview-build-env-complete` Docker image and the Qt 6.12 SDK at `/tmp/launcher-qt-6.12.0`. The script configures the launcher, fetches the exact SDL `release-3.4.16` tag, builds in Release mode, starts Xvfb/Openbox with Qt's xcb backend, and runs `launcher_controller`. A nonzero configure, build, or test result propagates as a failing exit status. The default evidence directory is `/tmp/pt-controller-acceptance-13`; it contains `controller-acceptance.log` and the configured build/test artifacts.

The test attaches an SDL virtual gamepad and injects axis/button values through SDL's virtual joystick API. It observes actual Qt focus and button actions in the Qt event loop. Covered behavior includes disabled-control skipping, focus restore after a modal, focus-sensitive hints, dead zone, delayed/repeating navigation, edge-triggered A, B/Escape, Qt foreground and session-owner gating, handoff neutral rearming, and disconnect/reconnect.

These fixtures verify SDL-to-Qt behavior on a virtual Linux display. They do not establish physical Steam Deck behavior, Steam Input profile delivery, Gaming Mode/Gamescope handoff, or Windows controller behavior.
