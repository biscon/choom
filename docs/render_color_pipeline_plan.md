# Render Color Pipeline Plan

Branch: `feature/colorhandling`

Policy: the mixed renderer will be replaced by one linear HDR scene pipeline.
There is no permanent legacy renderer, compatibility color semantic, or
player-visible old/new pipeline switch.

## Color Contract

### Scene color encoded as sRGB

Sector albedo/diffuse textures, scene decals, sky and emissive artwork, and
glTF base-color/emissive textures contain sRGB-authored color. They are decoded
to linear before scene lighting once slice 2 activates the new pipeline.

### Linear non-color data

Normal, metallic, roughness, occlusion/AO, scalar material, shadow/depth, noise,
mask, baked-lightmap, and illumination-probe data is linear numeric data and
must never receive sRGB decoding. Baked lightmap RGB is linear HDR radiance and
its alpha is bounded linear AO. Slice 3 stores it as explicit little-endian
RGBA binary16 and stores probes as explicit little-endian float32.

### Display-referred sRGB

UI textures, fonts, HUD/crosshair colors, editor/menu artwork, and byte colors
drawn after scene presentation remain display-referred. They do not enter the
scene decode, lighting, or tone-map path. A PNG used for a scene texture and a
PNG shown by an editor picker can therefore require different request semantics.

### Authored scene color swatches

Actual authored scene-color swatches such as light, fog, haze, dust, decal
tint, sky-cap, clear, and muzzle-effect colors are decoded from sRGB to linear
scene values while alpha remains a linear opacity/value. Numeric scalar values
remain linear and unchanged even when they happen to be stored or transported
through raylib's `Color` type or vertex attributes. In particular, sector
ambient is numeric linear data and is not sRGB-decoded. Runtime rendering uses
this contract after slice 2; slice 3 applies the corresponding authored-light
conversion to baking.

### glTF conventions

glTF base-color and emissive textures use sRGB decode. Base-color, emissive,
and material factors retain glTF-defined numeric conventions. Metallic,
roughness, occlusion, and normal channels remain linear data. raylib-loaded
model textures bypass the generic texture asset API; the active model shader
therefore continues to decode base-color and emissive sampling roles explicitly.
glTF vertex colors remain linear factors and are never sRGB-decoded.

## Migration Tracker

| Slice | State | Intent |
|---|---|---|
| 1. Color/HDR foundations | Complete | Contract, exact transfer helpers, texture semantics, float-target factory, diagnostics, tests |
| 2. Linear HDR world and presentation | Complete | Replace world/viewmodel scene output with linear HDR and one global tone/output pass |
| 2.1. glTF/PBR lighting diagnosis and correction | Complete | Remove false achromatic indirect light, isolate contributions, and expose reusable PBR diagnostics |
| 3. HDR baked illumination | Complete | Preserve HDR radiance in lightmaps and illumination probes; update bake conventions and hashes deliberately |
| 4. Atmosphere, blending, muzzle and bloom | Complete | Promote effect intermediates/radiance to HDR and replace the LDR bloom workaround with scene-wide pre-tone-map bloom |
| 5. Audit, cleanup, controls and hardening | Pending | Remove obsolete mixed paths, validate platforms, add final controls and regression hardening |

Texture semantics are authoritative metadata and cache identity. Since slice 2,
generic `SceneSrgb` uploads use sRGB GPU storage and sampling while `LinearData`
and `DisplaySrgb` retain ordinary linear-format storage.

## Implemented HDR Bloom Contract

Slice 4 implements one scene-wide pre-tone-map bloom response. A max-channel
continuous soft-knee prefilter samples the visible supersampled HDR scene and
prefilters every full-resolution tap before quarter-resolution averaging, so
small bright cores can seed bloom before spatial reduction. Three separable
quarter-resolution `RGBA16F` blur targets retain color; their alpha is always
zero and never carries bloom energy. The result is added to scene RGB while
preserving scene alpha, before the existing supersample resolve and global
neutral presentation pass. HUD, crosshair, menus, ordinary UI, FPS text, and
depth-tested editor diagnostics are drawn after bloom and cannot seed it.

Threshold zero admits every positive finite scene value. A zero soft knee is a
stable hard threshold. Threshold is measured by maximum RGB channel in the
existing exposure-1 linear scene space, deliberately allowing saturated red,
green, or blue emission to seed bloom without a luminance penalty. Bloom
enable, threshold, knee, intensity, and radius are validated application
settings. Bloom sees depth-correct visible emissive decal pixels, per-fragment
glTF emission (including `KHR_materials_emissive_strength`), muzzle radiance,
dust, atmosphere, and all other bright scene radiance without source-specific
blur or whole-model bloom flags.

## Slice 1 Completion Record

Date: 2026-08-10

Status: complete.

Summary: added exact CPU and canonical GLSL sRGB transfer helpers; explicit
`SceneSrgb`, `LinearData`, and `DisplaySrgb` texture request semantics and cache
identity; a validated project render-target factory for `RGBA8_UNORM` and real
`RGBA16_FLOAT`; and one-shot context plus per-target runtime diagnostics. All
active world, viewmodel, editor, UI, fog, haze, dust, and bloom targets remain
`RGBA8_UNORM`. Texture semantics do not yet change raylib upload formats or
sampling behavior. glTF model-local decode, ACES, and sRGB encoding remain
active for the slice 2 atomic presentation conversion.

Verification:

- `cmake --build cmake-build-debug -j2`: passed.
- Focused `render_color_foundations` CTest: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: 20/20 passed.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed; changes are scoped to
  slice 2 implementation, tests, build wiring, and this living plan.
- Interactive runtime launch and screenshots: intentionally not performed for
  this slice; actual GPU state is emitted by the new startup/target diagnostics
  when runtime testing begins.

## Slice 2 Completion Record

Date: 2026-08-10

Status: complete.

Summary: the world and viewmodel targets are now required `RGBA16_FLOAT`
targets with no RGBA8 scene fallback. The 2880x1620 world is resolved to a
1920x1080 `RGBA16_FLOAT` target while still linear HDR. One required global
presentation shader then applies the fixed exposure-1.0 neutral max-channel
curve (`rgb / (1 + max(rgb))`), preserving channel ratios, followed by the
exact sRGB transfer into a 1920x1080 RGBA8 presentation target. FXAA operates
into a separate 1920x1080 RGBA8 output, after which only final platform
drawable scaling remains display-referred. No alternate operator, exposure
control, or legacy rendering mode was added.

Runtime scene textures requested as `SceneSrgb` now receive `GL_SRGB8` or
`GL_SRGB8_ALPHA8` storage, including the generated vertical environment
cubemap. raylib-loaded glTF base-color and emissive textures retain their
explicit shader decode because they bypass the asset manager. Scene swatches
for dynamic lights, fog, local fog, haze, dust, door/decal/billboard tints,
sky cap, clear color, and muzzle lighting/effects are decoded explicitly.
Sector ambient, probe-derived lighting, lightmaps, glTF material factors, and
glTF vertex colors remain linear and unchanged.

The existing bloom, local fog, haze, and dust effects remain active. Their
low-resolution RGBA8 intermediates represent bounded linear effect quantities:
they contain no local tone mapping or sRGB encoding, and their own radiance may
clip until slice 4. Full-scene copies and composite staging are `RGBA16_FLOAT`,
so the effects composite linearly and never force the whole scene through
RGBA8. Model-local ACES/sRGB output was removed. Depth-tested editor 3D
diagnostics render into the HDR world before presentation with decoded authored
overlay swatches; probe-derived marker colors remain linear. HUD, crosshair,
ordinary UI, 2D editor, and menus remain after scene presentation.

Decisions and deviations:

- Selected the fixed neutral max-channel curve instead of retaining the prior
  model-local ACES fit.
- Added an explicit output-resolution HDR resolve target; supersample reduction
  is no longer performed after sRGB encoding.
- Kept the range-limited effect accumulators in RGBA8 as allowed by the slice,
  but promoted every full-scene composite/copy target to RGBA16F.
- Chose pre-tone-map HDR editor 3D overlays, avoiding depth copies or resolved
  depth matching in this slice.
- Texture upload formats are selected by declared semantic only; use of
  raylib's `Color` type does not imply sRGB color semantics.
- No topology mutation/cache behavior, baked-lightmap or probe artifact format,
  lightmap source-hash input, collision, sector lookup, physics, or camera
  behavior changed.

Verification:

- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: 20/20 passed.
- `git diff --check`: passed.
- Interactive runtime launch and screenshots: intentionally not performed;
  visual validation is reserved for the user.

## Slice 2.1 Completion Record

Date: 2026-08-10

Status: complete. Slice 3 remains pending.

Root cause: the active PBR path created a generated environment cubemap even
when the map had no successfully loaded sky image. Its fallback was neutral
mid-gray, it was still reported to the shader as a valid environment, and
indoor models retained a minimum environment exposure. Rough dielectric glTF
materials therefore received a broad achromatic reflection. The shader also
added a synthetic `roughSpecularFloor` derived from `F0` and static lighting,
creating a second white/gray indirect-specular veil. This was especially
visible on the test character because its ordinary opaque material is
nonmetallic, fully rough, and primarily base-color textured. A separate issue
forced dynamic models and the viewmodel down the object-probe shader branch
even when the sampled probe was invalid; fallback values happened to mask the
color difference, but source validity was not inspectable. Finally, PBR scene
radiance was capped at 4.0 before the HDR target.

Corrections and final composition policy:

- A PBR environment is now eligible only when a real sky/environment image
  loaded and its cubemap upload succeeded. No-sky and failed-sky maps expose an
  inactive environment and contribute exactly zero. Rebuild/shutdown clears
  the handle and eligibility, preventing a previous map's sky from leaking
  across map switches. No neutral-gray fallback is generated. Real cubemaps
  use `SceneSrgb` hardware-sRGB sampling exactly once and receive no shader
  decode.
- Removed the synthetic rough-specular floor. Environment specular now comes
  only from an eligible reflection source. Material AO attenuates indirect
  diffuse and environment specular; it does not incorrectly attenuate direct
  point/spot lighting.
- Valid object probes replace sector ambient for dynamic world models and the
  viewmodel. Invalid probes select the sector-ambient fallback explicitly.
  Static models select their valid static-model lightmap when present,
  otherwise sector ambient. These sources are mutually exclusive rather than
  stacked.
- The selected probe, fallback ambient, or static-lightmap RGB is treated as
  incoming diffuse illumination, never final gray surface radiance. It is
  multiplied by the material base-color diffuse contribution, nonmetal factor,
  material AO, and the existing renderer normalization before accumulation.
  The same rule applies to static and dynamic world models.
- Direct point/spot diffuse and specular use the same selected light colors,
  intensity, range, attenuation, cones, and shadows already shared with sector
  rendering. Direct diffuse and direct specular are now separate diagnostic
  terms, without changing those input conventions.
- Removed the PBR finite upper radiance clamp before the HDR target. The CPU
  draw-state builder rejects non-finite exposure, brightness, and scale inputs;
  authored dynamic lights already validate finite positive ranges and
  intensities before upload. The shader retains only a physically meaningful
  nonnegative radiance bound and adds no replacement high ceiling.
- raylib-loaded glTF material textures now record their actual OpenGL internal
  formats once during main-thread model finalization. Base-color and emissive
  use one explicit shader sRGB decode when stored in raylib's normal linear
  formats, or rely on hardware decode without a second shader decode if an
  sRGB internal format is encountered. Metallic, roughness, normal, occlusion,
  masks, and scalar data remain raw linear. glTF metallic B and roughness G
  continue through raylib's split R8 texture maps; numeric factors and vertex
  colors retain glTF linear-factor conventions.

Lasting defaults and diagnostics:

- Runtime world-model indirect-diffuse and environment-specular scales are
  independent, default to `1.0`, clamp to `[0, 1]`, and are not coupled to
  sector ambient. They are runtime debug/tuning state rather than serialized
  map settings. The corrected sources no longer require a compensating default
  reduction.
- The modal 3D debug overlay has a focused `PBR` tab with Full PBR, Base Color,
  Direct Diffuse, Direct Specular, Probe/Indirect Diffuse, Environment
  Specular, Emissive, Material AO, Metallic/Roughness, and Shading Normal
  outputs. All modes reuse the active shader and remain in the shared HDR
  presentation path. Scalar and normal modes are explicitly labeled bounded
  scene-linear visualizations.
- The same tab provides `0`, `0.25`, `0.5`, and `1.0` world contribution
  presets and a reset. It reports selected world-model path, indirect source,
  probe selection, environment eligibility/exposure, material factors,
  texture-role presence, actual internal formats and transfer route, plus the
  current viewmodel path and overrides.
- World draw-state construction always forces brightness to 1 and rejects
  viewmodel material overrides. Viewmodel draw states force both world tuning
  scales to 1 and retain only their existing data-driven brightness/material
  overrides. No override state is shared or persisted between those paths.

Remaining boundary: the current baked static-model lightmaps and illumination
probes are still range-limited RGBA8 artifacts. Their diagnostic routing is now
correct, but their brightness/range is not authoritative until Slice 3 promotes
the artifact formats. No correction was fitted to those old artifacts. The
unrelated baked black-patch/normal-map shadow issue was not changed.

This historical Slice 2.1 boundary was resolved by Slice 3 without changing
the accepted PBR routing or composition.

Materially changed systems: glTF model material metadata/finalization, the PBR
environment lifecycle, static/dynamic/viewmodel PBR shader composition and CPU
draw-state routing, `SectorMeshRenderer` PBR state exposure, selected-object
diagnostics, the editor modal 3D PBR UI, focused policy tests, CMake test wiring,
and this living plan. No topology mutation, 2D topology cache, collision,
sector lookup, physics, camera behavior, bake format, or lightmap source-hash
input changed. Runtime-only PBR diagnostics and scales remain excluded from the
lightmap hash.

Verification:

- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: 21/21 passed,
  including the new focused `pbr_lighting_policy` test.
- `git diff --check`: passed.
- Interactive runtime launch and screenshots: intentionally not performed;
  visual validation is reserved for the user.

## Slice 3 Completion Record

Date: 2026-08-10

Status: complete. Its documented effect-buffer limitation was subsequently
resolved by Slice 4 without reopening Slice 3.

Artifact and color contract:

- Sector/world and static-model illumination share a custom
  `rgba16fLinearHdrRgbAoLE` atlas artifact (`SLMH`, artifact version 1). Bake
  and interpolation storage is float32. Disk texels are row-major interleaved
  IEEE binary16 RGBA: linear HDR RGB plus bounded linear AO. Runtime upload is
  a `PIXELFORMAT_UNCOMPRESSED_R16G16B16A16` image owned by `AssetManager` and
  stored by OpenGL as linear `GL_RGBA16F`, never an sRGB internal format. A
  2048x2048 atlas occupies 32 MiB. Binary16 is deterministic and sufficient
  for baked-light precision; finite positive RGB above 65504 is rejected rather
  than clipped or encoded as infinity.
- Object probes use `layeredAmbientCubeLinearHdrF32LE` (`SOPB`, sidecar version
  3). Probe positions, placement settings, and six-face RGB values remain
  float32. Static-model UV/remap metadata is version 3 and remains separate
  from the shared illumination atlas.
- Every multibyte binary field is fixed-width and explicitly little-endian.
  Float32 and binary16 are serialized through their integer bit
  representations; readers reconstruct host values before CPU use or GPU
  upload. No artifact serializes a native C++ struct or relies on padding or
  host byte order. Headers carry dimensions/counts, semantic identifiers,
  source hashes, payload byte counts, and FNV-1a checksums; loaders require
  exact EOF.
- Authored static point-, spot-, and directional-light byte swatches receive
  the exact IEC sRGB-to-linear transfer once when world-space bake lights are
  built. Sector ambient remains numeric `channel / 255 * intensity` linear
  data. Intensity, range, attenuation, shadow, AO, normal, mask, and other
  numeric inputs retain their existing linear meanings.
- The existing indirect pass does not sample sector diffuse textures, glTF
  base-color/emissive textures, material factors/colors, or emissive artwork.
  It samples the already-linear direct-light float buffer and applies distance,
  facing, cosine, configured strength, and the existing neutral 0.55 bounce
  albedo. Normal maps and alpha masks remain numeric normal/occlusion inputs.
  Slice 3 deliberately did not invent colored-surface or emissive bounce.

Validation, versions, and installation:

- `kSectorLightmapBakeVersion` is 15. The source hash now includes explicit
  Slice 3 HDR/color/format tags, atlas/probe/static-sidecar versions, the
  exactly-once light-swatch convention, and the retained neutral-bounce model.
  Geometry, receiver/occluder, texture-data, static-light, static-model, probe,
  and `ceilingSky` inputs remain included. Preview and sky-visual settings
  remain excluded. Old RGBA8/PNG metadata cannot satisfy the current version
  and format and requires a rebake; no compatibility reader or render switch
  was added.
- Status reporting distinguishes no metadata, missing data, stale/versioned
  data, and invalid decoded data. Atlas, probe, and static-model-remap
  availability are independent. Corrupt, truncated, trailing, checksum-invalid,
  structurally invalid, non-finite, negative-radiance, invalid-AO, and wrong-hash
  data fail safely instead of receiving an LDR interpretation.
- Writers close and reopen artifacts through the production reader. Bake
  reports retain pre-encode float32 statistics separately from authoritative
  reopened binary16 atlas statistics; probe statistics likewise come from the
  reopened float32 payload. Reported minimum/maximum and above-one counts
  therefore describe stored data.
- Installation validates temporary files, copies every product to
  same-directory `.installing` paths, reopens the complete staged set, publishes
  data files, then reopens the final set. The editor assigns one complete
  `SectorLightmapMetadata` value only after that succeeds. A failure or crash
  therefore leaves the previous set, a missing set, a stale hash/version, or an
  invalid artifact rather than partially publishing new current metadata.
- Baked atlas and probe/static-model sidecar payloads are generated local data,
  ignored by the repository through `*.lightmap.*` and
  `*.object_probes.bin*`, and are not tracked. Map JSON metadata remains
  authored and tracked, while a fresh checkout reports missing/stale
  illumination until the map is rebaked. This repository policy changes
  neither the Slice 3 artifact formats nor source-hash validity rules.

Runtime consumers and diagnostics:

- Sector geometry samples HDR atlases without a baked-light or final-lighting
  upper clamp. Static PBR models retain the accepted Slice 2.1 incoming-light
  material modulation. Dynamic PBR models and viewmodels retain float32 HDR
  probe selection and interpolation. Billboards no longer upper-clamp combined
  probe/direct radiance. Doors transport probe lighting in a float tangent
  attribute instead of quantized vertex colors.
- Fog, haze, and dust receive unclamped HDR probe values. At Slice 3 completion
  their effect-local RGBA8/4.0 bounds were deliberately left to Slice 4; Slice
  4 has now removed those bounds without changing the probe artifacts.
  Debug-only bounded color visualizations remain presentation aids rather than
  illumination transport.
- Bake reports show artifact version/representation, CPU/disk/GPU formats,
  pre-encode and stored ranges, and above-one preservation. PBR diagnostics
  continue to report the chosen static-lightmap/probe/fallback source and now
  identify it as linear HDR. Color-pipeline diagnostics identify Slice 3 and
  the baked representations. Metadata and statistics are gathered at bake/load
  time without per-frame artifact scans or OpenGL queries.

Verification:

- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed.
- Focused coverage includes exact authored-light sRGB conversion, unchanged
  numeric sector ambient, binary16 HDR/AO round trips, explicit little-endian
  byte layout, deterministic output, stored-payload statistics, probe HDR
  serialization/interpolation, source-version invalidation, corrupt/truncated/
  legacy rejection, staged installation, static-model lighting transport,
  `GL_RGBA16F` selection, and shader policy.
- `git diff --check`, `git diff --stat`, and `git status --short`: reviewed.
- Interactive engine launch, screenshots, and authored production-map rebakes:
  intentionally not performed. Existing maps require rebaking and visual
  validation remains with the user.

## Slice 4 Completion Record

Date: 2026-08-11

Status: complete. Slice 5 remains pending.

Pass graph and target ownership:

- Opaque sector geometry, doors, billboards, and static/dynamic PBR models draw
  into the original supersampled linear `RGBA16F` world target. The simple
  distance-fog equation remains integrated in those material shaders. Local
  fog, light haze, and dust then compose in world space. The isolated
  `RGBA32F` viewmodel target receives the captured-position muzzle flash first
  and opaque arms/weapon second, so the weapon depth-correctly occludes the
  flash without letting world atmosphere fog the viewmodel.
- Full-scene atmosphere and viewmodel composition write a shared point-filtered
  `RGBA32F` scratch and commit to the original world color only after a pass
  completes. No pass samples and writes one texture. The original world depth
  attachment is never detached or overwritten and remains available to the
  depth-tested editor diagnostic pass. On every successful step the original
  world target again owns authoritative normal scene color; an optional-effect
  failure leaves it unchanged rather than promoting a partial scratch result.
  At 2880x1620 the shared scratch and isolated viewmodel color each cost about
  71.2 MiB. `RGBA32F` is deliberate for these same-size transactional/additive
  work targets: overlapping GPU contributions remain finite float32 until the
  single guarded world `RGBA16F` commit. The three 720x405 bloom targets remain
  `RGBA16F` and total about 6.7 MiB, avoiding a full-resolution MRT and keeping
  recurring blur bandwidth quarter-resolution.
- Scene-wide bloom runs after viewmodel/muzzle composition and before editor
  diagnostic lines. The original world target is then supersample-resolved
  while linear HDR, globally tone-mapped and sRGB-encoded, optionally FXAA
  filtered, and finally composed with the HUD, crosshair, menus, FPS display,
  and editor UI. This retains the deliberate Slice 2 resolve/presentation/FXAA
  order while excluding all ordinary UI and editor lines from bloom.

Finite HDR and atmosphere contracts:

- Every affected `RGBA16F` shader write sanitizes RGB to nonnegative finite
  radiance. NaN, negative values, and negative infinity become zero; positive
  infinity and finite overflow saturate only to binary16's maximum finite
  value, 65504. All finite values below 65504 survive without artistic clamps,
  normalization, tone mapping, or exposure changes. Alpha is sanitized and
  bounded independently. This named storage guard replaces neither the removed
  `dynamicLightingClamp` nor the obsolete `[0,1]` and `4.0` radiance ceilings.
- Local fog and haze accumulation targets are bilinear/clamp `RGBA16F` at their
  existing quality-dependent scale. RGB is premultiplied linear in-scattered
  radiance and alpha is bounded opacity (`1-transmittance`); composition is
  `scene * (1-alpha) + RGB`. Their depth-aware bilateral upsample and the
  existing geometry/thickness path-length saturation remain intact. Selected
  overlapping volumes retain deterministic distance/ID selection and existing
  capped-opacity integration; Slice 4 adds no general OIT system.
- Dust is exactly-once sRGB-decoded in its shader, multiplies accepted HDR probe
  and dynamic-light inputs without an upper artistic clamp, and adds
  premultiplied radiance with alpha zero into the shared float scratch. The
  existing soft depth intersection, fog attenuation, placement, animation,
  emitter limits, and particle shapes remain unchanged.

Bloom, emission, and muzzle integration:

- Bloom uses three bilinear/clamp quarter-resolution `RGBA16F` targets. Its
  fused 4x4 downsample prefilters each full-resolution scene tap before
  averaging, retaining compact lamp tubes and muzzle cores without a
  full-resolution MRT. Three separable Gaussian iterations preserve RGB;
  bloom alpha is always zero. Composition changes only scene RGB and preserves
  scene alpha. The shared same-size `RGBA32F` scratch remains point-filtered.
- The exact max-channel prefilter uses `k=threshold*softKnee`,
  `q=clamp(brightness-threshold+k,0,2k)`, and
  `excess=max(q*q/(4k),brightness-threshold,0)`, then scales RGB by
  `excess/brightness`. Threshold zero admits every positive finite color;
  black remains zero; knee zero takes a division-free hard-threshold branch.
  This metric preserves saturated-color ratios and treats a bright individual
  channel as a bloom source. Threshold, soft knee, intensity, and radius are
  finite validated application settings in exposure-1 linear scene space.
- The former decal-specific rerender/blur path is removed. A decal is emissive
  exactly when its existing emissive flag is set. Its decoded texture/tint and
  cutout alpha retain normal depth and visibility. Non-emissive decals keep the
  old base-color mix. Emissive decals use
  `litBase*(1-coverage) + decodedDecal*emissiveStrength*coverage`; unit strength
  reproduces the old visible emissive term, while strength never changes
  alpha, silhouette, cutout, or depth. The existing serialized
  `bloomIntensity` field is retained without a topology-schema migration but
  is presented and consumed as decal emissive strength.
- glTF emission remains per material/per fragment through the existing
  exactly-once emissive texture decode and numeric emissive factor. Core glTF
  `KHR_materials_emissive_strength` is loaded, validated, and applied before
  the finite-half write. Non-emissive model parts are never tagged as bloom
  sources; they bloom only if their final visible scene radiance crosses the
  common threshold.
- Muzzle swatches remain authored sRGB and are decoded once in the HDR muzzle
  shader. Visible-flash radiance strength is a validated weapon/default and
  per-weapon application override, distinct from the temporary geometry-light
  intensity. Existing shape randomization, fire-time transform capture,
  lifetime, attachment, recoil, cadence, and weapon-registry ownership remain
  intact. The flash enters the visible viewmodel scene once and therefore
  contributes to common bloom without a depth-incorrect overlay redraw.

State, diagnostics, and limitations:

- Affected passes flush rlgl batches at framebuffer/shader/blend boundaries,
  use raylib/rlgl state transitions, disable blending for complete fullscreen
  replacements, use additive blending only for alpha-zero dust/muzzle
  radiance, and restore blend, depth-mask, culling, shader, framebuffer, and
  viewport expectations on success. All resource validation queries happen at
  creation; resize rebuilds are lazy and allocation-free per steady frame.
  Failure to validate an HDR target disables only that optional effect for the
  current dimensions and records the reason; there is no RGBA8 fallback.
- Cached diagnostics expose active scene/bloom dimensions, actual formats,
  max-channel threshold/knee/intensity/radius policy, finite-half protection,
  premultiplied atmosphere semantics, disabled-effect status, and selectable
  presentation-only scene-before/prefilter/blur/bloom/scene-after views. Debug
  substitution never overwrites the authoritative world result and uses the
  normal presentation transform only for inspection.
- Local transparent volumes retain the existing selected-volume limits and
  dust remains additive rather than becoming a general transparency solution.
  No unsupported glTF emissive extension beyond core
  `KHR_materials_emissive_strength` was added. No HDR-monitor output, exposure
  control, alternate tone mapper, or production-asset retuning was introduced.

Scope and verification:

- Slice 3 bake algorithms, artifact formats, versions, source hashes,
  installation policy, and authored lighting are unchanged. The accepted
  Slice 2.1 PBR direct/indirect/environment routing is unchanged. No topology
  mutation/cache behavior, collision, sector lookup, physics, camera, recoil,
  hitscan, damage, cadence, or other gameplay behavior changed.
- Non-interactive coverage exercises HDR target selection, finite-half
  sanitization, bloom defaults/domains and exact edge cases, saturated-color
  max-channel behavior, HDR atmosphere/bloom composition and alpha policy,
  muzzle/decal/glTF emission routing, obsolete clamp/RGBA8/decal-bloom source
  guards, and render-pass ordering/UI exclusion.
- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: 21/21 passed.
- `git diff --check`: passed; final diff/stat/status were reviewed in the
  implementation handoff. Interactive launch, automation, screenshots, and
  source-model, texture, and authored-map edits were intentionally not
  performed; visual validation remains with the user.

### Slice 4 viewmodel ownership follow-up

- Dynamic-model projected and contact shadows remain world receiver passes and
  still execute before the isolated first-person viewmodel. Their fragment
  alpha is only a straight-alpha factor for darkening world RGB. The receiver
  pass disables alpha-channel writes so this temporary opacity cannot create a
  shadow-shaped deficit in the otherwise opaque world target alpha. Projected
  silhouettes use a maximum opacity of `0.45` (50 percent stronger than the
  original `0.30`); contact shadows retain their separate `0.20` opacity.
- The original defect survived viewmodel composition because that composition,
  HDR resolve, presentation, and FXAA deliberately preserve scene alpha. The
  final scene was then alpha-composited over the backbuffer, applying the old
  projected-shadow mask to the already-composed hands and weapon. Preserving
  destination alpha at the shadow draw fixes the source of that leak. The
  unsuccessful sampleable-viewmodel-depth workaround was removed; the private
  renderbuffer depth and opaque-alpha viewmodel contract remain sufficient.
- The muzzle flash keeps its alpha-zero additive HDR contract, while opaque
  arms and weapon pixels retain alpha one. The original world depth and
  authoritative world color remain attached to the supersampled world target.
- Dynamic-model shadow-map and receiver transitions flush pending rlgl batches
  and restore the RGBA color mask, texture slot, blend mode, depth test/write,
  shader, and culling state for the documented caller. No shadow placement,
  lighting, gameplay, collision, camera, topology, bake format, or source-hash
  behavior changed.

### Slice 4 AMD/Mesa model-sampler compatibility follow-up

- The shared glTF PBR shader kept an active `samplerCube` environment uniform
  even when a map had no eligible sky/environment texture. Because raylib only
  assigned sampler units for present material textures, the absent cubemap
  sampler retained OpenGL's default unit 0 and aliased the base-color
  `sampler2D`. NVIDIA accepted this invalid mixed-sampler state, while
  Mesa/AMD rejected the model draw.
- Model shader initialization now assigns every active sampler its fixed
  raylib material-map unit before any draw, including the environment cubemap
  unit when no cubemap is bound. The no-environment shader branch still
  contributes exactly zero; no neutral fallback cubemap was restored.
- This correction changes no topology/cache behavior, lightmap artifacts or
  source hashes, collision, sector lookup, physics, camera, or gameplay
  behavior.

### Slice 4 performance follow-up

Status: implemented; hardware acceptance measurements remain pending.

- Application graphics settings now persist beside the existing application
  settings. The menu exposes transactional Apply/Cancel controls for render
  scale, FXAA, the authored-volumetric quality cap, shadow quality, bloom, and
  the F9 performance overlay. Required supersampled HDR targets are built
  before the settings file or live target set is changed; allocation failure
  keeps the previous working targets and settings.
- The presentation path combines supersample resolve, neutral tone mapping,
  and display encoding in one texture sample/pass. FXAA runs while drawing the
  cached presentation texture to the backbuffer, removing the old linear
  resolve and intermediate FXAA targets and their two full-frame writes.
- The viewmodel framebuffer now attaches the active world HDR color texture
  with its own private depth buffer. Viewmodel geometry renders directly into
  the active scene, eliminating the separate viewmodel color composite and
  subsequent full-scene commit. Additive dust likewise renders directly into
  the active HDR scene instead of copying the scene through RGBA32F scratch.
- Bloom prefilters the active HDR scene directly and writes its finite-guarded
  composite to the alternate HDR target. Gameplay presentation consumes that
  target without copying it back; editor preview retains the commit because
  its post-bloom depth-tested authoring overlays still target world color.
- Low volumetrics use quarter-resolution accumulation with bilinear upscale;
  medium uses half resolution and a five-tap depth-aware cross; high retains
  full resolution. The application cap is combined with the map-authored
  quality by taking the lower setting, so it never silently raises authored
  cost. Analytic distance fog and dust remain independent of that cap.
- When local fog and light haze are both active, their composites now ping-pong
  through the shared scratch and a color-only world framebuffer view. This
  removes the intermediate and final full-scene scratch commits while keeping
  the world depth texture read-only and available to both effects.
- Static spotlight shadow maps are retained while selected lights/matrices and
  alpha-caster texture readiness remain unchanged. Door shadow casters keep
  the cache invalid so moving doors remain correct. Shadow Off disables both
  spotlight and projected-model shadow work, Low uses 512-pixel spotlight
  maps and updates projected models at 15 Hz, Medium uses 1024 pixels/30 Hz,
  and High retains 1024 pixels/every-frame projected updates.
- Immutable placed objects no longer repeat sector lookup, baked-probe
  sampling, collider collection, and formatted diagnostic construction during
  every steady frame. Door collider/portal vectors update when door motion
  changes. Renderer pose application can defer visibility work so the game
  and editor steady update perform their explicit visibility update once.
- The F9 overlay uses delayed, non-blocking OpenGL timer queries plus CPU wall
  timings for shadows, world, atmosphere, viewmodel, bloom, presentation, and
  final composition. It is intended to identify the remaining bottleneck on
  the target GPU without introducing a synchronous query stall.
- Opening the menu intentionally freezes and reuses the last presentation
  texture. The scene visible below the menu is cached: shadow maps, geometry,
  atmosphere, viewmodel, bloom, and presentation are not rerendered. This is
  why menu FPS is dramatically higher; paused procedural animation contributes
  some CPU savings, but it is not the primary cause of that jump.

The GTX 3060 release-build acceptance targets remain: fog-off HDR within 15%
of the main-branch LDR frame time at the matched hub view, and medium authored
volumetrics adding no more than 1 ms. These figures require the user's hardware
run and are not inferred from compile/test results.

### Static-light specular follow-up

- Authored static point and spot lights now provide a bounded runtime GGX
  specular term to PBR world models and the first-person viewmodel. Their
  diffuse contribution remains exclusively in the baked surface lightmap or
  object-probe ambient cube, so direct diffuse is not counted twice.
- Each receiver selects at most four range-overlapping lights. Selection uses
  cached per-sector candidates, current portal visibility, receiver bounds,
  light intensity/color, distance attenuation, and spotlight cones. The steady
  draw path uses fixed arrays and performs no raycasts or dynamic allocation.
- Static specular is eligible only while the receiver's corresponding baked
  source is current and loaded: a valid surface lightmap for lightmapped static
  models, or valid object-probe data for dynamic models and the viewmodel.
  Missing, unavailable, or stale bake data leaves the previous diffuse fallback
  behavior and contributes no static-light specular.
- The PBR diagnostic tab reports eligibility, selected count, and authored light
  IDs for both the selected world model and the viewmodel.
- This reuses existing authored static-light data. It adds no static shadow map,
  runtime occlusion ray, topology/schema field, bake artifact, bake version, or
  source-hash input.
