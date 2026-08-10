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
must never receive sRGB decoding. Baked lightmap RGB is linear radiance and its
alpha is linear AO, even while the current artifact is stored in RGBA8.

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
| 3. HDR baked illumination | Pending | Preserve HDR radiance in lightmaps and illumination probes; update bake conventions and hashes deliberately |
| 4. Atmosphere, blending, muzzle and bloom | Pending | Promote bounded effect intermediates/radiance to HDR and replace the LDR bloom workaround |
| 5. Audit, cleanup, controls and hardening | Pending | Remove obsolete mixed paths, validate platforms, add final controls and regression hardening |

Texture semantics are authoritative metadata and cache identity. Since slice 2,
generic `SceneSrgb` uploads use sRGB GPU storage and sampling while `LinearData`
and `DisplaySrgb` retain ordinary linear-format storage.

## Future HDR Bloom Requirement

Slice 4 replaces the current LDR decal-specific workaround. Bloom will operate
on pre-tone-map HDR scene radiance and/or an explicit emissive-radiance
contribution; accept visible emissive fixtures, models, particles, muzzle
flashes, and future sources; respect normal scene depth; and use bright HDR
values with a meaningful threshold instead of LDR intensity/clamp scaling.
Decal sorting and bloom behavior do not change in slice 1. The render-target
factory supports arbitrary dimensions and RGBA16F so it can later provide
quarter-resolution HDR intermediates without prebuilding that pipeline now.

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
