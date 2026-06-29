# Sector Dynamic Point Lighting Plan

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

```plan-state-json
{
  "plan_id": "sector_dynamic_point_lighting_plan",
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
      "title": "Dynamic Point Light Data And Editor Authoring",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_01a",
      "title": "Add Dynamic Point Light Data, Serialization, And Hash Isolation",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_01b",
      "title": "Add Dynamic Light Editor Tool, Selection, And Inspector",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_02",
      "title": "Shader Dynamic Point Light Support",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02a",
      "title": "Pass Fragment World Position And Normal Through Sector Shader",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02b",
      "title": "Add Fixed-Size Dynamic Point Light Shader Uniforms And Contribution",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "Visibility-Aware Dynamic Light Selection",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_03a",
      "title": "Assign Dynamic Lights To Sectors And Collect Visibility Candidates",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b",
      "title": "Rank, Cap, And Pack Selected Dynamic Lights",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_04",
      "title": "Dynamic Light Debug And Selection Stability",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_04a",
      "title": "Add Dynamic Light Debug Readout And Runtime Toggle",
      "type": "pass",
      "parent": "phase_04",
      "status": "Not Started"
    },
    {
      "id": "phase_04b",
      "title": "Add Simple Top-N Selection Hysteresis",
      "type": "pass",
      "parent": "phase_04",
      "status": "Not Started"
    },
    {
      "id": "phase_05",
      "title": "Polish, Tests, Documentation, And Completion",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_05a",
      "title": "Tune Defaults, Strengthen Tests, Update Docs, And Close Plan",
      "type": "pass",
      "parent": "phase_05",
      "status": "Not Started"
    }
  ]
}
```

## Current Progress

| Phase / Pass                                                                  | Status      | Date | Notes                                                                      |
| ----------------------------------------------------------------------------- | ----------- | ---- | -------------------------------------------------------------------------- |
| Phase 1: Dynamic Point Light Data And Editor Authoring                        | Completed   | 2026-06-29 | Dynamic light data and editor authoring are implemented.                   |
| Phase 1A: Add Dynamic Point Light Data, Serialization, And Hash Isolation     | Completed   | 2026-06-29 | Added dynamic point light data/persistence and hash isolation tests; no renderer/editor UI behavior changes. |
| Phase 1B: Add Dynamic Light Editor Tool, Selection, And Inspector             | Completed   | 2026-06-29 | Added separate Static Light and Dynamic Light editor tools, dynamic selection/drag/edit/delete support, and distinct 2D glyphs. |
| Phase 2: Shader Dynamic Point Light Support                                   | Completed   | 2026-06-29 | Shader dynamic point light support is implemented.                         |
| Phase 2A: Pass Fragment World Position And Normal Through Sector Shader       | Completed   | 2026-06-29 | Shader plumbing only; dynamic light count remains zero.                    |
| Phase 2B: Add Fixed-Size Dynamic Point Light Shader Uniforms And Contribution | Completed   | 2026-06-29 | Adds capped fixed-array dynamic point-light contribution.                  |
| Phase 3: Visibility-Aware Dynamic Light Selection                             | Completed   | 2026-06-29 | Visibility candidate filtering and ranked top-N selection are implemented. |
| Phase 3A: Assign Dynamic Lights To Sectors And Collect Visibility Candidates  | Completed   | 2026-06-29 | Dynamic lights are assigned to sectors and selected against visible receiver bounds. |
| Phase 3B: Rank, Cap, And Pack Selected Dynamic Lights                         | Completed   | 2026-06-29 | Candidate lights are ranked by estimated visible-receiver contribution and capped at 8. |
| Phase 4: Dynamic Light Debug And Selection Stability                          | Not Started |      | Parent phase.                                                              |
| Phase 4A: Add Dynamic Light Debug Readout And Runtime Toggle                  | Not Started |      | Makes selection visible/debuggable.                                        |
| Phase 4B: Add Simple Top-N Selection Hysteresis                               | Not Started |      | Reduces threshold flicker.                                                 |
| Phase 5: Polish, Tests, Documentation, And Completion                         | Not Started |      | Parent phase.                                                              |
| Phase 5A: Tune Defaults, Strengthen Tests, Update Docs, And Close Plan        | Not Started |      | Final cleanup and plan closure.                                            |

## Execution Log

### 2026-06-29: Phase 1A Completed

Summary:

* Added authored `SectorTopologyDynamicPointLight` map-level data with stable IDs, position, color, intensity, radius, and enabled state.
* Added `dynamicPointLights` serialization/loading for topology and graph-native documents; missing field loads as empty, and default `enabled: true` is omitted on save.
* Added validation/helper coverage for dynamic point lights and preserved graph-derived map-level copying.

Behavior notes:

* Source code changed.
* Serialization field name is `dynamicPointLights`.
* Rendering, shader behavior, editor UI/tools, and static light behavior are intended unchanged.
* Dynamic point lights are intentionally excluded from `ComputeSectorLightmapSourceHash()`.
* Topology render-cache invalidation behavior was not changed; no topology mutation UI path was added in this pass.
* Lightmap source-hash behavior was tested: dynamic light additions/edits do not affect the hash, while existing static light hash assertions still pass.
* No manual GUI verification was performed.

Verification:

* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_topology_(serialization|lightmap)$"` passed.
* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed.
* `git diff --check` passed.
* `git diff --stat` reviewed.
* `git status --short` reviewed.

### 2026-06-29: Phase 1B Completed

Summary:

* Added separate editor tools labeled `Static Light` and `Dynamic Light`.
* Added dynamic point light placement in 2D sectors using the static-light placement height style, with default radius 8.0 world units, intensity 1.0, white color, and enabled state.
* Added dynamic light selection, drag/move, delete, and inspector editing for enabled, position, radius, intensity, and color.
* Added distinct dynamic light 2D cache entries/glyphs while preserving static light source-radius display.

Behavior notes:

* Source code changed.
* Static light tool and inspector behavior are intended unchanged except labels/status text now say static light explicitly.
* Dynamic light edits use the existing document-edited path, so 2D topology render-cache invalidation happens through `MarkTopologyDocumentEdited()` / `FinishTopologyActionResult()`.
* Dynamic light changes remain serialized through the existing `dynamicPointLights` field from Phase 1A.
* Lightmap source-hash behavior is intended unchanged: dynamic point lights remain excluded, static lights remain bake/hash inputs.
* No shader dynamic lighting exists yet; renderer output is unchanged except editor overlay glyphs.
* No gameplay, collision, sector lookup, or physics behavior changed.
* No manual GUI verification was performed.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_authoring_graph|sector_topology_(serialization|lightmap)$"` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed.
* `git diff --check` passed.
* `git diff --stat` reviewed.
* `git status --short` reviewed.

### 2026-06-29: Phase 2A Completed

Summary:

* Added `fragWorldPosition` and `fragWorldNormal` plumbing to the embedded sector shader.
* Added the `vertexNormal` shader attribute binding for preview and bloom-source materials that share the sector vertex shader.
* Normalized fragment normals in the sector fragment shader without adding dynamic-light uniforms or contribution.

Behavior notes:

* Source code changed.
* Visual behavior is intended unchanged; the baked ambient/direct lighting formula remains unchanged.
* Dynamic light count remains effectively zero because Phase 2B has not added uniforms or light contribution yet.
* Bloom source output remains emissive-only; only the shared vertex shader plumbing changed.
* Lightmap source-hash behavior is unchanged; no lightmap hash code or baked-light inputs were touched.
* No topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* No gameplay, collision, sector lookup, or physics behavior changed.
* No manual GUI verification was performed, so shader runtime compilation was not manually smoke-tested.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed.
* `git diff --check` passed.
* `git diff --stat` reviewed.
* `git status --short` reviewed.

### 2026-06-29: Phase 2B Completed

Summary:

* Added fixed-size sector shader uniforms for up to 8 dynamic point lights.
* Added per-pixel dynamic point light contribution using world position, world normal, squared falloff, and Lambert `ndotl`.
* Added preview-side packing/upload for enabled, finite, positive-radius, positive-intensity authored dynamic point lights.

Behavior notes:

* Source code changed.
* `MAX_DYNAMIC_LIGHTS` is 8.
* Dynamic lighting clamp max is 4.0; the zero-dynamic-light path keeps the prior baked-only 0..1 clamp to preserve old output.
* Upload order before Phase 3 ranking is authored `dynamicPointLights` insertion order, skipping invalid/disabled lights and capping at 8.
* Dynamic light positions and radii are converted from authoring units to preview/runtime world units during preview rebuild.
* Dynamic lights add direct lighting and are not multiplied by baked AO.
* Bloom source output remains emissive-only; dynamic lights are not uploaded to or evaluated by the bloom-source shader.
* Static baked lightmaps remain unchanged.
* Lightmap source-hash behavior is unchanged; no lightmap hash code or baked-light inputs were touched.
* No topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* No gameplay, collision, sector lookup, or physics behavior changed.
* No manual GUI verification was performed, so visible dynamic-light behavior and runtime shader compilation were not manually smoke-tested.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed.
* `git diff --check` passed.
* `git diff --stat` reviewed.
* `git status --short` reviewed.

### 2026-06-29: Phase 3A Completed

Summary:

* Added a small dynamic point light selection helper that converts valid authored dynamic lights into preview uniform data and assigns an owning sector through `SectorCollisionWorld::FindSectorContainingPoint()`.
* Replaced rebuild-time first-eight packing with visibility-time candidate collection.
* Added candidate filtering; later Phase 3 correction expanded this from owner-sector visibility to visible receiver bounds.
* Added tests for visible receiver overlap, fallback draw-all behavior, invalid-light dropping, world-space conversion, and unassigned outside-map lights.

Behavior notes:

* Source code changed.
* Sector ownership lookup uses the existing topology-built collision world and world-space XZ light position.
* Disabled, non-positive-radius, non-positive-intensity, non-finite-position, and invalid converted lights are dropped before candidate filtering.
* If visibility is invalid or `fallbackDrawAll` is true, all valid dynamic light sources are candidates.
* During normal valid visibility, owner-sector visibility is only an inclusion shortcut; hidden-owner lights may still be included when they overlap visible receiver bounds.
* No scoring/ranking formula was added; Phase 3B remains responsible for ranking and the final cap policy. The shader upload helper still caps the list at `MAX_DYNAMIC_LIGHTS`.
* Shader formula and dynamic-light contribution behavior are unchanged.
* Portal geometry draw culling is unchanged.
* Static baked lighting and lightmap source-hash behavior are unchanged; no lightmap hash code or baked-light inputs were touched.
* No topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* No gameplay, collision, sector lookup, or physics behavior changed; the existing collision world is only read for preview light ownership.
* No manual GUI verification was performed.

Verification:

* `cmake --build cmake-build-debug --target sector_topology_mesh_builder_tests -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_topology_mesh_builder$"` passed.
* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed.
* `git diff --check` passed.
* `git diff --stat` reviewed.
* `git status --short` reviewed.

### 2026-06-29: Phase 3B Completed

Summary:

* Added ranked dynamic point light selection after portal-visibility candidate collection.
* Ranked visible/fallback candidates by estimated visible-receiver contribution and packed only the selected lights into the existing shader uniform upload path.
* Added tests for receiver-bound priority, fallback draw-all bounds, boundary padding, disabled/invalid-light dropping through source collection, max-light capping, and deterministic light-ID tie-breaking.

Behavior notes:

* Source code changed.
* Candidate policy uses owner-sector visibility as an inclusion shortcut, visible receiver AABB overlap as the main safe inclusion path, and all valid sources during invalid visibility or fallback draw-all.
* Ranking score is `intensity * max(color.r, color.g, color.b) * atten^2`, where `atten = saturate(1 - nearestDistance(light.position, visibleReceiverBounds) / radius)`.
* Lights are not dropped solely because the camera is outside their influence radius.
* Selected dynamic lights are capped at `MAX_DYNAMIC_LIGHTS` / 8 before shader upload.
* Ties use ascending stable light ID.
* Dynamic light shader formula and uniform layout are unchanged.
* Portal geometry draw culling is unchanged.
* Static baked lighting and lightmap source-hash behavior are unchanged; no lightmap hash code or baked-light inputs were touched.
* No topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.
* No gameplay, collision, sector lookup, or physics behavior changed; the existing collision world continues to be read only for preview light ownership.
* No manual GUI verification was performed.

Verification:

* `cmake --build cmake-build-debug --target sector_topology_mesh_builder_tests -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_topology_mesh_builder$"` passed.
* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed.
* `git diff --check` passed.
* `git diff --stat` reviewed.
* `git status --short` reviewed.

### 2026-06-29: Phase 3 Dynamic Light Selection Correction

Summary:

* Dynamic light candidate selection now uses visible receiver bounds instead of camera-distance hard rejection. Owner-sector visibility is only an inclusion shortcut, not the only inclusion path.
* Added per-sector receiver bounds derived from generated sector draw-record geometry.
* Updated candidate collection to include owner-visible lights, lights overlapping padded visible receiver bounds, and all valid lights during fallback draw-all.
* Updated ranking to estimate contribution to visible receiver bounds rather than distance to the camera.

Behavior notes:

* Source code changed.
* Receiver bounds include generated renderable sector geometry and exclude the separately generated sky cylinder/top cap.
* Fallback draw-all treats all receiver bounds as visible; if receiver bounds are unavailable, valid dynamic lights are included and ranked conservatively.
* Receiver bounds are padded slightly for sphere-overlap and nearest-distance scoring to avoid exact-boundary misses.
* Owner-sector visibility does not add a scoring bonus; ranking remains receiver-contribution based when bounds are available.
* Dynamic light selection remains part of the per-frame preview visibility update and is not gated by debug UI display.
* Dynamic light shader formula and uniform layout are unchanged.
* Static baked lighting and lightmap source-hash behavior are unchanged; no lightmap hash code or baked-light inputs were touched.
* No topology mutation paths were touched, so 2D topology render-cache invalidation behavior is unchanged.

Verification:

* `cmake --build cmake-build-debug --target sector_topology_mesh_builder_tests -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_topology_mesh_builder$"` passed.
* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed.
* `git diff --check` passed.
* `git diff --stat` reviewed.
* `git status --short` reviewed.

## Execution Tracking Rules

* Each pass must leave the project buildable and runnable.
* Each pass final report must state whether source code changed.
* Each implementation pass must update this document before finishing.
* The update should be small and local.
* Do not rewrite unrelated phases when marking progress.
* If behavior is intended to remain unchanged, explicitly state that.
* If a pass changes serialization, generated data, public APIs, runtime behavior, cache invalidation, shader behavior, or build/test behavior, clearly say so.
* Do not claim manual GUI verification unless it was actually performed.
* If a pass produces only a plan or audit and no source changes, state that clearly.
* If a pass is too broad, split it into smaller child passes and stop without source changes.

## Goal And Desired End State

Add forward-rendered dynamic point lights to the sector renderer.

Desired end state:

* Authored dynamic point lights exist separately from existing baked `staticLights`.
* Dynamic point lights save/load correctly.
* Dynamic point lights are editable in the sector editor.
* Existing static lights remain baked lightmap inputs.
* Dynamic point lights do not affect the lightmap source hash.
* Sector shader supports per-pixel dynamic point lights.
* Dynamic light count zero preserves old visual output.
* Portal visibility is used to select candidate dynamic lights.
* More authored dynamic lights can exist than the shader evaluates.
* Selected lights are ranked by estimated contribution.
* Debug output shows candidate/selected dynamic light counts.
* No shadow-map, spotlight, dynamic-sector, deferred-rendering, or clustered-lighting scope leaks into this plan.

Target shader composition:

```text
lighting = bakedDirect + sectorAmbient * bakedAO + dynamicDirect
finalRgb = surfaceRgb * lighting
```

Current audit facts to preserve:

* Sector shader is embedded in `SectorMeshPreview.cpp`.
* Material slots are piggybacked:

  * diffuse = base texture
  * specular = lightmap
  * normal = decal texture
* Current lighting is `clamp(vertexAmbient * bakedAO + bakedDirect, 0..1) * surfaceRgb`.
* AO affects ambient only, not baked direct.
* Meshes already upload positions and normals, but the shader does not currently pass position/normal to the fragment stage.
* Existing `staticLights` are bake lights and source-hash inputs.
* First dynamic light implementation should use fixed uniform arrays and no new samplers.

## Dependency Direction Rules

* Static baked lighting must not depend on dynamic runtime lighting.
* Dynamic runtime lighting may read topology, current camera state, and portal visibility.
* Dynamic light selection may depend on visibility results.
* Visibility/culling must not depend on dynamic lighting.
* Lightmap source hashing must not include runtime dynamic point lights.
* The shader may receive selected dynamic light uniforms, but mesh generation should not need to know selected lights.
* Editor authoring data may be serialized, but runtime selected-light state must not be serialized.

## Proposed Phases

### Phase 1: Dynamic Point Light Data And Editor Authoring

Goal:

Add dynamic point lights as their own authored map-level concept.

Why it helps:

This keeps baked static lights and runtime dynamic lights separate, avoiding source-hash confusion and double-lighting surprises.

Files/functions likely touched:

* `sources/sector_demo/SectorTopologyTypes.h`
* `sources/sector_demo/SectorTopologySerialization.cpp`
* `sources/sector_demo/SectorLightmap.cpp`
* `sources/sector_editor/SectorEditorTopologyActions.cpp`
* `sources/sector_editor/SectorEditorLightInspector.cpp`
* editor selection/tool files as needed
* topology serialization/hash tests

Exact behavior that must remain unchanged:

* Existing `staticLights` still save/load as before.
* Existing static light baking behavior stays unchanged.
* Lightmap source hashes change for static light edits, but not dynamic light edits.
* Levels with no `dynamicPointLights` field load normally.

Risks/goblins:

* Accidentally including dynamic lights in lightmap hash.
* Accidentally reusing static-light source radius semantics.
* Confusing static and dynamic lights in editor labels.
* Serialization compatibility issues with old levels.

Non-goals:

* No dynamic shader lighting yet.
* No shadow settings.
* No sourceRadius on dynamic lights.
* No spotlights.
* No animation/flicker system.

Suggested tests/manual smoke checks:

* Serialization round-trip for dynamic point lights.
* Missing `dynamicPointLights` loads as empty.
* Invalid values are handled defensively.
* Dynamic light edits do not affect lightmap source hash.
* Manual editor smoke only after Phase 1B.

Final report expectations:

* State files changed.
* State serialization field name.
* State whether hash behavior was tested.
* State whether source code changed.
* State verification commands/results.

How to update this plan after completion:

* Mark completed pass as `Completed` in JSON and table.
* Add date, summary, verification results, and behavior notes.
* Mark parent phase `Completed` only after both Phase 1A and Phase 1B are complete.

#### Phase 1A: Add Dynamic Point Light Data, Serialization, And Hash Isolation

Goal:

Add the data model and persistence for authored dynamic point lights without editor UI or shader changes.

Implementation guidance:

Add a separate dynamic point light type, for example:

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

Add map-level storage such as:

```cpp
std::vector<SectorTopologyDynamicPointLight> dynamicPointLights;
```

Use stable positive integer IDs.

Serialize under a distinct root field such as:

```json
"dynamicPointLights": []
```

Load missing field as empty.

Omit empty field on save if that matches the project’s existing save style.

Validate bad data defensively.

Do not include dynamic point lights in `ComputeSectorLightmapSourceHash()`.

Add tests for:

* missing dynamic lights load as empty
* save/load round-trip
* dynamic light changes do not change lightmap source hash
* static light changes still change lightmap source hash

Behavior that must remain unchanged:

* Renderer output.
* Editor UI.
* Static light serialization.
* Static light bake/source-hash behavior.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant serialization/hash tests.

Final report expectations:

* Confirm no rendering/editor behavior changed.
* Confirm dynamic lights are not part of lightmap source hash.
* Confirm tests/checks run.

#### Phase 1B: Add Dynamic Light Editor Tool, Selection, And Inspector

Goal:

Make authored dynamic point lights addable, selectable, editable, saveable, and reloadable from the editor.

Implementation guidance:

Add separate UI/tool affordance for dynamic lights. Do not make dynamic/static a checkbox on one light object.

Suggested labels:

```text
Static Light
Dynamic Light
```

Dynamic point light placement should reuse static-light placement patterns:

* click in 2D sector to place
* default height: sector floor + 1.8 world units, matching current static-light placement style where appropriate
* default radius: 8.0 world units
* default intensity: 1.0
* default color: white
* enabled: true

Add selection/editing support.

Dynamic light inspector fields:

* enabled
* position
* radius
* intensity
* color

Do not show `sourceRadius`.

If editor overlays/glyphs exist for static lights, render dynamic light glyphs distinctly but simply.

Behavior that must remain unchanged:

* Static light tool and inspector behavior.
* Lightmap baking and source hash.
* Renderer output except editor overlay glyphs.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant editor/topology tests.

Manual smoke checks:

* Add a dynamic light.
* Select it.
* Edit enabled/radius/intensity/color/position.
* Save and reload.
* Confirm it persists.
* Confirm no lightmap stale/rebake behavior is triggered by dynamic-only edits.

Final report expectations:

* State UI/tool labels.
* State manual verification if actually performed.
* Confirm no shader dynamic lighting exists yet.

### Phase 2: Shader Dynamic Point Light Support

Goal:

Add per-pixel point light support to the sector shader.

Why it helps:

This proves the renderer can blend dynamic direct lighting with baked lightmaps/AO before adding ranking/culling complexity.

Files/functions likely touched:

* `sources/sector_demo/SectorMeshPreview.cpp`
* sector shader strings inside `SectorMeshPreview.cpp`
* shader uniform location/setup code
* draw loop uniform upload helpers
* possible shader helper structs/constants
* shader/data packing tests if helpers are extracted

Exact behavior that must remain unchanged:

* Dynamic light count zero must match current output.
* Base texture/lightmap/decal material slot behavior must stay compatible.
* Emissive bloom source must not be affected by dynamic lights.
* Alpha-tested middle textures must still discard correctly.

Risks/goblins:

* Shader compile errors from attribute names/locations.
* NaNs near zero distance.
* Overbright scenes due to LDR clamp behavior.
* Accidentally changing old baked-only appearance.
* Accidentally treating decal material slot as normal map support.

Non-goals:

* No visibility-aware selection in Phase 2.
* No per-sector light lists.
* No shadows.
* No spotlights.
* No new samplers.

Suggested tests/manual smoke checks:

* With zero dynamic lights, output should appear unchanged.
* One dynamic point light visibly affects nearby surfaces.
* Normals affect floor/wall brightness directionally.
* Decals and alpha-tested middle textures still render.
* Bloom source remains emissive-only.

Final report expectations:

* State shader/uniform changes.
* State clamp used.
* State tests/checks run.
* State any manual smoke results if performed.

How to update this plan after completion:

* Mark completed pass as `Completed` in JSON and table.
* Add date, summary, verification results, and behavior notes.
* Mark parent phase `Completed` only after both Phase 2A and Phase 2B are complete.

#### Phase 2A: Pass Fragment World Position And Normal Through Sector Shader

Goal:

Add shader plumbing for fragment world position and normal, but keep dynamic light count zero.

Implementation guidance:

Modify embedded sector shader strings.

Pass from vertex to fragment:

```glsl
fragWorldPosition
fragWorldNormal
```

Current generated mesh positions are already world-space and draw records use `MatrixIdentity()`, so first pass can treat `vertexPosition` as world position and `vertexNormal` as world normal.

Normalize normals in shader.

Do not change lighting formula yet except unavoidable plumbing.

Behavior that must remain unchanged:

* Visual output should be unchanged.
* No dynamic light contribution yet.
* Bloom source remains unchanged unless compile plumbing requires a harmless matching update.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant tests/build.

Manual smoke:

* Existing levels render like before.
* No shader compile errors.

Final report expectations:

* Confirm whether visual behavior is intended unchanged.
* Confirm shader compiled in manual or automated run if actually checked.

#### Phase 2B: Add Fixed-Size Dynamic Point Light Shader Uniforms And Contribution

Goal:

Add actual dynamic point light contribution with a simple global upload path.

Implementation guidance:

Use fixed uniform arrays:

```glsl
#define MAX_DYNAMIC_LIGHTS 8

uniform int dynamicLightCount;
uniform vec3 dynamicLightPositions[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightColors[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightRadii[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightIntensities[MAX_DYNAMIC_LIGHTS];
```

Add matching C++ uniform locations and upload helper.

For this phase, upload enabled authored dynamic point lights in deterministic ID/insertion order, capped at `MAX_DYNAMIC_LIGHTS`. Phase 3 will replace this with visibility-aware ranking.

Skip invalid lights:

* disabled
* radius <= 0
* intensity <= 0
* invalid position/color

Dynamic contribution:

```text
toLight = light.position - fragWorldPosition
distance = length(toLight)
if distance < radius:
    L = toLight / distance
    ndotl = max(dot(normalize(fragWorldNormal), L), 0)
    atten = saturate(1 - distance / radius)
    atten = atten * atten
    dynamicDirect += light.colorRgb * intensity * atten * ndotl
```

Handle very small distance safely.

Composition:

```text
lighting = bakedDirect + sectorAmbient * bakedAO + dynamicDirect
lighting = clamp(lighting, 0, dynamicLightingClamp)
finalRgb = surfaceRgb * lighting
```

Use `dynamicLightingClamp` of either 2.0 or 4.0 and document the choice.

Do not multiply dynamic light by baked AO.

Do not affect emissive bloom source.

Behavior that must remain unchanged:

* Dynamic light count zero matches current rendering.
* Static baked lightmaps remain unchanged.
* Decal/emissive behavior remains unchanged.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run build/tests.

Manual smoke:

* Disable all dynamic lights: old look.
* Enable one dynamic light: visible local lighting.
* Radius/intensity/color changes are visible.
* Alpha-tested middle textures still work.
* Decals/bloom still work.

Final report expectations:

* State MAX_DYNAMIC_LIGHTS.
* State clamp max.
* State upload order before Phase 3 ranking.
* State manual smoke if performed.

### Phase 3: Visibility-Aware Dynamic Light Selection

Goal:

Use portal visibility to select/rank dynamic lights before uploading them.

Why it helps:

The map can contain more dynamic lights than the forward shader evaluates, without every hidden light consuming a shader slot.

Files/functions likely touched:

* `SectorMeshPreview.cpp`
* `SectorPortalVisibility` integration call sites
* `SectorCollisionWorld` or sector lookup helpers
* new dynamic light selection helper files if useful
* tests for candidate selection/ranking

Exact behavior that must remain unchanged:

* Portal geometry draw culling remains unchanged.
* `fallbackDrawAll` remains safe and conservative.
* Static baked lighting remains unchanged.
* Shader formula from Phase 2 remains unchanged except selected input list.

Risks/goblins:

* False negative visibility excluding lights that should illuminate visible surfaces.
* Hidden-sector lights wasting slots.
* Top-N flicker near score thresholds.
* Ambiguous sector ownership for lights near portals/walls.
* Incorrect fallback behavior.

Non-goals:

* No hysteresis in Phase 3 unless trivial.
* No per-sector or per-draw-record light lists.
* No clustered/tiled/deferred lighting.
* No shadows.

Suggested tests/manual smoke checks:

* Candidate filtering by visible sectors.
* Fallback draw-all considers all lights.
* Top-N cap keeps highest scores.
* More than 8 authored lights present.
* Hidden-sector lights generally do not consume slots in normal visibility cases.

Final report expectations:

* State candidate policy.
* State fallback behavior.
* State ranking formula.
* State tests/checks run.

How to update this plan after completion:

* Mark completed pass as `Completed` in JSON and table.
* Add date, summary, verification results, and behavior notes.
* Mark parent phase `Completed` only after Phase 3A and Phase 3B are complete.

#### Phase 3A: Assign Dynamic Lights To Sectors And Collect Visibility Candidates

Goal:

Determine which dynamic point lights are candidates based on portal visibility.

Implementation guidance:

Run dynamic light candidate selection after the current visibility result is updated and before drawing.

Assign each dynamic light to one sector where practical using existing sector/collision lookup helpers.

Candidate rules:

* If visibility is invalid, consider all enabled dynamic point lights.
* If `fallbackDrawAll` is true, consider all enabled dynamic point lights.
* Otherwise, prefer lights whose owning sector is in `visibleSectorIds`.

For lights that cannot be assigned to a sector:

* choose a conservative documented behavior
* recommended first behavior: include them only during fallback draw-all; otherwise exclude and report/debug-count them if easy

Drop invalid lights:

* disabled
* radius <= 0
* intensity <= 0
* invalid position/color

Behavior that must remain unchanged:

* Shader still uses whatever selected/uploaded list exists.
* No scoring changes beyond candidate inclusion.
* No geometry culling changes.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Add/run tests for visible-sector candidate filtering and fallback behavior.

Final report expectations:

* State sector ownership lookup method.
* State unassigned-light behavior.
* State fallback behavior.

#### Phase 3B: Rank, Cap, And Pack Selected Dynamic Lights

Goal:

Rank candidates by estimated contribution, keep top `MAX_DYNAMIC_LIGHTS`, and upload only selected lights.

Implementation guidance:

Do not sort by distance only.

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

Use deterministic tie-breaking by light ID.

Drop lights outside influence radius from camera as a cheap first cut.

Keep top `MAX_DYNAMIC_LIGHTS`.

Pack selected lights into shader uniforms.

Add testable helper functions where practical.

Behavior that must remain unchanged:

* Dynamic light shader formula.
* Portal geometry culling.
* Static lighting.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Add/run tests:

* stronger light beats weaker light
* nearby light beats far light when otherwise equal
* outside-radius light is dropped
* disabled light is dropped
* max-light cap keeps top N
* tie-breaking is deterministic

Manual smoke:

* Create more dynamic lights than the cap.
* Move around and confirm visible/relevant lights tend to win slots.

Final report expectations:

* State exact score formula.
* State max cap.
* State tests/checks run.

### Phase 4: Dynamic Light Debug And Selection Stability

Goal:

Expose dynamic light selection state and reduce obvious top-N flicker.

Why it helps:

The feature becomes debuggable and less visually twitchy before final polish.

Files/functions likely touched:

* `SectorMeshPreview.cpp`
* debug text/status rendering
* dynamic light selection helper code
* tests for hysteresis if helper logic is extracted

Exact behavior that must remain unchanged:

* Geometry visibility culling.
* Static lighting.
* Save/load.
* Shader formula.

Risks/goblins:

* Debug text becoming noisy.
* Hysteresis retaining stale/deleted/disabled lights.
* Hysteresis making selection confusing.
* Runtime toggle accidentally affecting saved data.

Non-goals:

* No complex performance overlay.
* No per-sector light lists.
* No shadow/spotlight UI.
* No permanent settings overbuild.

Suggested tests/manual smoke checks:

* Debug counts match expectations.
* Disabling selected light removes it immediately.
* Selected IDs remain stable near thresholds.
* Fallback draw-all still behaves safely.

Final report expectations:

* State debug text format.
* State toggle/hysteresis behavior.
* State tests/checks run.

How to update this plan after completion:

* Mark completed pass as `Completed` in JSON and table.
* Add date, summary, verification results, and behavior notes.
* Mark parent phase `Completed` only after Phase 4A and Phase 4B are complete.

#### Phase 4A: Add Dynamic Light Debug Readout And Runtime Toggle

Goal:

Show what the dynamic light system is doing.

Implementation guidance:

Add debug text near the existing portal visibility debug.

Show at least:

```text
dynamic lights: selected / candidates / total
```

Optionally show selected IDs:

```text
selected dynamic light ids: 3, 7, 12
```

Add a simple runtime debug toggle to disable dynamic lighting if there is an obvious existing pattern for debug toggles.

The toggle should be runtime-only unless existing settings infrastructure makes persistence trivial and appropriate.

Behavior that must remain unchanged:

* Dynamic lights still render when enabled.
* Static lighting unchanged.
* No save/load changes unless explicitly justified.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Manual smoke:

* Counts make sense with zero, one, and many dynamic lights.
* Toggle disables/re-enables dynamic contribution if implemented.

Final report expectations:

* State readout format.
* State toggle key/behavior if added.

#### Phase 4B: Add Simple Top-N Selection Hysteresis

Goal:

Reduce flicker when lights compete near the `MAX_DYNAMIC_LIGHTS` boundary.

Implementation guidance:

Add simple hysteresis only to selected dynamic light IDs, not to saved data.

Suggested behavior:

```text
Keep previously selected lights if their current score is still close enough.
Only replace a retained light if a new candidate score is at least 15-25% higher.
```

Constraints:

* deleted lights are removed immediately
* disabled lights are removed immediately
* outside-radius lights are removed
* invalid lights are removed
* selected count never exceeds `MAX_DYNAMIC_LIGHTS`
* fallback draw-all does not retain invalid stale lights

Keep the algorithm deterministic.

Behavior that must remain unchanged:

* Ranking still mostly follows contribution score.
* Static lighting unchanged.
* Shader formula unchanged.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Add/run tests if helper logic is testable:

* retained selected light survives small score difference
* clearly better light replaces old one
* disabled/deleted light is removed
* selected count capped

Manual smoke:

* Move near two competing lights around the slot boundary.
* Confirm no rapid flicker.

Final report expectations:

* State hysteresis threshold.
* State invalidation behavior.
* State tests/checks run.

### Phase 5: Polish, Tests, Documentation, And Completion

Goal:

Tune defaults, strengthen tests/docs, and close the dynamic point lighting plan.

Why it helps:

This locks down the first usable version and records the intentionally deferred goblins.

Files/functions likely touched:

* dynamic light code/comments
* tests
* `docs/sector_dynamic_lighting_design.md`
* this plan document
* maybe editor labels/defaults

Exact behavior that must remain unchanged:

* Static lights remain baked-only.
* Dynamic lights remain runtime-only.
* No source-hash change for dynamic lights.
* No shadow/spotlight scope.

Risks/goblins:

* Over-tuning without screenshots/manual feedback.
* Accidentally broadening into shadow or spotlight work.
* Weak tests around serialization/ranking.

Non-goals:

* No shadow maps.
* No PCF.
* No spotlights.
* No dynamic floors/doors.
* No renderer architecture replacement.

Suggested tests/manual smoke checks:

* Full relevant test suite.
* Manual map with baked lights, AO, decals, emissive decals, alpha-tested middle textures, sky sectors, and many dynamic lights.

Final report expectations:

* State final defaults.
* State docs updated.
* State full verification.
* State plan completion criteria met/unmet.
* State known deferred work.

How to update this plan after completion:

* Mark Phase 5A `Completed`.
* Mark Phase 5 `Completed`.
* If all phases are complete, set all parent phases to `Completed`.
* Leave a final completion note.

#### Phase 5A: Tune Defaults, Strengthen Tests, Update Docs, And Close Plan

Goal:

Finish the feature pass cleanly.

Implementation guidance:

Tune first-pass defaults:

* dynamic light radius
* intensity
* dynamic lighting clamp
* debug wording
* editor labels

Update `docs/sector_dynamic_lighting_design.md` with implementation notes if behavior differs from the original audit/design.

Document:

* dynamic lights are runtime shader lights
* static lights are baked lightmap lights
* dynamic lights do not affect lightmap source hash
* first pass supports point lights only
* max dynamic lights is currently 8
* selected dynamic lights are visibility-culled/ranked
* shadows and spotlights are future work

Ensure tests cover:

* dynamic light serialization
* missing/default load behavior
* dynamic lights not affecting lightmap source hash
* candidate selection
* ranking/capping
* packing zero-count and max-count
* deterministic selection order

Manual smoke checklist:

* renderer unchanged when dynamic lights disabled
* dynamic point lights add on top of baked lighting
* baked AO still affects ambient only
* dynamic lights do not stale/recompute baked lightmaps
* portal culling still culls draw records
* dynamic light selection uses visible sectors
* hidden-sector lights do not consume slots in normal valid visibility cases
* fallback draw-all remains safe
* picking/highlight behavior from portal culling still works
* save/reload preserves authored dynamic lights

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run the full relevant test suite.

Final report expectations:

* State final defaults.
* State verification results.
* State manual smoke results if actually performed.
* State whether the plan is complete.

## Deferred Decisions For Later Phases

These are intentionally out of scope for this plan:

* Spotlights.
* 3D spotlight editing/gizmos.
* Shadow maps.
* PCF.
* Shadow bias tuning.
* Point-light cubemap shadows.
* Flashlight gameplay item.
* Dynamic floors/doors.
* Per-sector/per-draw-record light lists.
* Clustered/tiled/deferred lighting.
* Volumetric lights/fog.
* Projected cookies.
* Normal mapping.
* Replacing raylib material/shader architecture.

## Final Completion Criteria

This plan is complete when:

* dynamic point lights are separate from static bake lights
* dynamic point lights save/load correctly
* dynamic point lights are editable in the editor
* dynamic point lights render as per-pixel forward lights
* dynamic light count zero preserves old visual output
* portal visibility is used to select candidate lights
* more authored dynamic lights can exist than the shader evaluates
* selected lights are ranked by estimated contribution
* debug output shows candidate/selected light counts
* dynamic lights do not affect lightmap source hash
* static light baking behavior remains unchanged
* no shadow-map/spotlight/dynamic-sector scope has leaked into this implementation
