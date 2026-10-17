# Additional launcher designs

Brief: add two substantially different choices that are easy to understand, retaining the agreed optional GUI over the existing portable bundle. These are variants D and E of the same throwaway study; neither chooses the production toolkit or changes the game.

## D: Compact

- Palette: ink `#202939`, paper `#F7FAFD`, ice `#E7EEF5`, blue `#315F94`, slate `#56667A`, warning `#8A5D17`.
- Type: Franklin Gothic Medium/Arial Narrow for the small P.T. title; Tahoma/Verdana for readable desktop controls. Native fallbacks keep the artifact offline and self-contained.
- Layout: a narrow, left-aligned single-column utility. Choose display or headset with a radio control, read the relevant readiness message, then use one primary launch button. Secondary tasks stay in a small footer.

```text
+-----------------------------------+
| P.T.                   Windows    |
| pt-pcport                 Change  |
|                                   |
| Where would you like to play?      |
| (o) On my display                 |
| ( ) In my headset                 |
| Relevant readiness + recovery     |
| [             Play              ] |
| Settings   Game files   Updates   |
+-----------------------------------+
```

- Principle: the mode choice determines one next action, with no requirement to interpret an installation dashboard.

## E: Two choices

- Palette: ink `#243753`, mist `#EEF0F6`, white `#FFFFFF`, violet `#6553A4`, slate `#617187`, warning `#8B5E20`.
- Type: Trebuchet MS/Aptos for approachable, open shapes, with a clear 30/20/14px hierarchy and sentence-case labels.
- Layout: a wide, left-aligned choice sheet. Two full-width action rows use simple monitor/headset symbols and explain what each choice does. A short settings summary sits below; there is no sidebar or sequence to complete for everyday play.

```text
+------------------------------------------+
| P.T.                         Windows     |
| Choose how to play                       |
|                                          |
| [display] Play on my display     Ready    |
|           Use your usual controls.       |
|------------------------------------------|
| [headset] Try VR anyway       Not checked |
|           Quest 3 through Steam Link     |
|                                          |
| Windowed, 1080p          Change settings  |
| Game files                      Updates  |
+------------------------------------------+
```

- Principle: each row is the action itself. Clear icons and ordinary language explain display versus headset play without a separate mode form.

## Review before building

The initial idea included a decorative doorway graphic and prominent runtime metadata. Remove both: they use space needed for the launch choice and recreate the visual emphasis of the previous cinematic/dashboard variants. Keep the subject-specific identity in the P.T. title, display/headset distinction, and the user's portable bundle. D's single-action form and E's two-action list must differ structurally, not merely in palette.

Both variants retain setup, extraction/cancellation, settings, update, and fallback simulations. Technical/runtime details belong to repair explanations or the study's state readout rather than the normal play screen. Reuse the earlier in-memory behavior; add no real file, network, or game-process operations. Review screenshots at desktop and narrow widths before publishing.

## Critique after screenshots

Reviewed D and E at 728px and 360px. Removed D's redundant frame around the Change link. Replaced E's sentence containing an oversized inline retry button with a short status line and a separate Check VR action, avoiding awkward wrapping and an orphaned full stop. Kept each layout's distinct decision structure, the shared prototype controls outside the product surface, and clear keyboard focus. Focused browser interactions confirmed the mode chooser, settings navigation, VR-only blocking, and in-place adoption simulations; no real file or hardware behavior was exercised.
