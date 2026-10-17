# Horror-inspired launcher directions

Brief: two further simple designs inspired by Silent Hill, horror-game menus, and haunted houses. These are F and G in the existing throwaway study. Preserve the original controls, portable bundle, and simulated behavior; the aesthetic does not authorize changes to the game.

## F: Fog menu

- Color: fog `#DEE2D9`, paper `#EEF0E8`, ink `#26302D`, distant trees `#89918A`, iron `#59645E`, old oxide `#6C352E`.
- Type: Georgia/Times New Roman for a large, slightly weathered P.T. title; Verdana/Tahoma for legible menu actions and explanations. The title carries the atmosphere; action labels stay ordinary.
- Layout: a classic title-screen composition, left-aligned text menu against an original vector illustration of a deserted road, bare trees, distant houses, and heavy fog. Keep controls on the pale, quiet part of the scene; keep the illustration strongest on the right.

```text
+-------------------------------------------+
| P.T. launcher                     Windows |
|                                           |
| P.T.                  fog, bare trees     |
|                       distant houses      |
| Play on my display                        |
| Try VR anyway                             |
|                                           |
| Settings                                  |
| Game files                                |
| Updates                                   |
|                                           |
| Display ready. VR needs a check.           |
+-------------------------------------------+
```

- Principle: borrow the quiet unease and simple hierarchy of a horror title screen. The landscape supplies the mood; typography and clear labels supply usability.

## G: The foyer

- Color: charred wood `#2A241F`, old wallpaper `#45483A`, aged paper `#D9D2BC`, lamplight `#D8B573`, iron `#8F9488`, warning `#B87E67`.
- Type: Palatino Linotype/Book Antiqua for the small game identity; Trebuchet MS/Tahoma for the control panel. Use a restrained 42/24/15px hierarchy.
- Layout: a split entryway: an original vector drawing of a worn hallway and half-open door on the left, a simple solid control panel on the right. Two plainly named launch actions sit above the setup/settings tools. On narrow screens the illustration becomes a short atmospheric strip above the controls.

```text
+-------------------------------------------+
|                 | P.T.                    |
| worn wallpaper  | Play P.T.                |
|                 | Your game is ready.     |
| half-open door  | [Play on my display]     |
|                 | [Try VR anyway]         |
| lamplight       | VR check explanation    |
|                 | Settings  Files Updates |
+-------------------------------------------+
```

- Principle: the house feels tangible while the controls remain a familiar utility. The illustration is decorative, never an unlabeled launch target.

## Review before building

An initial red/black treatment and flickering lamp would repeat common horror-interface shortcuts and impair readability. Revise F to daylight fog with dark, calm text; revise G to wood, wallpaper, and steady lamplight. Use original code-native vector scenes rather than copying game artwork. Keep texture low-contrast and confined away from controls. No automatic motion, sound, hidden actions, fake error messages, or ambiguous buttons such as Enter the nightmare.

The two layouts must disagree structurally: F is a sparse full-scene menu, while G separates the illustration from a solid control panel. Existing setup/settings/update/fallback simulations stay reachable. Real hardware results remain unverified, including the user's active OpenXR runtime.

## Critique after screenshots

Reviewed both directions at 728px and 360px. F's inherited boxed tools initially broke the title-menu composition; replaced them with a plain stacked menu and shortened its readiness copy. Kept the fog strongest around the distant architecture, with pale space behind text. G's illustration becomes a cropped strip at narrow widths, and hides while editing settings/preparing files so those tasks get the full control width. All illustrations are static and decorative. Added clear F/G comparison buttons outside the product mockup, leaving earlier layouts in the selector. Browser interactions confirmed settings access, preparation cancellation, VR-only blocking, and preserving a simulated flat fallback without JavaScript errors.

## Selected revision

The user chose G with the supplied P.T. forest photograph in place of the original foyer illustration, asked for the flat button to stay Play, and authorized matching colors. Keep the split layout; use the unchanged JPEG as a local image asset. Its focal position preserves the stump/P.T. mark in the tall desktop crop and in the bottom-aligned narrow strip.

Revised tokens: evergreen base `#17231D`, forest surface `#22342A`, stone text `#E7E9DF`, muted foliage `#B6C1B3`, sage action `#BCCCA6`, and edge `#4B6251`. Keep the selected typography and interaction layout. Final desktop/narrow previews showed the image loaded and the P.T. mark visible. No new hardware results or production behavior are implied by the revision.

The next user refinement replaced the title type with Pirata One, a locally bundled gothic display face, while preserving readable body/control type. Removed routine ready/running prose beneath the title, leaving actionable unknown/failure messages and disabled-action state. The title now has a deliberate gap before Play. Reviewed at desktop and narrow widths; no animation or additional decorative text was introduced.


## Licensed forest replacement

The user requested a royalty-free spooky forest instead of the supplied promotional image. Use Mathias Reding's Foggy Forest with Leafless Trees from Pexels, with source, license URL, download date, and checksum in `assets/spooky-forest-LICENSE.md`. Keep the approved green palette, gothic title, controls, and split composition. The image's tangled branches carry the horror atmosphere without extra decoration or text. Center the portrait photograph on desktop; show its upper-middle branches in the narrow strip. The old promotional JPEG is removed from current assets; earlier archival commits retain the historical study.

Replacement-image review: desktop (728px) and narrow (360px) previews loaded without JavaScript errors. Browser checks confirmed the 1600 × 2400 local photo, loaded gothic font, stable Play label, absent routine ready/running copy, and no horizontal overflow.
