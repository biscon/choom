# Sector Door System V1 Plan

## How To Use This Plan

This is a living execution plan for implementing the first version of procedural 3D portal-attached doors.

When an agent is asked to execute this plan, it must:

1. Read this section first.
2. Read `docs/audit/sector_door_system_audit.md`, especially sections 3, 5, 6, 7, 9, 10, 11, and 13.
3. Read the `plan-state-json` block.
4. Identify the selected phase/pass.
5. Execute only that selected phase/pass.
6. Do not skip ahead.
7. Do not execute multiple phases/passes in one run unless the selected item explicitly says it is a combined pass.
8. If the selected item is too broad, update this plan with smaller child passes and stop.
9. If smaller passes are added, do not also implement source changes in the same run unless explicitly instructed.
10. After executing a phase/pass, update this plan with status, date, summary, verification results, and behavior notes.
11. Do not claim manual GUI verification unless it was actually performed.
12. Keep this plan self-tracking so future fresh-context runs can resume from it.

Important product decisions:

- Do not implement Doom-style dynamic sector-height doors in this plan.
- Do not rebuild sector meshes to animate doors in this plan.
- Doors are procedural 3D runtime objects attached to portal segments.
- Portals remain zero-thickness logical connections.
- Doors own thickness, render mesh/slab, dynamic collider state, animation state, and dynamic portal-block state.
- Door mutable runtime state lives in ECS.
- `SectorMeshPreview` remains renderer-only.
- V1 supports a simple procedural slab door with slide motion. Hinged/swing doors are explicitly future work.
- V1 includes basic interactivity:
  - authored `autoOpen` option
  - when `autoOpen` is true, doors open automatically as the player approaches
  - when `autoOpen` is false, the player can use the Interact key `F` to toggle/open the targeted nearby door
  - no broader interaction framework or scripting system is required beyond the smallest door-focused input path.

```plan-state-json id="sector-door-system-v1"
{
  "plan_id": "sector_door_system_v1_plan",
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
      "title": "Authored Door Data And Serialization",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_01a",
      "title": "Add Door Payload And JSON Round Trip",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_01b",
      "title": "Add Door Defaults Validation And Fixture Coverage",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_02",
      "title": "Portal Anchor Resolution And Diagnostics",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02a",
      "title": "Resolve Door Anchors To Portal Basis",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02b",
      "title": "Surface Door Anchor Diagnostics",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "Runtime ECS Door Spawn And Closed Slab Rendering",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_03a",
      "title": "Spawn Door ECS Components",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b",
      "title": "Render Closed Procedural Door Slabs",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_04",
      "title": "Door Motion And Runtime State",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_04a",
      "title": "Add Door Motion Update",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_04b",
      "title": "Add Door Transform Collider And Blocker State",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_05",
      "title": "Door Interaction",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_05a",
      "title": "Add Door Auto Open Runtime Behavior",
      "type": "pass",
      "parent": "phase_05",
      "status": "Completed"
    },
    {
      "id": "phase_05b",
      "title": "Add Interact Key Door Toggle",
      "type": "pass",
      "parent": "phase_05",
      "status": "Completed"
    },
    {
      "id": "phase_06",
      "title": "Dynamic Collision Blockers",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_06a",
      "title": "Collect Door Dynamic Colliders",
      "type": "pass",
      "parent": "phase_06",
      "status": "Completed"
    },
    {
      "id": "phase_06b",
      "title": "Resolve Player Movement Against Door Colliders",
      "type": "pass",
      "parent": "phase_06",
      "status": "Completed"
    },
    {
      "id": "phase_07",
      "title": "Dynamic Portal Visibility Blockers",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_07a",
      "title": "Add Dynamic Portal Blocker Query",
      "type": "pass",
      "parent": "phase_07",
      "status": "Not Started"
    },
    {
      "id": "phase_07b",
      "title": "Wire Door Blockers Into Preview Visibility",
      "type": "pass",
      "parent": "phase_07",
      "status": "Not Started"
    },
    {
      "id": "phase_08",
      "title": "Door Editor Tool And Inspector",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_08a",
      "title": "Add Door Placement Tool And 2D Footprint",
      "type": "pass",
      "parent": "phase_08",
      "status": "Not Started"
    },
    {
      "id": "phase_08b",
      "title": "Add Door Inspector Core Fields",
      "type": "pass",
      "parent": "phase_08",
      "status": "Not Started"
    },
    {
      "id": "phase_08c",
      "title": "Add Door Inspector Interaction Controls",
      "type": "pass",
      "parent": "phase_08",
      "status": "Not Started"
    },
    {
      "id": "phase_09",
      "title": "Door Material V1",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_09a",
      "title": "Add Single Texture Door Material",
      "type": "pass",
      "parent": "phase_09",
      "status": "Not Started"
    },
    {
      "id": "phase_09b",
      "title": "Add Door Texture Picker Integration",
      "type": "pass",
      "parent": "phase_09",
      "status": "Not Started"
    },
    {
      "id": "phase_10",
      "title": "Tests Documentation And Cleanup",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_10a",
      "title": "Strengthen Door Tests And Diagnostics",
      "type": "pass",
      "parent": "phase_10",
      "status": "Not Started"
    },
    {
      "id": "phase_10b",
      "title": "Update Documentation And Close Plan",
      "type": "pass",
      "parent": "phase_10",
      "status": "Not Started"
    }
  ]
}
```

## Current Progress

| Phase / Pass | Status | Date | Notes |
| --- | --- | --- | --- |
| Phase 1: Authored Door Data And Serialization | Completed | 2026-07-02 | Authored door serialization/default validation passes complete. |
| Phase 1A: Add Door Payload And JSON Round Trip | Completed | 2026-07-02 | Added authored door payload structs and JSON round-trip support; data model and JSON only. |
| Phase 1B: Add Door Defaults Validation And Fixture Coverage | Completed | 2026-07-02 | Tightened door defaults/validation and added invalid JSON fixture coverage. |
| Phase 2: Portal Anchor Resolution And Diagnostics | Completed | 2026-07-02 | Anchor resolution and diagnostics surfacing passes complete. |
| Phase 2A: Resolve Door Anchors To Portal Basis | Completed | 2026-07-02 | Added resolver helper for portal basis/opening/default dimensions. |
| Phase 2B: Surface Door Anchor Diagnostics | Completed | 2026-07-02 | Added runtime/editor-facing door anchor diagnostic counts and messages. |
| Phase 3: Runtime ECS Door Spawn And Closed Slab Rendering | Completed | 2026-07-02 | Door ECS spawn and closed procedural slab rendering passes complete. |
| Phase 3A: Spawn Door ECS Components | Completed | 2026-07-02 | Valid authored doors now spawn ECS entities with door-specific components; no rendering yet. |
| Phase 3B: Render Closed Procedural Door Slabs | Completed | 2026-07-02 | Added renderer-only procedural slab drawing for ECS doors. |
| Phase 4: Door Motion And Runtime State | Completed | 2026-07-02 | Door motion advancement and derived transform/collider/blocker state passes complete. |
| Phase 4A: Add Door Motion Update | Completed | 2026-07-02 | Added deterministic ECS door motion advancement. |
| Phase 4B: Add Door Transform Collider And Blocker State | Completed | 2026-07-02 | Derived current slab transform, collider OBB state, and portal blocker state from ECS door motion. |
| Phase 5: Door Interaction | Completed | 2026-07-02 | Auto-open and Interact key passes complete. |
| Phase 5A: Add Door Auto Open Runtime Behavior | Completed | 2026-07-02 | Player proximity opens auto-open doors and closes them when leaving range. |
| Phase 5B: Add Interact Key Door Toggle | Completed | 2026-07-02 | Added minimal F interact path for targeted non-auto doors. |
| Phase 6: Dynamic Collision Blockers | Completed | 2026-07-02 | Door dynamic collider collection and player movement resolution passes complete. |
| Phase 6A: Collect Door Dynamic Colliders | Completed | 2026-07-02 | Added ECS door dynamic collider snapshot collection; movement resolution remains scoped to Phase 6B. |
| Phase 6B: Resolve Player Movement Against Door Colliders | Completed | 2026-07-02 | Added post-static player movement resolution against collected door OBB colliders. |
| Phase 7: Dynamic Portal Visibility Blockers | Not Started |  | Parent phase. |
| Phase 7A: Add Dynamic Portal Blocker Query | Not Started |  | Static graph plus dynamic blocker overlay. |
| Phase 7B: Wire Door Blockers Into Preview Visibility | Not Started |  | Preview traversal uses door blockers. |
| Phase 8: Door Editor Tool And Inspector | Not Started |  | Parent phase. |
| Phase 8A: Add Door Placement Tool And 2D Footprint | Not Started |  | Click portal to place door. |
| Phase 8B: Add Door Inspector Core Fields | Not Started |  | Dimensions/motion/anchor fields. |
| Phase 8C: Add Door Inspector Interaction Controls | Not Started |  | Auto open and debug/open controls. |
| Phase 9: Door Material V1 | Not Started |  | Parent phase. |
| Phase 9A: Add Single Texture Door Material | Not Started |  | One texture ID for all faces. |
| Phase 9B: Add Door Texture Picker Integration | Not Started |  | Reuse topology texture picker. |
| Phase 10: Tests Documentation And Cleanup | Not Started |  | Parent phase. |
| Phase 10A: Strengthen Door Tests And Diagnostics | Not Started |  | Focused tests and diagnostics pass. |
| Phase 10B: Update Documentation And Close Plan | Not Started |  | Docs and final closure. |

## Pass Execution Log

### Phase 1A: Add Door Payload And JSON Round Trip

- Status: Completed
- Date: 2026-07-02
- Summary: Added authored `SectorDoorAnchor`, `SectorPlacedDoor`, and `SectorDoorMotionType` data to runtime objects, plus `kind: "door"` JSON read/write support with nested `door` payload round-trip coverage.
- JSON/default behavior: Door anchors save stable IDs and exact integer endpoint coordinates. Default door values are omitted on save and restored on load for dimensions, thickness, normal offset, `slide_vertical` motion, open distance, speed, initial open fraction, `autoOpen`, interaction distances, and empty texture ID. Non-default `autoOpen`, `interactionDistance`, and `autoOpenDistance` are serialized as authored settings only.
- Validation behavior: This pass rejects unknown door motion strings and non-finite authored door numeric values on save/load, while stricter positive/range/anchor validation remains scoped to Phase 1B.
- Behavior notes: Existing billboard runtime object serialization remains supported. Renderer behavior, runtime ECS spawn/update behavior, collision, portal visibility, topology render-cache invalidation, and lightmap source-hash behavior are unchanged.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_topology_serialization_tests sector_runtime_object_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R 'sector_topology_serialization|sector_runtime_object'` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed; `git diff --check` passed.

### Phase 1B: Add Door Defaults Validation And Fixture Coverage

- Status: Completed
- Date: 2026-07-02
- Summary: Tightened door validation so anchor IDs must be positive, endpoint coordinates must remain exact integer pairs, width/height/openDistance accept zero-derived defaults but reject negatives, thickness and interaction distances require positive values, speed rejects negatives, and `initialOpenFraction` must stay within `0..1`.
- JSON/default behavior: Existing default omission/restoration is unchanged; zero width, height, and open distance remain valid derived defaults. Added in-memory invalid JSON coverage for door dimensions, interaction distances, initial open fraction, anchor IDs/types, texture ID type, and mixed billboard + door runtime objects.
- Validation behavior: Unknown motion strings remain rejected. Door validation now runs after loading door payloads and during save validation.
- Behavior notes: Existing billboard runtime object serialization remains supported. Renderer behavior, runtime ECS spawn/update behavior, collision, portal visibility, topology render-cache invalidation, and lightmap source-hash behavior are unchanged.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_topology_serialization_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_topology_serialization` passed; `cmake --build cmake-build-debug -j2 --target sector_runtime_object_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed; `git diff --check` passed; `git diff --stat` reviewed; `git status --short` reviewed.

### Phase 2A: Resolve Door Anchors To Portal Basis

- Status: Completed
- Date: 2026-07-02
- Summary: Added `ResolveSectorDoorAnchor()` and `SectorResolvedDoorAnchor` to resolve authored door anchors by `lineDefId` into current portal endpoints, midpoint, tangent, front-to-back normal, vertical opening, portal dimensions, and resolved door width/height defaults.
- Basis/origin behavior: Endpoints are read from the current linedef vertices in world XZ. Tangent follows linedef start-to-end. Normal points from the stored front sector toward the stored back sector. Vertical opening is `max(front.floorZ, back.floorZ)` to `min(front.ceilingZ, back.ceilingZ)` converted to world units; zero authored width/height derive from portal width/opening height.
- Invalid-anchor behavior: Missing linedef, one-sided/non-portal linedef, sidedef ID mismatch, invalid front/back sidedef pairing, sector-pair mismatch, missing sector/vertex, zero-length segment, and non-positive vertical opening return an invalid result with a diagnostic string instead of guessing.
- Behavior notes: No runtime ECS door spawn, rendering, collision, portal visibility traversal, editor diagnostics, topology mutation, 2D topology render-cache invalidation, serialization behavior, asset loading, or lightmap source-hash behavior changed.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_runtime_object_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `cmake --build cmake-build-debug -j2 --target sector_topology_tests sector_runtime_object_tests` passed after removing an unintended `SectorTopologyUnits.cpp` link dependency from the resolver; `ctest --test-dir cmake-build-debug --output-on-failure -R 'sector_topology_validation|sector_runtime_object'` passed; `git diff --check` passed; `git diff --stat` reviewed; `git status --short` reviewed.

### Phase 2B: Surface Door Anchor Diagnostics

- Status: Completed
- Date: 2026-07-02
- Summary: Added `SectorDoorAnchorDiagnostic` plus door anchor counts to `SectorRuntimeObjectState`, populated during `RefreshSectorRuntimeObjectMapData()` by resolving authored `kind: "door"` anchors.
- Invalid-anchor behavior: Invalid door anchors now produce placed-object/linedef diagnostic messages and warning/status counts. Door ECS spawn remains deferred; authored door objects are skipped safely without creating entities or asset scopes. Valid billboard runtime object behavior is unchanged when invalid door anchors are present.
- Behavior notes: No topology mutation, 2D topology render-cache invalidation, serialization schema/default behavior, renderer behavior, collision/sector lookup/physics behavior, portal visibility traversal, asset loading policy, or lightmap source-hash behavior changed.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_runtime_object_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R 'sector_topology|sector_runtime_object'` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed.

### Phase 3A: Spawn Door ECS Components

- Status: Completed
- Date: 2026-07-02
- Summary: Added door-specific ECS components for placed object identity, resolved portal anchor basis, motion state, render payload, collider marker, and portal blocker marker. Valid authored `kind: "door"` runtime objects now spawn into `EngineContext::world` through the existing runtime object reset/refresh lifecycle.
- Spawn lifecycle and reset behavior: `SpawnPlacedRuntimeObjects()` replaces existing mapped runtime entities before respawn, skips invalid door anchors with existing diagnostics, and stores valid door placed-object ID to ECS entity mappings. Door spawn reuses `SectorObjectTransform`, `SectorObject`, and `SectorObjectLighting`; no runtime object asset scope is created for doors yet because material loading is deferred.
- Runtime state behavior: `openFraction` and `targetOpenFraction` initialize from authored `initialOpenFraction`. No auto-open behavior, Interact key behavior, animation update, renderer draw path, collision response, or portal visibility traversal is implemented in this pass.
- Behavior notes: Serialization schema/default behavior, static sector rendering, billboard rendering, topology mutation/cache invalidation, collision/sector lookup/physics behavior, portal visibility traversal, asset loading policy, and lightmap source-hash behavior are unchanged. Door entities participate in the existing object-sector lookup and baked object lighting sample update through shared runtime object components.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_runtime_object_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `git diff --check` passed; `git diff --stat` reviewed; `git status --short` reviewed.

### Phase 3B: Render Closed Procedural Door Slabs

- Status: Completed
- Date: 2026-07-02
- Summary: Added a renderer-only `DrawRuntimeDoors()` path in `SectorMeshPreview` that observes ECS door components and draws opaque procedural box/slab geometry from the resolved portal basis, transform, dimensions, thickness, and normal offset established during spawn.
- Renderer path: Door slabs are emitted with immediate-mode quads after static sector geometry and before runtime billboards. The pass uses the existing sector object shader with lightmap and decal inputs disabled, depth test/write enabled, blending disabled, and a default material texture fallback when the door `textureId` is empty or unresolved. No per-door mesh allocation or sector mesh rebuild is performed.
- Lighting approach: Door faces use the ECS `SectorObjectLighting` object-probe ambient cube by face normal and receive the existing selected dynamic point/spot light uniforms. Doors do not use surface lightmaps and do not participate in bloom in this pass.
- Behavior notes: Door animation/open fraction transforms, collision, portal visibility blocking, interaction, editor placement/inspection, serialization schema/default behavior, topology mutation/cache invalidation, asset loading policy, and lightmap source-hash behavior are unchanged. No manual GUI verification was performed.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_runtime_object_tests` passed with no work needed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `git diff --check` passed; `git diff --stat` reviewed; `git status --short` reviewed.

### Phase 4A: Add Door Motion Update

- Status: Completed
- Date: 2026-07-02
- Summary: Added `AdvanceSectorDoorMotionSystem()` and wired it into `UpdateSectorRuntimeObjects()` so enabled ECS doors advance `openFraction` deterministically toward clamped `targetOpenFraction`.
- Motion semantics: Door `speed` is treated as world units per second and converted to fraction-per-second through `openDistance`. Authored zero `openDistance` derives to portal opening height for `slide_vertical` and portal width for `slide_left` / `slide_right` at spawn. Non-finite or non-positive dt is ignored, zero/non-finite speed is safe and does not advance, and invalid current/target fractions are clamped or reset into `0..1`.
- Implemented motion types: `slide_vertical`, `slide_left`, and `slide_right` all share the same fraction update semantics in this pass. Physical transform offset by motion type remains scoped to Phase 4B.
- Behavior notes: Source code changed. Authored `initialOpenFraction` remains unchanged during runtime updates. Save/load schema, renderer transform derivation, collider shape, portal blocker state, collision/sector lookup/physics behavior, portal visibility traversal, topology mutation/cache invalidation, asset loading policy, and lightmap source-hash behavior are unchanged. No manual GUI verification was performed.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_runtime_object_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `cmake --build cmake-build-debug -j2` passed; `git diff --check` passed; `git diff --stat` reviewed; `git status --short` reviewed.

### Phase 4B: Add Door Transform Collider And Blocker State

- Status: Completed
- Date: 2026-07-02
- Summary: Added `UpdateSectorDoorDerivedStateSystem()` so ECS doors derive current `SectorObjectTransform`, `SectorDoorCollider` OBB footprint/vertical interval, and `SectorDoorPortalBlocker::blocksPortal` from resolved anchor data, render dimensions, and `SectorDoorMotion`.
- Motion semantics: Phase 4A fraction advancement remains unchanged. `speed` is still world units per second converted through `openDistance`; Phase 4B only derives the current physical state from the resulting `openFraction`.
- Implemented motion types: `slide_vertical` offsets the slab upward by `openFraction * openDistance`; `slide_left` offsets along negative portal tangent; `slide_right` offsets along positive portal tangent. Closed center remains portal midpoint plus normal offset and half door height above the resolved opening bottom.
- Transform/collider/blocker behavior: Spawned doors now run the derived-state system so authored `initialOpenFraction` affects the initial ECS transform. Collider state stores current XZ center, tangent, normal, half extents, bottom, top, and an enabled flag for valid enabled doors. Portal blocker state uses `openFraction <= 0.001f` as the closed threshold and opens beyond that threshold.
- Behavior notes: Source code changed. Renderer door drawing continues to consume `SectorObjectTransform`, so it now follows the current derived slab transform. Collision response and portal visibility traversal are still not wired to these ECS components. Authored data/save-load schema, `initialOpenFraction` persistence, topology mutation/cache invalidation, asset loading policy, and lightmap source-hash behavior are unchanged. Collision/sector lookup/physics behavior is unchanged except door ECS transforms can update the existing object-sector/light-probe sampling point. No manual GUI verification was performed.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_runtime_object_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `cmake --build cmake-build-debug -j2` passed; `git diff --check` passed; `git diff --stat` reviewed; `git status --short` reviewed.

### Phase 5A: Add Door Auto Open Runtime Behavior

- Status: Completed
- Date: 2026-07-02
- Summary: Added ECS `SectorDoorInteraction` runtime settings copied from authored door interaction fields, plus `UpdateSectorDoorAutoOpenSystem()` wired through sector demo and editor Preview3D runtime updates using the current freefly/player pose.
- Auto-open policy: Doors with `autoOpen == true` set `targetOpenFraction` to `1.0` while the player is within horizontal `autoOpenDistance` of the current door transform, and set it back to `0.0` when the player leaves range. Non-auto doors are ignored in this pass. Invalid player positions are ignored, and disabled doors are unaffected.
- Runtime behavior: Auto-open changes ECS `SectorDoorMotion::targetOpenFraction` only. Authored `initialOpenFraction`, saved door data, and authored interaction settings are not mutated. Motion advancement and derived transform/collider/blocker updates continue through the existing Phase 4 systems.
- Behavior notes: Source code changed. No `F` interact key, keybinding UI, general interaction framework, collision response, dynamic collider collection, portal visibility traversal, topology mutation/cache invalidation, serialization schema/default behavior, asset loading policy, or lightmap source-hash behavior changed. Collision/sector lookup/physics behavior is unchanged except auto-open doors may move their ECS transform during runtime animation and therefore update existing object-sector/light-probe sampling as in Phase 4B. No manual GUI verification was performed.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_runtime_object_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed; `git diff --check` passed; `git diff --stat` reviewed; `git status --short` reviewed.

### Phase 5B: Add Interact Key Door Toggle

- Status: Completed
- Date: 2026-07-02
- Summary: Added `ToggleTargetedSectorDoorInteractionSystem()` and wired `KEY_F` pressed events in the sector demo and editor Preview3D paths to toggle the targeted non-auto door's ECS `SectorDoorMotion::targetOpenFraction`.
- Targeting behavior: Manual interaction ignores auto-open doors, disabled doors, doors outside `interactionDistance`, doors behind/outside a facing cone, invalid player positions, and invalid horizontal view directions. If multiple valid manual doors are targeted, the nearest door center in horizontal distance is toggled. The toggle opens when the current/target fraction is mostly closed and closes when current or target state is mostly open.
- Input behavior: The path uses key-pressed events rather than held state. In the editor, the `F` interaction runs only in Preview3D after modal checks, so texture/sprite/document modals keep input focus. The sector demo assumes the active freefly 3D mode owns keyboard input. No keybinding UI or general interaction framework was added.
- Behavior notes: Source code changed. Runtime interaction mutates ECS target state only; authored `initialOpenFraction`, authored door interaction settings, serialization schema/default behavior, topology mutation/cache invalidation, asset loading policy, dynamic collision response, portal visibility traversal, and lightmap source-hash behavior are unchanged. Collision/sector lookup/physics behavior is unchanged except manually toggled doors may move their ECS transform during existing runtime animation and object-sector/light-probe updates as in Phase 4B. Auto-open policy remains open while near and close when leaving range. No manual GUI verification was performed.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_runtime_object_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `cmake --build cmake-build-debug -j2` passed; `git diff --check` passed; `git diff --stat` reviewed; `git status --short` reviewed.

### Phase 6A: Collect Door Dynamic Colliders

- Status: Completed
- Date: 2026-07-02
- Summary: Added `SectorDynamicDoorCollider` snapshots and `CollectSectorDoorDynamicColliders()` to collect enabled, valid ECS door collider OBB footprints and vertical intervals for future movement queries.
- Collection behavior: The helper appends to a caller-owned vector so callers can reserve/reuse storage. It filters disabled doors, disabled colliders, invalid/non-finite shapes, non-positive extents, and invalid vertical intervals. Fully open doors are still collected as physical moved slabs; Phase 6B will decide movement blocking from the current OBB plus vertical interval.
- Behavior notes: Source code changed. Authored data, serialization schema/default behavior, topology mutation/cache invalidation, asset loading policy, static collision resolution, player movement, sector lookup, portal visibility traversal, renderer state, and lightmap source-hash behavior are unchanged. No manual GUI verification was performed.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_runtime_object_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_collision|sector_runtime_object"` passed; `git diff --check` passed; `git diff --stat` reviewed; `git status --short` reviewed.

### Phase 6B: Resolve Player Movement Against Door Colliders

- Status: Completed
- Date: 2026-07-02
- Summary: Added `ResolveSectorDoorDynamicCollidersForPlayerMovement()` and wired editor gameplay preview movement to resolve collected ECS door OBB colliders after static topology collision.
- Collision integration point: Static `SectorCollisionWorld::ResolveMovement()` still runs first. The dynamic door pass then resolves the player cylinder/circle against `SectorRuntimeObjectState::dynamicDoorColliders`, which is cleared and recollected from ECS doors each Preview3D update after runtime door state updates.
- Dynamic collider shape: Door blockers use the current collected 2D OBB footprint (`center`, normalized `tangent`/`normal`, `halfExtents`) plus vertical interval overlap against the player feet/head interval. The pass uses conservative blocking from the current slab shape; open vertical doors allow movement when their moved vertical interval no longer overlaps the player, and horizontally opened doors block only at their current moved footprint.
- Player-inside behavior: If the player starts inside a door slab, the resolver pushes out along a least-penetration OBB axis when possible and reports `hitWall`; invalid/non-finite colliders are ignored safely. If static movement crossed sectors and then hits a dynamic door, the resolver constrains the player back to the starting side and preserves the prior sector ID.
- Behavior notes: Source code changed. Authored data, serialization schema/default behavior, topology mutation/cache invalidation, asset loading policy, portal visibility traversal, renderer state, and lightmap source-hash behavior are unchanged. Collision/sector lookup/physics changed only for editor gameplay preview player movement after static collision; static topology collision remains unchanged. No manual GUI verification was performed.
- Verification: `cmake --build cmake-build-debug -j2 --target sector_runtime_object_tests` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_collision|sector_runtime_object"` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed; `git diff --check` passed; `git diff --stat` reviewed; `git status --short` reviewed.

## Execution Tracking Rules

- Each phase/pass must be independently buildable and testable.
- Each phase/pass final report must state whether source code changed.
- Each implementation phase/pass must update this document before finishing.
- The update should be small and local.
- Do not rewrite unrelated phases when marking progress.
- If behavior is intended to remain unchanged, explicitly state that.
- If a phase/pass changes serialization, generated data, public APIs, runtime behavior, cache invalidation, asset loading, ECS ownership, collision, portal visibility, renderer state, or build/test behavior, clearly say so.
- Do not claim manual GUI verification unless it was actually performed.
- If a phase/pass is too broad, add smaller child passes under that phase and stop without source changes.

## Goal And Desired End State

Implement V1 procedural portal-attached doors.

Desired end state:

- Doors are saved/loaded as `runtimeObjects[]` with `kind: "door"`.
- Doors attach to valid two-sided portal linedefs.
- Doors have width, height, thickness, normal offset, motion type, open distance, speed, initial open fraction, auto-open setting, and a simple texture/material.
- Doors spawn into `EngineContext::world` as ECS runtime objects.
- Doors render as procedural 3D box/slab geometry in the 3D preview.
- Doors are lit by baked object probes and dynamic lights, not surface lightmaps.
- Doors can slide open/closed at runtime.
- Auto-open doors open when the player approaches.
- Non-auto doors can be toggled/opened with the Interact key `F`.
- Closed doors block player collision through dynamic collider support.
- Closed doors block portal visibility traversal through a dynamic portal blocker overlay.
- Partly open doors allow visibility traversal conservatively.
- Editor can place doors on portal segments, select/delete them, and inspect/edit basic fields.
- Door world/debug state remains separate from authored initial state.
- No dynamic sector-height doors, partial sector mesh rebuilds, glTF loading, NPC AI, scripting, or full physics engine are added.

## Audit Dependency

Future runs should consult:

```text
docs/audit/sector_door_system_audit.md
```

Especially:

- Section 3 for portal anchor model, door local basis, variable thickness, and default dimensions.
- Section 5 for authored door data model.
- Section 6 for runtime ECS door model and ownership.
- Section 7 for procedural door rendering and lighting recommendations.
- Section 9 for dynamic collision integration.
- Section 10 for dynamic portal visibility blockers.
- Section 11 for editor tool/inspector recommendations.
- Section 13 for object probe lighting.

This plan adds one product requirement not covered in the audit: V1 door interactivity includes an authored `autoOpen` setting and a minimal `F` interact-key path for non-auto doors.

## Architecture And Ownership Rules

- `EngineContext::world` is the authoritative ECS world for runtime objects.
- `SectorTopologyMap::runtimeObjects` is authored level data.
- ECS components are mutable runtime state.
- Authored door data stores initial/default settings.
- Runtime door state such as current `openFraction`, `targetOpenFraction`, obstruction state, cooldowns, and interaction state belongs in ECS or future save-game state, not in authored level data.
- `SectorRuntimeObjectState` may keep bookkeeping:
  - placed-object ID to ECS entity mapping
  - asset scope
  - diagnostics/counts
  - object probe runtime data
  - sector lookup helpers
  - dynamic blocker collection buffers if project style supports that
- `SectorRuntimeObjectState` must not store shadow copies of mutable door transform or animation state.
- `SectorMeshPreview` may render ECS doors passed to it.
- `SectorMeshPreview` must not:
  - own `engine::World`
  - spawn/despawn runtime object entities
  - reset runtime object lifecycle
  - own door gameplay/interaction state
- Static sector topology, generated sector meshes, draw records, lightmap atlas data, and static portal visibility graph remain non-ECS.
- Door animation must not mutate static sector mesh geometry in V1.
- Door rendering should be opaque/cutout with depth write. No transparent/glass pass in this plan.
- Lightmap source hash should remain unchanged for V1 doors.

## Proposed Door Data Model

Use project style and adjust names as needed, but the intended model is:

```cpp
enum class SectorDoorMotionType {
    SlideVertical,
    SlideLeft,
    SlideRight
};

struct SectorDoorAnchor {
    int lineDefId = 0;
    int frontSectorId = 0;
    int backSectorId = 0;
    int frontSideDefId = 0;
    int backSideDefId = 0;
    SectorCoord endpointA = {};
    SectorCoord endpointB = {};
};

struct SectorPlacedDoor {
    SectorDoorAnchor anchor;
    float width = 0.0f;
    float height = 0.0f;
    float thickness = 0.25f;
    float normalOffset = 0.0f;
    SectorDoorMotionType motion = SectorDoorMotionType::SlideVertical;
    float openDistance = 0.0f;
    float speed = 1.5f;
    float initialOpenFraction = 0.0f;
    bool autoOpen = false;
    float interactionDistance = 1.5f;
    float autoOpenDistance = 2.0f;
    std::string textureId;
};

struct SectorPlacedRuntimeObject {
    int id = 0;
    std::string kind; // "billboard" or "door"
    Vector3 position = {};
    float yawRadians = 0.0f;
    SectorPlacedBillboard billboard;
    SectorPlacedDoor door;
};
```

Recommended JSON shape:

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
    "autoOpen": false,
    "interactionDistance": 1.5,
    "autoOpenDistance": 2.0,
    "textureId": "industrial_door"
  }
}
```

Serialization/default policy:

- New authored doors use `kind: "door"`.
- Use stable positive runtime object IDs.
- Store anchor IDs and exact endpoint coordinates for diagnostics.
- Omit defaults where consistent with existing project style.
- If zero-as-derived dimensions are awkward for validation, store explicit default width/height after anchor placement in the editor instead.
- Prefer `speed` in world units per second.
- `initialOpenFraction` is authored initial state only.
- `autoOpen` is authored interaction behavior.
- `interactionDistance` and `autoOpenDistance` may use defaults and be omitted on save if default.
- `textureId` can be empty until Phase 9, but runtime must not crash on missing texture.

Validation:

- IDs must be positive and unique.
- `kind` must be known.
- Door anchor field types must be valid.
- Motion string must be known.
- Width/height/openDistance must be finite and non-negative if zero-derived is accepted; positive if explicit-only.
- Thickness must be finite and positive.
- Normal offset must be finite.
- Speed must be finite and non-negative.
- `initialOpenFraction` must be finite and clamped/rejected outside `0..1`.
- `interactionDistance` and `autoOpenDistance` must be finite and positive when present.
- Texture ID, if present, must be a string. Missing map texture should be a runtime/editor diagnostic, not a crash.

## Interaction Model

V1 interaction is intentionally small and door-specific.

Authored fields:

- `autoOpen`
- `autoOpenDistance`
- `interactionDistance`

Runtime ECS state:

- current `openFraction`
- target open fraction
- opening/closing state
- any simple cooldown/last interaction time if needed to prevent double toggles

Auto-open behavior:

- If `autoOpen == true`, door target opens when the player is within `autoOpenDistance`.
- V1 may either:
  - close when the player leaves the range, or
  - stay open once opened.
- Pick one policy in the implementation pass and document it. Recommendation: auto-open while near, close when no longer near, unless collision obstruction requires staying open.
- Use distance to door anchor/slab center or nearest point on door footprint; pick the simplest stable approach for V1.

Manual interaction behavior:

- If `autoOpen == false`, pressing `F` should toggle/open the targeted nearby door.
- `F` is not currently a general engine action. Add the smallest door-focused interact input path needed for V1.
- Prefer naming it as a future-friendly interaction action internally if the input system already has action mapping; otherwise key check `KEY_F` in the sector preview/player update path is acceptable for V1.
- Do not build a full interaction framework.
- Targeting can be:
  - nearest door in front of the player within `interactionDistance`, using view direction dot product; or
  - nearest door whose footprint/anchor is within distance.
- Recommendation: require both distance and facing cone/dot product so random doors behind the player do not toggle.
- Interaction should affect ECS `targetOpenFraction`, not authored `initialOpenFraction`.
- The editor inspector can expose `Auto Open`, `Auto Open Distance`, `Interaction Distance`, and a debug/open toggle later.

## Aspect Of V1 Door Types

Implement a flexible enum, but keep V1 small:

- Required V1: `slide_vertical`.
- Optional if small after vertical works: `slide_left` and `slide_right`.
- Defer:
  - split doors
  - hinged/swing doors
  - rotating colliders
  - glass/transparent doors
  - glTF/model doors

The schema may accept all three slide values from the start if implementation remains simple.

## Proposed Phases

### Phase 1: Authored Door Data And Serialization

Goal:

Add generic authored door data and JSON round-trip support without runtime spawn/rendering/editor behavior.

Why it helps:

This establishes the schema and validation review point before runtime, collision, visibility, or editor work.

Files/functions likely touched:

- `sources/sector_demo/SectorTopologyMap.h`
- `sources/sector_demo/SectorTopologySerialization.cpp`
- `tests/SectorTopologySerializationTests.cpp`
- `tests/SectorRuntimeObjectTests.cpp`

Exact behavior that must remain unchanged:

- Existing billboard runtime objects still load/save unchanged.
- Maps without runtime objects still load as empty.
- Renderer behavior unchanged.
- Runtime ECS behavior unchanged.
- Collision/visibility unchanged.
- Lightmap source hash unchanged.

Risks/goblins:

- Breaking billboard serialization defaults.
- Over-validating topology anchors before the topology can be checked.
- Confusing authored initial state with runtime open state.
- Adding interaction behavior too early.
- Using world floats for exact anchor endpoints instead of `SectorCoord`.

Non-goals:

- No runtime spawn.
- No rendering.
- No editor tool.
- No collision.
- No portal visibility blockers.
- No Interact key behavior yet.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run serialization/runtime object tests.

Final report expectations:

- State data types added.
- State JSON fields and defaults.
- State validation behavior.
- State how `autoOpen`, `interactionDistance`, and `autoOpenDistance` are serialized.
- State whether renderer/runtime behavior changed.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 1 `Completed` only after Phase 1A and Phase 1B are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 1A: Add Door Payload And JSON Round Trip

Goal:

Add `kind: "door"` and nested `door` payload round-trip serialization.

Implementation guidance:

- Add `SectorPlacedDoor` / `SectorDoorAnchor` or project-style equivalents.
- Add `kind: "door"` support to runtime object read/write.
- Add `door.anchor`, dimensions, motion, speed, initial open fraction, interaction fields, and texture ID fields.
- Do not resolve anchors against topology in this pass unless the existing serializer already has the right context.
- Do not spawn doors into ECS.
- Keep `autoOpen`, `interactionDistance`, and `autoOpenDistance` as authored settings only.

Suggested tests:

- Door JSON round-trips.
- Default values omit/restore according to project style.
- Motion strings read/write correctly.
- Interaction fields read/write correctly.
- Billboard JSON round-trip still passes.

#### Phase 1B: Add Door Defaults Validation And Fixture Coverage

Goal:

Tighten validation/defaults and add focused fixture coverage.

Implementation guidance:

- Validate finite/positive dimensions according to chosen zero-derived or explicit-only policy.
- Validate `initialOpenFraction`.
- Validate positive interaction distances.
- Validate anchor field types and positive IDs.
- Add invalid JSON tests.
- Add mixed billboard + door runtime object tests.

Suggested tests:

- Invalid motion rejected.
- Invalid dimensions rejected.
- Invalid interaction distances rejected.
- Invalid initial open fraction rejected.
- Duplicate runtime object IDs still rejected.
- Existing billboard fixtures unaffected.

### Phase 2: Portal Anchor Resolution And Diagnostics

Goal:

Resolve authored door anchors to current topology portal data.

Why it helps:

Door spawn, render, collision, and visibility all need a reliable portal basis.

Files/functions likely touched:

- `SectorTopologyMap.*`
- `SectorPortalVisibility.*`
- `SectorRuntimeObjects.*`
- editor topology render cache if diagnostics are visualized

Exact behavior that must remain unchanged:

- Static portal graph generation unchanged.
- No runtime door spawn yet unless needed for diagnostics tests.
- No collision/visibility behavior change yet.
- No mesh rebuild behavior.

Risks/goblins:

- Wrong normal direction.
- Sector-pair mismatch after topology edits.
- One-sided walls accidentally accepted.
- Zero-length or zero-height portal openings.
- Exact coordinate conversion mistakes.

Non-goals:

- No door rendering.
- No door animation.
- No dynamic portal blockers yet.
- No editor placement tool yet.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run topology/runtime object tests.

Final report expectations:

- State anchor resolution helper names.
- State basis/origin/opening derivation.
- State invalid-anchor behavior.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 2 `Completed` only after Phase 2A and Phase 2B are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 2A: Resolve Door Anchors To Portal Basis

Goal:

Add a helper that resolves authored door anchors into portal endpoints, tangent, normal, opening bottom/top, and default dimensions.

Implementation guidance:

- Resolve by `lineDefId`.
- Verify two valid sidedefs.
- Verify stored front/back sectors still match.
- Compute endpoints from current linedef vertices.
- Compute tangent, normal, openBottom/openTop, width, and height.
- Return structured success/failure diagnostics.
- Prefer fail-safe invalid result over guessing.

Suggested tests:

- Valid two-sided portal resolves.
- One-sided wall rejected.
- Sector mismatch rejected.
- Zero-height opening rejected.
- Normal direction stable/deterministic.

#### Phase 2B: Surface Door Anchor Diagnostics

Goal:

Make anchor validity available to runtime/editor diagnostics without changing behavior broadly.

Implementation guidance:

- Add diagnostic strings/counts for invalid door anchors.
- Invalid anchors should not crash load/update/render.
- If runtime spawn is not implemented yet, tests can exercise the helper directly.
- Editor visual warning can be deferred to Phase 8 if too broad.

Suggested tests:

- Invalid anchor produces diagnostic.
- Missing linedef produces diagnostic.
- Diagnostics do not affect valid billboard runtime object behavior.

### Phase 3: Runtime ECS Door Spawn And Closed Slab Rendering

Goal:

Spawn valid authored doors as ECS objects and render closed procedural slabs.

Why it helps:

This proves the main architecture before animation/collision/visibility.

Files/functions likely touched:

- `SectorRuntimeObjects.h/.cpp`
- `SectorMeshPreview.cpp`
- renderer shader helpers if needed
- runtime object tests

Exact behavior that must remain unchanged:

- `EngineContext::world` remains authoritative.
- `SectorMeshPreview` remains renderer-only.
- Runtime reset/refresh lifecycle remains explicit.
- Billboards still render unchanged.
- Static sector rendering unchanged.
- Collision and visibility unchanged.

Risks/goblins:

- Per-frame mesh allocation.
- Shader state leaking into sector/billboard draw.
- Missing texture fallback crashing.
- Door rendering trying to use lightmap UVs.
- Mutable door state mirrored outside ECS.
- Door entity duplication on refresh.

Non-goals:

- No animation yet.
- No collision yet.
- No visibility blocking yet.
- No Interact key yet.
- No texture picker UI yet.

Suggested checks:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object
git diff --check
git diff --stat
git status --short
```

Final report expectations:

- State ECS components added.
- State spawn lifecycle and reset behavior.
- State renderer path.
- State lighting approach.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 3 `Completed` only after Phase 3A and Phase 3B are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 3A: Spawn Door ECS Components

Goal:

Spawn valid door objects into ECS without drawing if rendering is too broad.

Implementation guidance:

- Add narrow door ECS components.
- Reuse `SectorObjectTransform`, `SectorObject`, and `SectorObjectLighting` where appropriate.
- Add door-specific anchor/motion/render/collider/blocker components as needed.
- Initialize `openFraction` and `targetOpenFraction` from `initialOpenFraction`.
- Initial target should probably equal initial fraction.
- Do not implement auto-open or `F` interaction yet.
- Do not mirror mutable door state in `SectorRuntimeObjectState`.

Suggested tests:

- Valid door spawns one entity.
- Invalid anchor skips or creates disabled diagnostic state according to chosen policy.
- Reset/refresh does not duplicate doors.
- Door entity has expected components.

#### Phase 3B: Render Closed Procedural Door Slabs

Goal:

Draw closed procedural door slabs in 3D preview.

Implementation guidance:

- Add `DrawRuntimeDoors()` or equivalent renderer-only path.
- Use current door transform, dimensions, thickness, and origin/basis.
- Start with one texture/material or fallback color if material phase is deferred.
- Use object probe lighting if practical in this pass; otherwise clearly stage it and do not fake lightmap use.
- Add dynamic light receiving if small; otherwise defer to a clearly named follow-up.
- Preserve depth test/write and render state for sector/billboard passes.
- No transparent blending.

Suggested tests/manual smoke:

- Runtime door draw path handles missing texture.
- No crash with one spawned door.
- Existing billboard draw tests still pass.
- Manual GUI smoke remains for user.

### Phase 4: Door Motion And Runtime State

Goal:

Animate door open/close state and derive current slab transform/collider/blocker state.

Why it helps:

Collision, visibility, and interaction all need a single runtime motion source.

Files/functions likely touched:

- `SectorRuntimeObjects.h/.cpp`
- runtime object tests
- possibly renderer draw transform helper

Exact behavior that must remain unchanged:

- Authored `initialOpenFraction` does not change during runtime animation.
- Save/load schema unchanged.
- No collision/visibility behavior change until later phases.
- Renderer continues to draw current slab transform.

Risks/goblins:

- Fraction-per-second vs world-units-per-second confusion.
- Wrong tangent sign for left/right.
- Door animation writing back into authored data.
- Closing into player behavior implemented too early.

Non-goals:

- No auto-open yet.
- No `F` interaction yet.
- No collision response yet.
- No portal traversal blocking yet.

Suggested checks:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object
git diff --check
git diff --stat
git status --short
```

Final report expectations:

- State motion semantics.
- State implemented motion types.
- State how transform is derived.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 4 `Completed` only after Phase 4A and Phase 4B are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 4A: Add Door Motion Update

Goal:

Advance `openFraction` toward `targetOpenFraction`.

Implementation guidance:

- Use `speed` as world units per second if possible, converted through `openDistance`.
- Clamp `openFraction` to `0..1`.
- Implement `slide_vertical` as required.
- Implement `slide_left` / `slide_right` if they are small and already in schema; otherwise leave them parsed but unsupported with diagnostics.
- Keep deterministic update behavior testable with fixed dt.

Suggested tests:

- closed-to-open advances deterministically.
- open-to-closed advances deterministically.
- clamp at 0 and 1.
- zero speed behavior is safe.
- initialOpenFraction initializes current state.

#### Phase 4B: Add Door Transform Collider And Blocker State

Goal:

Derive current slab transform, OBB footprint, vertical interval, and closed/open blocker state from motion.

Implementation guidance:

- Use anchor basis + dimensions + motion offset.
- Update `SectorDoorCollider` shape from current transform.
- Update `SectorDoorPortalBlocker` closed/open state from `openFraction`.
- Do not wire collision or portal traversal yet.
- Keep this data in ECS components.

Suggested tests:

- transform at openFraction 0, 0.5, 1.
- vertical slide changes Y.
- side slide changes tangent position.
- closed blocker state true at epsilon threshold.
- open blocker state false beyond threshold.

### Phase 5: Door Interaction

Goal:

Add basic authored auto-open behavior and manual `F` interaction for doors.

Why it helps:

Doors need a minimal gameplay interaction model before editor workflows are useful.

Files/functions likely touched:

- `SectorRuntimeObjects.h/.cpp`
- sector preview/player update path
- input handling code near 3D preview/FPS controller
- runtime object tests if input helpers can be isolated

Exact behavior that must remain unchanged:

- No full interaction framework required.
- No scripting/AI/combat.
- Authored data remains initial/default settings.
- Runtime interaction changes ECS `targetOpenFraction`, not authored data.
- Existing input behavior for editor UI/text fields remains respected.
- UI focus should prevent text-entry conflicts if `F` is ever used near UI text editing.

Risks/goblins:

- Toggling doors behind the player.
- Interact key firing while UI is focused/unlocked.
- Repeated toggles from key repeat.
- Auto-open fighting manual target state.
- Closing into player before collision-obstruction behavior exists.

Non-goals:

- No keybinding UI.
- No generalized use/action framework.
- No sound effects.
- No locks/keys/scripts.
- No network/savegame persistence.

Suggested checks:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object
git diff --check
git diff --stat
git status --short
```

Final report expectations:

- State auto-open policy.
- State `F` interaction targeting rule.
- State input focus/cursor-lock assumptions.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 5 `Completed` only after Phase 5A and Phase 5B are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 5A: Add Door Auto Open Runtime Behavior

Goal:

Auto-open doors when the player approaches if authored `autoOpen` is true.

Implementation guidance:

- Use ECS door state and player/camera position.
- Use `autoOpenDistance`.
- Recommendation: open while player is within range; close when player leaves range, unless obstruction handling later says otherwise.
- Do not mutate authored `initialOpenFraction`.
- If collision is not implemented yet, keep behavior simple and documented.

Suggested tests:

- auto-open door target becomes open when player enters range.
- auto-open door target becomes closed when player leaves range if that policy is chosen.
- non-auto door unaffected.
- missing/invalid door anchor safe.

#### Phase 5B: Add Interact Key Door Toggle

Goal:

Add minimal `F` key interaction for non-auto doors.

Implementation guidance:

- Add the smallest input path needed.
- Use key press, not repeat.
- Prefer cursor-locked 3D gameplay mode only. Avoid firing while cursor is unlocked for editor UI interaction.
- Target nearest door in front of the player within `interactionDistance`, or use a similarly simple and documented rule.
- Toggle target open/closed.
- Auto-open doors may ignore manual interact in V1 or allow it; choose and document. Recommendation: manual interact targets non-auto doors only.

Suggested tests:

- pressing F toggles targeted door.
- pressing F does not toggle out-of-range door.
- pressing F does not toggle door behind player if facing test is implemented.
- key repeat does not double-toggle.

### Phase 6: Dynamic Collision Blockers

Goal:

Closed or insufficiently open doors block player movement.

Why it helps:

A door that only renders and animates is not yet a real door.

Files/functions likely touched:

- `SectorCollisionWorld.*`
- `SectorFpsController.*` or current movement/controller code
- `SectorRuntimeObjects.*`
- collision/movement tests

Exact behavior that must remain unchanged:

- Static sector collision still runs first.
- Topology collision data remains non-ECS.
- No full 3D physics engine.
- No mesh-triangle collision.
- Door collider state comes from ECS.

Risks/goblins:

- Jitter against static portal walls.
- Player starts inside door slab.
- Door closes into player.
- Current-sector update crosses a dynamically blocked portal.
- Dynamic collision collection lifetime/order bugs.

Non-goals:

- No NPC collision.
- No pushable objects.
- No door pushing player.
- No obstruction sounds/effects.

Suggested checks:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure -R "sector_collision|sector_runtime_object"
git diff --check
git diff --stat
git status --short
```

Final report expectations:

- State collision integration point.
- State dynamic collider shape.
- State door-open threshold for collision.
- State player-inside behavior.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 6 `Completed` only after Phase 6A and Phase 6B are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 6A: Collect Door Dynamic Colliders

Goal:

Collect enabled ECS door collider shapes for movement/collision queries.

Implementation guidance:

- Add dynamic collider collection helper.
- Use reserved vectors/stack structures where practical.
- Include OBB footprint and vertical interval.
- Include placed object/entity ID for diagnostics if useful.
- Collection should not mutate authored data.
- Future dynamic blockers can reuse this path.

Suggested tests:

- closed door collider collected.
- fully open door collider excluded or included according to chosen threshold.
- invalid/disabled door excluded.
- vertical interval data correct.

#### Phase 6B: Resolve Player Movement Against Door Colliders

Goal:

Resolve player cylinder/circle movement against collected dynamic door OBBs after static collision.

Implementation guidance:

- Preserve existing static collision behavior.
- Apply dynamic collision after static movement.
- Use conservative blocking.
- If player starts inside door, resolve least-penetration if small/simple; otherwise leave unchanged and report blocked safely.
- Keep vertical interval overlap check.

Suggested tests:

- closed door blocks portal crossing.
- open door allows crossing.
- no vertical overlap means no block.
- starts-inside case safe/no crash.

### Phase 7: Dynamic Portal Visibility Blockers

Goal:

Closed doors block portal traversal in visibility culling.

Why it helps:

Closed doors should behave visually like boundaries for sector visibility, without rebuilding static portal graph.

Files/functions likely touched:

- `SectorPortalVisibility.h/.cpp`
- `SectorMeshPreview.cpp`
- `SectorRuntimeObjects.*`
- portal visibility tests

Exact behavior that must remain unchanged:

- Static portal graph build unchanged.
- Dynamic blockers are an overlay query.
- False positives are acceptable.
- False negatives are bugs.
- Hidden-sector draw/light/bloom filtering behavior otherwise unchanged.

Risks/goblins:

- Directed edge key mismatch.
- Stale blocker data after ECS update.
- Blocking traversal too aggressively while partly open.
- Forgetting fallback draw-all semantics.
- Dynamic blocker query accidentally changing static graph tests.

Non-goals:

- No partial aperture clipping.
- No portal mesh rebuild.
- No door 3D picking integration unless needed.
- No lightmap changes.

Suggested checks:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure -R "sector_portal_visibility|sector_runtime_object"
git diff --check
git diff --stat
git status --short
```

Final report expectations:

- State blocker key.
- State traversal hook.
- State openFraction threshold.
- State fallback behavior.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 7 `Completed` only after Phase 7A and Phase 7B are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 7A: Add Dynamic Portal Blocker Query

Goal:

Add an optional dynamic blocker lookup to portal visibility traversal APIs.

Implementation guidance:

- Key blockers by `lineDefId` plus from/to sector or sidedef ID.
- Do not rebuild static graph.
- Traversal should check:
  - static `edge.open`
  - dynamic blocker says blocked?
  - then angular/window tests.
- If no dynamic blocker data is supplied, behavior must match current tests exactly.
- Preserve fallback draw-all behavior.

Suggested tests:

- no blocker preserves current traversal.
- blocker on directed edge prevents traversal.
- blocker not matching edge does not prevent traversal.
- partly open/unblocked allows traversal.

#### Phase 7B: Wire Door Blockers Into Preview Visibility

Goal:

Build dynamic blocker data from ECS door state and pass it to preview visibility.

Implementation guidance:

- Ensure ECS door motion/blocker update happens before visibility computation.
- Closed threshold from Phase 4B.
- If runtime object world unavailable, no blockers.
- If visibility falls back to draw-all, do not over-filter.
- Debug text may mention dynamic door blockers if small.

Suggested tests:

- closed door blocks sector visibility in preview traversal helper.
- open door allows visibility.
- static graph edge count unchanged.

### Phase 8: Door Editor Tool And Inspector

Goal:

Add editor authoring UI for placing and editing doors.

Why it helps:

The runtime system becomes level content instead of test fixtures.

Files/functions likely touched:

- `SectorEditorTypes.h`
- `SectorEditor.cpp`
- `SectorEditorHelpers.cpp`
- `SectorEditorTopologyRenderCache.*`
- UI layout tests if available
- runtime object tests

Exact behavior that must remain unchanged:

- Existing sector/line/vertex/light/billboard tools continue working.
- UI text focus behavior remains respected.
- Object edits mutate authored data, mark dirty, invalidate cache, and refresh runtime ECS.
- No broad editor refactor.

Risks/goblins:

- Selecting wrong portal side.
- Long inspector labels/warnings overflowing.
- Missed cache invalidation.
- Door footprint drawn inconsistent with runtime collider.
- Editing runtime state instead of authored defaults.

Non-goals:

- No 3D transform gizmo.
- No swing doors.
- No full material editor.
- No keybinding UI.
- No GUI automation.

Suggested checks:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_authoring_graph|sector_topology_serialization"
git diff --check
git diff --stat
git status --short
```

Final report expectations:

- State tool label/help.
- State placement behavior.
- State inspector fields.
- State dirty/cache/runtime refresh behavior.
- State manual GUI smoke status.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 8 `Completed` only after Phase 8A, 8B, and 8C are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 8A: Add Door Placement Tool And 2D Footprint

Goal:

Add a `Door` tool that places a door on a valid two-sided portal.

Implementation guidance:

- Click/select a two-sided portal segment.
- Create `kind: "door"` object with anchor IDs and endpoint diagnostics.
- Default dimensions from resolved portal opening.
- Draw 2D door marker/footprint including thickness.
- Select newly placed door.
- Mark document dirty and refresh runtime objects.
- Invalid placement should show status/warning, not crash.

Suggested tests/manual smoke:

- placement on portal creates door.
- placement on one-sided wall rejected.
- marker/footprint appears.
- save/reload preserves door.
- manual smoke remains user-performed unless actually done.

#### Phase 8B: Add Door Inspector Core Fields

Goal:

Expose core authored door fields in the inspector.

Inspector fields:

```text
Object ID
Type: Door
Anchor status
Line ID / sector pair
Width
Height
Thickness
Normal Offset
Motion
Open Distance
Speed
Initial Open Fraction or Starts Open
Texture status if material exists
Delete
```

Implementation guidance:

- Use existing UI widgets.
- Use small/secondary editor font for key:value/status/descriptive text.
- Use word-wrap for warnings/diagnostics.
- Do not add long overflowing labels.
- Mutations use `MutateSelectedRuntimeObject()`.
- Mark dirty, invalidate cache, refresh runtime objects.
- Motion can use `Option()` dropdown.

Suggested tests/manual smoke:

- edits persist save/load.
- dimensions update runtime slab/footprint.
- invalid anchors show warning.
- delete removes authored object/runtime entity.

#### Phase 8C: Add Door Inspector Interaction Controls

Goal:

Expose authored door interaction fields.

Inspector fields:

```text
Auto Open checkbox
Auto Open Distance
Interaction Distance
Debug Open / Close toggle if runtime preview support exists
```

Implementation guidance:

- `Auto Open` edits authored `door.autoOpen`.
- `Auto Open Distance` edits authored default distance.
- `Interaction Distance` edits authored manual interact distance.
- Debug open/close toggle should mutate runtime ECS target state only if it is clearly labeled as preview/debug runtime state. It must not silently rewrite `initialOpenFraction`.
- If adding debug toggle is too broad, document it as deferred.

Suggested tests/manual smoke:

- autoOpen persists.
- distances persist.
- toggling autoOpen affects runtime behavior after refresh.
- debug toggle, if implemented, changes runtime state only.

### Phase 9: Door Material V1

Goal:

Add minimal texture/material support for procedural door slabs.

Why it helps:

Plain debug-colored slabs prove behavior, but textured slabs make the feature useful for level work.

Files/functions likely touched:

- `SectorTopologyMap.h`
- `SectorTopologySerialization.cpp`
- `SectorEditorTextureModals.*`
- `SectorEditorTextureActions.*`
- `SectorMeshPreview.cpp`
- material/texture tests

Exact behavior that must remain unchanged:

- Existing map texture dictionary behavior unchanged.
- Door texture ID is authored data, not an AssetManager handle.
- Missing texture falls back safely.
- No per-face UV editor yet unless explicitly scoped.
- No transparent pass.

Risks/goblins:

- Texture ID not present in map dictionary.
- Sampler/filter mismatch.
- Shader state leaks.
- Saving raw texture handles.
- UVs stretched badly but functionally correct.

Non-goals:

- No six-face material UI.
- No per-face UV inspector.
- No glTF/material import.
- No texture animation.

Suggested checks:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_topology_serialization"
git diff --check
git diff --stat
git status --short
```

Final report expectations:

- State material fields.
- State picker behavior.
- State UV generation.
- State missing-texture behavior.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 9 `Completed` only after Phase 9A and Phase 9B are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 9A: Add Single Texture Door Material

Goal:

Use one map texture ID for all door slab faces.

Implementation guidance:

- Use `door.textureId`.
- Resolve through map texture dictionary / AssetManager patterns.
- Generate procedural UVs from dimensions.
- Use fallback material/color for missing texture.
- Keep sampler/depth state safe.

Suggested tests:

- texture ID round-trips.
- missing texture safe.
- renderer does not crash without texture.

#### Phase 9B: Add Door Texture Picker Integration

Goal:

Allow selecting door texture from existing map textures.

Implementation guidance:

- Reuse existing texture picker patterns.
- Do not build a new modal if existing picker can handle a door texture target.
- Apply texture through authored object mutation path.
- No per-face picker yet.

Suggested tests/manual smoke:

- picker applies texture ID.
- save/reload persists.
- missing texture shows warning.

### Phase 10: Tests Documentation And Cleanup

Goal:

Strengthen coverage, update docs, and close V1.

Why it helps:

Door system touches many subsystems and will become a foundation for future actors/objects.

Files/functions likely touched:

- tests
- `docs/sector_editor.md`
- this plan document
- maybe audit reference updates

Exact behavior that must remain unchanged:

- No dynamic sector-height doors.
- No glTF/model loading.
- No NPC/combat/scripting.
- No broad ECS conversion.
- No transparent pass.
- No broad UI refactor.

Risks/goblins:

- Tests proving only data and not behavior.
- Docs claiming manual behavior not actually tested.
- Door debug helpers becoming permanent weird paths.
- Scope creep into swing doors/material editor.

Non-goals:

- No unrelated cleanup crusade.
- No `SectorEditor.cpp` extraction unless explicitly scoped.
- No advanced door art/material pass.

Suggested checks:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Final report expectations:

- State docs updated.
- State tests added/updated.
- State known deferred work.
- State manual smoke checklist.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 10 `Completed` only after Phase 10A and Phase 10B are complete.
- If all phases are complete, ensure all parent phases are `Completed`.
- Leave final completion note.

#### Phase 10A: Strengthen Door Tests And Diagnostics

Goal:

Add or strengthen focused door tests and diagnostics.

Test areas:

- serialization/defaults
- anchor resolution
- runtime spawn
- motion update
- interaction targeting if testable
- collision blocker collection/resolution
- portal visibility blocker traversal
- editor mutation helpers if testable
- material texture ID round-trip
- invalid-anchor diagnostics

#### Phase 10B: Update Documentation And Close Plan

Goal:

Document V1 doors and close the plan.

Docs should mention:

- Door tool
- portal-attached procedural slabs
- variable thickness
- motion types implemented
- auto-open behavior
- `F` interact behavior
- collision behavior
- visibility behavior
- material limitations
- deferred swing doors, frames, glTF, per-face UVs, dynamic sector-height doors
- small/secondary font and wrapped inspector diagnostics if UI docs mention patterns

Suggested manual smoke checklist for user:

```text
- Place a door on a valid two-sided portal.
- Confirm one-sided wall placement is rejected.
- Enter 3D preview and confirm the closed slab renders with thickness.
- Toggle/open the door and confirm it slides.
- Test auto-open near/far behavior.
- Test F interact on non-auto door.
- Confirm closed door blocks player movement.
- Confirm open door allows passage.
- Confirm closed door blocks visibility traversal enough to hide sectors behind it.
- Confirm save/reload preserves door fields.
- Confirm texture assignment persists if Phase 9 is implemented.
- Confirm invalid anchor diagnostics after topology edits.
```

## Deferred Decisions For Later Plans

These are intentionally out of scope:

- Dynamic sector-height doors.
- Partial sector mesh rebuilds.
- Hinged/swing doors.
- Split doors.
- Door frames/trim authoring.
- Full six-face material assignment.
- Per-face UV editor.
- glTF/model door loading.
- Transparent/glass doors.
- Door sound effects.
- Locks/keys/scripts.
- General interaction framework.
- Save-game runtime door state.
- NPC/pathfinding integration.
- Door shadow casting.
- Door influence on baked lighting/lightmap source hash.
- Advanced obstruction handling/pushing the player.
- Broad editor UI/layout refactor.

## Final Completion Criteria

This plan is complete when:

- `kind: "door"` authored runtime objects save/load.
- Door anchors resolve to valid two-sided portal segments.
- Door slabs spawn into ECS.
- Door slabs render as procedural 3D objects with thickness.
- Door motion works for at least `slide_vertical`.
- Auto-open behavior works.
- `F` interact behavior works for non-auto doors.
- Closed doors block player movement.
- Closed doors dynamically block portal visibility traversal.
- Editor can place/select/delete/inspect doors.
- V1 material/texture support exists or is explicitly deferred with debug/fallback rendering accepted.
- Docs/tests are updated.
- `SectorMeshPreview` remains renderer-only.
- Static sector meshes are not rebuilt for door animation.
- No dynamic sector-height/glTF/NPC/scripting/swing-door scope leaked into V1.
