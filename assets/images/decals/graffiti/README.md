# Graffiti wall decals

Eight albedo-only decals generated individually with built-in imagegen, with no
CLI fallback. The original 1536 x 1024 RGBA PNGs and generated alpha are preserved
without image postprocessing. Exact prompts are in `generation_prompts.json`.

| Material ID / PNG filename stem | Artwork |
| --- | --- |
| `graffiti_decal_tag_jot` | Ivory-white JoT handstyle tag with underline. |
| `graffiti_decal_tag_vim` | Charcoal VIM handstyle tag with hooked underline. |
| `graffiti_decal_tag_moa` | Faded blue MOA handstyle tag with underline. |
| `graffiti_decal_piece_jot` | Teal JoT piece with cream highlights and ochre accents. |
| `graffiti_decal_piece_vim` | Angular rust-orange VIM piece with pale keyline. |
| `graffiti_decal_piece_moa` | Dusty violet MOA bubble-letter piece. |
| `graffiti_decal_slogan_red` | Crude red two-line "FUCK THE SYSTEM" slogan. |
| `graffiti_decal_anarchy_red` | Red hand-sprayed circled anarchy A. |

## Placement

Registered in `assets/materials/materials.json`. Select a wall surface, switch
to **Layer: Decal**, and filter the material picker by `graffiti_decal_`. Start
with white tint, opacity 1, and emissive disabled. Those are surface settings,
not material-registry presets. Restart the editor if its loaded catalog does
not include the new entries.

Preserve the full canvas's 3:2 aspect ratio. Suggested initial canvas sizes:
tags 1.5 x 1 meter, pieces 3 x 2 meters, slogan 2.4 x 1.6 meters, anarchy symbol
1.5 x 1 meter. Adjust decal UV scale/offset for the wall and keep drips pointing
down. The symbol is approximately square within its landscape canvas. The dark
VIM tag suits the lighter concrete above the black wall band; the ivory JoT tag
also reads against darker surfaces. Opacity can reduce the paint's prominence.

Each PNG is a single decal, not an atlas or tileable base material. Transparent
gaps and partial-alpha wear let the existing wall show through. The pieces have
opaque-looking paint fills and painted outlines. No companion maps are needed:
the decal renderer consumes albedo only. Registry settings match the existing
concrete decals: anisotropic8x, nonmetallic, roughness 0.85. No saved levels were
edited or decals placed automatically.

## Verification

Checked original-file byte preservation, PNG dimensions and alpha, all eight
registry paths, and composite previews over dark, mid-gray, and light grounds.
Substantial painted features end inside the canvas; some perimeter pixels have
alpha 1/255, preserved along with the generated soft overspray. No rectangular
background or visibly clipped artwork was observed.

The debug build and `git diff --check` passed. CTest was skipped because this
change contains no C++ edits. Interactive editor verification remains for the
user: material picking, UV placement, and blending under the actual room lights.
No topology/cache-invalidation, lightmap source-hash, renderer, collision,
sector-lookup, physics, or camera code changed.
