# Sector Dynamic Spotlight And Pilot Mode Plan

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

```plan-state-json id="dynamic-spotlights"
{
  "plan_id": "sector_dynamic_spotlight_and_pilot_mode_plan",
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
      "title": "Dynamic Spotlight Data And Serialization",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_01a",
      "title": "Add Dynamic Spotlight Data Model And JSON Round Trip",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_02",
      "title": "Dynamic Spotlight Editor Authoring",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02a",
      "title": "Add Dynamic Spotlight Tool And Inspector Fields",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02b",
      "title": "Add 2D Spotlight Overlay And Selection Handles",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "Runtime Dynamic Spotlight Rendering",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_03a",
      "title": "Extend Runtime Light Selection And Packing For Spotlights",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b",
      "title": "Add Spotlight Shader Contribution",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_04",
      "title": "3D Spotlight Visualization",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_04a",
      "title": "Draw Selected Spotlight Cone Overlay In 3D Preview",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_05",
      "title": "Spotlight Pilot Mode",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_05a",
      "title": "Add Pilot Selected Spotlight Apply Cancel Workflow",
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
      "title": "Tune Defaults Strengthen Tests Update Docs And Close Plan",
      "type": "pass",
      "parent": "phase_06",
      "status": "Completed"
    }
  ]
}
```

## Current Progress

| Phase / Pass                                                        | Status      | Date | Notes                                                                                    |
| ------------------------------------------------------------------- | ----------- | ---- | ---------------------------------------------------------------------------------------- |
| Phase 1: Dynamic Spotlight Data And Serialization                   | Completed   | 2026-06-29 | Completed with Phase 1A.                                                                 |
| Phase 1A: Add Dynamic Spotlight Data Model And JSON Round Trip      | Completed   | 2026-06-29 | Added `dynamicSpotLights` data model, JSON round trip, validation, helpers, authoring map-level copy, and tests. |
| Phase 2: Dynamic Spotlight Editor Authoring                         | Completed   | 2026-06-29 | Completed with Phase 2A and Phase 2B.                                                    |
| Phase 2A: Add Dynamic Spotlight Tool And Inspector Fields           | Completed   | 2026-06-29 | Added `Dynamic Spot` tool, origin placement, origin picking, delete flow, and inspector fields. |
| Phase 2B: Add 2D Spotlight Overlay And Selection Handles            | Completed   | 2026-06-29 | Added cached 2D origin/target/cone overlay and top-down origin/target drag handles.       |
| Phase 3: Runtime Dynamic Spotlight Rendering                        | Completed   | 2026-06-29 | Completed with Phase 3A and Phase 3B.                                                    |
| Phase 3A: Extend Runtime Light Selection And Packing For Spotlights | Completed   | 2026-06-29 | Dynamic spotlights join the runtime selection pool and packed uniform data under the shared cap. |
| Phase 3B: Add Spotlight Shader Contribution                         | Completed   | 2026-06-29 | Dynamic spotlights now contribute cone-filtered direct light in the forward shader.       |
| Phase 4: 3D Spotlight Visualization                                 | Completed   | 2026-06-29 | Completed with Phase 4A.                                                                 |
| Phase 4A: Draw Selected Spotlight Cone Overlay In 3D Preview        | Completed   | 2026-06-29 | Added selected dynamic spotlight 3D wire overlay.                                        |
| Phase 5: Spotlight Pilot Mode                                       | Completed   | 2026-06-29 | Completed with Phase 5A.                                                                 |
| Phase 5A: Add Pilot Selected Spotlight Apply Cancel Workflow        | Completed   | 2026-06-29 | Added selected dynamic spotlight 3D pilot apply/cancel workflow.                         |
| Phase 6: Polish Tests Documentation And Completion                  | Completed   | 2026-06-29 | Completed with Phase 6A.                                                                 |
| Phase 6A: Tune Defaults Strengthen Tests Update Docs And Close Plan | Completed   | 2026-06-29 | Strengthened defaults/runtime packing tests, updated dynamic-lighting docs, and closed the plan. |

### Phase 1A Completion Notes - 2026-06-29

Summary:

* Source code changed.
* Added `SectorTopologyDynamicSpotLight` and map-level `dynamicSpotLights`.
* Added stable ID allocation, lookup, and removal helpers.
* Added topology and graph-native authoring JSON round-trip support.
* Added validation and tests for serialization defaults, missing fields, invalid fields, deterministic output, helper functions, authoring map-level persistence, and static lightmap hash isolation.

Serialized field names:

* Root array: `dynamicSpotLights`.
* Per light: `id`, `position`, `target`, `range`, `intensity`, `color`, optional `innerConeDegrees`, optional `outerConeDegrees`, optional `enabled`, optional `flicker`, optional `flickerSpeed`, optional `flickerAmount`.

Default values:

* `enabled = true`, `color = WHITE`, `intensity = 1.0`, `range = SectorWorldToAuthoringDistance(8.0f)`, `innerConeDegrees = 20.0`, `outerConeDegrees = 35.0`, `flicker = false`, `flickerSpeed = DynamicLightFlickerDefaultSpeed`, `flickerAmount = DynamicLightFlickerDefaultAmount`.
* Default optional fields are omitted on save where matching existing dynamic point light style.

Validation and behavior notes:

* Missing `dynamicSpotLights` loads as empty.
* Required `position`, `target`, `range`, `intensity`, and `color` fields are validated.
* `range` must be finite and positive.
* `intensity` must be finite and non-negative.
* Cone fields load with clamps `0.0..179.0`, and `outerConeDegrees` is widened to at least `innerConeDegrees`.
* Flicker fields use the existing dynamic light defaults and clamps.
* Coincident `position` and `target` remain loadable.
* Renderer/editor UI behavior is unchanged.
* Static lightmap source hash ignores dynamic spotlights and dynamic spotlight edits.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.

### Phase 2A Completion Notes - 2026-06-29

Summary:

* Source code changed.
* Added a `Dynamic Spot` map-object tool.
* Clicking inside a sector creates a dynamic spotlight origin at floor + 1.8 world units.
* New spotlight target defaults to a forward X offset and slightly lower target height.
* Added origin picking while the `Dynamic Spot` tool is active so saved/reloaded spotlights can be selected before Phase 2B overlays exist.
* Added delete support for selected dynamic spotlights.
* Added dynamic spotlight inspector fields: Enabled, Position X/Y/Z, Target X/Y/Z, Range, Inner cone degrees, Outer cone degrees, Intensity, Color RGB, Flicker, Flicker speed, and Flicker amount.
* Added focused test coverage for the new tool label/help/availability.

Behavior notes:

* Existing static light and dynamic point light tools remain separate and unchanged.
* Dynamic spotlight edits use the existing `MarkTopologyDocumentEdited()` / topology action finish path, so the document is marked dirty and the 2D topology render cache is invalidated.
* Save/load schema from Phase 1 is unchanged.
* Runtime rendering, shader behavior, 2D spotlight cone overlay, 2D drag handles, 3D overlay, and pilot mode are unchanged.
* Static lightmap source-hash behavior is unchanged; dynamic spotlight data remains excluded from the static lightmap source hash.
* Manual GUI smoke was not performed.

Verification:

* `git diff --check` passed.
* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.

### Phase 2B Completion Notes - 2026-06-29

Summary:

* Source code changed.
* Added cached 2D dynamic spotlight overlay data for origin, target, range, and cone angles.
* Draws dynamic spotlights in 2D with origin marker, target marker, line from origin to target, outer cone wedge, and selected inner cone guide.
* Added hover/selection support for both origin and target handles while the `Dynamic Spot` tool is active.
* Added simple top-down drag handles:
    * dragging the origin moves both origin and target together, preserving the target offset and authored heights
    * dragging the target updates target X/Z only, preserving target height

Behavior notes:

* Existing static point light and dynamic point light overlays are unchanged.
* Sector, line, face, and vertex selection behavior is unchanged.
* Dynamic spotlight save/load schema is unchanged.
* Dynamic spotlight 2D overlay data is cached with the topology render cache; finished drag edits go through `FinishTopologyActionResult()` / `MarkTopologyDocumentEdited()`, so the document is marked dirty and the 2D topology render cache is invalidated.
* Runtime rendering, shader behavior, 3D overlay, and pilot mode are unchanged.
* Static lightmap source-hash behavior is unchanged; dynamic spotlight data remains excluded from the static lightmap source hash.
* Manual GUI smoke was not performed.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* `git diff --check` passed.
* `git diff --stat` ran; 12 files changed.
* `git status --short` ran; expected modified files remain uncommitted.

### Phase 3A Completion Notes - 2026-06-29

Summary:

* Source code changed.
* Added a packed runtime dynamic light kind with `Point` and `Spot` values.
* Dynamic spotlights are collected into the same runtime dynamic light source/candidate/selection lists as dynamic point lights.
* Dynamic point lights and dynamic spotlights share the existing `MAX_DYNAMIC_LIGHTS = 8` total selection cap.
* Dynamic spotlight packing includes position, normalized direction, color, range, base intensity, inner/outer cone cosines, kind, and flicker settings.
* Added focused tests for spotlight source packing, candidate selection, fallback draw-all behavior, shared cap/ranking, deterministic ties, and flicker ranking by base intensity.

Behavior notes:

* Dynamic point light selection, ranking, flicker upload, and shader contribution remain supported.
* Dynamic spotlight candidate selection is conservative: owner visible sector or range-sphere overlap with visible receiver bounds; no cone test yet.
* Ranking uses the same receiver-distance score as point lights and uses base intensity, not flickered upload intensity.
* Fallback draw-all considers all valid enabled dynamic point lights and dynamic spotlights.
* Shader uniform layout now includes `dynamicLightTypes`, `dynamicLightDirections`, `dynamicLightInnerConeCos`, and `dynamicLightOuterConeCos`.
* Actual spotlight cone lighting is still deferred to Phase 3B; Phase 3A shader code skips non-point entries so spotlights do not render as point lights.
* Serialization, editor behavior, 2D topology render-cache invalidation, 3D overlay, pilot mode, static lightmaps, and lightmap source-hash behavior are unchanged.
* Manual GUI smoke was not performed.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `./cmake-build-debug/sector_topology_mesh_builder_tests` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.

### Phase 3B Completion Notes - 2026-06-29

Summary:

* Source code changed.
* Added runtime spotlight contribution in the sector lightmap forward shader.
* Dynamic point lights keep the existing distance attenuation, normal Lambert term, flicker-upload intensity, and shared `MAX_DYNAMIC_LIGHTS = 8` budget behavior.
* Dynamic spotlights use the existing packed uniform layout: `dynamicLightTypes`, `dynamicLightDirections`, `dynamicLightInnerConeCos`, and `dynamicLightOuterConeCos`.

Shader behavior:

* Type `0` lights remain point lights with cone attenuation fixed at `1.0`.
* Type `1` lights use `spotDirection = normalize(light target direction)`, `fragmentDirectionFromLight = normalize(fragWorldPosition - light.position)`, `coneDot = dot(spotDirection, fragmentDirectionFromLight)`, and `coneAtten = smoothstep(outerConeCos, innerConeCos, coneDot)`.
* Equal inner/outer cone cosines use a hard `step(innerConeCos, coneDot)` fallback to avoid undefined `smoothstep` edges.
* Dynamic spotlight direct lighting is not multiplied by baked AO.

Behavior notes:

* Runtime shader output changed for selected dynamic spotlights; they no longer get skipped by the forward dynamic light loop.
* Dynamic light selection/ranking behavior is unchanged from Phase 3A.
* Serialization, editor behavior, 2D topology render-cache invalidation, 3D overlay, pilot mode, static lightmaps, and lightmap source-hash behavior are unchanged.
* Manual GUI smoke was not performed.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `./cmake-build-debug/sector_topology_mesh_builder_tests` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.

### Phase 4A Completion Notes - 2026-06-29

Summary:

* Source code changed.
* Added an editor-only 3D preview overlay for the selected dynamic spotlight.
* The overlay draws an origin wire marker, target wire marker, origin-to-target line, range direction line, outer cone wire ring with spokes, and inner cone wire ring.
* Cone direction, range, and inner/outer angles use the same authored dynamic spotlight values as runtime spotlight rendering, converted to world units for preview drawing.

Behavior notes:

* Overlay drawing is selected dynamic spotlight only; unselected spotlights do not add 3D preview clutter.
* Rendering/culling, runtime dynamic spotlight shader behavior, dynamic light selection/ranking, serialization, and public data schema are unchanged.
* This pass does not mutate topology, so 2D topology render-cache invalidation behavior is unchanged.
* Static lightmap source-hash behavior is unchanged; dynamic spotlights remain excluded from the static lightmap source hash.
* Gameplay collision, sector lookup, physics, and camera behavior are unchanged.
* Manual GUI smoke was not performed.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* `git diff --check` passed.
* `git diff --stat` ran; 3 files changed.
* `git status --short` ran; expected modified files remain uncommitted.

### Phase 5A Completion Notes - 2026-06-29

Summary:

* Source code changed.
* Added editor state for piloting a selected dynamic spotlight in 3D FreeFly mode.
* Added 3D overlay controls: `Pilot Light`, `Apply`, and `Cancel`.
* `Pilot Light` stores the original spotlight position/target, original free-fly camera pose/state, and target distance, then moves the camera to the spotlight origin looking at its target.
* `Apply` writes the current camera position to the spotlight origin and writes the target as camera position plus camera forward times the preserved target distance.
* `Apply` refreshes the 3D preview dynamic-light source cache so the moved spotlight updates in the live preview without rebuilding geometry.
* `Cancel`, leaving 3D mode, selection loss, and deleting the selected spotlight cancel the pilot state safely.

Behavior notes:

* Dynamic spotlight authored data does not mutate while previewing; topology data changes only on `Apply`.
* `Apply` uses `MarkTopologyDocumentEdited()`, so the document is marked dirty and the 2D topology render cache is invalidated.
* `Cancel` restores the original spotlight position/target and original free-fly camera pose/state without marking the document dirty.
* `SectorMeshPreview` gained a narrow `RefreshDynamicLightSources()` method for editor-side dynamic-light cache refresh after Apply.
* Normal 3D camera mode is unchanged when not piloting.
* Save/load schema, serialization, runtime shader behavior, generated geometry, static lightmaps, and lightmap source-hash behavior are unchanged.
* Gameplay collision, sector lookup, physics, and camera behavior outside editor pilot camera control are unchanged.
* Manual GUI smoke was not performed.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* `git diff --check` passed.
* `git diff --stat` ran; 6 files changed.
* `git status --short` ran; expected modified files remain uncommitted.

### Phase 6A Completion Notes - 2026-06-29

Summary:

* Source code changed only in tests; runtime/editor behavior was not changed.
* Added serialization coverage that pins dynamic spotlight authoring defaults:
  origin height 1.8 world units, target 4.0 world units forward in X at 1.0
  world unit high, range 8.0 world units, intensity 1.0, default omitted
  enabled/cone/flicker fields, and existing 20/35 degree cone defaults.
* Added runtime packing coverage for coincident spotlight position/target using
  the safe fallback direction and for outer cone ordering when authored outer
  angle is narrower than inner angle.
* Updated `docs/sector_dynamic_lighting_design.md` to describe implemented
  dynamic spotlights, shared dynamic point/spot light budget, conservative
  candidate selection, pilot target-distance behavior, and deferred static
  spotlight/shadow-map work.

Behavior notes:

* Dynamic point lights, dynamic point flicker, static baked lights, shader
  runtime behavior, editor behavior, serialization schema, generated geometry,
  and public APIs are unchanged.
* This pass did not touch topology mutation code, so 2D topology render-cache
  invalidation behavior is unchanged.
* Static lightmap source-hash behavior is unchanged; dynamic point lights and
  dynamic spotlights remain excluded from the static lightmap source hash.
* Gameplay collision, sector lookup, physics, and camera behavior are unchanged.
* Static baked spotlights, shadow maps, PCF, flashlight gameplay, and generic
  object/entity work remain deferred to later plans.
* Manual GUI smoke was not performed.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* `git diff --check` passed.
* `git diff --stat` ran; 3 files changed before the plan update.
* `git status --short` ran; expected modified files remain uncommitted.

## Execution Tracking Rules

* Each pass must leave the project buildable and runnable.
* Each pass final report must state whether source code changed.
* Each implementation pass must update this document before finishing.
* The update should be small and local.
* Do not rewrite unrelated phases when marking progress.
* If behavior is intended to remain unchanged, explicitly state that.
* If a pass changes serialization, generated data, public APIs, runtime behavior, cache invalidation, shader behavior, editor behavior, or build/test behavior, clearly say so.
* Do not claim manual GUI verification unless it was actually performed.
* If a pass produces only a plan or audit and no source changes, state that clearly.
* If a pass is too broad, split it into smaller child passes and stop without source changes.

## Goal And Desired End State

Add authored dynamic spotlights and a simple 3D pilot workflow without building a Blender-style transform gizmo system.

Desired end state:

* Dynamic spotlights are a separate authored dynamic light type.
* Dynamic spotlights save/load correctly.
* Dynamic spotlights are editable in the editor.
* Dynamic spotlights render in the existing forward dynamic light path.
* Dynamic point lights continue to work unchanged.
* Dynamic point and dynamic spot lights share the same total runtime dynamic light budget.
* Dynamic spotlights can reuse dynamic-light flicker controls where practical.
* A selected spotlight can be piloted in 3D mode:

    * enter pilot mode
    * move/aim using normal camera controls
    * apply to copy camera pose to spotlight
    * cancel to restore previous spotlight transform
* Static baked lights remain unchanged.
* Static spotlights are deferred to a later plan.
* Shadow maps are deferred to a later plan.

Core authoring representation:

```text
spotlight = position + target + range + innerConeDegrees + outerConeDegrees
direction = normalize(target - position)
```

Use position + target instead of yaw/pitch as the editor-facing model.

## Dependency Direction Rules

* Dynamic spotlight authored data may be serialized in topology/map data.
* Runtime selected-light state must not be serialized.
* Dynamic spotlight rendering may depend on existing dynamic light selection/ranking.
* Dynamic light selection/ranking may depend on portal visibility and receiver bounds.
* Portal visibility must not depend on dynamic lighting.
* Static light baking and source hashing must not depend on dynamic spotlights.
* Pilot mode may mutate selected authored spotlight data only on Apply.
* Pilot mode Cancel must restore the original authored spotlight transform.
* Shader changes must preserve dynamic point lights and zero-light behavior.

## Proposed Phases

### Phase 1: Dynamic Spotlight Data And Serialization

Goal:

Add a dynamic spotlight data model and JSON round-trip without editor UI or runtime rendering changes.

Why it helps:

This makes the authored data shape stable before touching tools, shader packing, or pilot mode.

Files/functions likely touched:

* `sources/sector_demo/SectorTopologyTypes.h`
* `sources/sector_demo/SectorTopologySerialization.cpp`
* dynamic light validation helpers
* dynamic light serialization tests
* lightmap hash tests if dynamic-light hash isolation has coverage

Exact behavior that must remain unchanged:

* Existing dynamic point lights save/load unchanged.
* Existing dynamic point flicker save/load unchanged.
* Existing static lights save/load unchanged.
* Static lightmap source hash unchanged by dynamic spotlight edits.
* Renderer output unchanged.
* Editor UI unchanged.

Risks/goblins:

* Accidentally mixing static and dynamic light semantics.
* Accidentally including dynamic spotlights in lightmap source hash.
* Serialization default/omit-default inconsistencies.
* Target equal to position producing invalid direction later.

Non-goals:

* No shader changes.
* No editor tool.
* No 2D overlay.
* No 3D overlay.
* No pilot mode.
* No static spotlights.
* No shadows.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant serialization/hash tests.

Final report expectations:

* State files changed.
* State serialized field names.
* State default values.
* State validation behavior.
* Confirm renderer/editor behavior unchanged.
* Confirm dynamic spotlights do not affect static lightmap source hash.
* State verification commands/results.

How to update this plan after completion:

* Mark Phase 1A `Completed` in JSON and table.
* Mark Phase 1 `Completed` if Phase 1A is complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 1A: Add Dynamic Spotlight Data Model And JSON Round Trip

Goal:

Add authored dynamic spotlight persistence only.

Implementation guidance:

Add a new type separate from dynamic point lights, for example:

```cpp
struct SectorTopologyDynamicSpotLight {
    int id = 0;
    bool enabled = true;
    Vector3 position = {};
    Vector3 target = {};
    Color color = WHITE;
    float intensity = 1.0f;
    float range = 8.0f;
    float innerConeDegrees = 20.0f;
    float outerConeDegrees = 35.0f;
    bool flicker = false;
    float flickerSpeed = 1.0f;
    float flickerAmount = 0.35f;
};
```

Use actual project naming/style.

Add map-level storage such as:

```cpp
std::vector<SectorTopologyDynamicSpotLight> dynamicSpotLights;
```

Use stable positive integer IDs.

Serialize under a distinct root field such as:

```json
"dynamicSpotLights": []
```

Load missing field as empty.

Omit empty/default fields on save if that matches existing style.

Suggested defaults:

```text
enabled = true
color = white
intensity = 1.0
range = 8.0 world units / matching current authoring unit convention
innerConeDegrees = 20
outerConeDegrees = 35
flicker = false
flickerSpeed = dynamic point light default
flickerAmount = dynamic point light default
```

Validation/clamping:

* enabled must be boolean if present
* position/target must be valid vectors
* color must be valid color
* intensity must be finite and non-negative
* range must be finite and positive
* inner/outer cone must be finite and clamped to sane ranges
* outer cone must be greater than or equal to inner cone
* if target equals position or is too close, keep data loadable but later runtime should handle it safely; optionally repair target by default direction during authoring only
* flicker fields should use the same defaults/clamps as dynamic point lights

Recommended cone clamps:

```text
innerConeDegrees: 0.0 .. 179.0
outerConeDegrees: 0.0 .. 179.0
outerConeDegrees >= innerConeDegrees
```

Do not include dynamic spotlights in `ComputeSectorLightmapSourceHash()`.

Tests:

* missing `dynamicSpotLights` loads as empty
* default dynamic spotlight fields load correctly
* dynamic spotlight save/load round-trip
* omit-default behavior if applicable
* invalid numeric fields handled consistently with existing dynamic point light behavior
* dynamic spotlight edits do not affect lightmap source hash
* existing dynamic point light serialization tests still pass

### Phase 2: Dynamic Spotlight Editor Authoring

Goal:

Add editor creation and basic 2D editing for dynamic spotlights.

Why it helps:

Spotlights need a usable authoring model before runtime rendering/pilot mode gets useful.

Files/functions likely touched:

* sector editor topology/action files
* light tool panel/tool mode files
* light inspector files
* 2D overlay/render files
* selection files
* topology/editor tests where available

Exact behavior that must remain unchanged:

* Static light tool unchanged.
* Dynamic point light tool unchanged.
* Existing dynamic point light inspector unchanged except shared helper refactors.
* Save/load behavior from Phase 1 unchanged.
* Runtime rendering unchanged until Phase 3.

Risks/goblins:

* UI confusing static/dynamic or point/spot categories.
* Too much 3D manipulation scope sneaking in early.
* Target editing awkward in 2D without pilot mode.
* Dirty/cache invalidation not following existing light edit behavior.

Non-goals:

* No shader rendering yet.
* No pilot mode yet.
* No static spotlights.
* No shadows.
* No complex 3D transform gizmos.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant editor/topology tests.

Final report expectations:

* State tool labels.
* State inspector fields.
* State 2D overlay/edit behavior.
* State save/reload behavior if manually checked.
* State manual GUI smoke status.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 2 `Completed` only after Phase 2A and Phase 2B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 2A: Add Dynamic Spotlight Tool And Inspector Fields

Goal:

Add a dynamic spotlight creation tool and inspector fields.

Implementation guidance:

Add a tool/button label such as:

```text
Dynamic Spot
```

Keep static/dynamic categories clear. Do not turn static/dynamic into a checkbox on one light object.

Placement:

* click in 2D sector to place spotlight origin
* default position height: similar to dynamic point light default, probably floor + 1.8 world units
* default target: origin plus a simple forward offset in X/Z and slightly downward if practical
* default range/inner/outer/intensity/color from Phase 1 defaults
* enabled = true

Inspector fields:

```text
Enabled
Position X/Y/Z
Target X/Y/Z
Range
Inner cone degrees
Outer cone degrees
Intensity
Color
Flicker
Flicker speed
Flicker amount
```

Editing these fields should mark the document dirty and use the same cache/render invalidation path as dynamic point light edits.

Do not add shadow settings.

Do not add pilot mode controls in this pass unless a minimal placeholder is unavoidable.

Manual smoke:

* add dynamic spotlight
* select it
* edit fields
* save/reload
* confirm persistence
* confirm dynamic point/static light tools still work

#### Phase 2B: Add 2D Spotlight Overlay And Selection Handles

Goal:

Make dynamic spotlights understandable in 2D mode.

Implementation guidance:

Draw selected and unselected dynamic spotlights in 2D:

* origin marker
* target marker
* line from origin to target
* top-down cone/wedge using outer cone angle
* optional inner cone/wedge if cheap/readable

Add simple 2D handles if the editor has existing handle patterns:

* drag origin
* drag target

If handle infrastructure is not straightforward, draw overlay first and leave direct dragging for later, but inspector editing must still work.

Behavior that must remain unchanged:

* Existing point light overlays unchanged.
* Sector/line/face selection unchanged.
* Dynamic spotlight save/load unchanged.

Manual smoke:

* selected spotlight overlay is readable
* moving/editing origin/target updates overlay
* angle/range changes update wedge
* no weird selection conflicts with sectors/lines/lights

### Phase 3: Runtime Dynamic Spotlight Rendering

Goal:

Make dynamic spotlights contribute to runtime lighting in the existing forward shader path.

Why it helps:

This validates the dynamic spotlight data model before adding pilot mode.

Files/functions likely touched:

* dynamic light selection/ranking helper files
* `SectorMeshPreview.cpp`
* embedded sector shader strings
* dynamic light uniform packing/upload code
* dynamic light debug text
* tests for selection/packing

Exact behavior that must remain unchanged:

* Dynamic point lights still render.
* Dynamic point flicker still works.
* Dynamic light count zero still preserves old visual output.
* Static lightmaps/AO/decal/emissive behavior unchanged.
* `MAX_DYNAMIC_LIGHTS` remains a total budget, not point + spot separately.

Risks/goblins:

* Shader uniform array changes breaking point lights.
* Cone math inverted or too harsh.
* Spotlights consuming all light slots unexpectedly.
* Selection too strict and dropping visible spotlights.
* Flicker accidentally affecting selection/ranking.

Non-goals:

* No shadow maps.
* No static baked spotlights.
* No per-sector/per-draw-record light lists.
* No clustered/tiled/deferred lighting.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant build/tests.

Final report expectations:

* State uniform layout changes.
* State dynamic light budget behavior.
* State selection/ranking behavior.
* State shader cone formula.
* State manual GUI smoke status.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 3 `Completed` only after Phase 3A and Phase 3B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 3A: Extend Runtime Light Selection And Packing For Spotlights

Goal:

Include dynamic spotlights in the existing dynamic light selection pipeline without yet changing shader output if feasible.

Implementation guidance:

Use one unified selected dynamic light list:

```text
MAX_DYNAMIC_LIGHTS = 8 total
```

not separate budgets for point and spot lights.

Add a runtime/packed light kind:

```text
Point
Spot
```

For spotlights, pack:

```text
position
direction
color
intensity
range
innerConeCos
outerConeCos
kind/type
flicker settings if upload applies flicker later
```

Selection/candidate behavior:

* dynamic point lights keep existing receiver-bound selection
* dynamic spotlights should be candidates if their range sphere overlaps visible receiver bounds or owner sector is visible
* first pass may be conservative and not cone-test receiver bounds
* ranking can use the same receiver-distance contribution score as points, with optional cone relevance later
* selection/ranking must use base intensity
* flicker applies only during upload, same rule as dynamic point lights
* fallbackDrawAll remains safe and considers all valid enabled dynamic point/spot lights

Tests:

* points and spots share total cap
* fallback considers both point and spot lights
* spot range sphere overlap includes candidate
* hidden irrelevant spotlight is excluded in normal valid visibility
* deterministic ranking/tie behavior
* flicker spotlight still ranks by base intensity

#### Phase 3B: Add Spotlight Shader Contribution

Goal:

Add actual spotlight cone contribution to the sector shader.

Implementation guidance:

Extend the existing dynamic light shader uniforms carefully.

Possible packed fields:

```glsl
uniform int dynamicLightTypes[MAX_DYNAMIC_LIGHTS]; // 0 point, 1 spot
uniform vec3 dynamicLightDirections[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightInnerConeCos[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightOuterConeCos[MAX_DYNAMIC_LIGHTS];
```

Use project naming/style.

Point lights should behave exactly as before.

Spotlight contribution:

```text
toLight = light.position - fragWorldPosition
distance attenuation as existing dynamic point light
L = normalize(toLight)
NdotL = max(dot(normal, L), 0)

spotDirection points from light origin toward target
fragmentDirectionFromLight = normalize(fragWorldPosition - light.position)
coneDot = dot(normalize(spotDirection), fragmentDirectionFromLight)

coneAtten = smoothstep(outerConeCos, innerConeCos, coneDot)
dynamicDirect += color * intensity * distanceAtten * NdotL * coneAtten
```

Handle invalid/degenerate direction safely.

Outer cone should be wider than inner cone. If equal, handle as a hard-ish cone without NaNs.

Do not apply baked AO to dynamic spotlight direct lighting.

Do not affect bloom-source shader except compile compatibility if needed.

Manual smoke:

* one spotlight aimed at floor creates visible cone-like lighting
* changing target moves cone
* changing inner/outer cone changes softness/width
* dynamic point lights still behave
* flicker works on spotlight if enabled
* decals/bloom/AO still behave

### Phase 4: 3D Spotlight Visualization

Goal:

Draw a readable 3D overlay for selected dynamic spotlights.

Why it helps:

You need to see what you are aiming before pilot mode is worth using.

Files/functions likely touched:

* `SectorMeshPreview.cpp`
* 3D overlay/debug drawing code
* selected object/light state code

Exact behavior that must remain unchanged:

* Rendering/culling unchanged.
* Dynamic spotlight shader behavior unchanged.
* Existing selection/highlight unchanged.

Risks/goblins:

* Overlay depth fighting or being hidden.
* Cone math not matching shader cone.
* Drawing too much clutter for unselected lights.

Non-goals:

* No pilot mode yet.
* No 3D transform gizmo.
* No shadow visualization.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Manual smoke:

* select dynamic spotlight
* see origin marker
* see target line
* see outer cone
* optionally see inner cone
* cone roughly matches visual light contribution

Final report expectations:

* State overlay components.
* State whether inner/outer cones are both drawn.
* State manual GUI smoke status.

How to update this plan after completion:

* Mark Phase 4A and Phase 4 `Completed` if done.
* Add date, summary, verification results, and behavior notes.

#### Phase 4A: Draw Selected Spotlight Cone Overlay In 3D Preview

Goal:

Draw selected dynamic spotlight orientation/range/cone in 3D preview.

Implementation guidance:

When a dynamic spotlight is selected in 3D preview, draw:

```text
origin marker
target marker or endpoint
line from origin to target
outer cone wireframe at range
inner cone wireframe if cheap
```

Use existing debug draw helpers/raylib primitives where possible.

Prefer overlay readability over perfect geometry.

The cone should use the same direction/range/inner/outer values as the shader.

Do not add persistent settings.

### Phase 5: Spotlight Pilot Mode

Goal:

Add a simple “become the light” workflow for aiming dynamic spotlights in 3D mode.

Why it helps:

This avoids building a full 3D transform/gizmo system while making spotlights pleasant to place.

Files/functions likely touched:

* `SectorMeshPreview.cpp/.h`
* sector editor 3D mode UI/bottom panel
* selected light mutation/action code
* input/camera handling around 3D preview
* undo/dirty handling if present

Exact behavior that must remain unchanged:

* Normal 3D camera mode unchanged when not piloting.
* Selection unchanged when not piloting.
* Dynamic spotlight data only mutates on Apply, not while previewing unless existing editor model requires live preview.
* Cancel restores old spotlight transform.
* Save/load unchanged.

Risks/goblins:

* Camera state not restored after cancel/apply.
* Light transform dirtying document during preview instead of apply.
* Input focus/cursor lock conflicts.
* Accidentally piloting point lights or static lights.
* Losing original spotlight transform.

Non-goals:

* No generic 3D gizmo.
* No static spotlight pilot mode in this plan unless trivially shared and explicitly desired later.
* No gameplay flashlight.
* No shadow maps.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Manual smoke required for confidence:

* select dynamic spotlight
* enter pilot mode
* camera jumps to spotlight position looking at target
* move/look with normal controls
* Apply updates position/target and marks document dirty
* Cancel restores old spotlight and camera state
* leaving 3D mode while piloting behaves safely
* deleting/losing selection while piloting behaves safely

Final report expectations:

* State pilot controls/UI labels.
* State apply/cancel behavior.
* State camera restore behavior.
* State dirty/undo behavior if applicable.
* State manual GUI smoke status.

How to update this plan after completion:

* Mark Phase 5A and Phase 5 `Completed` if done.
* Add date, summary, verification results, and behavior notes.

#### Phase 5A: Add Pilot Selected Spotlight Apply Cancel Workflow

Goal:

Implement pilot mode for selected dynamic spotlights.

Implementation guidance:

Add UI controls in 3D mode when a dynamic spotlight is selected:

```text
Pilot Light
Apply
Cancel
```

Possible workflow:

1. Select dynamic spotlight.
2. Click `Pilot Light`.
3. Store:

    * original spotlight position
    * original spotlight target
    * original camera pose/state
4. Move camera to spotlight position.
5. Aim camera at spotlight target.
6. While piloting, normal 3D camera controls move the pilot camera.
7. Optionally show live preview by temporarily using camera pose as spotlight pose.
8. `Apply` writes:

    * spotlight position = camera position
    * spotlight target = camera position + cameraForward * preservedTargetDistance
9. `Cancel` restores original spotlight position/target and original camera pose.
10. Exiting mode/selection loss should cancel safely unless apply was already pressed.

Target distance:

* preserve original distance from position to target if finite and reasonable
* otherwise use default distance such as `range * 0.5` or `4.0` world units

Dirty behavior:

* Apply marks document dirty.
* Cancel should not mark dirty.
* If live preview mutates data during piloting, Cancel must restore and dirty behavior must remain sane. Prefer temporary preview state if practical.

### Phase 6: Polish Tests Documentation And Completion

Goal:

Tune defaults, strengthen tests/docs, and close the dynamic spotlight plan.

Why it helps:

This locks down the first useful version and records deferred static/shadow work.

Files/functions likely touched:

* spotlight code/comments
* tests
* docs
* this plan document
* maybe editor labels/defaults

Exact behavior that must remain unchanged:

* Dynamic point lights still work.
* Dynamic point flicker still works.
* Static lights remain baked-only.
* No shadow-map scope.
* No static spotlight scope.

Risks/goblins:

* Over-tuning without manual map feedback.
* Weak tests around serialization/shader packing.
* Accidentally broadening into static spotlight baking.

Non-goals:

* No static spotlights.
* No shadow maps.
* No PCF.
* No flashlight gameplay item.
* No generic object/entity system.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run full relevant test suite.

Final report expectations:

* State final defaults.
* State docs updated.
* State verification results.
* State manual smoke results if performed.
* State known deferred work.

How to update this plan after completion:

* Mark Phase 6A and Phase 6 `Completed`.
* If all phases are complete, ensure all parent phases are `Completed`.
* Leave a final completion note.

#### Phase 6A: Tune Defaults Strengthen Tests Update Docs And Close Plan

Goal:

Finish the dynamic spotlight feature pass cleanly.

Implementation guidance:

Tune and document:

* default range
* default inner/outer cone angles
* default target distance
* pilot target distance behavior
* overlay readability
* dynamic light budget behavior
* dynamic point + spot sharing one max count
* no static spotlight support yet
* no shadow-map support yet

Ensure tests cover:

* dynamic spotlight serialization
* missing/default load behavior
* dynamic spotlight hash isolation
* dynamic point and spot shared cap behavior
* spot packing safe degenerate direction
* cone angle clamp/order behavior
* pilot mode helper math if extracted and testable

Manual smoke checklist:

* add dynamic spotlight
* edit position/target/range/cones/color/intensity/flicker
* save/reload
* see 2D overlay
* see 3D overlay
* see runtime cone lighting
* pilot mode apply/cancel works
* dynamic point lights still work
* dynamic light debug counts still make sense
* portal culling still works
* static lightmap is not invalidated by dynamic spotlight edits

## Deferred Decisions For Later Plans

These are intentionally out of scope:

* Static baked spotlights.
* Static spotlight source hash and lightmap baking.
* Shadow maps.
* PCF.
* Shadow-casting dynamic spotlights.
* Gameplay flashlight item.
* Point-light cubemap shadows.
* 3D transform gizmos.
* Clustered/tiled/deferred lighting.
* Dynamic sector lighting policy.
* Normal mapping.
* Projected cookies.
* Volumetric cones/fog.

## Final Completion Criteria

This plan is complete when:

* dynamic spotlights exist separately from dynamic point lights
* dynamic spotlights save/load correctly
* dynamic spotlights are editable in the editor
* dynamic spotlights render in the forward dynamic lighting shader
* dynamic point and spot lights share the same total dynamic light cap
* dynamic point lights still behave as before
* dynamic spotlight flicker works if enabled
* selected dynamic spotlights have a readable 3D cone overlay
* pilot mode can aim a selected dynamic spotlight
* Apply/Cancel behavior is safe
* static light baking/source hash remains unchanged
* no static spotlight/shadow-map/flashlight scope leaked into this implementation
