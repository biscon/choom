# Sector Billboard Authoring Plan

## How To Use This Plan

This is a living execution plan for replacing the temporary goblin-only runtime object tool with a real generic `Billboard` authoring tool.

When an agent is asked to execute this plan, it must:

1. Read this section first.
2. Read `docs/audit/sector_billboard_authoring_audit.md`, especially sections 3, 4, 6, and 7.
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

Important product decision:

The old hardcoded `goblin` runtime-object definition/tool was demo scaffolding. Do **not** preserve it as a user-facing feature. Do **not** add schema migration that converts old `definitionId: "goblin"` objects into functional goblin billboard objects. New authored objects should use the generic billboard path.

```plan-state-json id="sector-billboard-authoring"
{
  "plan_id": "sector_billboard_authoring_plan",
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
    { "id": "phase_01", "title": "Billboard Data Model And Serialization", "type": "phase", "status": "Completed" },
    { "id": "phase_01a", "title": "Add Generic Billboard Payload And JSON Round Trip", "type": "pass", "parent": "phase_01", "status": "Completed" },
    { "id": "phase_01b", "title": "Remove Goblin Definition Dependency From Authored Data", "type": "pass", "parent": "phase_01", "status": "Completed" },
    { "id": "phase_02", "title": "Generic Billboard Runtime Spawn And Playback", "type": "phase", "status": "Not Started" },
    { "id": "phase_02a", "title": "Spawn Generic Billboard Objects Into ECS", "type": "pass", "parent": "phase_02", "status": "Not Started" },
    { "id": "phase_02b", "title": "Add Single Clip Resolution And Playback State", "type": "pass", "parent": "phase_02", "status": "Not Started" },
    { "id": "phase_03", "title": "Sprite Asset Scanner And Picker Modal", "type": "phase", "status": "Not Started" },
    { "id": "phase_03a", "title": "Add Sprite Metadata Scanner", "type": "pass", "parent": "phase_03", "status": "Not Started" },
    { "id": "phase_03b", "title": "Add Sprite Picker Modal With Atlas Preview", "type": "pass", "parent": "phase_03", "status": "Not Started" },
    { "id": "phase_04", "title": "Editor Billboard Tool And Inspector", "type": "phase", "status": "Not Started" },
    { "id": "phase_04a", "title": "Replace Object Tool With Billboard Placement Tool", "type": "pass", "parent": "phase_04", "status": "Not Started" },
    { "id": "phase_04b", "title": "Add Billboard Inspector Fields And Aspect Ratio Editing", "type": "pass", "parent": "phase_04", "status": "Not Started" },
    { "id": "phase_04c", "title": "Wire Sprite Picker And Clip Selection Into Inspector", "type": "pass", "parent": "phase_04", "status": "Not Started" },
    { "id": "phase_05", "title": "Tests Documentation And Cleanup", "type": "phase", "status": "Not Started" },
    { "id": "phase_05a", "title": "Strengthen Tests And Remove Goblin-Only Scaffolding", "type": "pass", "parent": "phase_05", "status": "Not Started" },
    { "id": "phase_05b", "title": "Update Documentation And Close Plan", "type": "pass", "parent": "phase_05", "status": "Not Started" }
  ]
}
```

## Current Progress

| Phase / Pass | Status | Date | Notes |
| --- | --- | --- | --- |
| Phase 1: Billboard Data Model And Serialization | Completed | 2026-07-02 | Phase 1A and Phase 1B complete. Authored runtime-object serialization now uses generic `kind: "billboard"` objects with nested `billboard` payloads; legacy `definitionId`-only authored objects are rejected rather than migrated or synthesized. |
| Phase 1A: Add Generic Billboard Payload And JSON Round Trip | Completed | 2026-07-02 | Source changed. Added `SectorPlacedBillboard`, `SectorPlacedRuntimeObject::kind`, and nested billboard JSON round-trip for `kind: "billboard"`. JSON fields: `billboard.spriteAnimationPath`, `width`, `height`, optional `keepAspectRatio`, `originNormalized`, `directional`, `clip`, `frontClip`, `backClip`, `leftClip`, `rightClip`, `playing`. Defaults restore bottom-center origin, keep-aspect enabled, non-directional, playing, empty single clip, and Front/Back/Left/Right directional clips; default optional fields are omitted on save while width/height remain explicit. Validation rejects unknown new `kind`, missing billboard payload, empty sprite path, non-positive/non-finite size, out-of-range/non-finite origin, non-finite position/yaw, and duplicate/non-positive IDs. Legacy `definitionId` records are still read/written unchanged for Phase 1B; no goblin migration/default synthesis was added. Runtime behavior, editor UI, cache invalidation, asset loading, ECS ownership, and lightmap source hash behavior unchanged. Verification: `cmake --build cmake-build-debug --target sector_topology_serialization_tests -j2`, `ctest --test-dir cmake-build-debug -R '^sector_topology_serialization$' --output-on-failure`, `cmake --build cmake-build-debug -j2`, and `ctest --test-dir cmake-build-debug --output-on-failure` passed; focused serialization build/test was rerun after final test cleanup; `git diff --check` passed; `git diff --stat` and `git status --short` reviewed. |
| Phase 1B: Remove Goblin Definition Dependency From Authored Data | Completed | 2026-07-02 | Source changed. Chosen old-object policy: legacy `definitionId`-only runtime objects, including `definitionId: "goblin"`, are rejected during load/save validation with no goblin migration/default synthesis. Serialization and editor-document tests now use generic `kind: "billboard"` fixtures with `billboard.spriteAnimationPath`; 2D cached runtime-object known/display state keys off `kind == "billboard"` instead of `definitionId == "goblin"`. The old click placement helper no longer writes `definitionId: "goblin"` authored data; it reports that billboard placement needs sprite selection, leaving the full placement workflow for Phase 4. Runtime spawning now skips legacy placed objects with non-empty `definitionId` before built-in definition registry lookup, so `definitionId: "goblin"` does not spawn a hardcoded object or request sprite animation assets; generic `kind: "billboard"` spawning remains deferred to Phase 2. The existing user-facing RuntimeObject/Object tool text and goblin-specific inspector copy are intentionally left for Phase 4/5 per the plan. Topology render-cache invalidation behavior is unchanged; no topology mutation path was changed. Lightmap source-hash behavior is unchanged; runtime billboard authored data remains outside baked lightmap topology hashing. Verification: `cmake --build cmake-build-debug --target sector_topology_serialization_tests -j2`, `cmake --build cmake-build-debug --target sector_authoring_graph_tests -j2`, `cmake --build cmake-build-debug --target sector_runtime_object_tests -j2`, `ctest --test-dir cmake-build-debug -R '^sector_topology_serialization$' --output-on-failure`, `ctest --test-dir cmake-build-debug -R '^sector_authoring_graph$' --output-on-failure`, `ctest --test-dir cmake-build-debug -R '^sector_runtime_object$' --output-on-failure`, `cmake --build cmake-build-debug -j2`, and `ctest --test-dir cmake-build-debug --output-on-failure` passed. |
| Phase 2: Generic Billboard Runtime Spawn And Playback | Not Started |  | Parent phase. |
| Phase 2A: Spawn Generic Billboard Objects Into ECS | Not Started |  | Runtime spawn path for `kind: "billboard"`. |
| Phase 2B: Add Single Clip Resolution And Playback State | Not Started |  | Single-clip path plus playing/paused authored default. |
| Phase 3: Sprite Asset Scanner And Picker Modal | Not Started |  | Parent phase. |
| Phase 3A: Add Sprite Metadata Scanner | Not Started |  | Scan `assets/sprites/**/*.json` and read metadata/clip names. |
| Phase 3B: Add Sprite Picker Modal With Atlas Preview | Not Started |  | Modal like texture picker; atlas preview only for v1. |
| Phase 4: Editor Billboard Tool And Inspector | Not Started |  | Parent phase. |
| Phase 4A: Replace Object Tool With Billboard Placement Tool | Not Started |  | User-facing tool becomes Billboard, not Object/goblin. |
| Phase 4B: Add Billboard Inspector Fields And Aspect Ratio Editing | Not Started |  | Width/height, keep aspect, origin, playing, directional mode. |
| Phase 4C: Wire Sprite Picker And Clip Selection Into Inspector | Not Started |  | Asset picker and clip dropdown/list integration. |
| Phase 5: Tests Documentation And Cleanup | Not Started |  | Parent phase. |
| Phase 5A: Strengthen Tests And Remove Goblin-Only Scaffolding | Not Started |  | Clean old demo-only expectations and hardcoded goblin paths. |
| Phase 5B: Update Documentation And Close Plan | Not Started |  | Docs and final plan closure. |

## Execution Tracking Rules

- Each phase/pass must be independently buildable and testable.
- Each phase/pass final report must state whether source code changed.
- Each implementation phase/pass must update this document before finishing.
- The update should be small and local.
- Do not rewrite unrelated phases when marking progress.
- If behavior is intended to remain unchanged, explicitly state that.
- If a phase/pass changes serialization, generated data, public APIs, runtime behavior, cache invalidation, asset loading, ECS ownership, or build/test behavior, clearly say so.
- Do not claim manual GUI verification unless it was actually performed.
- If a phase/pass produces only planning/audit changes and no source changes, state that clearly.
- If a phase/pass is too broad, add smaller child passes under that phase and stop without source changes.

## Goal And Desired End State

Replace the temporary goblin-only runtime object authoring path with a real generic `Billboard` level-scenery tool.

Desired end state:

- The editor has a user-facing `Billboard` placement tool.
- The old user-facing `Object`/goblin placement behavior is gone.
- New placed billboard objects save/load as generic billboard authored data.
- Placed billboard objects spawn into `EngineContext::world`.
- `SectorMeshPreview` remains renderer-only and only renders ECS objects passed to it.
- Billboards use the existing cutout billboard renderer.
- Billboards can be single-clip or directional.
- Directional billboards use Front/Back/Left/Right clip names.
- Single billboards use one selected clip.
- Aseprite frame durations, reverse playback, and pingpong playback remain respected by the runtime path.
- Inspector exposes position, yaw, width, height, keep-aspect, normalized origin, sprite asset, playing/paused, directional mode, clip selection, and delete.
- Sprite picker scans `assets/sprites` recursively for Aseprite JSON files and can preview the atlas PNG referenced by `meta.image`.
- Bottom-center origin `{0.5, 1.0}` is the default.
- Width and height are world units.
- No NPC AI, scripting, combat, collision, doors/lifts, 3D models, or transparent alpha-blended sprites are added.

## Audit Dependency

Future runs should consult:

```text
docs/audit/sector_billboard_authoring_audit.md
```

Especially:

- Section 3 for billboard ECS components, renderer behavior, and ECS source-of-truth boundaries.
- Section 4 for Aseprite parser/playback support and known gaps.
- Section 5 for sprite scanner/picker feasibility and UI components.
- Section 6 for proposed billboard data model.
- Section 7 for aspect ratio and anchor rules.

This plan intentionally overrides the audit's suggested goblin compatibility/migration path. The hardcoded goblin feature was a demo and should not be kept alive as a first-class or migrated feature.

## Architecture And Ownership Rules

- `EngineContext::world` is the authoritative ECS world for runtime objects.
- `SectorTopologyMap::runtimeObjects` is authored level data.
- ECS components are mutable runtime state.
- `SectorRuntimeObjectState` may keep bookkeeping: placed-object ID to ECS entity mapping, asset scope, diagnostics, object probe runtime data, and sector lookup helpers.
- `SectorRuntimeObjectState` must not store shadow copies of mutable runtime transform or animation state.
- `SectorMeshPreview` may render ECS billboards passed to it.
- `SectorMeshPreview` must not own `engine::World`, spawn/despawn runtime object entities, reset runtime object lifecycle, or request sprite assets during render.
- Asset requests happen during explicit spawn/setup/selection changes, not every frame.
- ECS components should store asset handles/IDs, not raw raylib texture/font/sprite pointers.
- Missing/not-ready/failed assets must not crash.
- Billboards remain cutout/alpha-tested. No true transparent render pass in this plan.
- Lightmap source hash behavior should remain unchanged. Runtime billboards are not baked lightmap topology.

## Proposed Billboard Data Model

Use project style and adjust names as needed, but the intended model is:

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
    std::string kind; // "billboard" for new authored objects
    Vector3 position = {};
    float yawRadians = 0.0f;
    SectorPlacedBillboard billboard;
};
```

Recommended JSON shape:

```json
{
  "id": 1,
  "kind": "billboard",
  "position": [8.0, 0.0, 6.0],
  "yawDegrees": 0.0,
  "billboard": {
    "spriteAnimationPath": "assets/sprites/torch/torch.json",
    "width": 1.0,
    "height": 2.0,
    "keepAspectRatio": true,
    "originNormalized": [0.5, 1.0],
    "directional": false,
    "clip": "Idle",
    "playing": true
  }
}
```

Directional JSON shape:

```json
{
  "id": 2,
  "kind": "billboard",
  "position": [10.0, 0.0, 6.0],
  "yawDegrees": 90.0,
  "billboard": {
    "spriteAnimationPath": "assets/sprites/goblin.json",
    "width": 0.8,
    "height": 1.2,
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

Serialization policy:

- New authored objects use `kind: "billboard"`.
- Do not write new goblin-specific object definitions.
- Do not synthesize old `definitionId: "goblin"` into a functional new billboard.
- If existing repo fixtures or sample levels contain demo goblin objects, update those fixtures to generic billboards or remove the objects.
- If old external maps contain `definitionId: "goblin"` without `kind`, it is acceptable for them to fail validation or load as unsupported/skipped with diagnostics. The chosen policy must be explicit and tested.
- Do not keep a hardcoded goblin image/path feature merely for migration.

Recommended defaults:

- `kind = "billboard"`
- `originNormalized = {0.5f, 1.0f}`
- `keepAspectRatio = true`
- `playing = true`
- `directional = false`
- Empty single `clip` means first/default clip.
- Directional clip defaults are `Front`, `Back`, `Left`, `Right`.
- Width/height default to `{1.0f, 1.0f}` until a sprite is selected.

Validation:

- IDs are positive and unique.
- `kind` must be known for renderable objects.
- `spriteAnimationPath` must be non-empty for a renderable billboard.
- Missing/failed asset files must not crash runtime; use diagnostics.
- Width/height must be finite and positive.
- Position/yaw values must be finite.
- `originNormalized` components must be finite and in `0..1` when loaded from authored data.
- Missing clips should warn and fall back to first/default clip rather than crash.
- Unknown object kinds may be rejected or skipped with diagnostics; pick one policy and test it.

## Aspect Ratio And Anchor Rules

- Billboard anchor defaults to bottom-center `{0.5f, 1.0f}`.
- Store origin/anchor as normalized coordinates on the authored billboard payload.
- Inspector should allow normalized origin override, clamped to `0..1`.
- Width and height are world units.
- Do not apply authoring-to-world distance conversion twice to width/height.
- Derive aspect ratio from selected animation frame metadata:
  - Prefer selected clip's first frame source size.
  - Fallback to first frame source size.
  - Fallback to source rectangle dimensions.
- All frames in a selected Aseprite animation are expected to have the same frame size.
- If frame sizes differ, show/report a warning and derive aspect from the first valid frame.
- With keep-aspect enabled:
  - editing width updates height to `width / aspect`
  - editing height updates width to `height * aspect`
- With keep-aspect disabled, width and height edit independently.
- If aspect metadata is unavailable, do not fight the numeric input; leave the other dimension unchanged and show/report that aspect is unavailable until sprite metadata loads.

## Proposed Phases

### Phase 1: Billboard Data Model And Serialization

Goal:

Add generic authored billboard data and JSON round-trip support without changing runtime rendering or editor UI yet.

Why it helps:

This replaces the demo-only goblin object schema with real generic object data before wiring runtime/editor features.

Files/functions likely touched:

- `sources/sector_demo/SectorTopologyMap.h`
- `sources/sector_demo/SectorTopologySerialization.cpp`
- runtime object tests
- topology serialization tests
- sample/test level fixtures if they contain goblin runtime objects

Exact behavior that must remain unchanged:

- Maps without `runtimeObjects` still load as empty.
- Existing non-object topology serialization remains unchanged.
- Renderer behavior remains unchanged until Phase 2.
- `EngineContext::world` ownership remains unchanged.
- Lightmap source hash remains unchanged.

Risks/goblins:

- Accidentally preserving hardcoded goblin as a migrated feature.
- Breaking maps with no objects.
- Confusing `kind` with old `definitionId`.
- Applying authoring/world unit conversions to billboard width/height at serialization time.
- Over-broad schema rewrite.

Non-goals:

- No sprite picker.
- No editor inspector changes beyond what tests/fixtures require.
- No runtime spawn for new billboard payload yet.
- No Aseprite playback changes yet.
- No NPC/collision/scripting/doors.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant serialization/runtime object tests.

Final report expectations:

- State the exact C++ data types/fields added or changed.
- State the exact JSON field names.
- State missing/default behavior.
- State validation behavior.
- State the chosen policy for old `definitionId: "goblin"` objects.
- Confirm no goblin migration/default synthesis was added.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 1 `Completed` only after Phase 1A and Phase 1B are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 1A: Add Generic Billboard Payload And JSON Round Trip

Goal:

Add the generic `kind: "billboard"` authored data model and round-trip serialization.

Implementation guidance:

- Add a placed billboard payload to the existing placed runtime object data.
- Use `kind: "billboard"` for new generic billboard runtime objects.
- Read/write a nested `billboard` JSON object.
- Keep position/yaw behavior compatible with existing placed objects.
- Use explicit `width` and `height` fields in the JSON payload.
- Keep bottom-center origin as default.
- Keep default omission style consistent with the project.
- Validate finite positive width/height, finite origin in `0..1`, finite position/yaw, positive unique IDs, and non-empty sprite path for renderable billboards.
- Do not add runtime spawn support in this pass.

Suggested tests:

- Missing `runtimeObjects` loads as empty.
- New `kind: "billboard"` object round-trips.
- Defaults omit/restore correctly.
- Directional billboard fields round-trip.
- Single clip billboard fields round-trip.
- Non-finite/invalid size/origin/path values are rejected.
- Existing maps with no runtime objects still load.

#### Phase 1B: Remove Goblin Definition Dependency From Authored Data

Goal:

Stop treating `goblin` as the authored object feature.

Implementation guidance:

- Remove the built-in goblin object definition from the primary authored-data path.
- Do not synthesize old `definitionId: "goblin"` objects into new billboard payloads.
- If old tests or fixtures rely on `definitionId: "goblin"`, update them to generic `kind: "billboard"` or remove those demo objects.
- Pick and document a simple old-object policy:
  - reject old `definitionId`-only runtime objects during load, or
  - preserve them as unsupported/skipped records with diagnostics but no rendered goblin.
- Do not keep hardcoded goblin defaults solely for compatibility.
- Do not reintroduce the F5 goblin debug spawn path.

Suggested tests:

- New billboard fixtures/tests no longer rely on a hardcoded goblin definition.
- Old `definitionId: "goblin"` does not spawn a hardcoded goblin.
- Runtime object validation/reporting for old `definitionId`-only data follows the chosen policy.
- No user-facing editor labels mention goblin as the placement tool target after later phases; if not changed in this pass, leave a clear TODO/plan note.

### Phase 2: Generic Billboard Runtime Spawn And Playback

Goal:

Spawn generic authored billboard objects into ECS and render them through the existing sector billboard renderer.

Why it helps:

This connects the new generic data model to visible runtime billboards.

Files/functions likely touched:

- `sources/sector_demo/SectorRuntimeObjects.h`
- `sources/sector_demo/SectorRuntimeObjects.cpp`
- `sources/sector_demo/SectorMeshPreview.cpp` only for renderer-only draw/stat gaps if necessary
- runtime object tests

Exact behavior that must remain unchanged:

- `EngineContext::world` remains authoritative.
- `SectorMeshPreview` remains renderer-only.
- Runtime reset/refresh lifecycle remains explicit.
- Asset requests happen during spawn/setup/selection changes, not render loops.
- Directional billboard rendering remains camera-relative.
- Existing dynamic lighting/probe lighting behavior remains unchanged.

Risks/goblins:

- Adding mutable transform/animation shadow state outside ECS.
- Requesting sprite assets from draw/update loops.
- Duplicating spawned entities during authoring refresh or preview rebuild.
- Treating pending assets as failed.
- Breaking directional clip selection while adding single-clip support.
- Incorrect authoring-to-world conversion for position or size.

Non-goals:

- No sprite picker.
- No editor UI beyond whatever is required to keep existing tools compiling.
- No new animation authoring UI.
- No repeat/once semantic overhaul unless tests prove a blocker.
- No collision/NPC/gameplay behavior.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run runtime object tests and relevant full test suite.

Final report expectations:

- State how generic billboards spawn into ECS.
- State which ECS components hold size/origin/clip/playback state.
- State how single clip names are resolved.
- State how directional clip names are resolved.
- State asset request lifecycle.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 2 `Completed` only after Phase 2A and Phase 2B are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 2A: Spawn Generic Billboard Objects Into ECS

Goal:

Spawn `kind: "billboard"` placed objects into ECS using the authored billboard payload.

Implementation guidance:

- Update `SpawnPlacedRuntimeObjects()` or equivalent spawn helper to handle generic billboards.
- Request `billboard.spriteAnimationPath` through `AssetManager::RequestSpriteAnimation()` using the runtime object asset scope.
- Populate ECS:
  - `SectorObjectTransform`
  - `SectorObject`
  - `SectorObjectLighting`
  - `SectorBillboardSprite`
  - `SectorBillboardAnimator`
  - optional `SectorBillboardDirectionalClips` only when directional is true
- Apply authored width/height directly as world units.
- Apply authored `originNormalized`.
- Apply authored `playing`.
- Convert authored position to world position using the existing placed-object conversion path, but do not convert size twice.
- Keep placed-object ID to entity mapping as bookkeeping only.
- Record diagnostics for missing/unsupported object kind, missing/failed/pending sprite assets, and missing clips where possible.

Suggested tests:

- One generic billboard spawns exactly one ECS entity.
- Width/height/origin/playing are copied to ECS components.
- Authoring refresh replaces the mapped ECS entity without duplicates.
- Reset clears only `SectorObject` entities and unloads object asset scope.
- Missing sprite asset does not crash and reports diagnostics.

#### Phase 2B: Add Single Clip Resolution And Playback State

Goal:

Add the non-directional billboard clip path and ensure animation playback uses existing Aseprite timing/modes.

Implementation guidance:

- For non-directional billboards, store the authored single clip name in ECS or a small ECS component until it can be resolved after asset load.
- Do not store mutable animation state in `SectorRuntimeObjectState`.
- Resolve single clip names when the sprite animation asset is ready.
- Empty/missing clip should fall back to first/default clip with diagnostics.
- Preserve current directional clip behavior.
- Confirm existing sector billboard playback respects frame durations, reverse, and pingpong as described in the audit.
- Do not implement full Aseprite `repeat`/`once` semantics unless a small existing helper already supports it and tests are cheap.

Suggested tests:

- Single clip name flows into spawned ECS state and resolves to expected clip index.
- Empty clip falls back safely.
- Missing clip warns/falls back safely.
- Directional front/back/left/right still resolve.
- Paused billboard does not advance animator time.
- Playing billboard advances animator time.
- Variable frame duration/reverse/pingpong behavior remains covered if helpers are testable.

### Phase 3: Sprite Asset Scanner And Picker Modal

Goal:

Add sprite asset discovery and a v1 modal picker for Aseprite animations.

Why it helps:

A generic Billboard tool is not useful unless the user can choose a sprite asset and clip.

Files/functions likely touched:

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorHelpers.h/.cpp`
- `sources/sector_editor/SectorEditorTextureModals.h/.cpp` or new sibling sprite modal files
- editor action/helper files near existing texture picker code
- tests for scanner/helper logic
- `docs/ui.md` only if new UI patterns are added, but prefer existing components

Exact behavior that must remain unchanged:

- Existing texture picker/import behavior remains unchanged.
- UI uses existing immediate UI components.
- AssetManager owns preview textures.
- Missing/not-ready preview assets render placeholders.
- No live animation preview required.

Risks/goblins:

- Re-scanning/parsing every frame.
- Loading raylib textures directly instead of using `AssetManager`.
- Leaking preview asset scopes.
- Opening dropdown overlays inside clipped scroll areas if the current UI cannot support that.
- Tying picker metadata to runtime object mutable state.

Non-goals:

- No live animated preview.
- No editing Aseprite timing/modes.
- No object browser beyond sprite JSON selection.
- No new UI framework.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run scanner/helper tests and existing editor/runtime tests.

Final report expectations:

- State scanner helper names and behavior.
- State candidate validation rules.
- State how clip names are discovered.
- State how atlas preview path is resolved/requested.
- State preview scope lifecycle.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 3 `Completed` only after Phase 3A and Phase 3B are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 3A: Add Sprite Metadata Scanner

Goal:

Scan `assets/sprites` recursively and extract cheap picker metadata from Aseprite JSON files.

Implementation guidance:

- Follow the pattern of `ScanAssetImagePngs()` and texture import helpers.
- Recursively scan `ASSETS_PATH/sprites`.
- Include `.json` files only.
- Normalize returned paths to project asset paths like `assets/sprites/...json`.
- Parse JSON cheaply enough to validate object root, `frames` object, `meta` object, and non-empty `meta.image`.
- Discover clip names from `meta.frameTags[*].name`; fallback to existing frame-name derived clips where practical; fallback to `Default` when no clips exist.
- Resolve `meta.image` relative to the JSON file path for metadata.
- Do not request textures or sprite animation assets during scanner tests unless needed.

Suggested tests:

- Finds nested sprite JSON files in a fixture/temp tree if existing test style supports it.
- Rejects JSON without `frames`.
- Rejects JSON without `meta.image`.
- Resolves relative `meta.image`.
- Discovers clip names from `meta.frameTags`.
- Provides `Default` fallback when needed.

#### Phase 3B: Add Sprite Picker Modal With Atlas Preview

Goal:

Add a modal picker for selecting an Aseprite JSON and clip metadata, with atlas PNG preview.

Implementation guidance:

- Build the modal after the existing texture picker/add-texture modal pattern.
- Use existing immediate UI components from `docs/ui.md`: panels/property rows, buttons, lists or scroll areas, and image preview.
- Keep modal state caller-owned in editor state.
- Scan/refresh on modal open or explicit refresh button, not every frame.
- Selecting an item updates selected path/metadata and requests atlas preview through `AssetManager::RequestTexture()`.
- Use a modal preview asset scope and unload it when closing or changing selection, following texture picker preview patterns.
- The modal should return/commit selected sprite animation JSON path, available clip names, and chosen clip if the modal includes clip selection in this phase.
- Atlas preview only is acceptable. No playing animation preview.

Suggested tests/manual smoke:

- Scanner list appears for existing `assets/sprites` JSON files.
- Selecting a sprite requests/shows atlas preview or placeholder.
- Closing modal unloads preview scope.
- No crashes when selected asset is pending/failed.
- No fragile GUI automation; report manual smoke as not performed unless actually done.

### Phase 4: Editor Billboard Tool And Inspector

Goal:

Replace the goblin-only editor workflow with a real `Billboard` placement and inspector workflow.

Why it helps:

This makes the feature usable as authored level scenery.

Files/functions likely touched:

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorHelpers.cpp`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorTopologyRenderCache.cpp`
- sprite modal/action files from Phase 3
- editor/action tests where available

Exact behavior that must remain unchanged:

- Existing sector/line/vertex/face/light tools continue working.
- UI text focus still prevents Delete from propagating.
- Object edits mutate authored placed-object data and refresh ECS preview entities.
- Runtime/gameplay ECS mutation is not written back to authored level data implicitly.
- Topology render cache invalidation remains explicit.
- Save/reload uses authored billboard data.

Risks/goblins:

- Broad `SectorEditor.cpp` refactor.
- Missing dirty flag/cache invalidation after field edits.
- Numeric keep-aspect logic fighting focused `FloatInput()` edit state.
- Selection conflicts with sectors/lights.
- Sprite picker state leaking across selected objects.
- Accidentally leaving user-facing goblin wording/tooling.

Non-goals:

- No undo/redo unless existing edit path provides it.
- No in-world 3D transform gizmo.
- No asset browser beyond sprite picker.
- No NPC tool.
- No dynamic billboard tool.
- No collision/gameplay fields.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant editor/runtime/serialization tests.

Final report expectations:

- State user-facing tool label/help/status text.
- State placement defaults.
- State inspector fields.
- State keep-aspect behavior.
- State sprite picker/clip selection behavior.
- State dirty/cache/runtime refresh behavior.
- State manual GUI smoke status.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 4 `Completed` only after Phase 4A, 4B, and 4C are complete.
- Add date, summary, verification results, and behavior notes.

#### Phase 4A: Replace Object Tool With Billboard Placement Tool

Goal:

Change the user-facing tool from goblin/object placement to generic billboard placement.

Implementation guidance:

- User-facing name should be `Billboard`.
- Help/status text should describe placing billboards, not goblins.
- Placement should create `kind: "billboard"` authored data with default bottom-center origin, width/height `{1.0f, 1.0f}`, keep aspect true, playing true, directional false, and empty sprite path unless a safe project-approved default has already been selected.
- If sprite path is empty, object should draw as a 2D marker and runtime should report missing sprite path; it must not crash.
- Select the new object after placement.
- Mark document dirty and refresh runtime objects.
- 2D overlay should draw billboard markers and yaw tick/arrow without goblin-specific known-definition logic.
- Remove or stop using `definitionId = "goblin"` in placement.

Suggested tests/manual smoke:

- Tool label/help says Billboard.
- Placement creates `kind: "billboard"` object.
- Placement does not use hardcoded goblin definition.
- Empty sprite path gives safe diagnostic/skipped render.
- Marker appears in 2D and is selectable.
- Manual smoke not claimed unless actually performed.

#### Phase 4B: Add Billboard Inspector Fields And Aspect Ratio Editing

Goal:

Add core billboard property editing in the inspector.

Inspector fields:

```text
Object ID
Position X/Y/Z
Yaw
Width
Height
Keep aspect ratio
Origin X
Origin Y
Directional
Playing
Delete
```

Implementation guidance:

- Use existing immediate UI components documented in `docs/ui.md`.
- Use caller-owned numeric input states.
- Width/height are world units and clamped to a small positive minimum in UI.
- Origin X/Y are normalized and clamped to `0..1`.
- Keep-aspect behavior:
  - if width commit changes and aspect is available, update height
  - if height commit changes and aspect is available, update width
  - do not fight text editing while the field is focused
  - if no sprite metadata is available, leave the paired dimension unchanged and show/report a small status
- Aspect ratio should come from selected sprite metadata when available.
- Mutations must edit authored placed-object data, mark document dirty, invalidate/update render cache as needed, and refresh runtime objects.
- Delete removes authored object, clears selection/drag state, and clears mapped runtime entity through existing refresh/reset behavior.
- Delete key must still respect UI text focus behavior.

Suggested tests/manual smoke:

- Width/height edit persists through save/load.
- Keep-aspect updates paired dimension on committed width/height changes.
- Disabling keep-aspect allows stretch.
- Origin edits persist and affect spawned ECS origin.
- Playing checkbox persists and affects animator playing state.
- Delete removes object and runtime entity.
- Manual GUI smoke not claimed unless actually performed.

#### Phase 4C: Wire Sprite Picker And Clip Selection Into Inspector

Goal:

Connect the sprite picker and clip selection UI to authored billboard data.

Implementation guidance:

- Add a button/row to open sprite picker for the selected billboard.
- On selection commit, set `billboard.spriteAnimationPath`, refresh cached sprite metadata/clip list, choose sensible defaults for empty/invalid clip fields, update aspect ratio if keep-aspect is enabled and metadata is available, mark document dirty, and refresh runtime objects.
- Single mode: show one clip selector/list/dropdown and store `billboard.clip`.
- Directional mode: show Front, Back, Left, Right clip selectors and store corresponding fields.
- Directional defaults are `Front`, `Back`, `Left`, `Right` if available.
- Missing directional clips should warn/fallback, not crash.
- If `Option()` dropdowns are unsuitable inside existing inspector scroll areas, use a modal/list selection pattern rather than inventing a new UI system.
- Do not expose Aseprite reverse/pingpong/frame-time settings in the inspector.

Suggested tests/manual smoke:

- Picking a sprite writes the asset path.
- Clip selection writes the chosen clip names.
- Single/directional mode toggles persist through save/load.
- Missing clip diagnostics show but rendering falls back safely.
- Sprite picker preview works or shows placeholder.
- Manual smoke not claimed unless actually performed.

### Phase 5: Tests Documentation And Cleanup

Goal:

Close the feature, remove goblin-only scaffolding, and document the new Billboard workflow.

Why it helps:

This makes the generic billboard system the supported editor feature and avoids future Codex runs keeping the demo goblin path alive.

Files/functions likely touched:

- tests
- `docs/sector_editor.md`
- this plan document
- any old goblin-only docs/tests/status text

Exact behavior that must remain unchanged:

- No NPC AI.
- No collision.
- No scripting.
- No doors/lifts.
- No 3D models.
- No transparent alpha-blended sprites.
- `SectorMeshPreview` remains renderer-only.
- Runtime object lifecycle remains explicit.

Risks/goblins:

- Leaving a hidden hardcoded goblin feature in tests/docs.
- Removing useful generic billboard tests while deleting goblin tests.
- Claiming manual smoke without doing it.
- Updating docs but not plan state.

Non-goals:

- No cleanup crusade across unrelated old 2D sprite systems.
- No large editor refactor.
- No asset pipeline redesign.

Suggested checks:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Final report expectations:

- State tests added/updated/removed.
- State docs updated.
- State all goblin-only feature remnants removed or explicitly quarantined as test asset data only.
- State manual smoke checklist for the user.
- State verification results.

How to update this plan after completion:

- Mark completed pass in JSON and table.
- Mark Phase 5 `Completed` only after Phase 5A and Phase 5B are complete.
- If all phases are complete, ensure all parent phases are `Completed`.
- Leave a final completion note.

#### Phase 5A: Strengthen Tests And Remove Goblin-Only Scaffolding

Goal:

Finish test coverage and remove obsolete goblin-only implementation expectations.

Implementation guidance:

- Replace old runtime object tests that rely on hardcoded `goblin` definitions with generic billboard tests.
- Remove user-facing goblin placement/help/status text.
- Remove built-in goblin definition if it is no longer needed by tests/runtime.
- If `assets/sprites/goblin.json` remains, treat it as an ordinary sprite asset fixture, not a hardcoded object type.
- Ensure no F5 debug goblin path is restored.
- Ensure `definitionId: "goblin"` is not a supported authored feature.
- Keep any necessary unsupported/legacy-data tests aligned with the policy chosen in Phase 1B.

Suggested tests:

- Generic billboard serialization.
- Generic billboard spawn into ECS.
- Single clip resolution.
- Directional clip resolution.
- Width/height/origin propagation.
- Keep-aspect helper behavior if testable outside GUI.
- Sprite scanner metadata tests.
- Missing sprite/missing clip diagnostics.
- Reset/refresh no duplicates.
- Delete clears authored object/runtime entity.

#### Phase 5B: Update Documentation And Close Plan

Goal:

Update docs and mark the feature complete.

Implementation guidance:

- Update `docs/sector_editor.md` to document Billboard tool, bottom-center default anchor, width/height world units, keep aspect behavior, sprite picker scanning `assets/sprites`, single vs directional clips, playing/paused checkbox, Aseprite timing/modes respected automatically where currently supported, cutout-only billboards, and no NPC/collision/scripting behavior.
- Mention that goblin is no longer a special editor object type.
- Keep docs honest about manual smoke status.
- Update this plan's progress table and JSON.

Suggested final manual smoke checklist for user:

```text
- Place a Billboard in 2D.
- Pick an Aseprite sprite asset.
- Select a clip and confirm it renders in 3D preview.
- Edit width with keep-aspect enabled and confirm height updates.
- Edit height with keep-aspect enabled and confirm width updates.
- Disable keep-aspect and confirm free stretch works.
- Change origin and confirm placement/anchor changes.
- Toggle playing/paused and confirm animation behavior.
- Toggle directional mode and assign Front/Back/Left/Right clips.
- Save/reload and confirm marker, properties, and rendered billboard persist.
- Delete the billboard and confirm runtime entity disappears.
```

## Deferred Decisions For Later Plans

These are intentionally out of scope:

- NPC placement tool.
- Dynamic gameplay actors.
- Collision/physics for objects.
- Object scripting.
- Doors/lifts.
- Pickups/projectiles.
- Attached lights.
- 3D model/gltf renderer.
- Skeletal animation.
- Live animated preview inside sprite picker.
- Full object definition file format.
- Transparent alpha-blended sprite pass.
- Particles/smoke/spells/glass.
- Save-game runtime ECS persistence.
- Broad `SectorEditor.cpp` refactor.
- Old goblin schema migration.

## Final Completion Criteria

This plan is complete when:

- New authored runtime objects use generic `kind: "billboard"` data.
- The user-facing editor tool is `Billboard`.
- The goblin-only placement feature is removed.
- No hardcoded goblin object definition is required for placing billboards.
- Placed billboards save/load with sprite path, size, origin, clip mode, clip names, playing state, position, and yaw.
- Generic billboards spawn into `EngineContext::world`.
- Billboards render in 3D preview through the existing sector billboard renderer.
- Single clip and directional clip modes work.
- Width/height keep-aspect behavior works from selected sprite metadata.
- Sprite picker scans `assets/sprites` recursively and can show atlas preview.
- Runtime lifecycle remains explicit and does not duplicate entities.
- `SectorMeshPreview` remains renderer-only.
- Tests/docs are updated.
- No NPC/collision/scripting/door/3D-model scope leaked into the implementation.
