# Agree on the first-run and everyday launcher screens

Type: prototype
Labels: wayfinder:prototype
Status: resolved
Assignee: watermelon
Parent: ../map.md
Blocked by: 01, 02, 03, 05

## Question

What GUI makes first-time preparation and later flat/VR launches understandable? Use the prototype skill to produce a throwaway artifact for the user to react to, covering source/destination selection, installation progress/cancellation, readiness/recovery, basic settings, and everyday Play/Play in VR actions.

Resolve the design through the user's live feedback. Link the prototype as an asset; it is not production launcher implementation. Include a long extraction and a failed VR readiness check. Keep implementation details out of normal player flows.

## Comments

- Prerequisites: [Choose the installation and package model](01-installation-and-packages.md), [Define downstream update ownership](02-downstream-updates.md), [Define the launcher's basic settings](03-basic-settings.md), [Define readiness and VR failure behavior](05-readiness-and-vr-behavior.md).
- User-approved direction: the GUI sits over the existing portable bundle and can replace the everyday need for launch scripts while preserving their underlying behavior and usability. Do not introduce a launcher-specific game/data format.
- Claimed after its prerequisites were resolved. Apply the prototype skill's UI branch: three structurally different layouts in a self-contained throwaway HTML artifact, selectable with `?variant=A`, `B`, or `C` and a floating switcher. There is no existing web UI to embed this desktop-launcher study into. Browser presentation does not choose the production GUI toolkit.
- Use only in-memory simulated actions; do not inspect or mutate the real portable bundle from the prototype. Capture the selected design only after the user reacts. This ticket remains open until that live feedback.
- Asset: [Launcher UI prototype](https://github.com/spitefulFr0g/pt-pc/blob/a24c10d89a1ad77f92698db19b510ed9635c9380/.scratch/launcher/prototype/launcher-prototype.html), with [Run and review instructions](https://github.com/spitefulFr0g/pt-pc/blob/a24c10d89a1ad77f92698db19b510ed9635c9380/.scratch/launcher/prototype/README.md). Three layouts share the same simulated state: A Play first, B Utility, C Guided. The example-state selector covers preparation, extraction/cancel, readiness, fallback, settings, and update/recovery scenarios.
- Browser interaction review and 728px/360px HTML previews completed without JavaScript errors. The prototype has no connection to real game files or hardware. UI verdict remains pending; no prototype branch has been captured yet because the user has not selected a design.
- The user requested `frontend-design` and two more simple, varied choices. Added D Compact (mode chooser plus one launch action) and E Two choices (large display/headset action rows), preserving A/B/C. [Design plan and critique](https://github.com/spitefulFr0g/pt-pc/blob/a24c10d89a1ad77f92698db19b510ed9635c9380/.scratch/launcher/prototype/design-notes.md) records the two-pass design process. Desktop/narrow screenshots and focused simulated interactions were reviewed; the design decision remains open for user feedback.
- The user then requested two more directions inspired by Silent Hill, horror games, and haunted houses. Added F Fog menu and G The foyer, with original code-native vector artwork and distinct full-scene/split-panel layouts. [Horror design plan and critique](https://github.com/spitefulFr0g/pt-pc/blob/a24c10d89a1ad77f92698db19b510ed9635c9380/.scratch/launcher/prototype/horror-design-notes.md) records the design process and screenshot review. All seven requested concepts remain available; no aesthetic winner has been selected and no production launcher functionality was added.

## Answer

The user selected **G's split illustration/control-panel layout**, initially replacing the hallway illustration with their supplied forest image, then requesting a freely licensed spooky forest replacement. The selected asset is now Mathias Reding’s foggy forest photograph from Pexels. They additionally required the flat launch label to stay **Play** and authorized a color theme that matches the image.

- Keep the selected two-column composition at desktop widths: licensed forest photograph on the left, simple launch controls on the right. At narrow widths the image becomes a strip above the controls. Keep the tangled branches visible in the responsive crop.
- Use deep forest/evergreen surfaces, pale stone text, and a muted sage primary button. Keep labels and readiness explanations legible; no automatic flicker or audio.
- Subsequent user refinement: use a fitting gothic title font and remove routine Your game is ready to play / Your game is running copy from the main screen. Use locally bundled Pirata One for the P.T. title, retaining ordinary readable type for controls. Show actionable warnings and recovery information when needed, rather than routine ready/running prose. Keep launch labels stable and disable unavailable actions.
- Flat launch is labeled **Play**, including when a readiness result is inconclusive or a session is running. Its enabled state and surrounding status still reflect the actual situation. The VR action remains separate: Play in VR when prerequisites are found, with the previously approved explicit Try VR anyway action for unknown checks.
- Setup, extraction/cancellation, basic settings, manual updates, and readiness/fallback recovery retain the shared prototype flows. Their controls stay separate from the artwork and from the prototype's example-state controls.
- The replacement [Selected image](https://github.com/spitefulFr0g/pt-pc/blob/a24c10d89a1ad77f92698db19b510ed9635c9380/.scratch/launcher/prototype/assets/spooky-forest.jpg) is Foggy Forest with Leafless Trees by Mathias Reding, used under the Pexels License. [Provenance and license details](https://github.com/spitefulFr0g/pt-pc/blob/a24c10d89a1ad77f92698db19b510ed9635c9380/.scratch/launcher/prototype/assets/spooky-forest-LICENSE.md) accompany the unchanged downloaded JPEG. Display cropping is CSS-only. The previously supplied promotional JPEG is removed from the current prototype assets. No game archives, executable, saves, mods, or real settings were changed.

Primary artifact: [Selected UI prototype](https://github.com/spitefulFr0g/pt-pc/blob/a24c10d89a1ad77f92698db19b510ed9635c9380/.scratch/launcher/prototype/launcher-prototype.html), default `?variant=G`, with [Run and review instructions](https://github.com/spitefulFr0g/pt-pc/blob/a24c10d89a1ad77f92698db19b510ed9635c9380/.scratch/launcher/prototype/README.md). Published snapshot: `prototype/launcher-ui-accepted`, commit `a24c10d89a1ad77f92698db19b510ed9635c9380`, retained by tag `archive/launcher-prototypes-2026-10-17`. The accepted archive contains current licensed assets without superseded photo history. It is an interaction reference; the [approved C++/Qt implementation boundary](06-launcher-boundary.md) governs production work. Do not merge the prototype branch as the implementation.

Desktop (728px) and narrow (360px) previews displayed the image without JavaScript errors. Prior focused browser reviews covered the shared settings, setup/cancel, VR-only blocker, and flat-fallback interactions. These are prototype checks, not real game or hardware validation.


Later scope extension: the user subsequently required Steam Deck Gaming Mode and controller navigation in the first Linux preview. The selected G artwork/layout remains accepted; [Design Steam Deck Gaming Mode and controller navigation](10-steam-deck-controller-ui.md) must resolve its controller/session adaptation before the map is complete. Earlier pointer/keyboard prototype review does not establish Gaming Mode support.


The Gaming Mode/controller extension is now accepted and resolved in [Design Steam Deck Gaming Mode and controller navigation](10-steam-deck-controller-ui.md). It preserves G and adds the controller/keyboard/file-selection/focus model and independent session-helper policy.
