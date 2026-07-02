# Sector Door System Audit

Audit date: 2026-07-02

Scope: source inspection and design report only. No door implementation or GUI
verification was performed.

## 1. Current Runtime Object Foundation

Known current code behavior:

- Authored runtime objects live in `SectorTopologyMap::runtimeObjects` as
  `std::vector<SectorPlacedRuntimeObject>` in
  `sources/sector_demo/SectorTopologyMap.h`. Current fields are `id`,
  legacy `definitionId`, `position`, `yawRadians`, `kind`, and a nested
  `SectorPlacedBillboard billboard` payload.
- Current JSON supports `runtimeObjects[].kind = "billboard"` only. The
  billboard JSON payload is read by `ReadPlacedBillboard()` and
  `ReadRuntimeObject()` in `SectorTopologySerialization.cpp`; it writes
  `billboard.spriteAnimationPath`, `width`, `height`, optional
  `keepAspectRatio`, optional `originNormalized`, optional `directional`,
  clip fields, and optional `playing`. Defaults are omitted where existing
  style allows.
- `ReadRuntimeObject()` rejects unknown `kind` values and rejects
  `definitionId`-only legacy objects. `ValidatePlacedBillboard()` rejects
  non-finite or non-positive size and invalid normalized origin.
- `SectorRuntimeObjectState` in `SectorRuntimeObjects.h` stores runtime-object
  bookkeeping: asset scope, placed-object/entity mapping, counts/diagnostics,
  baked object-probe runtime data, object sector lookup `SectorCollisionWorld`,
  and reservation state. It does not store billboard animation state.
- `ResetSectorRuntimeObjectsForMap()` clears ECS entities, refreshes map-side
  runtime data, then calls `SpawnPlacedRuntimeObjects()`.
- `SpawnPlacedRuntimeObjects()` creates ECS entities in `EngineContext::world`
  for authored billboards. It adds `SectorObjectTransform`, `SectorObject`,
  `SectorObjectLighting`, `SectorBillboardSprite`,
  `SectorBillboardAnimator`, and either directional or single clip components.
- `UpdateSectorRuntimeObjects()` advances billboard animation, resolves clips,
  updates current sector through `UpdateSectorObjectCurrentSectorSystem()`,
  samples baked object lighting through
  `UpdateSectorObjectBakedLightingSystem()`, and refreshes diagnostics.
- Editor mutation paths such as `AddRuntimeObjectAt()`,
  `DeleteRuntimeObjectById()`, `MutateSelectedRuntimeObject()`, and
  `RefreshRuntimeObjectsAfterAuthoringEdit()` in `SectorEditor.cpp` mutate
  authored objects, call `MarkTopologyDocumentEdited()`, and respawn runtime
  ECS objects.
- `SectorMeshPreview::Render()` and `DrawScene()` accept an optional
  `engine::World* runtimeObjectWorld`. `SectorMeshPreview` observes and draws
  ECS billboards through `DrawRuntimeBillboards()`; it does not own spawn/reset
  lifecycle.

Recommendation:

- Reuse `SectorTopologyMap::runtimeObjects`, `EngineContext::world`,
  `SectorRuntimeObjectState::placedObjectEntities`, object-probe data, sector
  lookup data, and the explicit reset/refresh lifecycle for doors.
- Do not generalize billboards and doors into an abstract object hierarchy.
  Add narrow door-specific authored payload and ECS components.
- Keep mutable door transform, animation, collider, and portal-block state in
  ECS. `SectorRuntimeObjectState` should only keep mapping, diagnostics, probe
  data, and lookup helpers.
- Preserve `SectorMeshPreview` as renderer-only. It may draw door entities
  supplied through `EngineContext::world`, but it must not spawn, reset, or own
  door entities.

## 2. Current Portal / Topology / Visibility System

Known current code behavior:

- Topology v2 is linedef/sidedef/sector based. `SectorTopologyVertex`,
  `SectorTopologyLineDef`, `SectorTopologySideDef`, and
  `SectorTopologySector` are in `SectorTopologyTypes.h`. Stable positive IDs
  exist for vertices, linedefs, sidedefs, sectors, lights, and runtime objects.
- A `SectorTopologyLineDef` has `startVertexId`, `endVertexId`,
  `frontSideDefId`, `backSideDefId`, and `flags.blocksPlayer`. Two valid
  sidedefs make the linedef two-sided.
- `BuildRuntimeSectorVisibilityGraph()` in `SectorPortalVisibility.cpp`
  creates one `RuntimeSectorNode` per sector and two directed
  `RuntimePortalEdge` records per two-sided linedef. Each edge stores
  `lineDefId`, `sideDefId`, `fromSectorId`, `toSectorId`, world-XZ endpoints
  `a`/`b`, `openBottom`, `openTop`, and `open`.
- `AppendDirectedPortal()` computes the portal vertical interval as
  `max(from.floorZ, to.floorZ)` to `min(from.ceilingZ, to.ceilingZ)` in world
  units. `edge.open` is true when `openBottom < openTop`.
- `TraverseRuntimeSectorVisibility()` does connected traversal and skips
  `!edge.open`.
- `ComputeRuntimeSectorVisibilityFromViewSeeds()` performs view-aware portal
  traversal. It also skips `!edge.open`, then uses
  `TestPortalAgainstWindow()` for angular clipping.
- `ComputeRuntimeSectorVisibilityFromView()` gathers starting sector seeds via
  `SectorCollisionWorld`, then calls the view-seed traversal.
- `SectorMeshPreview::UpdateVisibilityDebug()` is the preview entry point. It
  calls `ComputeRuntimeSectorVisibilityFromView()`, then uses the result for
  sector draw-record filtering and dynamic-light candidate selection.

Recommendation:

- Door anchors can use stable authored `lineDefId` plus either a side or sector
  pair. The current visibility graph already has enough endpoint and sector
  data to resolve a portal segment.
- Add dynamic portal blockers as a separate overlay checked during traversal,
  immediately after `edge.open` and before recording/traversing the edge. This
  avoids rebuilding `RuntimeSectorVisibilityGraph` when a door opens or closes.
- Visibility must prefer false positives over false negatives. A partly open
  or uncertain door should allow traversal and accept overdraw. Incorrectly
  hiding a reachable/visible sector is a bug.

## 3. Door Anchor Model

Recommendation:

- Author doors as runtime objects attached to a two-sided portal segment.
- V1 anchor payload should store:
  - `lineDefId`
  - `frontSectorId`
  - `backSectorId`
  - optional `frontSideDefId` / `backSideDefId` if convenient
  - endpoint coordinates copied from authoring time as a fallback/diagnostic
- The robust v1 resolution path should first find the linedef by ID, verify it
  is still two-sided, verify the current side sectors still match the stored
  sector pair, then compute current endpoints from the linedef vertices. If the
  linedef changed or sectors no longer match, the door should fail closed for
  collision/portal blocking if an ECS entity exists, or fail to spawn with an
  editor warning if resolution cannot produce a sane transform.
- Endpoint coordinates are useful for diagnostics and possible migration, but
  should not be the primary identity because exact coordinates can collide or
  change after edits.
- A sector-pair plus segment endpoints is a good fallback descriptor for human
  warnings, but not enough as the primary saved identity when multiple portals
  connect the same sectors.

Door local basis:

- Tangent: normalized portal segment from endpoint A to B in world XZ.
- Normal: perpendicular across the portal, oriented from chosen front/from
  sector toward back/to sector. Derive from the directed sidedef/portal edge
  orientation and verify against sector ownership.
- Up: world Y.
- Origin: bottom-center of the portal opening is most useful for slab meshes
  and colliders. The horizontal origin is the portal midpoint; vertical origin
  is `openBottom`. A midpoint-origin can be derived for rendering if needed.

Variable thickness:

- The portal remains a zero-thickness logical line.
- Door slab thickness expands along the local normal. Default is centered on
  the portal plane, half thickness into each sector.
- A future `normalOffset` can make flush/inset doors without changing the
  anchor. Positive offset should move toward the chosen normal.
- Width defaults to portal segment length. Height defaults to the current
  vertical opening `openTop - openBottom`. Explicit width/height can override
  after validation.

## 4. Door Frames Versus Door Slabs

Known current code behavior:

- Static sector wall/lower/upper/middle geometry is generated from topology by
  `BuildSectorGeneratedGeometry()` and drawn as sector mesh batches.
- Middle textures are visual portal-plane surfaces and can be paired with
  `blocksPlayer`, but they are not dynamic door meshes.

Recommendation:

- V1 should implement the moving slab only. Skip decorative frames initially.
- If a minimal frame is needed for readability, make it a non-moving procedural
  companion mesh/component owned by the door runtime object, not a sector mesh
  mutation.
- Later frame support can reuse the same anchor and basis: static trim pieces
  sit around `openBottom/openTop` and segment endpoints while the slab moves.
- Editor diagnostics should warn when `thickness` plus `normalOffset` overlaps
  nearby walls or when the portal opening is too narrow/short for the door.
  Different wall-thickness illusions are an art/material issue; the physical
  door thickness is independent of zero-thickness portals.

## 5. Authored Door Data Model

Recommendation:

Use `runtimeObjects` with `kind = "door"` and a nested `door` payload:

```json
{
  "id": 42,
  "kind": "door",
  "door": {
    "anchor": {
      "lineDefId": 17,
      "frontSectorId": 3,
      "backSectorId": 4,
      "frontSideDefId": 31,
      "backSideDefId": 32,
      "endpointA": [0, 64],
      "endpointB": [64, 64]
    },
    "width": 4.0,
    "height": 2.5,
    "thickness": 0.25,
    "normalOffset": 0.0,
    "motion": "slide_vertical",
    "openDistance": 2.5,
    "speed": 1.5,
    "initialOpenFraction": 0.0,
    "textureId": "industrial_wall_01"
  }
}
```

Notes:

- `endpointA/B` should use exact topology coordinates (`SectorCoord`) if saved,
  not lossy world floats.
- `width`, `height`, and `openDistance` may be omitted to default from the
  resolved portal opening. `thickness` should have a positive default, for
  example 0.25 world units.
- Prefer `initialOpenFraction` over both `startsOpen` and a bool. It supports
  starts-closed, starts-open, and test/debug partially-open states. The editor
  can expose "Starts Open" as a convenience that writes 0 or 1.
- Defer locked/trigger/script fields unless v1 interaction explicitly needs
  them. A debug toggle can be editor/runtime-only and not authored.
- Minimal v1 material can be a single `textureId` for all faces. Defer six-face
  slots and UV editing.
- Future save-game state is separate from authored level data. Current
  `openFraction`, target state, timers, obstruction state, and interaction state
  belong in ECS or a future save-state layer, not in `SectorTopologyMap`.

Validation rules:

- Reject non-positive IDs, duplicate runtime object IDs, unknown door motion
  strings, non-finite numbers, negative dimensions, zero/negative thickness,
  out-of-range `initialOpenFraction`, and invalid anchor field types.
- During load, structural anchor existence may be validated strictly if topology
  data is already loaded. During editor topology mutation, invalid anchors
  should become warnings and disabled/spawn-skipped doors rather than crashing.
- Unknown `kind` should remain rejected unless the project adopts an extension
  policy.

## 6. Runtime ECS Door Model

Known current code behavior:

- Reusable ECS components for sector runtime objects are
  `SectorObjectTransform`, `SectorObject`, and `SectorObjectLighting`.
- `SectorBillboard*` components are specific to sprite billboards and should
  not be reused for door state.

Recommendation:

- Add narrow door components, names matching current project style:
  - `SectorDoor`: authored object ID, resolved validity, and high-level enabled
    state.
  - `SectorDoorAnchor`: resolved line/sidedef/sector IDs, endpoints, basis,
    bottom/top opening, default dimensions.
  - `SectorDoorMotion`: `openFraction`, `targetOpenFraction`, speed,
    open distance, motion type.
  - `SectorDoorRender`: dimensions, texture/material handle or texture ID
    handle, tint, visibility.
  - `SectorDoorCollider`: current OBB footprint, vertical interval, enabled
    flag.
  - `SectorDoorPortalBlocker`: line/sidedef/sector pair and current blocking
    state.
- `openFraction` and target animation state must live in ECS
  `SectorDoorMotion`.
- `SectorRuntimeObjectState` may count doors, map placed object IDs to
  entities, and report invalid-anchor diagnostics. It must not mirror mutable
  door animation or transform state.

Update responsibilities:

- Door control system sets `targetOpenFraction` from debug/interaction input.
- Motion system advances `openFraction`.
- Transform system derives slab transform from anchor, dimensions, and motion.
- Collider system updates the OBB/vertical interval from the current transform.
- Portal blocker system updates closed/open blocking state from `openFraction`.
- Existing object-sector and baked-lighting systems can update
  `SectorObject::currentSectorId` and `SectorObjectLighting` using the door
  transform/sample position.

## 7. Procedural Door Rendering

Known current code behavior:

- Static sector geometry is drawn by `SectorMeshPreview::DrawScene()` from
  `meshes.sectorDrawRecords` using a lightmap/dynamic-light shader.
- Runtime billboards are drawn after sector batches by
  `DrawRuntimeBillboards()` using immediate `rlBegin(RL_QUADS)` with a cutout
  shader, no blending, depth test/write enabled, and dynamic light uniforms.

Recommendation:

- Render procedural doors as opaque 3D boxes/slabs from ECS, not as generated
  sector surfaces.
- V1 can add a `DrawRuntimeDoors()` pass in `SectorMeshPreview` that observes
  `EngineContext::world`, similar to `DrawRuntimeBillboards()`.
- Avoid allocating/uploading a unique mesh every frame. Either:
  - use immediate-mode quads for the first slab path, or
  - keep one unit-box `Mesh` owned by `SectorMeshPreview` renderer resources
    and draw it with per-entity transforms.
- Reusing the sector lightmap shader directly is awkward because doors have no
  lightmap UVs. A small 3D object shader should support base texture, per-face
  normals, object-probe ambient cube/normal lookup, and dynamic lights. It can
  share helper shader code/uniform packing patterns with sector/billboard
  shaders.
- Material v1: one texture for all faces. Front/back plus edge texture is a
  reasonable next step. Defer full six-face material and UV editor.
- Doors should be opaque or alpha-cutout, depth-write, and not in a transparent
  pass. Defer glass/translucent doors.
- Lighting v1: sample `SectorObjectLighting` from object probes, evaluate
  ambient by slab face normal from the ambient cube, then add dynamic point/spot
  lights. Do not lightmap moving doors.

## 8. Door Motion Types

Recommendation:

- Keep the authored enum flexible, but implement a small v1:
  - `slide_vertical`
  - `slide_left`
  - `slide_right`
- Defer split doors and hinged doors.
- `openFraction` semantics:
  - 0 means fully closed.
  - 1 means fully open.
  - `targetOpenFraction` is the commanded target.
  - `speed` is fraction per second or world units per second. Prefer world
    units per second plus `openDistance`, because it gives predictable timing
    across different door sizes.
- Transform derivation:
  - `slide_vertical`: translate slab up by `openFraction * openDistance`.
  - `slide_left/right`: translate along +/- tangent by
    `openFraction * openDistance`.
  - Closed transform is centered on the portal plane plus `normalOffset`.
- Hinged doors later need hinge side, pivot transform, rotating OBB collider,
  collision during closing, approximate portal blocker logic, and a decision
  about pushing/blocking the player.

## 9. Dynamic Collision Integration

Known current code behavior:

- `SectorCollisionWorld::BuildFromTopology()` builds static collision sectors
  from topology loops. Edges are either `BlockingWall` or `Portal`; two-sided
  edges record neighbor sectors and `blocksPlayer`.
- `SectorCollisionWorld::ResolveMovement()` resolves player XZ movement by
  pushing a cylinder footprint away from blocking walls or portals blocked by
  player flag, step height, or ceiling height.
- The current player collision path is topology-based, not mesh-triangle based.

Recommendation:

- Add a dynamic collider layer after static sector movement. It should take the
  static movement result and then resolve against enabled dynamic blockers
  collected from ECS.
- V1 door collider shape: 2D rectangle/OBB footprint from tangent, normal,
  width, thickness, and current translation, with vertical interval
  `[bottom, bottom + height]`.
- Player shape remains a cylinder/circle in XZ with feet/head Y interval.
- Dynamic blockers should live in ECS components and be collected each frame
  into a small reserved vector for collision queries. This path can later serve
  NPCs, push blocks, lifts, and other movable blockers.
- V1 acceptable behavior:
  - Closed doors block.
  - Open doors with `openFraction > epsilon` may either keep collider enabled
    until sufficiently open or disable when clear; choose conservative blocking
    for collision.
  - If player starts inside a door, push out using the least-penetration axis
    or leave position unchanged and mark blocked. Do not crash.
  - If a door closes into the player, stop closing or reopen. Pushing the
    player can be deferred.
- Risks/goblins:
  - Closing while the player crosses the portal.
  - Thick doors overlapping adjacent static walls.
  - Current-sector update disagreeing with a dynamic door blocker.
  - Dynamic collision order causing jitter near static portal edges.

## 10. Dynamic Portal Visibility Blocking

Known current code behavior:

- Static `RuntimeSectorVisibilityGraph` stores portal edges and static vertical
  openness. Traversal currently checks `edge.open` only.
- `SectorMeshPreview::UpdateVisibilityDebug()` computes one visibility result
  and reuses it for draw filtering and dynamic-light selection.

Recommendation:

- Keep the static graph intact. Add a dynamic gate overlay keyed by portal
  identity: `lineDefId` plus from/to sector or sidedef ID.
- Closed door can block traversal. Open or partly open door should allow
  traversal conservatively.
- V1 threshold: `openFraction <= 0.001f` blocks; `openFraction > 0.001f`
  allows visibility traversal. This creates false positives/overdraw while the
  door is barely open, which is acceptable.
- Wire the lookup into both `TraverseRuntimeSectorVisibility()` and
  `ComputeRuntimeSectorVisibilityFromViewSeeds()` after `edge.open` and before
  `TestPortalAgainstWindow()` / traversal recording.
- Do not rebuild `RuntimeSectorVisibilityGraph` when doors animate.
- Door state should not affect static 3D picking data, generated surface
  metadata, or bloom source filtering in v1. If doors later emit bloom or
  become pickable in 3D, that should be a runtime-object picking/render pass,
  not static sector metadata.

## 11. Editor Door Tool And Inspector

Recommendation:

- V1 workflow:
  - Add a `Door` tool.
  - Click a valid two-sided portal segment.
  - Create `runtimeObjects[]` entry with `kind = "door"` anchored to that
    portal.
  - Draw a 2D marker/footprint showing slab width and thickness.
  - Select, delete, inspect, and refresh the ECS door like existing runtime
    objects.
- Treat doors as runtime objects with portal-attached anchors, not as sidedef
  material edits. Selection can use the runtime-object selection bucket, with
  door-specific hit testing biased toward the portal footprint.
- Inspector fields:
  - Object ID
  - Type: Door
  - Anchor status: valid/invalid, line ID, sector pair
  - Width, height, thickness
  - Normal offset if included in v1
  - Motion type
  - Open distance
  - Speed
  - Starts open / initial open fraction
  - Debug open toggle if runtime preview supports it
  - Texture/material if v1 includes materials
- UI should follow existing compact inspector patterns. Use small/secondary
  editor font for key:value/status text, wrap diagnostics, and avoid long
  overflowing labels.
- Invalid anchors/topology changes should show a 2D warning marker near the
  last known segment and an inspector diagnostic. Spawning should skip invalid
  doors or create disabled entities with clear diagnostics.

## 12. Door Materials / Texture / UV Editing

Known current code behavior:

- Texture definitions are map-local IDs in `SectorTopologyMap::texturesById`.
- The editor has texture picker infrastructure in
  `SectorEditorTextureModals.*` and `SectorEditorTextureActions.cpp`.
  `PopulateTexturePickerOptions()` uses `SortedSectorTopologyTextureIds()`.
- Existing material edit paths in `SectorEditorMaterialActions.cpp` and
  inspector code support floor/ceiling/wall/lower/upper/middle texture IDs,
  UV scale/offset, decals, fit/reset, and middle clearing.

Recommendation:

- V1 door material should use one `textureId` for all faces. Reuse the existing
  topology texture picker option list and missing-texture diagnostics.
- Defer per-face UV editing. Generate procedural UVs from slab dimensions:
  front/back U by width and V by height; sides by thickness/height or
  width/thickness.
- Next material step: `frontTextureId`, `backTextureId`, `edgeTextureId` with
  shared UV scale/offset.
- Full six-face material slots and per-face UV editor should be deferred until
  there is real art pressure.
- Saved material data needs texture IDs and optional UV scale/offset. Do not
  save raylib texture handles.
- Risks: sampler/filter consistency with map textures, alpha-test state if
  using cutout doors, depth state matching opaque sector geometry, and shader
  divergence between sector objects and static sector meshes.

## 13. Lighting And Object Probes

Known current code behavior:

- Baked object probes are loaded into
  `SectorRuntimeObjectState::objectLightProbes` by
  `RefreshSectorRuntimeObjectMapData()`.
- `SampleBakedObjectLighting()` in `SectorLightmap.cpp` samples nearby object
  probes, blends adjacent sectors near portals, falls back to sector ambient,
  then to neutral lighting.
- `SectorObjectLighting` stores `BakedObjectLightingSample`, which contains an
  ambient cube.
- Billboards use `BakedBillboardLighting()` in `SectorMeshPreview.cpp`, which
  averages selected ambient-cube faces. This is billboard-specific.
- Dynamic point/spot light uniforms are selected in
  `SectorMeshPreview::UpdateVisibilityDebug()` and uploaded for sector and
  billboard shaders.
- `ComputeSectorLightmapSourceHash()` includes topology, sector geometry,
  texture/material/lightmap inputs, static lights, directional light, and
  `ceilingSky`. Runtime object placements are not included.

Recommendation:

- V1 3D doors should sample object probes at the slab center, likely the center
  of the closed or current transformed slab.
- Use the ambient cube by face normal for procedural box faces instead of the
  billboard upper-hemisphere average.
- Add dynamic lights on top using existing dynamic light uniform selection and
  shader upload helpers/patterns.
- Door dynamic shadow casting can be deferred. Doors receiving dynamic light is
  enough for v1.
- Closed doors should not affect baked lighting in v1. Lightmap source hash
  should remain unchanged unless a later deliberate static/baked interaction is
  added, such as authored static closed-door occlusion.

## 14. Suggested Incremental Runner Plan

Future runner phases should reread this audit sections 3, 5, 6, 9, 10, and 13
before touching implementation.

Phase 1: authored door data + serialization only

- Likely files/functions: `SectorTopologyMap.h`, `SectorTopologySerialization.cpp`,
  `SectorTopologySerializationTests.cpp`, `SectorRuntimeObjectTests.cpp`.
- Work: add `SectorPlacedDoor`, extend `SectorPlacedRuntimeObject`, read/write
  `kind = "door"`, validate fields, add round-trip tests.
- Risks/goblins: breaking billboard serialization defaults; rejecting old maps;
  over-validating topology anchors before editor repair can run.
- Tests: generated JSON round-trip, invalid motion, invalid dimensions,
  duplicate IDs, default field omission.

Phase 2: portal anchor resolution and editor diagnostics

- Likely files/functions: `SectorTopologyMap.*`, `SectorPortalVisibility.*`,
  `SectorRuntimeObjects.*`, editor cached runtime-object draw code.
- Work: helper to resolve door anchor to endpoints, basis, vertical interval;
  diagnostics for invalid anchors.
- Risks/goblins: front/back normal sign, edited topology IDs, equal-height and
  zero-height openings.
- Tests: anchor resolves on two-sided portal, rejects one-sided wall, fails
  safely after sector mismatch.

Phase 3: runtime ECS door spawn with closed procedural slab render

- Likely files/functions: `SectorRuntimeObjects.*`, `SectorMeshPreview.*`,
  CMake test targets if new files are added.
- Work: add door ECS components, spawn closed slabs, draw opaque procedural box.
- Risks/goblins: per-frame mesh allocation, shader state leaking into sector or
  billboard draw, missing texture fallback.
- Tests: spawn count diagnostics, ECS component existence, render path handles
  missing texture without crash.

Phase 4: door animation/openFraction for slide_vertical/slide_horizontal

- Likely files/functions: `SectorRuntimeObjects.*`.
- Work: add motion update and transform derivation for `slide_vertical`,
  `slide_left`, `slide_right`.
- Risks/goblins: authored units versus world units, variable open distance,
  target state persistence confusion.
- Tests: deterministic openFraction advancement, transform at 0/0.5/1.

Phase 5: dynamic collision blockers

- Likely files/functions: `SectorCollisionWorld.*`, `SectorFpsController.*`,
  `SectorRuntimeObjects.*`, movement tests.
- Work: collect ECS door colliders, resolve player cylinder against dynamic
  OBBs after static collision.
- Risks/goblins: jitter at portal planes, player starts inside slab, door
  closing into player.
- Tests: closed door blocks, open door allows, partial open policy, vertical
  interval rejects non-overlap.

Phase 6: dynamic portal visibility blockers

- Likely files/functions: `SectorPortalVisibility.*`, `SectorMeshPreview.*`,
  `SectorPortalVisibilityTests.cpp`.
- Work: add optional dynamic blocker lookup to traversal APIs and preview call.
- Risks/goblins: false negatives, directed-edge key mismatch, stale blocker
  data after ECS updates.
- Tests: closed door hides sector in traversal, partly open allows traversal,
  static graph count unchanged.

Phase 7: Door editor tool + inspector

- Likely files/functions: `SectorEditorTypes.h`, `SectorEditor.cpp`,
  `SectorEditorHelpers.cpp`, `SectorEditorTopologyRenderCache.*`,
  UI layout tests.
- Work: portal click placement, 2D footprint, selection, inspector fields,
  invalid-anchor warnings.
- Risks/goblins: long labels, picking disagreement with cached drawing,
  missed cache invalidation.
- Tests: UI layout height, add/delete mutation, cache contents after mutation.

Phase 8: materials/texture/UV v1

- Likely files/functions: `SectorEditorTextureActions.*`,
  `SectorEditorTextureModals.*`, `SectorMeshPreview.*`,
  serialization tests.
- Work: one texture ID, picker integration, generated UVs.
- Risks/goblins: missing texture IDs, sampler mismatch, shader state.
- Tests: texture round-trip, picker target apply, missing texture fallback.

Phase 9: tests/docs/cleanup

- Likely files/functions: docs, focused tests, no broad refactor.
- Work: update `docs/sector_editor.md`, add focused unit tests, run normal
  checks.
- Risks/goblins: mixing feature cleanup with unrelated `SectorEditor.cpp`
  extraction.
- Tests: full requested project checks.

## 15. Non-Goals / Guardrails

- No dynamic sector-height doors in v1.
- No partial sector mesh rebuilds for doors in v1.
- No glTF/model loading in v1.
- No NPC AI, combat, scripting, pickups, inventory, or party behavior.
- No broad ECS conversion of sector topology.
- No full 3D physics engine.
- No transparent door/glass pass.
- No fragile GUI automation.
- No new F-key driven debug UI unless explicitly requested.

## 16. Open Questions / Recommendations

Decisions needing human review:

- Should v1 include both `slide_vertical` and one horizontal slide direction, or
  start with vertical only?
- Should door `speed` be world units per second or fraction per second?
- Should invalid anchors skip spawning entirely or spawn disabled diagnostic
  entities?
- Should `normalOffset` ship in v1 or be deferred until flush/inset art exists?
- What default `textureId` should new doors use when the map has several wall
  textures?
- Should a closed door with `openFraction <= epsilon` block visibility only, or
  should collision remain blocked until a larger clearance threshold?

Recommended v1 defaults:

- Centered slab on the zero-thickness portal plane.
- `slide_vertical` first; add `slide_left`/`slide_right` only if the first pass
  stays small.
- One material/texture for all faces.
- Closed blocks collision and portal traversal.
- Partially open allows visibility traversal conservatively.
- Dynamic collision can remain conservative longer than visibility.

Recommended first implementation phase:

- Start with Phase 1, authored door data and serialization only. It is the
  smallest executable pass, establishes schema review points, and does not
  touch renderer, collision, portal traversal, or editor interaction yet.

