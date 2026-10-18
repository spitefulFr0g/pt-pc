# P.T. PC portable wrapper

This repository distributes a lightweight Qt launcher around the native P.T. PC
runtime by [LoreanXavier](https://github.com/LoreanXavier/pt-pc). The wrapper
handles game-data preparation, bundle inspection, and launching flat or OpenXR
VR sessions. It is not an emulator and does not include P.T. game data.

The paired runtime is pinned to upstream
[v1.0.5](https://github.com/LoreanXavier/pt-pc/releases/tag/v1.0.5), source revision
`60fe8b324aca24c264e89378ae0d2c61f3a674af`. Windows uses the checksum-pinned
released executable; Linux rebuilds the unchanged pinned engine source with
OpenXR enabled and Windows-only upscalers disabled. Each bundle records its
actual runtime provenance in `runtime-origin.json` and pairs the launcher with
the executable's SHA-256 hash. Wrapper version **0.1.0** is separate from the
upstream runtime version.

## Download and requirements

Get the Windows x64 ZIP or Linux x86_64 tar.gz from this fork's
[Releases](https://github.com/spitefulFr0g/pt-pc/releases). Choose **v0.1.0** for
the stable wrapper release; tags named `launcher-v…-preview.…` are historical
previews. Use the checksum sidecar and file inventory supplied with your chosen
release. These are portable archives, not the upstream setup installers.

You need:

- Windows 10/11 x64, or Linux x86_64 with a desktop session. Linux bundles target
  Ubuntu 24.04 (glibc 2.39); older distributions and other compositors are not
  established compatibility targets. This wrapper release has no macOS bundle.
- A Vulkan 1.3-capable GPU and compatible system graphics driver. Bundled
  libraries do not replace your graphics driver.
- Your own supported decrypted P.T. game data: a P.T. fPKG (`.pkg`) or an
  already extracted dump. The supported identities are US `CUSA01127`, European
  `CUSA01114`, and Japanese `CUSA01098`; recognition of a region does not prove
  complete gameplay compatibility. Encrypted retail/store packages cannot be
  decrypted by this application.
- A writable destination with enough free space for the extracted archives and
  temporary preparation files. Keep a backup of your game data and saves.
- For VR, a compatible installed host OpenXR runtime supporting
  `XR_KHR_vulkan_enable2`, its graphics driver, and a suitable headset. The
  wrapper does not install or select system VR services.

Qt libraries/plugins and the self-contained extraction helper are included;
no system Qt or .NET SDK is needed to run the bundle.

## Extract, prepare, and play

1. Extract the whole archive into a writable folder. Keep the launcher, platform
   runtime folder, libraries, and assets together; running inside the archive
   or copying only the executable will not work.
2. On Windows, run `pt-launcher.exe` at the archive root. On Linux, run
   `./pt-launcher` from that root in your desktop session. If extraction lost
   executable permissions, restore them with `chmod +x pt-launcher`; use an
   extractor that preserves Unix permissions for the included helper/runtime.
3. Choose **Choose .pkg and prepare** and select your supported local fPKG.
   You can also drop a package onto the window or pass its path as the
   launcher's sole command-line argument. For a decrypted dump, choose
   **Import extracted game folder**, including a dump's `CUSA01127` folder.
4. Wait for background preparation and validation to finish. Progress and
   cancellation are available. The launcher stages and checks the required
   archives before installing them into the bundle's `CUSA01127` directory.
   Errors explain unsupported, encrypted, damaged, or unrelated input.
5. Choose **Play** for flat mode or **Play VR** to request OpenXR VR. Readiness
   is checked separately for each mode, so missing VR prerequisites leave flat
   play available. An unknown result may offer **Try Play anyway** or
   **Try VR anyway**; those actions are attempts, not compatibility guarantees.

Already prepared a portable bundle? Use **Open existing portable bundle** to
select its root, with game archives under `CUSA01127`. An existing runtime may
be unverified if it does not match the paired executable hash.

Preparation leaves the original package/dump in place and preserves saves,
mods, and unrelated destination files. Interrupted preparation is recovered
when the bundle is reopened. Mutations are guarded while a game session is
active. Keep the portable directory writable and retain player data when moving
or replacing a bundle.

The upstream runtime supplies the game controls: WASD/mouse to move and look,
right mouse to zoom, left mouse/Enter/E to interact, Esc to pause, and F10 for
PC settings. The voice sequence uses a microphone; the upstream `pt.ini`
`[voice]` setting `key = J` can substitute a key.

## Included components and limits

The archive includes the launcher, paired PC runtime, Qt libraries/plugins,
platform dependencies, extraction helper, fonts/assets, voice libraries/models,
OpenXR loader where applicable, licenses/notices, provenance, and file inventory.
It excludes P.T. game archives, console executables/modules, the upstream
updater, and upstream loop preview images captured from the game. Users supply
their own game data. No separate game download is provided here.

Bundles are **unsigned** and have **no secure or automatic wrapper updater**.
Updates are manual downloads from this fork's Releases page. Extract an update
separately, preserve your existing player files, and use the launcher's import
or existing-bundle selection as appropriate; do not overwrite a running game.
Launcher flat and VR launches pass `--no-update-check`. Upstream in-game updating
cannot maintain this paired layout and is unsupported; direct runtime launches
should also pass `--no-update-check`.

Native package checks cover Windows/Linux bundle construction and extracted
launcher startup, checksums, and inventory integrity. They do not establish a
complete playthrough, physical controller behavior, or headset presentation.
Gameplay and physical headset presentation remain unverified for this wrapper
release. An OpenXR readiness probe or successful process start is not proof of
a connected headset or rendered VR; inspect the game log for fallback details.
Native Wayland, SteamOS/Hyprland/Gaming Mode, and physical Deck behavior are not
established by the desktop package checks. See [VR details](docs/vr.md).

## Source and build references

The [launcher developer notes](launcher/README.md) document the C++20 Qt Widgets
target, preparation/session contracts, acceptance commands, and historical
verification boundaries. Some sections describe earlier preview slices; use the
chosen release's bundled documentation and inventory for its exact contents.

Build the standalone launcher with CMake from `launcher/` and shared Qt 6.12.0;
this target does not configure or link the engine. The maintained
`launcher/run-acceptance.sh <evidence-directory>` command builds and runs the
scoped desktop checks (`QT_QPA_PLATFORM=offscreen` for headless checks).
[CI packaging tools](tools/ci/) assemble the paired portable distribution.

Engine build references remain available in [Linux](docs/linux.md),
[macOS](docs/macos.md), [installer](docs/installer.md),
[upscaling](docs/upscaling.md), [modding](docs/modding.md),
[Lua API](docs/lua_api.md), and [format notes](docs/formats/).
Those describe upstream engine capabilities and development workflows; macOS
and upstream installers are outside this wrapper release. See the
[upstream project](https://github.com/LoreanXavier/pt-pc) for its current releases
and engine instructions.

## Credit and license

The native port and Hallway Engine are upstream work by LoreanXavier and its
contributors. P.T. is the work of Kojima Productions and is owned by Konami.
This project is not affiliated with or endorsed by either organization.
The shadPS4 developers provided upstream with a reference for original behavior.

Project code is under the [MIT license](LICENSE). Bundled third-party components
retain their own licenses; consult the shipped notices, including Qt and the
extraction helper's LGPL material and corresponding-source information.
AI coding tools were used in developing and debugging the port and wrapper.
