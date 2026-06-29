# Sector Static Baked Spotlight Plan

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

```plan-state-json id="static-spotlights"
{
  "plan_id": "sector_static_baked_spotlight_plan",
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
      "title": "Static Spotlight Data And Serialization",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_01a",
      "title": "Add Static Spotlight Data Model And JSON Round Trip",
      "type": "pass",
      "parent": "phase_01",
      "status": "Not Started"
    },
    {
      "id": "phase_02",
      "title": "Static Spotlight Editor Authoring",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_02a",
      "title": "Add Static Spotlight Tool And Inspector Fields",
      "type": "pass",
      "parent": "phase_02",
      "status": "Not Started"
    },
    {
      "id": "phase_02b",
      "title": "Reuse Spotlight Overlay And Pilot Mode For Static Spotlights",
      "type": "pass",
      "parent": "phase_02",
      "status": "Not Started"
    },
    {
      "id": "phase_03",
      "title": "Static Spotlight Lightmap Baking",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_03a",
      "title": "Add Static Spotlight Source Hash And Bake Inputs",
      "type": "pass",
      "parent": "phase_03",
      "status": "Not Started"
    },
    {
      "id": "phase_03b",
      "title": "Evaluate Static Spotlight Direct Lighting In Baker",
      "type": "pass",
      "parent": "phase_03",
      "status": "Not Started"
    },
    {
      "id": "phase_04",
      "title": "Bake Preview And Debug Polish",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_04a",
      "title": "Add Static Spotlight Bake Debug And Smoke Map Support",
      "type": "pass",
      "parent": "phase_04",
      "status": "Not Started"
    },
    {
      "id": "phase_05",
      "title": "Polish Tests Documentation And Completion",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_05a",
      "title": "Tune Defaults Strengthen Tests Update Docs And Close Plan",
      "type": "pass",
      "parent": "phase_05",
      "status": "Not Started"
    }
  ]
}
```

## Current Progress

| Phase / Pass                                                           | Status      | Date | Notes                                                                      |
| ---------------------------------------------------------------------- | ----------- | ---- | -------------------------------------------------------------------------- |
| Phase 1: Static Spotlight Data And Serialization                       | Not Started |      | Parent phase.                                                              |
| Phase 1A: Add Static Spotlight Data Model And JSON Round Trip          | Not Started |      | First executable pass. Data/persistence only, no UI or bake behavior.      |
| Phase 2: Static Spotlight Editor Authoring                             | Not Started |      | Parent phase.                                                              |
| Phase 2A: Add Static Spotlight Tool And Inspector Fields               | Not Started |      | Add editor tool and inspector.                                             |
| Phase 2B: Reuse Spotlight Overlay And Pilot Mode For Static Spotlights | Not Started |      | Static spotlights reuse dynamic spotlight aim/overlay workflow.            |
| Phase 3: Static Spotlight Lightmap Baking                              | Not Started |      | Parent phase.                                                              |
| Phase 3A: Add Static Spotlight Source Hash And Bake Inputs             | Not Started |      | Static spotlights become bake-affecting inputs.                            |
| Phase 3B: Evaluate Static Spotlight Direct Lighting In Baker           | Not Started |      | Cone attenuation plus existing bake occlusion/shadow path where practical. |
| Phase 4: Bake Preview And Debug Polish                                 | Not Started |      | Parent phase.                                                              |
| Phase 4A: Add Static Spotlight Bake Debug And Smoke Map Support        | Not Started |      | Debug/readability pass.                                                    |
| Phase 5: Polish Tests Documentation And Completion                     | Not Started |      | Parent phase.                                                              |
| Phase 5A: Tune Defaults Strengthen Tests Update Docs And Close Plan    | Not Started |      | Final cleanup.                                                             |

## Execution Tracking Rules

* Each pass must leave the project buildable and runnable.
* Each pass final report must state whether source code changed.
* Each implementation pass must update this document before finishing.
* The update should be small and local.
* Do not rewrite unrelated phases when marking progress.
* If behavior is intended to remain unchanged, explicitly state that.
* If a pass changes serialization, generated data, public APIs, runtime behavior, cache invalidation, shader behavior, editor behavior, lightmap hashing, baking, or build/test behavior, clearly say so.
* Do not claim manual GUI verification unless it was actually performed.
* If a pass produces only a plan/audit and no source changes, state that clearly.
* If a pass is too broad, split it into smaller child passes and stop without source changes.

## Goal And Desired End State

Add **static baked spotlights** to the sector engine.

Desired end state:

* Static spotlights are separate from static point lights.
* Static spotlights save/load correctly.
* Static spotlights are editable in the editor.
* Static spotlights reuse the same practical `position + target` authoring model as dynamic spotlights.
* Static spotlights can reuse the 2D/3D spotlight overlay and pilot workflow where practical.
* Static spotlights affect baked lightmaps.
* Static spotlights affect the lightmap source hash.
* Static point lights continue to work unchanged.
* Dynamic point lights continue to work unchanged.
* Dynamic spotlights continue to work unchanged.
* Runtime dynamic light selection/shader uniforms are unchanged.
* Shadow maps remain out of scope.

Core authoring representation:

```text
static spotlight = position + target + range + innerConeDegrees + outerConeDegrees + sourceRadius
direction = normalize(target - position)
```

Static spotlights are bake lights. They are not runtime dynamic lights.

## Dependency Direction Rules

* Static spotlight authored data may be serialized in topology/map data.
* Static spotlight edits must eventually affect the baked lightmap source hash once bake support lands.
* Static spotlight bake evaluation may reuse existing static point-light bake infrastructure where practical.
* Static spotlight editor tools may reuse dynamic spotlight overlay/pilot UI where practical.
* Runtime dynamic light selection must not depend on static spotlights.
* Dynamic shader uniform layout must not change for static spotlights.
* Dynamic spotlight behavior must remain unchanged.
* Static spotlight pilot mode may mutate selected authored static spotlight data only on Apply.
* Cancel must restore the original authored static spotlight transform.

## Proposed Phases

### Phase 1: Static Spotlight Data And Serialization

Goal:

Add static spotlight data and JSON round-trip without editor UI or bake behavior changes.

Why it helps:

This stabilizes the map format before editor and baker changes.

Files/functions likely touched:

* `sources/sector_demo/SectorTopologyTypes.h`
* `sources/sector_demo/SectorTopologySerialization.cpp`
* topology validation helpers
* serialization tests

Exact behavior that must remain unchanged:

* Existing static point lights save/load unchanged.
* Existing dynamic point lights save/load unchanged.
* Existing dynamic spotlights save/load unchanged.
* Existing dynamic flicker fields save/load unchanged.
* Lightmap source hash should not change yet unless this pass explicitly wires hash behavior and tests it.
* Renderer output unchanged.
* Editor UI unchanged.

Risks/goblins:

* Confusing static and dynamic spotlight semantics.
* Adding flicker to static baked spotlights by accident.
* Adding runtime shader behavior by accident.
* Source hash changing before bake support is implemented.
* Target equal to position producing invalid direction later.

Non-goals:

* No editor tool.
* No inspector.
* No overlay.
* No pilot mode.
* No lightmap bake contribution.
* No shader changes.
* No shadow maps.
* No dynamic runtime changes.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant serialization tests.

Final report expectations:

* State files changed.
* State serialized field names.
* State default values.
* State validation/clamp behavior.
* Confirm renderer/editor behavior unchanged.
* Confirm whether source hash behavior changed or did not change.
* State verification commands/results.

How to update this plan after completion:

* Mark Phase 1A `Completed` in JSON and table.
* Mark Phase 1 `Completed` if Phase 1A is complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 1A: Add Static Spotlight Data Model And JSON Round Trip

Goal:

Add authored static spotlight persistence only.

Implementation guidance:

Add a new type separate from both static point lights and dynamic spotlights, for example:

```cpp
struct SectorTopologyStaticSpotLight {
    int id = 0;
    Vector3 position = {};
    Vector3 target = {};
    Color color = WHITE;
    float intensity = 1.0f;
    float radius = 8.0f;
    float sourceRadius = 0.25f;
    float innerConeDegrees = 20.0f;
    float outerConeDegrees = 35.0f;
};
```

Use actual project naming/style.

Prefer `radius` if the existing static point-light type uses `radius`; prefer `range` only if current dynamic spotlight code has already standardized that naming and it is clearly better. Avoid mixing terms in the same inspector unless necessary.

Add map-level storage such as:

```cpp
std::vector<SectorTopologyStaticSpotLight> staticSpotLights;
```

Use stable positive integer IDs.

Serialize under a distinct root field such as:

```json
"staticSpotLights": []
```

Load missing field as empty.

Omit empty/default fields on save if that matches existing save style.

Suggested defaults:

```text
color = white
intensity = 1.0
radius = 8.0 world units / matching current static point light authoring convention
sourceRadius = 0.25 world units / matching static point light convention
innerConeDegrees = 20
outerConeDegrees = 35
target = position + default forward/down-ish direction when created by editor later
```

Validation/clamping:

* position/target must be valid vectors
* color must be valid color
* intensity must be finite and non-negative
* radius must be finite and positive
* sourceRadius must be finite and non-negative
* inner/outer cone must be finite and clamped to sane ranges
* outer cone must be greater than or equal to inner cone
* target equal to position should remain loadable, but runtime/bake code must later handle degenerate direction safely

Recommended cone clamps:

```text
innerConeDegrees: 0.0 .. 179.0
outerConeDegrees: 0.0 .. 179.0
outerConeDegrees >= innerConeDegrees
```

Tests:

* missing `staticSpotLights` loads as empty
* default static spotlight fields load correctly
* static spotlight save/load round-trip
* omit-default behavior if applicable
* invalid numeric fields handled consistently with existing static point light behavior
* existing static point/dynamic point/dynamic spot serialization tests still pass

### Phase 2: Static Spotlight Editor Authoring

Goal:

Add editor creation/editing for static spotlights.

Why it helps:

Static baked spotlights need the same usable authoring workflow as dynamic spotlights before they become bake inputs.

Files/functions likely touched:

* sector editor topology/action files
* light tool panel/tool mode files
* light inspector files
* 2D overlay/render files
* selection files
* `SectorMeshPreview` 3D overlay/pilot files if sharing is clean
* editor/topology tests where available

Exact behavior that must remain unchanged:

* Static point light tool unchanged.
* Dynamic point light tool unchanged.
* Dynamic spotlight tool unchanged.
* Dynamic spotlight pilot mode unchanged.
* Runtime rendering unchanged.
* Lightmap bake contribution unchanged until Phase 3.

Risks/goblins:

* UI confusing static/dynamic categories.
* Accidentally adding flicker controls to static spotlights.
* SourceRadius omitted even though static baked soft shadows may need it.
* Dirty/cache invalidation not following existing static light edit behavior.
* Pilot mode accidentally mutating dynamic spotlights when static selected, or vice versa.

Non-goals:

* No bake contribution yet.
* No shadow maps.
* No dynamic runtime changes.
* No generic 3D transform gizmo.

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
* State overlay/pilot reuse behavior.
* State manual GUI smoke status.
* State verification commands/results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 2 `Completed` only after Phase 2A and Phase 2B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 2A: Add Static Spotlight Tool And Inspector Fields

Goal:

Add static spotlight creation and inspector editing.

Implementation guidance:

Add a tool/button label such as:

```text
Static Spot
```

Keep static/dynamic categories clear. Do not turn static/dynamic into a checkbox on one light object.

Placement:

* click in 2D sector to place spotlight origin
* default position height: similar to static point light default, probably floor + 1.8 world units
* default target: origin plus a simple forward offset in X/Z and slightly downward if practical
* default radius/sourceRadius/inner/outer/intensity/color from Phase 1 defaults

Inspector fields:

```text
Position X/Y/Z
Target X/Y/Z
Radius
Source radius
Inner cone degrees
Outer cone degrees
Intensity
Color
```

Do not add:

```text
Enabled
Flicker
Flicker speed
Flicker amount
Shadow map fields
```

unless existing static point lights already have an enabled flag. Follow current static point-light semantics.

Editing fields should mark the document dirty and use the same lightmap dirty/source invalidation path as static point light edits where appropriate. If bake/hash support is not landed yet, still use the same document dirty behavior as other static light edits.

Manual smoke:

* add static spotlight
* select it
* edit fields
* save/reload
* confirm persistence
* confirm static point/dynamic light tools still work

#### Phase 2B: Reuse Spotlight Overlay And Pilot Mode For Static Spotlights

Goal:

Let static spotlights use the same practical visualization and aiming workflow as dynamic spotlights.

Implementation guidance:

Reuse dynamic spotlight overlay code where practical.

2D overlay:

* origin marker
* target marker
* line from origin to target
* top-down cone/wedge using outer cone angle
* optional inner cone/wedge

3D overlay:

* origin marker
* target line
* outer cone wireframe
* optional inner cone wireframe

Pilot mode:

* selected static spotlight can enter pilot mode
* camera moves to spotlight position and looks at target
* Apply writes position/target to static spotlight and marks document dirty
* Cancel restores original static spotlight transform and camera state
* dynamic spotlight pilot mode remains unchanged
* point lights cannot be piloted as spotlights

Manual smoke:

* select static spotlight
* see 2D overlay
* see 3D overlay
* pilot static spotlight
* Apply updates static spotlight
* Cancel restores
* dynamic spotlight pilot still works

### Phase 3: Static Spotlight Lightmap Baking

Goal:

Make static spotlights affect baked direct lighting and lightmap source hash.

Why it helps:

This is the actual baked-light feature. Static spotlights should become authorable, bake-affecting cone lights.

Files/functions likely touched:

* `sources/sector_demo/SectorLightmap.cpp`
* lightmap source hash functions
* bake light conversion helpers
* static light bake evaluation code
* lightmap tests

Exact behavior that must remain unchanged:

* Static point light bake behavior unchanged.
* Directional bake behavior unchanged.
* AO behavior unchanged.
* Bounce behavior unchanged unless existing direct-light loops naturally feed it later.
* Dynamic lights remain runtime-only and excluded from static bake hash.
* Runtime dynamic shader unchanged.

Risks/goblins:

* Cone attenuation backwards due to direction convention.
* Inner/outer cone math inverted.
* Degenerate target direction causing NaNs.
* Static spotlight source hash missing important fields.
* Bake becoming unexpectedly slower if loops are inefficient.
* Static point soft-shadow/source-radius behavior accidentally changed.

Non-goals:

* No shadow maps.
* No dynamic spotlight runtime changes.
* No static spotlight runtime forward shader contribution.
* No new bake UI beyond existing Bake Lightmaps flow.
* No volumetric shafts.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant lightmap tests.

Final report expectations:

* State source hash fields included.
* State bake contribution formula.
* State degenerate direction behavior.
* State tests/checks run.
* State manual bake smoke status if performed.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 3 `Completed` only after Phase 3A and Phase 3B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 3A: Add Static Spotlight Source Hash And Bake Inputs

Goal:

Make static spotlights known to the bake pipeline as source-hash-affecting bake inputs.

Implementation guidance:

Add static spotlights to lightmap source hash.

Hash should include stable sorted data, likely by ID:

```text
id
world position
world target or normalized direction + distance if that is the existing convention
color
intensity
world radius
world sourceRadius
innerConeDegrees
outerConeDegrees
```

Use the same world-unit conversion style as static point lights.

Sort by ID before hashing, matching existing static point light hash behavior.

Add or reuse bake input conversion helpers so the baker can consume static spotlights in world units.

Do not evaluate lighting yet if this pass is kept small; or if evaluation is trivial, do not broaden beyond hash/input plumbing unless explicitly selected.

Tests:

* static spotlight edit changes source hash
* static spotlight order does not affect source hash
* changing position/target/radius/sourceRadius/intensity/color/cone angles changes hash
* dynamic spotlight edits still do not affect source hash
* static point light hash tests still pass

#### Phase 3B: Evaluate Static Spotlight Direct Lighting In Baker

Goal:

Add static spotlight contribution to baked direct lightmaps.

Implementation guidance:

Reuse static point light direct evaluation where practical.

Conceptual formula:

```text
toLight = light.position - samplePosition
distance attenuation as existing static point light
L = normalize(toLight)
NdotL = max(dot(surfaceNormal, L), 0)

spotDirection = normalize(target - position)
fragmentDirectionFromLight = normalize(samplePosition - position)
coneDot = dot(spotDirection, fragmentDirectionFromLight)
coneAtten = smoothstep(outerConeCos, innerConeCos, coneDot)

contribution = color * intensity * distanceAtten * NdotL * coneAtten
```

Important:

* Use the same direction convention as dynamic spotlights.
* Use the same inner/outer cone behavior as dynamic spotlights where practical.
* Handle degenerate target safely.
* Do not apply contribution outside radius.
* Use existing occlusion/shadow ray path where practical.
* If static point lights support soft shadows through `sourceRadius`, static spotlights should use equivalent sourceRadius behavior if practical.
* If sourceRadius integration is not straightforward, implement hard occlusion first and document soft-shadow follow-up. Prefer matching static point light behavior if reasonable.

Tests:

* sample inside cone receives light
* sample outside outer cone receives little/no light
* sample between inner and outer cone receives partial contribution
* degenerate target produces no NaNs and safe no-light or default-direction behavior
* occlusion blocks spotlight contribution if existing point light occlusion helper is shared
* static point light bake tests still pass

Manual bake smoke:

* place static spotlight aimed at floor/wall
* bake lightmaps
* confirm cone-shaped baked light
* change target/cone/radius and rebake
* confirm baked result changes
* confirm source hash invalidation behaves

### Phase 4: Bake Preview And Debug Polish

Goal:

Make static spotlight bake behavior easier to verify.

Why it helps:

Static lights are hard to debug if the only feedback is “bake looked odd.”

Files/functions likely touched:

* debug text/status
* light inspector labels
* overlay code
* docs/test maps

Exact behavior that must remain unchanged:

* Bake math unchanged except debug/polish.
* Dynamic lighting unchanged.
* Static point lights unchanged.

Risks/goblins:

* Debug display becoming noisy.
* Creating large test assets accidentally.
* Confusing static spotlight overlays with dynamic selected runtime lights.

Non-goals:

* No volumetric shafts.
* No shadow maps.
* No runtime dynamic changes.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Final report expectations:

* State debug/polish changes.
* State manual bake smoke status.
* State verification results.

How to update this plan after completion:

* Mark Phase 4A and Phase 4 `Completed`.
* Add date, summary, verification results, and behavior notes.

#### Phase 4A: Add Static Spotlight Bake Debug And Smoke Map Support

Goal:

Improve visibility/readability of static spotlight bake state.

Implementation guidance:

Possible small improvements:

* inspector label clearly says `Static Spot`
* 3D overlay color/style distinguishes static spotlights from dynamic spotlights
* bake status/debug text mentions static spot count if existing UI has a suitable place
* optional small saved test map only if project already keeps such assets and user expects it

Do not add major UI.

Manual smoke:

* static spotlight overlays are distinguishable from dynamic spotlights
* bake status/source-hash invalidation is understandable
* no dynamic light debug regression

### Phase 5: Polish Tests Documentation And Completion

Goal:

Tune defaults, strengthen tests/docs, and close the static baked spotlight plan.

Why it helps:

This locks down the first usable baked spotlight version and records deferred shadow/volumetric work.

Files/functions likely touched:

* spotlight code/comments
* tests
* docs
* this plan document
* maybe defaults/labels

Exact behavior that must remain unchanged:

* Dynamic point lights work unchanged.
* Dynamic spotlights work unchanged.
* Static point lights work unchanged.
* No shadow-map scope.
* No volumetric light shaft scope.

Risks/goblins:

* Over-tuning without enough visual bake tests.
* Weak tests around source hash.
* Accidentally broadening into shadows or volumetrics.

Non-goals:

* No shadow maps.
* No PCF.
* No flashlight gameplay item.
* No volumetric cones/godrays.
* No runtime static spotlight contribution.
* No renderer architecture changes.

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

* Mark Phase 5A and Phase 5 `Completed`.
* If all phases are complete, ensure all parent phases are `Completed`.
* Leave a final completion note.

#### Phase 5A: Tune Defaults Strengthen Tests Update Docs And Close Plan

Goal:

Finish the static spotlight feature pass cleanly.

Implementation guidance:

Tune and document:

* default radius
* default sourceRadius
* default target distance
* default inner/outer cone angles
* source hash behavior
* bake behavior
* relationship to dynamic spotlights
* no runtime dynamic behavior
* no shadow-map behavior

Ensure tests cover:

* static spotlight serialization
* missing/default load behavior
* static spotlight source hash inclusion
* source hash sort stability
* all bake-affecting fields affect hash
* dynamic spotlights still excluded from source hash
* inside/outside/penumbra cone bake contribution
* degenerate target safety
* static point light bake regression safety

Manual smoke checklist:

* add static spotlight
* edit position/target/radius/sourceRadius/cones/color/intensity
* save/reload
* see 2D/3D overlay
* pilot static spotlight apply/cancel works
* bake lightmaps
* see cone-shaped baked light
* source hash invalidates when static spotlight changes
* dynamic point/spot lights still work
* dynamic light debug counts still make sense
* portal culling still works

## Deferred Decisions For Later Plans

These are intentionally out of scope:

* Shadow maps.
* PCF.
* Shadow-casting dynamic spotlights.
* Gameplay flashlight item.
* Volumetric spotlight shafts.
* God rays.
* Projected cookies/gobos.
* Point-light cubemap shadows.
* Clustered/tiled/deferred lighting.
* Dynamic sector lighting policy.
* Normal mapping.
* Runtime contribution from static spotlights.

## Final Completion Criteria

This plan is complete when:

* static spotlights exist separately from static point lights and dynamic spotlights
* static spotlights save/load correctly
* static spotlights are editable in the editor
* static spotlights reuse the spotlight overlay/pilot workflow where practical
* static spotlights affect lightmap source hash
* static spotlights bake cone-shaped direct lighting into lightmaps
* static point lights still behave as before
* dynamic point lights still behave as before
* dynamic spotlights still behave as before
* no dynamic runtime shader changes are made for static spotlights
* no shadow-map/volumetric/flashlight scope leaks into this implementation
