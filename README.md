# Dota2ModHelper

A Windows desktop companion tool to [Dota2Changer](https://github.com/lighthurtsmyeyes/openchanger) for working with Dota 2 Source 2 assets. Built with Dear ImGui + DirectX 11, C++20, MSVC.

## Features

- **Decompiler tab** — a full GUI frontend for [Source2Viewer-CLI](https://github.com/ValveResourceFormat/ValveResourceFormat) (VRF 15.0):
  - Automatic download and setup of the CLI on first run.
  - Every CLI option exposed in the UI: decompile (`-d`), block inspection (`-b DATA/RERL/REDI/NTRO`), VPK listing, glTF export (materials, animations, texture adaptation), recursive scans, VPK filters, threading.
  - If the input model does not exist on disk, its compiled `.vmdl_c` is pulled from `pak01_dir.vpk` automatically and decompiled from a temp file.
  - Post-decompile **enchantments**: ASEQ animation ordering, activity modifiers, and facial flex (morph) restoration — glTF morph targets are injected back into binary DMX meshes as `DmeVertexDeltaData` shapes with a fully wired `DmeCombinationOperator`.
- **Model Helper tab**:
  - `MergeMeshes` — merges a target model (`.vmdl`, `.vmdl_c`, or straight from a `.vpk`) into a source `.vmdl`: render meshes are copied over, `RenderMeshFile`/`LODGroup`/`Attachment` entries appended, and target-only bones grafted into the source skeleton by name. Re-running a merge is a no-op.
  - `ApplyActivityModifiers` — applies activity modifier metadata to decompiled animations.
- **VPK manager** — thread-safe VPK read/write wrapper with an LRU descriptor cache and a batch API (shared with, and kept in sync with, the main Dota2Changer project).
- **Steam integration** — automatic Dota 2 path detection (game dir, `pak01_dir.vpk`, addons, resourcecompiler).
- **Crash logging** — non-fatal/fatal log helpers for diagnosing field issues.

## Requirements

- Windows x64 only (Win32 + DirectX 11).
- MSVC **v145** toolset, C++20 (built with Visual Studio 2026 Insiders MSBuild).
- [vcpkg](https://vcpkg.io) with `$(VCPKG_ROOT)` set; port used: `nlohmann-json`.
- A [sourcepp](https://github.com/craftablescience/sourcepp) checkout at `F:\!CPP_Projects\sourcepp` — include and library paths are **hard-coded** to this location. If it lives elsewhere, update the project file.
- Prebuilt static libraries (`sourcepp` family, minizip-ng, libwebp, bzip2) in `ext/` (a copy is tracked in this repo; the build links against `..\ext` relative to the project file).

## Build

From the repository root:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe" `
  Dota2ModHelper.vcxproj /p:Configuration=Release /p:Platform=x64
```

Or via the shared solution one level up (`../Dota2Changer.sln`), which also contains the main Dota2Changer project.

Output: `x64/Release/bin/Dota2ModHelper.exe`

There is no test suite — verification is a successful Release x64 build.

## Project layout

| Path | Description |
|---|---|
| `src/main.cpp` | Win32 window, DX11 setup, ImGui loop, VRF startup, Dota 2 path detection |
| `src/DecompilerUI.*` | ImGui window exposing all Source2Viewer-CLI options; orchestrates decompile jobs and morph merge |
| `src/VRF.*` | Download, extraction and process management for Source2Viewer-CLI |
| `src/SteamManager.*` | Dota 2 path detection and derived paths |
| `src/ModelHelper.*` | Merge Meshes / activity-modifier backend (KV3 text-surgery engine) |
| `src/DecompilerEnhancements.*` | Post-decompile enchantments (ASEQ, activity modifiers, morph restore) |
| `src/GltfMorph.*` | glTF/GLB morph-target reader (nlohmann-json) |
| `src/MorphMerge.*` | Injects glTF morph targets into binary DMX as flex shapes/channels |
| `src/DmxBinary.*` | Dependency-free binary DMX (keyvalues2 v9 / model 22) reader/writer |
| `src/VPKManager.*` | Thread-safe VPK wrapper with LRU cache and batch API |
| `src/parser/` | Model DATA-block decompiler, skin data manager, supporting utilities |
| `libs/imgui/` | Bundled Dear ImGui + Win32/DX11 backends |
| `ext/` | Prebuilt native libraries (sourcepp family etc.) |
| `command_line_guide.txt` | Source2Viewer-CLI reference the Decompiler UI is modeled after |

## Notes

- The facial-flex restoration assumes the pinned VRF 19.2 CLI, which exports glTF geometry already in Source coordinate space. `MorphMerge::Options::applyAxisTransform` must stay `false` — enabling it permutes every facial flex. See `AGENTS.md` for the full invariant list.
- `VPKManager` is a copy of the main project's wrapper; keep the two in sync when changing VPK behavior.
- `AGENTS.md` contains a compact deep-dive into the internals (VPK cache semantics, morph pipeline, merge rules) intended for AI assistants and contributors.

## License

See `LICENSE.txt`.
