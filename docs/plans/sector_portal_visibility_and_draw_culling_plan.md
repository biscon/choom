# Sector Portal Visibility and Draw Culling Plan

This is a runner-compatible implementation plan for adding sector-based portal visibility and draw culling to the 3D preview/runtime rendering path.

The initial goal is to get visibility culling working for level geometry. Future work such as dynamic sector heights, doors, per-sector dirty mesh rebuilds, and object/entity culling is intentionally deferred.

Design reference:

`docs/sector_portal_visibility_design.md`

This plan assumes the completed authoring-graph transition:

* `AuthoringGraph` is editor source of truth.
* `SectorTopologyMap` is derived/runtime topology.
* Runtime/preview/collision/lightmap systems consume valid derived topology.
* Portal visibility should be built from derived topology, not directly from authoring graph.

```plan-state-json
{
  "plan_id": "sector_portal_visibility_and_draw_culling_plan",
  "items": [
    {
      "id": "phase_01_sector_portal_visibility_graph",
      "title": "Build sector portal visibility graph and traversal",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_02_visibility_debug_integration",
      "title": "Integrate current-sector lookup and visibility debug output",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_03_sector_aware_mesh_draw_records",
      "title": "Build sector-aware mesh draw records",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_04_draw_scene_and_bloom_culling",
      "title": "Cull DrawScene and bloom rendering by visible sectors",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_05_pick_and_highlight_visibility_filter",
      "title": "Filter 3D picking and highlights by visible sectors",
      "type": "phase",
      "status": "Planned",
      "parent": null
    }
  ]
}
```

## Global Runner Instructions

Implement only the selected phase.

If a selected phase is too broad to implement safely in one pass:

* split only that selected phase into focused child pass items
* keep child items directly under the selected phase using `parent`
* mark the parent phase `In Progress`
* leave new child items `Planned`
* stop after updating the plan
* do not modify unrelated future phases

Do not create third-level plan items.

Before each phase, read:

* `docs/sector_portal_visibility_design.md`
* `docs/topology_audits/06_authoring_graph_transition_design.md`

General constraints:

* Keep changes narrow.
* Prefer pure data/functions/tests before rendering changes.
* Do not rewrite the renderer.
* Do not rewrite mesh generation wholesale.
* Do not change graph-native save/load.
* Do not change authoring graph derivation.
* Do not change collision/physics behavior.
* Do not change lightmap baking or source hash policy.
* Do not add dynamic floors/doors in this plan.
* Do not add linked/remote/non-Euclidean portals.
* Do not add object/entity ownership culling in this plan.
* Do not add GUI automation tests.
* Do not use `xdotool`.
* Do not add screenshot tests.
* Do not launch the game/editor as an interactive GUI test.
* All Codex-run verification must be CMake/CTest or non-interactive helper tests.

Important map-object note:

* The only current map objects are static lights.
* Static lights are map-level data used by baked lighting/source hash and editor display.
* This plan should not introduce a full object/entity ownership model.
* Future 3D objects/models/particles may use visible-sector ownership later, but that is out of scope here.

Visibility correctness rule:

* Over-rendering is acceptable in early phases.
* Over-culling is not acceptable.
* If visibility data is invalid, missing, or uncertain, draw all sectors as fallback.

## Phase 1: Build sector portal visibility graph and traversal

Selected item:

`phase_01_sector_portal_visibility_graph`

Goal:

Add a pure runtime sector visibility graph built from valid derived `SectorTopologyMap`.

This phase should not change rendering behavior.

Add small reusable files, likely under `sources/sector_demo/`, for example:

* `SectorPortalVisibility.h`
* `SectorPortalVisibility.cpp`

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
    bool fallbackDrawAll;
    std::string status;
};
```

Exact names may follow local style.

Required graph build behavior:

* Input is a valid derived `SectorTopologyMap`.
* Build one node per sector.
* Use normal two-sided linedefs only.
* A valid two-sided linedef creates two directed portal edges.
* One-sided linedefs create no portal edge.
* Resolve front/back sidedefs and sector IDs through topology references.
* Store portal segment endpoints in world XZ or clearly documented topology/world units.
* Compute open vertical interval:

    * `openBottom = max(floorA, floorB)`
    * `openTop = min(ceilingA, ceilingB)`
* Mark portal closed/non-traversable if `openBottom >= openTop`.
* Do not treat `blocksPlayer` as a visibility blocker in this phase.
* Do not add linked/remote portal support.

Required traversal behavior:

* Given a start sector ID, produce visible sector IDs.
* Start sector is always visible if valid.
* Traverse through open portal edges.
* Use visited-sector tracking to prevent cycles.
* Include a defensive iteration/depth cap.
* First pass may be conservative connected-portal visibility.
* Do not do exact recursive portal clipping yet.
* Invalid start sector should return `validStartSector=false` and `fallbackDrawAll=true`.

Tests:

Create `tests/SectorPortalVisibilityTests.cpp` or equivalent.

Cover:

* one sector: no portals, start sector visible
* two adjacent sectors: two directed portal edges
* one-sided wall: no portal edge
* two-sided linedef: portal edge both ways
* height-closed portal is not traversed
* connected open sectors become visible
* disconnected sectors are not visible
* cyclic graph traversal terminates
* invalid start sector reports fallback draw-all
* malformed topology references fail cleanly or produce a clear invalid result according to existing validation style

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Completion criteria:

* visibility graph builds from derived topology
* traversal works in non-GUI tests
* rendering behavior unchanged
* no mesh/collision/lightmap behavior changed
* selected plan item marked Completed

## Phase 2: Integrate current-sector lookup and visibility debug output

Selected item:

`phase_02_visibility_debug_integration`

Goal:

Wire the visibility graph into the 3D preview/runtime state enough to compute and inspect visible-sector results, without changing draw culling yet.

This phase should still draw the whole level.

Required behavior:

* Build/store visibility graph at the same preview/runtime rebuild boundary as generated geometry.
* Use current valid derived topology.
* Compute visible sectors from the current camera/player sector.
* Use existing sector lookup where practical:

    * gameplay preview current sector if available
    * `SectorCollisionWorld::FindSectorContainingPoint()` if collision world is available
    * brute-force safe fallback helper if needed
* If the camera/player is outside all sectors, mark invalid start and use draw-all fallback.
* Add simple debug output:

    * current/start sector ID
    * visible sector count
    * visible sector IDs
    * fallback reason if any
* Keep output unobtrusive and consistent with existing preview/debug status style.

Optional, only if low risk:

* Add a debug 2D/editor overlay tint or label for visible sectors.
* Do not make this required if it risks UI churn.

Required constraints:

* Do not change actual rendered sector set yet.
* Do not alter collision behavior.
* Do not alter camera movement.
* Do not alter lightmap behavior.
* Do not alter editor authoring behavior.

Tests:

* current-sector lookup helper returns expected sector for simple map
* outside-map lookup reports invalid/fallback
* visibility debug result reports visible sector IDs deterministically
* visibility result remains valid across connected sectors
* fallback draw-all result is produced when lookup fails
* no GUI automation

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Manual user-only smoke suggestions, not for Codex:

* enter 3D preview in a multi-sector map
* enable/view debug visibility text
* move between sectors
* confirm current sector and visible-sector list update
* confirm rendered scene still draws everything

Completion criteria:

* preview/runtime can compute visible sector set
* debug output exists
* draw behavior remains unchanged
* selected plan item marked Completed

## Phase 3: Build sector-aware mesh draw records

Selected item:

`phase_03_sector_aware_mesh_draw_records`

Goal:

Add sector-owned draw granularity while preserving existing material/lightmap/decal behavior.

Current audit finding:

* `SectorGeneratedSurface::ref` retains sector, linedef, sidedef, side, and surface kind before batching.
* Uploaded `SectorMeshBatch` objects are global material/decal/lightmap batches and do not retain sector ranges.
* Current `DrawMesh()` usage cannot skip only a sector’s triangles inside a global batch.

Therefore, real draw culling needs sector-aware draw records.

Required behavior:

* At preview/runtime rebuild time, build sector-aware uploaded mesh batches from `SectorGeneratedGeometry`.
* Prefer grouping by `(sectorId, existing material/decal/lightmap key)`.
* Reuse the existing material key/batching logic as much as practical.
* Preserve:

    * base texture behavior
    * decal texture/uniform behavior
    * decal opacity/emissive/tint/bloom behavior
    * alpha-test state/cutoff
    * receives-lightmap flag
    * vertex color/ambient behavior
    * lightmap UVs from original generated surface chart indices
    * middle texture alpha-test behavior
* Store enough metadata per draw record:

    * sector ID
    * mesh handle
    * material/batch data
    * emissive/decal/bloom flags
* Keep existing global batch path only if needed during transition, but avoid duplicated rendering.
* For this phase, still draw all sector draw records.

Important:

* Do not remesh visible sectors every frame.
* Build these records only on preview/runtime resource rebuild.
* Do not implement per-sector dirty rebuild yet.
* Do not change lightmap source hash.
* Do not change generated surface content.

Possible data shape:

```cpp
struct SectorMeshDrawRecord {
    int sectorId;
    Mesh mesh;
    SectorMeshBatchMaterial material;
    bool alphaTest;
    bool receivesLightmap;
    bool emissiveDecal;
};
```

Exact names should follow nearby code.

Tests:

* sector-aware draw records are built for a one-sector map
* two sectors with same material produce separate sector-owned draw records or otherwise retain sector ownership
* material/decal/lightmap grouping still preserves material key behavior
* lightmap UVs are copied using original generated surface indices
* middle texture alpha-test draw records preserve alpha-test settings
* emissive decal draw records preserve bloom-relevant metadata
* existing mesh builder tests continue to pass
* no rendering/GUI tests

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Manual user-only smoke suggestions, not for Codex:

* enter 3D preview
* confirm scene still renders like before
* confirm textured walls/floors/ceilings still appear
* confirm middle alpha-test textures still appear
* confirm baked lightmap still appears
* confirm emissive decal bloom still appears if test map has one

Completion criteria:

* sector-owned draw records exist
* all sectors still render
* existing visual/material behavior intended to be preserved
* draw culling not enabled yet
* selected plan item marked Completed

## Phase 4: Cull DrawScene and bloom rendering by visible sectors

Selected item:

`phase_04_draw_scene_and_bloom_culling`

Goal:

Use the visibility result to draw only visible sector draw records.

Required behavior:

* In `SectorMeshPreview::DrawScene()` or equivalent:

    * compute/use current `RuntimePortalVisibilityResult`
    * draw only sector draw records whose `sectorId` is visible
    * if visibility is invalid or fallbackDrawAll is true, draw all sector draw records
* Sky cylinder/top cap rendering remains independent of sector visibility.
* Apply the same visible-sector filter in bloom/emissive decal rendering.
* Hidden sectors should not contribute emissive bloom.
* Preserve shader/material/texture/lightmap behavior.
* Preserve draw-all fallback.

Debug behavior:

* expose enough status/debug information to verify:

    * current sector
    * visible sectors
    * number of draw records drawn vs total draw records
    * fallback reason if any

Important correctness rule:

* Over-rendering is acceptable.
* Missing visible geometry is not acceptable.
* If uncertain, fallback to drawing all.

Tests:

* draw selection helper returns all records when fallbackDrawAll is true
* draw selection helper returns only visible sector records when visibility is valid
* draw selection helper handles empty visible set safely
* bloom selection uses same visible-sector filtering
* sky draw is not included in sector filtering helper
* invalid start sector draws all records
* disconnected sectors are not selected when not visible
* no GUI automation

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Manual user-only smoke suggestions, not for Codex:

* create/load a multi-sector map with disconnected or portal-separated regions
* enter 3D preview
* enable visibility debug output
* move between sectors
* confirm visible-sector list changes
* confirm hidden disconnected sectors are not drawn
* confirm no missing adjacent visible geometry
* confirm sky still renders
* confirm bloom does not appear from hidden sectors

Completion criteria:

* sector draw culling works in 3D preview/runtime draw path
* fallback draw-all is safe
* bloom uses same visibility filter
* selected plan item marked Completed

## Phase 5: Filter 3D picking and highlights by visible sectors

Selected item:

`phase_05_pick_and_highlight_visibility_filter`

Goal:

Make 3D surface picking and hover/selection highlights respect visible-sector culling, so editor interaction matches what is drawn.

Current behavior:

* 3D picking/highlighting uses `preview.RenderedGeometry().surfaces`.
* That generated geometry includes all sectors.

Required behavior:

* When sector draw culling is active and visibility result is valid:

    * ray picking should ignore generated surfaces whose `topologySectorId` is not visible
    * hover highlight should only draw for visible sector surfaces
    * selected surface highlight should only draw if its surface sector is visible
* When visibility result is invalid/fallback draw-all:

    * picking/highlighting can consider all generated surfaces as before
* Keep existing authoring mapping behavior for selected surfaces.
* Do not alter inspector write-through behavior.
* Do not alter generated geometry.
* Do not alter mesh/lightmap/rendering behavior beyond picking/highlight filtering.

Tests:

* visible-sector surface filter includes only surfaces with visible sector IDs
* fallback draw-all includes all surfaces
* picking helper ignores hidden-sector surfaces when culling active
* selected/hovered surface highlight helper suppresses hidden-sector surfaces
* authoring mapping still works for visible picked surfaces
* no GUI automation
* no screenshot tests

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Manual user-only smoke suggestions, not for Codex:

* enter 3D preview with culling active
* look at/draw through visible sectors
* confirm hover/select works on visible walls/floors
* confirm hidden sectors cannot be picked through walls
* move to another sector and confirm picking/highlighting updates
* confirm inspector still maps picked surfaces to authoring anchors/sides

Completion criteria:

* picking/highlighting matches visible geometry
* fallback behavior remains safe
* authoring inspector mapping still works for visible surfaces
* selected plan item marked Completed

## Final Plan Completion Criteria

This plan is complete when:

* a portal visibility graph can be built from derived topology
* visibility traversal produces deterministic visible sector sets
* debug visibility status exists
* sector-aware draw records preserve existing material/lightmap/decal behavior
* 3D preview/runtime draw path can draw only visible sector records
* bloom uses the same visibility filter
* picking/highlighting respects visible-sector culling
* invalid visibility state falls back to draw-all
* no graph-native save/load behavior changed
* no lightmap baking/source hash policy changed
* no collision/physics behavior changed
* no dynamic floors/doors implemented
* no object/entity ownership system added
* tests pass through CMake/CTest/non-interactive harnesses
