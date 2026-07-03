# Codebase Architecture Audit

Date: 2026-07-03

## Executive Summary

Overall architecture health is mixed but workable. The lower-level reusable
engine pieces are compact and mostly one-way: ECS, assets, input, UI, and
sprite systems have clear ownership and few include surprises. The sector
backend is also more layered than its directory name suggests: topology data,
validation, creation/editing, generated geometry, collision, visibility,
mesh-building, lightmaps, and runtime objects are mostly separated by headers
and free functions.

The largest risk is not include cycles. A regex include graph over `sources/`
and `tests/` found 387 resolved local include edges and no strongly connected
components. The larger risk is conceptual coupling through wide headers and
god files. `SectorEditor.cpp` is 13,313 lines and `SectorEditor.h` declares a
large private API for canvas input, authoring, preview, document IO, modal UI,
material editing, runtime objects, lights, and lightmap work. `SectorEditorTypes.h`
is 784 lines and includes most sector backend/runtime headers, so almost any
editor state consumer sees the full sector stack. `SectorMeshPreview` is the
second major pressure point: it owns generated geometry, visibility debug,
raylib GPU resources, sky, bloom, dynamic lights, dynamic spot shadow maps,
runtime billboard rendering, and door mesh caches in one class.

The good news is that many boundaries already exist and can be improved with
small behavior-preserving extractions. `SectorEditorTopologyRenderCache` is
already separated and gives a good precedent for extracting more editor
backend helpers. Runtime objects have already been split into billboard and
door modules. Topology mutation helpers centralize many hard invariants. The
AssetManager boundary is reasonably clear about CPU worker work versus
main-thread GPU upload/unload.

Top 5 recommended next actions:

1. Extract small shared sector math/path/bounds utilities from repeated
   anonymous helpers. This is low risk and reduces helper drift.
2. Split `SectorMeshPreview` renderer internals by resource family: sky/bloom,
   dynamic lighting/shadows, runtime billboards/doors. Keep behavior and public
   facade stable at first.
3. Split `SectorEditorTypes.h` into narrower state headers, especially render
   cache types, modal state, selection/picking state, and async lightmap state.
4. Continue extracting editor action/UI panels out of `SectorEditor.cpp`, but
   only along existing seams such as texture actions, material actions,
   inspectors, preview settings, and render cache.
5. Introduce a thin sector asset-path utility used by preview, lightmap,
   serialization/test helpers, and editor texture scans instead of local
   `assets/` prefix handling.

## Scope And Method

Analyzed:

- `sources/`
- `tests/`
- `tools/`
- `CMakeLists.txt`

Excluded from architecture conclusions:

- `sources/util/json.hpp`
- `sources/util/earcut.h`

Those two files are treated as vendor/drop-in dependencies. They are counted
only where explicitly noted.

Commands and methods used:

- `rg --files sources tests tools`
- `find . -maxdepth 3 -name CMakeLists.txt -o -name '*.cmake'`
- Python one-off line-count script run from the shell
- Python one-off local `#include "..."` graph script run from the shell
- `rg` searches for topology invalidation, validation, local helpers, asset
  path handling, math helpers, and large-file private helper patterns
- Manual spot checks with `sed`/`nl` of major files and headers

Limitations:

- The include graph is regex/static analysis, not compiler-grade dependency
  analysis.
- Conceptual dependency findings are based on static reads and naming. Some
  should be confirmed during an implementation pass.
- Function-level duplication was identified by grep/manual sampling, not a
  token-based clone detector.
- This audit did not run a full build or tests. It is documentation-only.

## Codebase Inventory

Line-count summary from custom code plus tests/tools:

| Area | Files | Lines | Notes |
| --- | ---: | ---: | --- |
| Total scanned | 142 | 121,890 | Includes vendor/drop-in headers |
| Total excluding vendor headers | 140 | 96,477 | Better estimate for custom code |
| `sources/engine` | 35 | 7,355 | ECS/assets/input/UI/systems |
| `sources/sector_demo` | 51 | 28,084 | Sector topology/runtime/render backend |
| `sources/sector_editor` | 32 | 27,618 | Editor UI/state/actions/cache |
| `sources/game` | 5 | 131 | Small demo rectangle code |
| `tests` | 14 | 26,927 | Strong sector test investment |
| `tools` | 2 | 6,115 | Plan executor tools |
| `sources/util` custom count | 0 | 0 | `json.hpp` and `earcut.h` excluded |

Largest custom source/header/test/tool files:

| Lines | File |
| ---: | --- |
| 13,313 | `sources/sector_editor/SectorEditor.cpp` |
| 7,933 | `tests/SectorAuthoringGraphTests.cpp` |
| 4,102 | `sources/sector_demo/SectorLightmap.cpp` |
| 3,694 | `tests/SectorRuntimeObjectTests.cpp` |
| 3,412 | `tools/plan_executor.py` |
| 3,351 | `tests/SectorTopologySerializationTests.cpp` |
| 3,271 | `sources/sector_demo/SectorMeshPreview.cpp` |
| 2,778 | `tests/SectorTopologyCreationTests.cpp` |
| 2,770 | `sources/sector_demo/SectorTopologySerialization.cpp` |
| 2,703 | `tools/plan_executor_tui.py` |
| 2,571 | `sources/engine/ui/UI.cpp` |
| 2,541 | `sources/sector_demo/SectorAuthoringGraph.cpp` |
| 2,340 | `tests/SectorTopologyLightmapTests.cpp` |
| 2,205 | `sources/sector_demo/SectorTopologyEdit.cpp` |
| 2,198 | `sources/sector_editor/SectorEditorAuthoringState.cpp` |

Largest headers excluding vendor/drop-in headers:

| Lines | File |
| ---: | --- |
| 784 | `sources/sector_editor/SectorEditorTypes.h` |
| 437 | `sources/sector_editor/SectorEditor.h` |
| 388 | `sources/sector_editor/SectorEditorUiHelpers.h` |
| 368 | `sources/engine/ui/UI.h` |
| 355 | `sources/sector_demo/SectorAuthoringGraph.h` |
| 312 | `sources/sector_demo/SectorMeshPreview.h` |
| 275 | `sources/sector_editor/SectorEditorAuthoringState.h` |
| 274 | `sources/sector_demo/SectorTopologyTypes.h` |
| 272 | `sources/sector_demo/SectorDoorRuntime.h` |
| 272 | `sources/engine/ecs/World.h` |
| 266 | `sources/sector_demo/SectorLightmap.h` |
| 257 | `sources/sector_demo/SectorTopologyMap.h` |

Largest tests/tools:

| Lines | File |
| ---: | --- |
| 7,933 | `tests/SectorAuthoringGraphTests.cpp` |
| 3,694 | `tests/SectorRuntimeObjectTests.cpp` |
| 3,412 | `tools/plan_executor.py` |
| 3,351 | `tests/SectorTopologySerializationTests.cpp` |
| 2,778 | `tests/SectorTopologyCreationTests.cpp` |
| 2,703 | `tools/plan_executor_tui.py` |
| 2,340 | `tests/SectorTopologyLightmapTests.cpp` |
| 2,167 | `tests/SectorTopologyMeshBuilderTests.cpp` |

Vendor/drop-in files noted but excluded:

| Lines | File |
| ---: | --- |
| 24,596 | `sources/util/json.hpp` |
| 817 | `sources/util/earcut.h` |

Build-system note: `CMakeLists.txt` globs main `sources/` files for the app,
but test targets use explicit source lists. This means source splits usually
enter the app automatically but require CMake edits for tests.

## Subsystem Map

### Engine Core / ECS

- Responsibilities: entity handles, sparse component pools, ECS world storage,
  simple movement/sprite systems.
- Primary files: `sources/engine/ecs/World.h`,
  `sources/engine/ecs/ComponentPool.h`, `sources/engine/ecs/Entity.h`,
  `sources/engine/components/*`, `sources/engine/systems/*`.
- Owns: entity/component storage inside `engine::World`.
- Main API: `World::ReserveEntities`, `ReserveComponentTypes`,
  `ReserveComponent<T>`, `CreateEntity`, `DestroyLater`, `FlushDestroyedEntities`,
  `Add/Get/Remove`, `ForEach`.
- Depends on: STL and component headers; no sector/editor dependencies.
- Used by: sprite/movement systems, runtime objects, game demo, sector preview.
- Boundary quality: Good.
- Notes: `World.h` enforces no structural mutation during `ForEach()` with a
  guard and asserts. It also warns when reserved capacity is exceeded, matching
  project rules.

### Asset Manager / Assets

- Responsibilities: asset scopes, texture/font/sprite-animation ownership,
  request deduplication, worker thread for CPU-side texture/sprite work, main
  thread GPU upload/unload.
- Primary files: `sources/engine/assets/AssetManager.h/.cpp`,
  `TextureAssets.*`, `FontAssets.*`, `SpriteAnimationAssets.*`,
  `AssetHandles.h`.
- Owns: raylib textures/fonts, asset slots, worker queue, asset scopes.
- Main API: `RequestTexture`, `CreateTextureFromImage`, `RequestFont`,
  `RequestSpriteAnimation`, `UpdateMainThread`, `Shutdown`.
- Depends on: raylib and STL. No sector/editor dependencies.
- Used by: editor, preview renderer, UI, sprite systems, runtime objects.
- Boundary quality: Good.
- Notes: `AssetManager.h` exposes raylib resource access through handles and
  immediate-use pointers. The worker/main-thread split is clear in the public
  API.

### Input

- Responsibilities: frame input state and events.
- Primary files: `sources/engine/input/Input.h/.cpp`,
  `InputEvents.h`, `InputConfig.h`.
- Owns: current input state and per-frame events.
- Main API: `Input` polling/event consumption helpers.
- Depends on: raylib and STL.
- Used by: `EngineContext`, UI, editor, controllers.
- Boundary quality: Good.
- Notes: The boundary is service-like and not stored in ECS components.

### UI Toolkit

- Responsibilities: immediate-mode-ish widgets, scroll areas, fields,
  dropdowns, modal-ish overlay behavior, drawing.
- Primary files: `sources/engine/ui/UI.h/.cpp`,
  `sources/engine/ui/demo/UIDemo.*`.
- Owns: `UIContext` transient interaction state and widget input state structs.
- Main API: functions in `engine::UI`, plus `UIContext`, `UIConfig`, input state
  structs.
- Depends on: engine assets/input and raylib.
- Used by: editor, UI demo.
- Boundary quality: Acceptable.
- Notes: `UI.cpp` is 2,571 lines and has many private layout/input helpers, but
  it is still contained inside engine UI. It should not absorb editor-specific
  modal patterns.

### Sector Topology / Map / Serialization

- Responsibilities: topology v2 data model, stable IDs, texture definitions,
  runtime-object authoring data, preview/lightmap settings, validation,
  loop extraction, JSON serialization.
- Primary files: `SectorTopologyMap.h/.cpp`, `SectorTopologyTypes.h`,
  `SectorTextureTypes.*`, `SectorTopologyValidation.cpp`,
  `SectorTopologySerialization.*`, `SectorTopologyUnits.*`.
- Owns: `SectorTopologyMap` as the source-of-truth document data. It contains
  vertices, linedefs, sidedefs, sectors, lights, runtime objects, preview
  settings, sky settings, directional light settings, lightmap settings, and
  baked lightmap metadata.
- Main API: `BuildSectorTopologyIndexes`, `Find*`, `Allocate*`, `Remove*`,
  `ResolveSectorDoorAnchor`, `ExtractSectorTopologyLoops`,
  `ValidateSectorTopologyMap`, `Load/Save` helpers.
- Depends on: raylib value types, texture/lightmap types, STL.
- Used by: almost every sector subsystem and most tests.
- Boundary quality: Acceptable but broad.
- Notes: `SectorTopologyMap.h:129` puts map geometry, preview settings,
  lighting settings, runtime object authoring, and baked output metadata into
  one aggregate. That is convenient but makes `SectorTopologyMap.h` the
  highest fan-in project header.

### Sector Topology Creation / Editing / Authoring Graph

- Responsibilities: topology creation/edit commands and authoring graph
  derivation.
- Primary files: `SectorTopologyCreation.*`, `SectorTopologyEdit.*`,
  `SectorAuthoringGraph.*`, `SectorEditorAuthoringState.*`.
- Owns: mostly no long-lived ownership, except editor authoring state and
  authoring graph data.
- Main API: creation/edit result structs and free functions, authoring
  derivation/validation helpers.
- Depends on: topology map/validation/geometry.
- Used by: editor and tests.
- Boundary quality: Acceptable.
- Notes: These modules are valuable because they keep many topology mutations
  out of `SectorEditor.cpp`. They still call validation/loop extraction
  frequently, which is correct for mutation phases but should remain outside
  steady draw paths.

### Generated Geometry / Mesh Builder / Collision / Visibility

- Responsibilities: derive render surfaces from topology, build mesh draw
  records, collision query world, runtime portal visibility.
- Primary files: `SectorGeneratedGeometry.*`, `SectorMeshBuilder.*`,
  `SectorCollisionWorld.*`, `SectorPortalVisibility.*`,
  `SectorMeshTypes.h`.
- Owns: generated geometry/result structs, collision world caches, visibility
  graph/result structs.
- Main API: build functions and query functions.
- Depends on: topology map/types/units; mesh builder also depends on lightmap
  layout; generated geometry uses `earcut.h`.
- Used by: preview, lightmap, runtime objects, tests.
- Boundary quality: Good to Acceptable.
- Notes: `SectorCollisionWorld` is reusable and separate from render triangles,
  which is a strong boundary. `SectorMeshTypes.h` is a shared bridge for
  receiver bounds and draw records.

### Sector Mesh Preview / Renderer

- Responsibilities: preview mesh rebuild, raylib GPU resources, texture
  handles, generated geometry, camera pose, visibility debug, sky, lightmaps,
  dynamic lights/shadows, runtime billboard drawing, runtime door drawing,
  bloom.
- Primary files: `SectorMeshPreview.h/.cpp`, `SectorSkyCylinder.*`,
  `SectorDynamicPointLightSelection.*`.
- Owns: generated geometry copy, mesh build result, visibility graph/cache,
  texture handles, raylib `Material`, `Mesh`, `Shader`, `RenderTexture2D`,
  dynamic light arrays, door mesh cache, bloom resources.
- Main API: `Rebuild`, `Shutdown`, `Render`, `DrawScene`,
  `RenderDynamicSpotLightShadowMaps`, `ApplyEmissiveDecalBloom`,
  `UpdateVisibilityDebug`, camera pose methods.
- Depends on: assets, collision, generated geometry, mesh types, visibility,
  runtime objects, dynamic light selection, raylib.
- Used by: `SectorEditor`.
- Boundary quality: Leaky/Risky.
- Notes: The public facade is useful, but the implementation is now a renderer
  landfill. It reaches into `engine::World` runtime objects for billboards and
  doors, owns render caches, and does lighting selection. This is a prime
  candidate for internal helper extraction without changing the facade.

### Lightmaps / Object Probes

- Responsibilities: lightmap layout, BVH/raycast bake, static lights,
  directional light, AO/indirect samples, alpha occluders, object probe
  placement/bake/sidecar, source hashing/status/reporting.
- Primary files: `SectorLightmap.h/.cpp`, `SectorLightmapTypes.h`.
- Owns: bake-local structures, alpha mask cache, bake result/status structs,
  object probe runtime metadata loading helpers.
- Main API: `BuildSectorLightmapLayout`, `BakeSectorLightmap`,
  object probe placement/bake/load helpers, source hash/status helpers.
- Depends on: topology map, generated geometry, raylib, filesystem.
- Used by: editor, preview, runtime objects, tests.
- Boundary quality: Acceptable but heavy.
- Notes: `SectorLightmap.cpp` is 4,102 lines and contains several subdomains
  in anonymous namespace. Its public API is coherent, but internals are ready
  for small backend splits such as asset paths, BVH/raycast, object probes, and
  reporting.

### Runtime Objects / Billboards / Doors

- Responsibilities: spawn placed runtime objects into ECS, resolve sectors,
  update object sector and lighting, request billboard animation assets,
  update/render door motion/colliders/portal blockers.
- Primary files: `SectorRuntimeObjects.*`, `SectorBillboardRuntime.*`,
  `SectorDoorRuntime.*`.
- Owns: `SectorRuntimeObjectState`, ECS components for sector objects, door
  collider/blocker arrays, object probe runtime data.
- Main API: reserve/reset/spawn/update functions, billboard/door component and
  helper APIs.
- Depends on: ECS, assets, collision, lightmap types/runtime data, portal
  visibility, topology map.
- Used by: editor preview and tests.
- Boundary quality: Acceptable.
- Notes: The split into billboard and door runtime files is a positive
  direction. Remaining leakage comes from renderer-owned door mesh caches and
  dynamic receiver bounds living in preview rendering.

### Sector Editor

- Responsibilities: app/editor orchestration, document lifecycle, 2D topology
  UI, authoring graph state, selection/hover/dragging, texture/modals,
  material editing, preview mode, gameplay preview controller, lightmap bake
  async workflow.
- Primary files: `SectorEditor.h/.cpp`, `SectorEditorTypes.h`,
  `SectorEditor*Actions.*`, `SectorEditor*Inspector.*`,
  `SectorEditor*Modal.*`, `SectorEditorTopologyRenderCache.*`.
- Owns: `SectorEditorState`, UI state, `SectorMeshPreview`, editor texture
  scope, runtime object state, collision world, async lightmap bake state.
- Main API: `Init`, `Shutdown`, `Update`, `Render`, `RenderUI`, preview render
  entry points.
- Depends on: nearly everything in sector backend plus engine UI/input/assets.
- Used by: `Main.cpp`.
- Boundary quality: Risky.
- Notes: The editor has some good extracted action/cache modules, but
  `SectorEditor.cpp` and `SectorEditorTypes.h` remain the central landfill.
  Direct state/map mutations are common, though many are correctly routed
  through `MarkTopologyDocumentEdited()`.

### Tools / Tests

- Responsibilities: broad regression coverage and local plan execution tooling.
- Primary files: `tests/*.cpp`, `tools/plan_executor*.py`.
- Owns: generated test maps and tool runtime state.
- Boundary quality: Tests are strong but CMake explicit source lists are a
  maintenance burden.
- Notes: Tests are large but valuable. They reveal hidden dependencies because
  many test targets must link several production `.cpp` files manually.

## Dependency And Include Graph

Static include graph summary:

- Scanned C/C++ files under `sources/` and `tests/`: 140
- Resolved local quote-include edges: 387
- Unresolved quote includes: 0
- Strongly connected components/cycles: 0

High fan-in headers:

| Incoming includes | Header |
| ---: | --- |
| 25 | `sources/sector_demo/SectorTopologyMap.h` |
| 21 | `sources/sector_demo/SectorUnits.h` |
| 18 | `sources/engine/assets/AssetManager.h` |
| 16 | `sources/sector_editor/SectorEditorHelpers.h` |
| 14 | `sources/engine/input/Input.h` |
| 13 | `sources/sector_demo/SectorCollisionWorld.h` |
| 13 | `sources/sector_demo/SectorGeneratedGeometry.h` |
| 13 | `sources/sector_editor/SectorEditorTypes.h` |
| 12 | `sources/sector_demo/SectorTopologyUnits.h` |
| 11 | `sources/sector_demo/SectorPortalVisibility.h` |
| 11 | `sources/sector_demo/SectorLightmap.h` |
| 11 | `sources/engine/ui/UI.h` |

High fan-out files:

| Outgoing includes | File |
| ---: | --- |
| 27 | `sources/sector_editor/SectorEditor.cpp` |
| 13 | `sources/sector_editor/SectorEditorTypes.h` |
| 13 | `tests/SectorAuthoringGraphTests.cpp` |
| 9 | `sources/sector_demo/SectorMeshPreview.cpp` |
| 8 | `sources/sector_demo/SectorMeshPreview.h` |
| 8 | `sources/sector_demo/SectorRuntimeObjects.h` |
| 8 | `sources/sector_editor/SectorEditor.h` |
| 7 | `sources/sector_demo/SectorDemo.h` |
| 7 | `tests/SectorRuntimeObjectTests.cpp` |

Likely architectural layers:

1. Engine primitives: ECS/assets/input/UI/render helpers.
2. Sector data model: topology types/map/units/textures/lightmap types.
3. Sector backend derivations: validation, authoring graph, topology editing,
   generated geometry, collision, visibility, lightmap bake, mesh build.
4. Sector runtime objects: ECS object spawning/update, billboard/door runtime.
5. Preview renderer: raylib render resources, generated meshes, dynamic lights,
   sky, runtime object drawing.
6. Editor application: document state, 2D UI, preview control, modals,
   high-level orchestration.

Include layering is mostly one-way. The suspicious dependencies are not cycles
but broad bridges:

- `SectorEditorTypes.h` includes collision, authoring graph, controllers,
  lightmap, mesh preview, runtime objects, texture types, topology creation,
  topology edit, and topology map at lines 3-15. Any file needing a small
  editor enum or modal state inherits much of the sector stack.
- `SectorMeshPreview.h` includes runtime objects and dynamic light selection at
  lines 3-10, then exposes renderer methods that accept `engine::World*` at
  lines 60-72 and visibility/runtime blocker debug at lines 95-100.
- `SectorTopologyMap.h` includes lightmap and texture types at lines 3-5, then
  stores preview, sky, directional light, runtime object, lightmap settings,
  and baked metadata directly in the map at lines 129-144.
- Test targets in `CMakeLists.txt` manually enumerate production source files.
  For example `sector_authoring_graph_tests` pulls editor authoring/document/
  texture/topology action sources plus lightmap and serialization sources.
  This is manageable but makes future file splits easy to forget in tests.

No include SCCs were found. Do not read that as "no architecture cycles"; it
means header dependencies currently avoid direct include cycles.

## Boundary And Ownership Review

### Editor vs Runtime

The editor owns both document state and runtime preview state in
`SectorEditorState`. `SectorEditorTypes.h:561` starts `SectorEditorState`;
document data appears at `SectorEditorTypes.h:562-575`, while runtime/preview
state appears in the same struct at `SectorEditorTypes.h:633-657`.

Good:

- Runtime objects are kept in `SectorRuntimeObjectState` rather than being
  scattered entirely through the editor.
- `MarkTopologyDocumentEdited()` centralizes document dirtying and cache
  invalidation in `SectorEditor.cpp:2750-2758`.
- `InvalidateTopologyRenderCache()` is explicit at `SectorEditor.cpp:5091-5095`.

Weak:

- Direct `state.topologyMap` writes still occur in multiple files. Some are
  intentionally followed by higher-level finish paths; others require careful
  review during future edits. Example: texture insertion writes
  `state.topologyMap.texturesById[id]` in
  `SectorEditorTextureActions.cpp:412-416`.
- Preview settings and sky/directional light live on the map and are edited
  through editor modal state. `ApplyPreviewSettingsModal()` writes preview,
  sky, directional light, and object probe settings at
  `SectorEditor.cpp:11076-11080`, then marks the document edited. This is
  currently correct, but it mixes visual preview settings with baked-light
  source-affecting directional/object-probe settings in one workflow.

### Runtime Objects vs Renderer

Good:

- Runtime object lifecycle is exposed as free functions in
  `SectorRuntimeObjects.h`, and object state is explicit.
- Door and billboard runtime components have separate files.

Weak:

- `SectorMeshPreview` renders runtime billboards and doors by taking
  `engine::World*` in render methods. It owns door mesh cache entries,
  dynamic receiver bounds, and shadow-caster arrays in the renderer header.
  See `SectorMeshPreview.h:127-135` and `SectorMeshPreview.h:244-263`.
- `BuildDirectDynamicLightReceiverBounds()` copies sector receiver bounds and
  appends door receiver bounds from the ECS world at
  `SectorMeshPreview.cpp:3205-3214`. This is a practical bridge, but it means
  dynamic-light receiver selection is a renderer concern that reaches into
  runtime object state.

### Topology / Map vs Renderer / Editor

Good:

- The map remains the source-of-truth data structure.
- Expensive render-cache building is separated into
  `SectorEditorTopologyRenderCache.cpp` and called on invalidation, not every
  draw.
- Generated geometry and mesh building are separate backend modules.

Weak:

- `SectorTopologyMap` contains runtime object authoring and preview/lightmap
  metadata in the same root aggregate as linedef topology. This is convenient
  for serialization but broadens every topology dependency.
- `SectorGeneratedGeometry.cpp` validates maps and extracts loops as part of
  generation at `SectorGeneratedGeometry.cpp:537` and
  `SectorGeneratedGeometry.cpp:559`. That is acceptable in rebuild phases but
  would be too expensive in steady UI draw paths.

### ECS vs Non-ECS Systems

Good:

- ECS stays small and reusable.
- Sector runtime objects use ECS for runtime object state, not for topology
  document data.
- `EngineContext` provides explicit services.

Weak:

- Some runtime/render operations take raw `engine::World*` optional pointers.
  This is pragmatic but weakens ownership clarity. Renderer methods can behave
  differently depending on whether a world is supplied.

### AssetManager / Raylib Resource Ownership

Good:

- `AssetManager` is the clear owner for texture/font/sprite assets. The public
  API returns handles and immediate-use pointers.
- Preview renderer explicitly owns its raylib render resources and has
  `ShutdownRendererResources`.
- Runtime object asset scopes are tracked in `SectorRuntimeObjectState`.

Weak:

- Generated/preview resources are partly AssetManager-owned and partly raw
  raylib-owned inside `SectorMeshPreview`. That is expected for render targets,
  materials, and meshes, but it makes `SectorMeshPreview` the lifetime hotspot.
- Asset path resolution is duplicated between preview and lightmap code, which
  raises the chance that resource loading and status checks disagree.

### Lightmap / Object-Probe Ownership

Good:

- Lightmap bake input uses a map snapshot in `SectorTopologyLightmapBakeInput`,
  which avoids live editor-state races.
- Baked object probe metadata/runtime data is explicit in lightmap types and
  runtime object state.
- Source-hash-sensitive data appears deliberately handled in the lightmap
  module.

Weak:

- `SectorLightmap.cpp` owns many subdomains in one file: layout, BVH, raycast,
  alpha mask cache, direct light evaluation, directional light, object probe
  placement/bake, sidecar IO, status, hash, and report formatting.
- Because source hash behavior is central and subtle, future splits must keep
  hash-affecting settings near tests.

### UI Toolkit Boundaries

Good:

- Engine UI has no sector dependency.
- Editor-specific inspectors/modals have started moving out of
  `SectorEditor.cpp`.

Weak:

- `SectorEditorUiState` is a large field inventory in `SectorEditorTypes.h`,
  which makes adding an inspector field cheap but keeps all editor UI state
  globally visible.
- Repeated modal patterns exist across editor modal files and `SectorEditor.cpp`.
  They are not necessarily wrong, but a small modal layout helper layer could
  reduce copy/paste.

## Duplicated Or Near-Duplicated Functionality

### Math Helpers

Examples:

- `ClampFinite` in `SectorFpsController.cpp:28-34`.
- Similar `ClampFinite` in `SectorTopologyMap.cpp:42-48`.
- `SmoothStep` in `SectorDynamicPointLightSelection.cpp:273`.
- Similar `SmoothStep` in `SectorLightmap.cpp:1320-1327`.
- `SmootherStep01` in `SectorDoorRuntime.cpp:740-744`.
- Local vector math helpers in `SectorDoorRuntime.cpp:705-737`.

Similarity: high for finite clamp and smoothstep; moderate for vector helpers.

Recommendation: extract a small `SectorMath.h` or `engine/math/RaylibMath.h`
with finite checks, clamp-finite, normalize-or-fallback, smoothstep, and small
Vector2/Vector3 helpers. Keep it plain and inline.

Risk: Low if tests cover callers; medium only if exact epsilon/fallback
behavior is accidentally unified where it should remain domain-specific.

### Finite / Sanitize / Validation Helpers

Examples:

- `IsFinite(Vector2)` and `NormalizeOrZero(Vector2)` in
  `SectorCollisionWorld.cpp:32` and `SectorCollisionWorld.cpp:74`.
- `IsFinite(Vector2)` and `IsFinite(Vector3)` in
  `SectorGeneratedGeometry.cpp:55-60`.
- `IsFiniteVector3`, `NormalizeOrFallback`, and `IsFiniteMatrix` in
  `SectorDynamicPointLightSelection.cpp:38-40`, `139-147`, `149-155`.
- `IsFinite(Vector3)` and `NormalizeOrFallback` in
  `SectorLightmap.cpp:363-366` and `1334-1340`.
- Lambda `isFiniteVector3` inside `AppendSectorDoorReceiverBounds()` at
  `SectorDoorRuntime.cpp:513-515`.

Similarity: high. These are classic anonymous/private helper pressure points.

Recommendation: extract finite vector/matrix helpers into a low-level math
header. Keep domain-specific validation such as receiver bounds near the
receiver-bound type until enough callers need it.

Risk: Low for finite checks; medium for normalize helpers because fallback and
epsilon choices need call-site review.

### UV / Texture Helpers

Examples:

- Door face UV fitting in `SectorDoorRuntime.cpp:134-180`.
- Editor wall/decal fitting public/private methods in
  `SectorEditor.h:362-366`.
- `TopologyUvFitMode` in `SectorEditorTypes.h:150`.
- Door has separate `SectorDoorUvFitMode` in `SectorDoorRuntime.h`.
- Texture path/asset resolution appears in preview and lightmap code; see
  `SectorMeshPreview.cpp:3238-3245` and `SectorLightmap.cpp:3297-3304`.

Similarity: conceptual rather than literal. Door faces and topology wall/decal
surfaces have different data, but both are manipulating scale/offset/fit.

Recommendation: do not force one generic UV abstraction yet. Start by extracting
small shared validation helpers such as "positive finite span" and asset path
resolution. Consider a later `SectorUvFit` helper only if wall/decal/door
fitting bugs appear together.

Risk: Medium if over-generalized; low for path utility extraction.

### Bounds / Collision / AABB Helpers

Examples:

- Receiver bounds validity and clamp in
  `SectorDynamicPointLightSelection.cpp:69-85`.
- Receiver bounds accumulation in `SectorMeshBuilder.cpp:237-272`.
- Door receiver bounds expansion in `SectorDoorRuntime.cpp:505-580`.
- Geometry bounds in `SectorMeshPreview.cpp:1522-1545`.
- Lightmap `BakeAabb`, `EmptyAabb`, `ExpandAabb`, `TriangleBounds`,
  `IntersectRayAabb` in `SectorLightmap.cpp:100`, `594-625`, `771`.

Similarity: moderate. Some are general AABB utilities; others are tied to
receiver bounds or bake BVH epsilon.

Recommendation: extract a minimal `SectorBounds` helper for Vector3 AABB
accumulation/validity and receiver-bound validation if needed by dynamic
lights, mesh builder, and doors. Keep BVH-specific ray/AABB math in lightmap
until a second BVH user exists.

Risk: Low for accumulation helpers; medium for BVH ray intersection because
epsilon details affect baked shadows.

### Lighting / Color Helpers

Examples:

- `ClampColorByte` and `MakeTopologySectorVertexColor` in
  `SectorGeneratedGeometry.cpp:50-84`.
- `ColorToUnitRgb` in `SectorDynamicPointLightSelection.cpp:43-49`.
- Lightmap direct-light conversion/evaluation around
  `SectorLightmap.cpp:1315-1345`.
- Door baked/object lighting color application around
  `SectorDoorRuntime.cpp:500`.

Similarity: moderate. Color conversion is shared; lighting evaluation is
domain-specific.

Recommendation: extract only trivial color conversion helpers first:
`ColorToVector3Unit`, `ClampColorByte`, maybe `Vector3ToColorByte`. Do not
merge static bake lighting and dynamic preview light evaluation in a quick
refactor.

Risk: Low for conversion; high for lighting evaluation if behavior changes.

### Serialization / Validation Helpers

Examples:

- `ValidateForSerialization()` and `ValidateAuthoringMapData()` are nearly
  identical in `SectorTopologySerialization.cpp:1649-1668`.
- Validation and loop extraction are called from topology edit/creation,
  generated geometry, collision, lightmap, render cache, and authoring state.

Similarity: high for the serialization validation wrapper, conceptual for
  validation call patterns.

Recommendation: extract a small local helper for "first validation error or
  null" if it simplifies callers. Do not hide validation behind a broad service;
  explicit validation call sites are useful.

Risk: Low.

### UI / Modal Helpers

Examples:

- `SectorEditor.cpp` still has many modal draw declarations in
  `SectorEditor.h:130-178` and document/modal functions at
  `SectorEditor.h:262-271`.
- Dedicated modal files already exist:
  `SectorEditorTextureModals.*`, `SectorEditorPreviewSettingsModal.*`,
  `SectorEditorLightmapModal.*`.
- `SectorEditorUiState` has many input state fields in
  `SectorEditorTypes.h:677-746`.

Similarity: conceptual. Many panels repeat scroll bounds, rows, buttons,
modal status/error layout, and picker option refresh patterns.

Recommendation: keep extracting by modal/panel, not by introducing a large UI
framework. A small editor-only helper for modal header/footer/status rows would
be reasonable after another modal extraction.

Risk: Low if scoped to one modal at a time.

### Asset / Path Helpers

Examples:

- `SectorMeshPreview::ResolveAssetPath()` at
  `SectorMeshPreview.cpp:3238-3245`.
- `ResolveSectorAssetPath()` at `SectorLightmap.cpp:3297-3304`.
- Lightmap also owns `MakeSectorAssetRelativePath()` and lightmap sidecar path
  helpers around `SectorLightmap.cpp:3306`.
- Editor texture/sprite scans have separate path/catalog flows in
  `SectorEditorTextureActions.cpp`.

Similarity: high for `assets/` prefix resolution.

Recommendation: extract `SectorAssetPaths.h/.cpp` or a tiny engine asset path
utility. This is one of the safest and highest-value duplication removals.

Risk: Low, but run serialization/lightmap/runtime-object tests.

### Diagnostics / Status Helpers

Examples:

- `FormatSectorTopologyValidationIssue()` centralizes topology validation
  diagnostics.
- `FormatSectorGeneratedSurfaceLabel()` in `SectorGeneratedGeometry.cpp:508`.
- `FormatSectorLightmapBakeReport()` in `SectorLightmap.cpp:3842`.
- Many editor status strings are built inline around topology/material
  mutations and modal actions.

Similarity: low to moderate. The reports are domain-specific.

Recommendation: leave most status formatting local. Extract only if the same
  message format is required in editor and tests.

Risk: Low benefit unless tied to a UI cleanup.

## God Files And Landfill Areas

### `sources/sector_editor/SectorEditor.cpp`

- Line count: 13,313.
- Mixed responsibilities: input handling, canvas transforms, picking, dragging,
  document load/save, authoring graph derivation, preview mode, gameplay
  controller integration, render-cache use, 2D drawing, 3D overlays, inspector
  UI, texture picker flows, material editing, runtime object editing, lights,
  async lightmap bake, preview settings.
- Why it matters: changes in one area require compiling and navigating a huge
  file; private helper pressure encourages new code to copy local helpers
  instead of sharing them; direct `state.topologyMap` mutation is harder to
  audit for cache invalidation.
- Recommended split/refactor: continue incremental extractions only. Good
  boundaries are preview-mode controller, selection/picking, runtime object
  inspector/actions, light actions/inspector, document workflow, and remaining
  modal draw functions.
- Priority: High, but only through small behavior-preserving passes.

### `sources/sector_editor/SectorEditorTypes.h`

- Line count: 784.
- Mixed responsibilities: editor tools/enums, picker state, cached draw data,
  main editor document state, preview runtime state, UI input field state,
  async lightmap bake state.
- Why it matters: high fan-out/fan-in bridge that forces many editor modules to
  include collision, lightmap, preview, runtime objects, topology edit, and UI.
- Recommended split/refactor: split into narrow headers:
  `SectorEditorSelectionTypes.h`, `SectorEditorModalTypes.h`,
  `SectorEditorRenderCacheTypes.h`, `SectorEditorPreviewState.h`,
  `SectorEditorLightmapBakeState.h`. Keep `SectorEditorTypes.h` as an umbrella
  temporarily if needed.
- Priority: High quick/medium refactor.

### `sources/sector_demo/SectorMeshPreview.cpp` and `.h`

- Line count: 3,271 cpp, 312 header.
- Mixed responsibilities: mesh preview facade, shader/material loading,
  texture requests, sky mesh/material, lightmap texture status, visibility
  graph/debug, camera pose, dynamic point/spot light selection, dynamic spot
  shadow maps, runtime billboards, runtime doors, door mesh cache, bloom.
- Why it matters: renderer changes risk door/runtime/light/shadow regressions.
  It is also the main place where renderer code reaches into runtime ECS world.
- Recommended split/refactor: preserve public `SectorMeshPreview` facade while
  extracting internal helpers:
  `SectorPreviewSkyRenderer`, `SectorPreviewBloom`, `SectorPreviewDynamicLights`,
  `SectorPreviewRuntimeBillboardRenderer`, `SectorPreviewDoorRenderer`.
- Priority: High medium refactor.

### `sources/sector_demo/SectorLightmap.cpp`

- Line count: 4,102.
- Mixed responsibilities: layout, BVH/raycast, alpha masks, direct/AO/indirect
  lighting, object probes, sidecar IO, source hash/status/report formatting,
  asset path utilities.
- Why it matters: source-hash and bake behavior are fragile; helper duplication
  grows because bake-local utilities are private.
- Recommended split/refactor: extract low-risk utilities first:
  asset path helpers, report formatting, object probe sidecar IO. Defer BVH and
  lighting evaluation extraction until there is a focused test plan.
- Priority: Medium.

### `sources/engine/ui/UI.cpp`

- Line count: 2,571.
- Mixed responsibilities: widget drawing, input consumption, text fields,
  dropdown overlays, scroll areas, numeric inputs, image drawing.
- Why it matters: UI is engine-level and reusable; if editor-specific layout
  patterns leak into it, it can become a framework landfill.
- Recommended split/refactor: leave alone unless UI changes demand it. If
  needed, split by widget families after tests/manual UI checks exist.
- Priority: Low/medium.

### Large Tests

- `tests/SectorAuthoringGraphTests.cpp`: 7,933 lines.
- `tests/SectorRuntimeObjectTests.cpp`: 3,694 lines.
- `tests/SectorTopologySerializationTests.cpp`: 3,351 lines.
- Why it matters: big tests are valuable but can hide repeated fixture builders
  and make refactors harder to validate quickly.
- Recommended split/refactor: only split when adding more tests in the same
  domain. Prefer fixture helpers outside `assets/` as project rules require.
- Priority: Low unless test maintenance becomes painful.

## Encapsulation / Private Helper Pressure

### Finite/Normalize Helpers

- Current locations: `SectorCollisionWorld.cpp`, `SectorGeneratedGeometry.cpp`,
  `SectorDynamicPointLightSelection.cpp`, `SectorLightmap.cpp`,
  `SectorDoorRuntime.cpp`.
- Similar/copy sites: see finite helper examples above.
- Proposed lower-level home: `sources/sector_demo/SectorMath.h` or
  `sources/engine/math/RaylibMath.h`.
- Extraction risk: Low for finite checks; medium for normalize helpers due to
  epsilon/fallback choices.

### Asset Path Resolution

- Current locations: `SectorMeshPreview::ResolveAssetPath()` and
  `ResolveSectorAssetPath()` in lightmap.
- Similar/copy sites: editor texture/sprite scanning and serialization/status
  checks likely have related path assumptions.
- Proposed lower-level home: `SectorAssetPaths.h/.cpp`.
- Extraction risk: Low. This is a strong quick win.

### Receiver Bounds Helpers

- Current locations: mesh builder, dynamic light selection, door runtime, mesh
  preview.
- Similar/copy sites: `IsValidReceiverBounds`, AABB expansion, closest point to
  bounds, door slab bounds.
- Proposed lower-level home: `SectorReceiverBounds` helper functions next to
  `SectorMeshTypes.h` or a new `SectorBounds.h`.
- Extraction risk: Low/medium. Keep dynamic-light padding local if it is a
  selection policy rather than a bounds property.

### UI Modal Rows / Picker Patterns

- Current locations: `SectorEditor.cpp`, texture modal files, preview settings
  modal, lightmap modal.
- Similar/copy sites: modal header/status/footer layout, picker option refresh,
  validation message handling.
- Proposed lower-level home: editor-only UI helper file, not engine UI.
- Extraction risk: Low but benefit is mostly maintainability, not correctness.

### Door/Wall/Decal UV Fitting

- Current locations: door UV fitting in `SectorDoorRuntime`, wall/decal fitting
  in `SectorEditor` material actions.
- Similar/copy sites: fit width/height/both concepts and finite span checks.
- Proposed lower-level home: start with tiny span/scale validation helper, not
  a large generic UV system.
- Extraction risk: Medium because door faces and wall/decal topology surfaces
  have different authoring semantics.

## Architectural Risks And Anti-Patterns

### High: God Editor File

- Evidence: `SectorEditor.cpp` is 13,313 lines; `SectorEditor.h` declares
  hundreds of private methods spanning input, draw, modal, material, document,
  preview, runtime object, light, and bake workflows.
- Why it matters: high change conflict risk and difficult cache-invalidation
  audits.
- Possible fix: continue focused extraction into action/panel/modal/backend
  files. Do not attempt a single large rewrite.

### High: Renderer Runtime Coupling In `SectorMeshPreview`

- Evidence: render methods accept `engine::World*` at
  `SectorMeshPreview.h:60-72`; runtime billboard/door methods and door mesh
  cache are renderer internals at `SectorMeshPreview.h:127-135` and
  `SectorMeshPreview.h:244-263`.
- Why it matters: renderer changes can mutate or depend on runtime object
  lifecycle assumptions; dynamic lighting receiver bounds mix static mesh and
  runtime door data.
- Possible fix: introduce internal runtime-object render adapters while keeping
  the public preview facade stable.

### High: Broad Editor State Header

- Evidence: `SectorEditorTypes.h` includes much of the backend at lines 3-15
  and defines document, render cache, runtime preview, UI, and async bake state
  in one file.
- Why it matters: it is a dependency hub and encourages unrelated modules to
  see/edit broad state.
- Possible fix: split type groups into narrow headers.

### Medium: Map Aggregate Carries Multiple Layers

- Evidence: `SectorTopologyMap.h:129-144` stores topology, lights, runtime
  objects, preview settings, sky, directional light, lightmap settings, and
  baked metadata together.
- Why it matters: convenient serialization but blurred ownership. Visual-only
  preview settings and bake-affecting settings must be kept separate in hash
  logic and editor workflows.
- Possible fix: keep schema stable, but use helper accessors/normalizers and
  avoid passing mutable whole-map references where a smaller view suffices.

### Medium: Copy-Paste Helper Drift

- Evidence: repeated `ClampFinite`, `IsFinite`, `NormalizeOrFallback`,
  `SmoothStep`, AABB expansion, and asset path resolution helpers.
- Why it matters: behavior drifts across rendering, baking, collision, and
  editor paths.
- Possible fix: extract small low-level helpers, one category at a time.

### Medium: Validation Cost Can Sneak Into Steady Paths

- Evidence: render cache build calls validation/index/loop/earcut in
  `SectorEditorTopologyRenderCache.cpp:402-497`; generated geometry calls
  validation/loop extraction in `SectorGeneratedGeometry.cpp:537-559`.
- Why it matters: these are acceptable during invalidated rebuilds, not every
  frame. Future direct draw edits could accidentally reintroduce expensive work
  into steady 2D rendering.
- Possible fix: preserve render-cache invalidation discipline and review every
  topology mutation for cache invalidation.

### Medium: Test CMake Explicit Source Lists

- Evidence: test targets in `CMakeLists.txt` manually list production `.cpp`
  files while the main app uses `GLOB_RECURSE`.
- Why it matters: implementation splits can break or silently omit tests until
  CMake is updated.
- Possible fix: consider small test object libraries by subsystem later, but
  only after source layout stabilizes.

### Low: Engine UI File Size

- Evidence: `UI.cpp` is 2,571 lines.
- Why it matters: not currently leaking sector dependencies, but widget growth
  can become hard to navigate.
- Possible fix: defer until a UI-specific feature/refactor requires it.

## Recommended Refactor Roadmap

### Quick Wins: 1-2 Hour Safe Splits/Extractions

1. Extract sector asset path helpers.
   - Benefit: removes duplicate `assets/` prefix logic between preview and
     lightmap/status paths.
   - Risk: Low.
   - Likely files: new `SectorAssetPaths.h/.cpp`, `SectorMeshPreview.cpp`,
     `SectorLightmap.cpp`, related tests.
   - Tests: `sector_topology_lightmap`, `sector_runtime_object`,
     `sector_topology_serialization`, plus `git diff --check`.

2. Extract finite vector and clamp-finite helpers.
   - Benefit: reduces anonymous helper duplication and drift.
   - Risk: Low if exact fallback behavior is preserved.
   - Likely files: new small math header, `SectorFpsController.cpp`,
     `SectorTopologyMap.cpp`, `SectorGeneratedGeometry.cpp`,
     `SectorDynamicPointLightSelection.cpp`, `SectorLightmap.cpp`.
   - Tests: `sector_fps_controller`, `sector_collision_world`,
     `sector_topology_generated_geometry`, `sector_topology_lightmap`,
     `sector_topology_mesh_builder`.

3. Move render-cache cached draw structs out of `SectorEditorTypes.h`.
   - Benefit: narrows the main editor state header.
   - Risk: Low if include updates are mechanical.
   - Likely files: new `SectorEditorTopologyRenderCacheTypes.h`,
     `SectorEditorTypes.h`, `SectorEditorTopologyRenderCache.*`.
   - Tests: `sector_authoring_graph`, `sector_editor_ui_layout`.

4. Extract lightmap bake report formatting from `SectorLightmap.cpp`.
   - Benefit: reduces lightmap file size without touching bake math.
   - Risk: Low.
   - Likely files: `SectorLightmapReport.h/.cpp` or private helper cpp.
   - Tests: `sector_topology_lightmap`, serialization tests if report strings
     are asserted.

### Medium Refactors: 0.5-1 Day

1. Split `SectorEditorTypes.h` into modal, selection, preview, lightmap async,
   and render-cache type headers.
   - Benefit: reduces include fan-out and makes state ownership easier to
     reason about.
   - Risk: Medium due to many includes and test target source lists.
   - Likely files: editor headers and CMake test lists only.
   - Tests: full `ctest`.

2. Extract `SectorPreviewDynamicLighting` internals from `SectorMeshPreview`.
   - Benefit: isolates dynamic point/spot selection, shader uniform locations,
     and shadow map resources.
   - Risk: Medium because rendering behavior and shader resource lifetimes are
     sensitive.
   - Likely files: `SectorMeshPreview.*`, new preview dynamic lighting files,
     `SectorDynamicPointLightSelection.*`.
   - Tests: mesh builder/runtime tests plus manual preview rendering.

3. Extract runtime door rendering/mesh cache from `SectorMeshPreview`.
   - Benefit: separates runtime object drawing from static sector mesh preview.
   - Risk: Medium; door lighting/shadow behavior must not regress.
   - Likely files: `SectorMeshPreview.*`, `SectorDoorRuntime.*`, new renderer
     helper.
   - Tests: `sector_runtime_object`, dynamic light/shadow visual manual check.

4. Split object probe sidecar IO from `SectorLightmap.cpp`.
   - Benefit: isolates file format/lifetime from bake math.
   - Risk: Medium; source hash and sidecar status must remain unchanged.
   - Likely files: `SectorLightmap.*`, `SectorLightmapTypes.h`, new sidecar
     files.
   - Tests: `sector_topology_lightmap`, `sector_runtime_object`.

### Larger Refactors: Dedicated Plan Required

1. Rework `SectorMeshPreview` into a facade over renderer components.
   - Benefit: clear ownership of sky, static mesh, runtime objects, dynamic
     lights, bloom, and debug.
   - Risk: High without screenshots/manual verification.
   - Needs: explicit file split plan, shader/resource lifetime checklist,
     manual preview verification.

2. Rework editor document state versus transient preview/UI state.
   - Benefit: clearer ownership and safer cache invalidation.
   - Risk: High because it touches most editor workflows.
   - Needs: dedicated editor refactor plan and cache-invalidation audit.

3. Introduce subsystem test object libraries in CMake.
   - Benefit: source splits become less painful for tests.
   - Risk: Medium; can churn build structure.
   - Needs: build-system-specific plan after source boundaries settle.

### Things Not Worth Doing Yet

- A broad OOP hierarchy for editor tools or runtime entities. The existing
  plain-data/free-function style fits the project and should be preserved.
- A generic universal UV/material framework. Door, wall, decal, and flat
  surfaces share concepts but still have different authoring semantics.
- Moving all sector code out of `sector_demo/` in one pass. The directory name
  is misleading, but behavior-preserving internal splits are more valuable
  than a large rename.
- Splitting `UI.cpp` without a UI-specific need. It is large but not currently
  a cross-subsystem dependency problem.

## Appendix: Evidence

### Commands Used

```sh
git status --short
rg --files sources tests tools
find . -maxdepth 3 -name CMakeLists.txt -o -name '*.cmake'
python3 - <<'PY'  # line-count inventory
python3 - <<'PY'  # quote-include graph and SCC analysis
rg -n "bool IsFinite|IsFiniteVector|NormalizeOr|SmoothStep|SmootherStep|ClampFinite|ClampColorByte|ColorToUnitRgb|ResolveSectorAssetPath|ResolveLightmapTexturePath|ResolveAssetPath|Bounds|Aabb|Fit" sources/sector_demo sources/sector_editor sources/engine --glob '!sources/util/json.hpp' --glob '!sources/util/earcut.h'
rg -n "MarkTopologyDocumentEdited|InvalidateTopologyRenderCache|topologyMap\.|ValidateSectorTopologyMap|ExtractSectorTopologyLoops|BuildSectorTopologyIndexes|earcut" sources/sector_editor sources/sector_demo --glob '!sources/util/earcut.h'
nl -ba <selected files> | sed -n '<ranges>'
```

The Python scripts were run as shell heredocs and were not saved as permanent
files.

### Include-Cycle Summary

```text
files: 140
edges: 387
unresolved quote includes: 0
strongly connected components/cycles: 0
```

Largest fan-in headers and fan-out files are listed in
`Dependency And Include Graph`.

### Selected File / Line References

- `SectorEditor.h:3-10`: editor public header includes assets, engine context,
  input, UI, material/topology actions, editor types, and mesh preview.
- `SectorEditor.h:44-410`: private method declarations cover many editor
  workflows in one class.
- `SectorEditorTypes.h:3-15`: broad backend includes from a state/types header.
- `SectorEditorTypes.h:561-675`: `SectorEditorState` mixes topology document,
  authoring, render cache, preview/runtime, controller, texture, modal, and
  runtime-object state.
- `SectorEditorTypes.h:677-746`: large UI input state aggregate.
- `SectorEditor.cpp:2750-2758`: `MarkTopologyDocumentEdited()` dirtying and
  cache invalidation path.
- `SectorEditor.cpp:5091-5107`: topology render cache invalidation/rebuild.
- `SectorEditor.cpp:10560-10597`: level load clears runtime objects, shuts
  preview renderer resources, replaces map/authoring state, and invalidates
  cache.
- `SectorEditor.cpp:11076-11080`: preview/sky/directional/object-probe setting
  mutation followed by document-edited marking.
- `SectorEditorTextureActions.cpp:412-416`: direct texture registry mutation.
- `SectorTopologyMap.h:129-144`: map aggregate owns topology, lights, runtime
  objects, preview/sky/directional settings, lightmap settings, and baked
  lightmap metadata.
- `SectorMeshPreview.h:44-116`: preview facade API.
- `SectorMeshPreview.h:127-135`: private runtime billboard/door rendering and
  dynamic light receiver helpers.
- `SectorMeshPreview.h:138-280`: preview renderer state combines generated
  geometry, visibility, asset handles, raylib materials/shaders/meshes,
  runtime door cache, dynamic lights/shadows, bloom.
- `SectorMeshPreview.cpp:1522-1545`: local geometry bounds helper.
- `SectorMeshPreview.cpp:3205-3214`: dynamic light receiver bounds combine
  static mesh bounds and runtime door bounds.
- `SectorMeshPreview.cpp:3238-3245`: preview asset path resolver.
- `SectorLightmap.cpp:363-366`: local finite Vector3 helper.
- `SectorLightmap.cpp:1320-1327`: local smoothstep helper.
- `SectorLightmap.cpp:1334-1340`: local normalize-or-fallback helper.
- `SectorLightmap.cpp:3297-3304`: lightmap asset path resolver.
- `SectorLightmap.cpp:3842`: bake report formatting starts.
- `SectorFpsController.cpp:28-34` and `SectorTopologyMap.cpp:42-48`:
  duplicate `ClampFinite` helpers.
- `SectorDynamicPointLightSelection.cpp:38-49`: finite Vector3 and color
  conversion helpers.
- `SectorDynamicPointLightSelection.cpp:69-85`: receiver bounds clamp/validity.
- `SectorDoorRuntime.cpp:505-580`: door receiver bounds helper with local
  finite/bounds lambdas.
- `SectorDoorRuntime.cpp:705-744`: local Vector2 helpers and smootherstep.
- `SectorTopologySerialization.cpp:1649-1668`: two similar validation wrappers.
- `SectorEditorTopologyRenderCache.cpp:402-497`: render-cache rebuild performs
  index build, validation, loop extraction, and earcut triangulation.
- `SectorGeneratedGeometry.cpp:537-559`: generated geometry rebuild validates
  and extracts topology loops.

### Cache Invalidation / Lightmap Hash Notes

This audit did not change source code or runtime behavior.

Topology mutation behavior observed:

- The intended editor mutation path remains `MarkTopologyDocumentEdited()`,
  which sets document dirty flags and calls `InvalidateTopologyRenderCache()`.
- `InvalidateTopologyRenderCache()` increments a revision and marks the cache
  invalid.
- Loading a document invalidates the topology render cache after replacing map
  and authoring state.
- Future refactors touching editor mutation code should preserve this path and
  explicitly audit any direct `state.topologyMap` mutations.

Lightmap source-hash behavior observed:

- This audit did not inspect every hash field in detail and did not alter
  lightmap behavior.
- The architecture risk is that visual preview settings, sky settings,
  directional light settings, object-probe settings, and geometry-affecting
  fields share nearby editor/modal workflows. Future lightmap refactors should
  keep source-hash-affecting settings explicit and covered by tests.

### Collision / Sector Lookup / Physics Notes

This audit did not change gameplay collision, sector lookup, camera, or
physics behavior. The existing architecture keeps `SectorCollisionWorld`
separate from generated render triangles, which is a strong boundary to
preserve.
