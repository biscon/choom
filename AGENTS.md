# Project / Engine / Sector Editor Rules

This project uses a small sparse-set ECS for a C++17 / raylib 6 personal
indie-game framework and a Doom/Build-ish sector-engine editor.

The ECS/framework rules below still apply to reusable engine/game code. Sector
editor work must also follow the topology, cache, rendering, sky, lightmap, and
task-scoping rules in the later sections.

## ECS design rules

1. Entity is a generational handle.
2. Components are plain data structs with no behavior.
3. Systems are plain functions first, not classes.
4. World owns entity/component storage.
5. Systems receive external context explicitly.
6. Structural changes are deferred during update.
7. Runtime Entity handles are not save IDs.
8. Prefer sparse-set storage first.
9. Keep system order explicit.
10. Do not over-split components.
11. Do not make everything an entity.
12. Avoid raw pointers between components.
13. Use handles for assets.
14. Add reflection/serialization manually before getting fancy.
15. Build the framework from real game needs, not theoretical engine purity.

## Allocation rules

- Avoid dynamic allocation during normal frame update/render.
- Pre-reserve entity capacity during initialization or level loading.
- Pre-reserve component pool capacities during initialization or level loading.
- Pre-reserve command/deferred-destruction buffers where practical.
- Do not load textures, sounds, shaders, files, or other assets during normal update/render.
- Entity deletion must be deferred during system iteration.
- Component add/remove/destroy operations during gameplay should be used carefully and preferably through explicit command/deferred phases if they may happen during iteration.

## ECS allocation/debug rules

- The ECS should be pre-reserved during initialization or level loading.
- Runtime dynamic allocation is allowed as a fallback, but it should print a console warning when detected.
- Call `ReserveEntities()` before creating runtime entities.
- Call `ReserveComponentTypes()` before registering component pools.
- Call `ReserveComponent<T>()` for every component type expected during normal runtime.
- Call `LockComponentRegistration()` after expected component pools have been registered.
- Creating component pools after registration is locked is allowed, but must warn.
- Growing entity/component storage beyond reserved capacity is allowed, but must warn.
- These warnings are intended to catch accidental frame-time allocation without making early development painful.

## ECS iteration/structural modification rules

- Do not structurally modify the ECS while inside `World::ForEach()`.
- Structural modifications include:
  - creating entities
  - queuing entity destruction
  - flushing destroyed entities
  - adding components
  - removing components
- Structural modification during `ForEach()` should assert in debug builds.
- Use explicit phases for structural changes.
- A command buffer may be added later, but do not implement it yet.

## Asset system rules

- ECS components store asset handles, not raylib resource objects.
- `AssetManager` owns raylib resources such as `Texture2D` and `Font`.
- Systems receive `AssetManager&` explicitly when they need assets.
- Background loader threads may perform file IO, CPU decoding, and CPU preprocessing.
- Background loader threads must not call OpenGL/GPU resource creation APIs.
- Texture GPU upload must happen on the main thread.
- Texture unload must happen on the main thread before the raylib window/context is closed.
- Font loading and unloading must happen on the main thread before the raylib window/context is closed.
- Asset scopes are named dynamic handles.
- The engine creates a global asset scope during asset manager initialization.
- Level/cutscene/temporary resources should use their own scopes.
- Generated textures should be uploaded through `AssetManager` and owned by an
  asset scope; temporary/level generated textures should unload with that scope.
- CPU image baking may happen during initialization or explicit load phases.
  GPU upload/unload still belongs to the asset manager on the main thread.
- Duplicate texture requests in the same scope with the same key/path/flags should return the same handle.
- Loading systems should support both:
  - request assets and render placeholders until ready
  - request all assets, show a loading screen, then spawn entities
- Missing, failed, or not-yet-ready textures must not crash render systems.

## Font asset rules

- Fonts are assets/resources owned by `AssetManager`.
- ECS components and UI/game code should store/use `FontHandle`, not raylib `Font` ownership.
- A font asset represents a concrete font file loaded at a concrete pixel size.
- The loaded pixel size must be stored with the font asset.
- Drawing text at a different size is bitmap scaling and should be avoided unless intentional.
- Font loading currently happens on the main thread using raylib `LoadFontEx()`.
- Font unloading must happen on the main thread before the raylib window/context is closed.

## Asset manager threading rules

- Public `AssetManager` API is intended to be called from the main thread unless explicitly documented otherwise.
- The internal worker thread may perform CPU-side file loading and preprocessing.
- The internal worker thread must not create, mutate, or destroy GPU resources.
- Texture GPU upload and unload happen during `AssetManager::UpdateMainThread()` or `AssetManager::Shutdown()` on the main thread.
- Font loading and unloading happen during `AssetManager::UpdateMainThread()` or `AssetManager::Shutdown()` on the main thread.
- `GetTexture()` returns a pointer valid for immediate main-thread rendering use; do not store it long term.

## Sprite animation rules

- Aseprite sprite animations are assets/resources, not per-entity component data.
- Sprite animation assets own parsed frame/clip metadata and reference their atlas with `TextureHandle`.
- ECS components store `SpriteAnimationHandle`, clip index, playback state, and render state only.
- Components must not have methods that need access to global managers/systems.
- Use free helper functions such as `SetSpriteClip()` for simple state changes.
- `SpriteAnimatorSystem` advances playback and writes `SpriteRenderer`.
- `SpriteRenderSystem` draws `SpriteRenderer`.
- Do not perform string clip lookup every frame; resolve clip names to indices during load/spawn where practical.
- Missing/not-ready animation assets must not crash systems.

## Input system rules

- Input is an engine service owned by `EngineContext`.
- Input is not stored in ECS components.
- Raw raylib input polling should happen in `engine::Input`.
- Poll input once per frame.
- Convert input transitions into per-frame events.
- Use events for presses, releases, clicks, text input, and wheel movement.
- Use current input state for continuous movement/aiming.
- UI should consume input events before game systems.
- Consumed input events should set `InputEvent::handled = true`.
- Game systems should normally read only unhandled events.
- Do not use `std::unordered_map` for per-frame key repeat state.
- Reserve input event capacity during initialization.
- If event capacity is exceeded, warn but do not crash.
- `BeginFrame()` clears frame events only; it must not reset double-click/key-repeat state.

## Style rules

- Keep code simple, readable, and debuggable.
- Prefer explicit system order over hidden scheduling.
- `engine/` contains reusable framework code.
- `game/` contains current project-specific code.
- Reusable symbols use `namespace engine`.
- Project-specific symbols use `namespace game`.
- Folder subdirectories may organize code by subsystem, but do not add matching nested namespaces unless the project grows enough to justify it.
- Avoid the global namespace for new project code.
- Do not add subsystem namespaces unless the project grows enough to justify them.
- Do not introduce Unity-style GameObject classes.
- Do not add virtual Update methods.
- Do not add inheritance-based gameplay entities.
- Do not add large framework abstractions before they are needed.
- Preserve existing project structure unless a small addition is clearly needed.

## Sector editor / topology rules

- The active editor format is topology v2 / linedef-based `SectorTopologyMap`.
- Do not reintroduce the old polygon `SectorMap` fallback.
- Stable positive integer IDs are used for vertices, linedefs, sidedefs,
  sectors, lights, etc.
- Planar topology coordinates use exact integer `SectorCoord` with
  `coordSubdivisions = 16`.
- Sectors/linedefs/sidedefs follow Doom-like topology:
  - vertices define endpoints
  - linedefs connect vertices
  - sidedefs belong to one side of a linedef
  - sectors own loops/sidedefs
  - two-sided linedefs are portals
  - one-sided linedefs are walls
- Preserve existing topology schema unless a task explicitly asks for a schema
  change.
- Use optional/backward-compatible JSON fields where practical.
- Omit default values on save when that matches existing serialization style.
- Prefer small, data-oriented structs and helper functions over OOP-heavy
  designs or polymorphism.
- Use stable IDs/handles rather than storing raw pointers into STL containers.

## Sector editor mutation and cache invalidation rules

- The 2D topology editor has a derived render cache for validation warnings,
  sector fills, outlines, labels, linedefs, vertices, and lights.
- Do not rebuild expensive derived topology every frame.
- Avoid calling `ValidateSectorTopologyMap()`, `ExtractSectorTopologyLoops()`,
  `BuildSectorTopologyIndexes()`, or `mapbox::earcut()` from the steady 2D frame
  draw path.
- The topology map is the source of truth; the 2D render cache is
  derived/editor-only.
- Any mutation that changes live topology or visible cached 2D editor state must
  invalidate the 2D topology render cache.
- Prefer existing dirty/invalidation helpers such as
  `MarkTopologyDocumentEdited()` / `InvalidateTopologyRenderCache()` rather than
  ad-hoc cache edits.
- Future direct `state.topologyMap` mutations must be paired with the
  appropriate document-edited/cache invalidation path.
- Over-invalidation is acceptable; missed invalidation is the bug.
- Texture registry changes currently do not require 2D render-cache invalidation
  unless the 2D cache starts storing texture/material display state.
- Baked lightmap result changes should not invalidate the 2D topology render
  cache unless the 2D editor starts drawing that data.
- Keep picking behavior consistent with what is drawn.
- Pending tool overlays, hover/selection overlays, drag previews, and UI may
  remain live/immediate; do not cache them unless explicitly scoped.
- When touching topology mutation code, mention cache invalidation behavior in
  the final report.

## Sector editor rendering and preview rules

- 3D preview builds cached/generated meshes when entering/rebuilding preview; do
  not regenerate expensive 3D geometry every frame.
- Visual-only camera effects such as step smoothing, headbob, and landing dip
  must never feed into collision, sector lookup, or physics.
- Gameplay preview settings use runtime/world units, roughly
  `1 world unit ~= 1 meter`.
- Sector heights and static-light authoring values are converted to world units
  at render/collision/bake boundaries. Do not mix authored height units with
  runtime gameplay units.
- `SectorCollisionWorld` is the reusable topology-based collision query layer.
  Do not use generated render triangles as gameplay collision.
- Gameplay collision is currently considered good enough/locked unless a bug is
  specifically being fixed.
- Missing, failed, or pending textures must not crash preview rendering.

## Sky and outdoor-sector rules

- `SectorTopologySector::ceilingSky` marks a sector ceiling as open sky.
- Sky ceilings do not generate normal ceiling geometry.
- Sky-sky portals suppress upper wall strips between both-sky sectors.
- `ceilingSky` is geometry-affecting and must remain included in the lightmap
  source hash.
- Sky visual settings are map-level, not per-sector.
- Sky visual settings include texture ID, yaw offset, vertical offset/scale, and
  top cap color.
- Sky visual settings are visual-only and must not be included in the lightmap
  source hash.
- The sky cylinder/top cap is visual-only:
  - no collision
  - no picking
  - no bloom
  - no lightmap receiver/occluder role
  - no generated surface metadata
- Do not hardcode sky asset paths; use map texture IDs.
- Missing/unloaded/failed sky textures must gracefully fall back to the existing
  clear-color behavior.

## Lightmap and lighting rules

- Static baked lightmaps are part of the topology editor.
- Existing point lights and AO must not regress when adding new lighting
  features.
- Lightmap source hash must include all settings/data that affect baked
  geometry, receiver layout, occluders, or baked lighting results.
- Lightmap source hash must not include purely visual preview settings.
- Preview settings are excluded from the lightmap source hash.
- Sky visual settings are excluded from the lightmap source hash.
- `ceilingSky` is included because it changes generated/baked geometry.
- Directional sun/moon light settings, if present, are included because they
  affect baked lighting.
- Middle textures currently receive baked light but do not cast baked
  shadows/occlude lightmap rays unless a task explicitly changes that behavior.
- Do not add alpha-aware middle texture shadows unless explicitly scoped.
- Do not add dynamic runtime lights or shadowmaps as part of baked-light tasks
  unless explicitly requested.
- When touching lightmaps, mention source-hash behavior in the final report.

## Feature tasks vs cleanup/refactor tasks

- During normal feature or bugfix tasks, avoid unrelated architecture cleanup,
  broad rewrites, renames, file moves, or style churn.
- Architecture/code-quality cleanup is allowed when the task explicitly asks for
  it.
- Cleanup tasks should still be scoped, incremental, and behavior-preserving
  unless the task explicitly says behavior may change.
- Prefer extracting small helpers/backend modules over large framework
  abstractions.
- Do not split `SectorEditor.cpp` casually during feature work.
- `SectorEditor.cpp` may be cleaned up or partially extracted during a dedicated
  cleanup/refactor task.
- Dedicated cleanup/refactor tasks must state:
  - what code is being cleaned up
  - what behavior must remain unchanged
  - what files/modules are allowed to move/change
  - what tests/build commands must pass
  - any manual verification needed
- Do not mix cleanup and feature work unless the cleanup is directly required to
  implement the feature safely.

## Codex task behavior for this project

- Keep changes scoped to the requested task.
- Avoid unrelated rewrites, renames, style churn, or architecture cleanup during
  feature/bugfix tasks.
- Preserve existing structure unless the task explicitly asks for a refactor.
- Prefer incremental helpers over broad framework abstractions.
- Do not add Unity-style GameObject classes, virtual update hierarchies, or
  OOP-heavy gameplay/entity designs.
- When touching topology mutations, explicitly mention cache invalidation in the
  final report.
- When touching lightmaps, explicitly mention source-hash behavior in the final
  report.
- When touching gameplay/collision/camera, explicitly state whether
  collision/sector lookup/physics changed.
- Do not claim manual GUI verification unless it was actually performed.
- C++ tests must not depend on user-edited levels under `assets/levels` or
  `assets/sector_demo` staying in a particular shape. Use generated test data,
  temporary JSON files, or dedicated immutable fixtures outside the engine asset
  tree instead.
- Run the usual checks:
  - `cmake --build cmake-build-debug -j2`
  - `ctest --test-dir cmake-build-debug --output-on-failure`
  - `git diff --check`
  - `git diff --stat`
  - `git status --short`
