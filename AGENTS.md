# AGENTS.md — Dota2ModHelper

Compact reference for working in this helper project.

## What this project is

`Dota2ModHelper` is a small companion executable in the same solution as `Dota2Changer`. It shares the same toolset, dependencies, and a stripped-down copy of the VPK wrapper (`VPKManager`). Currently it is just a minimal Dear ImGui + DirectX 11 window with a demo button that shows VPK cache stats.

## Build environment

- **Toolchain**: MSVC C++20, Windows SDK 10.0, `v145` toolset.
- **Build runner**: MSBuild from Visual Studio 2026 Insiders:
  `C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe`
- **Target platform**: Windows x64 only (Win32 + DirectX 11).
- **Package manager**: vcpkg. `$(VCPKG_ROOT)` must be set. Ports used: `nlohmann-json` and the same sourcepp/vcpkg link set as the main project.
- **External native libs**: prebuilt `ext/*.lib` at repo root, especially the sourcepp family. Do not delete `ext/*.lib`.

## Hard-coded external dependency

- sourcepp headers and libs are referenced by an **absolute path**:
  `F:\!CPP_Projects\sourcepp\`.
  - Includes: `F:\!CPP_Projects\sourcepp\include` plus various `ext/...` subdirs.
  - Libraries: linked from `$(ProjectDir)..\ext` (repo `ext/`), but those `.lib` files were built against the sourcepp above.
  - If that directory is missing or moved, the build breaks. There is no fallback.

## Project layout

- Solution root: `Dota2Changer.sln`.
- Helper project: `Dota2ModHelper/Dota2ModHelper.vcxproj`.
- Output directory: `Dota2ModHelper/x64/Release/bin/Dota2ModHelper.exe`.
- Local debugger working directory: `$(ProjectDir)`.

## How to build

From the repo root:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe" `
  Dota2ModHelper/Dota2ModHelper.vcxproj /p:Configuration=Release /p:Platform=x64
```

Or build through the solution:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe" `
  Dota2Changer.sln /p:Configuration=Release /p:Platform=x64 /p:ProjectName=Dota2ModHelper
```

There is no test suite, lint step, formatter, or CI. Verification == a successful Release x64 build.

## Configuration quirks

- `Debug|x64` and `Release|x64` both use `CharacterSet=MultiByte` and `SubSystem=Windows` with `EntryPointSymbol=mainCRTStartup`.
- `Win32` configurations exist but are `Unicode`/`Console`; the real target is x64.
- Preprocessor defines match the main project: `FMT_HEADER_ONLY;WIN32_LEAN_AND_MEAN;NOMINMAX;_CRT_SECURE_NO_WARNINGS` (Release adds `UNICODE;_UNICODE`).

## Source code

- `src/main.cpp` — Win32 window creation, DX11 setup, ImGui loop, cleanup, VRF startup, and Dota 2 path auto-detection.
- `src/DecompilerUI.cpp/.h` — Dear ImGui window for the VRF/Source2Viewer-CLI decompiler with all CLI options exposed.
- `src/VRF.cpp/.h` — wrapper around Source2Viewer-CLI (download, extraction, process management, compile helper).
- `src/SteamManager.cpp/.h` — Dota 2 path detection and derived paths (VPK, addons, resource compiler).
- `src/CrashLogger.cpp/.h` — non-fatal/fatal logging helpers.
- `src/DecompilerEnhancements.cpp/.h` — post-decompile enchantments for animation ordering and activity modifiers.
- `src/ModelHelper.cpp/.h` — Model Helper tab backend: `ApplyActivityModifiers` and `MergeMeshes` (KV3 text-surgery engine: copies target `.dmx` meshes into the source content dir, appends `RenderMeshFile`/`LODGroup mesh_references`/`Attachment` entries, grafts target-only bones under the nearest matching source bone).
- `src/parser/ModelDecompiler.cpp/.h` — model DATA-block decompilation and material-group parser.
- `src/parser/SkinDataManager.cpp/.h`, `Logger.cpp/.h`, `Utils.cpp/.h` — supporting parser infrastructure used by ModelDecompiler.
- `src/VPKManager.cpp/.h` — copy of the VPK wrapper from the main project. Thread-safe cache, batch API, deferred-operation deadlock guard.
- `src/PathValidator.cpp/.h` — path safety helpers.
- `src/Structures.h` — shared data types (`FileData`, `FileEntry`, `Modification`, `Replacement`, `UniversalSlot`, etc.).
- `src/Result.h` — `Result<T, E>` monad.
- `src/SecurityHardening.h` — anti-tampering helpers used by VPKManager and VRF.
- `src/GltfMorph.cpp/.h` — glTF/GLB morph-target reader (self-contained; uses nlohmann-json).
- `src/MorphMerge.cpp/.h` — injects glTF morph targets into a binary DMX mesh as `DmeVertexDeltaData` shapes + `DmeCombinationInputControl` channels + `DmeCombinationOperator`.
- `src/DmxBinary.cpp/.h` — dependency-free binary DMX (keyvalues2 binary v9 / model 22) reader/writer used by MorphMerge.
- `libs/imgui/` — bundled Dear ImGui + Win32/DX11 backends.

## VPKManager quick reference

`VPKManager` is a singleton:

```cpp
VPKManager::GetInstance()
```

Key operations:

- `CreateVPK(path, files)` — create a new VPK from a list of `FileEntry`.
- `GetFileFromVPK(vpkPath, vpkFilePath)` — read a file into `FileData`.
- `SaveFileFromVPK(vpkPath, vpkFilePath, diskFilePath)` — extract a file to disk.
- `AddFileToVPK(vpkPath, vpkFilePath, data, deferBake)` — add or replace an entry.
- `BeginBatch(vpkPath)` → `AddFileToBatch(...)` → `EndBatch()` — preferred bulk path; one `bake()` at the end.
- `ExecForEntries(vpkPath, dir, callback)` — iterate entries under a directory.
- `GetCacheStats()` — hit/miss statistics for the VPK descriptor cache.
- `ClearCache()` / `FlushCacheEntry(path)` — cache management. `ClearCache` bakes modified entries first.

Important behavior:

- The cache holds open `vpkpp::PackFile` descriptors (default max 10, LRU eviction).
- Each VPK has its own `recursive_mutex`.
- A `thread_local` flag plus deferred-operation queue prevents recursive deadlocks.
- `AddFileToVPK` checks the active batch first and redirects into it if one is open.

## Morph (facial flex) restoration

The `RestoreMorphs` enchantment (`DecompilerEnhancements.h`, flag `1 << 4`) rebuilds the facial flex system in the decompiled render-mesh `.dmx` files. Orchestrated by `ApplyMorphMergeToOutput()` in `src/DecompilerUI.cpp`.

### Pipeline

1. Export the compiled model to glTF via the CLI: `-d --gltf_export_format gltf --gltf_export_animations` into a temp dir.
2. `gltfmorph::Load()` (`src/GltfMorph.cpp`) reads every primitive that carries morph targets. Target name = POSITION accessor `name` (VRF stores the flex name there; there is **no** `mesh.extras.targetNames` in VRF 15.0 exports), then generic `morph_<i>` fallback.
3. Each render-mesh `.dmx` referenced by the `.vmdl` (`RenderMeshFile.filename`, found by regex) is matched to a glTF morph primitive by **bind-pose vertex count** (primary) and/or **mesh-name substring** (secondary). Unmatched meshes (e.g. lower LODs with no flex data) are skipped, never corrupted.
4. Per mesh: `morphmerge::StripMorphs(doc)` → `morphmerge::Merge(doc, gm, opt, log)` → `morphmerge::WireCombinationOperatorToScene(doc)` → `dmxbin::WriteFile`.
5. `Merge` emits, per morph target: one `DmeVertexDeltaData` (shape; sparse `position$0`/`normal$0` deltas + `position$0Indices`/`normal$0Indices`, threshold 1e-5) and one `DmeCombinationInputControl` (channel; `rawControlNames`, `flexMin`/`flexMax` 0..1). One `DmeCombinationOperator` references all channels (`controls`) and the mesh (`targets`); the mesh gets `deltaStates`/`deltaStateWeights(Lagged)`/`baseStates`/`bindState` patched. The operator is linked to the scene root so resourcecompiler discovers the flex controllers.

### Coordinate space — CRITICAL invariant

**The pinned Source2Viewer-CLI (VRF 15.0) exports glTF geometry — base positions AND morph deltas — already in Source space (Z-up, X-forward, Y-left, inches, unscaled).** Verified against `models/heroes/lina/lina.vmdl_c`: glTF bind positions match the DMX bind pose exactly, and named deltas are anatomically correct as-is (`eyeUp` = +Z, `eyeLeft` = +Y, `jawOpen` = −Z/−X).

Therefore `MorphMerge::Options::applyAxisTransform` **must stay `false`**. The `gltfmorph::GltfToSource()` remap `(x,y,z)→(x,z,-y)` exists only for a hypothetical exporter that bakes the Source→glTF rotation (Y-up, meters) into geometry — newer VRF source does this via `BakePositions` (rotation from `Quaternion.CreateFromYawPitchRoll(0,-90°,-90°)` + 0.0254 scale). Applying the remap to VRF 15.0 data rotates every delta −90° around the forward axis, which **permutes each flex's function** (symptom: `eyeUp` behaves as `eyeLeft`, `eyeLeft` as `eyeDown`, etc.). This was the flex-mixup bug; do not re-enable the transform unless the bundled CLI is upgraded AND its exports are re-verified.

### Shape ↔ channel pairing rules

- Shapes and channels are paired 1:1 **by name**, both derived from the same glTF morph target in the same loop, so index order always matches.
- Combination/corrective shapes (names containing `"__"`, e.g. `browLowerer__noseWrinkler`) are kept as `deltaStates` **only** — no channel; the model compiler infers the corrective rule from the `"__"` name.
- Targets with no non-zero deltas are skipped entirely (neither shape nor channel).

### Verification (no test suite)

The DMX stores sparse deltas plus vertex indices, and the bind pose lives in the same file, so a merged mesh can be validated anatomically: for each `deltaState`, average the bind positions of affected vertices and the delta direction, then check the function (e.g. `eyeUp` must average +Z near the eyes at the top of the head, `jawOpen` −Z at the jaw). This is how the Lina flex set (40 morphs) was confirmed correct after the fix.

## What to avoid

- Do not delete `ext/*.lib`.
- Do not assume cross-platform builds; this is Windows-only.
- Do not downgrade the toolset to `v143`; the project requires `v145`.
- Keep `VPKManager` in sync with the main project's copy if you change VPK behavior there.
- Do not set `MorphMerge::Options::applyAxisTransform = true` for VRF 15.0 glTF exports — it permutes every facial flex (see "Morph (facial flex) restoration").

## Merge Meshes (Model Helper tab)

`model_helper::MergeMeshes()` merges a target model into a source `.vmdl`:

- Source: a `.vmdl` on disk (must live under a `models/` folder — the content root and game dir are derived from its path).
- Target: `.vmdl` on disk (used directly), `.vmdl_c` on disk, or a `.vpk` + entry path. Compiled/VPK targets are decompiled to `temp/merge_target_*` with **all 5 enchantments** (`DecompileTargetForMerge` in `DecompilerUI.cpp`), mirroring `RunDecompileJob` (temp single-file VPK packing for disk inputs, `-f` filter, flatten, ASEQ + morph restore).
- Copies each target render mesh `.dmx` into the source game dir (unique suffix on content conflict), appends a `RenderMeshFile` block cloned from the source's first entry with the ref rewritten to `<source game dir>/<dmx filename>`.
- LODs: target mesh→LOD mapping is preserved by index (clamped to the source LOD count); a target with no `LODGroupList` puts its meshes into **all** source LOD groups. New `mesh_references` entries are cloned from the LOD's first entry.
- Attachments: appended by name (dedup), re-emitted in canonical field order with values preserved as raw text.
- Skeleton: target bones are matched to source bones **by name**; target-only subtrees are grafted under the nearest matching ancestor (canonical form: `children` first, then `origin`/`angles`/`do_not_discard = true`). If the source has no skeleton, the whole target forest is inserted.
- Re-running a merge is a no-op (meshes dedup by `RenderMeshFile` name → "No new meshes to merge").
- The source `.vmdl` (the Model Helper "VMDL source file") is modified **in place** — no separate output file.

Verified against `morphling.vmdl` + `crown_of_tears_model.vmdl`: output matches the reference merged file line-for-line except the reference tool's float32 round-trip (this implementation preserves exact value text).

## Transfer Animations (Model Helper tab)

`model_helper::TransferAnimations()` replaces source animations with "mirror" animations from a target model:

- Target selection mirrors Merge Meshes (own UI fields `transferTargetPath`/`transferTargetVpkEntry`): `.vmdl` used directly, `.vmdl_c`/`.vpk` decompiled with all 5 enchantments via the shared `PrepareTargetVmdl()` helper in `DecompilerUI.cpp` (also used by `RunMergeMeshesJob`).
- For every `AnimFile` in the source `AnimationList` (recursive through `Folder`s, reusing `ExtractAnimFiles`) a match key is built from `activity_name` + sorted `ActivityModifier` list; the same key is built for target animations (map of key → queue of indices, duplicates consumed in order).
- **Match**: the source block is replaced by the target block **verbatim** except `name` (keeps the source name, re-indented to the source depth). Referenced `source_filename` / `additional_anim_files` `.dmx` files are copied into the source game dir (same reuse/unique-suffix rules as MergeMeshes, suffix `_t<N>`) and refs rewritten.
- **No match**: the animation is renamed to `<name>_MISSMATCH` across the `AnimationList` section — but only in `name = "..."` values and bare string-array entries (`anim_order`, `blend_anim_list`, `blendList[].name`). The `IsNameKeyValue`/`IsArrayStringEntry` guards in `ModelHelper.cpp` exist because a blind quoted-token rename corrupts `activity_name` values when an animation name collides with a modifier name (real case: anim `loadout` vs modifier `loadout`). Do not "simplify" this back to a plain find/replace.
- Animations with empty `activity_name` (delta `@...`, `bindPose`) and names already ending in `_MISSMATCH` are skipped — re-running is a no-op.
- Source `.vmdl` is modified in place.

Verified bristleback ← axe: 30 transferred / 204 mismatched, second run fully idempotent, KV3 stays balanced.

## VRF / Decompiler UI

`src/DecompilerUI.cpp/.h` exposes the `Source2Viewer-CLI.exe` functionality through an ImGui window.

### How it works

1. On startup `main.cpp` ensures the VRF CLI tool is downloaded (`VRF::Setup()`) and restores or auto-detects the Dota 2 path via `SteamManager`.
2. The user selects an **input** (a model path like `models/heroes/axe/axe.vmdl` or a `.vpk` archive) and an **output directory**.
3. If the input is a model that does not exist on disk, the tool extracts the compiled `.vmdl_c` from `pak01_dir.vpk`, writes it to a unique `temp/temp_model_<id>.vmdl_c` file, runs the CLI, and deletes the temp file afterwards.
4. The CLI command is built from the options selected in the GUI (see below).
5. Output is written to `<outputDir>/<model_stem>_decompiler/`.

### Available CLI options in the GUI

| Section | GUI control | CLI flag |
|---|---|---|
| **Input / Output** | Recursive folder scan | `--recursive` |
| | Recursive into VPK archives | `--recursive_vpk` |
| | VPK extension filter | `-e` / `--vpk_extensions` |
| | VPK path filter | `-f` / `--vpk_filepath` |
| | Use cached VPK manifest | `--vpk_cache` |
| | Verify VPK checksums/signatures | `--vpk_verify` |
| **Decompile / Inspect** | Print all blocks | `-a` / `--all` |
| | Specific block | `-b` / `--block` (e.g. `DATA`, `RERL`, `REDI`, `NTRO`) |
| | Decompile supported resources | `-d` / `--vpk_decompile` |
| | Texture decode flags | `--texture_decode_flags` (`auto`, `none`, `ForceLDR`) |
| | List VPK resources | `--vpk_list` |
| | List VPK resources with metadata | `--vpk_dir` |
| **glTF export** | glTF export format | `--gltf_export_format` (`gltf`/`glb`) |
| | Export materials | `--gltf_export_materials` |
| | Export animations | `--gltf_export_animations` |
| | Mesh whitelist | `--gltf_mesh_list` |
| | Animation whitelist | `--gltf_animation_list` |
| | Adapt textures for glTF spec | `--gltf_textures_adapt` |
| | Export extra mesh properties | `--gltf_export_extras` |
| **Other** | Short tools_asset_info output | `--tools_asset_info_short` |
| | Threads | `--threads` |

### Key files

- `src/VRF.h` / `src/VRF.cpp` — download/extract/run Source2Viewer-CLI.
- `src/SteamManager.h` / `src/SteamManager.cpp` — Dota 2 path and derived paths.
- `src/parser/ModelDecompiler.h` / `.cpp` — lower-level model DATA-block decompile used by the parser backend.
- `src/parser/Utils.h` / `.cpp` — helpers such as `isModelPath()`.

### Source2Viewer-CLI reference

The options above are taken from the ValveResourceFormat CLI documentation (`command_line_guide.htm` and `vrf_file_guide.htm`).
The CLI binary is downloaded on demand from:
`https://github.com/ValveResourceFormat/ValveResourceFormat/releases/download/15.0/cli-windows-x64.zip`
into the `decompiler/` folder.

### Examples

Decompile a model's DATA block to a `.vmdl` source file:
```
-i "temp/temp_model_123456.vmdl_c" -o "decompiled_models/axe_decompiler" -b DATA -d
```

Export a model to glTF with materials and animations:
```
-i "temp/temp_model_123456.vmdl_c" -o "decompiled_models/axe_decompiler" --gltf_export_format gltf --gltf_export_materials --gltf_export_animations
```

List a VPK archive:
```
-i "game/dota/pak01_dir.vpk" --vpk_list
```
