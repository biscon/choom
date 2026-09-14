# Wall and floor tile decals

Four albedo-only wear decals generated separately with built-in imagegen, with
no CLI fallback. Original 1536 x 1024 RGBA PNGs are preserved without image
postprocessing. Exact prompts are recorded in `generation_prompts.json`.

| Material ID / PNG filename stem | Intended surface and detail |
| --- | --- |
| `tile_decal_wall_crazed_glaze` | Off-white ceramic walls: fine branching fractures and small glaze chips. |
| `tile_decal_wall_mineral_streaks` | Walls near sinks or leaks: downward mineral deposits, dirt, and restrained rust traces. |
| `tile_decal_floor_chipped_fractures` | Tan ceramic floors: angular fractures and scattered shallow beige ceramic chips. |
| `tile_decal_floor_scuff_wear` | Floor traffic areas: faded rubber arcs, drag streaks, short scuffs, and dusty smears. |

## Placement

Registered in `assets/materials/materials.json`. Select a wall or floor, switch
to **Layer: Decal**, and search for `tile_decal_` in the material picker. Start
with white tint, opacity 1, and emissive disabled. These surface settings are
placement suggestions, not material-registry presets.

Begin around 3 meters wide by 2 meters tall, maintaining the 3:2 image aspect
ratio. Adjust decal UV scale and offset for the selected surface. Keep wall
runoff vertical; floor scuffs work well around doors, sinks, and traffic routes.
Reduce opacity for subtler stains or scuffs. Fractures can be scaled smaller to
concentrate damage on fewer tiles.

The images contain only wear marks; they do not impose a second tile/grout grid.
Existing tile color and grout remain visible through transparent areas. Damage
is not automatically masked at grout joints, so position opaque chips within
tile faces where practical. Each image combines several details for the single
decal slot and is non-tileable. No saved levels have been edited.

Only albedo is supplied because the current decal path consumes albedo, not
normal or roughness companions. Registry definitions use anisotropic8x filtering
and rough, nonmetallic defaults. Scuffs change color; they do not change gloss.

## Verification

Verified RGBA dimensions, real transparent and partially transparent pixels,
and compositing over the project's `white_wall_tiles.png` and `floor_tiles.png`,
plus dark and light solid backgrounds. No rectangular backdrop or visible
clipped edges was observed. Perimeter alpha is at most 1/255; this negligible
generated fringe is retained with the original alpha rather than thresholded.

Interactive editor checks remain with the user: material selection, wall/floor
UV placement, contrast under scene lighting, and appearance near grout joints.
This asset-only addition changes no topology/cache invalidation, lightmap source
hashing, collision, sector lookup, or physics code. No CTest run is needed.
