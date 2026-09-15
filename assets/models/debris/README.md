# Debris kit

Static tunnel collapses and loose concrete debris for the underground base. Built in Blender through MCP from the approved concept, 2026-09-15.

**13 individual glTF models, 13 BIN files, 9 shared PNG textures.** No GLB or archive. All exports are in `exports/gltf/`; keep its `textures/` subfolder beside the glTF files.

## Pieces and scale

One unit is one metre. All pivots are on the floor, centred across the complete asset footprint. glTF axes: **+Y up, +Z front**, width along X. The models are complete closed meshes; the large assemblies have debris on both front and rear.

| Collapse | Width × height × depth, metres | Tested clear opening, width × height |
|---|---|---|
| `Collapse_A_Diagonal_Slabs` | 3.60 × 3.50 × 1.70 | 3.06 × 2.975 |
| `Collapse_B_Broken_Beams` | 4.00 × 3.764 × 1.90 | 3.60 × 3.387 |
| `Collapse_C_Upright_Slab` | 3.40 × 3.60 × 1.65 | 3.06 × 3.24 |

The full envelope includes protruding steel and jagged edges. The tested opening sizes describe smaller centred rectangles that were fully covered by the actual mesh in a 61 × 61 ray grid. These are finite geometric samples, not a collision test in your engine.

The ten scatter models are:

- `Chip_08cm`, `Chip_14cm`, `Fragment_25cm`, `Fragment_40cm`, `Chunk_60cm` — five irregular fragments. Names indicate approximate sizes.
- `Slab_Rebar` — roughly 76 cm concrete slab, 1.375 m overall including steel.
- `Beam_End_Rebar` — short broken beam, 1.191 m overall including steel.
- `Slab_Flat_50cm` — low flat broken slab.
- `Scatter_Cluster_Small`, `Scatter_Cluster_Large` — loose arrangements of 9 and 16 fragments, respectively. Pieces in these clusters deliberately do not all touch; every fragment rests on the floor plane.

The manifest records exact dimensions and triangle counts. There are **28,192 exported triangles across all 13 files**. The three large collapses use roughly 8–9k triangles each; individual loose pieces use 36–872 triangles.

## Placing a collapse

1. Place a large collapse across the passage with its pivot at floor level. Its +Z face points toward the player; rotating 180° also gives a dressed rear face.
2. Embed its perimeter in the walls and ceiling. The irregular edges and reinforced concrete core are designed for this. The top and side returns are closed, but these are tunnel plugs rather than freestanding, walk-around piles.
3. For openings larger than the tested rectangle, scale the model until its solid part overlaps the entire opening. For example, B at scale **1.2** gives a tested covered rectangle of about **4.32 × 4.06 m**. A small floor overlap also hides contact seams on uneven floors.
4. Place scattered fragments over the floor in front, with denser/larger debris nearest the blockage. Rotate about engine Y and vary the selection; all of these have floor pivots.
5. In the cutscene, enable the collapse and its debris while the player looks away, alongside the rumble/screenshake. Enable an engine collision blocker/nav obstruction at the same time. Standard glTF does not encode your engine's collision or cutscene events; avoid relying on individual rebar or small rubble triangles to block movement.

The `.blend` includes a studio lineup and a separate simple tunnel placement scene. These rooms, cameras, and lights are not in the model exports.

## Shared materials

Three common materials: formed concrete, broken concrete/aggregate, and rusty reinforcing steel. Each has base colour, tangent-space **OpenGL Y+ normal**, and ORM. Concrete textures are **1024²**, steel **512²**. All 13 glTFs reference the same relative image paths, allowing your engine to cache and reuse the textures.

- Base colour: sRGB, decode to linear and multiply by the glTF `baseColorFactor`.
- Normal: linear data, OpenGL green/Y+, no green-channel flip.
- ORM: linear data, **R = occlusion, G = roughness, B = metallic**. R is white/neutral; glTF uses the image's G/B via `metallicRoughnessTexture`. No extra roughness file is necessary.
- Concrete has a linear tint factor of **[0.63, 0.61, 0.58, 1]**. The exporter script writes this explicitly because Blender's exporter did not preserve the legacy multiply node's constant. Respect this factor to match the source material.
- Concrete is nonmetallic, rough, and dusty. The steel's rust mask reduces metallic response and raises roughness. Nothing emits light.

Normals are restrained height approximations from the source swatches, not measured scans. No directional room shadows or reflections are baked into the textures. Blender previews use Cycles/AgX; they do not validate your engine's ACES implementation.

## Checks performed

- Inspected the reference renders from front, rear, above, at floor level, and close to exposed rebar; also inspected a tunnel placement.
- Corrected rebar roots that initially stopped short of their slabs, ground gaps under the blocking cores, and exposed overly regular fragment shapes.
- Every final non-cluster asset forms one contact assembly. Loose scatter clusters are intentionally separated and grounded.
- All evaluated mesh components are closed; no zero-area export triangles. Coplanar overlap audit found **zero overlapping coplanar triangles between disconnected components**. This check does not prove the absence of every near-coplanar or same-component issue; visual inspection complements it. Intentional noncoplanar overlap embeds rubble and steel.
- All glTFs reimported successfully with matching triangle counts and metre bounds. Binary checks validate indices, finite UVs, unit normals/tangents, material tint factors, external PNG paths, shared payloads, and texture sizes.
- Ray sampling confirms the opening coverage listed above. The engine's collision, lighting and cutscene logic still come from the level editor.

Reports: `export_validation.json`, `roundtrip_validation.json`, `surface_audit.json`, `contact_audit.json`, `coverage_audit.json`.

## Editable source and rebuilding

Open `debris_reference.blend`. Final source maps are in `textures/`; approved concept, source swatches and prompts are under `reference/`.

`scripts/build_all.py` runs the final staged Blender construction, contact/coverage checks, export, reimport, and render queue. Run it through Blender MCP in a suitable fresh scene/context; it creates new kit scenes and preserves existing unrelated scenes. Existing scenes are not automatically deleted, so repeated builds will accumulate copies. `make_maps.py` is a system-Python/Pillow/numpy preparation step if regenerating PNGs from the already saved source.

The one-time `08_core_fit_revision.py` documents an intermediate live fix. **Do not run it after the final build**: its change is already incorporated in `00_build.py`. Final surface dressing is applied once by `09_surface_dressing.py`.

After a geometry/material revision, run `verify_exports.py` and `audit_surfaces.py` with system Python, inspect the new renders, and save the corrected `.blend` as well as the exports.
