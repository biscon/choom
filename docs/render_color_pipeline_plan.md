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

Map/editor RGB byte colors used for lights, fog, haze, tints, muzzle lighting,
and related scene quantities are visible sRGB color-picker swatches. The final
pipeline decodes RGB to linear scene values while leaving alpha as a linear
opacity/value. Slice 1 provides the canonical helpers but deliberately retains
the active raw `Color/255` behavior. Slice 2 migrates runtime rendering and
slice 3 migrates baking.

### glTF conventions

glTF base-color and emissive textures use sRGB decode. Base-color, emissive,
and material factors retain glTF-defined numeric conventions. Metallic,
roughness, occlusion, and normal channels remain linear data. raylib-loaded
model textures bypass the generic texture asset API; the active model shader
continues to decode color sampling roles explicitly until slice 2.

## Migration Tracker

| Slice | State | Intent |
|---|---|---|
| 1. Color/HDR foundations | Complete | Contract, exact transfer helpers, texture semantics, float-target factory, diagnostics, tests |
| 2. Linear HDR world and presentation | Pending | Replace world/viewmodel scene output with linear HDR and one global tone/output pass |
| 3. HDR baked illumination | Pending | Preserve HDR radiance in lightmaps and illumination probes; update bake conventions and hashes deliberately |
| 4. Atmosphere, blending, muzzle and bloom | Pending | Move effects/composition to linear HDR and replace the LDR bloom workaround |
| 5. Audit, cleanup, controls and hardening | Pending | Remove obsolete mixed paths, validate platforms, add final controls and regression hardening |

Texture semantics are authoritative metadata and cache identity in slice 1,
but all active generic uploads remain ordinary raylib linear-format textures.
GPU sRGB interpretation is activated atomically with slice 2 presentation work.

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
- Interactive runtime launch and screenshots: intentionally not performed for
  this slice; actual GPU state is emitted by the new startup/target diagnostics
  when runtime testing begins.
