# Sector Portal Visibility / Draw Culling Audit and Design

This is an audit/design document only. It does not define production behavior
changes for the current codebase.

## Goals

Future feature goals:

- Build a runtime sector visibility graph from derived `SectorTopologyMap`.
- Traverse visible sectors from the player/current sector through two-sided
  linedef portals.
- Provide debug visible-sector output.
- Eventually draw only visible sector geometry instead of drawing the whole
  level as global static mesh batches.

First implementation scope should stay limited to:

- Phase 1: sector visibility graph and debug visible-sector output.
- Phase 2: runtime draw culling with minimal changes to existing generated
  geometry, material, lightmap, decal, sky, and highlight behavior.

Dynamic floors/doors, sector-owned objects, and per-sector dirty mesh rebuilds
are future work.

## Audit: Current Topology / Runtime Data Flow

Authored editor data is graph-native:

- `sources/sector_demo/SectorAuthoringGraph.h`
  - `SectorAuthoringGraph` owns authoring vertices, lines, line sides, and face
    anchors.
  - Authoring face anchors carry sector-like properties such as floor/ceiling
    height, textures, ambient, default wall parts, decals, and `ceilingSky`.
- `sources/sector_demo/SectorAuthoringGraph.cpp`
  - `PlanarizeSectorAuthoringGraph()` splits/intersects graph lines into planar
    edges.
  - `ExtractSectorAuthoringFaces()` extracts bounded faces.
  - `DeriveSectorTopologyMapFromAuthoringGraph()` creates the active
    linedef-based `SectorTopologyMap` plus authoring-to-derived mappings.
  - `BuildDerivedTopologyFacesAndLines()` creates derived vertices, linedefs,
    sidedefs, sectors, and mapping records.

Editor state stores both authoring and derived data:

- `sources/sector_editor/SectorEditorTypes.h`
  - `SectorEditorState::topologyMap` is the current derived runtime topology.
  - `authoringGraph`, `authoringDerivation`,
    `lastValidAuthoringDerivedTopology`, and `authoringDerivedTopologyStale`
    track derivation state.
- `sources/sector_editor/SectorEditorAuthoringState.cpp`
  - `CopyEditorMapLevelFields()` preserves map-level fields such as textures,
    static lights, preview settings, sky settings, directional light, and
    lightmap settings across derivation.
  - `TryDeriveSectorEditorAuthoringTopology()` updates `state.topologyMap` when
    derivation succeeds.

3D preview/runtime data is generated from `SectorTopologyMap`:

- `sources/sector_demo/SectorGeneratedGeometry.cpp`
  - `BuildSectorGeneratedGeometry()` validates topology, extracts sector loops,
    builds floor/ceiling surfaces, wall strips, portal lower/upper strips, and
    middle textures.
  - Ceiling sky sectors skip normal ceiling geometry.
  - Sky-sky portals suppress upper wall strips.
- `sources/sector_demo/SectorMeshBuilder.cpp`
  - `BuildSectorMeshes()` calls `BuildSectorGeneratedGeometry()`, groups
    generated surfaces into material/decal/lightmap batches, uploads raylib
    meshes, and returns `SectorMeshBuildResult`.
- `sources/sector_demo/SectorMeshPreview.cpp`
  - `SectorMeshPreview::RebuildRendererResources()` builds generated geometry,
    requests textures/lightmap assets, creates sky meshes/materials, builds
    topology meshes, and initializes preview camera state.
  - `SectorMeshPreview::DrawScene()` draws sky, then all mesh batches.
- `sources/sector_demo/SectorDemo.cpp`
  - Runtime demo loading uses `LoadSectorTopologyMap()` and
    `preview.RebuildRendererResources()`.
  - `SectorDemo::Render()` calls `preview.DrawScene()`.
- `sources/sector_editor/SectorEditor.cpp`
  - `TryEnterPreview3D()` gates on current valid derived topology, rebuilds
    preview renderer resources, and builds the collision world.
  - `RenderPreview3DScene()` calls `preview.DrawScene()`.
  - `ApplyPreview3DBloom()` calls
    `preview.ApplyEmissiveDecalBloomToScene()`.

Collision/player sector lookup exists for editor gameplay preview:

- `sources/sector_demo/SectorCollisionWorld.*`
  - `BuildFromTopology()` builds collision sectors, loops, edges, and portal
    neighbor lists from `SectorTopologyMap`.
  - `FindSectorContainingPoint()`,
    `FindSectorContainingPointPreferCurrent()`, and
    `FindSectorForPlayerFootprint()` provide sector lookup.
- `sources/sector_editor/SectorEditorPreviewActions.cpp`
  - `RebuildSectorEditorCollisionWorld()` builds the collision world for
    preview gameplay.
  - `RefreshSectorEditorGameplaySectorAndVerticalContext()` updates
    `fpsControllerState.currentSectorId`.

Lightmap data is topology-derived:

- `sources/sector_demo/SectorLightmap.cpp`
  - `BuildSectorLightmapLayout()` rebuilds generated geometry and assigns
    charts by generated surface index.
  - `BakeSectorLightmap()` bakes from generated geometry, static point lights,
    directional light, AO, and indirect settings.
  - `ComputeSectorLightmapSourceHash()` includes topology geometry, referenced
    lightmap textures, side/sector material data, `ceilingSky`, static lights,
    directional light, bake constants, and lightmap bake settings.
  - Sky visual settings are not in the source hash.
  - `GetSectorLightmapStatus()` compares `bakedLightmap.sourceHash` to the
    computed hash and verifies the atlas file exists.

## Audit: Current Mesh Generation and Ownership

Generated geometry is created in `BuildSectorGeneratedGeometry()`:

- Floors and ceilings are triangulated with `mapbox::earcut()` from
  `ExtractSectorTopologyLoops()`.
- Ceiling geometry is omitted for `SectorTopologySector::ceilingSky`.
- One-sided linedefs generate full `Wall` surfaces.
- Two-sided linedefs generate lower/upper wall strips where neighbor floor or
  ceiling heights expose a solid span.
- Upper strips are suppressed between two sky-ceiling sectors.
- Middle textures are generated for two-sided linedefs when either side has a
  middle texture; two alpha-tested middle surfaces are emitted, one for each
  face direction.

Surface source IDs are retained before batching:

- `SectorGeneratedSurface::ref` stores:
  - `kind`: floor, ceiling, wall, lower wall, upper wall, middle.
  - `topologySectorId`.
  - `topologyLineDefId`.
  - `topologySideDefId`.
  - `topologySide`.
- Flat surfaces have sector IDs and no linedef/sidedef IDs.
- Wall and middle surfaces have sector, linedef, sidedef, and side IDs.

Mesh ownership after batching:

- `BuildSectorMeshBatchData()` groups all generated surfaces into global
  batches keyed by:
  - base texture ID
  - decal texture ID
  - decal opacity/emissive/tint/bloom intensity
  - alpha-test state/cutoff
  - receives-lightmap flag
- `SectorMeshBatchData` contains only merged vertices and batch material state.
  It does not retain per-surface or per-sector ranges.
- `CreateMeshFromBatch()` allocates CPU arrays, writes position/normal/base UV,
  lightmap UV, decal UV in tangents, vertex color, and calls `UploadMesh()`.
- `SectorMeshBatch` owns the uploaded `Mesh` plus batch material metadata.
- `UnloadSectorMeshes()` unloads each batch mesh.

Current mesh structure is therefore global material/decal/lightmap batches, not
one giant mesh, not per-sector meshes, and not per-surface meshes. Sector
ownership is recoverable from `SectorGeneratedGeometry` before upload, but not
from `SectorMeshBuildResult::batches` after upload.

To draw per-sector or per-sector/per-material batches, phase 2 needs one of:

- Add surface/range metadata while batching so each uploaded global batch can
  identify sector-owned triangle ranges. Raylib `DrawMesh()` cannot draw ranges
  directly with the current mesh object, so this likely still needs either
  separate meshes or lower-level draw calls.
- Build cached sector/material batches from `SectorGeneratedGeometry`, e.g.
  key by `(sectorId, material/decal/lightmap key)` and upload one mesh per
  sector/material key.
- Build visibility-maskable draw records around existing generated surfaces,
  then upload grouped meshes once at preview/rebuild time. Do not remesh the
  currently visible set every frame.

The least-risk first rendering implementation is sector/material batches built
at preview rebuild time. It preserves the current material key and lightmap UV
generation while adding sector ownership at draw-record granularity.

## Audit: Current Render Path

3D preview draw path:

- `SectorEditor::RenderPreview3DScene()` calls `SectorMeshPreview::DrawScene()`.
- `SectorDemo::Render()` also calls `SectorMeshPreview::DrawScene()`.
- `DrawScene()`:
  - `BeginMode3D(camera)`.
  - Draws sky cylinder/top cap if a sky texture is ready and sky meshes exist.
  - Gets the baked lightmap texture if available.
  - Sets shader uniforms for lightmap use and AO.
  - Iterates every `SectorMeshBatch` in `meshes.batches`.
  - Resolves base texture and optional decal texture.
  - Sets material textures and shader uniforms.
  - Calls `DrawMesh(batch.mesh, material, MatrixIdentity())`.
  - `EndMode3D()`.

Sky handling:

- `sources/sector_demo/SectorSkyCylinder.cpp`
  - `ShouldRenderSkyCylinder()` returns true if any sector has `ceilingSky`.
  - Sky visual settings come from map-level `skySettings`.
- `SectorMeshPreview::RebuildRendererResources()` builds a visual-only sky
  cylinder and top cap when needed.
- `DrawSkyCylinder()` translates the sky mesh to the camera position, disables
  depth writes, draws cylinder and top cap, then restores depth writes.
- Sky ceilings are not normal ceiling geometry, and the sky cylinder has no
  generated surface metadata.

Selection/hover highlights:

- `SectorEditor::PickSectorSurface3D()` raycasts against
  `preview.RenderedGeometry()` with `PickSectorGeneratedGeometry()`.
- `DrawPreviewSurfaceHighlights()` iterates `preview.RenderedGeometry().surfaces`
  and draws triangle wire outlines for hovered/selected surface refs.
- This uses generated geometry, not uploaded mesh batches.
- If phase 2 culls draw batches, picking/highlighting must choose whether it
  uses all generated geometry or only visible sectors. To keep behavior aligned
  with what is drawn, picking/highlighting should use the same visible-sector
  set when culling debug/draw mode is active.

Decals, middle textures, and cutout materials:

- Decals are handled in the main shader using `decalTexture`, opacity, emissive
  flag, tint, and decal UV stored in mesh tangents.
- Emissive decals bloom through `RenderBloomSource()`, which loops all batches,
  skips non-emissive/no-decal batches, and draws emissive decal batches to a
  downsampled bloom source.
- Middle textures are generated as alpha-tested `Middle` surfaces with
  `alphaTest = true`, `alphaCutoff = 0.5`, and `receivesLightmap = true`.
- Current draw order is simply batch iteration order after unordered-map-backed
  batch creation; no special sorted cutout/decal ordering is visible in the
  current code.

Lightmap application:

- `BuildSectorLightmapLayout()` creates per-surface chart UVs indexed by
  `SectorGeneratedGeometry::surfaces`.
- `BuildSectorMeshBatchData()` copies chart UVs into mesh texcoord2 by source
  surface index and vertex index.
- `DrawScene()` binds the lightmap atlas to `MATERIAL_MAP_SPECULAR`; shader
  combines vertex ambient color, AO alpha, and baked direct RGB.
- Batches with `receivesLightmap == false` set `hasLightmap = 0`.

Render state:

- Main mesh draw uses raylib material state and shader uniforms per batch.
- Sky draw temporarily disables depth writes with `rlDisableDepthMask()` and
  restores with `rlEnableDepthMask()`.
- Bloom uses render textures and its own shader/material resources.

Least-risk insertion point:

- Phase 1 should not touch render code.
- Phase 2 should insert culling inside `SectorMeshPreview` draw loops, not in
  editor UI code:
  - `DrawScene()` should draw only visible sector draw records when culling is
    enabled and a valid visibility result exists.
  - `RenderBloomSource()` must use the same visible sector filtering, otherwise
    hidden emissive decals can still bloom.
  - Sky draw should stay independent of sector culling.
  - Surface highlight/pick filters can be layered in editor code after draw
    culling exists.

## Audit: Current Collision and Player-Sector Lookup

`SectorCollisionWorld` already has reusable topology-based sector lookup:

- `BuildFromTopology()` validates the topology, sorts sectors by ID, extracts
  loops, converts planar coordinates to world units, stores floor/ceiling in
  world-space Y units, and builds edges.
- `SectorCollisionSector` stores:
  - `sectorId`
  - heights
  - outer loop and hole loops
  - edges
  - `portalNeighbors`
- `SectorCollisionEdge` stores endpoints in world XZ, line/sidedef/sector IDs,
  neighbor sector ID, kind (`BlockingWall` or `Portal`), and `blocksPlayer`.
- A two-sided linedef creates portal edges on both sector sides.
- A one-sided linedef creates blocking edges.
- `blocksPlayer` is a movement rule on the linedef, not necessarily a visual
  visibility blocker.

Lookup helpers:

- `FindSectorContainingPoint()` brute-forces all sectors.
- `FindSectorContainingPointPreferCurrent()` checks current sector, then portal
  neighbors, then brute-force fallback.
- `FindSectorForPlayerFootprint()` uses current/neighbor lookup and movement
  constraints to avoid premature sector changes around drops.
- `ResolveMovement()` updates movement through portals and blocking edges.

Editor gameplay preview:

- `RefreshSectorEditorGameplaySectorAndVerticalContext()` uses
  `FindSectorForPlayerFootprint()` and stores
  `fpsControllerState.currentSectorId`.
- FreeFly preview has no authoritative current sector; it is noclip.

First visibility pass should use:

- Existing current sector from gameplay preview when available.
- Otherwise brute-force `FindSectorContainingPoint()` or a new equivalent
  topology helper for the camera XZ position.
- Later optimization can use current-sector plus neighbor tracking or a spatial
  hash. Brute force is safest for first pass because maps are small and it
  avoids coupling visibility correctness to movement state.

## Audit: Current Light / Object Ownership

Static lights:

- `SectorTopologyMap::staticLights` stores `SectorTopologyStaticPointLight`.
- `SectorTopologyStaticPointLight` has stable ID, authoring-space position
  vector, color, intensity, radius, and source radius.
- Static lights are map-level world-position records, not sector-owned records.
- The 2D render cache draws them through
  `SectorEditorTopologyRenderCache::staticLights`.
- Lightmap baking converts them to world lights and includes them in the source
  hash.

Objects/entities:

- The sector editor/runtime topology path currently has no object/entity
  ownership model for map objects.
- The engine ECS exists separately, but sector preview rendering is not using an
  ECS object list for level objects.

Later sector-owned object culling would need:

- A map/runtime object representation with stable IDs.
- Sector assignment policy for point objects and objects spanning portals or
  multiple sectors.
- Visibility integration separate from static baked lighting.

Do not design a full object system as part of this work.

## Audit: Existing Tests and Helpers

Relevant tests:

- `tests/SectorAuthoringGraphTests.cpp`
  - Authoring planarization, face extraction, derivation to topology, mapping,
    generated geometry from derived topology, and authoring surface mapping.
- `tests/SectorTopologyValidationTests.cpp`
  - Topology validity and invalid reference handling.
- `tests/SectorTopologyCreationTests.cpp`
  - Topology construction and generated surface expectations.
- `tests/SectorTopologyGeneratedGeometryTests.cpp`
  - Generated floors, ceilings, walls, lower/upper strips, sky behavior,
    middle textures, UVs, decals, and source refs.
- `tests/SectorTopologyMeshBuilderTests.cpp`
  - Material/decal batching behavior from generated geometry.
- `tests/SectorCollisionWorldTests.cpp`
  - Collision world build, portal extraction, portal neighbor lists, point
    lookup, movement, and height/world-unit behavior.
- `tests/SectorCollisionMovementTests.cpp`
  - Movement/collision edge cases.
- `tests/SectorFpsControllerTests.cpp`
  - FPS controller and visual camera offsets.
- `tests/SectorTopologyLightmapTests.cpp`
  - Lightmap layout, source hash behavior, middle texture receiver behavior,
    and sky/lightmap interactions.
- `tests/SectorSkyCylinderTests.cpp`
  - Sky cylinder mesh/settings behavior.
- `tests/SectorEditorUiLayoutTests.cpp`
  - UI layout tests only; not useful for visibility core.

Future non-GUI tests should live beside existing focused tests:

- New visibility graph/traversal tests: `tests/SectorPortalVisibilityTests.cpp`.
- Draw batch selection helper tests can live in
  `tests/SectorTopologyMeshBuilderTests.cpp` if the helper belongs to mesh
  building, or in `tests/SectorPortalVisibilityTests.cpp` if it remains a
  visibility-facing draw selection helper.
- Avoid GUI automation, screenshot tests, and editor launch tests.

## Design: Runtime Sector Portal Graph

Conceptual data:

```cpp
struct RuntimePortalEdge {
    int lineDefId;
    int sideDefId;
    int fromSectorId;
    int toSectorId;
    Vector2 a;
    Vector2 b;
    float openBottom;
    float openTop;
    bool open;
};

struct RuntimeSectorNode {
    int sectorId;
    std::vector<int> outgoingPortalEdgeIndices;
};

struct RuntimeSectorVisibilityGraph {
    std::vector<RuntimeSectorNode> sectors;
    std::vector<RuntimePortalEdge> portals;
};

struct RuntimePortalVisibilityResult {
    int startSectorId;
    std::vector<int> visibleSectorIds;
    std::vector<int> traversedPortalLineDefIds;
    bool validStartSector;
};
```

Build source:

- Use normal derived `SectorTopologyMap`.
- Use two-sided linedefs only: both `frontSideDefId` and `backSideDefId` must
  resolve to valid sidedefs and sectors.
- Build two directed portal edges per valid two-sided linedef.
- Convert portal endpoints to world XZ using existing sector coordinate helpers.
- Compute vertical open interval from authored sector heights:
  - `openBottom = max(floorA, floorB)`
  - `openTop = min(ceilingA, ceilingB)`
  - Treat as closed/non-visible if `openBottom >= openTop`.
  - Store in the same height unit the traversal expects; for camera/player use,
    world-space Y is preferable, so convert authored heights with
    `SectorAuthoringToWorldDistance()`.

Handling rules:

- One-sided walls: no portal edge.
- `lineDef.flags.blocksPlayer`: do not automatically block visibility in phase
  1. It blocks movement today, but visually the line may still be open. Add an
  explicit future visual blocking flag only if needed.
- Closed vertical interval: edge exists for debug but `open = false` and is not
  traversed.
- Sky ceilings: no special portal traversal rule in phase 1. `ceilingSky`
  affects generated geometry and sky rendering, but a two-sided linedef with an
  open vertical interval remains a visibility portal.
- Missing/invalid references: graph build should fail or return diagnostics.
  Do not silently create one-way graph holes from malformed topology.
- No linked, remote, or non-Euclidean portals.

Implementation location:

- Prefer new small files under `sources/sector_demo/`, for example
  `SectorPortalVisibility.h/.cpp`, because the graph is reusable by runtime and
  editor preview.
- Keep this independent from `SectorCollisionWorld` movement constraints. It
  can reuse helper patterns, but visibility should not inherit player step,
  ceiling, or `blocksPlayer` behavior.

## Design: Player Sector Lookup

Simplest safe strategy:

- If editor gameplay preview has `fpsControllerState.currentSectorId != 0`, use
  it.
- Otherwise find the current sector from camera/player XZ position:
  - Use `SectorCollisionWorld::FindSectorContainingPoint()` if the collision
    world exists and is valid.
  - If visibility is needed outside the editor gameplay path, provide a small
    visibility/topology lookup helper that brute-forces point-in-sector using
    extracted loops, or build lookup loops into the visibility graph.
- If no sector contains the camera/player, return invalid start sector and draw
  all sectors as fallback in phase 2.

Later optimizations:

- Current-sector plus neighbor tracking.
- Spatial hash or broad-phase over sector bounds.

## Design: Visibility Traversal

Phase 1 should be conservative and may overestimate:

- Start from `startSectorId`.
- Mark the start sector visible.
- Breadth-first or depth-first traverse open portal edges.
- Maintain visited sector IDs to handle cycles.
- Include a depth cap or iteration cap as a defensive guard, but visited-sector
  tracking should be the primary infinite-loop prevention.
- Optional cheap frustum/view-direction test can skip portals clearly behind or
  outside the camera, but it must not over-cull.
- First implementation can mean "potentially visible through connected open
  portals", not exact portal occlusion.

Correctness rule:

- Over-culling is forbidden.
- Over-rendering is acceptable for the first pass.
- If graph build, sector lookup, or traversal input is invalid, phase 2 should
  draw all sectors and report debug status rather than hide geometry.

Portal screen-span clipping:

- Exact recursive portal clipping is future work.
- The first traversal result can still be useful for connected-component culling
  and debug validation.

## Design: Debug Visibility Output

Keep debug output simple and testable:

- Add a debug status/list string that reports:
  - current/start sector ID
  - visible sector count
  - visible sector IDs
  - invalid-start or fallback-all reason
- In editor 3D overlay, append this to existing preview status when debug mode
  is enabled.
- Optional 2D editor overlay: tint/highlight visible sectors using the existing
  topology render cache sector draw data. This should be debug-only.
- Add non-GUI tests that build a small map, build the visibility graph, run
  traversal, and compare visible sector IDs.

## Design: Draw Culling Integration

Phase 2 should not remesh visible sectors every frame.

Recommended shape:

1. Keep `SectorGeneratedGeometry` as the source surface list.
2. During preview/runtime rebuild, build sector-aware draw records:
   - Either `SectorMeshBatch` per `(sectorId, material key)`, or
     `SectorDrawBatch` records containing a sector ID and uploaded mesh.
   - Reuse the existing material key from `SectorMeshBatchKey`.
   - Reuse `BuildSectorLightmapLayout()` chart UV lookup by original generated
     surface index.
3. Draw path:
   - Compute/update `RuntimePortalVisibilityResult` from the current camera or
     gameplay sector.
   - In `DrawScene()`, draw only sector draw records whose `sectorId` is visible.
   - If visibility result is invalid, draw all sector draw records.
   - Draw sky cylinder/top cap independently.
4. Bloom path:
   - Apply the same visible-sector filter in `RenderBloomSource()`.
5. Picking/highlighting:
   - Keep using `SectorGeneratedGeometry`, but filter by visible sector when
     culling mode is active so picking matches what is drawn.
6. Preserve:
   - Existing texture handles and fallback behavior.
   - Lightmap atlas and UVs.
   - Decal shader behavior.
   - Middle texture alpha-test behavior.
   - Sky behavior.
   - Selected/hovered surface rendering, except filtered to drawn sectors when
     culling is active.

If current global material batching must be kept initially, add a no-op phase 2a
helper that maps generated surfaces to visible/not-visible and reports what
would be drawn. Real culling needs sector-owned draw granularity because current
uploaded global batches cannot skip sector triangles with `DrawMesh()`.

No per-sector dirty mesh rebuild is required for phases 1-2. Rebuild sector draw
batches only when the preview/runtime resources are rebuilt.

## Design: Tests

Future tests should include:

- Build visibility graph for one sector: no portals, one visible sector.
- Build graph for two adjacent sectors: two directed portal edges.
- One-sided walls do not create portals.
- Two-sided linedef creates portal edge both ways.
- Height-closed portal is not traversed.
- Connected open sectors become visible.
- Disconnected sectors are not visible.
- Cycles do not infinite-loop.
- Player/camera sector lookup works for simple sectors.
- Invalid/outside start sector reports fallback/invalid without hiding all
  geometry.
- Draw culling helper selects only visible sector batches.

No GUI automation, screenshot tests, or editor launch tests.

## Goblins / Risks

- Non-convex sectors and holes: point lookup and debug 2D overlays must respect
  outer loops and holes, matching collision/generated geometry semantics.
- Cyclic sector graphs: traversal must use visited IDs and defensive caps.
- Exact portal clipping vs conservative over-rendering: first pass should avoid
  exact clipping and never over-cull.
- Sector ownership under current mesh batching: source surfaces know sector IDs,
  uploaded global material batches do not.
- Lightmap atlas assumptions: culling draw records must preserve original
  generated surface chart UVs and atlas sampling.
- Decals/middle textures/cutout material ordering: sector/material splitting can
  change batch order; verify alpha-tested middle textures and emissive bloom do
  not regress.
- Selected/hovered surface rendering after culling: picking/highlight should
  match visible geometry when culling is enabled.
- Player outside all sectors: draw all sectors as fallback and show debug status.
- Objects spanning multiple sectors later: needs explicit ownership/spanning
  policy, not part of phases 1-2.
- Dynamic sector heights/doors later: graph open intervals and sector draw data
  will need runtime updates, but not in this plan.

## Explicit Out Of Scope

Out of scope for this audit/design and the first implementation plan:

- Linked/remote/non-Euclidean portals.
- Dynamic floors/doors.
- Per-sector dirty mesh rebuild.
- Object/entity sector ownership.
- Changing lightmap baking/hash policy.
- Rewriting the renderer.
- Rewriting mesh generation wholesale.
- Replacing editor UI.
- GUI automation tests.

## Implementation Plan Summary

Phase 1: Sector visibility graph

- Add `SectorPortalVisibility.h/.cpp`.
- Build `RuntimeSectorVisibilityGraph` from valid derived `SectorTopologyMap`.
- Add traversal from start sector through open two-sided linedef portals.
- Add simple debug output/listing of current sector and visible sector IDs.
- Add non-GUI tests for graph build and traversal.
- Do not alter rendering behavior.

Phase 2: Runtime draw culling

- Add sector-aware draw batch data built at preview/runtime rebuild time from
  `SectorGeneratedGeometry`.
- Preserve the existing material key, shader uniforms, texture fallback,
  lightmap UVs, decals, middle alpha test, sky draw, and bloom behavior.
- In `SectorMeshPreview::DrawScene()` and `RenderBloomSource()`, filter sector
  draw records by `RuntimePortalVisibilityResult`.
- On invalid visibility, draw all sectors.
- Add tests for draw batch selection helpers.
- Do not add dynamic mesh rebuilds or object culling.

## Notes On Cache, Lightmap, Collision, And Behavior

This document changes no production code.

Future topology mutations still need existing document/cache invalidation
paths. A visibility graph built from topology should be rebuilt on the same
preview/runtime rebuild boundary as generated geometry, not from the steady 2D
draw path.

The first implementation should not change lightmap source hash policy. Portal
visibility is runtime/draw-selection data; it should not affect baked geometry,
receiver layout, occluders, or baked lighting results.

The first implementation should not change gameplay collision, sector lookup,
or physics. It may reuse current-sector lookup as input, but visual camera
effects must not feed collision or sector lookup.
