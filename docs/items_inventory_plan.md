# Items And Inventory Implementation Plan

## How To Use This Living Plan

This document is the execution plan and compacted investigation record for the
global item registry, placed world items, player inventory, runtime icon atlas,
health-item behavior, and Object-on-prop scripting.

Before executing a slice:

1. Read this document, `AGENTS.md`, and
   `docs/architecture/sector_editor_architectural_principles.md`.
2. Execute only the requested slice.
3. Mark it **In Progress** before editing. Finish as **Completed**,
   **Partial**, or **Blocked**.
4. If implementation disproves an assumption, first update the relevant
   contract or investigation note. Condense findings into durable facts; do not
   paste raw investigation logs.
5. Append an execution-log entry containing changes, deviations, tests,
   manual-verification status, cache behavior, lightmap-hash behavior, and
   collision/physics behavior.
6. Keep every completed slice buildable. Preserve old level JSON and levels
   without items.
7. Do not perform interactive or xdotool testing. Manual GUI verification
   remains user-owned.
8. Run after every slice containing C++ changes:

   ```sh
   cmake --build cmake-build-debug -j2
   ctest --test-dir cmake-build-debug --output-on-failure
   git diff --check
   git diff --stat
   git status --short
   ```

### Slice State

| Slice | Title | Status | Completed |
|---|---|---|---|
| 1 | Global item definitions, settings, assets, and Item Editor | Completed | 2026-08-25 |
| 2 | Authored item placements, world rendering, pickup, and session inventory | Not Started | — |
| 3 | Runtime icon atlas, inventory UI, health use, and safe dropping | Not Started | — |
| 4 | Object-on-prop use mode, scripting completion, and integration hardening | Not Started | — |

## Goal And Acceptance Criteria

Implement a global item system with Object, Weapon, Ammo, and Health
definitions; authorable item placements; lit and highlighted world models;
weight- and slot-limited pickup; a grid inventory; runtime-baked icons; health
use; safe dropping; and script-gated pickup/Object use.

Completion requires:

- Items are stored globally in `assets/config/items.json` and shared by every
  level.
- The Editors menu contains an Item Editor matching the existing list/detail
  editor layout.
- Definitions have immutable IDs, title, description, model, type, and
  per-unit weight.
- Weapon and Ammo definitions reference Weapon Editor weapon IDs.
- Health definitions support instant healing or linear healing over time.
- Levels serialize backward-compatible `kind: "item"` runtime objects.
- The Item tool places selectable, draggable, inspectable item instances in
  derived sectors.
- World items use the dynamic-prop PBR lighting, fog, environment, probe,
  shadow, visibility, and highlight policies, but are never animated or
  collidable.
- E shows `Take <TITLE>`, honors `onTakeScript`, and atomically takes or
  refuses the full placement quantity.
- The player has 30 kg and 24-slot defaults from hidden application settings.
- Ammo stacks by definition; Object, Weapon, and Health units occupy
  individual slots.
- Inventory opens with I, freezes player controls without pausing the world,
  and uses an opaque grid/detail UI with integer-aligned text.
- Icons are baked at runtime from item models into a generated 128×128-cell
  atlas owned by `AssetManager`.
- Drop places the selected slot at a clear arm's-length floor position or
  refuses without losing the item.
- Object use targets static or dynamic props and invokes the instance
  `onUseScript(targetInstanceId)`.
- Inventory, timed healing, collected authored placements, and session drops
  survive map changes and revisits. New Game clears them.
- Item placements and item-registry display changes correctly invalidate the
  2D editor cache.
- Items remain excluded from static lightmap geometry and the lightmap source
  hash.
- Existing doors, dynamic props, weapons, health, collision, sector lookup,
  and player physics do not regress.

## Fixed Scope And Decisions

### Item and Inventory Semantics

- Add optional application-settings data:

  ```json
  "playerInventory": {
    "maxCarryWeightKg": 30.0,
    "maxSlots": 24
  }
  ```

- Missing settings use those defaults. Keep application settings version 1 and
  do not expose these values in the game's Graphics Settings UI.
- Item weight is finite and non-negative. Carried weight is
  `definition weight × quantity`, accumulated with double precision for
  capacity checks.
- `maxCarryWeightKg` must be finite and positive. `maxSlots` must be between 1
  and 1024.
- Inventory starts empty. Existing weapon selection remains unrestricted until
  the explicitly deferred weapon-ownership integration.
- Ammo entries merge by item-definition ID and use one slot per stack.
- Object, Weapon, and Health placement quantities expand into one-unit entries
  and therefore require one slot per unit.
- Duplicate Weapon items are allowed and occupy separate slots.
- Pickup is atomic. If the complete quantity does not fit, leave the world item
  unchanged and show `I can't carry anymore.`
- Run a capacity preflight before `onTakeScript`, then recheck after a yielding
  hook completes.
- Ammo Drop drops the whole stack. Other types drop one unit.
- Dropped items receive a new session instance ID, default
  transform/scale/shadow presentation, blank `onTakeScript`, and retain
  `onUseScript` only for Object items.

### UI and Control Behavior

- I toggles inventory. Esc closes inventory before the application menu can
  open.
- Inventory and held-Object modes disable player movement, mouse look, firing,
  weapon switching, E interaction, and normal use prompts.
- Scripts, NPCs, doors, audio, and healing-over-time continue updating.
  Loading transitions may pause timed healing until gameplay becomes active
  again.
- The inventory remains open after Drop. Select the next occupied entry when
  the selected entry disappears.
- Health Use closes inventory immediately.
- Object Use closes inventory and enters held-targeting mode.
- In held mode, left-clicking a valid target invokes the hook; clicks elsewhere
  do nothing.
- Esc or right-click cancels held mode and restores FPS controls. I cancels held
  mode and reopens inventory.
- Once a yielding Object hook starts, keep controls locked and disallow
  cancellation until it finishes.
- After Object hook completion—true, false, missing function, or error—restore
  normal FPS controls rather than reopening inventory.
- Ammo and Weapon selections show no Use button.
- Object selections show Use only when their carried instance has a non-empty
  `onUseScript`.
- Health Use is disabled at full health.
- The inventory panel, grid, and detail pane are opaque. A separate
  world-dimming backdrop may be translucent.
- Round all panel rectangles, icon destinations, text origins, and measured
  follow-on positions to integral logical pixels.

### Persistence Boundaries

- Campaign-owned item state survives `changeMap`, current-map reload, editor
  suspend/resume, and revisiting a previously loaded level.
- Per-level state records collected authored item IDs, session-dropped item
  placements, and a collision-free positive dropped-object ID allocator.
- Picking up an authored item adds its placed-object ID to that level's
  collected set.
- Picking up a session drop removes its dropped record rather than marking an
  authored ID collected.
- On level load, suppress collected authored items and append current session
  drops before runtime spawning.
- New Game clears inventory, healing effects, collected IDs, and drops.
- Disk Save/Load Game serialization remains deferred because those menu actions
  are currently unimplemented. Keep campaign item state data-oriented and
  serializable for that future work.
- Item Editor saving is blocked while a game session exists, preventing live
  migration of carried stacks and dropped records.

### Explicitly Deferred

- Reload/ammo consumption and ammunition display.
- Restricting weapon equip keys to owned Weapon items.
- Save-game file format and Load/Save Game menu implementation.
- Inventory tetris, variable-sized items, equipment slots, hotbars, drag
  reordering, stack splitting, and quantity dialogs.
- Item trading, containers, crafting, durability, armor, quest tracking,
  dynamic runtime lights, or item animation.
- Using items on doors, NPCs, arbitrary world surfaces, or other items.
- Item collision, item shadowmaps beyond the existing dynamic-prop shadow
  modes, and static lightmap participation.
- Full inventory behavior in sector-editor Gameplay preview. Preview renders
  items but does not allow pickup, I inventory, healing, drop, or Object use.

## Compacted Investigation Record

- Global registries currently live under `assets/config`; weapons use
  `FpsWeaponRegistry` and an existing list/detail Weapons Editor.
- `FpsApplicationSettings` is loaded from
  `assets/config/application_settings.json`. Save/load already supports
  optional backward-compatible fields.
- Save Game and Load Game menu actions currently perform no operation.
- Runtime placements use `SectorPlacedRuntimeObject`, stable positive IDs, and
  `kind`-specific payloads. Runtime-object ownership in `SectorTopologyMap`
  remains accepted transitional architecture.
- Runtime-object editor mutations already route through
  `SectorEditorRuntimeObjectEditingService::MarkEdited()`, which marks the
  document dirty and invalidates the 2D cache.
- Dynamic props use `SectorDynamicModel`, model asset handles, object-probe
  lighting, dynamic lights, fog, environment reflections, contact/dynamic
  shadows, visibility culling, and interaction highlighting.
- Dynamic-prop rendering currently supports highlights; static-prop rendering
  must be extended to support the same held-target highlight.
- `SectorUseInteraction` already resolves centered-view E targets, performs
  topology occlusion checks, animates highlight strength, and draws the bottom
  prompt.
- Door permission hooks already demonstrate observed foreground Lua calls that
  may yield and eventually return boolean. The underlying script call path does
  not yet accept function arguments.
- Static and dynamic props have stable string `instanceId` values. Object use
  will pass this string, not the level-local numeric object ID.
- Health is integer-valued and currently has damage helpers but no healing
  helper.
- Generated textures can be uploaded through
  `AssetManager::CreateTextureFromImage()`, satisfying generated-resource
  ownership rules.
- Model loads are queued and finalized on the main thread. Icon baking therefore
  needs an explicit post-load preparation state rather than draw-time work.
- Static models are the only runtime objects currently included in the
  lightmap source hash. Dynamic props and the new item kind must remain
  excluded.
- The editor Gameplay preview only implements door interaction today. Full
  inventory behavior will remain game-session-only.
- Slice 1 intentionally seeds `assets/config/items.json` with an empty version-1
  registry; definitions are authored through the Item Editor rather than
  coupling the initial data to project-specific sample models.
- The application owns the shared item registry and item model asset scope.
  The editor's existing weapon-registry editing session remains separate, so
  item validation uses the editor weapon registry while application startup
  validates against the runtime weapon registry loaded from the same file.
- Before Slice 2 adds graph-native item placement structs, reference-safe item
  deletion scans v4 authoring-graph JSON `runtimeObjects` directly and fails
  closed on malformed item payloads or incomplete filesystem traversal.

## Target Contracts

### Global Item Registry

Add a versioned `assets/config/items.json`:

```json
{
  "version": 1,
  "items": [
    {
      "id": "small_medkit",
      "title": "Small Medkit",
      "description": "Restores a modest amount of health.",
      "modelPath": "assets/models/items/small_medkit.glb",
      "type": "health",
      "weightKg": 0.5,
      "healAmount": 25
    },
    {
      "id": "pistol_ammo",
      "title": "Pistol Ammunition",
      "description": "A box of pistol cartridges.",
      "modelPath": "assets/models/items/pistol_ammo.glb",
      "type": "ammo",
      "weightKg": 0.02,
      "weaponId": "pistol"
    },
    {
      "id": "regeneration_injector",
      "title": "Regeneration Injector",
      "description": "Gradually restores health.",
      "modelPath": "assets/models/items/regeneration_injector.glb",
      "type": "health",
      "weightKg": 0.25,
      "healAmount": 40,
      "healOverTime": true,
      "healDurationSeconds": 8.0
    }
  ]
}
```

Definition contract:

- `id`: generated `new_item`, `new_item_2`, etc.; immutable and displayed
  read-only.
- `title`: required human-readable UTF-8 text, maximum 96 codepoints.
- `description`: required multiline UTF-8 text, maximum 2048 codepoints.
- `modelPath`: required repository asset path selected by the existing model
  picker.
- `type`: `object`, `weapon`, `ammo`, or `health`.
- `weightKg`: finite and non-negative; per-unit weight.
- Weapon and Ammo require `weaponId` referencing an existing weapon registry
  entry.
- Health requires positive integer `healAmount`.
- `healOverTime` defaults false and is omitted when false.
- `healDurationSeconds` is required, finite, and positive only when
  heal-over-time is enabled.
- Object has no definition-specific extra fields.
- Sort saved definitions by stable ID for deterministic output.
- Structural or weapon-reference errors fail registry initialization with
  actionable messages. Model load failures do not invalidate definitions; they
  use runtime visual/icon fallbacks.

The Item Editor:

- Appears as `Item Editor` under Editors.
- Uses the existing scrolling list-left/detail-right modal pattern.
- Provides Add, Delete, Save, and Cancel.
- Shows ID read-only, then Title, Description, Model, Type, and Weight.
- Shows weapon dropdown for Weapon and Ammo.
- Shows Heal Amount, Heal Over Time, and conditional Duration for Health.
- Blocks deletion if the open document or any parseable v4 level under
  `assets/levels` references the ID. Fail closed if reference scanning cannot
  safely complete.
- Does not mark the current level dirty for definition-only changes.
- Invalidates item asset/icon state and item-dependent 2D cache data after a
  successful registry save.
- Weapon Editor save/delete/ID-edit validation must reject changes that would
  orphan Weapon or Ammo item references.

### Authored Item Placement

Extend runtime-object JSON with `kind: "item"`:

```json
{
  "id": 42,
  "kind": "item",
  "position": [12.0, 0.0, 8.0],
  "yawDegrees": 0.0,
  "item": {
    "definitionId": "pistol_ammo",
    "instanceId": "item_42",
    "quantity": 12,
    "takeDistance": 1.5,
    "onTakeScript": "canTakePistolAmmo",
    "onUseScript": "",
    "rotationXDegrees": 0.0,
    "rotationZDegrees": 0.0,
    "heightOffsetWorld": 0.0,
    "scale": 1.0,
    "shadowMode": "contact"
  }
}
```

Placement contract:

- `definitionId` references the global registry.
- `instanceId` is a stable, unique script-style string used for diagnostics and
  future extensibility.
- `quantity` is an integer from 1 through 1,000,000; default 1.
- `takeDistance` is finite and positive; default 1.5 world units.
- `onTakeScript` is optional for every type.
- `onUseScript` is authored and retained only for Object behavior; non-Object
  runtime use ignores it with a diagnostic rather than crashing.
- Transform fields and `shadowMode` match dynamic-prop presentation.
- There are no model-path, title, description, animation, loop,
  animation-speed, single-use, use-title, or collision fields.
- Omit default values following existing serializer style.
- Old levels without items load and save unchanged.
- Missing definitions produce an editor/runtime diagnostic and a 2D invalid
  marker; they do not become pickable inventory items.

The Item inspector contains:

- Definition picker.
- Read-only resolved title, type, model, and definition status.
- Instance ID.
- Position X/Z, rotation X/Y/Z, height offset, scale, and shadow mode.
- Quantity and Take Distance.
- On Take.
- Conditional On Use for Object definitions.
- Delete.
- No animation or collision controls.

### Runtime Models and Icons

Introduce application-global item asset state with a dedicated asset scope:

- One model handle per definition, requested outside update/render.
- Models load without animation data and render their default scene pose.
- World ECS components store only item definition/instance data and handles.
- Item models use the existing world dynamic-model PBR draw helper and lighting
  policy.
- The same item draw path receives object probes, sector fallback ambient,
  dynamic lights, fog, environment reflections, visibility, contact/dynamic
  shadow policy, and highlight strength.
- Item models never enter static lightmap charts or baked occluders.
- Missing, pending, or failed models skip 3D mesh draw while preserving safe
  point-based interaction and placeholder icons.

Icon atlas contract:

- Bake one 128×128 transparent icon cell per sorted item definition.
- Use an adaptive row-major atlas with up to eight columns; report allocation
  failure instead of silently omitting definitions.
- Fit the model's ready local bounds into the cell with eight-pixel padding
  using a fixed three-quarter camera.
- Use a deterministic key/fill/rim lighting shader and neutral orientation; no
  bloom, world fog, or lightmaps.
- Render/read back on the main thread during explicit startup or
  registry-rebuild preparation.
- Assemble the cells into one CPU `Image`, then upload the atlas through
  `AssetManager::CreateTextureFromImage()`.
- Store item-ID-to-source-rectangle metadata separately from the texture
  handle.
- Rebuild only when the item registry revision/model set changes; never bake or
  allocate in steady draw.
- Use a deterministic placeholder cell for missing/failed models.
- Pure atlas layout, camera fit, and region mapping must be testable without a
  GPU window.

### Player Inventory and Healing

Use a campaign-owned data-oriented state:

- Pre-reserved vector of inventory entries, maximum capacity from settings.
- Stable runtime entry ID, item definition ID, quantity, and optional Object
  `onUseScript`.
- Per-level collected authored IDs, dropped placements, and next drop ID.
- Pre-reserved active healing effects.
- No raylib resources or raw registry pointers inside inventory entries.

Health behavior:

- Instant items apply `min(healAmount, maximum-current)` and consume one entry.
- Use is unavailable at full health.
- Timed items consume one entry and add an effect with total amount, duration,
  elapsed time, and already-applied integer amount.
- Apply the linear cumulative target
  `floor(total × elapsed/duration)` so the final tick delivers exactly the
  authored total unless maximum health discards part of it.
- Multiple effects add concurrently.
- Healing never raises health above maximum. Healing lost while already at
  maximum is not banked for later damage.
- Effects survive map changes and continue only during active gameplay
  simulation.

### Pickup and Lua Hooks

Extend use-target kinds with Item and generalize the prompt renderer to accept
`Use` or `Take`.

Pickup order:

1. Resolve the best centered-view target using model bounds or a small fallback
   bound, take distance, facing, and topology occlusion.
2. Preflight complete weight/slot capacity.
3. If `onTakeScript` is blank, commit immediately.
4. Otherwise start an observed foreground call with no arguments.
5. Immediate or eventual boolean true permits pickup.
6. False, no boolean, missing function, or error leaves the item in the world.
7. Recheck capacity and entity/session identity before committing after a
   yield.
8. Commit inventory and per-level world ledger together, then queue ECS
   destruction outside iteration.

The same item cannot start a second take while its hook is pending.
Foreground-busy leaves it available without mutation.

Document this script-visible callback contract in `docs/scripting_api.md`.

### Object-on-Prop Use

Extend the script system with an allocation-conscious observed foreground call
accepting a pointer/count or equivalent fixed argument view of `ScriptValue`
values.

Object use flow:

- The carried entry must have a non-empty `onUseScript`.
- Enter held mode with the inventory hidden, cursor visible, camera/player
  controls locked, and the item atlas icon drawn beneath the cursor.
- Cast through the cursor into ready static and dynamic prop model bounds.
- Exclude NPCs, doors, items, and world surfaces as targets.
- Reject targets behind nearer topology or prop geometry.
- Pulse-highlight the chosen static/dynamic prop using the existing
  interaction shader effect.
- Call `onUseScript(targetInstanceId)` with the target's stable string instance
  ID.
- Boolean true consumes the Object entry.
- False, no boolean, missing function, or error keeps the entry.
- A yielding call remains pending and resolves through the observed-result
  path.
- Restore normal FPS controls after resolution.

Update `docs/scripting_api.md` with the argument and return-value contract.

### Inventory UI and Messages

At the 1920×1080 reference resolution:

- Center an opaque modal sized from its contents and clamped to safe viewport
  margins.
- Left side: six-column grid at the default 24 slots, scrolling vertically for
  larger configured limits.
- Each slot uses a uniform integer-aligned cell with the 128×128 icon, quantity
  badge where relevant, empty-slot treatment, and a distinct selected outline.
- Show carried weight/max weight and occupied/max slots.
- Right side: larger emphasized title, wrapped multiline description, then
  context-sensitive Use and Drop buttons.
- Measure wrapped description height and expand the detail/modal height to fit
  until reaching viewport limits; only then use an internal detail scroll area.
- If selection disappears, select the next entry, otherwise the previous
  entry, otherwise none.
- Hide normal Take/Use prompt while inventory or held mode is active.

Transient messages use the existing use-prompt font and bottom-center
placement:

- Hold fully opaque for 1.5 seconds, then fade over 0.75 seconds.
- Weight or slot refusal: `I can't carry anymore.`
- Unsafe drop refusal: `I can't drop that here.`
- Prompt/message layout uses rounded pixel coordinates.

### Safe Drop Placement

- Desired drop X/Z is 0.9 world units along the player's horizontal forward
  direction.
- Resolve the containing sector and place the model's transformed lower bound
  on that sector's floor.
- Use the item model bounds when ready and a conservative fallback bound
  otherwise.
- Refuse unless the bounds fit below the ceiling and remain clear of blocking
  topology, doors, static props, dynamic props, existing world items, and the
  player.
- Include non-collidable props in visual-overlap clearance; do not rely only on
  player-collider lists.
- Do not search a wide ring and do not fall back under the player.
- On success, add one session drop record and spawn only that item entity. Do
  not rebuild all runtime objects or reset NPC/door state.
- Items never participate in player collision after being dropped.

### Editor Cache, Lightmaps, and Physics

- Item add/delete, definition assignment, transforms, quantity, distances,
  scripts, scale, and shadow edits use the runtime-object editing service's
  normal document-edited path.
- The 2D cache stores item position, yaw, definition validity, compact label,
  and draw/pick marker data.
- Draw a distinct Item marker and keep picking consistent with that marker.
- Item-registry revision changes invalidate item-dependent cached data without
  marking the level document dirty.
- Steady 2D drawing performs no registry file I/O, topology validation, loop
  extraction, index rebuild, triangulation, or model inspection.
- All item definition and placement fields remain excluded from
  `ComputeSectorLightmapSourceHash()`.
- Item models receive probe/dynamic lighting only and do not become lightmap
  receivers or baked occluders.
- Items add no gameplay collider. Drop clearance is a query only.
- Static topology collision, sector lookup, door collision, and general player
  physics remain unchanged.

## Implementation Slices

### Slice 1 — Global Item Definitions, Settings, Assets, and Item Editor

**Intent:** establish validated global data and authoring before levels or
gameplay depend on it.

Implement:

- Item definition types, registry parser/validator/serializer, stable lookup,
  deterministic save, and an initial version-1 `items.json`.
- Optional `playerInventory` application-settings parsing/saving/defaults
  without adding Graphics Settings controls.
- Application ownership of the shared item registry and dedicated item model
  asset state.
- Item Editor state/service/panel using the established modal list/detail
  layout and model picker.
- Immutable ID generation, conditional fields, deletion reference scanning,
  live-game save blocking, and actionable validation.
- Editors menu routing, open/close/modal blocking, shutdown, and registry
  refresh behavior.
- Weapon Editor cross-reference protection so weapon changes cannot orphan item
  definitions.
- Pure icon atlas layout/camera-fit data contracts, while deferring actual GPU
  atlas baking to Slice 3.

Tests:

- Registry round-trip, deterministic order, every type, invalid
  IDs/types/weights/health fields, duplicate IDs, and invalid weapon references.
- Application settings old-data defaults, explicit fields, invalid ranges, and
  save/load.
- Item Editor add/select/delete/conditional-layout behavior and immutable IDs.
- Deletion blocks current/saved-level references and fails safely on unreadable
  candidate levels.
- Weapon Editor rejects orphaning edits.
- Missing/failed model paths remain resource diagnostics rather than registry
  parse failures.

Acceptance:

- Item Editor can create and save valid definitions.
- No level schema or gameplay behavior changes yet.
- Existing weapon/application settings tests remain green.

### Slice 2 — Authored Item Placements, World Rendering, Pickup, and Session Inventory

**Intent:** make items placeable, visible, lit, highlightable, and pickable into
a tested campaign inventory backend.

Implement:

- `SectorPlacedItem`, `kind: "item"` JSON read/write/validation/default
  omission, stable instance-ID allocation, and authoring derivation copying.
- Item tool button, inline last-definition picker, placement in derived sectors,
  inspector, selection, drag, delete, cached marker draw, and picking.
- Item-registry revision participation in cache rebuild validity.
- Runtime item ECS component reservation/spawning/removal and global
  model-handle resolution.
- Nonanimated PBR item rendering with dynamic-prop lighting/shadow policy and
  interaction highlight.
- Pure inventory capacity/stacking transaction logic with pre-reservation and
  allocation warnings.
- Campaign per-level collected/drop ledger and level-load reconciliation.
- Item use-target resolution, `Take` prompt, bottom refusal message, and async
  `onTakeScript` gating.
- Focused incremental item removal without rebuilding unrelated runtime
  objects.
- Game-only behavior; editor Gameplay preview renders authored item models but
  does not offer pickup.

Tests:

- Old and mixed runtime-object JSON, defaults, invalid
  quantities/distances/scripts, missing definitions, and graph-native
  round-trip.
- Tool availability, placement, inspector edits, selection/drag/delete, cache
  invalidation, cached marker picking, and registry-revision invalidation.
- Item spawn readiness/failure, lighting policy, visibility, shadow modes,
  highlight routing, and no animation components.
- Ammo-only stacking, non-ammo expansion, duplicate weapons,
  weight/slot boundaries, overflow safety, atomic refusal, and no partial
  mutations.
- Immediate/yielding true/false/missing/error `onTakeScript` outcomes.
- Collected authored items and drops reconcile correctly across level
  changes/revisits.
- Every item field leaves the lightmap source hash unchanged.

Required slice report:

- Enumerate topology mutation paths and confirm normal
  document-edited/cache invalidation.
- Confirm item fields remain excluded from the lightmap source hash.
- Confirm items add no collision and do not alter sector lookup or player
  physics.

### Slice 3 — Runtime Icon Atlas, Inventory UI, Health Use, and Safe Dropping

**Intent:** expose the inventory to the player and complete non-Object item
actions.

Implement:

- Main-thread runtime icon bake state, render target/readback, CPU atlas
  assembly, AssetManager upload, region lookup, placeholders, and explicit
  rebuild lifecycle.
- Startup/global-load gating so initial item models and icon baking finish or
  fail diagnostically before normal use.
- Item-editor-save atlas invalidation/rebuild without steady-frame baking.
- Campaign inventory modal state, integer-aligned responsive layout, grid
  selection, weight/slot summaries, description measurement, and queued UI
  actions.
- I/Esc input precedence, cursor/mouse-look transitions, and player-control
  suppression while world simulation continues.
- Instant and timed Health use with exact cumulative integer application and
  overlapping effects.
- Ammo/Weapon Use omission and temporary Object Use omission until Slice 4.
- Safe arm's-length drop clearance against topology, ceiling, doors, props,
  items, and player.
- Whole-stack Ammo dropping, single-unit other dropping, new session placement
  creation, incremental spawn, and stay-open UI behavior.
- Bottom message timing/fade and integral-pixel prompt/text fixes.

Tests:

- Atlas packing, stable sorted regions, camera fit, placeholder mapping, and
  revision transitions without a GPU window.
- Grid geometry, scrolling, selected outline geometry, description
  expansion/clamping, and integral text/layout coordinates.
- I/Esc/UI input precedence and cursor/control-state transitions.
- Instant health, full-health disabled use, timed exact totals, frame-rate
  variation, overlapping effects, maximum-health discard, and map-transition
  persistence.
- Safe drop success and refusal for wall, ceiling, door, static prop, dynamic
  prop, item, and player overlap.
- Failed drop leaves inventory/session/world unchanged; successful drop updates
  all three atomically.
- Repeated drop/pick cycles do not grow runtime capacity unexpectedly or reset
  unrelated objects.

Required slice report:

- Record that drop clearance queries geometry but items remain non-collidable.
- Record unchanged lightmap-hash behavior and whether icon GPU baking was
  manually inspected; do not claim inspection if it was not.

### Slice 4 — Object-on-Prop Use, Scripting Completion, and Integration Hardening

**Intent:** complete contextual Object use and close cross-subsystem gaps
without adding new item features.

Implement:

- Script-system foreground-hook argument support for immediate and yielding
  observed calls.
- Held-Object state machine, cursor icon, input cancellation, pending-call
  state, and normal-control restoration.
- Cursor-ray selection of static/dynamic prop bounds with topology/nearer-prop
  occlusion.
- Static- and dynamic-prop pulse highlighting in held mode.
- `onUseScript(targetInstanceId)` true/false/missing/error/yield behavior and
  Object entry consumption.
- I-to-reopen inventory, Esc/right-click cancel, invalid-click no-op, and no
  cancellation after a yielding call starts.
- Exhaustive switch/default/reservation/reset/shutdown audits for new item types
  and UI states.
- Cross-level campaign-state hardening, editor attach/resume behavior, registry
  diagnostics, pending/failed asset behavior, and repeated scene rebuilds.
- `docs/scripting_api.md` and `docs/sector_editor.md` updates.
- Final plan state/log and user-owned manual checklist.

Tests:

- Script argument delivery and returned values for immediate and yielding
  hooks.
- Target selection for static/dynamic props, stable string IDs, nearest-hit
  ordering, topology occlusion, and exclusion of NPCs/doors/items/surfaces.
- True consumes exactly one Object; false/no value/missing/error keeps it.
- Busy and pending hooks cannot duplicate consumption.
- Cancel/reopen/restore-control transitions and pending-call shutdown safety.
- Mixed maps with all item types, doors, props, NPCs, missing definitions,
  failed models, and repeated map revisits.
- Existing dynamic-prop and door interaction tests remain unchanged.

Final report must explicitly state:

- Item authoring mutations invalidate the 2D cache through the normal
  document-edited path.
- Item definitions, placements, models, drops, and icons remain excluded from
  the lightmap source hash.
- Items add no collision; only drop-clearance and held-target queries were
  added, with static collision, sector lookup, and player physics unchanged.
- Lua documentation was updated for both item hooks.
- Manual GUI verification remains user-owned unless the user reports results.

## Cross-Slice Test Matrix

| Area | Required coverage |
|---|---|
| Registry | Every item type, conditional fields, stable IDs, deterministic save, invalid data, weapon references |
| Settings | Backward-compatible defaults, 30 kg/24 slots, invalid values, hidden UI behavior |
| Serialization | Old maps, item defaults, all placement fields, invalid quantities/distances, graph-native round-trip |
| Editor | Modal layout, conditional controls, model/definition pickers, reference-safe delete |
| Cache | Mutation invalidation, registry revision, marker draw/pick agreement, no steady derived work |
| Runtime assets | Pending/ready/failed models, global scope rebuild, icon placeholder and atlas lifecycle |
| Rendering | Dynamic-prop PBR policy, probes, fog, environment, shadows, visibility, highlighting |
| Inventory | Ammo stacks, non-ammo units, duplicates, atomic weight/slot checks, stable selection |
| Pickup scripts | Blank, true, false, no value, missing, error, yield, busy, capacity recheck |
| Health | Instant, full health, timed linear total, overlap, clamping, map transitions |
| Drop | Whole ammo stack, safe floor placement, all obstruction categories, atomic refusal |
| Object use | Cursor target, string instance ID, occlusion, yield, consumption/cancellation |
| Persistence | Collected IDs, drops, revisits, reload, editor suspend/resume, New Game reset |
| Lightmaps | All item changes leave the source hash unchanged |
| Physics | No item collider and no changes to topology collision, sector lookup, or player movement |

Tests must use generated maps, temporary JSON, or dedicated immutable fixtures
outside user-edited level assets.

## Manual Acceptance Checklist

1. Create one definition of each type and save/reopen the Item Editor.
2. Place items with different transforms, quantities, distances, hooks, and
   shadow modes; save/reload the level.
3. Confirm world models receive the same lighting/fog/reflections/highlight
   treatment as dynamic props and remain unanimated/non-colliding.
4. Verify weight and slot refusal, including an Ammo stack that merges and a
   non-Ammo quantity that needs multiple slots.
5. Open inventory with I, select grid entries, inspect wrapped descriptions,
   and confirm opaque/integer-aligned UI.
6. Use instant and timed Health items, including overlapping effects and full
   health.
7. Drop each type, confirm arm's-length floor placement, and confirm obstructed
   drops are refused.
8. Verify Ammo drops as one full stack and duplicate Weapon items remain
   separate.
9. Exercise blank, true, false, missing, error, and yielding `onTakeScript`.
10. Use an Object on static and dynamic props; verify the cursor icon,
    highlight, target string ID, cancellation, and true/false consumption.
11. Change maps and return; confirm collected authored pickups stay gone and
    dropped items remain.
12. Start New Game and confirm all campaign item state resets.
13. Temporarily break a model path and confirm safe world/icon fallbacks.
14. Confirm sector-editor Gameplay preview renders items but does not run pickup
    or inventory behavior.

## Execution Log

Append one entry per attempted slice:

```text
### YYYY-MM-DD — Slice N — Completed|Partial|Blocked

- Summary:
- Decisions/deviations folded back into plan:
- New compacted findings:
- Files/modules materially affected:
- Automated verification:
- Manual verification: Not performed (user-owned), unless reported otherwise.
- Cache invalidation behavior:
- Lightmap source-hash behavior:
- Collision/sector lookup/physics behavior:
- Allocation/load-phase behavior:
- Remaining follow-up within this plan:
```

Do not delete older entries. If a later correction changes a completed slice's
contract, update the contract section and append a dated post-slice correction
entry.

### 2026-08-25 — Slice 1 — Completed

- Summary: Added the validated global item registry, hidden inventory settings,
  application-owned item model asset scope, pure icon-layout/camera-fit
  contracts, Item Editor, multiline UI input, and Weapon Editor cross-reference
  protection.
- Decisions/deviations folded back into plan: The initial registry is
  intentionally empty. Item IDs are immutable 1-63 byte ASCII identifiers;
  titles and descriptions are strictly validated UTF-8. Model existence is a
  runtime asset diagnostic rather than a registry validation failure.
- New compacted findings: Application and editor weapon registries remain
  separate views of the same persisted registry. Until graph-native item
  placements arrive in Slice 2, a fail-closed v4 JSON scanner provides deletion
  safety without changing the level schema.
- Files/modules materially affected: Item definitions/assets/icon-layout under
  `sources/game/items`, application settings and ownership, reusable multiline
  UI input, `sources/sector_editor/items`, Item Editor routing/modal integration,
  Weapon Editor validation, configuration assets, CMake, and focused tests.
- Automated verification: `cmake --build cmake-build-debug -j2` passed;
  `ctest --test-dir cmake-build-debug --output-on-failure` passed all 31 tests;
  `git diff --check`, `git diff --stat`, and `git status --short` completed with
  no whitespace errors.
- Manual verification: Not performed (user-owned).
- Cache invalidation behavior: A successful Item Editor save rebuilds the
  dedicated item model scope and calls `InvalidateTopologyRenderCache()` without
  marking the current level document dirty. No topology mutation path was added
  in this slice.
- Lightmap source-hash behavior: Unchanged. Slice 1 adds no level schema,
  generated geometry, receivers, or occluders; global item definitions and
  model asset state are excluded from the lightmap source hash.
- Collision/sector lookup/physics behavior: Unchanged; no item placement,
  collider, collision query, sector lookup, or player-physics behavior was
  introduced.
- Allocation/load-phase behavior: Item model handles are requested in the
  dedicated global scope during application initialization and explicit
  post-save registry rebuilds, never from steady update or render. Icon atlas
  allocation/GPU baking remains deferred to Slice 3.
- Remaining follow-up within this plan: Slices 2-4.
