# Sector Billboard Authoring Audit

Date: 2026-07-02

Scope: audit only. This report investigates the current goblin-only runtime
object authoring path and proposes an implementation path for replacing it with
a generic `Billboard` editor tool. No feature implementation is included here.

## 1. Current Runtime Object Architecture

Current authored placed-object data lives in
`sources/sector_demo/SectorTopologyMap.h`:

- `SectorPlacedRuntimeObject`
  - `int id`
  - `std::string definitionId`
  - `Vector3 position`
  - `float yawRadians`
- `SectorTopologyMap::runtimeObjects` owns the authored placed-object list.

Current JSON serialization is in
`sources/sector_demo/SectorTopologySerialization.cpp`:

- `ReadRuntimeObject()` requires:
  - `id`
  - `definitionId`
  - `position`
  - `yawDegrees`
- `WriteRuntimeObject()` writes those same fields.
- `ValidateRuntimeObjects()` requires positive unique IDs, non-empty
  `definitionId`, finite position components, and finite yaw.

Runtime spawning is in `sources/sector_demo/SectorRuntimeObjects.cpp`:

- `ResetSectorRuntimeObjectsForMap()` clears ECS runtime objects, refreshes
  map-derived runtime object support data, then spawns placed objects.
- `RefreshSectorRuntimeObjectMapData()` loads baked object probe data and builds
  `SectorCollisionWorld` for object sector lookup.
- `SpawnPlacedRuntimeObjects()` iterates `map.runtimeObjects`, resolves each
  object's definition, requests its sprite animation asset through
  `AssetManager::RequestSpriteAnimation()`, converts authored position to world
  units with `PlacedRuntimeObjectAuthoringToWorldPosition()`, creates an ECS
  entity in `EngineContext::world`, and adds sector object components.
- `UpdateSectorRuntimeObjects()` advances billboard animator time, resolves
  directional clips once assets are available, updates current sector lookup,
  refreshes baked lighting samples, and refreshes diagnostics.

Placed IDs map to ECS entities through
`SectorRuntimeObjectState::placedObjectEntities`, a vector of
`SectorPlacedRuntimeObjectEntity { placedObjectId, entity }`. This is
bookkeeping only; the authored object remains in `SectorTopologyMap`, and the
runtime mutable transform/animation state belongs in ECS components.

Runtime reset/refresh lifecycle:

- Full reset: `ClearSectorRuntimeObjects()` destroys ECS entities that have
  `SectorObject`, flushes deferred destruction, unloads the runtime object asset
  scope, and keeps world reservation state.
- Map reset: `ResetSectorRuntimeObjectsForMap()` calls full clear, reloads
  object probe and sector lookup data, then spawns.
- Authoring refresh: `SectorEditor::RefreshRuntimeObjectsAfterAuthoringEdit()`
  calls `SpawnPlacedRuntimeObjects()` without unloading the runtime object asset
  scope. This replaces mapped runtime entities without duplicating them.
- Demo/editor load and preview setup call the reset/spawn helpers explicitly;
  asset requests happen during those explicit phases, not inside render loops.

`SectorMeshPreview` remains renderer-only for runtime objects. It includes
`SectorRuntimeObjects.h` and draws ECS billboards in
`SectorMeshPreview::DrawRuntimeBillboards(engine::AssetManager&, engine::World&)`,
but it does not own, spawn, reset, or destroy runtime object ECS entities. Docs
in `docs/sector_editor.md` already state that `EngineContext::world` is the
authoritative ECS world and `SectorMeshPreview` only observes it for rendering.

## 2. Current Goblin Definition/Tool Path

The built-in `goblin` definition lives in
`GetSectorRuntimeObjectDefinitions()` in
`sources/sector_demo/SectorRuntimeObjects.cpp`. It is a
`SectorRuntimeObjectDefinition` with:

- `id = "goblin"`
- `kind = "billboard"`
- `billboard.spriteAnimationAssetPath = "assets/sprites/goblin.json"`
- `billboard.sizeWorld = {0.8f, 1.2f}`
- `billboard.originNormalized = {0.5f, 1.0f}`
- directional clips `Front`, `Back`, `Left`, `Right`

The editor tool is still named `SectorEditorTool::RuntimeObject` in
`sources/sector_editor/SectorEditorTypes.h`. User-facing labels/help are in
`SectorEditorHelpers.cpp`:

- `ToolName(RuntimeObject)` returns `"Object"`.
- `ToolHelpText(RuntimeObject)` says it places a goblin runtime object.

The placement path is `SectorEditor::AddRuntimeObjectAt()` in
`SectorEditor.cpp`. It finds the containing sector, creates a
`SectorPlacedRuntimeObject`, assigns `definitionId = "goblin"`, places it at
`{mapPoint.x, sector->floorZ, mapPoint.y}`, selects it, marks the topology
document edited, and refreshes runtime objects.

The inspector path in `SectorEditor::DrawSectorsPanel()` is goblin-specific:

- It displays Object ID and Definition ID.
- If the selected definition is `goblin`, it displays `Definition: goblin`.
- Otherwise it offers a `Set definition: goblin` button.
- It exposes Position X/Y/Z, Yaw, and Delete.

The 2D cache draws runtime objects in
`SectorEditorTopologyRenderCache.cpp`. Cache build currently treats
`definitionId == "goblin"` as known; the direct update helper in
`SectorEditor.cpp` uses `FindSectorRuntimeObjectDefinition()`.

What to remove, rename, migrate, or preserve:

- Rename the user-facing tool from `Object` to `Billboard`, and change help and
  status text from goblin placement to billboard placement.
- Keep the internal enum name for a small phase if that reduces churn, but the
  final code should probably rename `RuntimeObject` UI concepts where they are
  billboard-specific. Avoid broad editor refactors during the feature phase.
- Replace the inspector's `Set definition: goblin` branch with generic
  billboard fields.
- Replace the built-in registry as the primary data source for new billboards.
  A generic billboard should be authored per placed object rather than selecting
  one hardcoded content definition.
- Preserve `definitionId: "goblin"` loading for existing maps. The safest
  compatibility path is to treat old objects with `definitionId == "goblin"` and
  no billboard payload as legacy billboard objects using the current goblin
  defaults. On save, either keep the legacy shape if unchanged or migrate to the
  new generic billboard representation once edited. The project should choose
  one save policy before implementation.

## 3. Billboard ECS Components And Renderer

Relevant sector-runtime ECS components are defined in
`SectorRuntimeObjects.h`:

- `SectorObjectTransform`
  - `Vector3 position`
  - `float yawRadians`
- `SectorObject`
  - `int currentSectorId`
  - `bool visible`
- `SectorObjectLighting`
  - `BakedObjectLightingSample baked`
- `SectorBillboardSprite`
  - `SpriteAnimationHandle animation`
  - `uint32_t clipIndex`
  - `Rectangle source`
  - `TextureHandle texture`
  - `Vector2 sizeWorld`
  - `Vector2 originNormalized`
  - `float alphaCutoff`
  - `Color tint`
  - `bool visible`
- `SectorBillboardAnimator`
  - `std::string animationId`
  - `float timeSeconds`
  - `float speed`
  - `bool playing`
  - `bool loop`
  - `bool finished`
- `SectorBillboardDirectionalClips`
  - clip names and resolved clip indices for front/back/left/right

Width/height are currently represented as `SectorBillboardSprite::sizeWorld`.
Origin/anchor is already represented as normalized `originNormalized`, default
bottom-center `{0.5f, 1.0f}` in the goblin definition. The quad builder uses
that normalized origin to offset a camera-facing world-space quad.

Directional clip selection already exists:

- `SectorBillboardDirectionalClips` stores the four authored clip names and
  resolved indices.
- `ResolveSectorBillboardDirectionalClips()` resolves names after the animation
  asset is available.
- `SelectSectorBillboardDirectionalClip()` compares camera direction to
  `SectorObjectTransform::yawRadians` and returns front/back/left/right.
- `SectorMeshPreview::DrawRuntimeBillboards()` uses directional clips when the
  component exists.

Single non-directional playback can fit by omitting
`SectorBillboardDirectionalClips` and setting `SectorBillboardSprite::clipIndex`
to the selected clip index. The renderer already falls back to `clipIndex`, and
if that is invalid it uses clip 0. The missing piece is a spawn-time path that
resolves a single clip name into `sprite.clipIndex` after the asset is ready, or
a small runtime component that stores the single clip name until it can be
resolved. Do not store raw pointers to clips or animation assets.

Clip names are currently stored in definitions and copied to ECS
`SectorBillboardDirectionalClips`. They are not stored on
`SectorPlacedRuntimeObject`, which is the central gap for generic authoring.

Mutable runtime state is kept in ECS:

- Transform: `SectorObjectTransform`
- Sector lookup: `SectorObject`
- Lighting sample: `SectorObjectLighting`
- Animation playback time/playing/finished: `SectorBillboardAnimator`
- Sprite render state and asset handles: `SectorBillboardSprite`

`SectorRuntimeObjectState` currently stores bookkeeping and diagnostics only,
plus map-derived probe/sector lookup helpers. That is the right boundary. Do
not add shadow copies of mutable transform or animation state there.

Two-source-of-truth risks:

- The authored object has position/yaw, while ECS has mutable runtime
  position/yaw. For editor-authored static billboards this is acceptable because
  refresh respawns ECS from authored data. Future gameplay mutation must not
  write back implicitly to authored level data.
- A future sprite picker may be tempted to cache parsed animation metadata in
  `SectorRuntimeObjectState`. Keep picker metadata in editor modal state or use
  asset metadata through handles; do not mirror runtime playback state outside
  ECS.
- `SectorBillboardSprite::source` and `texture` appear to be legacy/cache
  fields; the sector billboard draw path resolves the current frame from the
  animation asset each draw. Future work should avoid making these a second
  current-frame truth unless deliberately converting to a renderer cache.

## 4. Aseprite Asset Support

Aseprite animation asset support is implemented in
`sources/engine/assets/SpriteAnimationAssets.cpp/.h`.

The parser currently reads:

- `frames` object.
- Per-frame `frame` rectangle.
- Per-frame `sourceSize`.
- Per-frame `spriteSourceSize`.
- Per-frame `duration`, converted from milliseconds to seconds with a safe
  `0.1f` fallback.
- `meta.image`, resolved relative to the JSON file path when relative.
- `meta.frameTags`, including `name`, `from`, `to`, optional `repeat`, and
  `direction`.
- Tag direction values:
  - `"reverse"` maps to `SpritePlaybackMode::Reverse`
  - `"pingpong"` maps to `SpritePlaybackMode::PingPong`
  - other/forward values map to `SpritePlaybackMode::Loop`

If no frame tags are present, clips are inferred from frame names with
`ExtractClipNameFromFrameName()`. If that still yields no clips, a `Default`
clip covering all frames is created.

Runtime animation data model:

- `SpriteAnimationAsset` owns atlas texture handle, frames, and clips.
- `SpriteFrame` stores source rectangles, source sizes, sprite source sizes,
  and duration.
- `SpriteClip` stores clip name, first frame, frame count, playback mode, and
  repeat.

Clip lookup:

- Generic asset lookup exists as `AssetManager::FindSpriteClipIndex()` and
  `SpriteAnimationAssets::FindClipIndex()`.
- Sector-specific directional lookup uses local helpers in
  `SectorRuntimeObjects.cpp`.

Sector billboards currently respect:

- Frame durations: yes. `ResolveBillboardFrameIndexAtTime()` sums each frame's
  duration and selects frames by `SectorBillboardAnimator::timeSeconds`.
- Animation playback state/time: mostly yes. `AdvanceSectorBillboardAnimatorSystem()`
  increments `timeSeconds` only when `playing`, not `finished`, and
  `speed > 0`.
- Paused state: yes at runtime through `SectorBillboardAnimator::playing`.
  However the authored placed-object data cannot yet save a per-object
  playing/paused default.
- Reverse playback: yes in `BillboardPlaybackFrameOffset()`.
- Pingpong playback: yes in `BillboardPlaybackFrameCount()` and
  `BillboardPlaybackFrameOffset()`.

Known gaps:

- `SpritePlaybackMode::Once` exists but `ParsePlaybackMode()` does not appear to
  parse an Aseprite tag value into `Once`; unknown/forward values become loop.
- `SpriteClip::repeat` is parsed but the sector billboard draw-time resolver
  does not use repeat counts. Existing engine 2D `SpriteAnimatorSystem` also
  ignores `repeat`.
- `SectorBillboardAnimator::finished` is never set by the sector billboard
  time-based resolver. Non-loop/once completion is therefore not fully modeled
  in the sector billboard path.
- The inspector should not expose reverse/pingpong/frame times for v1; the
  current asset data is enough for automatic playback modes except repeat/once
  completion semantics.

Smallest safe implementation path if playback gaps must be closed:

1. Add tests around `ResolveBillboardFrameIndexAtTime()` behavior or extract it
   to a testable helper shared with the 2D animator.
2. Define exact semantics for Aseprite `repeat` and any intended `Once`
   mapping. Do this once in engine animation helpers, then call the helper from
   both sector billboards and `SpriteAnimatorSystem`.
3. Keep `SectorBillboardAnimator` as the mutable ECS state. Do not add
   per-object animation time to `SectorRuntimeObjectState`.

## 5. Sprite Asset Scanner / Picker Feasibility

Existing modal patterns:

- Texture picker state: `TexturePickerState` in `SectorEditorTypes.h`.
- Texture picker UI: `DrawTexturePickerModal()` in
  `SectorEditorTextureModals.cpp`.
- Add-map-texture modal with scan/preview flow:
  `DrawAddMapTextureModal()`, `RefreshAddMapTextureScan()`,
  `SelectAddMapTexturePath()`, and `RefreshAddMapTexturePreview()`.
- Editor wrapper methods in `SectorEditor.cpp` call modal helpers and pass
  callbacks.

Existing file scanning helpers:

- `ScanAssetImagePngs()` in `SectorEditorHelpers.cpp` scans
  `ASSETS_PATH/images` recursively with
  `std::filesystem::recursive_directory_iterator`.
- It returns normalized `assets/...` paths and status messages. This is the
  right pattern for an `assets/sprites` recursive scanner.

Recommended sprite scan:

- Add a helper like `ScanAssetSpriteAsepriteJsons(std::string& message)` that
  scans `ASSETS_PATH/sprites` recursively.
- Filter `.json` files.
- Return stable sorted `assets/sprites/...json` paths.
- Do not load sprite textures during scanning.

Cheap validation for candidate JSON:

- Parse JSON in editor helper code with `nlohmann::ordered_json::parse(...,
  nullptr, false)`.
- Require object root, object `frames`, object `meta`, non-empty string
  `meta.image`.
- Optionally discover clip names from `meta.frameTags` first, then fallback to
  `ExtractClipNameFromFrameName()` behavior. If no clips are found, expose
  `Default`.
- This validation should be enough for picker listing; full asset loading still
  belongs to `AssetManager`.

Resolving `meta.image`:

- Follow the same behavior as `ParseSpriteAnimationJson()`: if `meta.image` is
  relative, resolve relative to the JSON file's parent directory.
- Convert `assets/...` editor paths to filesystem paths with the existing
  `ResolveEditorAssetPath()` pattern.
- For preview, convert the resolved filesystem path back to an asset request key
  or use a stable modal-local key.

Preview texture request:

- Use an editor modal preview scope, mirroring `AddMapTextureState::previewScope`.
- Request the atlas image through `AssetManager::RequestTexture()` on selection
  change, not every draw.
- Use point filtering for sprite previews unless v1 wants a filter option.
- Unload the preview scope when closing the modal or changing selection.
- The live playing clip preview is not required for v1; `engine::Image()` can
  preview the atlas texture and safely shows a placeholder while loading/failed.

UI components from `docs/ui.md` to use later:

- `BeginPanel()` / `EndPanel()` and `LabelFieldRow()` for inspector/property rows.
- `Button()` and `ToolButton()` for commands and mode toggles.
- `Checkbox()` for keep-aspect, playing, and directional mode.
- `FloatInput()` for position, yaw, width, height, and origin fields.
- `List()` in a `BeginScrollArea()` / `EndScrollArea()` for sprite and clip
  lists.
- Existing option/dropdown-like behavior may need to be represented with a
  list/modal in the current UI library; do not invent a separate UI system.
- `Image()` for atlas preview.

Do not implement the picker in this audit.

## 6. Proposed Data Model For Generic Billboard

Recommendation: make generic billboards authored placed-object data, not
hardcoded runtime definitions. Preserve `SectorPlacedRuntimeObject` as the
object row with stable ID, position, and yaw, and add an optional billboard
payload struct.

Suggested v1 C++ shape:

```cpp
struct SectorPlacedBillboard {
    std::string spriteAnimationPath;
    Vector2 sizeWorld = {1.0f, 1.0f};
    bool keepAspectRatio = true;
    Vector2 originNormalized = {0.5f, 1.0f};
    bool directional = false;
    std::string clip;
    std::string frontClip = "Front";
    std::string backClip = "Back";
    std::string leftClip = "Left";
    std::string rightClip = "Right";
    bool playing = true;
};

struct SectorPlacedRuntimeObject {
    int id = 0;
    std::string definitionId; // legacy compatibility; "billboard" or "goblin"
    Vector3 position = {};
    float yawRadians = 0.0f;
    SectorPlacedBillboard billboard;
};
```

JSON shape recommendation:

```json
{
  "id": 1,
  "kind": "billboard",
  "position": [8.0, 0.0, 6.0],
  "yawDegrees": 0.0,
  "billboard": {
    "spriteAnimationPath": "assets/sprites/goblin.json",
    "width": 0.8,
    "height": 1.2,
    "keepAspectRatio": true,
    "originNormalized": [0.5, 1.0],
    "directional": true,
    "frontClip": "Front",
    "backClip": "Back",
    "leftClip": "Left",
    "rightClip": "Right",
    "playing": true
  }
}
```

Compatibility choices:

- Prefer `kind: "billboard"` for new saves.
- Continue reading old `definitionId`. For old goblins, synthesize the
  billboard payload from the built-in goblin defaults.
- Consider writing both `kind` and `definitionId` for one transition only if
  older tools must read new maps. Otherwise write `kind` for new billboards and
  only preserve `definitionId` on legacy objects that were not migrated.
- If both `kind` and `definitionId` are present, `kind` should be authoritative
  for new code and `definitionId` should only be compatibility metadata.

Fields to omit on save when default:

- Omit `yawDegrees` only if the existing serializer style is intentionally
  changed; current runtime objects always write yaw, so keeping yaw explicit is
  lower risk.
- In `billboard`, omit:
  - `width`/`height` only if defaults are accepted and documented; explicit size
    is clearer for authored props.
  - `keepAspectRatio` when `true`.
  - `originNormalized` when `{0.5, 1.0}`.
  - `directional` when `false`.
  - `playing` when `true`.
  - Single `clip` when empty/default clip 0 is desired.
  - Directional clip names that equal `Front`, `Back`, `Left`, `Right`.

Validation rules:

- IDs remain positive and unique.
- `kind` must be known; unknown kinds should load as skipped objects with
  diagnostics only if the project wants forward compatibility. Current
  validation rejects malformed fields, so this is a policy decision.
- `spriteAnimationPath` must be non-empty for a renderable billboard. Missing or
  invalid files should not crash; runtime spawn should request the asset and
  renderer should skip while failed/missing.
- Width and height must be finite. Clamp or reject non-positive values. For
  authored data, rejecting on load is stricter; for editor UI, clamp to a small
  positive minimum.
- `originNormalized` must contain finite values. Recommended editor range is
  `[0, 1]` for both axes. Loading can either clamp or reject out-of-range values;
  rejecting is easier to diagnose.
- Missing clip names are allowed but should warn and fall back to clip 0 or
  `Default` if available.
- Non-finite position/yaw/size/origin values must be rejected.

Backward compatibility for existing goblins:

- Keep `FindSectorRuntimeObjectDefinition("goblin")` for at least one migration
  phase.
- Read old objects as generic billboards with goblin defaults at runtime.
- Keep existing maps under `assets/levels` loading without edits.
- Update tests to cover old `definitionId: "goblin"` load/spawn and new
  `kind: "billboard"` load/spawn.

## 7. Aspect Ratio And Anchor Rules

Aspect ratio should be derived from selected animation frame metadata:

- Prefer `SpriteFrame::sourceSize` from the selected animation's first valid
  frame or selected clip's first frame.
- Fallback to `SpriteFrame::source.width / source.height`.
- All frames in a selected Aseprite animation are expected to have the same
  frame size. Still validate during picker metadata parsing or asset readiness:
  if frame sizes differ, show a warning and derive aspect from the first frame.

Keep-aspect behavior:

- If keep aspect ratio is enabled and width changes, update height to
  `width / aspect`.
- If keep aspect ratio is enabled and height changes, update width to
  `height * aspect`.
- If no valid sprite metadata is available yet, preserve the other dimension and
  show a muted/diagnostic status such as "Aspect unavailable until sprite loads".
- If keep aspect is disabled, width and height are independent.

Anchor/origin:

- Default to bottom-center: `{0.5f, 1.0f}`.
- Store normalized origin as authored data.
- Recommended UI range is `0..1` for both X and Y.
- The existing quad builder interprets origin Y with top/bottom semantics such
  that `{0.5, 1.0}` places the object position at the bottom center. Preserve
  this behavior.

World-unit pitfalls:

- Authored position is stored in sector authoring units and converted through
  `SectorAuthoringToWorldDistance()` at spawn.
- Current `SectorRuntimeObjectBillboardDefinition::sizeWorld` is already
  runtime/world units. The new saved `width`/`height` should be explicitly
  defined as world units, not authoring height units.
- Avoid applying `SectorAuthoringToWorldDistance()` twice to billboard size.
- Inspector copy should say width/height are world units if any label ambiguity
  appears later.

## 8. Proposed Runner Plan Inputs

Future runner phases should read this whole audit, especially sections 3, 4,
6, and 7. Section 3 contains the ECS source-of-truth boundary. Section 4
contains the playback support status. Section 6 contains the recommended saved
schema and compatibility strategy. Section 7 contains aspect/anchor rules.

### Phase 1: Data Model + Serialization/Backward Compatibility

Likely files/functions:

- `sources/sector_demo/SectorTopologyMap.h`
- `sources/sector_demo/SectorTopologySerialization.cpp`
- `tests/SectorTopologySerializationTests.cpp`
- `tests/SectorAuthoringGraphTests.cpp`
- `tests/SectorRuntimeObjectTests.cpp`

Work:

- Add a generic billboard payload to placed runtime objects.
- Read old `definitionId: "goblin"` objects and synthesize goblin billboard
  defaults.
- Read/write new `kind: "billboard"` plus `billboard` object.
- Preserve existing old-map load tests.

Main risks/goblins:

- Breaking old level files that only have `definitionId`.
- Accidentally changing save ordering or default omission style broadly.
- Confusing authored units with runtime world units.

Test ideas:

- Old goblin JSON loads and spawns as before.
- New billboard JSON round-trips with default omissions.
- Invalid non-finite sizes/origins are rejected.
- Missing clips remain loadable but report runtime diagnostics.

### Phase 2: Runtime Spawn/Render Support For Generic Billboards

Likely files/functions:

- `SectorRuntimeObjects.h/.cpp`
- `SectorMeshPreview.cpp`
- `tests/SectorRuntimeObjectTests.cpp`

Work:

- Spawn generic billboard data from `SectorPlacedRuntimeObject`.
- Add a single-clip name resolution path, probably with a small ECS component
  for pending single clip name or by extending billboard clip state.
- Keep directional clip path unchanged for directional billboards.
- Keep `SectorMeshPreview` renderer-only.

Main risks/goblins:

- Adding mutable animation state to `SectorRuntimeObjectState`.
- Requesting sprite assets from render/draw.
- Duplicating current frame state between ECS and renderer cache fields.

Test ideas:

- Generic single-clip billboard spawns with `SectorBillboardSprite` and
  `SectorBillboardAnimator`.
- Generic directional billboard resolves front/back/left/right.
- Missing asset fails safely and does not crash draw/update.
- Authoring refresh replaces mapped ECS entity without duplicate entities.

### Phase 3: Aseprite Playback Gaps If Any

Likely files/functions:

- `SectorMeshPreview.cpp` playback helpers, or extracted shared helper.
- `sources/engine/systems/SpriteAnimatorSystem.cpp`
- `sources/engine/assets/SpriteAnimationAssets.cpp`
- `tests/SectorRuntimeObjectTests.cpp` or a focused animation test file.

Work:

- Only if needed, clarify repeat/once semantics.
- Prefer a shared helper for playback frame resolution so sector billboards and
  2D sprite animator do not diverge.
- Preserve reverse/pingpong and frame duration behavior.

Main risks/goblins:

- Regressing existing goblin animation timing.
- Implementing editor UI for timing modes despite v1 not needing it.

Test ideas:

- Variable frame durations select expected frames.
- Reverse clip order works.
- Pingpong sequence works.
- Paused animator holds time/frame.

### Phase 4: Sprite Scanner/Picker Modal

Likely files/functions:

- `SectorEditorTypes.h`
- `SectorEditorTextureModals.h/.cpp` or a sibling sprite modal file.
- `SectorEditorTextureActions.cpp` or a sibling sprite action file.
- `SectorEditorHelpers.h/.cpp`
- `docs/ui.md` for widget usage expectations.

Work:

- Add modal state and scanner for `assets/sprites/**/*.json`.
- Parse candidate JSON enough to list valid Aseprite animations and discovered
  clip names.
- Preview atlas image from `meta.image` through an editor preview asset scope.
- Do not implement live playing preview for v1.

Main risks/goblins:

- Loading textures directly instead of using `AssetManager`.
- Re-scanning/parsing every frame.
- Leaving preview scopes alive after modal close.

Test ideas:

- Scanner finds nested JSON files under a temporary sprites fixture.
- Candidate validation rejects JSON without `frames` or `meta.image`.
- Relative `meta.image` resolves next to the JSON file.
- Clip discovery reads `meta.frameTags` and fallback naming.

### Phase 5: Editor Billboard Tool + Inspector

Likely files/functions:

- `SectorEditorTypes.h`
- `SectorEditorHelpers.cpp`
- `SectorEditor.cpp`
- `SectorEditorTopologyRenderCache.cpp`
- `tests/SectorAuthoringGraphTests.cpp`
- `tests/SectorEditorUiLayoutTests.cpp`

Work:

- Rename user-facing tool to `Billboard`.
- Placement creates a generic billboard object with default bottom-center
  anchor.
- Inspector exposes Object ID, position, yaw, width, height, keep aspect,
  origin, sprite picker, playing, single/directional mode, clip fields, and
  Delete.
- Mutations must call `MarkTopologyDocumentEdited()` and refresh runtime
  objects.
- Any live topology or visible 2D cache mutation must invalidate or update the
  topology render cache. Over-invalidation is acceptable.

Main risks/goblins:

- Inspector becoming a broad `SectorEditor.cpp` refactor.
- Missed cache invalidation after direct `state.topologyMap.runtimeObjects`
  edits.
- UI states not reset on selection changes.
- Width/height aspect updates fighting focused numeric input behavior.

Test ideas:

- Tool label/help says Billboard and not goblin.
- Placement writes generic billboard data.
- Inspector mutations mark document dirty and refresh runtime objects.
- Delete removes authored object and clears selection/drag state.
- Cache invalidation or direct cache update remains correct for drag preview and
  committed edits.

### Phase 6: Tests/Docs/Cleanup

Likely files/functions:

- `docs/sector_editor.md`
- `docs/plans/*` only if a future implementation plan is maintained there.
- Focused tests listed above.

Work:

- Update runtime object/billboard docs.
- Remove or quarantine obsolete goblin-only test expectations.
- Keep legacy goblin compatibility tests.
- Run standard checks.

Main risks/goblins:

- Removing legacy compatibility too early.
- Claiming GUI verification without doing it.

Test ideas:

- Full `sector_runtime_object` tests.
- Serialization tests.
- Authoring graph/editor action tests for placement, save/load, and picker
  helper behavior.

## 9. Non-Goals / Guardrails

- No NPC AI.
- No collision/physics.
- No scripting.
- No doors/lifts.
- No 3D models.
- No transparent alpha-blended sprites; billboards remain cutout/alpha-tested.
- No new ECS world ownership.
- No runtime object lifecycle ownership in `SectorMeshPreview`.
- No mutable runtime transform/animation shadow copies in
  `SectorRuntimeObjectState`.
- No asset requests in render loops.
- No broad `SectorEditor.cpp` split during the feature unless explicitly scoped
  as cleanup.
- No fragile xdotool/GUI automation tests.

## 10. Open Questions / Recommendations

Decisions needing human review:

- Should new saves use only `kind: "billboard"`, or should they temporarily
  also write `definitionId: "billboard"` for transition readability?
- Should unchanged old `definitionId: "goblin"` objects be preserved in old
  format on save, or migrated to generic billboard format during the first save?
- Should load validation reject invalid billboard paths, or allow them and rely
  on runtime diagnostics? Recommendation: allow missing files as failed assets
  but require non-empty paths and valid JSON field types.
- Should origin overrides be clamped on load or rejected when outside `0..1`?
  Recommendation: reject in serialization tests and clamp in editor UI.
- Should `repeat` and `once` Aseprite semantics be finished before the editor
  tool ships? Recommendation: not required for v1 unless current assets depend
  on repeat counts.

Recommended v1 defaults:

- Tool label: `Billboard`.
- `kind: "billboard"` for new objects.
- Bottom-center origin `{0.5, 1.0}`.
- `keepAspectRatio = true`.
- `playing = true`.
- `directional = false` for generic scenery unless the user chooses directional.
- Empty single clip means first clip/default clip.
- Directional defaults remain `Front`, `Back`, `Left`, `Right`.
- Width/height default to `{1.0, 1.0}` until sprite metadata is selected, then
  keep height and derive width from aspect or use a project-approved default
  height.

Architectural concerns found:

- The current runtime object architecture is sound for this feature: authored
  data is saved on `SectorTopologyMap`; ECS owns mutable runtime state;
  `SectorRuntimeObjectState` is bookkeeping; `SectorMeshPreview` renders only.
- The main implementation risk is schema transition, not renderer capability.
  The renderer already supports size, normalized origin, directional selection,
  frame durations, reverse playback, pingpong playback, cutout rendering, baked
  object-probe lighting, and dynamic light receiving.
- The biggest support gap is authoring per-object billboard asset/clip/size
  data. The second gap is a single-clip name resolution path.
- Cache invalidation must be kept explicit for every authored object mutation.
  Existing placement/delete/inspector edits call `MarkTopologyDocumentEdited()`;
  drag previews directly update cached draw data and committed movement marks
  the document edited. Future billboard field edits that affect visible 2D
  markers should use the same invalidation/update policy.
- Lightmap source-hash behavior should remain unchanged for v1. Generic
  billboards are runtime ECS objects rendered in preview; they are not baked
  lightmap receivers/occluders in the topology lightmap hash.
- Collision, sector lookup, and physics should remain unchanged except for the
  existing object current-sector lookup used for lighting probes.
