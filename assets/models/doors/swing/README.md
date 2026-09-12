# Swing door assets

Seven authored door styles are available alongside the nine retained wooden
styles. The old downloaded metal and worn collections have been removed. Existing
hub placements still reference their original wooden assets.

Select the new styles in the existing model-swing door picker. Their frames and
leaves use the same fit, hinge, swing direction, lock, sound, and motion settings
as the existing doors. Reload the editor/application to pick up catalog changes.

| Catalog ID | Finish and construction | Leaf thickness | Assembly triangles |
|---|---|---:|---:|
| `wood_walnut_panel` | Dark walnut, two raised panels, brass lever and lock | 45 mm | 7,904 |
| `painted_ivory_panel` | Ivory enamel, two recessed panels, nickel hardware | 45 mm | 6,928 |
| `painted_sage_panel` | Muted green enamel, three recessed panels, nickel hardware | 45 mm | 7,292 |
| `kitchen_service` | Ivory flush leaf, stainless kick/push plates | 45 mm | 7,668 |
| `industrial_charcoal` | Charcoal painted steel, folded seams and scuff plate | 50 mm | 5,772 |
| `security_reinforced` | Heavy green-gray steel, reinforcement straps and deadlock | 90 mm | 8,596 |
| `security_institutional` | Plain thick gray steel, large lock plate and deadlock | 75 mm | 7,460 |

Each leaf is 0.90 m wide and 2.05 m high before the existing uniform fitting scale.
Frame assemblies are 1.068 m wide (1.074 m for security doors), 2.148 m high.
Use **Fit Inside** when the whole frame must remain within an aperture. Fit Width
and Manual retain their existing behavior. No level aperture has been resized.
Nominal thickness describes the structural leaf; hardware and panel moldings
project beyond it. Hardware is decorative and moves rigidly with the leaf.

## Files and coordinate contract

- Keep each `_leaf.gltf` / `_frame.gltf` beside its matching `.bin`.
- Keep `textures/` alongside the models; it is shared across the new styles.
- `source/door_set.blend` is the editable assembled source, with relative image paths.
- `source/texture_prompts.json` records the exact built-in ImageGen prompts.
- `previews/` contains front, rear, both hinge configurations, and hardware details.
- `catalog.json` remains format version 1; no engine or map schema additions.

Export coordinates are metres, +Y up, +X from hinge to free edge, +Z front.
The leaf origin is its bottom hinge at the depth center. The frame origin is its
bottom horizontal center. `leafHingeToFrameCenter` is 0.45 m and
`leafBottomOffset` is 0.008 m. The engine supplies all animation transforms.
Frames and leaves are separate, with transforms baked and one identity root per
export. No rigs, animations, cameras or lights are exported. Every material is
opaque and single-sided; each leaf has closed solid coverage on both faces.

The seven assemblies total 51,620 triangles. Leaf meshes are consolidated by
material (three or four materials per leaf); frames use two materials. Geometry
is limited to visible forms: panel profiles, edge bevels, handles, escutcheons,
lock cylinders, deadlocks, hinge barrels, screws, straps and protection plates.

## Materials

Four new base-color images were generated through the built-in ImageGen tool:
walnut, neutral enamel, brushed steel and brass. They are stored at their native
**1254 x 1254** resolution; the tool returned this size despite the requested
2K/1K targets. No old door texture was upscaled. UVs keep the visible material
region inside the image and orient rail grain horizontally and stile grain
vertically. The painted colors use glTF base-color factors on the enamel image.

Normal maps and packed ORM maps were baked in Blender at **1024 x 1024**.
Normals are tangent-space OpenGL Y+. The microstructure is deliberately subtle;
panel relief comes from geometry. Base color is sRGB; normals and ORM are linear.
ORM uses R=neutral occlusion, G=roughness, B=metallic. Painted metal is dielectric
at its paint surface; exposed steel/brass is metallic. No room lighting or room AO
is baked into these textures. No custom shader or new runtime material support is
required.

## Rebuild and validation

From the repository root, using Blender 5.2 (the authoring version):

```sh
blender --background --factory-startup --python tools/prepare_swing_door_assets.py -- --mode all
python3 tools/door_assets/validate.py
```

The first command rebakes normal/ORM maps and rebuilds the seven authored models
from `tools/door_assets/authoring.py`. It uses the checked-in ImageGen images and
preserves the nine original wooden exports. It never needs the removed download
packs or an API key. `--mode prepare --asset ID` rebuilds one style, `--mode verify`
checks exports, and `--mode render` produces review images. Blender MCP can call
individual functions from the same module to perform these steps incrementally.
The standalone validator requires NumPy and does not need Blender or a GPU.

Validation checks catalog IDs, file dependencies, opaque materials, PBR slots,
indices, finite vertices/UVs/normals/tangents, canonical transforms and bounds,
frame measurements, triangle budgets, and 3,237 sample positions on each face
for opaque coverage. Review images are rendered from reimported exported glTF,
not just from the authoring scene. Review output goes to
`build/swing_door_asset_work/new_doors/`.

The implementation was checked with a debug build, CTest, the asset validator,
and Blender renders. Interactive engine testing remains for the user: select
each new style, check fit in intended apertures, open/close from each hinge and
swing side, and inspect materials under room lighting. No automated GUI smoke
test was performed.

Engine door behavior, collision algorithms, sector lookup, physics, topology
mutation/cache invalidation, and lightmap source-hash policy are unchanged.
Asset dimensions feed the existing fitting and collision-proxy behavior. See
`ATTRIBUTION.md` for retained wooden-collection attribution and new-asset provenance.
