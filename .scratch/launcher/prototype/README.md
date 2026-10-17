# P.T. launcher UI prototype — throwaway

Question: which information layout makes first-time preparation and everyday flat/VR launch easiest to understand?

This is a desktop-launcher mockup, not a website or a choice of production GUI toolkit. All actions are simulated in memory. It does not read/write game files, check hardware, open a runtime, download updates, or persist preferences. The user selected G with a freely licensed spooky forest photograph, a matching green palette, and the flat launch label Play.

Open `launcher-prototype.html` directly, or run from the repository root:

```sh
python3 -m http.server 8765 --directory .scratch/launcher/prototype
```

Then open <http://localhost:8765/launcher-prototype.html?variant=G> for the selected design. Keep the `assets/` folder with the HTML when opening or sharing this prototype folder. The floating selector retains every earlier layout for context.

- **F — Fog menu:** a sparse horror-game title menu over an original fogbound street illustration, with clear display/VR actions.
- **G — Selected design:** a foggy forest photograph by Mathias Reding (Pexels License) beside a simple forest-green launch panel with a gothic title. The flat launch label is Play; the VR action is separate. Routine ready/running prose is removed from the main screen; actionable warnings remain.

The previous simple utility directions remain available:

- **D — Compact:** choose display/headset with a simple radio control, then use one primary launch action. Details stay collapsed until needed.
- **E — Two choices:** two large action rows for display/headset play, with status and ordinary-language explanations directly beside each action.

The original three remain available for comparison:

- **A — Play first:** quiet main screen emphasizing the two launch actions, with preparation/settings as secondary tools.
- **B — Utility:** persistent navigation and a compact installation/readiness workspace.
- **C — Guided:** a preparation/readiness/play sequence with a focused task at each step.

Use the floating layout selector or left/right arrows outside input controls. The `variant` URL parameter (`A` through `G`) preserves the chosen layout across reloads. Use the example-scene selector to try setup, long extraction/cancel, inconclusive checks, missing VR prerequisites, flat fallback, manual updates, and rollback. The platform selector changes mock data only; it does not establish Linux/Windows compatibility.

The default example uses the user's Windows + Meta Quest 3 / Steam Link context. The final planning map records observed OpenXR configuration and SteamOS/Omarchy targets; the mock's data is not live hardware detection. The Ready scene is a fictional positive prerequisite result, not evidence from the user's device.

Browser review covered variant switching/URL updates, the settings screen, extraction cancellation, continuing the upstream flat fallback, and blocking an update while a simulated game session runs. Static previews at 728px and 360px rendered without JavaScript errors. No implementation tests or hardware/runtime tests were run.

The user explicitly requested the frontend-design skill and two further simple, varied directions. [Design plan and critique](design-notes.md) records their palette/type/layout choices. Additional browser review exercised D's mode selection/launch blocking, E's settings, VR-only blocker, and adoption of an existing bundle. D/E desktop and narrow screenshots were reviewed; their controls remain simulations.

The next requested round added Silent Hill / horror-game / haunted-house inspiration. [Horror design plan and critique](horror-design-notes.md) records the F/G directions and final user choice. Those initial scenes were original inline SVG; selected G now displays [Foggy Forest with Leafless Trees](assets/spooky-forest.jpg) by Mathias Reding, replacing the user-supplied promotional image. The downloaded JPEG is unchanged; the responsive crop is CSS-only. [Source and license details](assets/spooky-forest-LICENSE.md) are bundled with it. The forest-green theme remains selected. Browser interaction review covered settings, setup/extraction cancellation, a VR-only blocker, and continuing the flat fallback. There is no automatic animation or audio.

The title uses [Pirata One from Google Fonts](https://github.com/google/fonts/tree/main/ofl/pirataone), bundled locally as `assets/PirataOne-Regular.ttf` with its unchanged [SIL Open Font License notice](assets/PirataOne-OFL.txt). The browser uses the local file; the inline review embeds the same font data. Desktop and narrow previews verified the font loaded, the routine ready/running sentences were absent, and the Play label stayed stable while a simulated running session blocked a second launch.

Verdict: use G's split layout with the licensed spooky forest image, forest colors, and the Play label. The parent launcher's **Agree on the first-run and everyday launcher screens** ticket (`.scratch/launcher/issues/07-first-run-and-home.md`) is the canonical decision in its planning map. The accepted snapshot is preserved on `prototype/launcher-ui-accepted` and tag `archive/launcher-prototypes-2026-10-17`. It contains the current licensed assets and excludes superseded photo history. The older iterative branch remains local history. Do not merge or promote this mockup as the launcher implementation.

## Steam Deck controller interaction study

[Controller prototype](deck-controller-prototype.html) is a self-contained offline interaction study of the accepted G design. Open it directly, or visit <http://localhost:8765/deck-controller-prototype.html> using the server above. Its six scenarios cover play/return, preparation, safe cancellation, path editing, VR failure, and closing the GUI independently. Every file/process/session action is simulated in memory.

Arrow keys/D-pad/left stick move; Enter/A selects; Escape/B backs out; X opens the built-in keyboard on text fields. The on-page controller and environment buttons make every scenario runnable without hardware. The state readout remains outside the product mockup. Browser review is automated; it does not connect to native SDL, Steam or Gamescope.

[Controller notes](deck-controller-notes.md) record the input/session recommendation, source evidence and automated review results. [Readable source](deck-controller-source.html) uses the local assets; regenerate the self-contained file with `python3 .scratch/launcher/prototype/build-deck-prototype.py`. Keep the image/font notices when sharing the prototype. The user accepted the controller flow and independent session-helper behavior. **Design Steam Deck Gaming Mode and controller navigation** records the canonical decision. All required release checks remain automated and agent-run.

Replacement-image review: desktop (728px) and narrow (360px) previews loaded without JavaScript errors. Browser checks confirmed the 1600 × 2400 local photo, loaded gothic font, stable Play label, absent routine ready/running copy, and no horizontal overflow.

Planning decisions live separately on the `launcher` branch. Read [the completed map](https://github.com/spitefulFr0g/pt-pc/blob/launcher/.scratch/launcher/map.md), [screen decision](https://github.com/spitefulFr0g/pt-pc/blob/launcher/.scratch/launcher/issues/07-first-run-and-home.md), and [controller/session decision](https://github.com/spitefulFr0g/pt-pc/blob/launcher/.scratch/launcher/issues/10-steam-deck-controller-ui.md).
