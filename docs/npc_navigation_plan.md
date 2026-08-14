# NPC Navigation And Pathfinding Plan

## How To Use This Living Plan

This document is the execution plan and compacted investigation record for NPC
navigation, pathfinding, scripted movement, door traversal, dynamic obstacles,
and eventual local avoidance. The selected foundation is Recast/Detour, using
the source dependencies under `sources/recast` and `sources/detour`.

When executing this plan:

1. Read this document, `AGENTS.md`, and
   `docs/architecture/sector_editor_architectural_principles.md` before making
   changes.
2. Execute only the requested slice. Do not silently combine slices or begin
   the next slice.
3. Set the selected slice to **In Progress** before implementation. On the same
   run, update it to **Completed**, **Partial**, or **Blocked** and append an
   execution-log entry with the date, changes, tests, decisions, and remaining
   debt.
4. Treat the contracts and decisions below as the current specification. When
   implementation reveals an incorrect assumption, update the relevant
   section before recording the slice result so this remains useful in a fresh
   context.
5. Keep every completed slice buildable. Preserve existing maps, NPC
   definitions, non-navigating NPC behavior, player collision, and runtime
   object behavior.
6. Keep new feature code in focused files. Do not accumulate the navigation
   implementation in `SectorEditor.cpp`, the preview overlay, scene runtime, or
   script bindings.
7. Do not perform interactive or xdotool smoke tests; the user owns manual GUI
   verification. Do not claim manual verification unless the user reports it.
8. At the end of every slice run the project checks unless the execution log
   records why one was inapplicable:

   ```sh
   cmake --build cmake-build-debug -j2
   ctest --test-dir cmake-build-debug --output-on-failure
   git diff --check
   git diff --stat
   git status --short
   ```

### Slice state

| Slice | Title | Status | Completed |
|---|---|---|---|
| 1 | Integrate dependencies and establish navigation contracts | Partial | - |
| 2 | Build tiled static navigation and add the 3D Nav debug tab | Not Started | - |
| 3 | Add basic NPC path following and locomotion | Not Started | - |
| 4 | Expose scripted NPC movement early | Not Started | - |
| 5 | Add door-aware traversal and door arbitration | Not Started | - |
| 6 | Add dynamic obstacle updates with DetourTileCache | Not Started | - |
| 7 | Add local avoidance with DetourCrowd | Not Started | - |
| 8 | Harden lifecycle, caching, diagnostics, and acceptance coverage | Not Started | - |

## Goal And Acceptance Criteria

Build a reusable navigation service which lets both scripts and future AI move
NPCs through sector maps while respecting the same walls, portal restrictions,
height changes, and collision-enabled props that block the player. Closed,
unlocked doors remain traversable in route planning, but an NPC must stop,
open the actual door, wait for physical clearance, and only then cross it.

Completion requires all of the following:

- Recast builds walkable navigation from dedicated CPU build input derived from
  authored/compiled sector semantics; it does not use GPU render meshes as
  gameplay truth.
- Detour produces bounded queries and corridors without per-frame file I/O,
  asset loading, or unbounded allocation.
- Walls, `Blocks Player` boundaries, insufficient portal clearance, invalid
  steps, and collision-enabled static props affect the generated navigation.
- Dynamic collision-enabled props can update affected navigation tiles without
  rebuilding the whole map.
- NPCs follow paths using existing sector, door, and prop collision as the
  final authority. Pathfinding never bypasses collision.
- NPCs loop semantic Idle, Walk, and Run actions from their definitions; code
  does not hardcode GLB clip names.
- Lua can issue blocking and asynchronous NPC move requests before the later
  crowd-avoidance slice is needed.
- Door links encode traversal intent. NPCs stage, request/hold an unlocked
  closed door open, wait for collider clearance, cross, and release the hold.
- A new **Nav** tab in the existing 3D preview debug panel exposes navigation
  visualization controls, rebuild actions, state, build/query diagnostics,
  and selected-NPC path information.
- AI and script callers use one movement-request backend rather than invoking
  Detour or animation state directly.
- Editor/runtime mutations mark derived navigation stale or update dynamic
  obstacles through explicit paths. Expensive rebuilds and debug extraction do
  not occur in steady 2D/3D drawing.
- Existing player collision, sector lookup, physics, topology render caching,
  lightmap behavior, and old serialized data do not regress.

## Fixed Scope And Decisions

### Chosen approach

- Use **Recast** to generate walkable meshes and **Detour** for polygon queries,
  paths, straight-path corners, corridors, and off-mesh connections.
- Use a tiled Detour navmesh for large-level partitioning. Recast layer output
  plus **DetourTileCache** supplies compressed tile storage and bounded
  per-tile rebuilding around dynamic obstacles; TileCache is not the component
  that performs path queries.
- Use **DetourCrowd** later for agent motion requests, corridor maintenance,
  neighbor queries, separation, and local avoidance. Crowd does not partition
  or dynamically rebuild the navmesh.
- Do not implement a custom navmesh, waypoint graph, or grid as the primary
  solution. The sector format looks simple at first, but agent erosion,
  concavity, holes, ledges, portal clearance, obstacles, and dynamic updates
  are already the problems Recast/Detour is designed to solve.
- Keep sector collision authoritative. Navigation proposes horizontal motion;
  existing collision constrains the actual displacement and the locomotion
  system reports drift, stalls, and failed traversal back to navigation.
- Begin with one humanoid navigation profile. Multiple radii/heights and
  multiple navmeshes are future work.
- Build navigation during explicit level/preview build phases. Never rebuild a
  navmesh in normal frame update or draw.
- Use fixed-capacity query/corridor/path buffers in runtime agents. Capacity
  overflow must fail clearly or return a diagnosed partial result; it must not
  silently allocate every frame.

### Initial humanoid profile

The first profile follows current player-collision scale rather than model
bounds or visual NPC scale:

- radius: `0.25` world units
- standing height: `1.6` world units
- maximum climb: `0.25` world units
- maximum slope, Recast cell size, cell height, region sizes, edge limits, and
  tile size: explicit navigation settings with documented defaults selected in
  Slice 2

Visual model scale does not automatically change the navigation agent. If
different physical sizes become a game requirement, introduce named agent
profiles rather than deriving collision from arbitrary GLB bounds.

### Included

- Static sector navigation for floors, walls, portals, steps, clearance, and
  collision-enabled static model props.
- Path projection, full/partial/no-path results, corridors, corners, replans,
  and actionable error strings/status codes.
- NPC movement requests shared by scripts and future AI.
- Basic collision-constrained locomotion with Idle/Walk/Run semantic actions.
- Closed unlocked door traversal via typed off-mesh links and runtime door
  coordination.
- Dynamic collision-enabled prop obstacles using DetourTileCache.
- DetourCrowd-based local steering/avoidance after deterministic scripted
  navigation works.
- Editor-only navigation visualization and diagnostics in a dedicated tab of
  the existing 3D preview debug panel.

### Explicitly deferred

- Door locks, keys, access groups, factions, permissions, or lock-picking. Add
  an explicit traversal-filter seam now, but do not add lock authoring fields
  in this plan.
- Flying, swimming, jumping, ladders, crouching, arbitrary drop-down links, or
  non-humanoid navigation profiles.
- Runtime-generated destructible geometry or arbitrary moving-platform
  navigation.
- Saving an NPC's live corridor, velocity, door wait, or move-operation state
  in save games.
- Network replication and deterministic multiplayer simulation.
- Navigation authoring paint tools, manual area painting, cost volumes, or
  manual off-mesh link placement.
- Replacing the existing player collision solver or making Detour collision
  authoritative.
- Baking NPC agents themselves into TileCache obstacles. Agents use local
  avoidance; they are not navmesh cutters.
- A broad `SectorEditor.cpp` refactor or unrelated editor/debug cleanup.

## Compacted Investigation Record

### Installed dependency state

- All installed files are an exact unmodified match for official Recast
  Navigation `v1.6.0`, commit
  `6dc1667f580357e8a2154c28b7867bea7e8ad3a7`:
  https://github.com/recastnavigation/recastnavigation/tree/v1.6.0
- `sources/recast` comes from upstream `Recast/Include` and `Recast/Source` and
  contains rasterization, filtering, regions, contours, polygon/detail mesh,
  and layer generation:
  https://github.com/recastnavigation/recastnavigation/tree/v1.6.0/Recast
- `sources/detour` comes from upstream `Detour/Include` and `Detour/Source` and
  contains tiled navmesh building/storage and path queries:
  https://github.com/recastnavigation/recastnavigation/tree/v1.6.0/Detour
- `sources/detour_tile_cache` comes from upstream `DetourTileCache/Include` and
  `DetourTileCache/Source` and contains compressed tile layers plus dynamic
  cylinder/box obstacle-driven tile rebuilds:
  https://github.com/recastnavigation/recastnavigation/tree/v1.6.0/DetourTileCache
- `sources/detour_crowd` comes from upstream `DetourCrowd/Include` and
  `DetourCrowd/Source` and contains path corridors/queues, proximity queries,
  obstacle avoidance, local boundaries, and crowd-agent simulation:
  https://github.com/recastnavigation/recastnavigation/tree/v1.6.0/DetourCrowd
- `sources/recastnavigation/UPSTREAM.md` records the mapping and update policy;
  `sources/recastnavigation/LICENSE.txt` retains the upstream zlib license.
- CMake compiles all four modules once in the explicit `recastnavigation`
  static-library target and excludes them from the main recursive source glob.
  Focused navigation tests should link this same target.
- `DebugUtils` is intentionally not installed: the engine will build its own
  cached raylib navigation visualization for the existing 3D preview debug
  panel. `RecastDemo` and upstream tests are likewise not runtime dependencies.
- Treat all four third-party directories as vendor code. Avoid local edits;
  isolate engine adaptation in project-owned navigation files.

### Sector geometry and collision semantics

- Runtime/editor map geometry is topology-v2 and linedef based. The authoring
  graph is the editor source of truth; `SectorTopologyMap` is derived runtime,
  collision, rendering, and preview input.
- `SectorCollisionWorld` already compiles sector loops, holes, portals,
  floor/ceiling heights, and `Blocks Player` semantics into the authoritative
  query layer. Navigation input should mirror those semantics without reading
  private collision caches or converting render triangles back into gameplay
  geometry.
- Generated floor rendering already triangulates concave sectors and holes.
  Its CPU triangulation helper may be factored/reused if suitable, but generated
  GPU meshes are not navigation input.
- Render geometry alone is insufficient: invisible `Blocks Player` boundaries
  may have no matching render surface, sky/open rendering has different visual
  rules from physical clearance, and coincident non-portal boundaries must not
  become accidental connections.
- Collision-enabled static model props already resolve a horizontal OBB and a
  vertical interval. Once model bounds are ready, those volumes belong in
  static navigation build input.
- Collision-enabled dynamic props require runtime obstacle handling rather than
  silently forcing whole-map rebuilds.

### NPC/runtime state

- NPC definitions provide semantic Idle, Walk, and Run action-to-animation
  assignments. Locomotion must select actions, not GLB clip strings.
- Placed NPC instance IDs are the stable script-facing identity. Runtime ECS
  entity handles remain generational runtime handles and are never serialized
  as save IDs.
- `SectorSceneRuntime` is the natural owner/composer for a per-level navigation
  service. Recast/Detour pointers and allocations remain inside that service,
  not in ECS components.
- New components must remain plain data. They may store stable navigation
  handles, bounded path/corner state, requested gait, progress, and status, but
  no raw `dtNavMesh`, `dtNavMeshQuery`, `dtCrowd`, or door pointers.
- New system functions receive navigation, collision, object, and animation
  contexts explicitly and run in a documented order.

### Door behavior

- Runtime doors already have stable placed-object IDs, portal anchors, motion,
  dynamic OBB collision, portal blocking, interaction, and auto-open behavior.
- A closed unlocked door must not be baked as an ordinary passable polygon, nor
  should an NPC simply path through its current collider. Represent traversal
  explicitly with a bidirectional off-mesh connection tied to the placed door
  ID.
- The route may include the connection while the door is closed. Runtime
  traversal pauses at a staging point, requests/holds the door open, waits for
  the real moving collider to provide clearance, then crosses and releases the
  hold.
- Existing player-distance auto-open is insufficient for NPC ownership because
  it may try to close while an NPC is waiting or crossing. Door traversal needs
  explicit acquire/release holds or equivalent arbitration.
- Door closing obstruction currently considers the player. The door slice must
  also prevent a closing leaf/slab from moving through an NPC that is waiting
  or crossing.
- Locks are not implemented. The link-filter API should ask whether a door is
  traversable for an NPC and currently return true for valid enabled doors;
  future lock state can disable the link without changing path consumers.

### Script operations

- Existing door scripts use a generalized operation system supporting blocking
  yield/resume, asynchronous operation userdata, completion, failure,
  cancellation, and pre-reserved host-side storage.
- NPC movement should follow the same binding and lifecycle conventions rather
  than add a second coroutine mechanism.
- Script movement is deliberately scheduled before door traversal, dynamic
  obstacles, and crowd avoidance so maps can exercise query and locomotion
  behavior early.

### Existing 3D debug overlay

- The preview debug overlay already has tab state in preview state, an overlay
  context, interaction bounds, and View/Render/Visibility/Lighting/PBR/Objects/
  Probes/Viewmodel/Controls tabs.
- Add `PreviewDebugOverlayTab::Navigation` and a visible **Nav** tab. Derive tab
  count, spacing, and hit regions from the tab array size instead of adding one
  more hardcoded count.
- The overlay should receive a focused navigation runtime/debug view through
  its context. Do not add callback-to-`SectorEditor` funnels for ordinary
  navigation service operations.
- Visualization consumes a derived debug draw cache built when navigation
  revisions or display modes change. It must not extract Detour polygons,
  rebuild topology, triangulate, or allocate on every preview draw.

## Target Architecture

### Ownership and data flow

```text
authoring graph edits
        |
        v
derived SectorTopologyMap + runtime object data
        |
        v
SectorNavigationBuildInput  <--- resolved static prop collision
        |
        v
Recast build -> Detour navmesh/query -> navigation debug cache
                         |
            +------------+-------------+
            |                          |
       script/AI move             3D Nav tab
          requests            visualization/diagnostics
            |
            v
      NPC path/corridor
            |
            v
 collision-constrained locomotion -> semantic NPC action
            |
            +---- door traversal holds / TileCache / Crowd
```

The authoring graph remains the source of truth for editor-owned geometry and
line flags. Navigation is disposable derived runtime/editor-preview data.

### Suggested project-owned modules

Exact names may adapt to existing conventions, but preserve these boundaries:

- `sources/game/navigation/SectorNavigationTypes.h`
  - settings, stable handles, status/result enums, area/flag constants, bounded
    query result types, and diagnostics POD
- `sources/game/navigation/SectorNavigationBuildInput.h/.cpp`
  - converts derived topology and resolved static collision objects into a
    deterministic, unit-correct CPU navigation source
- `sources/game/navigation/SectorNavigationWorld.h/.cpp`
  - owns Recast/Detour memory, build lifecycle, navmesh/query objects,
    revisions, TileCache/Crowd when introduced, and diagnostic counters
- `sources/game/navigation/SectorNavigationQuery.h/.cpp`
  - projections, filters, paths, straight corners, door-link metadata, and
    bounded error handling
- `sources/game/navigation/SectorNavigationDebug.h/.cpp`
  - builds/caches renderer-friendly nav triangles, edges, tile bounds, links,
    obstacles, paths, and agent shapes
- `sources/game/npc/NpcNavigationSystem.h/.cpp`
  - shared request/cancel/status API and path/corridor progression
- `sources/game/npc/NpcLocomotionSystem.h/.cpp`
  - desired displacement, authoritative collision integration, facing, sector
    update, progress/stall handling, and semantic action choice
- `sources/sector_editor/preview/SectorEditorPreviewNavigationOverlay.h/.cpp`
  - Nav tab controls, read-only diagnostic presentation, and raylib debug draw

Keep orchestration in existing scene/editor files small. Third-party includes
should be concentrated behind the navigation modules rather than spread across
NPC, script, and editor code.

### Runtime ownership and ECS data

- `SectorSceneRuntime` owns one `SectorNavigationWorld` per loaded map and
  builds/destroys it in explicit level lifecycle phases.
- `SectorNavigationWorld` owns all third-party objects and allocation callbacks.
  It exposes project types and handles to callers.
- NPC components remain plain data and should not own STL-heavy paths or
  third-party pointers. Prefer fixed-capacity arrays or handles into
  pre-reserved navigation-owned agent/path records.
- Reserve navigation agents, path records, query nodes/buffers, obstacle
  records, script operations, and debug-cache capacity during preview/level
  initialization. Growth is an allowed development fallback but prints a
  diagnostic warning.
- Register/reserve expected NPC components before ECS registration is locked.
- Do not add/remove components or create/destroy entities inside `World::ForEach`.
  Gather results and apply any structural work in explicit phases.

### Explicit runtime update order

The detailed implementation can refine names, but retain this dependency order:

1. apply queued navigation build/tile updates during an explicit safe phase;
2. collect script/AI movement requests;
3. update Detour paths/corridors and door traversal state;
4. update Crowd desired velocities when Crowd is enabled;
5. derive locomotion displacement and requested facing;
6. move through authoritative sector/door/prop collision;
7. write actual position, sector membership, and lighting/probe inputs;
8. feed actual progress/stalls back to navigation and schedule bounded replans;
9. select Idle/Walk/Run semantic action and advance existing model animation;
10. resolve operation completion/failure/cancellation and resume scripts.

No structural ECS modification belongs inside these iteration systems.

## Navigation Build Contract

### Coordinate and unit rules

- Navigation operates in runtime world units. Convert authored sector height
  units exactly once at the build-input boundary.
- Preserve engine world axes and winding through one documented conversion
  helper. Add a focused round-trip/unit test; do not scatter axis swaps through
  Recast calls and debug drawing.
- Build bounds include traversable sector floor extents plus configured padding.
  Empty maps and maps with no walkable floor produce a clear `Empty` state.

### Static build input

The build input must include:

- triangulated sector floor regions, including concave outlines and holes;
- the floor height of each source sector;
- ceiling clearance at every floor region and portal;
- passable two-sided portal connections only when step, head clearance,
  authored flags, and agent erosion permit them;
- one-sided walls, `Blocks Player` boundaries, non-passable portals, and
  coincident non-portal seams as permanent barriers;
- resolved collision-enabled static prop OBBs with vertical intervals when
  those intervals intersect the humanoid traversal band;
- a deliberate closed-door cut/barrier plus typed off-mesh connection rather
  than ordinary floor connectivity through the doorway.

Do not infer navigation connectivity merely because independently rasterized
triangles touch. Source adjacency/portal semantics must prevent accidental
cross-sector links.

### Recast/Detour policy

- Slice 2 selects and documents deterministic Recast/TileCache settings and
  maps them to the humanoid profile.
- Use per-tile Recast rasterization, heightfield layers, TileCache compression,
  and a tiled Detour navmesh from the first usable implementation. Do not build
  a whole-level voxel heightfield and retrofit tiling later.
- Compute tile counts and Detour `maxTiles`/`maxPolys` capacity before building.
  Diagnose a level/settings combination that exceeds the selected polygon-ref
  layout. Decide whether `DT_POLYREF64` is warranted before any persistent nav
  cache format is committed; 32-bit and 64-bit tile data are incompatible.
- Assign project-owned polygon flags/areas for normal ground and door
  connections. Keep area costs/filter policy centralized.
- Keep the original build input or sufficient source diagnostics only when
  needed for explicit rebuild/debug; release unnecessary intermediates after
  build.
- All Recast build-context errors/warnings become bounded project diagnostics
  viewable in the Nav tab and logs.

### Query result contract

Path requests distinguish at least:

- `Success`
- `Partial`
- `StartNotOnNavmesh`
- `DestinationNotOnNavmesh`
- `NoPath`
- `CapacityExceeded`
- `NavigationUnavailable`
- `InvalidAgent`
- `Cancelled`
- `Stalled`
- `TargetRemoved`
- `InternalError`

Preserve Detour status details for diagnostics but do not expose raw `dtStatus`
through game/ECS/script APIs. A partial route is never silently reported as
arrival. Script policy for partial routes is defined below.

## Navigation Source Revision And Cache Contract

- Navigation has its own source revision/hash and stale state. It is not part
  of the lightmap source hash, and navigation changes do not invalidate baked
  lightmaps.
- The navigation source hash includes every input that can change walkability:
  topology geometry, floor/ceiling heights, sky/open physical clearance rules,
  collision/portal/`Blocks Player` flags, navigation settings/profile,
  collision-enabled static prop transforms and resolved collision geometry or
  stable asset fingerprint, and door-link/cut geometry.
- It excludes NPC placements and definitions, runtime move state, dynamic prop
  transforms, visual materials/textures, decals, lights, probes, fog, sky
  visuals, debug toggles, and camera/preview settings.
- Dynamic prop obstacles update TileCache state/revisions and affected debug
  cache data; they do not rewrite the static navigation source hash.
- Editor authoring mutations that affect geometry/clearance/flags follow the
  existing candidate-derive-validate-commit path, then mark navigation stale.
  Runtime-object edits use their existing intentional ownership path and mark
  navigation stale only when static navigation inputs change.
- Existing topology mutations must continue to use
  `MarkTopologyDocumentEdited()`/normal cache invalidation. Navigation staleness
  is additional derived-state invalidation, never a replacement for the 2D
  topology render-cache contract.
- The navigation debug cache has its own source revision and display-mode key.
  Steady drawing reads it without topology validation, loop extraction,
  triangulation, Recast/Detour extraction, asset I/O, or GPU resource creation.

## Door Traversal Contract

### Build/query representation

- Treat the physical doorway as blocked normal navigation and add a
  bidirectional typed off-mesh link between safe staging points on its two
  sides.
- Store the stable placed door object ID in project metadata and, where
  appropriate, `offMeshConUserID`. Never store a raw runtime door pointer.
- Link endpoints must be far enough from the swept/translated collider for an
  agent to stage safely, projected onto valid polygons, and diagnosed if a
  usable link cannot be constructed.
- The path filter calls one project-owned traversability seam. V1 permits valid
  enabled doors because locks do not yet exist; future lock state can disable
  the connection without rebuilding NPC code.

### Runtime state machine

An NPC encountering a door connection proceeds through explicit states:

1. follow corridor to the near-side staging point;
2. acquire a navigation hold on the door;
3. request the door's open target through the existing door system;
4. wait outside the moving collider/sweep while continuing to face/idle as
   appropriate;
5. verify physical clearance for this agent using current door collider state;
6. traverse toward the far-side landing through normal authoritative collision;
7. confirm the corridor has advanced beyond the connection;
8. release the door hold and resume ordinary path following.

Cancellation, NPC deletion, map unload, path failure, and replan must release
all held doors. Multiple NPC holds are reference-counted or represented by
stable holder IDs so one NPC cannot close a door on another. Player auto-open
and NPC holds combine into one effective open request. Closing obstruction must
include relevant NPC cylinders, not just the player.

## Script And AI Movement Contract

### Shared backend

Provide project-owned functions resembling:

```cpp
NpcMoveRequestResult RequestNpcMove(...);
bool CancelNpcMove(...);
NpcMoveStatus GetNpcMoveStatus(...);
```

Both Lua and future AI call this backend. Callers provide intent—instance ID or
runtime handle, destination, and gait—not Detour polygons, corner arrays,
animation clips, or direct velocity.

Movement authority rules:

- one active script-owned move is allowed per NPC;
- a second scripted request fails clearly rather than silently replacing the
  coroutine waiting on the first;
- a script request may take authority from AI; AI cannot retarget that NPC
  until the script operation completes or is cancelled;
- after scripted authority ends, future AI may make a fresh decision; stale AI
  paths are not automatically resumed;
- map unload, NPC deletion, navigation rebuild, and script cancellation stop
  locomotion, release door holds, select Idle, and resolve the operation.

### Initial Lua API

Follow existing door-operation conventions with bindings shaped as:

```lua
moveNpc(instanceId, x, z [, gait])
startMoveNpc(instanceId, x, z [, gait])
```

- `instanceId` is the placed NPC's unique map instance ID.
- `x`/`z` are runtime world coordinates. The service projects the destination
  onto an appropriate walkable floor; scripts do not supply authored height
  units.
- `gait` is `"walk"` by default and may be `"run"`.
- `moveNpc` yields until arrival or terminal failure and follows existing
  binding conventions for returning success versus `(false, reason)`.
- `startMoveNpc` returns the existing operation userdata on success or the
  binding's normal immediate error result. Its completion, failure,
  cancellation, and status methods behave like other script operations.
- V1 treats partial paths as failure unless the NPC reaches the projected
  requested destination within the documented arrival tolerance.
- Validate empty/missing instance IDs, unsupported gait, unavailable
  navigation, non-projectable destinations, no path, removed NPC, stalled
  movement, rebuild interruption, and operation capacity explicitly.

Pre-reserve operation records. Do not allocate coroutine/operation tracking
containers every frame.

## 3D Preview Nav Debug Tab Contract

Add a **Nav** tab to the existing preview debug panel rather than a separate
window. Debug visibility/settings are session-only editor state, matching the
other preview debug controls.

### Controls

The tab should expose compact toggles for available data:

- navigation surface fill;
- polygon/boundary edges;
- tile boundaries;
- door/off-mesh links and their enabled/disabled state;
- static and dynamic obstacle bounds;
- active NPC corridors and straight-path corners;
- agent cylinders, desired direction, and actual velocity;
- selected-NPC-only versus all-agent path detail.

Include an explicit **Rebuild Navigation** action and, where useful, a draw-mode
or profile selector. Rebuild is serviced by the navigation/preview lifecycle,
not performed synchronously from the drawing function. Disable or label
controls whose data does not exist before the relevant slice.

### Diagnostics

Display bounded, stable text for:

- state: unavailable, empty, stale, queued, building, ready, or failed;
- last warning/error and failed build stage;
- navigation source revision/hash and debug-cache revision;
- active agent profile and Recast settings;
- world/build bounds, tiles/layers, polygon/vertex counts, off-mesh links, and
  build time/memory estimates where available;
- static obstacles plus active/pending TileCache obstacle/tile updates;
- active navigation agents and script-owned moves;
- cumulative/recent path successes, partials, failures, replans, stalls, and
  capacity warnings;
- selected NPC instance ID, state, requested destination/gait, nearest polygon,
  remaining corners, corridor length, last query result, progress/stall timer,
  current door hold, and actual versus desired speed.

### Rendering and UI rules

- Convert navigation geometry into renderer-friendly POD vertices/segments in
  the debug cache; raylib drawing uses that cache.
- Use readable colors and depth behavior that distinguish walkable fill,
  boundaries, disabled links, obstacles, and selected paths without affecting
  world rendering.
- Debug draw is visual only: no collision, picking, lightmap, shadow, bloom, or
  gameplay role.
- Add the Navigation enum/tab without leaving hardcoded nine-tab layout math.
  Test tab rectangles and overlay interaction bounds at supported window sizes.
- Pass a focused navigation debug/service view in the overlay context. The
  preview panel must not own the navmesh and must not call back into
  `SectorEditor` for each operation.

## Detailed Slices

### Slice 1 — Integrate Dependencies And Establish Navigation Contracts

Purpose: turn the copied source folders into a deliberate dependency boundary
and land the project-owned types/lifecycle skeleton needed by later slices.

Tasks:

1. **Completed 2026-08-14:** identify the official upstream URL and verify the
   copied core byte-for-byte as `v1.6.0` commit
   `6dc1667f580357e8a2154c28b7867bea7e8ad3a7`; retain license/provenance.
2. **Completed 2026-08-14:** install matching DetourTileCache and DetourCrowd
   headers/sources from the same release without modifying vendor code.
3. **Completed 2026-08-14:** compile Recast, Detour, TileCache, and Crowd once in
   the explicit `recastnavigation` target with exported module include paths;
   exclude vendor files from the main recursive project source glob.
4. **Completed 2026-08-14:** confirm the complete dependency target compiles
   cleanly with the project's C++17 build. Keep upstream warnings isolated
   where sensible rather than patching vendor code.
5. Add focused project-owned navigation types/settings/status/diagnostic files
   and a `SectorNavigationWorld` lifecycle skeleton with explicit
   uninitialized/empty/ready/stale/failed states.
6. Add stable NPC navigation/path record handles and capacity/reserve settings.
   Do not place third-party pointers in components or public script types.
7. Define project area/flag values, query-filter seam, coordinate conversion,
   navigation revision, and failure/status contracts.
8. Compose the service in scene/preview lifecycle without building real
   geometry yet. Shutdown must release it before dependent runtime state.

Tests:

- vendor target compiles and links into a minimal project-owned test;
- coordinate conversion round-trip and authored-height-to-world conversion;
- status conversion does not expose raw `dtStatus`;
- reserve/capacity warning behavior;
- initialize, empty, fail, rebuild-reset, and shutdown lifecycle;
- dependency sources are not compiled twice.

Completion criteria:

- provenance/license is present and exact dependency scope is known;
- the build uses explicit vendor target(s);
- the navigation service skeleton is owned and torn down correctly;
- no gameplay, collision, or editor behavior has changed yet.

### Slice 2 — Build Tiled Static Navigation And Add The 3D Nav Debug Tab

Purpose: produce and inspect a queryable, TileCache-backed tiled navmesh for
static sector maps before NPC movement is added. Tile partitioning and bounded
build memory are first-version requirements because maps may be large.

Tasks:

1. Implement deterministic `SectorNavigationBuildInput` from derived topology,
   sector heights/clearance, portal/player-blocking semantics, and resolved
   collision-enabled static prop OBBs.
2. Reuse or extract a focused CPU floor triangulation helper if appropriate;
   do not route through generated GPU/render meshes.
3. Select/document Recast and TileCache defaults for the initial humanoid
   profile. Rasterize with tile borders, build heightfield layers, compress
   them through a project-owned TileCache compressor, and build the tiled
   Detour navmesh in an explicit preview/level build phase.
4. Prevent accidental connections across one-sided, blocked, vertically
   invalid, or coincident non-portal boundaries.
5. Add bounded nearest-point, path, and straight-corner query helpers with full,
   partial, no-path, and capacity diagnostics.
6. Calculate tile-grid bounds and Detour tile/polygon-reference capacity before
   allocation. Report world/tile size, tile count, memory, and 32/64-bit limit
   headroom in diagnostics; fail clearly rather than truncating a large map.
7. Build tiles independently with bounded temporary working memory. The
   explicit load/rebuild phase may process a configurable tile budget per
   update so the Nav tab can show progress without doing build work in draw.
8. Establish static navigation source revision/hash inputs and stale/rebuild
   lifecycle. Full persistence to disk may remain for Slice 8.
9. Build the derived navigation debug cache on build/revision/display changes.
10. Add `PreviewDebugOverlayTab::Navigation`, a **Nav** tab, array-size-derived
   tab layout, controls, rebuild request, interaction bounds, and all currently
   available build/query diagnostics.
11. Draw cached walkable surface, polygon edges, tile bounds, static obstacle
   bounds, and any available door-link placeholders with raylib debug drawing.
12. Expose a read-only query probe or selected-position diagnostic if useful,
    but do not add a separate navigation editor tool.

Generated-fixture tests:

- rectangular, concave, and holed sector floors;
- connected passable portal;
- one-sided and `Blocks Player` boundaries;
- within-climb and over-climb height differences;
- adequate and inadequate ceiling/portal clearance;
- passage wider/narrower than agent erosion permits;
- coincident non-portal edges remain disconnected;
- rotated collision-enabled static prop OBB blocks the proper height band;
- empty/malformed input and deterministic failure reporting;
- full, partial, no-path, invalid-start, invalid-target, and capacity query
  results;
- source hash changes for every static walkability input and ignores visual
  settings;
- multi-tile paths cross tile boundaries and edge tiles retain the required
  rasterization border;
- large synthetic bounds produce correct capacity/headroom diagnostics without
  allocating a whole-level heightfield;
- TileCache layer compression/decompression and failed/corrupt layer handling;
- ten debug tabs fit without overlap; interaction bounds include Nav content;
- debug cache rebuilds on navigation/display revision, not on steady draw.

Completion criteria:

- a static map produces a TileCache-backed tiled Detour navmesh and bounded
  path results;
- build working memory is tile-bounded and oversized configurations fail with
  actionable capacity diagnostics;
- the 3D preview Nav tab can rebuild, visualize, and diagnose it;
- normal frame/draw paths perform no topology derivation, triangulation,
  navmesh rebuild, asset I/O, or Detour debug extraction;
- player collision and topology rendering are unchanged.

### Slice 3 — Add Basic NPC Path Following And Locomotion

Purpose: make one or more placed NPCs follow static paths around walls and
static collision props, without scripting, doors, dynamic obstacles, or Crowd
being prerequisites.

Tasks:

1. Add plain-data NPC navigation/locomotion components or pre-reserved external
   records, and reserve/register them during level spawning.
2. Add the shared request/cancel/status backend using placed NPC instance IDs
   and runtime entity handles internally.
3. Project start/destination, build a bounded corridor/corner list, and advance
   toward corners with a documented arrival tolerance.
4. Move through authoritative sector, door, and prop collision. Update actual
   transform, grounded floor position, current sector, and dependent lighting
   state through existing runtime paths.
5. Rotate toward actual movement while respecting authored initial yaw until a
   movement request begins.
6. Use definition WalkSpeed/RunSpeed and requested gait consistently. Select
   semantic Idle, Walk, and Run actions and their normalized animation-speed
   multipliers; do not look up hardcoded animation clip names in locomotion.
7. Feed actual displacement back into corridor progress. Detect stalls,
   off-corridor drift, and obstruction; rate-limit bounded replans and fail with
   a clear status when recovery is exhausted.
8. Extend Nav-tab debug data with paths, corners, agent cylinders, directions,
   speed, selected NPC state, and query/replan/stall counters.
9. Add a small non-script test/debug request seam so automated tests can drive
   movement before Slice 4.

Tests:

- NPC follows a multi-corner path around a wall and rotated static prop;
- movement cannot cross authoritative collision even with an invalid/forced
  desired vector;
- floor grounding, sector membership, facing, and arrival tolerance;
- Idle/Walk/Run semantic selection and animation multiplier application;
- missing action animation fails gracefully while motion remains diagnosed;
- low/high frame-time steps do not skip corners or tunnel through collision;
- stall detection, bounded replans, no-path, entity deletion, and map teardown;
- two independent agents do not share mutable path state.

Completion criteria:

- a programmatic request moves an NPC around static blockers and arrives;
- collision remains authoritative and no visual camera effects enter physics or
  sector lookup;
- the Nav tab explains the selected NPC's path and failures.

### Slice 4 — Expose Scripted NPC Movement Early

Purpose: let map scripts exercise real navigation and locomotion before door,
dynamic-obstacle, and crowd work is complete.

Tasks:

1. Add `moveNpc(instanceId, x, z [, gait])` using the existing blocking script
   operation/yield mechanism.
2. Add `startMoveNpc(instanceId, x, z [, gait])` using existing asynchronous
   operation userdata/status/cancel behavior.
3. Enforce the shared movement authority and duplicate-script-request policy.
   Script requests take authority from future AI; script-versus-script overlap
   fails clearly.
4. Complete/fail/cancel operations from navigation terminal state after
   locomotion feedback, not merely when Detour returns a route.
5. Handle invalid instance IDs, missing NPC definitions/entities, invalid
   destinations/gaits, unavailable/stale navigation, partial/no path, capacity,
   stalls, map reset, and NPC deletion.
6. Reserve operation storage and warn on fallback growth consistently with the
   existing script operation host.
7. Show active script-owned movement and operation outcome in Nav diagnostics.
8. Document the Lua API and current limitation that door links, dynamic
   TileCache obstacles, and local avoidance arrive in later slices.

Tests:

- blocking success and return values;
- asynchronous success, poll/status, cancellation, and terminal failure;
- walk/run gait validation and speed/action behavior;
- duplicate script move rejection and script authority policy;
- invalid/missing instance, target off mesh, no/partial path, stall, deletion,
  navigation rebuild, and map unload;
- capacity warning path without crash;
- operation resolves only after physical arrival.

User-owned manual acceptance after this slice:

- place an NPC, open the 3D preview Nav tab, and enable path/corner display;
- invoke a script move whose destination requires routing around a wall/static
  prop and confirm the operation resolves after arrival;
- request an unreachable destination and confirm the script and Nav tab expose
  a useful failure rather than hanging.

Completion criteria:

- scripts can reliably start, await, inspect, and cancel NPC movement;
- failures are actionable and lifecycle cleanup is complete;
- this slice does not depend on Crowd.

### Slice 5 — Add Door-Aware Traversal And Door Arbitration

Purpose: let paths intentionally cross unlocked doors without walking through a
closed collider.

Tasks:

1. Generate permanent doorway separation plus bidirectional typed off-mesh
   connections with stable door IDs and safe projected staging/landing points.
2. Add the door traversability filter seam; V1 treats valid doors as unlocked
   without adding lock schema.
3. Detect approaching door connections in the corridor and implement the
   staging/open/wait/cross/release state machine.
4. Add stable NPC door holds that combine with player auto-open/interaction
   into the effective door target. Multiple NPCs may hold one door safely.
5. Check actual sliding/swinging door collider clearance for the agent before
   crossing; do not use only `openFraction` or visual pose.
6. Include NPC cylinders in closing-door obstruction handling so a leaf/slab
   does not close through a waiting/crossing NPC.
7. Release holds on every cancellation, replan, path replacement, deletion,
   script failure, and map teardown path.
8. Replan/fail when a link becomes unavailable. Prefer an alternate path when
   the filter disables a door link.
9. Add Nav-tab link state, direction, door ID, current holder count, selected
   NPC traversal state, and door-wait diagnostics.

Tests:

- route crosses a closed valid door link rather than normal doorway polygons;
- NPC stages, opens, waits for physical clearance, crosses, and releases;
- sliding and swing doors use their actual collider state;
- two NPC holds do not prematurely close a door;
- player auto-open and NPC holds combine correctly;
- cancellation/deletion/map unload release holds;
- closing door reverses/halts for relevant NPC obstruction;
- disabled link yields alternate route or diagnosed failure;
- invalid/missing door link metadata never dereferences stale pointers.

User-owned manual acceptance after this slice:

- script an NPC to the opposite side of a closed door and observe it stop,
  open/wait, cross, and continue;
- test two NPCs approaching the same door and confirm it remains open until
  traversal ownership is released.

Completion criteria:

- closed valid doors are plan-able but never walked through physically;
- door lifecycle and arbitration are safe for player plus multiple NPC users;
- future locks can disable links through the existing filter seam.

### Slice 6 — Add Dynamic Obstacle Updates With DetourTileCache

Purpose: support collision-enabled dynamic props without whole-map navmesh
rebuilds and make affected NPCs replan.

Prerequisite:

- Already satisfied: `sources/detour_tile_cache` contains the unmodified
  `v1.6.0` module and is part of the explicit `recastnavigation` target. Verify
  that provenance remains pinned before beginning this slice.

Tasks:

1. Add the dynamic-obstacle lifecycle to the `dtTileCache` and compressed
   layers already established in Slice 2; do not introduce a second navmesh or
   incompatible rebuild format.
2. Represent supported collision-enabled dynamic prop OBBs as conservative
   TileCache obstacles with stable placed/runtime-object IDs.
3. Queue add/remove/transform changes and process a bounded amount of tile work
   in an explicit runtime phase. Expose backlog/capacity diagnostics.
4. Coalesce repeated transform updates. Avoid churning TileCache for tiny
   changes below documented position/yaw thresholds.
5. Define supported motion: slowly moved/settled props update obstacles; fast
   continuously moving props remain collision-only local blockers and trigger
   stall/replan handling rather than rebuilding tiles every frame.
6. Advance navigation/tile revisions and rebuild only affected debug cache
   geometry.
7. Invalidate or validate agent corridors touching changed tiles and schedule
   rate-limited replans.
8. Show obstacle IDs/bounds/states, updated tiles, backlog, failures, and timing
   in the Nav tab.

Tests:

- add, remove, translate, rotate, disable collision, and delete dynamic OBB;
- obstacle vertical band and conservative footprint behavior;
- affected tiles update while unaffected tiles/revisions remain stable;
- queue coalescing, per-frame budget, overflow warning, and teardown;
- path reroutes after a prop blocks a corridor and recovers when removed;
- fast-mover policy avoids per-frame tile churn while collision still blocks
  actual NPC motion;
- scripts receive arrival or diagnosed failure rather than hanging through
  topology changes.

User-owned manual acceptance after this slice:

- move a collision-enabled dynamic prop into/out of an NPC's planned route and
  inspect affected tiles/obstacles/replan state in the Nav tab.

Completion criteria:

- supported dynamic props update navigation incrementally and safely;
- navigation and authoritative collision stay consistent enough for agents to
  recover or fail clearly;
- there is no steady full-map rebuild.

### Slice 7 — Add Local Avoidance With DetourCrowd

Purpose: make multiple NPCs steer around each other and queue through constrained
spaces while keeping the tested path/script backend intact.

Prerequisite:

- Already satisfied: `sources/detour_crowd` contains the unmodified `v1.6.0`
  module and is part of the explicit `recastnavigation` target. Verify that it
  remains on the same revision as core Detour before beginning this slice.

Tasks:

1. Own/configure `dtCrowd` in `SectorNavigationWorld` with pre-reserved maximum
   agents and explicit avoidance parameters.
2. Map NPC navigation records to stable Crowd agent handles without storing raw
   Crowd pointers in ECS components.
3. Feed shared backend targets/corridors to Crowd and use desired velocity as
   locomotion input; authoritative sector/door/prop collision still produces
   actual displacement.
4. Reconcile actual position/velocity back into agent state and retain bounded
   stall/replan recovery.
5. Keep NPCs out of TileCache obstacle registration. Agent-agent separation is
   a local-avoidance concern.
6. Preserve scripted movement authority, completion, and cancellation semantics.
7. Tune/diagnose narrow doorway queuing without allowing agents to bypass the
   door hold/traversal state machine.
8. Add Crowd neighbor, desired velocity, avoidance-quality, active-agent,
   capacity, and selected-agent detail to the Nav tab.

Tests:

- head-on agents separate rather than overlap indefinitely;
- crossing flows and stationary agent avoidance;
- several agents queue through a door/narrow corridor without bypassing
  collision or losing holds;
- full capacity and agent removal/reuse are diagnosed and safe;
- script completion remains based on physical arrival;
- actual collision correction does not create unbounded Crowd drift;
- deterministic fixed-fixture tests use tolerances appropriate for steering.

User-owned manual acceptance after this slice:

- command several NPCs through intersecting routes and a single doorway while
  viewing agents, desired velocities, paths, and door holds in the Nav tab.

Completion criteria:

- multiple NPCs locally avoid each other while preserving path, door, script,
  and collision contracts;
- Crowd is an internal steering service, not a new gameplay API.

### Slice 8 — Harden Lifecycle, Caching, Diagnostics, And Acceptance Coverage

Purpose: close cross-slice failure paths, decide nav-data persistence policy,
and leave a maintainable/tested subsystem.

Tasks:

1. Audit load/reload/unload, preview enter/exit, map replacement, navigation
   rebuild, NPC spawn/delete, script shutdown, and renderer/context teardown.
2. Audit every reserve/capacity and normal-frame allocation path. Preserve
   warning fallbacks where project policy permits growth, and eliminate
   accidental per-frame containers/extraction.
3. Finalize navigation source hashing and choose one of:
   - rebuild on explicit level/preview load with clear progress/diagnostics; or
   - persist a versioned derived nav cache keyed by source hash and settings.
   Persisted navigation remains derived output, never authoring truth.
4. If persisted, validate format/version/hash atomically and rebuild safely on
   mismatch/corruption. Do not store third-party pointers or platform-unstable
   layouts directly.
5. Audit Nav-tab usability, stale/failed/empty behavior, counter reset rules,
   selected-NPC cleanup, and large diagnostic truncation.
6. Verify topology/render debug caches are invalidated only through their
   intended paths and navigation debug state cannot trigger lightmap changes.
7. Add architecture/API documentation, Lua examples, settings explanations,
   known limits, and troubleshooting based on Nav diagnostics.
8. Run focused sanitizers or additional deterministic stress tests if available
   and record any remaining debt in this document.

Tests:

- repeated load/rebuild/unload and failed/cancelled build lifecycle;
- stale navmesh during script request and rebuild interruption;
- corrupt/mismatched cache fallback if persistence is implemented;
- maximum agents, paths, obstacles, operation records, debug data, and warning
  truncation;
- deletion while moving, waiting at a door, updating a tile, and under Crowd;
- missing/failed model collision bounds during initial build and later asset
  readiness transition;
- navigation hash inclusions/exclusions and lightmap-hash independence;
- steady frame/draw allocation and expensive-work instrumentation;
- the complete generated-fixture regression suite from all earlier slices.

Final user-owned manual acceptance:

- Nav tab reports ready state and can toggle surfaces, edges, tiles, links,
  obstacles, paths, corners, and agents;
- scripted walk/run succeeds around walls and static/dynamic props;
- unreachable targets fail clearly;
- NPCs open and traverse closed doors without phasing through them;
- multiple NPCs avoid/queue while scripted operations still resolve correctly;
- navigation rebuild/stale/failure states remain understandable.

Completion criteria:

- all automated checks pass and manual acceptance is ready for the user;
- navigation ownership, source hash, caches, lifecycle, and allocation behavior
  are documented and enforced;
- remaining limitations are explicit rather than hidden in implementation.

## Cross-Slice Testing Rules

- Tests must generate topology/maps/NPC definitions or use immutable dedicated
  fixtures outside user-edited `assets/levels` and `assets/sector_demo` data.
- Use exact/simple assertions for build topology and tolerant assertions for
  floating-point movement/steering.
- Test the public project-owned navigation API; use raw Recast/Detour calls only
  in dependency integration tests.
- Every failure path must return a project status and useful bounded diagnostic.
- Debug visualization tests should validate cached draw data and UI geometry;
  they do not replace user-owned visual verification.
- No test should require an OpenGL context unless it is explicitly an existing
  renderer test with the proper harness.
- Every slice must state whether it changed collision/sector lookup/physics,
  topology cache invalidation, or lightmap source hashing.

## Deferred Backlog

These items are intentionally outside the eight slices and should become new
plans when a game requirement exists:

- multiple named navigation profiles/navmeshes;
- crouch, jump, drop, climb, ladder, swimming, or flying traversal;
- authored navigation areas/costs and manual links;
- lock/key/access-policy authoring and runtime behavior;
- moving platforms and destructible navigation geometry;
- save-game persistence of active movement and navigation state;
- network synchronization;
- general runtime AI behaviors beyond consuming the shared movement API.

## Execution Log

Append one entry per slice attempt using this shape:

```text
### YYYY-MM-DD — Slice N — Completed/Partial/Blocked

- Scope completed:
- Files/modules added or changed:
- Decisions or contract updates:
- Tests/checks:
- Collision/sector lookup/physics impact:
- Topology cache invalidation impact:
- Lightmap/source-hash impact:
- Manual verification performed by user:
- Remaining debt/blocker:
```

### 2026-08-14 — Slice 1 — Partial

- Scope completed: dependency identification, exact-version verification,
  TileCache/Crowd installation, license/provenance retention, and explicit
  combined vendor build target. Project-owned navigation types, service
  lifecycle, handles, settings, and status contracts remain for the normal
  Slice 1 implementation run.
- Files/modules added or changed: `sources/detour_tile_cache`,
  `sources/detour_crowd`, `sources/recastnavigation`, top-level CMake, vendor
  `.gitattributes` policy, and this plan.
- Decisions or contract updates: all modules are pinned to official `v1.6.0`
  commit `6dc1667f580357e8a2154c28b7867bea7e8ad3a7`; tiled Recast/TileCache build
  foundations move into Slice 2 because large maps are a first-version
  requirement; dynamic obstacle lifecycle remains Slice 6 and Crowd remains
  Slice 7.
- Tests/checks: all newly installed source/license files compare byte-for-byte
  with the official release archive; `cmake --build cmake-build-debug -j2`
  passed; all 25 CTest tests passed; `git diff --check` passed; diff/stat/status
  reviewed. The build retains the pre-existing Lua `tmpnam` linker warning.
- Collision/sector lookup/physics impact: none; dependency code is compiled but
  no runtime navigation integration exists yet.
- Topology cache invalidation impact: none.
- Lightmap/source-hash impact: none.
- Manual verification performed by user: none requested; no GUI test was run.
- Remaining debt/blocker: finish Slice 1 project-owned contracts and lifecycle;
  select and capacity-test the 32-bit versus 64-bit Detour polygon-ref layout in
  Slice 2 before committing any persistent nav data format.
