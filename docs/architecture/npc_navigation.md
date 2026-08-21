# NPC Navigation Architecture

NPC navigation is level-owned derived runtime data. `SectorTopologyMap` and
resolved collision inputs remain the source of truth; Detour objects and debug
geometry are never authored or serialized.

## Ownership and lifecycle

`SectorSceneRuntime` owns one `SectorNavigationWorld` and one
`NpcNavigationRuntime`. A level or preview rebuild follows this order:

1. release NPC paths, Crowd agents, and door holds;
2. shut down the old navigation world;
3. rebuild renderer resources and runtime objects as required;
4. initialize navigation with map-derived settings and reserved capacities;
5. queue a navigation build and initialize NPC records;
6. wait for terminal static-model collision readiness, then build within the
   configured per-update tile budget.

Map unload and renderer/context teardown release NPC navigation before Detour
and ECS runtime objects. Script shutdown cancels active operations before scene
teardown. An explicit rebuild immediately invalidates handles and releases the
old derived mesh; it never leaves the previous mesh queryable while reporting
`Queued` or `Building`. Active NPC/script movement fails clearly after a
rebuild, even when a small rebuild completes in the same frame.

Navigation data is rebuilt on each explicit level/preview load. There is no
persisted navigation cache. This keeps platform-specific Recast/Detour layouts
out of level files and makes topology the only authoring truth.

## Runtime update order

The scene updates door traversal intent/holds, synchronizes door links,
advances the bounded navigation build, reconciles dynamic TileCache obstacles,
updates NPC paths and locomotion, then updates remaining runtime object state.
Crowd only supplies local steering. Physical positions still come from the
topology-based `SectorCollisionWorld`; visual offsets never affect navigation,
sector lookup, or physics. Navigation locomotion enables the collision world's
grounded drop constraint: downward portal changes up to the configured agent
maximum climb remain valid stair connections, while larger drops behave as
blocking ledges. This prevents avoidance steering from moving a non-falling NPC
off the navigation surface; NPC gravity and airborne traversal remain out of
scope.

Passable portals between different floor heights receive deterministic
walkable-area variants during the derived navigation build. Straight-path
queries request area crossings, so authored stair treads retain an ordered
waypoint at every physical height transition even if Recast would otherwise
simplify the run into a single sloped polygon. All ground variants have
identical query flags and cost; the areas only preserve locomotion boundaries.

Locomotion progress is cumulative improvement toward the active waypoint,
rather than any tiny forward displacement. After 0.75 seconds without
meaningful improvement, the NPC temporarily follows its raw waypoint velocity
instead of Crowd steering while remaining a Crowd neighbor and retaining
authoritative physical collision. If recovery produces no progress by 1.5
seconds, the bounded replan policy applies. Advancing to a waypoint or
successfully replanning resets recovery state.

Normal-frame query and steering data use fixed arrays or pre-reserved vectors.
Agent, path, obstacle, diagnostic, and script-operation capacity overruns remain
safe development fallbacks and emit one-shot warnings where growth is allowed.
Diagnostic history is bounded, oversized messages are truncated, and the Nav
tab reports retained/dropped/truncated counts.

## Source hash and caches

The navigation source hash is versioned independently from the lightmap hash.
It includes normalized Recast settings, topology vertices and connectivity,
player-blocking linedefs, floor/ceiling heights, `ceilingSky`, resolved static
collision OBBs/fingerprints, and door geometry that changes cuts or link
staging. It excludes preview/camera settings, textures and other presentation,
sky visuals, lighting/lightmap metadata, audio, NPCs, dynamic props, and door
animation timing.

The navigation debug cache is derived from completed Detour tiles and bounded
runtime obstacle/link state. Expensive mesh extraction occurs during build or
changed-tile processing, not steady drawing. Source, build, tile, and debug
revisions have separate meanings. Navigation rebuild/debug actions do not
invalidate the 2D topology render cache, mark the authoring document dirty, or
change the lightmap source hash.

## Settings and diagnostics

The map preview step height supplies the navigation agent maximum climb. Other
defaults are a 0.25m radius, 1.6m height, 0.125m cells, 0.05m cell height, and
64-cell tiles. `SectorNavigationCapacitySettings` bounds agents, path records,
query nodes, tiles/layers, candidate triangles, TileCache temporary memory,
dynamic obstacles, diagnostics, and per-update work budgets.

The editor 3D preview Nav tab shows state/stage, source hash and revisions,
tile progress, build/update timing, memory/capacity data, Crowd and obstacle
counters, bounded diagnostics, door links, and the selected NPC. Rebuild is
disabled while a build is already queued or running. Debug toggles draw cached
surfaces, edges, tile bounds, obstacles, door links, step connections, paths,
corners, and agents. Surface and polygon-edge overlays use a small visual-only
vertical offset to avoid depth fighting with preview geometry.

Troubleshooting guidance:

- `Waiting for static collision`: allow model assets to reach ready or failed;
  failed/missing collision bounds are omitted with a warning.
- `Empty`: the map produced no walkable triangles; inspect sector topology,
  heights, and player-blocking boundaries.
- `Failed`: read the latest bounded diagnostic and capacity/memory counters.
- `Stale`: source data changed; request an explicit rebuild.
- partial/no path: inspect disconnected portals, climb, door-link state, and
  dynamic obstacles.
- capacity warning: raise the specific level-load reserve after confirming the
  expected content count; do not rely on runtime growth as normal behavior.

## Lua contract and current limits

Lua uses `moveNpc` for blocking movement and `startMoveNpc` with `await`,
`operationStatus`, or `cancelOperation` for asynchronous movement. Targets may
be runtime X/Z coordinates or exact compiled marker IDs. Requests require
ready navigation and report invalid NPCs, off-mesh targets, partial/no paths,
rebuilds, deletion, stalls, and capacity failures without exposing Detour
types.

Only one shared humanoid navigation profile exists. Crouch/jump/drop/climb,
manual links and area costs, moving platforms, save-game persistence of active
moves, dynamic sector geometry, and general AI behaviors remain out of scope.
Middle textures do not become navigation collision, and dynamic obstacles are
bounded box/cylinder TileCache inputs rather than arbitrary animated meshes.
