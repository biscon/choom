# Unified Volumetric Atmosphere Plan

## How To Use This Living Plan

This document is the execution plan and compacted investigation record for
replacing the existing per-light haze and separate local-fog ray marches with
one shared-medium volumetric atmosphere renderer. The final renderer must work
on the project's OpenGL 3.3 core profile without compute shaders.

The legacy `SectorLightHazeRenderer` may coexist with the replacement only
while comparison and migration are in progress. It is not a permanent fallback
or alternate rendering mode. Completion means the old haze renderer, per-light
haze schema, editor controls, tests, and runtime source-building paths have
been removed.

When executing this plan:

1. Read this document, `AGENTS.md`, and
   `docs/architecture/sector_editor_architectural_principles.md` before making
   changes.
2. Execute only the requested slice. Do not silently combine slices or begin
   the next slice.
3. Set the selected slice to **In Progress** before implementation. On the same
   run, update it to **Completed**, **Partial**, or **Blocked** and append an
   execution-log entry with the date, changes, measurements, tests, decisions,
   and remaining debt.
4. Treat the contracts and decisions below as the current specification. If
   implementation invalidates an assumption, update the applicable contract
   before recording the slice result.
5. Keep every completed slice buildable. Until Slice 7, keep the legacy path
   usable for comparison; do not let temporary adapters become the final data
   model.
6. Keep the implementation in focused renderer/data helpers. Do not grow the
   volumetric implementation inside `SectorEditor.cpp`, `Main.cpp`, or an
   existing material shader.
7. Do not allocate, load files/assets, compile shaders, or resize GPU resources
   during the normal steady render path. Pre-reserve CPU work buffers and only
   create/resize render resources during initialization, preview rebuild, or
   target-size/quality changes.
8. Do not perform interactive or xdotool smoke tests. The user owns visual and
   playability verification; record it only when the user reports results.
9. At the end of every implementation slice run the project checks unless the
   execution log records why one was inapplicable:

   ```sh
   cmake --build cmake-build-debug -j2
   ctest --test-dir cmake-build-debug --output-on-failure
   git diff --check
   git diff --stat
   git status --short
   ```

### Slice state

| Slice | Title | Status | Completed |
|---|---|---|---|
| 1 | Establish baseline measurements, diagnostics, and renderer contracts | Completed | 2026-08-17 |
| 2 | Add the shared-medium screen-space renderer and comparison mode | Completed | 2026-08-17 |
| 3 | Unify global fog and authored local fog under the final data model | Completed | 2026-08-17 |
| 4 | Add the OpenGL 3.3 froxel atlas and clustered light lists | Not Started | - |
| 5 | Add reconstruction, temporal stability, and supported volumetric shadows | Not Started | - |
| 6 | Migrate and tune the hub maps, then pass visual/performance gates | Not Started | - |
| 7 | Retire the legacy haze and local-fog renderers completely | Not Started | - |
| 8 | Harden lifecycle, diagnostics, documentation, and acceptance coverage | Not Started | - |

## Goal And Acceptance Criteria

Build one continuous-medium atmosphere system in which global fog and authored
local fog describe the air while all relevant lights illuminate that shared
medium. It must remove the current nearest-eight-haze-volume limitation and the
costly nested volume/march/light loop while retaining the inexpensive analytic
distance fog as a far-field continuation.

Completion requires all of the following:

- The runtime has exactly one continuous volumetric renderer. The old
  `SectorLightHazeRenderer` and `SectorLocalFogRenderer` no longer compile or
  run.
- There is no serialized per-light `haze` block and no light inspector haze
  extent/density/noise/flow UI. Tracked maps containing it are migrated.
- Global atmosphere and authored local fog volumes contribute density to the
  same medium and are integrated once per view ray.
- Static point/spot lights, dynamic point/spot lights, and the existing two
  dynamic spotlight shadow slots can illuminate the medium without using the
  surface renderer's global eight-light selection as the volumetric limit.
- The renderer supports up to 254 relevant visible volumetric lights per view
  and retains the 16 most important overlapping lights per coarse depth
  cluster, with deterministic selection and visible overflow diagnostics. This
  is a spatial budget, not an arbitrary eight-light whole-view cap.
- Unlit air contributes no emissive color. A totally dark room remains dark;
  fog becomes visible only through actual static/dynamic lighting or the
  explicitly lit analytic far-field response.
- Low, Medium, and High use bounded froxel resolutions and quality costs that
  are independent of world-render supersampling. Off disables the volumetric
  renderer without disabling analytic distance fog.
- The implementation uses GLSL 330 fragment passes and 2D textures/atlases. It
  does not require compute shaders, SSBOs, OpenGL 4.x, or 3D texture rendering.
- Depth-aware reconstruction avoids obvious foreground halos. Temporal history
  rejects invalid/disoccluded samples and resets on all documented lifecycle
  events.
- Missing shaders, unsupported target formats, allocation failure, or resource
  loss disables volumetrics with one stable diagnostic and leaves the scene
  renderable with analytic distance fog.
- On the same hub reference view and hardware, new High atmosphere median GPU
  time is no more than 70% of legacy High and its 95th percentile is no more
  than 75% of legacy High over 300 post-warmup frames. New Medium must not be
  slower than legacy Medium. Record raw measurements rather than claiming a
  hardware-independent speedup.
- The user accepts the migrated hub appearance before legacy deletion. The
  result need not be pixel-identical, but must preserve intentional shafts,
  readable darkness, and the sense of illuminated air without light-shaped
  additive blobs.
- Dust remains a separate particle effect. Collision, sector lookup, physics,
  topology geometry, picking, sky geometry, and baked lightmap results do not
  change.

## Fixed Scope And Decisions

### Final composition model

- **Analytic distance fog stays.** It is the cheap far-field/fallback layer and
  is not considered a second volumetric system. When volumetrics are active,
  its optical path begins where the finite volumetric integration distance
  ends so density is not applied twice.
- **One volumetric atmosphere renderer stays.** It owns global medium density,
  authored local fog density, direct light scattering, integration,
  reconstruction, temporal history, and HDR composition.
- **Dust stays separate.** `SectorLightDustRenderer` continues drawing discrete
  motes after continuous atmosphere. Its authoring and simulation are not
  folded into the froxel grid.
- Bloom continues after atmosphere. Volumetric scattering is HDR radiance and
  may naturally feed bloom; sky remains governed by its existing bloom policy.
- Atmosphere remains a visual preview/runtime effect. It has no collision,
  picking, lightmap receiver/occluder, or generated surface metadata role.

### Chosen OpenGL 3.3 architecture

- Use a camera-aligned froxel grid stored in packed 2D RGBA16F atlases. Each Z
  slice occupies one XY tile in a near-square atlas; shaders map atlas texels
  to `(x, y, z)` explicitly.
- Use logarithmic view-depth slices from the effective fog start distance to
  `volumetricMaxDistanceWorld`. Clamp the last segment to scene depth during
  integration.
- Use fragment-shader fullscreen passes for medium injection, light scattering,
  front-to-back integration, temporal resolve, and HDR composition.
- Do not add a generic render graph or generic 3D texture framework. Add only
  focused renderer-owned atlas/data-texture helpers needed by this feature.
- CPU code builds coarse-Z clustered light and local-volume lists into
  pre-sized byte buffers and updates persistent 2D data textures with
  `glTexSubImage2D` or the equivalent raylib path. It must not allocate or
  create textures per frame.
- Light records live in a renderer-owned RGBA32F data texture, four texels per
  light: position/type, color/intensity, direction/range, and cone/scattering/
  shadow metadata. Index 255 is the list terminator, leaving indices 0-253 for
  the 254-view-light budget.
- Cluster index textures store 16 indices as four RGBA8 texels per
  `(froxel X, froxel Y, coarse Z band)` cluster. Pack bands vertically in one
  light-list texture and one local-volume-list texture. Normalized byte samples
  are rounded back to integer indices in GLSL 330.
- Query `GL_MAX_TEXTURE_SIZE` and required framebuffer/float-texture support.
  If the requested atlas cannot fit, step down through the quality presets; if
  Low cannot fit, disable volumetrics with a diagnostic.

### Quality presets

Froxel XY dimensions preserve the current camera aspect ratio and are capped
by the listed 16:9 reference dimensions. They do not scale up with the 1.5x
world-render target.

| Quality | Reference XY | Z slices / cluster bands | Temporal resolve | Maximum cluster lights/volumes |
|---|---:|---:|---|---:|
| Off | none | 0 | no | 0 |
| Low | 120 x 68 | 32 / 4 | no | 16 / 16 |
| Medium | 160 x 90 | 48 / 6 | yes | 16 / 16 |
| High | 240 x 135 | 64 / 8 | yes | 16 / 16 |

- Bilateral depth-aware reconstruction is enabled at every active quality.
- The 254 relevant-light and 254 relevant-local-volume view budgets are hard
  safety bounds. View and cluster overflow retain the highest deterministic
  importance and increment diagnostics; they never grow buffers mid-frame.
- Every cluster band covers eight contiguous logarithmic froxel slices. Light
  and volume bounds must overlap both the projected XY cell and band depth
  interval before competing for that cluster's slots.
- Light importance uses projected cluster influence multiplied by effective
  intensity and per-light volumetric scattering intensity, with stable light
  kind/ID tie-breaking.
- Local-volume importance uses distance from the cluster center to bounds
  followed by stable authoring volume ID. Global fog is uniform state and
  consumes no list slot.

### Medium and lighting model

- Global density uses the existing fog start distance, density, reference
  height, and height falloff. Existing fog color is the scattering tint.
- Authored local fog volumes retain their current ellipsoid, density, maximum
  opacity, edge softness, noise, and flow behavior, but inject into the same
  medium instead of launching a separate ray march.
- Combine overlapping extinction additively. Combine scattering tint weighted
  by each contributor's extinction. Integrate transmittance with Beer-Lambert
  attenuation and store premultiplied scattering plus opacity for the existing
  HDR atmosphere composition policy.
- Use a Henyey-Greenstein phase approximation controlled by map-level
  anisotropy. The initial default is `0.20`, normalized to `[-0.90, 0.90]`.
- The initial default volumetric maximum distance is `32.0` world units,
  normalized to `[1.0, 256.0]`.
- Direct static and dynamic point/spot lights use their existing world-space
  position, color, effective intensity, range, direction, and cone falloff.
  Static lights are evaluated as direct, unshadowed volumetric emitters in V1;
  this does not change their baked surface-lighting behavior.
- Dynamic spotlights use their existing shadow map only when they own one of
  the two selected shadow slots and `castsShadow` is enabled. Dynamic point
  lights remain unshadowed, matching current engine capability.
- Sector ambient and scattering tint must not be treated as emission. Missing
  light probes or a dark probe result cannot make fog glow. Existing baked
  object probes may provide a bounded ambient/static baseline for authored
  local volumes, but direct static lights must not be double-counted.
- Directional sun/moon volumetric scattering and baked volumetric shadow fields
  are deferred. Do not add them implicitly while implementing point/spot V1.

### Final authored and runtime data

Extend the existing root `fogSettings` map metadata rather than adding a second
atmosphere object:

```cpp
struct SectorTopologyFogSettings {
    bool enabled;
    Color color;
    float startDistanceWorld;
    float density;
    float maxOpacity;
    float referenceHeightWorld;
    float heightFalloff;
    float anisotropy = 0.20f;
    float volumetricMaxDistanceWorld = 32.0f;
    VolumetricQuality volumetricQuality = VolumetricQuality::Medium;
};
```

- Rename the C++ and JSON concept `localVolumeQuality` to
  `volumetricQuality`; the final implementation must not keep both fields.
- `enabled` controls the shared global medium and analytic far field.
  `volumetricQuality == Off` disables only finite volumetric integration.
- Disabled global fog does not disable authored local fog. The unified renderer
  runs whenever quality is active and either global fog or at least one visible
  local volume contributes density.
- `maxOpacity` remains the artist safety ceiling for combined global
  atmosphere. Local volumes retain their own per-volume opacity limit.
- Map-level metadata continues through the existing document map-data path;
  derived `SectorTopologyMap` is renderer input. Do not create a normal editor
  mutation path that treats derived topology as independent source data.

Replace the haze portion of per-light atmosphere with one small light control:

```cpp
struct SectorLightAtmosphereSettings {
    float volumetricScatteringIntensity = 1.0f;
    SectorLightDustSettings dust;
};
```

- Normalize volumetric scattering intensity to `[0.0, 8.0]`; `0` excludes the
  light from volumetric scattering, while `1` follows its physical/artistic
  light intensity.
- Do not retain light-owned extent, haze density, haze tint, edge softness,
  noise, noise scale, or flow fields in the final schema. Density belongs to
  global atmosphere or an authored local fog volume; color belongs to the
  medium and the light.
- Reuse the existing light `castsShadow` setting. Do not introduce a second
  volumetric-shadow checkbox while only the existing two spotlight shadow
  slots can provide shadows.
- Default-valued per-light atmosphere remains omitted on save. Dust JSON and
  behavior remain backward-compatible.

### Editor contract

- Preview Settings -> Fog owns Enabled, Volumetric Quality, Start Distance,
  Density, Maximum Opacity, Reference Height, Height Falloff, Anisotropy,
  Volumetric Maximum Distance, and Color.
- The light inspector replaces the Haze subsection with one **Volumetric
  Scattering** scalar. The existing Dust subsection remains.
- Existing authoring-graph local fog tools continue to own local density
  placement and editing. Their mutations continue through the document-edited
  and topology-cache invalidation route; no topology-only fallback is added.
- Map/global fog and light changes refresh preview atmosphere resources through
  their existing map-metadata/light editing services. Do not perform topology
  validation, loop extraction, triangulation, shader compilation, or GPU target
  allocation from steady 2D draw.
- Add an Atmosphere diagnostic block to the existing 3D preview diagnostics,
  not a new floating window. Show backend during migration, effective quality,
  grid/atlas sizes, memory estimate, relevant/retained lights and volumes,
  view/cluster overflow counts, history validity, fallback reason, and delayed
  GPU timings.

### Temporary migration mode

- Slices 2-6 may expose **Legacy** and **Unified** backend selection in the 3D
  preview diagnostics. This toggle is editor/debug runtime state only and must
  not be serialized into maps or application settings.
- Never execute both continuous-medium backends in one frame. The toggle
  selects exactly one; dust still runs afterward.
- The Unified prototype may translate legacy haze records into transient
  density volumes during Slices 2-3 strictly for A/B comparison. Name the
  adapter `LegacyHazeComparisonAdapter`, keep it outside final authored types,
  and delete it in Slice 7.
- Slice 4 makes Unified the development default. Slice 7 removes the selector
  and all Legacy branches rather than leaving a fallback.

### Lightmap, topology-cache, and gameplay policy

- Global volumetric settings, local fog appearance, per-light volumetric
  intensity, dust, temporal settings, and quality settings are visual-only and
  remain excluded from `ComputeSectorLightmapSourceHash()`.
- Existing point lights, static spotlights, directional lighting, geometry,
  receivers, occluders, and `ceilingSky` keep their current source-hash rules.
  No lightmap artifact format or bake result changes in this plan.
- Local fog authoring mutations must continue invalidating the 2D topology
  render cache because fog volumes are visibly cached/pickable editor state.
  Global quality/lighting-result changes do not require cache invalidation
  unless cached 2D display begins storing them.
- Collision, sector lookup, portal traversal, physics, camera motion, and sky
  geometry are unchanged. Camera matrices used for temporal reprojection are
  visual data only.

## Compacted Investigation Record

### Current renderer and cost shape

- `SectorLightHazeRenderer` already performs one fullscreen accumulation draw,
  so draw-call count is not the main problem.
- The shader supports at most eight selected haze volumes. High quality runs at
  full world-target resolution with 12 samples.
- Each intersecting volume first loops over all samples to estimate inside
  length, then loops again for density/scattering. Every scattering sample can
  evaluate all eight selected dynamic lights and up to four spotlight shadow
  taps.
- A worst-case High pixel can therefore approach `8 volumes * 12 samples * 8
  lights = 768` dynamic-light evaluations, in addition to shape/noise work. At
  the current 1.5x world-render scale, High operates on a 2880x1620 target for a
  1920x1080 presentation.
- `SectorLocalFogRenderer` performs another fullscreen ray march before haze.
  `ApplyWorldAtmosphere()` then ping-pongs HDR targets and runs dust after both
  continuous-medium passes.
- The main renderer already exposes aggregate atmosphere CPU/GPU timing using
  delayed OpenGL timer queries. Slice 1 should extend this with non-blocking
  timestamp pairs for atmosphere subpasses; nested `GL_TIME_ELAPSED` queries
  are not valid while the aggregate query is active.

### Existing reusable inputs

- The world scene target is RGBA16F with a sampleable depth texture, which is
  sufficient for bounded screen-space volumetrics and HDR composition.
- Current dynamic lighting provides point/spot position, color, effective
  runtime intensity, range, cone data, stable IDs, and two selected spotlight
  shadow maps. The surface/billboard upload context is capped at eight, so the
  volumetric renderer must build its own broader source list from the renderer's
  full light sources.
- Static point/spot lights and authored local fog volumes are already compiled
  into world units during explicit preview/level rebuild.
- Existing baked object probes can supply coarse static lighting for local
  media, but they are not a baked volumetric shadow field.
- Existing render-target helpers support arbitrary 2D RGBA16F targets. A packed
  atlas therefore avoids introducing 3D render-target ownership.
- OpenGL timer queries already use four-frame latency in `Main.cpp`; the same
  non-blocking discipline applies to subpass diagnostics.

### Current authored/editor state

- Every static/dynamic point/spot light owns `SectorLightHazeSettings` plus
  `SectorLightDustSettings` under `SectorLightAtmosphereSettings`.
- Haze shape is inferred from the owning light: point lights produce spheres,
  spotlights produce finite cone proxies, and extent scales light range.
- Local fog is already authored in `SectorAuthoringGraph` and compiled into
  `SectorCompiledLocalFogVolume`; it is the correct source for spatial density
  that is independent of a light.
- Preview Settings -> Fog currently stores `localVolumeQuality`; light
  inspectors expose the legacy haze parameters.
- The repository currently contains 11 serialized haze blocks in
  `assets/levels/hub/hub.json` and 5 in
  `assets/levels/hub2/hub2.json`. Both tracked maps must be inspected and
  migrated even if `hub2` is only a derivative/test copy. Automated tests must
  not depend on either map's editable contents.

### Modern-engine reference points

- Unreal documents a low-resolution camera-frustum volume with temporal
  reprojection and spatially bounded local fog/light work:
  https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine
- Unity HDRP documents volumetric lighting on a camera-frustum 3D grid and
  quality/temporal tradeoffs:
  https://docs.unity3d.com/cn/Packages/com.unity.render-pipelines.high-definition%4010.4/manual/Volumetric-Lighting.html
- Frostbite describes the value of unifying participating media, lighting, and
  volumetric shadows rather than treating each visible haze as an independent
  effect:
  https://www.ea.com/news/physically-based-unified-volumetric-rendering-in-frostbite?isLocalized=true
- Compute shaders are core only from OpenGL 4.3. This plan deliberately uses
  fragment passes and 2D textures available to the project's 3.3 profile:
  https://wikis.khronos.org/opengl/Compute_Dispatch

## Target Runtime Architecture

### Ownership and data flow

```text
map/global fog metadata + compiled local fog volumes
                         |
full static/dynamic light sources + shadow slots
                         |
                         v
        SectorVolumetricAtmosphereRenderer
          | build fixed-capacity coarse-Z cluster lists on CPU
          | upload light/volume data textures
          | inject medium into packed froxel atlas
          | inject direct lighting/scattering atlas
          | integrate each XY column to low-res atmosphere
          | temporal resolve (Medium/High)
          | bilateral reconstruct and HDR composite
                         |
                         v
             SectorLightDustRenderer
                         |
                         v
                       bloom
```

- `SectorMeshRenderer` owns and sequences the renderer, forwards map/light/
  depth/camera inputs, and releases resources on reset/shutdown.
- `SectorVolumetricAtmosphereRenderer` owns shaders, atlases, history targets,
  GPU data textures, fixed-capacity CPU staging buffers, diagnostics, and
  previous visual camera state.
- Pure helpers own quality sizing, atlas packing, logarithmic depth slicing,
  projected bounds, deterministic importance, list insertion/overflow, history
  reset decisions, and analytic-far-field handoff. Keep them CPU-testable.
- Do not store raylib/OpenGL resources in ECS components or serialized map
  structs.

### Pass contract

1. **Prepare:** normalize settings, resolve effective quality cap, collect
   relevant visible lights/volumes, build coarse-Z cluster lists, and update
   persistent data textures.
2. **Medium injection:** write extinction plus extinction-weighted scattering
   tint for global and local density to the medium atlas.
3. **Light injection:** read medium and cluster light lists; write direct
   in-scattering and extinction to the lighting atlas. Apply phase response,
   cone attenuation, distance falloff, per-light intensity, and available
   dynamic spotlight shadowing.
4. **Column integration:** traverse logarithmic Z cells front-to-back, stop at
   reconstructed scene depth, and write premultiplied scattering plus opacity
   to the low-resolution atmosphere target.
5. **Temporal resolve:** reproject previous atmosphere/depth for Medium/High,
   reject invalid history, neighborhood-clamp radiance/opacity, and blend.
6. **Composite:** bilateral depth-aware reconstruction into the HDR scene using
   the existing premultiplied atmosphere equation.

### History invalidation

Reset temporal history on:

- first frame, renderer reset/shutdown, preview enter/exit, or scene rebuild;
- target, aspect, quality, atlas layout, shader, or fog-setting change;
- map/light/probe source revision change that rebuilds static inputs;
- camera FOV/projection change;
- camera translation greater than 2 world units in one frame, rotation greater
  than 30 degrees, or a render gap greater than 0.25 seconds;
- an unavailable/failed resource frame or backend switch during migration.

Ordinary movement, light animation, flicker, and door motion do not reset all
history. They rely on depth rejection, neighborhood clamping, and a responsive
current-frame weight to avoid trails.

## Implementation Slices

### Slice 1 — Establish Baseline Measurements, Diagnostics, And Renderer Contracts

**Intent:** make performance and scaling problems measurable before changing
the rendered result.

Required implementation:

- Add a focused volumetric diagnostics model with legacy eligible/active haze
  counts, local-fog counts, quality, target sizes, estimated bytes, march steps,
  and stable resource/fallback diagnostics.
- Extend the existing delayed GPU profiler with timestamp-pair measurements for
  local fog, haze accumulation/composite, and dust. Do not nest
  `GL_TIME_ELAPSED`; never block waiting for query results.
- Add an editor 3D Atmosphere diagnostics section and a deterministic 300-frame
  capture action that skips 60 warmup frames and reports median, p95, min/max,
  target resolution, quality, active counts, and camera transform.
- Add pure quality/atlas/cluster-list contracts and compile-time constants for
  the final budgets. Do not create the new renderer or change map JSON in this
  slice.
- Record baseline Low/Medium/High hub measurements for at least one sparse view
  and the known dense haze view. If the user has not run the GUI capture, mark
  the measurement portion Partial rather than inventing results.

Tests:

- Median/p95 calculation, warmup exclusion, empty capture, query-ring latency,
  unavailable-query behavior, and diagnostic formatting.
- Existing rendering pass order and atmosphere timing remain intact.
- No map, cache, lightmap hash, collision, or rendered-image behavior changes.

### Slice 2 — Add The Shared-Medium Screen-Space Renderer And Comparison Mode

**Intent:** prove the central cost/appearance hypothesis with one low-resolution
ray march before building the froxel infrastructure.

Required implementation:

- Add `SectorVolumetricAtmosphereRenderer` with a GLSL 330 low-resolution
  screen-space path that marches each view ray once and combines global density
  plus transient legacy haze/local-fog density at each sample.
- Evaluate the current selected dynamic lights and cached static/probe lighting
  once per sample after density combination, eliminating the legacy
  `volume * march * light` nesting. Retain bounded fixed shader arrays only for
  this prototype.
- Reuse the existing RGBA16F atmosphere composition and sampleable scene depth.
  Add depth-aware upsampling; temporal history is not part of this slice.
- Add the nonserialized Legacy/Unified diagnostics toggle. Only one continuous
  backend runs per frame. Legacy remains the default in this slice.
- Implement and label `LegacyHazeComparisonAdapter`; do not add its output to
  serialized/runtime map types.
- Preserve local fog, haze, dust, analytic distance fog, and lightmap behavior
  while Unified is not selected.

Tests/acceptance:

- Pure combined-density, Beer-Lambert integration, phase, depth clipping, and
  premultiplied composition helpers.
- Unified handles zero media/lights, camera inside a volume, overlapping
  volumes, sky-depth pixels, invalid depth, and missing probes/resources.
- With Unified selected, one atmosphere integration is performed regardless of
  legacy haze volume count.
- Capture legacy/unified aggregate and subpass timings at equal output quality;
  treat visual comparison as user-owned.

### Slice 3 — Unify Global Fog And Authored Local Fog Under The Final Data Model

**Intent:** stop defining atmospheric density as a property of lights and make
the unified renderer authorable without depending on the migration adapter.

Required implementation:

- Add/normalize/serialize the final map-level anisotropy, volumetric maximum
  distance, and `volumetricQuality` fields. During migration only, read
  `localVolumeQuality` as an alias when the new field is absent; always save the
  new name.
- Add per-light `volumetricScatteringIntensity` while retaining old haze fields
  temporarily for Legacy comparison. New Unified code must ignore per-light
  haze density/shape fields.
- Make global fog and compiled authoring-graph local fog volumes the only
  Unified density inputs. All relevant point/spot lights illuminate that
  density using the temporary fixed light array.
- Apply analytic distance fog only to the path beyond the finite volumetric
  maximum when Unified is active. Off/resource failure restores the original
  analytic path from its configured start distance.
- Update Preview Settings -> Fog and the light inspector with final controls.
  Keep the old Haze UI visible only while Legacy comparison mode is selected,
  clearly labeled temporary.
- Route local-volume mutations through authoring graph edit/derivation/cache
  invalidation and global/light edits through existing document/light services.

Tests:

- New defaults, normalization limits, default omission, full round-trip, and
  migration-only quality alias behavior using generated JSON.
- Global-only, local-only, overlapping global/local, quality Off, and analytic
  far-field handoff calculations.
- Per-light zero/one/high volumetric intensity and stable light ID behavior.
- Inspector layout and editing-service refresh/invalidation coverage.
- Explicit lightmap source-hash tests prove all new volumetric fields are
  excluded.

### Slice 4 — Add The OpenGL 3.3 Froxel Atlas And Clustered Light Lists

**Intent:** replace the prototype's fixed global light loop with the scalable
final rendering architecture.

Required implementation:

- Implement quality sizing, logarithmic depth slicing, near-square 2D atlas
  packing, and GL limit fallback exactly as specified above.
- Add renderer-owned medium and lighting RGBA16F atlases, the low-resolution
  integrated atmosphere target, light/volume data textures, and cluster-list
  textures. Allocate only on explicit lifecycle/size/quality changes.
- Build a volumetric light source list from all relevant full renderer sources,
  not `SectorBillboardDynamicLightContext`. Preserve runtime light animation,
  stable IDs, portal/frustum eligibility, and existing light semantics.
- Implement deterministic 254-view and 16-per-cluster light selection using
  the fixed 4/6/8 coarse depth bands. Implement the same view/cluster scheme
  for compiled local volumes. Pre-size staging buffers when dimensions change
  and warn/diagnose overflow without allocating.
- Replace the prototype march with medium injection, light injection, and
  front-to-back column integration fragment passes.
- Make Unified the development default; keep Legacy only behind the temporary
  diagnostics toggle for comparison.

Tests/acceptance:

- Atlas mapping round-trips every boundary `(x,y,z)` for all presets/aspects;
  no slice overlap or out-of-range texel access.
- Depth slice endpoints are monotonic, cover the exact configured interval, and
  remain finite for extreme near/far inputs.
- Projected light/volume cluster bounds, behind-camera rejection, stable
  importance ordering, depth-band intersection, 254-view overflow, 16-cluster
  overflow, sentinel encoding, and staging-buffer reuse.
- Shader/resource failure drops to analytic fog and produces one diagnostic.
- A view containing more than eight spatially separated haze-producing lights
  retains every visible non-overlapping light without a whole-view eight-light
  cutoff.

### Slice 5 — Add Reconstruction, Temporal Stability, And Supported Volumetric Shadows

**Intent:** raise the scalable renderer from functional to production-quality
within current engine capabilities.

Required implementation:

- Add bilateral depth-aware reconstruction for every quality and temporal
  reprojection for Medium/High with jitter, previous matrices/depth, history
  validity, neighborhood clamping, and responsive blending.
- Implement every history invalidation trigger in the contract and expose the
  current reset reason in diagnostics.
- Apply existing dynamic spotlight shadows only for the two current shadow-map
  owners. Reuse their matrices, bias, strength, and softness without adding new
  shadow maps or point-light cubemaps.
- Tune fixed internal jitter/history weights per quality; do not serialize
  low-level temporal controls or expose a panel of engine knobs.
- Add a freeze-history and show-froxels/history-weight debug view in the
  Atmosphere diagnostics. These are nonserialized editor diagnostics and must
  be removed or retained as debug-only—not map settings—in Slice 8.
- Ensure camera jitter/history never feeds collision, sector lookup, physics,
  picking, lightmap sampling positions, or gameplay camera state.

Tests/acceptance:

- Reprojection matrix math, offscreen rejection, depth disocclusion,
  neighborhood clamp, finite-color sanitization, and every reset trigger.
- Shadowed/unshadowed dynamic spotlight selection follows existing two-slot
  ownership and `castsShadow`; point/static behavior remains documented.
- Repeated steady frames do not allocate CPU/GPU storage or synchronously wait
  for timer queries.
- User checklist covers slow walk, sprint, rotation, doorway disocclusion,
  animated/flickering lights, thin foreground geometry, and camera teleport for
  ghosting, halos, flicker, and banding.

### Slice 6 — Migrate And Tune The Hub Maps, Then Pass Visual/Performance Gates

**Intent:** validate the replacement on the only tracked content using haze
before deleting the old path.

Required implementation:

- Inventory the 11 hub and 5 hub2 haze blocks by light type/ID and capture their
  artistic intent before editing. Do not assume every serialized block is
  enabled or needs a one-to-one replacement.
- Tune shared global fog, existing authored local fog volumes, and per-light
  volumetric scattering intensity for `hub` first. Use local fog only where
  spatial density is actually desired; do not recreate one density volume per
  light by habit.
- Apply the corresponding intentional migration to `hub2` or explicitly remove
  stale haze-only data if it is a derivative map. Remove haze blocks from both
  tracked JSON files once their Unified appearance is accepted.
- Capture 300-frame post-warmup Legacy and Unified Low/Medium/High measurements
  at identical camera transforms, render scale, resolution, light state, and
  shadow settings. Record median/p95 and view/cluster overflow counts in this
  plan.
- Meet the quantitative retirement gates in Goal And Acceptance Criteria. If a
  gate fails, optimize the Unified passes in this slice; do not weaken the gate
  or retire Legacy without updating the plan and recording the user's decision.
- Obtain user visual acceptance for lit air, darkness, shafts, transitions,
  temporal behavior, and local fog. Record reported results without claiming
  agent-performed GUI verification.

Automated verification:

- Serialization tests use generated fixtures, not hub JSON contents.
- Add a repository scan test or validation command proving tracked map JSON no
  longer contains a `haze` key after migration.
- Run the full project checks. Record the exact benchmark environment and raw
  measurements in the execution log.

### Slice 7 — Retire The Legacy Haze And Local-Fog Renderers Completely

**Intent:** enforce the single-system end state after visual and performance
acceptance.

Required implementation:

- Delete `SectorLightHazeRenderer`, `SectorLocalFogRenderer`, their shaders,
  renderer members, targets, diagnostics, caches, pass sequencing, and build
  references. Local fog remains authored data consumed only by
  `SectorVolumetricAtmosphereRenderer`.
- Delete `SectorLightHazeSettings`, haze normalization/default helpers, haze
  JSON read/write code, old tests, old inspector widgets/state, and
  `LegacyHazeComparisonAdapter`.
- Collapse `SectorLightAtmosphereSettings` to final volumetric intensity plus
  dust. Remove temporary old/new quality aliases and require/save only
  `volumetricQuality` now that tracked maps are migrated.
- Delete Legacy/Unified backend selection and all Legacy branches. Unified is
  the only continuous-medium runtime path; missing resources fall back only to
  analytic distance fog, never to old haze.
- Rename haze-specific static-lighting helpers and diagnostics that are still
  legitimately used by local volumetric media; delete unused helpers rather
  than preserving misleading names.
- Audit source/build/docs/tests with repository-wide searches for
  `SectorLightHaze`, `hazeParams`, `MaxHazeVolumes`, serialized `"haze"`,
  `localVolumeQuality`, and the temporary comparison toggle. Remaining uses
  must be execution-log/history prose only.

Tests/acceptance:

- Clean build proves no deleted legacy symbols or files remain referenced.
- Final JSON schema round-trips global/local volumetrics, per-light intensity,
  and dust, and rejects removed haze data with a field-specific error.
- Maps with volumetric quality Off, no fog, no lights, dust-only lights, and
  resource failure remain renderable and deterministic.
- Lightmap hash, topology cache, collision/sector lookup/physics, dust, bloom,
  sky, and analytic fog regression coverage remains green.

### Slice 8 — Harden Lifecycle, Diagnostics, Documentation, And Acceptance Coverage

**Intent:** close integration gaps after the legacy path is gone; do not add a
second renderer or new product scope.

Required implementation:

- Audit preview enter/exit/rebuild, gameplay scene load/unload, resize,
  quality-cap change, render-scale change, shader failure, GL resource failure,
  map reload, probe/light rebuild, and shutdown for exact-once creation/release
  and valid history reset.
- Verify all CPU vectors/staging buffers reserve during initialization or size
  changes. Add warnings for unexpected steady-frame growth consistent with
  engine allocation rules.
- Verify no steady update/render path performs map validation, topology loop
  extraction, triangulation, file I/O, asset requests, shader compilation,
  render-target allocation, or synchronous GPU query waits.
- Finalize concise diagnostics and remove migration-only/freeze tooling that is
  not useful as permanent debug support. Keep atlas/light/volume/overflow/
  timing/history/fallback information.
- Update `docs/sector_editor.md` and relevant lighting design documentation with
  final authoring, quality, GL 3.3 architecture, light limits, shadow behavior,
  analytic-fog relationship, and known artifacts.
- Update this plan state/log and run all project checks. Do not claim manual GUI
  verification unless the user reports it.

Final manual checklist for the user:

1. Compare Off/Low/Medium/High in sparse and dense hub views.
2. Walk from lit rooms into total darkness and confirm fog does not self-glow.
3. Inspect many simultaneous point/spot lights and overlapping shafts; confirm
   lights do not disappear because eight others are closer to the camera.
4. Inspect local fog alone and overlapping global/local fog.
5. Move through doorways and past thin foreground objects looking for halos,
   leaks, ghost trails, banding, or history flashes.
6. Exercise flickering/moving dynamic lights and both shadowed spotlight slots.
7. Change fog settings and quality, resize/rebuild preview, reload the map, and
   enter/exit gameplay preview while watching for stale history or leaks.
8. Confirm dust remains visible and independent, bloom reacts naturally, and
   sky rendering remains unchanged.
9. Save/reload hub and confirm no old haze controls or serialized haze fields
   return.

Final report requirements:

- State that one unified continuous volumetric renderer remains and enumerate
  the deleted legacy renderer/schema/UI paths.
- Report final quality dimensions, view/cluster budgets, overflow behavior,
  and hub Legacy-vs-Unified benchmark measurements.
- State that local fog authoring mutations continue through authoring graph
  document-edited/cache invalidation and whether any other cache paths changed.
- State that all volumetric settings remain excluded from the lightmap source
  hash and that no lightmap artifact/bake behavior changed.
- State that collision, sector lookup, physics, and gameplay camera behavior
  did not change; temporal matrices/jitter are visual-only.
- List automated commands actually run and keep manual verification identified
  as user-owned unless the user reported results.

## Cross-Slice Test Matrix

| Area | Required coverage |
|---|---|
| Math | Density combination, Beer-Lambert integration, phase function, depth clipping, premultiplied HDR composition, finite sanitization. |
| Quality/atlas | Aspect sizing, preset caps, atlas packing, GL max-size fallback, logarithmic Z bounds, memory diagnostics. |
| Cluster lists | Projected/depth bounds, behind-camera rejection, deterministic importance/ties, 254-view and 16-cluster overflow, sentinel encoding, no steady allocation. |
| Serialization | New defaults/omission/round-trip/ranges, temporary alias only in migration slices, final haze rejection, dust preservation. |
| Lighting | Static/dynamic point/spot falloff, cone response, runtime intensity, per-light volumetric intensity, two dynamic spotlight shadow slots. |
| Media | Global only, local only, overlap, noise/flow, camera inside, empty/dark medium, local opacity, analytic far-field handoff. |
| Reconstruction | Bilateral edges, sky depth, disocclusion, reprojection, neighborhood clamp, jitter, all history reset causes. |
| Lifecycle | Init/reset/shutdown, resize, preview rebuild, scene reload, resource/shader failure, quality changes, no leaks/double unload. |
| Editor/cache | Fog/light controls, layout, service routing, preview refresh, local fog authoring dirty/derivation/2D-cache invalidation. |
| Lightmaps | All visual atmosphere/light controls excluded; existing geometry/light/directional/sky hash policy unchanged. |
| Regression | Dust, bloom, sky, surface fog, HDR order, collision, sector lookup, physics, picking, portal visibility. |
| Retirement | No legacy classes/shaders/schema/UI/toggle/build references; tracked maps contain no `haze` key. |

Tests must use generated maps, temporary JSON, pure CPU records, or dedicated
immutable fixtures outside `assets/levels` and `assets/sector_demo`. Benchmark
and visual acceptance may use the editable hub maps but must not become C++ test
fixtures.

## Deferred Backlog

- Baked volumetric shadow/irradiance fields for static lights.
- Directional sun/moon scattering, cascaded volumetric shadows, and outdoor
  cloud/atmosphere simulation.
- Dynamic point-light cubemap shadows or more than the existing two dynamic
  spotlight shadow maps.
- Compute-shader/SSBO backend, OpenGL version upgrade, or Vulkan/other graphics
  API work.
- Arbitrary box/cone/SDF density-volume authoring beyond the existing local fog
  ellipsoids.
- Multiple scattering, colored extinction separate from scattering albedo, or
  physically calibrated photometric units.
- Volumetric participation by particles, translucent surfaces, middle-texture
  alpha, decals, or viewmodels.
- A generic render graph, generic clustered-deferred surface renderer, or broad
  `SectorEditor.cpp` refactor.

## Execution Log

Append one entry per attempted slice in this format:

```text
### YYYY-MM-DD — Slice N — Completed|Partial|Blocked

- Summary:
- Decisions/deviations folded back into plan:
- Files/modules materially affected:
- Automated verification:
- GPU measurements: Not captured (user-owned GUI run), or exact setup/results.
- Manual verification: Not performed (user-owned), unless reported otherwise.
- Cache invalidation behavior:
- Lightmap source-hash behavior:
- Collision/sector lookup/physics behavior:
- Legacy coexistence/removal state:
- Remaining follow-up within this plan:
```

### 2026-08-17 — Slice 1 — Partial

- Summary: Added fixed volumetric quality/grid/atlas/cluster contracts and final light/list budgets; centralized the legacy fog/haze quality constants; added legacy atmosphere target/count/resource diagnostics; added four-frame delayed, nonblocking OpenGL timestamp pairs for total atmosphere, local fog, haze accumulation/composite, and dust; and added an editor 3D Atmo diagnostics tab with deterministic 60-warmup/300-sample capture, stable-view rejection, logging, and copy-ready reports.
- Decisions/deviations folded back into plan: The existing Main aggregate `GL_TIME_ELAPSED` profiler remains unchanged. Renderer-owned `GL_TIMESTAMP` pairs provide nested-safe subpass and capture measurements independently of the F9 overlay. Atlas packing minimizes the maximum atlas dimension, then texel area and width/height difference. Capture advances only on successfully issued query frames, never overwrites an unresolved ring slot, and records skipped query frames.
- Files/modules materially affected: Focused atmosphere contracts/diagnostics/profiler modules under `sources/sector_demo/renderer`; `SectorMeshRenderer` legacy atmosphere sequencing instrumentation; the existing sector-editor preview overlay/layout; focused diagnostic, layout, and render-order tests; CMake test registration.
- Automated verification: Passed `cmake --build cmake-build-debug -j2`; passed `ctest --test-dir cmake-build-debug --output-on-failure` (27/27); passed `git diff --check`; `git diff --stat` and `git status --short` reviewed.
- GPU measurements: Not captured (user-owned GUI run). Slice remains Partial until Low/Medium/High captures are returned for one sparse hub view and one dense hub haze view.
- Manual verification: Not performed (user-owned).
- Cache invalidation behavior: No topology or visible cached 2D editor-state mutations were added; topology render-cache invalidation behavior is unchanged.
- Lightmap source-hash behavior: No map/light/fog schema or lightmap inputs changed; `ComputeSectorLightmapSourceHash()` behavior and bake artifacts are unchanged.
- Collision/sector lookup/physics behavior: Unchanged. Capture reads the visual renderer pose/FOV only and does not feed camera, collision, sector lookup, or physics.
- Legacy coexistence/removal state: `SectorLocalFogRenderer`, `SectorLightHazeRenderer`, and `SectorLightDustRenderer` remain the active legacy path in their existing order; no Unified renderer, adapter, backend toggle, or map migration was added.
- Remaining follow-up within this plan: User runs and returns the six hub capture reports plus hardware/settings metadata. On receipt, record the measurements, append a Slice 1 completion entry, and mark Slice 1 Completed without beginning Slice 2.

### 2026-08-17 — Slice 1 — Completed

- Summary: Closed the measurement portion of Slice 1 with user-run legacy Low/Medium/High captures at one sparse hub view and one dense hub haze view. Each capture contains 300 post-warmup samples and reported zero skipped query frames. Slice 1 is now complete; Slice 2 has not started.
- Decisions/deviations folded back into plan: The dense High legacy reference is the retirement-gate baseline for the same camera/settings/hardware: Unified High must be at most `4.789 ms` median (70% of `6.842 ms`) and `5.139 ms` p95 (75% of `6.852 ms`). Dense Unified Medium must not exceed the legacy `2.115 ms` median at the same view; the sparse Medium reference is `0.900 ms`. Comparisons must retain the captured target, camera transform, FOV, shadow state, and application graphics settings.
- Files/modules materially affected: Documentation only in this follow-up (`docs/volumetric_atmosphere_plan.md`).
- Automated verification: C++ tests were not rerun because this follow-up changes documentation only. `git diff --check`, `git diff --stat`, and `git status --short` were run after the update.
- GPU measurements: User-run on EndeavourOS, Linux `7.1.5-arch1-2`, NVIDIA GA106 GeForce RTX 3060 Lite Hash Rate using the loaded proprietary-compatible NVIDIA kernel driver/module `610.43.03` (`nvidia-open-dkms`/`nvidia-utils` package `610.43.03-5`). `nvidia-smi` was unavailable, but `lspci`, `modinfo`, and installed package metadata identified the active GPU/driver. Captured world target was `2880x1620`; repository application settings were render scale `1.5`, shadow quality High, VSync enabled, FXAA enabled, and horizontal FOV 95 degrees. Captures report vertical FOV `63.088` degrees.

  Sparse view: camera position `(-13.316, 1.348, 12.372)`, yaw/pitch/roll `(-0.270, -0.045, 0.000)` radians; fog `3/3`, haze `1/1`, dust `1/1` at every quality.

  ```text
  Low total:       min 0.732 ms | median 0.734 ms | p95 0.736 ms | max 0.959 ms
    local fog:     min 0.355 ms | median 0.357 ms | p95 0.358 ms | max 0.360 ms
    haze:          min 0.355 ms | median 0.356 ms | p95 0.357 ms | max 0.582 ms
    dust:          min 0.017 ms | median 0.018 ms | p95 0.019 ms | max 0.019 ms
  Medium total:    min 0.896 ms | median 0.900 ms | p95 0.905 ms | max 0.913 ms
    local fog:     min 0.432 ms | median 0.434 ms | p95 0.435 ms | max 0.438 ms
    haze:          min 0.441 ms | median 0.445 ms | p95 0.450 ms | max 0.458 ms
    dust:          min 0.017 ms | median 0.018 ms | p95 0.019 ms | max 0.020 ms
  High total:      min 1.641 ms | median 1.666 ms | p95 1.683 ms | max 1.702 ms
    local fog:     min 0.972 ms | median 0.991 ms | p95 1.006 ms | max 1.013 ms
    haze:          min 0.641 ms | median 0.651 ms | p95 0.666 ms | max 0.689 ms
    dust:          min 0.018 ms | median 0.019 ms | p95 0.019 ms | max 0.020 ms
  ```

  Dense view: camera position `(-6.433, 1.348, 12.644)`, yaw/pitch/roll `(-0.690, 0.024, 0.000)` radians; fog `3/3`, haze eligible `8` with active caps `2/4/8`, and dust `6/6`.

  ```text
  Low total:       min 0.820 ms | median 0.822 ms | p95 0.827 ms | max 1.153 ms
    local fog:     min 0.364 ms | median 0.366 ms | p95 0.368 ms | max 0.695 ms
    haze:          min 0.433 ms | median 0.434 ms | p95 0.436 ms | max 0.437 ms
    dust:          min 0.018 ms | median 0.019 ms | p95 0.022 ms | max 0.023 ms
  Medium total:    min 2.040 ms | median 2.115 ms | p95 2.122 ms | max 2.151 ms
    local fog:     min 0.787 ms | median 0.861 ms | p95 0.868 ms | max 0.897 ms
    haze:          min 1.225 ms | median 1.230 ms | p95 1.233 ms | max 1.234 ms
    dust:          min 0.018 ms | median 0.020 ms | p95 0.023 ms | max 0.023 ms
  High total:      min 6.823 ms | median 6.842 ms | p95 6.852 ms | max 6.865 ms
    local fog:     min 0.634 ms | median 0.653 ms | p95 0.662 ms | max 0.676 ms
    haze:          min 6.160 ms | median 6.164 ms | p95 6.168 ms | max 6.171 ms
    dust:          min 0.022 ms | median 0.022 ms | p95 0.023 ms | max 0.023 ms
  ```

  Findings: Dense/sparse total median ratios are `1.12x` Low, `2.35x` Medium, and `4.11x` High. Dense High haze alone consumes `6.164 ms`, approximately 90% of the `6.842 ms` total median, while dust stays near `0.02 ms`. The active haze cap scales `2 -> 4 -> 8`, and the dense High result confirms the expected legacy volume/march/light scaling problem. The fixed capture/query path was stable in all six runs (`skipped_queries=0`).
- Manual verification: The user performed the six requested GUI timing captures. No separate visual-parity or playability acceptance was reported or claimed.
- Cache invalidation behavior: Unchanged; this completion follow-up is documentation-only and adds no topology mutation.
- Lightmap source-hash behavior: Unchanged; no lightmap data, settings, source-hash logic, or bake artifacts changed.
- Collision/sector lookup/physics behavior: Unchanged.
- Legacy coexistence/removal state: The legacy local-fog, haze, and dust renderers remain active for comparison as required through Slice 6. No Unified renderer work or legacy removal was started.
- Remaining follow-up within this plan: Slice 2 may use these exact sparse/dense transforms and legacy measurements for equal-setting Legacy/Unified comparison when explicitly requested.

### 2026-08-17 — Slice 2 — Partial

- Summary: Added the temporary `SectorVolumetricAtmosphereRenderer` GLSL 330 screen-space backend, the explicitly named `LegacyHazeComparisonAdapter`, one shared density/light/integration march, depth-aware HDR composition, analytic-fog tail handoff, and a nonserialized Legacy/Unified selector in the existing Atmo diagnostics tab. Unified uses the final quality contract's bounded XY target and Z count as prototype march steps while retaining the legacy comparison caps for local fog, haze, eight selected dynamic lights, and two spotlight shadow slots. Legacy remains the default and its local-fog/haze sequence remains available.
- Decisions/deviations folded back into plan: Slice 2 uses the nonserialized final defaults of 32 world units maximum distance and 0.20 anisotropy until Slice 3 adds authored fields. A narrow pre-scene prepare step validates/allocates Unified resources before surface fog upload, so a failed Unified resource leaves the complete analytic fog path active instead of silently running Legacy. Cached local/haze probe lighting is extinction-weighted; selected dynamic lights are evaluated once after density combination. Temporal history, froxels, clustering, broader light lists, schema changes, and map migration were not started.
- Files/modules materially affected: Focused volumetric renderer/math/comparison-adapter modules under `sources/sector_demo/renderer`; `SectorMeshRenderer` atmosphere preparation/sequencing/diagnostics; existing sector-editor Atmo diagnostics controls; focused math, diagnostics, shader-policy, and layout tests.
- Automated verification: Passed `cmake --build cmake-build-debug -j2`; passed `ctest --test-dir cmake-build-debug --output-on-failure` (28/28); passed `git diff --check`; `git diff --stat` and `git status --short` reviewed.
- GPU measurements: Not captured (user-owned GUI run). Slice remains Partial pending four dense-view captures: Legacy/Unified Medium and Legacy/Unified High.
- Manual verification: Not performed (user-owned). One dense-view Legacy/Unified High visual comparison remains.
- Cache invalidation behavior: No topology or visible cached 2D editor-state mutation was added. The backend selector and renderer resources are nonserialized preview/debug state; topology render-cache invalidation is unchanged.
- Lightmap source-hash behavior: Unchanged. No map/light/fog schema, bake input, source-hash rule, or lightmap artifact changed; all new prototype state is visual-only and nonserialized.
- Collision/sector lookup/physics behavior: Unchanged. Unified reconstruction reads the visual render camera and scene depth only; it does not feed camera motion, collision, sector lookup, portal traversal, or physics.
- Legacy coexistence/removal state: Legacy remains the initial backend and the gameplay default. `SectorLocalFogRenderer`, `SectorLightHazeRenderer`, and their schema/UI remain intact for comparison; exactly one continuous backend executes per frame and dust remains afterward.
- Remaining follow-up within this plan: At the recorded dense hub transform, capture Legacy Medium, Unified Medium, Legacy High, and Unified High without moving or changing graphics settings, then report the four capture blocks and whether one High toggle preserves intended fog/haze without glowing darkness or a severe foreground halo. Do not begin Slice 3 as part of that follow-up.

### 2026-08-17 — Slice 2 — Completed

- Summary: Closed Slice 2 with user-run same-view Legacy/Unified Medium and High captures plus visual A/B acceptance. The central cost hypothesis is confirmed: one low-resolution shared-medium integration substantially outperformed the legacy separate local-fog and per-volume haze marches while retaining all three local-fog volumes, all eight eligible haze volumes at High, and the six dust emitters.
- Decisions/deviations folded back into plan: The comparison camera differs from the older Slice 1 dense baseline because exact WASD recreation was impractical, but all four Slice 2 captures use the identical transform, FOV, target, and media counts, so the internal Legacy/Unified ratios are valid. Unified is visually a little less intense than Legacy, resembling a slight density reduction; the user explicitly considered this acceptable rather than a bug. No tuning or Slice 3 data-model work was pulled into this completion follow-up.
- Files/modules materially affected: Documentation only in this completion follow-up (`docs/volumetric_atmosphere_plan.md`).
- Automated verification: C++ tests were not rerun because this follow-up changes documentation only. `git diff --check`, `git diff --stat`, and `git status --short` were run after the update. The Slice 2 implementation previously passed the debug build and all 28 CTest tests.
- GPU measurements: User-run at target `2880x1620`, vertical FOV `63.088` degrees, camera position `(-7.427, 1.650, 12.048)`, yaw/pitch/roll `(-0.453, -0.102, 0.000)`, with zero skipped query frames in all four captures. Medium retained fog `3/3`, haze `8/4`, and dust `6/6`; High retained fog `3/3`, haze `8/8`, and dust `6/6`. Unified evaluated two selected dynamic lights.

  ```text
  Medium Legacy total:  median 1.765 ms | p95 1.769 ms
  Medium Unified total: median 0.918 ms | p95 0.925 ms
    Unified/Legacy:     52.01% median | 52.29% p95
    Reduction:          47.99% median | 47.71% p95

  High Legacy total:    median 7.030 ms | p95 7.053 ms
  High Unified total:   median 1.383 ms | p95 1.393 ms
    Unified/Legacy:     19.67% median | 19.75% p95
    Reduction:          80.33% median | 80.25% p95
  ```

  Unified subpass medians were `0.895 ms` Medium and `1.360 ms` High. Legacy haze subpass medians were `1.293 ms` Medium and `6.393 ms` High. High clears the plan's 70%-median and 75%-p95 legacy ratios with substantial margin, and Unified Medium is not slower than Legacy Medium.
- Manual verification: User confirmed every haze remained visible, with Unified appearing slightly less intense than Legacy. No darkness glow and no severe foreground halos were observed.
- Cache invalidation behavior: Unchanged; this completion follow-up is documentation-only. Slice 2 added no topology or visible cached 2D editor-state mutation.
- Lightmap source-hash behavior: Unchanged. Slice 2 added no serialized inputs or lightmap-affecting data; all prototype settings and backend selection remain visual-only and excluded from `ComputeSectorLightmapSourceHash()`.
- Collision/sector lookup/physics behavior: Unchanged.
- Legacy coexistence/removal state: Legacy remains available and is still the initial backend as required for the migration period. Slice 2 is complete; no legacy renderer/schema/UI removal occurred.
- Remaining follow-up within this plan: Slice 3 may now begin when explicitly requested. Preserve the accepted slight intensity difference as a tuning reference; do not start Slice 3 or retune Slice 2 as part of this documentation-only closure.

### 2026-08-17 — Slice 3 — Completed

- Summary: Added the final map-level anisotropy, volumetric maximum distance, and `volumetricQuality` data with normalization and canonical serialization; retained a read-only `localVolumeQuality` migration alias. Added normalized per-light `volumetricScatteringIntensity`. Unified now takes density only from global fog and compiled authoring-graph local fog, and a deterministic temporary eight-record array supplies direct static/dynamic point/spot illumination without reading legacy haze density or shape. The analytic fog path begins at the authored volumetric maximum only when Unified is ready; quality Off, inactive Unified, and resource failure retain the full configured analytic path. Added the final Preview Fog controls, a light scattering control, Legacy-only temporary haze controls, and eligible/active Unified light diagnostics.
- Decisions/deviations folded back into plan: The Slice 3 fixed-array bridge selects visible lights by distance with stable kind/ID tie-breaking and reports eligible/active counts; the 254-view and clustered importance scheme remains exclusively Slice 4. Static lights are direct, unshadowed volumetric emitters. Dynamic flicker and the existing selected spotlight shadow slots are preserved. Unified no longer samples sector ambient/object probes, preventing unlit fog from becoming emissive and avoiding direct-static double counting. Legacy haze/schema/renderers remain intact solely for comparison; no tracked map or hub-content migration was performed.
- Files/modules materially affected: Topology fog/light data and JSON serialization; focused atmosphere source selection, Unified renderer, contracts, and diagnostics modules under `sources/sector_demo/renderer`; existing Preview Settings and light-inspector/service integration; focused serialization, volumetric math/policy, lightmap hash, editor layout/service, and selection tests; this living plan.
- Automated verification: Passed `cmake --build cmake-build-debug -j2`; passed `ctest --test-dir cmake-build-debug --output-on-failure` (28/28); passed `git diff --check`; `git diff --stat` and `git status --short` reviewed.
- GPU measurements: Not captured (user-owned GUI run); Slice 3 changes authoring/data semantics and no new performance gate was required.
- Manual verification: Not performed (user-owned).
- Cache invalidation behavior: Authored local fog continues through the authoring graph edit/derivation/cache path unchanged. Per-light scattering edits use `SectorEditorLightEditingService`, which marks the document edited, invalidates the 2D topology render cache, and requests preview source refresh through the inspector path. Global fog edits use the existing Preview Settings document mutation path and `MarkTopologyDocumentEdited()`; its existing conservative 2D cache invalidation remains. No direct local-volume or topology mutation path was added.
- Lightmap source-hash behavior: Anisotropy, volumetric maximum distance, volumetric quality, and per-light volumetric scattering intensity are visual-only and remain excluded from `ComputeSectorLightmapSourceHash()`. Explicit static point/spot, dynamic point/spot, and fog hash tests pass; baked lighting inputs/results are unchanged.
- Collision/sector lookup/physics behavior: Unchanged. Atmosphere remains visual-only; no collision, sector lookup, camera, portal topology, picking, or physics behavior changed.
- Legacy coexistence/removal state: Legacy local fog and haze renderers, haze schema, and temporary comparison controls remain usable as required through Slice 6. Unified does not invoke the legacy haze adapter or consume haze density/shape. The old haze inspector controls appear only when the Legacy comparison backend is selected and are labeled temporary.
- Remaining follow-up within this plan: Slice 4 may add the OpenGL 3.3 froxel atlas, 254-view light/volume staging, and deterministic clustered lists when explicitly requested. Do not migrate hub maps or remove Legacy as part of this completed slice.
