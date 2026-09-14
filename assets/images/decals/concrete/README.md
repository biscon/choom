# Concrete wall decals

Four albedo-only wall decals generated separately with the built-in imagegen
tool, with no CLI fallback. The original 1536 x 1024 RGBA PNGs are preserved
without image postprocessing. Exact prompts are in `generation_prompts.json`.

| Material ID / PNG filename stem | Surface feature |
| --- | --- |
| `concrete_decal_branching_cracks` | Broad diagonal fracture network, chipped lips, and smaller tapering branches. |
| `concrete_decal_spalled_patch` | Shallow missing cement surface, exposed aggregate, and satellite chips. |
| `concrete_decal_damp_damage` | Downward damp streaks, chalky mineral deposits, and a cracked seepage trace. |
| `concrete_decal_old_repair` | Irregular troweled cement repair with perimeter fractures and dirt streaks. |

## Placement

The materials are registered in `assets/materials/materials.json`. Select a wall
surface in the editor, switch to **Layer: Decal**, and choose a material by its
`concrete_decal_` prefix. Start with white tint, opacity 1, and emissive disabled.
These are surface decal settings, not presets stored in the material registry.

Start with the full image spanning approximately 3 meters wide by 2 meters tall.
Adjust decal UV scale and offset to position it; keep the 3:2 aspect ratio unless
stretching is intentional. The damp streaks should run downward. Use reduced
opacity or a darker tint if the pale exposed cement/minerals are too prominent
against the chosen wall. Each image combines multiple details within the
existing single decal slot; it is not a tileable base-wall texture or an atlas.
No decals have been placed in saved levels.

The alpha channel leaves the existing wall visible between isolated features.
The damaged patch interiors are mostly opaque; stains and fine edges have
partial coverage. No normal/roughness companion maps are included because the
decal path consumes albedo only. The registry uses anisotropic8x filtering and
standard rough, nonmetallic defaults.

## Verification

Checked PNG dimensions, RGBA channels, transparent regions, and compositing on
dark, light, and mid-gray backgrounds. All substantial features end inside the
canvas. Some perimeter pixels retain alpha 1/255 from generation; these were
preserved with the original soft alpha rather than thresholded. No rectangular
background or visible clipped perimeter was observed in the composite previews.

Interactive editor verification is left to the user: check selection in the
material picker, UV placement, and blending under the room's actual lighting.
No renderer, topology, cache-invalidation, lightmap source-hash, collision,
sector-lookup, or physics code changed. CTest is not required for this asset-only
change.
