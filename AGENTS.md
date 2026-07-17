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
- `src/ClothSimRestore.cpp/.h` — `Restore Cloth Sim` enchantment: parses the `PHYS` block's `m_pFeModel` and injects `ClothShapeSphere` / `ClothShapeCapsule` / `ClothShapeBox` / `ClothShapeSDF` nodes into the decompiled `.vmdl`, rebuilds the cloth proxy mesh `.dmx` (stage 2), and restores legacy `$cc` jiggle chains as a generated cloth proxy + `ClothVertexMap`/`ClothEffectStiffen` nodes (stage 3).
- `src/ModelHelper.cpp/.h` — Model Helper tab backend: `ApplyActivityModifiers` and `MergeMeshes` (KV3 text-surgery engine: copies target `.dmx` meshes into the source content dir, appends `RenderMeshFile`/`LODGroup mesh_references`/`Attachment` entries, grafts target-only bones under the nearest matching source bone).
- `src/parser/ModelDecompiler.cpp/.h` — model DATA-block decompilation and material-group parser.
- `src/parser/SkinDataManager.cpp/.h`, `Logger.cpp/.h`, `Utils.cpp/.h` — supporting parser infrastructure used by ModelDecompiler.
- `src/VPKManager.cpp/.h` — copy of the VPK wrapper from the main project. Thread-safe cache, batch API, deferred-operation deadlock guard.
- `src/PathValidator.cpp/.h` — path safety helpers.
- `src/Structures.h` — shared data types (`FileData`, `FileEntry`, `Modification`, `Replacement`, `UniversalSlot`, etc.).
- `src/Result.h` — `Result<T, E>` monad.
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

## Restore Cloth Sim (soft body collision shapes) — stage 1

The default Source2Viewer-CLI decompiler dumps the `PHYS` block as raw KV3 but does **not** turn it into a modeldoc `Softbody` node. Stage 1 of the `Restore Cloth Sim` enchantment bridges that gap by extracting the embedded cloth/soft-body collision rigids and emitting the corresponding `ClothShapeSphere` / `ClothShapeCapsule` children.

### Where the data lives

Inside the compiled model's `PHYS` block, under `m_pFeModel`:

- `m_CtrlName` — control/particle names in node order. Real skeleton bones appear here (e.g. `pelvis`, `spine_0`, `leg_upper_L`) alongside cloth particles (`$cloth_m0p*`).
- `m_SphereRigids` — sphere colliders.
- `m_TaperedCapsuleRigids` — tapered capsule colliders.
- `m_BoxRigids` / `m_SDFRigids` — reserved for later stages (empty in the current reference model).

### Mapping a rigid to its parent bone

Each rigid has an `nNode` field. It is an index into `m_CtrlName`:

```
m_TaperedCapsuleRigids[0].nNode = 7  -> m_CtrlName[7] = "leg_upper_L"
m_SphereRigids[0].nNode      = 21 -> m_CtrlName[21] = "pelvis"
```

Entries that resolve to names starting with `$cloth_` (cloth simulation particles, not bones) are skipped for now.

### Shape formats

Sphere (`FeSphereRigid_t` / `ClothShapeSphere`):

```
vSphere = [ x, y, z, radius ]
```

Capsule (`FeTaperedCapsuleRigid_t` / `ClothShapeCapsule`):

```
vSphere = [
    [ x0, y0, z0, radius0 ],
    [ x1, y1, z1, radius1 ],
]
```

These values are already in the parent bone's local space, so they can be written verbatim into `center` / `point0` / `point1`.

### Collision mask

`nCollisionMask` is a bitmask. ModelDoc uses four booleans:

```
cloth_collision_layer0 = (mask & 1) != 0
cloth_collision_layer1 = (mask & 2) != 0
cloth_collision_layer2 = (mask & 4) != 0
cloth_collision_layer3 = (mask & 8) != 0
```

The reference model uses mask `15`, so all four layers are `true`.

### Target modeldoc structure

A new `Softbody` child is added to the `rootNode` after the `Skeleton` block:

```kv3
{
    _class = "Softbody"
    children = [
        { _class = "ClothShapeSphere", name = "pelvis_clothSphere", parent_bone = "pelvis", ... radius = 16.0, center = [ ... ] },
        { _class = "ClothShapeCapsule", name = "leg_upper_L_clothCapsule", parent_bone = "leg_upper_L", ... radius0 = 10.0, radius1 = 11.0, point0 = [ ... ], point1 = [ ... ] },
        ...
    ]
    stiffness_on_ragdoll = 0.0
    motion_smooth_cdt = 0.0
    cloth_sleep_enabled = false
    cloth_immovable_hint = false
    cloth_per_bone_scale_enabled = false
    cloth_enable_empty_model = false
    cloth_keychain_motion = false
}
```

### Verification for stage 1

Compare the generated list of `ClothShapeSphere` / `ClothShapeCapsule` blocks against the hand-restored reference model (`invoker_kid_dark_artistry_back.vmdl`). The values and parent-bone assignments should match exactly for `invoker_kid_dark_artistry_back.vmdl_c`.

## Restore Cloth Sim — stage 2 (cloth proxy mesh reconstruction)

Stage 2 rebuilds the cloth proxy mesh `.dmx` (the thing `ClothProxyMeshFile` points at) from the `PHYS` block, so a decompiled model can be recompiled with working cloth. Reverse-engineered from two reference pairs (QoP arcana `queenofpain.vmdl` + `tail_proxy.dmx` + `queenofpain.vmdl_c`; Drow arcana `drow_base.vmdl` + `proxy_mesh_cape.dmx` + `drow_base.vmdl_c`) plus controlled `resourcecompiler.exe` experiments in a sandbox addon (`content\dota_addons\clothlab\`).

Reference pairs live under `D:\Apps\Steam\steamapps\common\dota 2 beta\`:
- `content\dota_addons\queen_of_pain_arcana\models\items\queenofpain\queenofpain_arcana\src\tail_proxy.dmx` ↔ compiled `game\dota_addons\queen_of_pain_arcana\models\heroes\queenofpain\queenofpain.vmdl_c` (sources: `content\...\models\heroes\queenofpain\queenofpain.vmdl`)
- `content\dota_addons\traxexexexex\models\items\drow\drow_arcana\proxy_mesh_cape.dmx` ↔ `game\dota_addons\traxexexexex\models\heroes\drow\drow_base.vmdl_c`

### Experiment setup (reusable)

- `resourcecompiler.exe` at `<dota>\game\bin\win64\resourcecompiler.exe`, invoked `-i <content path.vmdl>`; input must be under `content\dota_addons\<addon>\`, output mirrors into `game\dota_addons\<addon>\`. It **skips up-to-date rebuilds** — delete the `.vmdl_c` to force a rebuild.
- PHYS text dump for study: `Source2Viewer-CLI.exe -i model.vmdl_c -b PHYS > phys.txt` (KV3 text, junk header lines before the root `{`).
- A minimal `.vmdl` (RootNode + Skeleton + Softbody/ClothProxyMeshFile) compiles fine with no render mesh; the compiled model's skeleton is then culled (`m_boneName = []`) and back-solve weights are not generated ("Cloth in model references bone X which doesn't exist" warnings) — PHYS cloth data is still produced. In real models the skeleton exists and everything is generated.
- Recompiling the ORIGINAL sources with today's compiler does NOT reproduce the original PHYS byte-for-byte (2022 vs current rod params differ: structural `w0` 0.0 vs 0.5, simd packing). Round-trip goal is functional equivalence, not binary identity.

### Proxy mesh DMX anatomy (binary 9 / model 22, same container as DmxBinary)

Elements: `DmElement "Scene"` (attrs `skeleton`,`model` → DmeModel) → `DmeModel` (`children` = **[root joint, mesh dag] ONLY**, `baseStates`=[DmeTransformList], `axisSystem`, `transform`, `jointList`=[all DmeJoint elems]) → `DmeJoint` tree (`transform`→DmeTransform, `children`) → `DmeDag` (`shape`→DmeMesh, `transform`) → `DmeMesh` (`visible`, `bindState`/`currentState`/`baseStates`→DmeVertexData, `faceSets`) → `DmeFaceSet` (`material`→DmeMaterial(`mtlName`), `faces` = int array, **-1-terminated polygons over SCENE vertices**) → `DmeTransformList`(`transforms`) → `DmeAxisSystem`(`upAxis=3, forwardParity=1, coordSys=0`).

- Putting ALL joints in `DmeModel.children` duplicates every bone on import (`tail_0_duplicate`, `tail_1_duplicate1`, ...). Only the root joint + dag belong there.
- `DmeVertexData "bind"`: `vertexFormat` string list, `flipVCoordinates=true`, `jointCount`, then per-stream data + `...Indices`. Streams: `position$0` (vec3 per BIND vertex), `normal$0`, `texcoord$0`, `blendweights$0`/`blendindices$0` (**bindVertexCount × jointCount stride**, blend index = index into `DmeModel.jointList`), then cloth maps (`cloth_enable$0`, `cloth_goal_strength_v2$0`, `cloth_drag_v2$0`, `cloth_collision_radius$0`, optional `cloth_friction$0`), one float per BIND vertex, each with `...Indices` mapping scene vertex → bind vertex.
- Faces are quads in both references (grids: QoP 2×18 strip, Drow 6×12 cape). `position$0Indices` maps each face corner (scene vertex) to a bind vertex.
- Proxy joints are merged with the model skeleton **by name**; their transforms may differ from the real skeleton (QoP `arcana_tail_1` local origin 28.97 in proxy vs 15.66 in vmdl) — compiler uses the MODEL skeleton for offsets. Joints ARE the whole model skeleton as `DmeJoint`s.
- vmdl Skeleton bone → DmeTransform: `position = origin` (parent-relative), `orientation = quat` from `angles = [pitch, yaw, roll]` degrees with **R = Rz(yaw)·Ry(pitch)·Rx(roll)** (Source convention; validated ≤0.001 pos / ≤0.1° rot against `m_InitPose` on both references).

### PHYS (m_pFeModel) ↔ proxy DMX mapping (all verified)

Node classes by index: `[0, m_nStaticNodes)` static (invmass 0), `[nStatic, m_nFirstPositionDrivenNode)` dynamic, `[firstDriven, m_nNodeCount)` position-driven back-solve bones (invmass 1).

- **`$cloth_m0pN` = proxy bind vertex N** (`m0` = proxy mesh 0). Bone nodes keep bone names.
- `m_NodeCollisionRadii` / `m_DynNodeFriction` are indexed by **(node − m_nStaticNodes)** (cover dynamic + driven nodes) → `cloth_collision_radius$0` / `cloth_friction$0` verbatim (friction array empty unless the map existed).
- `m_InitPose[i]` = model-space bind transform `[x y z 1 qx qy qz qw]` per node; cloth vertex position = `m_InitPose[node].xyz` (write dag transform = identity).
- `flAnimationForceAttraction = goal³` ⇒ `cloth_goal_strength_v2 = cbrt(forceAttr)`.
- `flAnimationVertexAttraction = goal³·(1−f) + f` where **f = 1 − (1−drag)^(1/60)** ⇒ `cloth_drag_v2 = 1 − (1−f)^60`, `f = (vertAttr − goal³)/(1 − goal³)` (if goal³ == 1, f = vertAttr). Compiler edge case: goal exactly 1.0 → vertAttr collapses to f (goal term dropped) — clamp written goal to ≤ 0.999. `cloth_drag_v2` affects ONLY vertAttr (verified by full-PHYS diff).
- `cloth_enable`: < 0.05 → static vertex (its parent bone becomes a static node too); ALL vertices static → cloth is dropped entirely (no PHYS); otherwise the value is inert. Restore as 1.0 dynamic / 0.0 static.
- `m_CtrlOffsets` entries are keyed by **`nCtrlChild`** (not array order): `nCtrlParent` = dominant skin bone node, `vOffset` = bind offset in parent frame (compiler recomputes it from the dmx; only the parent matters to us).
- `m_FitWeights` (grouped per back-solved bone by `m_FitMatrices[i].nBeginDynamic/nEnd` as index range into FitWeights) = **raw skin weights** (uint8-quantized /255) of dynamic nodes to back-solved bones. Weights to non-back-solved bones are NOT stored. Reconstruction: emit FitWeight influences as-is, remainder (1 − Σ) goes to the CtrlOffsets dominant bone.
- Faces are mandatory (no faces → no PHYS). Compiler turns faces into `m_Rods`: polygon edges AND diagonals → structural rods (`min = 0.75·max`; `w0` 0.5 in current compiler, 0.0 in 2022 builds); 2-neighbor chains → bend rods (`min ≈ 0`, `w0` from topology). Reconstruction: structural rods (min/max ≈ 0.75) between cloth nodes of the same mesh → edge graph; **4-cliques → quads**, leftover edges → 3-cliques → triangles.
- **Static↔static rod pairs are never emitted** (cells between static verts miss those edges). Reconstruction adds *virtual static-static edges*: for every static pair with no rod that completes a K4 (5 of 6 pairs edged), the missing side is assumed. With this, grids recover all-quad faces (Invoker cape 31/31 quads, Drow 55/56, QoP 30).
- **Bogus twin quads near symmetry planes**: cross-link rods (e.g. left↔right across a cape centerline) form spurious K4s that overlap the real cells (z-fighting "white triangle" artifacts). Reconstruction sorts K4 candidates by edge-length aspect (regular cells first) and rejects any candidate sharing ≥3 of its 6 edges with a single already-emitted face.
- **Winding is harmonized** post-recovery: BFS over face adjacency flipping neighbors (shared edge must be traversed in opposite directions), then a per-component outward check (area-weighted mean of raw Newell normals dotted with `faceCentroid − outwardRef` flips each component independently if negative). `outwardRef` = average world position of the stage-1 rigid parent bones (torso interior), falling back to the all-skeleton average — do NOT use "nearest bone to face" (cape/collar folds locally point at the body and hijack the vote; a single global flip also mis-orients isolated faces). Per-face Newell normals are written to `normal$0`. All reference meshes come out with zero inconsistent shared-edge traversals; QoP tail matches the original proxy 27/28 (one quad in the ribbon-twist zone goes the other way — legal), Drow 55/55, Invoker 31/31 quads.
- `m_SimdRods`/`m_SourceElems`/`m_NodeBases`/`m_Tree*` are derived data; ignore for reconstruction.

### ClothParams ↔ FeModel floats

vmdl `ClothParams` maps to PHYS as: `default_exp_air_drag`→`m_flDefaultExpAirDrag`(+Quad twin), `velocity_smooth_rate`→`m_flRodVelocitySmoothRate`(+Quad), `velocity_smooth_iterations`→`m_nRod/QuadVelocitySmoothIterations`, `default_gravity_scale`→`m_flDefaultGravityScale`, `default_vel_air_drag`→`m_flDefaultVelAirDrag`, `internal_pressure`→`m_flInternalPressure`, `windage`/`wind_drag`→same, `default_stretch`→`m_flDefaultSurfaceStretch`(+Thread), `local_force`/`local_rotation`/`local_drag1`→same, `add_world_collision_radius`→same (DEFAULT 2.0), `extra_iterations`/`extra_goal_iterations`/`extra_pressure_iterations`→`m_nExtra*`. `add_curvature`, `quad_bend_tolerance`, `goal_strength_bias`, ground/collision switches are compile-time only — NOT recoverable from PHYS (Drow's 0.76/0.05 are likely the artist-standard values). Defaults (from a no-ClothParams compile): expAirDrag 0, smooth rate 0, add_world_collision_radius 2.0, gravity scale 1. `back_solve_joints_drive_meshes` (QoP false, Drow true) is not recoverable from PHYS — default false.

### Static vs driven bone classification (the `staticPinWeights` trick)

Whether a skeleton bone becomes a **static** ctrl node or a **position-driven** (back-solve) node is decided from the proxy's skin weights: bones referenced by STATIC cloth verts (even ~0.01 weight) pin as static; bones referenced only by dynamic verts join the back-solve chain. The original static verts' secondary weights are NOT stored in PHYS, so the bone set would silently drift (Drow: `cape_R0C1` vanished, `cape_R1C*` turned driven; QoP: `thigh_R/L` vanished). Fix implemented in `ApplyClothSimRestore`: every bone node that is STATIC in the original FeModel and is not (a) a dominant parent of a static vert or (b) a rigid (stage-1 shape) parent gets "pinned" with a small (0.05) skin weight from the nearest static cloth vertex (by `m_InitPose` distance). This reproduces the exact ctrl node set (109/34/100 Drow, 47/8/40 QoP) without ever pinning driven bones (they are not static in FeModel, so they are never candidates).

### Verification for stage 2

Both reference models were stripped of their `Softbody` block, restored with the enchantment, and recompiled with `resourcecompiler.exe`:

- QoP: `nodes=47 static=8 driven=40` — exact match; ctrl name sets identical; **0 mismatches** across invmass/radius/friction/forceAttr/vertAttr per node name; rods 163↔166 pairs (1 missing, 4 extra — compiler-version stiffness-rod noise).
- Drow: `nodes=109 static=34 driven=100` — exact match; 0 per-name mismatches; rods 335↔338; compile log reports the same 9 back-solve matrices (`cape_R2C1..cape_R4C0`); 25 capsules restored.
- Faces: recovered quads as 4-cliques of the structural-rod graph (Drow 50 quads + 10 tris vs 55 original quads — same rod set; cells touching static verts have no static↔static rods so they split into tris).

Known, accepted deviations: `back_solve_joints_drive_meshes` restored as false (not in PHYS); `add_curvature`/`quad_bend_tolerance` restored as 0.76/0.05 when `ClothParams` is emitted (not in PHYS); static verts' exact secondary skin weights are approximated by the pin heuristic.

## Restore Jiggle Chains / Vertex Maps / Effects (stage 3 of Restore Cloth Sim)

Older compiled models (pre-`m_JiggleBones` era, e.g. `faceless_void_arcana_base.vmdl_c`, 2022) encode ModelDoc jiggle chains directly in the FeModel as cloth particles. The current `resourcecompiler.exe` can no longer emit that encoding from `JiggleBone` vmdl nodes (those now compile to `m_pFeModel.m_JiggleBones` / `FeJiggleBone_t`, which loses rods, goal strengths, per-particle collision spheres and vertex maps from the ModelDoc cloth panel). Stage 3 therefore rebuilds the simulation **through the cloth path** instead of authoring `JiggleBone` nodes.

`ParseJiggleChainData` + `BuildJiggleProxyDmx` + `BuildClothVertexMapBlock` + `BuildClothEffectStiffenBlock` (`src/ClothSimRestore.cpp/.h`), all driven from the existing `ApplyClothSimRestore` call.

### Legacy `$cc` encoding (how jiggle lives in PHYS)

Per jiggled bone `B`:

- Two particles `$ccB_0` / `$ccB_1` in `m_CtrlName`, placed at **±length/2 along the bone axis** (`m_InitPose`).
- **Rigid rods** (`flMinDist == flMaxDist` — vs cloth's 0.75 ratio): pair rod + full bipartite cross-rods to the child's AND grandchild's particle pairs.
- The bone `B` itself is a position-driven node (back-solved from the particle frame via `m_NodeBases`, 4-node frames: own pair + child pair).
- `m_NodeIntegrator` per particle: `flAnimationForceAttraction = goal³`, `flAnimationVertexAttraction` (same cloth formulas as stage 2), `flGravity` (360 belt, 72 tentacles on FV — per-node gravity is NOT authorable via cloth; lost, restored sim uses the 360 default).
- `m_NodeCollisionRadii[node − m_nStaticNodes]` = per-particle collision radius (FV face tentacles 1/4/8; 0 elsewhere) — these are the "collision spheres" in ModelDoc's cloth debug view.
- Static pairs (both invmass 0, e.g. belt root anchors `FVArcanaBeltFront_0`/`Rear_0` in `m_LockToGoal`) stay STATIC cloth vertices (`cloth_enable = 0`) — they anchor the chains; the bones become static ctrl nodes exactly like the original.
- `m_VertexMaps` / `m_VertexMapValues`: named per-node uint8 weight sets (`skirt_vtxMap`, `backTentC_vtxMap`, ...; `nVertexBase/Count` span, `nMapOffset` into the flat values array).
- `m_Effects`: `FeEffectName` gameplay hooks (`backTentacles_stiffen`, `faceTentacles_stiffen_point_5`, ...), all `nType = 3` (Stiffen) with `m_Params{Stiffness, BoneOverlay, VertexMap}` where `VertexMap` = the map's `nNameHash` (compare as **uint32**; the KV3 text prints it signed).

### Restoration mapping (stage 3)

A generated cloth proxy mesh `<stem>_jiggle_proxy.dmx` (binary DMX, same builder patterns as stage 2):

- 2 vertices per `$cc` pair at the original `m_InitPose` positions, 100% skin weight to the jiggle bone → the compiler re-creates the position-driven back-solve bone nodes (compile log reports "N driving bones ... simplified single-polygon shape-fit mode" — expected: original used 4-node frames, ours has 2 verts per bone).
- Ribbon quads `{parent0, parent1, child1, child0}` between consecutive chain bones (chain topology from the vmdl skeleton hierarchy), oriented outward by Newell normal vs the rigid-parent centroid.
- Per-vertex cloth maps from the particle data (same formulas as stage 2): `cloth_enable` 1/0 (static), `cloth_goal_strength_v2 = cbrt(forceAttr)` (clamp 0.999), `cloth_drag_v2` from the vertAttr formula, `cloth_collision_radius$0` / `cloth_friction$0` verbatim (0 for static).
- `ClothVertexMap` Softbody children: `data.nodes` maps **cloth node names → weight**. `$cc` particles translate to the deterministic compiled names `$cloth_m<mesh>p<point>` (mesh = jiggle proxy index, point = vert index in build order); driven bones keep their bone names. References to non-cloth nodes are skipped with a log count. `volumetric_solve` from `flVolumetricSolveStrength`, `color` from `nColor` (RGB bytes).
- `ClothEffectStiffen` Softbody children (type-3 effects only): `name` = `sName`, `Stiffness` / `BoneOverlay` from `m_Params`, `vertex_map` = the vertex map whose `nNameHash` matches `m_Params.VertexMap`, `origin`/`angles` 0, `cloth_effect_version = 0`. Unresolvable hashes / other types are skipped with a log count. The `AE_CL_CLOTH_EFFECT` animation events that reference `FeEffectName` are already present in the VRF-decompiled `.vmdl`, so effects work end-to-end.
- Validation note: vertex map node names are validated at compile time against the generated cloth node set ("Invalid cloth node name #N" error) — this is why the FeJiggleBone approach was abandoned; `$cloth_mXpN` and driven-bone names are both accepted (verified in the jigglelab sandbox).

### Verification for stage 3

`faceless_void_arcana_base.vmdl_c` (242-node all-jiggle FeModel, 75 `$cc` groups, 6 vertex maps, 17 effects): restored proxy (150 verts, 61 ribbon quads) + 18 capsules + 6 `ClothVertexMap` + 17 `ClothEffectStiffen`, recompiled with `resourcecompiler.exe`:

- `m_nNodeCount` 242=242, `m_nStaticNodes` 23=23, `m_nFirstPositionDrivenNode` 169=169 — exact.
- 75 driven bones re-created as cloth back-solve targets; 2 static belt anchors stay static.
- Per-particle goal³/drag/collision radii round-trip exactly (60 nonzero radii 1/4/8 on face tentacles).
- All 6 vertex maps (identical names) and all 17 effects (identical `sName`, `Stiffness`, resolved `VertexMap` hashes) round-trip.
- QoP regression re-check unchanged (36 verts / 30 quads, static pins 3).

Accepted deviations (not authorable via the cloth path): per-node gravity (72 vs 360 default); rods are cloth-generated from faces (structural 0.75-ish + bend, relax 1.0, 411 rods) instead of the original rigid `min == max` rods with grandchild links (633 rods) — behavior is springier but structurally equivalent (385 of 633 original rod pairs reappear); driven bone nodes lose their own integrator goal-springs and bone-level collision radii (0 in restore — the old encoding kept bone-level duplicates); `m_AnimStrayRadii` (0.24–0.4 soft stray limits) and `m_nRotLockStaticNodes` (17 vs 23) are not restored; the old `$cc` particle names become `$cloth_mXpN` (vertex maps bridge the naming).


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
