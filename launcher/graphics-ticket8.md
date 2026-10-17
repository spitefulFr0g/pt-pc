# Ticket #8 graphics preset checks

Run from the repository root:

```sh
launcher/run-issue8-graphics-container.sh /tmp/pt-launcher-issue8-graphics-evidence
```

The wrapper reuses the complete #7 disposable-bundle Qt 6.12/container harness and CTest suite. The focused settings operations and real Qt Settings dialog tests cover preset selection/readback, complete graphics and ray-tracing key writes, unknown-key retention, unknown capability defaulting to no ray tracing, runtime replacement blocking, settings conflict handling, and existing activity guards. Evidence is written outside the checkout. This exercises Qt through its offscreen event loop and controlled bundle/process fixtures; it does not claim physical display-driver switching or native Windows process execution.
