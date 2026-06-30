# Sector Lightmap Alpha-Test Occlusion Plan

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

```plan-state-json id="lightmap-alpha-occlusion"
{
  "plan_id": "sector_lightmap_alpha_test_occlusion_plan",
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
      "title": "Alpha-Test Occluder Data And Texture Sampling",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_01a",
      "title": "Collect Alpha-Tested Middle Texture Occluders For Baking",
      "type": "pass",
      "parent": "phase_01",
      "status": "Not Started"
    },
    {
      "id": "phase_01b",
      "title": "Add CPU Alpha Mask Sampling Cache For Bake Occlusion",
      "type": "pass",
      "parent": "phase_01",
      "status": "Not Started"
    },
    {
      "id": "phase_02",
      "title": "Alpha-Aware Static Light Occlusion",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_02a",
      "title": "Integrate Alpha-Tested Occluders Into Static Ray Occlusion",
      "type": "pass",
      "parent": "phase_02",
      "status": "Not Started"
    },
    {
      "id": "phase_02b",
      "title": "Apply Alpha-Aware Occlusion To Static Points Spots And Directional Light",
      "type": "pass",
      "parent": "phase_02",
      "status": "Not Started"
    },
    {
      "id": "phase_03",
      "title": "Bake Version Tests Polish And Completion",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_03a",
      "title": "Bump Bake Version Strengthen Tests And Close Plan",
      "type": "pass",
      "parent": "phase_03",
      "status": "Not Started"
    }
  ]
}
```

## Current Progress

| Phase / Pass                                                                       | Status      | Date | Notes                                                                                              |
| ---------------------------------------------------------------------------------- | ----------- | ---- | -------------------------------------------------------------------------------------------------- |
| Phase 1: Alpha-Test Occluder Data And Texture Sampling                             | Not Started |      | Parent phase.                                                                                      |
| Phase 1A: Collect Alpha-Tested Middle Texture Occluders For Baking                 | Not Started |      | First executable pass. Build bake-side occluder data, no lighting behavior change yet if possible. |
| Phase 1B: Add CPU Alpha Mask Sampling Cache For Bake Occlusion                     | Not Started |      | CPU texture alpha lookup for bake rays.                                                            |
| Phase 2: Alpha-Aware Static Light Occlusion                                        | Not Started |      | Parent phase.                                                                                      |
| Phase 2A: Integrate Alpha-Tested Occluders Into Static Ray Occlusion               | Not Started |      | Rays continue through transparent texels and block on opaque texels.                               |
| Phase 2B: Apply Alpha-Aware Occlusion To Static Points Spots And Directional Light | Not Started |      | Ensure shared static bake paths use the new occlusion behavior.                                    |
| Phase 3: Bake Version Tests Polish And Completion                                  | Not Started |      | Parent phase.                                                                                      |
| Phase 3A: Bump Bake Version Strengthen Tests And Close Plan                        | Not Started |      | Invalidate old baked data, add coverage, document behavior.                                        |

## Execution Tracking Rules

* Each pass must leave the project buildable and runnable.
* Each pass final report must state whether source code changed.
* Each implementation pass must update this document before finishing.
* The update should be small and local.
* Do not rewrite unrelated phases when marking progress.
* If behavior is intended to remain unchanged, explicitly state that.
* If a pass changes bake output, bake versioning, source hash behavior, texture loading, generated geometry, runtime rendering, save/load, cache invalidation, or build/test behavior, clearly say so.
* Do not claim manual GUI verification unless it was actually performed.
* If a pass is too broad, split it into smaller child passes and stop without source changes.

## Goal And Desired End State

Make baked static lighting respect alpha-tested middle textures.

Current dynamic spotlight shadow maps already support alpha-tested middle texture cutouts. Static baked lighting should match that behavior so grates, bars, fences, and other cutout middle textures do not cast solid rectangular baked shadows.

Desired behavior:

```text
static light ray hits opaque wall:
  block light

static light ray hits alpha-tested middle texture:
  sample texture alpha at hit UV
  alpha >= alphaCutoff -> block light
  alpha < alphaCutoff  -> continue ray

static light ray passes through transparent/cutout texel:
  continue checking farther occluders
```

This should apply to baked static direct lighting for:

* static point lights
* static spotlights
* static directional light if it uses the same/static occlusion path

This does not affect runtime dynamic shadow maps except that baked and dynamic shadow behavior should now visually agree better.

## Dependency Direction Rules

* Bake occlusion may depend on generated sector geometry and middle texture material data.
* Bake occlusion may read CPU texture alpha data through a bake-side cache.
* Runtime draw/rendering must not depend on the bake alpha cache.
* Dynamic shadow map rendering must not be changed by this plan.
* Static light source data and authored topology format should not need new fields.
* Because bake output changes, the lightmap bake version or equivalent bake invalidation version must be updated before the plan is complete.

## Proposed Phases

### Phase 1: Alpha-Test Occluder Data And Texture Sampling

Goal:

Prepare bake-side data needed to evaluate alpha-tested middle textures during static light occlusion.

Why it helps:

The bake ray code needs both hit geometry and texture alpha at the hit UV. This phase creates those ingredients before changing lighting results.

Files/functions likely touched:

* `sources/sector_demo/SectorLightmap.cpp`
* generated geometry / mesh builder types if needed
* texture/material lookup helpers
* asset/image loading helpers if CPU texture access already exists
* lightmap tests

Exact behavior that must remain unchanged:

* Runtime rendering unchanged.
* Dynamic shadow maps unchanged.
* Static bake output unchanged until Phase 2 if possible.
* Save/load unchanged.
* Lightmap source hash/bake version unchanged until Phase 3 unless unavoidable.

Risks/goblins:

* UV at ray-hit point not matching visible middle texture UV.
* CPU texture data unavailable after GPU upload.
* AssetManager texture path/filtering not designed for CPU alpha reads.
* Magenta cutout textures without real alpha may need existing alpha-test behavior clarified.
* Bake performance could degrade from repeated texture sampling.

Non-goals:

* No new authored fields.
* No dynamic shadow changes.
* No runtime shader changes.
* No GUI changes.
* No volumetric shafts/god rays.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant unit tests.

Final report expectations:

* State how alpha-tested occluders are represented.
* State how CPU alpha data is loaded/cached.
* State whether bake output changed in this pass.
* State verification commands/results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 1 `Completed` only after Phase 1A and Phase 1B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 1A: Collect Alpha-Tested Middle Texture Occluders For Baking

Goal:

Make the baker aware of alpha-tested middle texture geometry as special occluders.

Implementation guidance:

Identify generated surfaces/draw records corresponding to alpha-tested middle textures.

Collect enough data for bake ray intersections:

```text
world-space geometry
surface normal / plane data if needed
base UVs at vertices
texture ID/path
alpha cutoff
sector/topology references for debugging if available
```

Only alpha-tested middle/cutout surfaces need this special behavior.

Opaque sector geometry should continue using existing occlusion behavior.

Sky geometry should not become a bake occluder.

Decals should not be treated as bake occluders.

Expected behavior after this pass:

* Data exists and is testable.
* Bake output may remain unchanged until Phase 2.

Tests:

* generated middle texture surface marked alpha-test becomes an alpha occluder
* opaque wall/floor/ceiling surfaces do not become alpha occluders
* sky surfaces/cylinder are excluded
* occluder data preserves UVs and alpha cutoff

#### Phase 1B: Add CPU Alpha Mask Sampling Cache For Bake Occlusion

Goal:

Allow bake code to sample texture alpha deterministically on CPU.

Implementation guidance:

Add a bake-side cache keyed by texture ID/path or existing texture dictionary key.

Cache should provide:

```text
width
height
alpha lookup at UV
valid/invalid state
```

Sampling:

* nearest-neighbor is acceptable and probably preferable for hard alpha-test cutouts
* wrap/repeat UVs consistently with visible texture behavior if textures tile
* clamp only if that matches current UV behavior; otherwise repeat is likely correct
* alpha cutoff comparison should match runtime alpha-test semantics

If texture alpha data cannot be loaded:

* prefer conservative opaque behavior
* report/log/debug this in a way that helps diagnose missing alpha data
* do not crash the bake

If the engine uses magenta-key textures converted to alpha at import/load, use the same effective alpha behavior if available. Do not invent a new magenta rule unless the current renderer already does.

Tests:

* alpha sample below cutoff means transparent
* alpha sample above cutoff means opaque
* tiled UVs sample consistently
* missing/unreadable alpha texture behaves conservatively as opaque
* cache avoids repeatedly reloading the same texture where testable

### Phase 2: Alpha-Aware Static Light Occlusion

Goal:

Use alpha-tested occluder data during static light ray occlusion.

Why it helps:

This is where baked static lights stop treating grates/bars/middle textures as solid slabs.

Files/functions likely touched:

* `sources/sector_demo/SectorLightmap.cpp`
* static light occlusion/raycast helpers
* static point/spot/directional light evaluation code
* lightmap tests

Exact behavior that must remain unchanged:

* Opaque wall/floor/ceiling occlusion remains unchanged.
* Static point/spot light contribution math remains unchanged except alpha occlusion.
* Static directional light contribution remains unchanged except alpha occlusion if it shares the path.
* AO behavior unchanged unless it explicitly shares the same occlusion helper and is intentionally included.
* Dynamic shadow maps unchanged.
* Runtime rendering unchanged.

Risks/goblins:

* Ray stops at first alpha-tested transparent texel instead of continuing.
* Ray self-intersects source/receiver geometry.
* Multiple alpha occluders along one ray not handled.
* Texture UV interpolation wrong at hit point.
* Bake time increases too much.

Non-goals:

* No soft/translucent partial alpha lighting. This is alpha-test, not transparency.
* No colored transmission.
* No dynamic shadow changes.
* No new light types.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant lightmap tests.

Final report expectations:

* State occlusion algorithm.
* State which light types use alpha-aware occlusion.
* State how transparent texels continue ray traversal.
* State performance notes if obvious.
* State verification commands/results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 2 `Completed` only after Phase 2A and Phase 2B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 2A: Integrate Alpha-Tested Occluders Into Static Ray Occlusion

Goal:

Make static occlusion rays continue through transparent alpha-tested middle texture texels and block on opaque texels.

Implementation guidance:

When testing a shadow/occlusion ray:

1. Find nearest hit along ray.
2. If hit is opaque geometry, block light as before.
3. If hit is alpha-tested middle texture:

    * compute hit UV
    * sample alpha
    * if alpha >= cutoff, block light
    * if alpha < cutoff, continue ray just beyond the hit point
4. Repeat until:

    * an opaque/alpha-opaque blocker is found
    * the ray reaches the light/sample endpoint
    * a safe iteration cap is reached

Use a small epsilon step after transparent alpha hits to avoid re-hitting the same surface.

If iteration cap is hit, choose conservative behavior and document it. Prefer treating as blocked if the ray logic is uncertain.

Tests:

* ray through transparent texel reaches light
* ray through opaque texel is blocked
* ray through transparent first alpha surface can still be blocked by a farther wall
* repeated transparent alpha hits do not infinite-loop
* opaque geometry behavior unchanged

#### Phase 2B: Apply Alpha-Aware Occlusion To Static Points Spots And Directional Light

Goal:

Ensure all relevant baked direct light paths use alpha-aware occlusion.

Implementation guidance:

Apply the shared alpha-aware occlusion helper to:

* static point light direct lighting
* static spotlight direct lighting
* static directional light direct lighting if it uses the same occlusion/ray path

Static spotlights and static point lights should both benefit from alpha-tested middle texture shadows.

If directional light integration is not straightforward, document why and leave a clear follow-up, but prefer using the shared helper if possible.

Soft static lights:

* if static point/spot sourceRadius uses multiple source samples, each sample ray should use alpha-aware occlusion
* this should naturally create softer baked cutout shadows when sourceRadius > 0

Tests:

* static point light casts cutout shadow through middle texture
* static spotlight casts cutout shadow through middle texture
* static directional light casts cutout shadow if supported by current bake path
* existing static point/spot bake tests still pass

Manual bake smoke:

* place alpha-tested bars/grate between static light and receiver
* bake
* confirm cutout pattern in lightmap
* compare static point and static spot behavior
* confirm opaque walls still block normally

### Phase 3: Bake Version Tests Polish And Completion

Goal:

Invalidate old baked lightmaps, strengthen tests, and document the new bake behavior.

Why it helps:

Changing bake occlusion changes baked output. Old lightmaps without alpha-aware occlusion should not silently appear valid.

Files/functions likely touched:

* lightmap bake version/source hash/version constant
* tests
* docs
* this plan document

Exact behavior that must remain unchanged:

* Runtime rendering unchanged.
* Dynamic shadows unchanged.
* Save/load format unchanged unless a bake version field is already part of existing lightmap metadata.
* Authored topology unchanged.

Risks/goblins:

* Forgetting to invalidate old lightmaps.
* Over-invalidating non-lightmap data.
* Weak test coverage around source hash/bake version.
* Bake performance unclear.

Non-goals:

* No new UI beyond existing bake invalidation/status.
* No dynamic shadow changes.
* No new assets unless a tiny test map is intentionally expected.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run full relevant test suite.

Final report expectations:

* State bake version/hash invalidation change.
* State docs updated.
* State tests added/updated.
* State manual bake smoke status.
* State known deferred work.

How to update this plan after completion:

* Mark Phase 3A and Phase 3 `Completed`.
* If all phases are complete, ensure all parent phases are `Completed`.
* Leave a final completion note.

#### Phase 3A: Bump Bake Version Strengthen Tests And Close Plan

Goal:

Complete the alpha-tested baked occlusion feature.

Implementation guidance:

Bump the lightmap bake version or equivalent bake invalidation constant.

Reason:

```text
Lightmap bake output changed because alpha-tested middle textures now affect static direct-light occlusion.
```

Ensure tests cover:

* old bake version is considered stale if existing tests cover this
* alpha-tested occlusion affects baked direct lighting
* opaque occlusion remains unchanged
* transparent alpha texel lets light pass
* opaque alpha texel blocks light
* static point light path
* static spotlight path
* directional path if supported
* missing alpha data conservative behavior
* UV repeat/tile behavior where practical
* no dynamic shadow regression

Document:

* alpha-tested middle textures now affect static baked shadows
* this applies to static point and static spot lights
* directional support status
* this is binary alpha-test, not colored transparency
* dynamic shadow map alpha-test path is separate but intended to match visually
* old lightmaps are invalidated by bake version bump

Manual smoke checklist:

* bake a grate/bars alpha-tested middle texture between static point light and floor/wall
* bake same with static spotlight
* confirm cutout shadows appear
* confirm changing sourceRadius softens baked result if current static soft shadow sampling supports it
* confirm dynamic shadow cutout behavior still works
* confirm runtime performance unchanged after bake

## Deferred Decisions For Later Plans

These are intentionally out of scope:

* Colored transparent light transmission.
* Partial translucent shadow strength from alpha values.
* Volumetric light shafts.
* God rays.
* Dynamic shadow algorithm changes.
* Runtime PCF/PCSS changes.
* New material flags for special shadow behavior.
* Shadow caster culling optimizations.
* Bake performance parallelization.

## Final Completion Criteria

This plan is complete when:

* static bake occlusion can detect alpha-tested middle texture hits
* CPU alpha sampling matches runtime alpha-test semantics closely enough
* transparent alpha texels let static light rays continue
* opaque alpha texels block static light rays
* static point lights cast baked cutout shadows
* static spotlights cast baked cutout shadows
* static directional light status is implemented or explicitly documented
* old baked lightmaps are invalidated by bake version/source version update
* opaque occlusion behavior remains unchanged
* dynamic shadow map behavior remains unchanged
* no volumetric/translucency/dynamic-shadow scope leaks into this implementation
