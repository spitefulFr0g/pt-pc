# Windows Desktop export example

Read-only inspection of the `pt-pcport` folder the user offered on their Windows Desktop. The initial observations below informed the subsequently approved [installation model](issues/01-installation-and-packages.md); this file records inspection evidence, not gameplay validation.

## Observed layout

- `game/CUSA01127/` contains `chunk1.psarc` (421,978,112 bytes), `texture.qar` (892,291,044 bytes), `pathid_list_ps4.bin` (93,248 bytes), and `source.txt`.
- `archive/P.T.-CUSA01127.pkg` retains the original package (1,345,257,472 bytes).
- `runtime/linux/pt` and `runtime/windows/pt.exe` are present. Windows also has `openxr_loader.dll` and graphics SDK libraries. Presence is not an execution or compatibility check.
- `saves/` contains existing save slots. Their contents were not read.
- `mods/` is shared at the bundle root.
- `start-pt.bat` and `start-pt.sh` invoke the appropriate runtime with explicit `--game`, `--save-dir`, and `--mods` paths relative to the bundle root, use the runtime folder as working directory, and forward extra arguments.
- The bundle README describes shared game files, saves, and mods; platform-specific settings/logs/caches under `runtime/<platform>/data/`; and moving the folder as a unit. This describes intent, not tested cross-platform save compatibility.

## Checks performed

- `chunk1.psarc` starts with the expected PSARC magic.
- `texture.qar` has the expected QAR footer signature.
- The path list contains the P.T. level marker used by the setup's recognition logic.
- `source.txt` identifies US title `CUSA01127`, region US, source `pkg`. This is source metadata, not independent provenance verification.
- The retained package has PKG container magic. Its encryption/extractability was not tested; container magic alone does not establish that it is a supported fake PKG.

These match the relevant identification probes in [Shared installation code](../../installer/Native/setup_core.h), but do not establish full archive integrity or playability. No extraction, runtime execution, save migration, or VR validation was performed. No game files were changed or copied into this repository.

## Original planning questions

- An existing portable bundle is a real adoption scenario, alongside fresh setup from an extracted folder or supported fake PKG.
- The GUI may be able to replace the launch scripts while preserving the existing archive, save, and mod locations. The current source accepts their relevant command-line flags; the exact settings/data behavior still needs integration validation.
- Re-extracting the retained package should not be required merely to recognize already extracted archives.
- Decide whether to keep this shared layout and separate platform settings. Do not assume the user has approved the layout solely because they offered it as an example.
- Updates and repairs must account for existing saves/mods and the distinction between program files and player-owned data.

These questions were subsequently resolved in the linked decision tickets. Related decision: [Choose the installation and package model](issues/01-installation-and-packages.md).
