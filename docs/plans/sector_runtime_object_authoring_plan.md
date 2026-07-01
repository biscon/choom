# Sector Runtime Object Authoring Plan

## How To Use This Plan

This is a living execution plan.

When an agent is asked to execute this plan, it must:

1. Read this section first.
2. Read the `plan-state-json` block.
3. Identify the selected phase/pass.
4. Execute only that selected phase/pass.
5. Do not skip ahead.
6. Do not execute multiple phases/passes in one run unless the selected item explicitly says it is a combined pass.
7. If the selected item is too broad, update this plan with smaller child passes and stop.
8. If smaller passes are added, do not also implement source changes in the same run unless explicitly instructed.
9. After executing a phase/pass, update this plan with status, date, summary, verification results, and behavior notes.
10. Do not claim manual verification unless it was actually performed.
11. Keep this plan self-tracking so future fresh-context runs can resume from it.

```plan-state-json id="runtime-object-authoring"
{
  "plan_id": "sector_runtime_object_authoring_plan",
  "status_values": [
    "Not Started",
    "Planned",
    "In Progress",
    "Completed",
    "Deferred",
    "Blocked",
    "Partial"
  ],
  "items": [
    {
      "id": "phase_01",
      "title": "Placed Runtime Object Data And Serialization",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_01a",
      "title": "Add Placed Runtime Object Data Model And JSON Round Trip",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_01b",
      "title": "Add Object Definition Data And Goblin Definition",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_02",
      "title": "Spawn Placed Objects Into ECS Runtime",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02a",
      "title": "Instantiate Placed Billboard Objects Into EngineContext World",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02b",
      "title": "Replace Temporary Goblin Spawn With Placed Object Spawn Path",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "Editor Placement Selection And Inspector",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_03a",
      "title": "Add Object Placement Tool And 2D Overlay",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b",
      "title": "Add Object Selection Move Delete And Inspector",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_04",
      "title": "Preview Lifecycle And Runtime Reset Polish",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_04a",
      "title": "Make Placed Object Runtime Lifecycle Explicit",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_04b",
      "title": "Add Runtime Object Debug Status And Spawn Diagnostics",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_05",
      "title": "Tests Documentation And Completion",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_05a",
      "title": "Strengthen Tests Update Docs And Close Plan",
      "type": "pass",
      "parent": "phase_05",
      "status": "Completed"
    }
  ]
}
```

## Current Progress

| Phase / Pass                                                            | Status      | Date | Notes                                                             |
| ----------------------------------------------------------------------- | ----------- | ---- | ----------------------------------------------------------------- |
| Phase 1: Placed Runtime Object Data And Serialization                   | Completed   | 2026-07-01 | Phase 1A and Phase 1B complete.                                   |
| Phase 1A: Add Placed Runtime Object Data Model And JSON Round Trip      | Completed   | 2026-07-01 | Added `SectorPlacedRuntimeObject`, optional `runtimeObjects` JSON, topology-v2 and graph-native round-trip tests. Verification: `cmake --build cmake-build-debug -j2`, `ctest --test-dir cmake-build-debug --output-on-failure`. Behavior notes: missing `runtimeObjects` loads empty; empty lists are omitted on save; invalid IDs, duplicate IDs, empty `definitionId`, non-finite position, and non-finite yaw are rejected; renderer/ECS/editor behavior unchanged. |
| Phase 1B: Add Object Definition Data And Goblin Definition              | Completed   | 2026-07-01 | Added minimal `SectorRuntimeObjectDefinition` / billboard definition metadata and a built-in `goblin` definition for `assets/sprites/goblin.json`. Tests cover definition lookup, missing lookup, string clip names, and asset path existence. Verification: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_topology_serialization"`; `git diff --check`; `git diff --stat`; `git status --short`. Behavior notes: data-only registry; no serialization, renderer, ECS spawn, asset request, editor, topology cache, lightmap source-hash, collision, sector lookup, or physics behavior changed. |
| Phase 2: Spawn Placed Objects Into ECS Runtime                          | Completed   | 2026-07-01 | Phase 2A and Phase 2B complete.                                   |
| Phase 2A: Instantiate Placed Billboard Objects Into EngineContext World | Completed   | 2026-07-01 | Added `SpawnPlacedRuntimeObjects()` for explicit demo/editor preview setup, placed-ID to ECS entity mapping, billboard definition lookup, runtime asset-scope sprite animation requests, missing-definition skip warnings, and idempotent refresh without unloading the runtime object asset scope. Tests cover placed goblin spawn, missing definition skip, repeated refresh without duplicates, and explicit reset cleanup. Verification: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object`; `ctest --test-dir cmake-build-debug --output-on-failure`. Behavior notes: ECS runtime ownership now includes placed object entities in `EngineContext::world`; asset requests happen during explicit spawn/setup, not render/update; editor preview setup/rebuild now spawns placed objects; preview renderer rebuilds do not unload the runtime object asset scope; temporary F5 goblin path remains unchanged for Phase 2B; topology serialization, topology cache invalidation, lightmap source hash, collision, sector lookup logic, physics, and camera behavior unchanged; no manual GUI verification performed. |
| Phase 2B: Replace Temporary Goblin Spawn With Placed Object Spawn Path  | Completed   | 2026-07-01 | Removed the F5 temporary goblin construction path, runtime state handle, editor shortcut/status text, and cleanup test expectation. Persistent placed objects now remain the only runtime object spawn path. Verification: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object`; `git diff --check`; `git diff --stat`; `git status --short`. Behavior notes: source code changed; ECS runtime ownership remains through `EngineContext::world`; asset requests remain in explicit placed-object spawn/setup; no separate debug goblin asset key/path remains; preview lifecycle still spawns placed objects on 3D entry/rebuild without unloading the runtime object asset scope; topology serialization, topology cache invalidation, lightmap source hash, collision, sector lookup logic, physics, and camera behavior unchanged; no manual GUI verification performed. |
| Phase 3: Editor Placement Selection And Inspector                       | Completed   | 2026-07-01 | Phase 3A and Phase 3B complete.                                   |
| Phase 3A: Add Object Placement Tool And 2D Overlay                      | Completed   | 2026-07-01 | Added an `Object` map-object tool that places the v1 `goblin` definition at snapped X/Z inside a sector, using the containing sector `floorZ` for Y and yaw `0`; placement allocates a stable placed-object ID, marks the topology document dirty, invalidates the 2D topology render cache, selects/highlights the newly placed object marker, and respawns placed runtime objects through the existing ECS path when an engine context is available. Added cached 2D runtime-object markers with yaw ticks and warning styling for non-v1 definition IDs. Verification: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_authoring_graph|sector_topology_serialization"`; `ctest --test-dir cmake-build-debug --output-on-failure`; `git diff --check`. Behavior notes: source code changed; editor behavior now supports placement and overlay only; click-select/move/delete/inspector fields remain deferred to Phase 3B; placement is saved through existing `runtimeObjects` serialization; topology cache invalidation uses `MarkTopologyDocumentEdited()`; runtime object asset requests remain in explicit spawn/refresh, not render/update; topology schema, graph-native save/load format beyond existing `runtimeObjects`, lightmap source hash, collision, sector lookup logic, physics, and camera behavior unchanged; no manual GUI verification performed. |
| Phase 3B: Add Object Selection Move Delete And Inspector                | Completed   | 2026-07-01 | Added click selection for runtime object markers, 2D object dragging that preserves Y and yaw, Delete-key/delete-button removal with existing UI focus handling, and a basic inspector showing Object ID, Definition ID, fixed v1 goblin selector, Position X/Y/Z, Yaw in degrees, and Delete. Verification: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_authoring_graph|sector_topology_serialization"`; `ctest --test-dir cmake-build-debug --output-on-failure`; `git diff --check`. Behavior notes: source code changed; object edits mutate authored `runtimeObjects`, mark the topology document dirty, invalidate the 2D topology render cache through `MarkTopologyDocumentEdited()`, and respawn placed runtime objects through the existing ECS path when an engine context is available; save/reload uses existing runtime-object serialization; topology schema, graph-native save/load format beyond existing `runtimeObjects`, lightmap source hash, collision, sector lookup logic, physics, and camera behavior unchanged; no manual GUI verification performed. |
| Phase 4: Preview Lifecycle And Runtime Reset Polish                     | Completed   | 2026-07-01 | Phase 4A and Phase 4B complete.                                   |
| Phase 4A: Make Placed Object Runtime Lifecycle Explicit                 | Completed   | 2026-07-01 | Added `ResetSectorRuntimeObjectsForMap()` as the explicit map load/reload lifecycle helper, used it from demo init and editor level load, and kept preview enter/rebuild on refresh/spawn without unloading the runtime object asset scope. Demo init now clears spawned runtime objects if preview renderer setup fails. Tests cover preview refresh without duplicates or asset-scope churn, explicit map reset/reload without duplicates, clearing only `SectorObject` entities, and latest map data spawning after reset. Verification: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object`; `ctest --test-dir cmake-build-debug --output-on-failure`; `git diff --check`; `git diff --stat`; `git status --short`. Behavior notes: source code changed; new/load/reload runtime reset now clears `SectorObject` entities and unloads the runtime object asset scope before spawning current placements; preview resource rebuild does not clear `context.world`, does not duplicate placed objects, and keeps the runtime object asset scope; authored object serialization/save data, topology cache invalidation, lightmap source hash, collision, sector lookup logic, physics, and camera behavior unchanged; no manual GUI verification performed. |
| Phase 4B: Add Runtime Object Debug Status And Spawn Diagnostics         | Completed   | 2026-07-01 | Added runtime object debug counts for placed/spawned/skipped objects, retained first spawn warning text for missing definitions/unsupported kinds/asset-scope failures, and surfaced status/warnings in the 3D preview status panel. Tests cover successful placed-object counts/status and missing-definition warning diagnostics. Verification: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object`; `ctest --test-dir cmake-build-debug --output-on-failure`; `git diff --check`. Behavior notes: source code changed; editor debug/status display changed; ECS runtime ownership, asset loading lifecycle, authored object serialization/save data, topology cache invalidation, lightmap source hash, collision, sector lookup logic, physics, and camera behavior unchanged; no manual GUI verification performed. |
| Phase 5: Tests Documentation And Completion                             | Completed   | 2026-07-01 | Phase 5A complete; persistent runtime object authoring v1 is closed. |
| Phase 5A: Strengthen Tests Update Docs And Close Plan                   | Completed   | 2026-07-01 | Added placed runtime object edit/delete helper coverage and updated `docs/sector_editor.md` to document persistent Object-tool authoring, object definitions vs placed objects vs runtime ECS entities, the `EngineContext::world` ownership boundary, Aseprite cutout billboard lighting behavior, removed F5 temporary spawn status, and deferred NPC/collision/doors/model work. Verification: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_topology_serialization|sector_authoring_graph"`; `ctest --test-dir cmake-build-debug --output-on-failure`; `git diff --check`; `git diff --stat`; `git status --short`. Behavior notes: source code changed only in tests; docs and active plan updated; runtime/editor behavior unchanged; topology serialization schema and graph-native save/load behavior unchanged beyond existing `runtimeObjects`; topology cache invalidation behavior unchanged and remains through `MarkTopologyDocumentEdited()` for object edits; lightmap source hash, ECS runtime ownership, asset loading lifecycle, preview lifecycle, collision, sector lookup logic, physics, and camera behavior unchanged; no manual GUI verification performed. |

## Execution Tracking Rules

* Each pass must leave the project buildable and runnable.
* Each pass final report must state whether source code changed.
* Each implementation pass must update this document before finishing.
* The update should be small and local.
* Do not rewrite unrelated phases when marking progress.
* If behavior is intended to remain unchanged, explicitly state that.
* If a pass changes topology serialization, graph-native save/load, ECS runtime ownership, asset loading, editor behavior, preview lifecycle, or build/test behavior, clearly say so.
* Do not claim manual GUI verification unless it was actually performed.
* If a pass is too broad, split it into smaller child passes and stop without source changes.

## Goal And Desired End State

Add persistent runtime object authoring for the sector engine.

The immediate use case is placing the current goblin billboard sprite as level content instead of spawning it through temporary debug code.

Desired end state:

* Level/map data can contain placed runtime objects.
* A placed object references an object definition ID, such as `goblin`.
* Object definitions describe render type and asset data.
* The editor can place, select, move, inspect, and delete placed objects.
* Placed billboard objects spawn into `EngineContext::world`.
* Runtime objects use the existing ECS object/billboard/probe-lighting path.
* The temporary F5 goblin spawn path is removed or clearly demoted.
* `SectorMeshPreview` remains renderer-only and does not own ECS entities.
* Sectors/linedefs/sidedefs/lightmaps/draw records remain non-ECS purpose-built data.
* No NPC AI, combat, collision, doors, 3D model rendering, or object scripting is implemented in this plan.

## Architecture Policy

Use ECS for movable/high-level runtime objects.

Do not convert these into ECS:

* sectors
* vertices
* linedefs
* sidedefs
* authoring graph data
* generated sector geometry
* lightmap atlas data
* portal visibility graph
* static sector draw records

Use `EngineContext::world` as the authoritative ECS world.

Editor/demo/runtime layers may own lightweight sector object state, definitions, and spawn bookkeeping, but must operate on `EngineContext::world`.

`SectorMeshPreview` may render ECS objects passed to it, but must not:

* own `engine::World`
* create/destroy runtime object entities
* reserve ECS component pools
* reset runtime object state
* request object assets during render

## Object Model

This plan separates:

```text id="ohaj6r"
Object definition:
  what kind of object this is

Placed object:
  where one instance exists in a level

Runtime ECS entity:
  spawned instance used for update/render
```

### Object definition

A definition describes reusable object content.

First target definition:

```text id="izkif5"
id: goblin
kind: billboard
sprite animation: assets/sprites/goblin/<actual json file>
size
origin
directional clips:
  Front
  Back
  Left
  Right
```

The implementation should inspect the actual asset file names under `assets/sprites/goblin`.

Definition storage can be one of:

```text id="8ejka2"
- small built-in registry for v1
- JSON definition file under assets/objects/
- other simple project-local definition table
```

Prefer the smallest approach that keeps data-driven IDs and does not hardcode gameplay enums.

The definition ID should be a string, e.g. `"goblin"`.

### Placed object

A placed object is saved in the level/map.

Suggested shape:

```cpp id="35tk7u"
struct SectorPlacedRuntimeObject {
    int id = 0;
    std::string definitionId;
    Vector3 position = {};
    float yawRadians = 0.0f;
};
```

Use actual project style and coordinate conventions.

Suggested JSON field:

```json id="sxnrc4"
"runtimeObjects": [
  {
    "id": 1,
    "definitionId": "goblin",
    "position": [12.0, 1.2, 8.0],
    "yawDegrees": 90.0
  }
]
```

Use the project’s existing save style:

* stable positive IDs
* omit defaults where appropriate
* load missing `runtimeObjects` as empty
* validate bad values defensively
* old maps load safely

### Runtime entity

On level/preview runtime spawn, placed objects become ECS entities.

For a billboard definition, add components such as:

* `SectorObject`
* `SectorObjectTransform`
* `SectorObjectLighting`
* `SectorBillboardSprite`
* `SectorBillboardAnimator`

Use actual existing component names from the billboard branch.

## Proposed Phases

### Phase 1: Placed Runtime Object Data And Serialization

Goal:

Add persistent placed object data and object definition metadata without spawning/rendering changes yet.

Why it helps:

This creates the authoring/storage foundation before editor tools and ECS spawning.

Files/functions likely touched:

* topology/map types
* topology serialization
* object definition helpers
* tests for serialization
* docs

Exact behavior that must remain unchanged:

* Existing graph-native maps load/save unchanged except optional new field support.
* Old maps with no runtime objects load as empty.
* Preview rendering unchanged.
* Temporary goblin spawn unchanged until later phase.
* ECS runtime object behavior unchanged until Phase 2.

Risks/goblins:

* Mixing definition data and placement data.
* Hardcoding `goblin` too deeply.
* Breaking old map load.
* Confusing world units vs authoring units.
* Adding NPC/gameplay fields too early.

Non-goals:

* No editor placement yet.
* No ECS spawning yet.
* No object inspector yet.
* No AI/combat/collision/doors.
* No persistent dynamic lights attached to objects.

Suggested checks:

```bash id="ku8y4p"
git diff --check
git diff --stat
git status --short
```

Run serialization tests.

Final report expectations:

* State data types added.
* State JSON field names.
* State default/missing behavior.
* State validation behavior.
* State whether renderer/ECS behavior changed.
* State verification results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 1 `Completed` only after Phase 1A and Phase 1B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 1A: Add Placed Runtime Object Data Model And JSON Round Trip

Goal:

Add persistent placed object data to map serialization.

Implementation guidance:

Add a map-level placed runtime object list.

Suggested fields:

```text id="4k1ddt"
id
definitionId
position
yawRadians or yawDegrees in JSON
```

Rules:

* stable positive IDs
* missing list loads as empty
* empty list omitted on save if that matches project style
* invalid definitionId rejected or loaded as invalid according to existing validation style
* invalid/non-finite position rejected
* invalid/non-finite yaw rejected
* duplicate IDs handled consistently with other map data

Do not spawn objects into ECS yet.

Tests:

* missing runtimeObjects loads as empty
* save/load round trip
* empty list omit behavior if applicable
* invalid data rejected defensively
* old maps still load
* graph-native save/load preserves runtime objects

#### Phase 1B: Add Object Definition Data And Goblin Definition

Goal:

Add minimal object definition support for the goblin billboard.

Implementation guidance:

Add a simple object definition registry or loader.

For v1, it is acceptable to have one built-in/test definition if clearly marked and easy to replace later, but prefer a small data file if that fits the project style.

Definition should include:

```text id="1j6av8"
id = goblin
kind = billboard
sprite animation asset path
sizeWorld
originNormalized
directional clip names:
  Front
  Back
  Left
  Right
```

Use string IDs for logical animation/direction names. Do not create hardcoded gameplay enums such as `WALK_FRONT`.

Asset requests should still go through `AssetManager::RequestSpriteAnimation()` when the object is spawned/loaded, not during serialization parsing.

Tests:

* goblin definition exists/resolves
* missing definition lookup fails safely
* clip names stored as strings
* no asset request occurs during pure data serialization tests unless that is already a project convention

### Phase 2: Spawn Placed Objects Into ECS Runtime

Goal:

Convert placed map objects into ECS runtime entities.

Why it helps:

This connects persistent level content to the existing billboard renderer and object lighting system.

Files/functions likely touched:

* `SectorRuntimeObjects.*`
* sector editor/demo update/load paths
* object definition registry
* asset request logic
* tests

Exact behavior that must remain unchanged:

* `EngineContext::world` remains authoritative.
* `SectorMeshPreview` remains renderer-only.
* Existing temporary goblin spawn remains until Phase 2B.
* Sector topology/lightmap/rendering unchanged.

Risks/goblins:

* Spawning duplicate ECS entities on every preview rebuild.
* Requesting sprite assets every frame.
* Resetting `EngineContext::world` too broadly.
* Losing runtime objects during renderer resource rebuild.
* Asset scope lifetime churn.

Non-goals:

* No editor placement yet.
* No NPC AI.
* No collision/physics.
* No persistent object inspector yet.

Suggested checks:

```bash id="b57drp"
git diff --check
git diff --stat
git status --short
```

Run runtime object tests.

Final report expectations:

* State spawn lifecycle.
* State asset scope behavior.
* State how placed IDs map to runtime entities.
* State reset rules.
* State verification results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 2 `Completed` only after Phase 2A and Phase 2B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 2A: Instantiate Placed Billboard Objects Into EngineContext World

Goal:

Spawn placed runtime objects from map data into ECS.

Implementation guidance:

At explicit level/runtime setup points:

```text id="ag8qm5"
clear sector runtime object entities
load/refresh object definitions
request needed sprite animation assets
spawn placed objects into context.world
```

For each placed object:

* lookup definition by `definitionId`
* if missing, record warning and skip
* if billboard definition, create entity
* add transform/current sector/lighting/billboard components
* store mapping from placed object ID to entity if useful for editor selection later

Asset scope:

* use sector runtime object asset scope
* do not request assets in render loop
* do not unload assets on preview renderer rebuild
* unload/clear on explicit runtime object reset/new/load/shutdown

Tests:

* placed goblin spawns one entity
* missing definition skipped with warning
* repeated preview rebuild does not duplicate entity
* explicit runtime reset clears only `SectorObject` entities
* asset request path is not called every frame if testable

#### Phase 2B: Replace Temporary Goblin Spawn With Placed Object Spawn Path

Goal:

Remove or demote the F5 temporary goblin spawn once persistent placed objects work.

Implementation guidance:

Options:

1. Remove F5 temporary spawn entirely.
2. Keep F5 only as a clearly marked debug helper that creates a placed-object-like test instance through the same object definition/spawn code.

Preferred:

* make persistent placed objects the primary path
* keep any remaining F5 path clearly marked:

```text id="xubett"
TODO_REMOVE_BILLBOARD_TEST_SPAWN
```

Do not keep a second separate goblin construction path that bypasses object definitions.

Tests/manual smoke:

* placed goblin appears without F5 if map contains object
* F5 debug path, if retained, uses same definition/spawn helpers
* no duplicate goblins after preview rebuild

### Phase 3: Editor Placement Selection And Inspector

Goal:

Add basic authoring tools for placed runtime objects.

Why it helps:

This turns billboard objects into actual level content.

Files/functions likely touched:

* sector editor tool panel
* editor selection state
* 2D overlay/drawing
* inspector UI
* topology/action/save dirty paths
* tests where available

Exact behavior that must remain unchanged:

* Existing sector/line/vertex/face/light tools continue working.
* Runtime spawned object rendering continues working.
* Object editing marks document dirty and saves to map.
* No AI/collision/gameplay behavior added.

Risks/goblins:

* Object selection conflicting with sectors/lines/lights.
* Deleting object while text input focused.
* Tool accidentally moving runtime ECS entity but not authored placed object.
* Save/reload mismatch.
* Y/height convention confusing in 2D editor.

Non-goals:

* No NPC behavior fields.
* No placement of arbitrary 3D models.
* No object asset browser beyond simple goblin definition if that is all that exists.
* No in-world 3D transform gizmo.

Suggested checks:

```bash id="6vg4m3"
git diff --check
git diff --stat
git status --short
```

Run editor/serialization tests.

Final report expectations:

* State object tool labels.
* State selection behavior.
* State inspector fields.
* State dirty/save behavior.
* State manual GUI smoke status.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 3 `Completed` only after Phase 3A and Phase 3B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 3A: Add Object Placement Tool And 2D Overlay

Goal:

Add a 2D editor tool to place runtime objects.

Implementation guidance:

Add a tool/button label such as:

```text id="l6wfrt"
Object
```

For v1, the only selectable definition may be:

```text id="2w9z3i"
goblin
```

Placement:

* click in a sector to place object at snapped or unsnapped world X/Z according to existing editor style
* choose Y as sector floor + reasonable default, or store floor-relative height if the existing transform convention supports it
* default yaw can be current camera yaw, 0, or tool setting; document choice
* assign stable placed object ID
* mark document dirty
* respawn/update runtime object scene

Overlay:

* draw object marker in 2D
* show direction/yaw tick/arrow
* selected object highlighted
* missing-definition object shown with warning color/style if practical

Manual smoke:

* place goblin object
* save/reload
* marker persists
* runtime goblin appears in 3D preview

#### Phase 3B: Add Object Selection Move Delete And Inspector

Goal:

Add basic editing for placed runtime objects.

Inspector fields:

```text id="0yifwt"
Object ID
Definition ID
Position X/Y/Z
Yaw
Delete
```

If editing `Definition ID` as free text is too clunky, use a simple fixed goblin selector for now and document future object definition picker.

Editing rules:

* edits mutate authored placed object data, not just runtime ECS component state
* mark document dirty
* update/rebuild runtime object entity for that placed object
* Delete removes authored object and runtime entity
* Delete key should respect UI text focus rules, matching existing editor behavior

Move behavior:

* move selected object in 2D
* preserve yaw
* update sector/current runtime state
* save/reload works

Tests/manual smoke:

* select object
* move object
* edit yaw/position
* delete object
* save/reload after edits
* runtime preview updates

### Phase 4: Preview Lifecycle And Runtime Reset Polish

Goal:

Make persistent object spawn/reset lifecycle predictable.

Why it helps:

Persistent ECS objects should not disappear during renderer rebuilds or duplicate during reload.

Files/functions likely touched:

* sector editor state/load/new/reload paths
* sector demo runtime paths
* preview mode transition code
* runtime object state helpers
* tests

Exact behavior that must remain unchanged:

* `EngineContext::world` stays top-level.
* `SectorMeshPreview` stays renderer-only.
* Existing sector rendering unchanged.
* Save/load object data unchanged.

Risks/goblins:

* Duplicate entities after load/reload.
* Objects disappearing on preview resource rebuild.
* Runtime entities not matching authored objects.
* Stale asset scopes after level change.
* ECS entities from other future systems accidentally cleared.

Non-goals:

* No object streaming.
* No gameplay persistence beyond authored placements.
* No undo/redo unless existing edit path provides it.

Suggested checks:

```bash id="ezmo32"
git diff --check
git diff --stat
git status --short
```

Run lifecycle/runtime tests.

Final report expectations:

* State reset points.
* State rebuild behavior.
* State mapping from authored object to entity.
* State verification results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 4 `Completed` only after Phase 4A and Phase 4B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 4A: Make Placed Object Runtime Lifecycle Explicit

Goal:

Ensure object runtime lifecycle is explicit and safe.

Implementation guidance:

Define exact reset/spawn points:

```text id="1pq8zq"
New level:
  clear sector runtime objects
  clear placed object list or initialize empty

Load level:
  clear sector runtime objects
  load placed object data
  spawn placed objects

Reload current level:
  clear sector runtime objects
  reload placed object data
  spawn placed objects

Preview renderer resource rebuild:
  do not clear context.world
  do not duplicate placed objects
```

Implement helper names clearly, such as:

```text id="ul3s2l"
ClearSectorRuntimeObjects(...)
SpawnPlacedRuntimeObjects(...)
RefreshPlacedRuntimeObjects(...)
```

Clear only entities marked with `SectorObject`.

Tests:

* preview rebuild preserves spawned entities
* load/reload does not duplicate entities
* clear only removes `SectorObject` entities
* asset scope is unloaded only on explicit reset/shutdown

#### Phase 4B: Add Runtime Object Debug Status And Spawn Diagnostics

Goal:

Make object spawn state debuggable.

Possible debug/status:

```text id="4t4144"
runtime objects: 3 placed / 3 spawned
runtime object warnings: missing definition goblin_bad
```

Use existing debug UI/status patterns.

Do not overbuild.

Manual smoke:

* missing definition shows warning
* placed/spawned count makes sense
* object debug does not clutter normal editing too much

### Phase 5: Tests Documentation And Completion

Goal:

Strengthen tests/docs and close the placed runtime object plan.

Why it helps:

This system becomes the foundation for doors, NPCs, pickups, props, projectiles, and future 3D models.

Files/functions likely touched:

* tests
* docs
* this plan document
* maybe temporary spawn cleanup

Exact behavior that must remain unchanged:

* No doors yet.
* No NPC AI.
* No collision components.
* No 3D model rendering.
* No object inspector beyond basic placed-object fields.

Risks/goblins:

* Temporary debug paths lingering unclearly.
* Missing docs around definition vs placement vs runtime entity.
* Future tasks using old 2D renderers by mistake.
* Object data tied too tightly to goblin test asset.

Non-goals:

* No cleanup crusade across old 2D demo systems unless tiny and clearly safe.
* No actor physics/combat.
* No object scripting.

Suggested checks:

```bash id="iesilx"
git diff --check
git diff --stat
git status --short
```

Run full relevant test suite.

Final report expectations:

* State docs updated.
* State tests added/updated.
* State final temporary spawn status.
* State known deferred work.
* State verification results.

How to update this plan after completion:

* Mark Phase 5A and Phase 5 `Completed`.
* If all phases are complete, ensure parent phases are `Completed`.
* Leave final completion note.

#### Phase 5A: Strengthen Tests Update Docs And Close Plan

Goal:

Finish persistent runtime object authoring v1.

Tests should cover:

* placed object serialization
* old maps without runtime objects
* missing/invalid placed object fields
* goblin definition lookup
* placed object spawn into ECS
* missing definition skip/warning
* clear only `SectorObject` entities
* preview rebuild does not duplicate
* save/reload preserves placed object
* inspector/object edit mutation if testable

Docs should state:

* `EngineContext::world` owns ECS runtime objects
* sector topology remains non-ECS
* object definitions are reusable content descriptions
* placed objects are authored level data
* runtime ECS entities are spawned from placed objects
* sector-world billboard renderer uses Aseprite assets
* billboards are cutout-only
* billboards use baked object probes and dynamic lights
* placement editor/inspector is now basic v1
* NPC AI/combat/doors/collision/3D models are deferred

## Deferred Decisions For Later Plans

These are intentionally out of scope:

* Door/lift runtime objects.
* Runtime object collision components.
* Actor/NPC AI.
* Combat/health/damage.
* Party system.
* TAB command overlay.
* Persistent object scripting.
* Object asset browser.
* Full object definition file format if v1 uses built-in registry.
* 3D glTF model renderer.
* Skeletal animation.
* Pickups/projectiles.
* Attached lights.
* Transparent particles/spells/smoke/glass.
* Cleanup/removal of old 2D sprite/rectangle test systems unless done as a later cleanup task.

## Final Completion Criteria

This plan is complete when:

* placed runtime objects are saved/loaded in the level/map
* object definitions exist at least for the goblin billboard
* placed goblin objects spawn into `EngineContext::world`
* persistent goblin billboards render in 3D preview
* object placement tool exists
* selected objects can be moved/deleted/inspected
* runtime object lifecycle is explicit across new/load/reload/preview rebuild
* F5 temporary goblin path is removed or clearly demoted to shared helper/debug path
* no ECS ownership is added to `SectorMeshPreview`
* no sector topology/lightmap/rendering architecture is converted to ECS
* no doors/NPC/combat/3D-model scope leaks into this implementation
