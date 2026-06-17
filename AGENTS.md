# Project ECS Rules

This project uses a small sparse-set ECS for a C++17 / raylib 6 personal indie-game framework.

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
