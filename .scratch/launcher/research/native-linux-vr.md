# Native Linux VR prerequisites and Windows launcher checks

Research date: 2026-10-17. Source baseline: `d0a9b8f8969588a9ab2b1db03a35854d3af63f2e`, version **1.0.5**. Addresses parent ticket `04-native-linux-vr`, assigned watermelon. Investigation only; proposed changes and validation below have **not** been implemented or run. Startup billing check succeeded (exit 0): `codex/gpt-6.1-sol`, subscription route `codex/chatgpt`. No billing blocker or route switch.

## Decision for the launcher spec

**Native Linux VR is a plausible extension of the existing OpenXR/Vulkan host, but is not a verified shipping capability.** First fix Linux build/loader packaging and simulator wiring, then validate real hardware. Keep it experimental/unverified until that evidence exists. Windows is the user's current VR platform; their headset, active OpenXR runtime, graphics backend, and whether this particular game works in that setup remain unknown. Steam Link describes a transport, not the selected OpenXR runtime.

Bundle the matching downstream **game executable and dependencies**, including its platform OpenXR loader, for offline setup/play. A loader does not supply headset drivers, a compositor, or a vendor OpenXR runtime. Treat an installed/configured compatible runtime, GPU driver, and headset connection as external prerequisites. Do not choose a runtime for the user or promise that third-party first-time setup/authentication can work offline. Source compilation belongs in downstream release production, outside the normal launcher flow.

## What the repository actually does

| Area | Verified source fact | Consequence |
| --- | --- | --- |
| Build gate | `PT_OPENXR` defaults ON only on Windows. Even ON, enabling `PT_WITH_OPENXR` requires `native/x64/release/bin/openxr_loader.dll` from pinned OpenXR.Loader **1.1.63** NuGet. Failure merely warns. [Build source][repo-build] | A Linux build may contain no VR support, or compile support while staging a Windows DLL. `-DPT_OPENXR=ON` alone is insufficient. |
| Native host | With support compiled, Linux calls `dlopen("libopenxr_loader.so.1", RTLD_NOW | RTLD_LOCAL)`; Windows loads `ExecutableDir()/openxr_loader.dll`. Without support, `Available()` is false and `Init()` reports no support. [Host][repo-host] | Linux code presence is not working Linux packaging. `Available()` reports compile capability, not installed runtime/headset availability. |
| Graphics | Host requests OpenXR **1.0**, enabling exactly `XR_KHR_vulkan_enable2`, or on Windows/D3D12 `XR_KHR_D3D12_enable`. It requires two stereo views and opaque blending. Vulkan instance/device creation and GPU selection go through the runtime. [Host initialization][repo-host-init] | Require the extension for the **chosen backend**; old `XR_KHR_vulkan_enable` alone is inadequate. No generic requirement for OpenXR 1.1, OpenVR, or Steamworks follows from this code. |
| Vulkan baseline | Engine requests Vulkan 1.3 and features including dynamic rendering, synchronization2, descriptor indexing, timeline semaphores, scalar layout, BC textures and anisotropy. [GPU setup][repo-vk] | “Has Vulkan” is too weak; test the runtime-selected GPU with the engine's requirements. Ray tracing/upscalers are not blanket VR prerequisites. |
| Launch and fallback | `--vr` requests VR; `--no-vr` overrides both CLI enable and `[vr] enabled`. `Host::Init()` failure continues flat. Later session-start failure also logs and continues flat; **renderer initialization failure does not get a general flat retry**. [Startup][repo-main-start], [session start][repo-main-session] | Process success is not proof VR started. Preserve flat recovery and report the actual outcome; do not promise every VR failure falls back successfully. |
| Windows backend | Current Windows CMake enables the Rendering API choice; startup has both Vulkan and D3D12 OpenXR session paths. [Backend configuration][repo-d3d], [startup][repo-main-start] | `docs/vr.md` is behind source when it universally requires Vulkan. D3D12 runtime compatibility is a separate check. |
| Simulator | Test runtime has non-Windows exports, but CMake writes `./pt_xr_test_runtime.dll` into its manifest. Walkthrough `--vr` sets `XR_RUNTIME_JSON` to that manifest and runs headless. [Build][repo-build], [runtime][repo-sim], [walkthrough][repo-walk] | Generate the actual target filename (`libpt_xr_test_runtime.so` on ordinary Linux builds); never ship or select this simulator for play. |
| Evidence | VR guide explicitly reports simulation/validation-layer checks and no real headset. View tests exercise math; Linux test script runs flat smoke/walkthroughs without `--vr`. [VR guide][repo-guide], [view tests][repo-view-test], [Linux checks][repo-linux-checks] | Existing tests/docs do not establish physical Windows or Linux VR success. No new compile, GPU, simulator, or headset test was performed in this research. |

## Loader, runtime discovery, and platform prerequisites

**Linux discovery:** `XR_RUNTIME_JSON`, when set, overrides normal discovery; use an absolute manifest path. Otherwise search `openxr/1/` under user config, ordered XDG config directories, then the loader's global config directory. In each directory, `active_runtime.x86_64.json` precedes `active_runtime.json`; symlinks are allowed. **Windows x64:** inspect string value `HKLM\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime` unless overridden. Its manifest names the runtime library; a relative pathname resolves against the manifest, a bare filename through library search. A valid manifest does not prove its runtime works. [Khronos loader rules][loader-rules]

XDG defaults are `$HOME/.config` and `/etc/xdg`. The pinned loader next checks compiled-in `SYSCONFDIR`, then optional `EXTRASYSCONFDIR` (upstream adds `/etc` when the first differs). Respect the child's actual environment and architecture. [XDG specification][xdg], [pinned loader implementation][loader-code], [loader build][sdk-loader]

**Runtime candidates, not selections:** Monado advertises `XR_KHR_vulkan_enable2`, but its headset/controller tracking support varies by device and driver. Its service configuration, device permissions/udev rules, Vulkan driver and compositor also matter. SteamVR is another candidate; Valve requires Vulkan drivers on Linux. Query the installed runtime rather than maintaining a guessed compatibility allowlist. [Monado capabilities/hardware][monado], [Monado setup][monado-setup], [Valve Linux requirements][valve-linux]

The OpenXR Vulkan extension is distinct from **Vulkan device extensions** needed by a runtime. For example, Monado documents Linux external-memory/semaphore FD extensions, with additional required/optional extensions for compositor paths. The engine uses `xrCreateVulkanInstanceKHR`, `xrGetVulkanGraphicsDevice2KHR`, and `xrCreateVulkanDeviceKHR`; preserve this integration instead of substituting the launcher's preferred GPU. A generic `vulkaninfo` result may describe a different adapter. [Vulkan binding][xr-vulkan], [GPU selection][xr-gpu], [Monado Vulkan requirements][monado-vk]

The host checks Vulkan graphics requirements, but only **warns** if the runtime's maximum tested version is below 1.3; it does not reject it. That maximum is not necessarily a hard incompatibility ceiling. Record the minimum and maximum-tested versions and the subsequent creation outcome. [Host][repo-host-init], [Khronos requirements query][xr-vk-requirements]

**Windows and Steam Link:** require the packaged x64 loader, the runtime's installed drivers/service, a working headset connection and the chosen backend's extension. Vulkan needs the same engine GPU capabilities; D3D12 needs `XR_KHR_D3D12_enable` and the runtime's adapter LUID/minimum feature-level requirements plus the engine's own D3D12 requirements. Do not force Vulkan just because the VR guide says so. [Host][repo-host-init], [D3D12 extension][xr-d3d], [startup][repo-main-start]

For a user-selected SteamVR/Steam Link setup the conceptual path is game → OpenXR loader → active runtime (SteamVR if selected) → Steam Link transport → headset. Valve's FAQ describes streaming through a PC running Steam/SteamVR and lists headset/network requirements. Its indexed FAQ currently lists Windows 10+, while Valve's newer **SteamVR 2.17** notes explicitly describe Steam Link fixes on Linux. Therefore do not turn an older Windows-only statement into a claim that Linux streaming is impossible, or those fixes into proof this game/headset works. Confirm installed versions and test the actual route. [Valve FAQ][valve-link], [Valve 2.17 release][valve-217] (release body also inspected through Valve's [public news API][valve-news-api]).

Do not reuse the guide's runtime-name list as a current support guarantee: Microsoft removed Windows Mixed Reality in Windows 11 24H2; its published legacy Steam-device continuation is conditional on 23H2 and runs through November 2026. Ask about OS/runtime if the user's headset depends on WMR. [Microsoft removal notice][wmr]

## Smallest plausible work outside the launcher

These are implementation recommendations inferred from the source, not completed fixes.

1. **Split `cmake/OpenXR.cmake` by target platform.** Preserve Windows pinning; on Linux acquire pinned OpenXR headers and a real Linux x86-64 loader. Upstream OpenXR-SDK supplies generated sources and a shared loader target (`BUILD_LOADER=ON`, `DYNAMIC_LOADER=ON`; keep layers/tests off for production). Enable `PT_WITH_OPENXR` only with validated target headers/loader. Make requested downstream VR release builds fail if support cannot be supplied, instead of silently losing it. Explicitly pass `PT_OPENXR=ON` in production configuration, including reused CMake caches/cross builds. End users compile nothing. [Existing build][repo-build], [upstream SDK][sdk], [loader target][sdk-loader], [cross build][repo-cross]
2. **Package the Linux SONAME and resolve it reliably.** Stage `libopenxr_loader.so.1` with its needed files/notices. `tools/package.py` copies root DLLs and selected Linux upscaler libraries, not arbitrary `.so` files; add an explicit XR runtime-file list. Just copying adjacent to `pt` does not satisfy a bare-name `dlopen`. Prefer an executable-relative load matching Windows (small Linux host edit), or configure `$ORIGIN` RUNPATH for the calling executable and test it. Build against the downstream ABI baseline; audit the loader's own dependencies, not just `pt`'s `DT_NEEDED`. Preserve licenses and inventory/hashes. [Host][repo-host], [packager][repo-package], [CMake staging][repo-cmake], [SDK loader][sdk-loader], [Linux loader search rules][dlopen]
3. **Repair simulator manifest generation and add Linux VR validation.** Use the built target's real filename, keep the simulator in its test-only subdirectory, and retain process-local `XR_RUNTIME_JSON` for tests. Extend Linux checks with explicit VR simulator runs and verify logs say VR actually started. Current flat-only checks can pass with the XR stub. [Build][repo-build], [walkthrough][repo-walk], [Linux script][repo-linux-checks]
4. **Expose truthful bundle capability.** Prefer generated, executable-bound release metadata recording OS/arch/build ID, XR compiled support, loader version/hash and backend support. File presence alone cannot distinguish `PT_WITH_OPENXR` from the stub. Optional small diagnostics entry point/helper can reuse host checks and emit structured outcomes; no rendering redesign is needed to begin. A full engine diagnostics mode is not required for initial static checks. [Stub/host][repo-host], [existing hashed file inventory][repo-package]
5. **Update the downstream VR guide after validation.** Describe platform/backend prerequisites and tested combinations separately. Keep source/packaging success distinct from simulation and physical validation. Also reconcile the approved explicit downstream-update policy: baseline engine defaults still point to upstream release metadata and can start automatic checks. That is an adjacent launcher/spec requirement, not a Linux VR prerequisite. [Version/update endpoint][repo-cmake], [startup update check][repo-main-start]

## Readiness checks and their limits

Proposed labels: **Build supports VR**, **Prerequisites found**, **Headset available now**, **VR session started**, and separately **Physically verified on this setup**. Unknown is a valid result. Cache evidence with timestamp/backend/runtime/build identity; recheck after environment/runtime changes.

| Check | Can happen before game launch? | Display/GPU/headset dependency and limits |
| --- | --- | --- |
| Bundle OS/architecture, executable-bound capability, loader/notices/hash, manifests and library-path readability | Yes, static | No display/GPU/headset. Check symlink targets and architecture; manifests can be stale. Windows runtime and Linux ELF compatibility differ. |
| Load the **same loader** the game will use; resolve entry points | Yes, separate short-lived probe | No graphics session inherently required. OS/ABI/transitive-library failures still possible. `ldd pt` misses its optional `dlopen` dependency. Never use the test runtime as fallback. |
| `xrEnumerateInstanceExtensionProperties`, then `xrCreateInstance` with the selected binding and `xrGetInstanceProperties` | Yes, active probe | No app window/session inherently required, but runtime implementation may start services or need GPU/display/device access. Enumeration is for the effective runtime/layers; extension presence is not headset availability. Bound process duration and report actual `XrResult`. [Enumeration][xr-enumerate] |
| `xrGetSystem(HMD)`, stereo/opaque checks, selected binding's graphics requirements | Yes, active probe | Requires a runtime exposing an available headset system; simulation can also return one. No game-rendered window is inherently needed, but a real runtime can depend on compositor/GPU/display/transport. `XR_ERROR_FORM_FACTOR_UNAVAILABLE` is current unavailability, not proof of missing installation or a specific cable fault. [System query][xr-system], [host][repo-host-init] |
| Runtime-selected GPU capability and device creation | Yes, with a deeper probe | Actual compatible graphics driver/device required; software Vulkan can test some simulation paths but proves no physical output. Session/swapchain probing has increasing side effects and duplicates startup unless shared with engine. [Runtime GPU query][xr-gpu], [engine][repo-vk] |
| Session creation, READY → begun session, renderable frames, swapchains and controller actions | Normally confirm during launch; optional explicit test | `Host::Ready()` only means instance+system, not session success. Normal game creates an SDL display window even for VR; `--headless` skips it but still renders on a graphics device. Session creation alone does not prove focused/visible output. [Startup][repo-main-start], [session lifecycle][repo-session], [OpenXR session][xr-session] |
| Correct stereo, tracking, controllers/haptics, scale/height, HUD/cutscenes, comfort, frame pacing, audio/stream quality | Needs real launch and user/headset | Physical test on the exact OS/GPU/driver/runtime/headset/controller/transport. Screenshots, simulator, and an API success cannot certify these. Record user confirmation and measured logs separately. |

Run active checks in a disposable child with the game's environment and architecture; they can activate runtime services. Default to the system's selected runtime. If the user explicitly chooses a manifest override, pass absolute `XR_RUNTIME_JSON` only to probe/game children and show its precedence over any inherited value. Do not edit registry/global symlinks automatically. Preserve an inherited override visibly rather than quietly replacing it. Failed/timeout checks should retain detail and leave an explicit flat launch available; `--no-vr` is the existing reliable opt-out.

## Required validation matrix (all pending)

| Configuration | Required evidence | What a pass establishes |
| --- | --- | --- |
| Linux XR OFF; Linux XR ON; Windows XR ON | Fresh production builds; capability metadata agrees with compile definitions; no silent stub; package inventory | Build/package correctness only. |
| Clean Linux portable install without system OpenXR loader | Packaged SONAME resolves from a different working directory, no development `LD_LIBRARY_PATH`, no build-tree dependencies; inspect loader dependencies | Offline loader packaging, not vendor runtime availability. |
| No runtime; broken override/manifest; runtime without HMD; missing binding extension; runtime stopped | Probe error categories, `pt.log`, `--vr` fallback or clear renderer failure; `--no-vr` flat launch | Negative paths and truthful UI. Repeat Windows and Linux. |
| Linux simulator + Vulkan; Windows simulator + Vulkan/D3D12 | Fixed manifest; explicit VR start/session logs, walkthrough assertions, no simulator `VIOLATION`; OpenXR and graphics validation where available | Simulated API/rendering coverage only. Headless still needs a suitable Vulkan/D3D12 device. |
| Linux physical runtime/headset combination selected by user/tester | Runtime identity, GPU/driver, display session (X11/Wayland separately where supported), headset/controllers, transport; session/frame logs plus in-headset checklist | Compatibility only for recorded setup. Include startup with runtime stopped/running, focus loss, disconnect/reconnect, and flat recovery. |
| User's current Windows VR route, each offered graphics backend | Same identity record; active runtime and overrides; if Steam Link, its actual connection established; stereo/input/pacing/exit tests | Windows evidence without inferring Linux support or another runtime. |
| Offline installed bundle and explicit downstream update | Network unavailable with prerequisites already provisioned; setup and play succeed; no unsolicited upstream replacement | Approved distribution behavior, not offline installation of every vendor ecosystem. |

No physical headset evidence was available here. Unit-test or simulator success must never mark the physical rows passed. WSL/Proton runs do not substitute for native Linux display/headset validation.

## Questions the parent must resolve with the user

- Exact headset/controllers; cable, Steam Link or another transport; whether this game has already run in VR.
- Windows version/build, GPU/driver, active runtime name/version and any `XR_RUNTIME_JSON`; selected Vulkan/D3D12 backend.
- If Steam Link: headset/host app versions and connection method; verify SteamVR is actually the selected OpenXR runtime.
- Intended Linux machine/distro, GPU/driver, X11/Wayland, headset/transport and runtime **they choose**; who can perform physical validation. Currently Linux remains a planned target they cannot validate.

## Primary sources

Repository citations below pin `d0a9b8f`; external pages were checked on the research date. Khronos API manual pages may track a newer patch than bundled loader 1.1.63; loader-specific rules/source are pinned to 1.1.63. Valve FAQ full body did not render in this environment; its indexed primary-source excerpts were available. Valve release text was verified through its public news API. No secondary-source assertions are relied on.

[repo-build]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/cmake/OpenXR.cmake
[repo-host]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/src/engine/xr/xr_host.cpp
[repo-host-init]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/src/engine/xr/xr_host.cpp#L258-L422
[repo-session]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/src/engine/xr/xr_host.cpp#L549-L840
[repo-vk]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/src/engine/render/vk_context.cpp#L190-L490
[repo-main-start]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/src/main.cpp#L7270-L7415
[repo-main-session]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/src/main.cpp#L7602-L7628
[repo-d3d]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/cmake/D3D12.cmake
[repo-sim]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/tools/xr_test_runtime/xr_test_runtime.cpp
[repo-walk]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/tools/walkthrough.py#L2338-L2385
[repo-guide]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/docs/vr.md
[repo-view-test]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/tests/vr_view_test.cpp
[repo-linux-checks]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/tools/linux/run_tests.sh
[repo-cross]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/tools/linux/cross_build.sh
[repo-package]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/tools/package.py
[repo-cmake]: https://github.com/spitefulFr0g/pt-pc/blob/d0a9b8f8969588a9ab2b1db03a35854d3af63f2e/CMakeLists.txt
[loader-rules]: https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/release-1.1.63/specification/loader/runtime.adoc
[loader-code]: https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/release-1.1.63/src/loader/manifest_file.cpp
[xdg]: https://specifications.freedesktop.org/basedir/latest/
[sdk]: https://github.com/KhronosGroup/OpenXR-SDK/blob/release-1.1.63/README.md
[sdk-loader]: https://github.com/KhronosGroup/OpenXR-SDK/blob/release-1.1.63/src/loader/CMakeLists.txt
[xr-vulkan]: https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_vulkan_enable2.html
[xr-vk-requirements]: https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrGetVulkanGraphicsRequirements2KHR.html
[xr-gpu]: https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrGetVulkanGraphicsDevice2KHR.html
[xr-d3d]: https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_D3D12_enable.html
[xr-enumerate]: https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrEnumerateInstanceExtensionProperties.html
[xr-system]: https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrGetSystem.html
[xr-session]: https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrCreateSession.html
[monado]: https://monado.freedesktop.org/
[monado-setup]: https://monado.freedesktop.org/getting-started.html
[monado-vk]: https://monado.pages.freedesktop.org/monado/vulkan-extensions.html
[valve-linux]: https://github.com/ValveSoftware/SteamVR-for-Linux#graphics-driver-requirements
[valve-link]: https://help.steampowered.com/en/faqs/view/0E2C-406B-9135-38A4
[valve-217]: https://store.steampowered.com/news/app/250820/view/679635225181946198
[valve-news-api]: https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=250820&count=100&maxlength=0
[wmr]: https://learn.microsoft.com/windows/whats-new/removed-features
[dlopen]: https://man7.org/linux/man-pages/man3/dlopen.3.html
