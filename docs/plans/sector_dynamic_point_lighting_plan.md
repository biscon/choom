# Sector Dynamic Point Lighting Implementation Plan

## Summary

Add forward-rendered dynamic point lights to the sector renderer.

The first implementation should be boring and practical:

* dynamic point lights only
* no spotlights
* no shadow maps
* no PCF
* no deferred/clustered/tiled renderer
* no dynamic floors/doors
* no full entity/object ownership system
* no reuse of existing baked `staticLights` as runtime dynamic lights

Static lights remain baked lightmap inputs.

Dynamic point lights are separate authored/runtime lights. They render in the sector fragment shader, are culled/ranked using the completed portal visibility system, and do not affect the baked lightmap source hash.

Target shader composition:

```text
lighting = bakedDirect + sectorAmbient * bakedAO + dynamicDirect
finalRgb = surfaceRgb * lighting
```

Dynamic light count zero must preserve the current visual output.

## Plan State

```plan-state-json
{
  "plan": "sector_dynamic_point_lighting",
  "version": 1,
  "status": "planned",
  "current_phase": "phase_01_dynamic_point_light_data_and_editor",
  "phases": [
    {
      "id": "phase_01_dynamic_point_light_data_and_editor",
      "status": "pending"
    },
    {
      "id": "phase_02_shader_dynamic_point_light_support",
      "status": "pending"
    },
    {
      "id": "phase_03_visibility_aware_light_selection",
      "status": "pending"
    },
    {
      "id": "phase_04_dynamic_light_debug_and_stability",
      "status": "pending"
    },
    {
      "id": "phase_05_polish_tests_and_plan_completion",
      "status": "pending"
    }
  ]
}
```

## Global Rules

Keep every phase compiling and runnable.

Use small, reviewable changes.

Update this plan document after each completed phase with:

* phase status
* date
* summary
* files changed
* verification results
* any follow-up notes

Dynamic point lights must not affect static lightmap baking or the lightmap source hash.

Existing `staticLights` remain baked/static lights.

Do not add shadow maps, PCF, spotlights, cookies, volumetrics, clustered lighting, deferred rendering, or dynamic sectors in this plan.

Do not add GUI automation or screenshot tests.

Prefer non-GUI unit/data tests for serialization, ranking, selection, and packing helpers.

## Phase 1: Dynamic Point Light Data and Editor

### Goal

Add authored dynamic point lights as a separate map-level concept and make them editable in the sector editor, without changing rendering output yet.

### Requirements

Add a new dynamic point light type separate from `SectorTopologyStaticPointLight`.

Suggested authored type:

```cpp
struct SectorTopologyDynamicPointLight {
    int id = 0;
    Vector3 position = {};
    Color color = WHITE;
    float intensity = 1.0f;
    float radius = 8.0f;
    bool enabled = true;
};
```

Use the same coordinate/unit convention as existing static lights in saved topology data. Convert to world units only for runtime/shader use.

Add storage to the topology/map model:

```cpp
std::vector<SectorTopologyDynamicPointLight> dynamicPointLights;
```

Use stable positive integer IDs.

Serialize under a distinct root field, for example:

```json
"dynamicPointLights": []
```

Omit the field when empty if that matches the project’s existing save style.

Load missing `dynamicPointLights` as empty.

Validate bad data defensively:

* invalid ID
* non-object entries
* bad position
* bad color
* negative radius
* negative intensity
* non-boolean enabled

Do not include dynamic point lights in `ComputeSectorLightmapSourceHash()`.

Add tests proving dynamic point light changes do not affect the baked lightmap source hash.

Add a separate editor tool/control for dynamic lights. Do not make this a checkbox on static lights.

Suggested UI:

```text
Static Light
Dynamic Light
```

Dynamic light placement should reuse the same general interaction style as static light placement:

* click in 2D sector to place
* default height: sector floor + existing static-light default height, probably 1.8 world units
* default color: white
* default intensity: 1.0
* default radius: 8.0 world units
* enabled: true

Add selection/editing support for dynamic lights.

Add a dynamic light inspector with:

* enabled
* position
* radius
* intensity
* color

Do not show `sourceRadius` for dynamic lights. That belongs to baked soft shadows and future dynamic shadow systems, not first-pass runtime point lights.

Render dynamic light editor/debug glyphs distinctly from static lights in 2D/preview editor overlays if there is already such a path. Keep it simple.

### Out of Scope

No shader lighting yet.

No dynamic-light culling/ranking yet.

No shadow settings.

No spotlights.

No light animation/flicker system.

### Verification

Run the relevant unit tests.

Run:

```bash
git diff --check
git diff --stat
git status --short
```

If tests exist for topology serialization/lightmap hashing, run them.

### Manual Smoke Test

In the editor:

* add a dynamic light
* select it
* edit radius/intensity/color/enabled
* save
* reload
* confirm it persists
* confirm no lightmap rebake/stale behavior is triggered solely by dynamic light edits

Expected rendering result: no visual dynamic lighting yet.

## Phase 2: Shader Dynamic Point Light Support

### Goal

Add forward per-pixel dynamic point light support to the sector shader, with a simple global upload path.

Dynamic light count zero must match current rendering.

### Requirements

Modify the embedded sector shader in `SectorMeshPreview.cpp`.

Pass these from vertex shader to fragment shader:

```glsl
fragWorldPosition
fragWorldNormal
```

Current sector meshes already upload world-space generated positions and normals. Current draw uses `MatrixIdentity()`, so local/world are effectively identical for this first implementation.

Add fixed-size dynamic point light uniforms.

Use:

```glsl
#define MAX_DYNAMIC_LIGHTS 8
```

or an equivalent C++/GLSL shared constant approach.

Suggested uniforms:

```glsl
uniform int dynamicLightCount;
uniform vec3 dynamicLightPositions[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightColors[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightRadii[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightIntensities[MAX_DYNAMIC_LIGHTS];
```

Add matching shader location fields and upload helpers in preview rendering code.

First shader formula:

```text
dynamicDirect = sum(point light contributions)

lighting = bakedDirect + sectorAmbient * bakedAO + dynamicDirect
lighting = clamp(lighting, 0, dynamicLightingClamp)
finalRgb = surfaceRgb * lighting
```

Use a clamp above 1 so dynamic lights can visibly brighten baked scenes, but avoid silly overbright explosions. Suggested first clamp:

```text
0..2
```

or

```text
0..4
```

Pick one and document it in code comments/debug notes.

Point light contribution:

```text
toLight = light.position - fragWorldPosition
distance = length(toLight)

if distance < radius:
    L = toLight / distance
    NdotL = max(dot(normalize(fragWorldNormal), L), 0)
    atten = saturate(1 - distance / radius)
    atten = atten * atten
    dynamicDirect += light.colorRgb * intensity * atten * NdotL
```

Handle very small distances without NaNs.

Alpha-tested middle textures must still discard before expensive lighting work.

Do not apply baked AO to dynamic lights in this first pass.

Do not add new samplers. Current material map slots are already reused:

* diffuse = base texture
* specular = lightmap
* normal = decal texture

Do not change bloom-source shader behavior except as necessary to keep it compiling. Dynamic lights should not affect emissive bloom source rendering.

For this phase, upload a simple global dynamic light list from authored dynamic point lights, capped at `MAX_DYNAMIC_LIGHTS`, without visibility-aware selection yet. It is acceptable to use insertion/ID order temporarily. Phase 3 will replace this with proper candidate selection/ranking.

Disabled lights, zero-radius lights, and zero-intensity lights should upload as inactive or be skipped.

### Out of Scope

No visibility-aware ranking yet.

No hysteresis.

No per-sector light lists.

No shadows or spotlights.

### Verification

Run the relevant CMake/CTest target set.

Run:

```bash
git diff --check
git diff --stat
git status --short
```

### Manual Smoke Test

Use a level with at least one dynamic point light.

Confirm:

* `dynamicLightCount = 0` or all dynamic lights disabled looks like the old renderer
* enabling one dynamic light visibly adds light
* light fades with radius
* wall/floor normals respond to light direction
* alpha-tested middle textures still render correctly
* decals still render correctly
* emissive bloom source is not accidentally affected by dynamic lights
* lightmapped scenes still retain baked lighting and AO

## Phase 3: Visibility-Aware Light Selection and Ranking

### Goal

Use portal visibility to select and rank dynamic lights before uploading them to the shader.

This lets the map contain more dynamic lights than the shader evaluates.

### Requirements

Add a runtime selected-light path that runs once per frame after the current portal visibility result is updated and before sector draw records are rendered.

Use the current portal visibility result:

* valid start sector
* visible sector IDs
* fallback draw-all
* total sector count

If visibility is invalid or `fallbackDrawAll` is true, consider all enabled dynamic point lights as candidates.

If visibility is valid and not fallback, prefer dynamic lights whose owning sector is visible.

Determine each dynamic light’s sector ownership using existing collision/sector lookup code where practical.

For first pass, one owning sector per light is enough.

If a light cannot be assigned to a sector:

* include it during fallback draw-all
* otherwise either exclude it or include it conservatively, but document the chosen behavior

Drop invalid candidates:

* disabled
* radius <= 0
* intensity <= 0
* invalid position
* invalid color

Drop lights outside their influence radius from the camera as a cheap first cut.

Use contribution scoring instead of distance-only sorting.

Suggested score:

```text
brightness = max(color.r, color.g, color.b)
atten = saturate(1 - distance(camera, light.position) / radius)
atten = atten * atten
score = intensity * brightness * atten
```

Optional radius bias:

```text
score *= sqrt(radius)
```

Keep top `MAX_DYNAMIC_LIGHTS`.

Use deterministic tie-breaking, preferably by light ID.

Upload only the selected lights.

Add helper functions for candidate collection, scoring, sorting, and packing where they can be tested without GUI/screenshot tests.

### Out of Scope

No hysteresis yet unless trivial and isolated. Phase 4 handles stability/debug polish.

No per-sector or per-draw-record light lists.

No clustered/tiled/deferred lighting.

### Verification

Add tests for:

* stronger light beats weaker light
* nearby light beats far light when otherwise equal
* lights outside radius are dropped
* disabled lights are dropped
* visible-sector filtering excludes hidden-sector lights
* fallback draw-all considers all enabled lights
* max-light cap keeps top N
* tie-breaking is deterministic

Run tests.

Run:

```bash
git diff --check
git diff --stat
git status --short
```

### Manual Smoke Test

Create more than 8 dynamic point lights.

Confirm:

* only selected top lights affect the scene
* lights in hidden sectors generally stop consuming slots
* fallback draw-all still renders safely
* no visible geometry disappears
* dynamic lighting updates as the camera moves between sectors

## Phase 4: Dynamic Light Debug and Stability

### Goal

Add debug visibility into dynamic light selection and reduce obvious top-N flicker.

### Requirements

Add debug text/readout near the existing portal visibility debug output.

Show at least:

```text
dynamic lights: selected / candidates / total
```

Optionally show selected IDs:

```text
selected dynamic light ids: 3, 7, 12
```

Add a debug flag or obvious code path for disabling dynamic light rendering if the project already has similar toggles. Do not overbuild a settings UI.

Add selection stability/hysteresis if top-N flicker is visible or easy to prevent cleanly.

Suggested hysteresis:

```text
Keep previously selected lights if their current score is still close enough.
Only replace a retained light if a new candidate score is at least 15-25% higher.
```

Keep the algorithm deterministic.

If hysteresis is added, make sure:

* deleted/disabled lights are removed immediately
* lights outside influence radius are removed
* fallback draw-all does not retain invalid stale lights
* selected count never exceeds `MAX_DYNAMIC_LIGHTS`

Document how the selected dynamic light list is updated each frame.

### Out of Scope

No per-sector light lists.

No shadow settings.

No spotlight UI.

No permanent complex performance overlay.

### Verification

Add non-GUI tests if helper functions make hysteresis testable.

Run tests.

Run:

```bash
git diff --check
git diff --stat
git status --short
```

### Manual Smoke Test

Create competing dynamic lights near the selection limit.

Move/rotate around.

Confirm:

* debug counts make sense
* selected IDs are stable enough
* lights do not flicker rapidly at threshold boundaries
* disabling a selected light removes it immediately
* fallback draw-all still behaves safely

## Phase 5: Polish, Tests, Documentation, and Plan Completion

### Goal

Finish the dynamic point lighting pass, tune defaults, document behavior, and close the plan.

### Requirements

Tune first-pass defaults:

* dynamic light radius
* intensity
* clamp max
* color handling
* debug text wording

Confirm static and dynamic light concepts are clearly separated in UI labels, inspector labels, comments, and docs.

Update `docs/sector_dynamic_lighting_design.md` with implementation notes if behavior differs from the original audit/design.

Document:

* dynamic lights are runtime shader lights
* static lights are baked lightmap lights
* dynamic lights do not affect lightmap source hash
* first pass supports point lights only
* max dynamic lights is currently 8
* selected dynamic lights are visibility-culled/ranked
* shadows and spotlights are future work

Add or update tests for:

* dynamic light serialization
* dynamic light omission/default load behavior
* dynamic lights not affecting lightmap source hash
* dynamic light candidate selection
* dynamic light packing zero-count and max-count behavior
* deterministic selection order

Clean up names/comments where necessary.

Avoid broad refactors.

### Out of Scope

No shadow maps.

No PCF.

No spotlights.

No dynamic sector implementation.

No replacement of raylib material/shader architecture.

### Verification

Run the full relevant test suite.

Run:

```bash
git diff --check
git diff --stat
git status --short
```

### Manual Smoke Test

Use a level containing:

* baked static lights
* baked AO
* dynamic point lights
* decals
* emissive decals/bloom
* alpha-tested middle textures
* sky sectors
* enough sectors/lights to trigger culling/ranking

Confirm:

* renderer looks unchanged when dynamic lights are disabled
* dynamic point lights add correctly on top of baked lighting
* baked AO still affects ambient only
* dynamic lights do not stale/recompute baked lightmaps
* portal culling still culls draw records
* dynamic light selection uses visible sectors
* no hidden sector light consumes slots in normal valid visibility cases
* fallback draw-all remains safe
* picking/highlight behavior from the portal culling plan still works
* save/reload preserves authored dynamic lights

## Final Completion Criteria

This plan is complete when:

* authored dynamic point lights exist separately from static bake lights
* dynamic point lights save/load correctly
* dynamic point lights are editable in the editor
* dynamic point lights render as per-pixel forward lights in the sector shader
* dynamic light count zero preserves old visual output
* portal visibility is used to select candidate lights
* more authored dynamic lights can exist than the shader evaluates
* selected lights are ranked by estimated contribution
* debug output shows candidate/selected light counts
* dynamic lights do not affect lightmap source hash
* static light baking behavior remains unchanged
* no shadow-map/spotlight/dynamic-sector scope has leaked into this implementation
