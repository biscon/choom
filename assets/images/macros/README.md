# Macro variation masks

Six masks generated with the built-in imagegen tool. They are intended for broad
material variation, independently of the base texture's UVs and repeat size.
The original generated PNGs are preserved at their native 1254 × 1254 resolution
(the prompts requested 1024 × 1024). They are opaque RGB grayscale-like images;
only the red channel is sampled as linear data, so small RGB differences do not
tint the material. Each was inspected individually and in a 3 × 3 repeat sheet.

In the Material Editor, enable **Macro variation**, choose a mask, and Save.
Existing materials have this feature disabled. Custom PNG masks can be added to
this directory or its subdirectories; they appear in the macro picker and are
excluded from the albedo picker. No separate material-registry entries are needed
for masks.

| Mask | Intended use | Repeat (m) | Darkening | Roughness change |
| --- | --- | ---: | ---: | ---: |
| `soft_mottling.png` | Gentle general variation on floors, walls, ceilings | 8 | 0.08 | +0.12 |
| `plaster_discoloration.png` | Uneven plaster or painted walls | 8 | 0.10 | +0.08 |
| `broad_grime.png` | Sparse areas of dull grime | 6 | 0.15 | +0.15 |
| `patchy_wear.png` | More distinct uneven wear | 8 | 0.08 | +0.10 |
| `uneven_polish.png` | Broad polished areas on tiles | 6 | 0.00 | −0.12 |
| `soft_streaks.png` | Directional variation on walls | 8 | 0.06 | +0.08 |

These are starting points, not presets automatically applied to materials.
Black leaves the surface unchanged; white applies the full configured effect.
Intermediate values scale it proportionally. Darkening is multiplicative in
linear color; positive roughness change makes areas duller and negative makes
them more polished. Roughness is clamped to the renderer's existing limits.

Mapping is anchored to world coordinates in metres and uses geometric-normal
triplanar blending. It does not restart at wall splits or use the base UV scale.
The streak mask runs vertically on axis-aligned walls; floor projection uses X/Z.
Strong settings can expose repeated motifs, so start gently and keep the repeat
size large relative to the base material.

The feature applies to static sector architecture and structural surfaces.
Decals stay layered above the darkened base color. Moving doors, removable covers,
model props, sky, and liquids do not use these settings in this first version.
Missing or pending masks leave the base material unchanged. Macro settings and
pixels do not affect the lightmap source hash and require no rebake.

## Generation prompts

Exact prompts and output names are recorded in `generation_prompts.json`.
All six were generated separately with the built-in tool, with no CLI fallback.
The common request specified a flat grayscale mask, seamless repetition, broad
irregular variation, and no lighting, perspective, objects, grout, cracks, text,
or borders. The collection contains two soft masks and four more structured
variants; use the latter at restrained strengths.

## Manual engine checks

Interactive GUI verification is left to the user. Check adjacent and angled
surfaces, darkening-only and roughness-only settings, decals, reflections,
Save/Cancel, missing masks, and the bottom of the expanded material form.
