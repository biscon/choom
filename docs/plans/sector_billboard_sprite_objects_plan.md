# Sector Billboard Sprite Objects Plan

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

```plan-state-json id="sector-billboard-sprites"
{
  "plan_id": "sector_billboard_sprite_objects_plan",
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
      "title": "ECS Runtime Object Foundation",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_01a",
      "title": "Add Sector Runtime Object Components And World Integration",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_01b",
      "title": "Add Current Sector And Baked Probe Lighting Systems",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_02",
      "title": "Aseprite Billboard Sprite Components",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02a",
      "title": "Add Billboard Sprite Animation Component And Asset Requests",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02b",
      "title": "Resolve Goblin Test Animation Clips",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "3D Billboard Rendering And Lighting",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_03a",
      "title": "Render Camera-Facing Billboard Quads In Sector 3D Preview",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b",
      "title": "Apply Baked Probe Lighting And Basic Dynamic Light Contribution",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_04",
      "title": "Directional Billboard Animation Selection",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_04a",
      "title": "Select Front Back Left Right Clips By Camera Relative Facing",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_05",
      "title": "Temporary Goblin Test Spawn",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_05a",
      "title": "Add Removable Goblin Debug Spawn Path",
      "type": "pass",
      "parent": "phase_05",
      "status": "Completed"
    },
    {
      "id": "phase_06",
      "title": "Polish Tests Documentation And Completion",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_06a",
      "title": "Document ECS Object Split And Close Plan",
      "type": "pass",
      "parent": "phase_06",
      "status": "Completed"
    },
    {
      "id": "phase_08",
      "title": "Dynamic Spotlight Shadow Receiving",
      "type": "phase",
      "status": "Completed"
    }
  ]
}
```

## Current Progress

| Phase / Pass                                                              | Status      | Date | Notes                                                                |
| ------------------------------------------------------------------------- | ----------- | ---- | -------------------------------------------------------------------- |
| Phase 1: ECS Runtime Object Foundation                                    | Completed   | 2026-07-01 | Phase 1A and Phase 1B complete.                                      |
| Phase 1A: Add Sector Runtime Object Components And World Integration      | Completed   | 2026-07-01 | Added core sector runtime object ECS components, preview-owned object world, and ECS lifecycle test. |
| Phase 1B: Add Current Sector And Baked Probe Lighting Systems             | Completed   | 2026-07-01 | Added current-sector and baked object lighting ECS systems with tests. |
| Phase 2: Aseprite Billboard Sprite Components                             | Completed   | 2026-07-01 | Phase 2A and Phase 2B complete.                                      |
| Phase 2A: Add Billboard Sprite Animation Component And Asset Requests     | Completed   | 2026-07-01 | Added billboard sprite/animator components and setup-time Aseprite animation request helper. |
| Phase 2B: Resolve Goblin Test Animation Clips                             | Completed   | 2026-07-01 | Added prototype Front/Back/Left/Right clip-index resolution with fallback tests. |
| Phase 3: 3D Billboard Rendering And Lighting                              | Completed   | 2026-07-01 | Phase 3A and Phase 3B complete.                                      |
| Phase 3A: Render Camera-Facing Billboard Quads In Sector 3D Preview       | Completed   | 2026-07-01 | Added first visual billboard rendering in the 3D preview.            |
| Phase 3B: Apply Baked Probe Lighting And Basic Dynamic Light Contribution | Completed   | 2026-07-01 | Billboard tint now uses object probe lighting plus simple selected dynamic light contribution. |
| Phase 4: Directional Billboard Animation Selection                        | Completed   | 2026-07-01 | Phase 4A complete.                                                   |
| Phase 4A: Select Front Back Left Right Clips By Camera Relative Facing    | Completed   | 2026-07-01 | Added 4-direction camera-relative billboard clip selection.          |
| Phase 5: Temporary Goblin Test Spawn                                      | Completed   | 2026-07-01 | Phase 5A complete.                                                   |
| Phase 5A: Add Removable Goblin Debug Spawn Path                           | Completed   | 2026-07-01 | Added F5 temporary goblin debug spawn/despawn path.                  |
| Phase 6: Polish Tests Documentation And Completion                        | Completed   | 2026-07-01 | Phase 6A complete.                                                   |
| Phase 6A: Document ECS Object Split And Close Plan                        | Completed   | 2026-07-01 | Documented the ECS object split and closed the plan.                 |
| Phase 7: Ownership And Cutout Rendering Correction                        | Completed   | 2026-07-01 | Moved sector objects to `EngineContext::world` and converted billboards to cutout rendering. |
| Phase 8: Dynamic Spotlight Shadow Receiving                               | Completed   | 2026-07-01 | Billboard shader now receives selected dynamic spotlight shadow maps; baked probe lighting remains unshadowed. |

## Execution Tracking Rules

* Each pass must leave the project buildable and runnable.
* Each pass final report must state whether source code changed.
* Each implementation pass must update this document before finishing.
* The update should be small and local.
* Do not rewrite unrelated phases when marking progress.
* If behavior is intended to remain unchanged, explicitly state that.
* If a pass changes runtime rendering, ECS components, asset loading, probe lighting, dynamic light sampling, serialization, editor behavior, or build/test behavior, clearly say so.
* Do not claim manual GUI verification unless it was actually performed.
* If a pass is too broad, split it into smaller child passes and stop without source changes.

## Post-Review Correction Notes, 2026-07-01

Phase 7 corrected two prototype mistakes from the initial billboard branch.

* ECS ownership: `EngineContext::world` is the authoritative ECS world for
  sector runtime objects. `SectorMeshPreview` no longer owns or exposes an
  `engine::World`, no longer owns temporary goblin spawn state, and no longer
  runs ECS object systems. Editor/demo runtime object state stores only handles,
  object probes, object sector lookup data, and asset-scope bookkeeping.
* Runtime object cleanup: `ClearSectorRuntimeObjects()` destroys only entities
  marked with `SectorObject`; it does not clear arbitrary ECS entities or run
  from preview renderer rebuilds. New/load level, editor/demo shutdown, and
  explicit runtime reset are the intended cleanup points.
* Runtime object asset scope: the temporary goblin Aseprite animation is
  requested from the higher-level runtime object spawn/setup path through
  `AssetManager::RequestSpriteAnimation()` with point-filtered texture loading.
  The runtime object asset scope is not created/unloaded by
  `SectorMeshPreview`, is not thrashed by F5 despawn, and unloads only during
  explicit runtime object cleanup or high-level document/lifecycle reset.
* Rendering: billboards now draw after static sector meshes have populated
  world depth. They use a cutout shader with alpha cutoff `0.5`, depth testing
  and depth writes, no true alpha blending, no transparent sorting, and explicit
  render-state restoration before overlays/bloom/debug drawing.
* UVs/filtering: manual billboard quads compute atlas UVs from the current
  Aseprite frame source rectangle, preserving source rectangle sign/flip
  behavior. Pixel-art filtering remains controlled by the asset system; the
  cutout shader only samples, discards, and outputs color.

## Dynamic Spotlight Shadow Receiving Notes, 2026-07-01

Billboard dynamic lighting now runs in the cutout shader using per-fragment
world position from the manual quad vertices. Dynamic point and spot light
distance/cone/intensity semantics mirror the sector shader as closely as the
current sprite lighting model allows; billboards still intentionally do not use
a camera-facing `NdotL` term for dynamic direct lighting.

Selected dynamic spotlights with existing shadow slots sample the same runtime
shadow maps, matrices, bias, strength, and softness as sector geometry.
Dynamic point lights and unshadowed dynamic spotlights remain unshadowed. Runtime
shadow maps attenuate only the dynamic spotlight contribution; baked object-probe
lighting is not shadowed. Billboards remain cutout-only, depth-tested, and
depth-writing, with no billboard shadow casting and no transparent render queue.

## Goal And Desired End State

Add the first ECS-backed movable object renderer for the sector engine: animated 3D billboards using Aseprite sprite animation assets.

Desired end state:

* The sector/static world remains purpose-built topology/mesh/lightmap data, not ECS.
* Movable/high-level runtime objects use the existing small ECS.
* A test goblin billboard can appear in the 3D sector world.
* The goblin uses the existing Aseprite sprite animation asset system.
* The goblin can use named animation clips `Front`, `Back`, `Left`, and `Right`.
* Directional clip selection is data-driven enough to avoid hardcoded gameplay enums.
* Billboards sample baked object lighting probes.
* Billboards can receive simple runtime dynamic light contribution.
* Billboard baked lighting does not depend on the camera-facing quad normal.
* Temporary test spawn code is clearly marked as temporary and easy to remove.
* No NPC AI, collision, combat, placement editor, inspector, 3D model renderer, or glTF support is implemented in this plan.

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
* static sector draw records
* portal visibility graph

Those systems are low-level world/map structures and remain purpose-built.

Use ECS for future high-level objects such as:

* billboard sprites
* future 3D models
* NPCs
* pickups
* projectiles
* temporary effects
* attached lights
* doors/lifts when they become gameplay/runtime objects

This plan only implements enough ECS object support for billboard sprites.

## Asset Assumptions

A test Aseprite asset exists under:

```text id="9i7z8e"
assets/sprites/goblin
```

The implementation must inspect the actual file names and use the real JSON path.

Expected animation clip names:

```text id="gotw36"
Front
Back
Left
Right
```

Each clip is currently one frame.

Use the existing `AssetManager::RequestSpriteAnimation()` path for Aseprite JSON.

Do not invent a separate sprite-sheet loader.

Do not load textures directly outside the asset system for this feature.

Use point filtering for pixel-art sprite assets unless the existing asset metadata says otherwise.

## Proposed Phases

### Phase 1: ECS Runtime Object Foundation

Goal:

Introduce a tiny ECS runtime object layer for sector-world movable objects.

Why it helps:

Billboards, future 3D models, pickups, projectiles, attached lights, NPCs, and other dynamic things need shared position, sector, and lighting plumbing.

Files/functions likely touched:

* `sources/engine/ecs/*`
* sector preview/runtime files
* new sector object component/system files
* `EngineContext` or sector preview state if needed

Exact behavior that must remain unchanged:

* Sector topology editing unchanged.
* Static sector rendering unchanged.
* Lightmap baking unchanged.
* Dynamic lights unchanged.
* Old 2D sprite/rectangle test systems should not be used by the new sector billboard renderer.
* Do not delete old test render systems in this phase unless explicitly isolated and safe.

Risks/goblins:

* Accidentally trying to ECS-ify sector topology.
* Component registration/allocation warnings if capacities are not reserved.
* Object systems becoming coupled to editor UI.
* Spawning/debug object code leaking into permanent gameplay architecture.

Non-goals:

* No rendering yet.
* No Aseprite use yet.
* No placement editor.
* No object inspector.
* No NPC behavior.
* No physics/collision for actors.

Suggested checks:

```bash id="rhi97o"
git diff --check
git diff --stat
git status --short
```

Run relevant tests.

Final report expectations:

* State components added.
* State how ECS world is owned/updated in sector preview/runtime.
* State what remains temporary/deferred.
* State verification results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 1 `Completed` only after Phase 1A and Phase 1B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 1A: Add Sector Runtime Object Components And World Integration

Goal:

Add a minimal ECS world for sector runtime objects and core components.

Implementation guidance:

Add components like:

```cpp id="kxx7rk"
struct SectorObjectTransform {
    Vector3 position = {};
    float yawRadians = 0.0f;
};

struct SectorObject {
    int currentSectorId = -1;
    bool visible = true;
};
```

Use actual project naming/style.

Integrate an `engine::World` instance into the sector preview/runtime object state.

Reserve expected initial capacities during setup/load to avoid ECS allocation warnings.

Do not create generic gameplay architecture yet.

Do not add rendering.

Tests if practical:

* create entity
* add transform/object components
* update/iterate without structural modification during `ForEach`
* destruction flush works if used

Completion notes, 2026-07-01:

* Source code changed: yes.
* Added `SectorObjectTransform` with world `Vector3` position and yaw, and `SectorObject` with current sector ID and visibility.
* Added `ReserveSectorRuntimeObjectWorld()` to pre-reserve entity/component capacity, register both component pools, and lock component registration for the preview runtime object world.
* `SectorMeshPreview` now owns an `engine::World` for sector runtime objects, reserves it during renderer resource rebuild, exposes it for later object systems, and resets it during preview resource shutdown.
* Behavior unchanged: no object rendering, asset loading, topology editing, static sector rendering, dynamic lights, lightmaps, serialization, placement UI, inspector, gameplay collision, sector lookup, or physics changed.
* Cache invalidation behavior: no topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* Lightmap source-hash behavior: unchanged; this pass did not touch lightmap inputs or hashing.
* Generated artifacts: no new generated artifact path was added; the plan-state JSON has no `sandbox_dir` field.
* Verification: `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed 14/14 including new `sector_runtime_object`; `git diff --check` passed.

#### Phase 1B: Add Current Sector And Baked Probe Lighting Systems

Goal:

Let ECS objects know their current sector and sampled baked object lighting.

Implementation guidance:

Add a component like:

```cpp id="j2czbs"
struct SectorObjectLighting {
    BakedObjectLightingSample baked = {};
};
```

or store only the ambient cube/valid flag if that fits better.

Add systems:

```text id="rlwyrc"
UpdateSectorObjectCurrentSectorSystem
UpdateSectorObjectBakedLightingSystem
```

Current sector lookup:

* use existing sector/collision/topology lookup helpers
* prefer stable current sector if available
* no actor collision/physics yet

Baked lighting:

* call `SampleBakedObjectLighting()`
* use object position and current/preferred sector
* tolerate missing/stale probes and use fallback lighting

No rendering yet.

Tests:

* object in sector receives current sector ID
* object lighting system gets valid/fallback sample without crashing
* missing probes fallback safely

Completion notes, 2026-07-01:

* Source code changed: yes.
* Added `SectorObjectLighting` with a baked `BakedObjectLightingSample`.
* Added `UpdateSectorObjectCurrentSectorSystem()` using `SectorCollisionWorld::FindSectorContainingPointPreferCurrent()` and writing `-1` when no sector contains the object.
* Added `UpdateSectorObjectBakedLightingSystem()` using `SampleBakedObjectLighting()` with the object's current sector as the preferred sector and neutral/ambient fallback behavior supplied by the existing sampler.
* `ReserveSectorRuntimeObjectWorld()` now pre-reserves/registers the lighting component pool before locking component registration.
* Behavior unchanged: no object rendering, asset loading, topology editing, static sector rendering, dynamic lights, serialization, placement UI, inspector, gameplay collision resolution, sector collision world behavior, or physics changed.
* Cache invalidation behavior: no topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* Lightmap source-hash behavior: unchanged; this pass samples existing baked object probes but did not change bake inputs, probe generation, or source hashing.
* Generated artifacts: none; the plan-state JSON has no `sandbox_dir` field.
* Verification: `cmake --build cmake-build-debug --target sector_runtime_object_tests -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed 14/14; `git diff --check` passed.

### Phase 2: Aseprite Billboard Sprite Components

Goal:

Add billboard sprite components that use existing Aseprite `SpriteAnimationHandle` assets.

Why it helps:

This uses the existing threaded asset manager and clip metadata instead of inventing sprite loading again.

Files/functions likely touched:

* asset manager use sites
* new billboard component/system files
* sector preview/runtime load/setup code

Exact behavior that must remain unchanged:

* Existing asset manager behavior unchanged.
* Old 2D sprite renderer/test systems unchanged unless explicitly isolated.
* No 3D rendering yet unless selected pass says so.

Risks/goblins:

* Requesting assets every frame instead of during setup/load.
* Storing raw `Texture2D*` or `SpriteAnimationAsset*` in components.
* Hardcoding animation enums.
* Clip name resolution failing before asset is ready.

Non-goals:

* No object placement editor.
* No NPC state machine.
* No complex animation graph.
* No 8-direction support yet if the goblin only has 4 directions.

Suggested checks:

```bash id="p7f57p"
git diff --check
git diff --stat
git status --short
```

Run relevant asset/ECS tests.

Final report expectations:

* State component fields.
* State asset request path/key.
* State clip resolution behavior.
* State fallback behavior when asset is not ready.
* State verification results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 2 `Completed` only after Phase 2A and Phase 2B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 2A: Add Billboard Sprite Animation Component And Asset Requests

Goal:

Create billboard sprite components backed by Aseprite assets.

Implementation guidance:

Add components similar to:

```cpp id="7rht8p"
struct SectorBillboardSprite {
    engine::SpriteAnimationHandle animation;
    uint32_t clipIndex = 0;
    Rectangle source = {};
    engine::TextureHandle texture;
    Vector2 sizeWorld = {1.0f, 1.0f};
    Vector2 originNormalized = {0.5f, 1.0f};
    Color tint = WHITE;
};

struct SectorBillboardAnimator {
    std::string animationId;
    float timeSeconds = 0.0f;
    bool loop = true;
};
```

Use actual project style.

Important:

* Components store handles/indices/state only.
* Do not store raw texture pointers or raw animation asset pointers.
* AssetManager is passed to systems when needed.
* Asset requests happen during explicit setup/test spawn/load path, not every frame.

No rendering yet.

Completion notes, 2026-07-01:

* Source code changed: yes.
* Added `SectorBillboardSprite` with `SpriteAnimationHandle`, clip index, current source rectangle, texture handle, world size, normalized origin, tint, and visibility state.
* Added `SectorBillboardAnimator` with a string animation ID and simple playback state.
* Added `RequestSectorBillboardSpriteAnimation()` as an explicit setup/load helper using `AssetManager::RequestSpriteAnimation()` with point filtering; it stores only handles/state in components and rejects missing IDs/paths with a null animation handle.
* Asset request path/key behavior: the helper uses the caller-provided animation ID as the asset request key and the caller-provided Aseprite JSON path as the load path. The actual test asset in this checkout is `assets/sprites/goblin.json`; wiring that asset into a test spawn remains deferred.
* `ReserveSectorRuntimeObjectWorld()` now pre-reserves/registers billboard sprite and animator component pools before locking component registration.
* Clip resolution behavior: unchanged/deferred to Phase 2B; new billboard sprites start with `InvalidSpriteClipIndex`.
* Fallback behavior when the asset is not ready: components keep null texture/source state until later systems resolve ready animation frames; missing request input returns a null animation handle and clears the animation ID.
* Behavior unchanged: no 3D rendering, topology editing, static sector rendering, dynamic lights, lightmaps, serialization, placement UI, inspector, gameplay collision, sector lookup, or physics changed.
* Cache invalidation behavior: no topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* Lightmap source-hash behavior: unchanged; this pass did not touch bake inputs, probe generation, or source hashing.
* Generated artifacts: none; the plan-state JSON has no `sandbox_dir` field.
* Verification: `cmake --build cmake-build-debug --target sector_runtime_object_tests -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed 14/14; `git diff --check` passed; `git diff --stat` and `git status --short` were run.

#### Phase 2B: Resolve Goblin Test Animation Clips

Goal:

Resolve and store the test goblin clip indices.

Implementation guidance:

Use the actual goblin Aseprite JSON path under `assets/sprites/goblin`.

Expected clip names:

```text id="uosbsc"
Front
Back
Left
Right
```

Resolve by name through existing AssetManager sprite clip lookup after the animation asset is ready.

If a clip is missing:

* log/warn
* fall back to clip 0 or `Default`
* do not crash

Keep animation IDs string/data-driven.

Do not introduce hardcoded gameplay enums such as:

```text id="rgzovk"
WALK_FRONT
WALK_BACK
```

For this test asset, it is acceptable to create a small test-only directional mapping:

```text id="g80ndm"
Front -> clip index
Back  -> clip index
Left  -> clip index
Right -> clip index
```

but mark it as test/prototype data, not final NPC animation architecture.

Completion notes, 2026-07-01:

* Source code changed: yes.
* Added prototype `SectorBillboardDirectionalClipNames` and `SectorBillboardDirectionalClips` data for Front/Back/Left/Right clip-index mapping; this is test/prototype billboard data, not final NPC animation architecture.
* Added `ResolveSectorBillboardDirectionalClips()` for ready `SpriteAnimationHandle` assets and `ResolveSectorBillboardDirectionalClipsFromAsset()` for focused tests. Missing clips warn and fall back to `Default` or clip 0; unavailable/null/not-ready assets clear the mapping and do not crash.
* Asset path behavior: the actual test asset in this checkout is `assets/sprites/goblin.json`, with atlas `assets/sprites/goblin.png`; no separate sprite-sheet loader was added and no direct texture loading was introduced.
* Clip resolution behavior: Front/Back/Left/Right are resolved by string clip names and stored as clip indices. The resolver keeps animation IDs/data-driven names as strings and does not introduce gameplay animation enums.
* Runtime rendering behavior: unchanged; no billboard rendering or test spawn was added.
* Asset loading behavior: unchanged; this pass only reads ready sprite animation asset metadata through the existing asset path.
* Behavior unchanged: no topology editing, static sector rendering, dynamic lights, lightmaps, serialization, placement UI, inspector, gameplay collision, sector lookup, or physics changed.
* Cache invalidation behavior: no topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* Lightmap source-hash behavior: unchanged; this pass did not touch bake inputs, probe generation, or source hashing.
* Generated artifacts: none; the plan-state JSON has no `sandbox_dir` field.
* Verification: `cmake --build cmake-build-debug --target sector_runtime_object_tests -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure -R sector_runtime_object` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed 14/14; `git diff --check` passed; `git diff --stat` and `git status --short` were run.

### Phase 3: 3D Billboard Rendering And Lighting

Goal:

Render animated billboard sprites in the 3D sector world.

Why it helps:

This is the first visible ECS object consumer of baked object probes.

Files/functions likely touched:

* `SectorMeshPreview.cpp/.h`
* new sector billboard renderer files
* shader/material code if needed
* asset manager access
* dynamic light data access
* object probe sampling code

Exact behavior that must remain unchanged:

* Sector world rendering unchanged.
* Dynamic point/spot lights unchanged.
* Static/baked lighting unchanged.
* Probe baking/loading unchanged.
* Billboard rendering should not affect portal culling for world geometry.

Risks/goblins:

* Wrong alpha blending/cutout behavior.
* Billboard sorting/depth issues.
* Camera-facing normal causing baked lighting to change with camera rotation.
* Sprite scale/origin wrong.
* Asset not ready causing crash.
* Dynamic light math duplicating too much sector shader complexity.

Non-goals:

* No AI.
* No collision.
* No placement editor.
* No sprite shadows.
* No normal maps.
* No 3D model renderer.

Suggested checks:

```bash id="zvk05e"
git diff --check
git diff --stat
git status --short
```

Run relevant tests/build.

Final report expectations:

* State render path.
* State alpha/depth behavior.
* State baked probe lighting behavior.
* State dynamic light behavior.
* State manual GUI smoke status.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 3 `Completed` only after Phase 3A and Phase 3B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 3A: Render Camera-Facing Billboard Quads In Sector 3D Preview

Goal:

Draw billboard sprites as quads in the 3D sector world.

Implementation guidance:

Draw a camera-facing quad using the current animation frame texture/source rectangle.

Requirements:

* world position from `SectorObjectTransform`
* bottom-centered origin by default
* configurable world size
* faces camera horizontally or fully, choose the simplest stable first approach and document it
* alpha blending or alpha test chosen deliberately
* depth test enabled so sprites can be occluded by world geometry
* no crash when animation/texture asset is missing or not ready
* draw a visible fallback marker if asset failed/missing, or skip with debug warning

Use the existing Aseprite animation asset data to get current frame source rectangle and atlas texture handle.

Do not use the old 2D `SpriteRenderSystem` directly.

Completion notes, 2026-07-01:

* Source code changed: yes.
* Render path: `SectorMeshPreview::DrawScene()` now draws ECS billboard sprites after static sector meshes and before leaving `BeginMode3D()`. The renderer iterates `SectorObjectTransform`, `SectorObject`, `SectorBillboardSprite`, and `SectorBillboardAnimator`, resolves the current Aseprite frame from the ready sprite animation asset, and draws it with `DrawBillboardPro()`.
* Billboard facing/origin behavior: first pass uses a Y-up camera-facing raylib billboard. The component's normalized origin is mapped so the default `{0.5, 1.0}` behaves as bottom-center anchoring at the object's world position.
* Alpha/depth behavior: billboards draw with alpha blending, depth testing enabled, and temporary backface culling disabled so the quad can be seen from either side. No sorting pass was added.
* Missing/not-ready fallback behavior: not-ready assets are skipped safely. Missing or failed animation assets produce a throttled preview warning and are skipped; no direct texture loading or fallback marker asset was added.
* Animation behavior: added `AdvanceSectorBillboardAnimatorSystem()` and call it from preview runtime advancement so billboard animation time advances without using the old 2D `SpriteRenderSystem`.
* Baked probe lighting behavior: unchanged/deferred to Phase 3B; this pass does not apply baked object probe lighting to billboard tint.
* Dynamic light behavior: unchanged/deferred to Phase 3B; this pass does not apply point/spot light contribution to billboards.
* Behavior unchanged: topology editing, static sector mesh rendering, portal visibility for world geometry, dynamic light selection/shadow maps, lightmap baking/loading, serialization, placement UI, inspector, gameplay collision, sector lookup, and physics were not changed.
* Cache invalidation behavior: no topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* Lightmap source-hash behavior: unchanged; this pass did not touch bake inputs, generated baked geometry, object probe generation, or source hashing.
* Generated artifacts: none; the plan-state JSON has no `sandbox_dir` field.
* Manual GUI smoke status: not performed.
* Verification: `cmake --build cmake-build-debug --target sector_runtime_object_tests -j2` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed 14/14; `git diff --check` passed; `git diff --stat` and `git status --short` were run.

#### Phase 3B: Apply Baked Probe Lighting And Basic Dynamic Light Contribution

Goal:

Light billboards consistently with the sector lighting system.

Baked lighting:

* use `SectorObjectLighting` / `SampleBakedObjectLighting()`
* for billboards, use average ambient cube or stable upper-hemisphere average
* do not use the camera-facing quad normal for baked lighting

Dynamic lights:

* add simple center-point contribution from selected/relevant dynamic point and spot lights
* first pass can use attenuation only or a stable fake/cylindrical normal
* do not require dynamic shadow receiving for billboards yet
* do not let billboard dynamic light code affect existing world dynamic lights

If shader support is needed, add a small billboard shader/material.

Manual smoke:

* goblin appears bright/dim according to probe-lit area
* moving goblin/test spawn between bright/dark probe areas changes lighting
* rotating camera does not pulse baked brightness
* dynamic light near goblin brightens it

Completion notes, 2026-07-01:

* Source code changed: yes.
* Render path: unchanged from Phase 3A; ECS billboard sprites are still drawn by `SectorMeshPreview::DrawRuntimeBillboards()` after static sector meshes inside the 3D preview.
* Alpha/depth behavior: unchanged; billboards still use alpha blending, depth testing, temporary disabled backface culling, and no sorting pass.
* Baked probe lighting behavior: billboard draw tint now uses `SectorObjectLighting` when present. The baked contribution is a stable upper-hemisphere average of the object ambient cube and does not use the camera-facing quad normal, so camera rotation should not change baked brightness.
* Dynamic light behavior: billboard draw tint now adds simple center-point contribution from the already selected preview dynamic point/spot lights. It uses distance attenuation, spot cone attenuation, flicker-adjusted intensity, and the existing dynamic-light enable flag; it does not receive dynamic shadows and does not change the world dynamic light shader path.
* Fallback behavior: billboards without `SectorObjectLighting` still render with neutral white baked lighting, and missing/not-ready animation assets remain skipped safely as in Phase 3A.
* Behavior unchanged: topology editing, static sector mesh rendering, portal visibility for world geometry, dynamic light selection/shadow maps for world geometry, lightmap baking/loading, serialization, placement UI, inspector, gameplay collision, sector lookup, and physics were not changed.
* Cache invalidation behavior: no topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* Lightmap source-hash behavior: unchanged; this pass consumes existing object probe samples and selected dynamic lights at preview render time but did not change bake inputs, generated baked geometry, object probe generation, or source hashing.
* Generated artifacts: none; the plan-state JSON has no `sandbox_dir` field.
* Manual GUI smoke status: not performed.
* Verification: `cmake --build cmake-build-debug --target sector_runtime_object_tests -j2` passed/no work; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed 14/14; `git diff --check` passed; `git diff --stat` and `git status --short` were run.

### Phase 4: Directional Billboard Animation Selection

Goal:

Pick the correct directional clip based on object facing and camera/view direction.

Why it helps:

This supports Doom-style 4-direction or later 8-direction NPC sprites without hardcoding gameplay enums.

Files/functions likely touched:

* billboard animation system
* billboard directional mapping component/data
* tests for direction selection math

Exact behavior that must remain unchanged:

* Non-directional billboard rendering still works.
* Asset loading unchanged.
* Gameplay/AI not implemented.

Risks/goblins:

* Front/back convention inverted.
* Left/right convention inverted.
* Camera angle vs object yaw confusion.
* Clip missing causing crash.
* Animation IDs becoming hardcoded enums.

Non-goals:

* No AI state machine.
* No attack/walk/death logic.
* No 8-direction goblin art requirement.
* No WAD/PK3 import.

Suggested checks:

```bash id="embegt"
git diff --check
git diff --stat
git status --short
```

Run direction selection tests.

Final report expectations:

* State direction convention.
* State clip mapping behavior.
* State missing-clip fallback.
* State verification results.

How to update this plan after completion:

* Mark Phase 4A and Phase 4 `Completed` if done.
* Add date, summary, verification results, and behavior notes.

#### Phase 4A: Select Front Back Left Right Clips By Camera Relative Facing

Goal:

Choose among `Front`, `Back`, `Left`, and `Right` clips for the goblin test asset.

Implementation guidance:

Use object yaw/facing and camera position/yaw to determine what side of the sprite the camera sees.

For the test goblin asset:

```text id="boj27j"
Front
Back
Left
Right
```

Each is currently one frame.

Rules:

* higher-level gameplay should set a logical animation id later
* this pass may use a test/prototype mapping for the goblin
* do not create hardcoded gameplay enums
* missing clip falls back safely

Add pure math tests for:

* camera in front selects `Front`
* camera behind selects `Back`
* camera to left selects `Left`
* camera to right selects `Right`
* wraparound at ±pi works

Document the convention.

Completion notes, 2026-07-01:

* Source code changed: yes.
* Direction convention: sector object yaw follows existing preview convention where yaw `0` faces +X. `Front` is selected when the camera is in front of the object along that facing direction, `Back` when behind, `Left` for negative relative XZ angle from facing, and `Right` for positive relative XZ angle from facing. Angle wraparound at +/-pi is normalized before selection.
* Clip mapping behavior: `SelectSectorBillboardDirectionalClip()` chooses from the resolved `SectorBillboardDirectionalClips` indices. `SectorMeshPreview::DrawRuntimeBillboards()` applies the selected clip only when the entity has a directional mapping component and the selection is valid.
* Missing-clip fallback: unchanged from Phase 2B; missing named clips are resolved through the existing `Default`/clip-0 fallback path, unresolved mappings return `InvalidSpriteClipIndex`, and the render path leaves the current sprite clip unchanged in that case.
* Render behavior: directional billboards now change their Aseprite clip based on camera-relative facing before resolving the current frame. Non-directional billboard rendering still works because entities without `SectorBillboardDirectionalClips` keep their existing `SectorBillboardSprite::clipIndex` behavior.
* Asset loading behavior: unchanged; this pass does not request or load assets and does not hardcode gameplay animation enums.
* Behavior unchanged: topology editing, static sector mesh rendering, portal visibility for world geometry, baked probe lighting, dynamic light contribution, lightmap baking/loading, serialization, placement UI, inspector, gameplay collision, sector lookup, and physics were not changed.
* Cache invalidation behavior: no topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* Lightmap source-hash behavior: unchanged; this pass did not touch bake inputs, generated baked geometry, object probe generation, or source hashing.
* Generated artifacts: none; the plan-state JSON has no `sandbox_dir` field.
* Manual GUI smoke status: not performed.
* Verification: `cmake --build cmake-build-debug --target sector_runtime_object_tests -j2` passed; `./cmake-build-debug/sector_runtime_object_tests` passed; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed 14/14; `git diff --check` passed; `git diff --stat` and `git status --short` were run.

### Phase 5: Temporary Goblin Test Spawn

Goal:

Add an easy way to spawn/test a goblin billboard in the current sector world.

Why it helps:

There is no placement editor/inspector yet, but the renderer needs manual smoke testing.

Files/functions likely touched:

* `SectorMeshPreview.cpp/.h`
* sector editor debug UI/input code
* test object setup code

Exact behavior that must remain unchanged:

* Save/load unchanged.
* No authored object placement data added.
* No object inspector.
* No NPC behavior.
* No permanent gameplay spawn system.

Risks/goblins:

* Temporary code becoming permanent accidentally.
* Test spawn tied to a specific user map.
* Asset requested every frame.
* Spawned entity leaking between level loads/rebuilds.

Non-goals:

* No placement editor.
* No inspector.
* No persistent object serialization.
* No NPC AI.
* No collision/combat.

Suggested checks:

```bash id="nkvzci"
git diff --check
git diff --stat
git status --short
```

Manual smoke expected.

Final report expectations:

* State test spawn trigger/UI.
* State temporary code marker.
* State cleanup/removal notes.
* State manual GUI smoke status.

How to update this plan after completion:

* Mark Phase 5A and Phase 5 `Completed` if done.
* Add date, summary, verification results, and behavior notes.

#### Phase 5A: Add Removable Goblin Debug Spawn Path

Goal:

Spawn a temporary test goblin billboard.

Implementation guidance:

Add a debug/test-only path to create a goblin billboard entity.

Use clear names/comments such as:

```text id="spn731"
TODO_REMOVE_BILLBOARD_TEST_SPAWN
temporary goblin billboard test spawn
```

Requirements:

* asset requested once during setup/load/test spawn path
* entity uses ECS components from earlier phases
* spawn at player/camera position or a fixed safe position in the current sector
* allow despawn/reset if easy
* do not serialize the spawned goblin
* do not add placement editor or inspector
* make removal easy when real object placement/NPC system arrives

Manual smoke:

* spawn goblin
* goblin appears in 3D
* goblin is depth-tested against world
* directional clip changes as camera/object relation changes
* baked probe lighting affects goblin
* dynamic lights affect goblin
* missing asset failure is handled safely

Completion notes, 2026-07-01:

* Source code changed: yes.
* Test spawn trigger/UI: in 3D preview, `F5` toggles the temporary goblin billboard spawn/despawn; the 3D preview shortcut text now lists `F5 goblin`.
* Temporary code marker: the spawn path is marked with `TODO_REMOVE_BILLBOARD_TEST_SPAWN` and logs that it is a temporary goblin billboard test spawn.
* Spawn behavior: pressing `F5` creates one ECS entity in the preview-owned runtime object world with `SectorObjectTransform`, `SectorObject`, `SectorObjectLighting`, `SectorBillboardSprite`, `SectorBillboardAnimator`, and `SectorBillboardDirectionalClips`; pressing `F5` again destroys and flushes that entity. The goblin is spawned in front of the current 3D camera and is not serialized.
* Asset loading behavior: the goblin Aseprite animation is requested only from the explicit `F5` spawn path through `AssetManager::RequestSpriteAnimation()` using `assets/sprites/goblin.json`; no direct texture loading or separate sprite-sheet loader was added. Directional clip resolution happens lazily after the requested asset is ready.
* Rendering behavior: the spawned entity uses the existing ECS billboard render path, so it remains alpha blended/depth tested and uses the existing camera-relative Front/Back/Left/Right clip selection.
* Baked probe lighting behavior: preview rebuild now loads baked object probe runtime data when present, and preview runtime advancement updates ECS object current-sector and baked-lighting components through the existing systems.
* Dynamic light behavior: unchanged from Phase 3B; the spawned goblin receives the existing billboard dynamic light contribution.
* Serialization/editor behavior: no authored object placement data, object inspector, persistent object serialization, or topology schema change was added.
* Cache invalidation behavior: no topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* Lightmap source-hash behavior: unchanged; this pass loads/samples existing object probe sidecars at preview runtime but did not change bake inputs, generated baked geometry, object probe generation, or source hashing.
* Gameplay/collision/camera behavior: collision, sector lookup implementation, physics, and visual camera effects were not changed; the debug object only queries the existing sector lookup to populate its current sector.
* Cleanup/removal notes: remove `ToggleTemporaryGoblinDebugSpawn()`, `temporaryGoblinDebugSpawnEntity`, the `F5` editor input/shortcut text, and the `TODO_REMOVE_BILLBOARD_TEST_SPAWN` block when real object placement or NPC spawning is implemented.
* Generated artifacts: none; the plan-state JSON has no `sandbox_dir` field.
* Manual GUI smoke status: not performed.
* Verification: `cmake --build cmake-build-debug --target sector_runtime_object_tests -j2` passed/no work; `cmake --build cmake-build-debug -j2` passed; `ctest --test-dir cmake-build-debug --output-on-failure` passed 14/14; `git diff --check` passed; `git diff --stat` and `git status --short` were run.

### Phase 6: Polish Tests Documentation And Completion

Goal:

Document the ECS object split and close the billboard prototype plan.

Why it helps:

This feature defines the foundation for future NPCs, props, pickups, projectiles, attached lights, and 3D models.

Files/functions likely touched:

* docs
* tests
* this plan document
* maybe removal/deprecation notes for old 2D test render systems

Exact behavior that must remain unchanged:

* Sector topology remains non-ECS.
* Object placement/editor/inspector remains deferred.
* No NPC AI or model renderer added.

Risks/goblins:

* Leaving temporary goblin code undocumented.
* Future tasks using old 2D sprite renderer by mistake.
* Not documenting billboard lighting conventions.
* Not documenting ECS scope boundary.

Non-goals:

* No cleanup crusade across unrelated old sample systems unless tiny and obvious.
* No model renderer.
* No placement editor.

Suggested checks:

```bash id="eiftfq"
git diff --check
git diff --stat
git status --short
```

Run relevant tests.

Final report expectations:

* State docs updated.
* State temporary debug spawn status.
* State deferred follow-ups.
* State verification results.

How to update this plan after completion:

* Mark Phase 6A and Phase 6 `Completed`.
* If all phases are complete, ensure all parent phases are `Completed`.
* Leave a final completion note.

#### Phase 6A: Document ECS Object Split And Close Plan

Goal:

Update docs for future dynamic-object work.

Documentation should state:

* sectors/linedefs/sidedefs/lightmaps remain purpose-built non-ECS data
* movable/high-level runtime objects use ECS
* billboard sprites use Aseprite `SpriteAnimationHandle`
* old 2D sprite/rectangle renderer systems are legacy/test examples and not the sector-world billboard path
* billboards sample baked object probes
* billboards do not use camera-facing normal for baked lighting
* dynamic lights are added on top
* placement editor/inspector is deferred until real prop/NPC object authoring
* temporary goblin spawn code is marked for later removal

Completion notes, 2026-07-01:

* Source code changed: no.
* Docs updated: `docs/sector_editor.md` now documents the non-ECS sector topology boundary, ECS runtime object scope, Aseprite billboard asset-handle path, legacy/test status of the 2D sprite and rectangle renderers, billboard baked/dynamic lighting behavior, deferred placement/inspector/NPC/model scope, and the temporary `TODO_REMOVE_BILLBOARD_TEST_SPAWN` / `F5` goblin spawn.
* Docs updated: `docs/sector_baked_object_lighting_probes_design.md` now records sector 3D preview billboards as the first object-probe consumer and clarifies stable upper-hemisphere billboard baked lighting plus simple dynamic point/spot light contribution.
* Docs updated: `docs/assets.md` now clarifies that the 2D `SpriteRenderer` / `SpriteRenderSystem` path is not the sector-world billboard renderer.
* Temporary debug spawn status: still present, non-serialized, and documented as temporary removal work for the later real placement/NPC path.
* Deferred follow-ups: placement editor, object inspector, persistent object serialization, NPC definitions/AI, actor collision/physics, 3D model rendering, billboard dynamic shadow receiving, production directional art, and cleanup/removal of old 2D sample render systems remain out of scope.
* Behavior unchanged: no runtime rendering, ECS components, asset loading, probe lighting, dynamic light sampling, serialization, editor behavior, build/test behavior, topology editing, gameplay collision, sector lookup, physics, or camera behavior changed.
* Cache invalidation behavior: no topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* Lightmap source-hash behavior: unchanged; this pass only updated documentation and did not change bake inputs, generated baked geometry, object probe generation, or source hashing.
* Generated artifacts: none; the plan-state JSON has no `sandbox_dir` field.
* Manual GUI smoke status: not performed.
* Verification: `cmake --build cmake-build-debug -j2` passed/no work; `ctest --test-dir cmake-build-debug --output-on-failure` passed 14/14; `git diff --check` passed; `git diff --stat` and `git status --short` were run.

## Deferred Decisions For Later Plans

These are intentionally out of scope:

* Placement editor for objects.
* Object inspector.
* Persistent object serialization.
* NPC definition system.
* AI/state machines.
* Actor collision/physics.
* Doors/lifts as ECS runtime objects.
* Pickups/projectiles.
* Attached light emitters.
* 8-direction or 16-direction production art.
* WAD/PK3 import.
* 3D glTF model renderer.
* Skeletal animation.
* Billboard dynamic shadow receiving.
* Sprite normal maps.
* Cleanup/removal of old 2D sprite/rectangle test systems, unless done as a later cleanup task.

## Final Completion Criteria

This plan is complete when:

* ECS runtime object foundation exists for sector-world movable objects
* objects can track current sector
* objects can sample baked object probe lighting
* Aseprite billboard sprite components exist
* the `assets/sprites/goblin` test asset can be requested and used
* `Front`, `Back`, `Left`, and `Right` clips can be resolved safely
* a goblin billboard can render in the 3D sector world
* billboard baked lighting uses probe-derived stable lighting
* billboard dynamic lighting works at a basic level
* 4-direction directional clip selection works
* temporary goblin test spawn is clearly marked and easy to remove
* no placement editor/inspector/NPC/3D-model scope leaks into this implementation
