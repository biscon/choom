# Macro variation masks

Twelve masks generated with the built-in imagegen tool. They are intended for broad
material variation, independently of the base texture's UVs and repeat size.
The original generated PNGs are preserved at their native 1254 × 1254 resolution
(the prompts requested 1024 × 1024). They are opaque RGB grayscale-like images;
only the red channel is sampled as linear data, so small RGB differences do not
tint the material. Each was inspected individually and in a 3 × 3 repeat sheet.

In the Material Editor, enable **Macro variation**, choose a mask, and Save.
Macro variation is opt-in per material. Custom PNG masks can be added to
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
| `sparse_damp_blooms.png` | Isolated diffuse damp patches on plaster or concrete | 8 | 0.12 | +0.10 |
| `mineral_tide_marks.png` | Broad mineral or dried-water stain contours | 10 | 0.08 | +0.12 |
| `broad_wipe_arcs.png` | Sweeping areas of uneven cleaning or floor polish | 8 | 0.00 | −0.15 |
| `horizontal_weathering.png` | Broken horizontal weathering bands on walls | 10 | 0.06 | +0.08 |
| `clustered_discoloration.png` | Groups of soft stains on walls, floors, or ceilings | 8 | 0.10 | +0.10 |
| `branching_stains.png` | Diffuse branching discoloration on concrete or plaster | 10 | 0.08 | +0.12 |

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
All masks were generated separately with the built-in tool, with no CLI fallback.
The wipe-arcs mask received built-in image edits to remove fine brush texture
and improve edge continuity; its initial and refinement prompts are recorded.
Generated edges are approximate; strong effects can expose small seams,
especially on the smooth wipe arcs. Final PNGs are copied
directly from the generated outputs without image postprocessing.
The common request specified a flat grayscale mask, seamless repetition, broad
irregular variation, and no lighting, perspective, objects, grout, cracks, text,
or borders. The second collection adds damp blooms, tide marks, wipe arcs,
horizontal weathering, clustered discoloration, and branching stains. Use the
more recognizable contours and directional patterns at restrained strengths and
large repeat sizes. These are variation masks, so mineral stains darken the base
when darkening is enabled; they do not add a white mineral color.

## Manual engine checks

Interactive GUI verification is left to the user. Check adjacent and angled
surfaces, darkening-only and roughness-only settings, decals, reflections,
Save/Cancel, missing masks, and the bottom of the expanded material form.
