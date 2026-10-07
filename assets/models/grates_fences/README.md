# Modular grates and fences

26 standalone glTF assets, 37,376 triangles across the full kit, using 9 shared 512×512 PNG maps. Worn charcoal steel, olive painted steel, and galvanized steel. All openings are real mesh geometry; no alpha masking, transmission, emission, or Blender-only shaders.

## Contents

- Bars, diamond lattice, and chain-link panels: **1 m, 2 m, and 4 m nominal widths** for each family.
- A short 1 m infill panel in each family.
- Three gate designs, each supplied as a fixed frame and separate rotating leaf.
- Round end, line, and corner posts; a square post for heavier grates.
- Wall mounting bracket, 1 m wall vent, and 1×1 m / 2×1 m floor grates.

4 m means width, not height. Tall panels fit the same nominal 2.4 m fence system. Short panels fit a nominal 0.8 m height. Frames sit above the floor with an intentional 8 cm clearance; posts and gate-frame feet start on the floor. Post caps reach up to 2.44 m.

## Import

Import files from `exports/gltf/`. Keep each `.gltf` and `.bin` together, with the shared `textures/` directory beside them. No GLB or archive is necessary. The manifest lists exact bounds, triangle counts, and pivots. Individual glTF files contain material definitions; resolved external image paths should be cached by the engine to share GPU allocations.

Scale: **1 unit = 1 metre**. Exported coordinates: +Y up, +X along the fence, +Z towards its front. Source Blender uses +Z up / -Y front. Static export transforms are identity; lineup positions are not baked into the files.

## Modular placement

Panel origins sit on the **left post centre at ground level**, not at the panel bounding-box centre. A 4 m panel at `(0,0,0)` connects posts centred at `(0,0,0)` and `(4,0,0)`. The next 2 m panel starts at `(4,0,0)`, ending at `(6,0,0)`; use one line post at the shared connection.

Nominal width is the post-centre spacing. Bar/lattice frame mesh widths are 0.90 / 1.90 / 3.90 m. Chain-link outer widths are 0.924 / 1.924 / 3.924 m. This leaves room for posts and connector arms, which physically engage the side frames. Avoid scaling panels to the nominal width a second time. Tall bar/lattice mesh occupies Y=0.08…2.38 m, while round chain-link frame edges occupy approximately Y=0.064…2.381 m.

Round post orientation at identity:

- End post arm points +X; rotate 180° at the far end of a straight run.
- Line post arms point ±X.
- Corner post arms point +X and -Z. Rotate it to match the incoming/outgoing panels.

A checked reference assembly is included in Blender: 4 m panel, 1.2 m gate module, 2 m panel, then a 90° return. It demonstrates connector engagement. The reference scene is only a placement aid; the engine loads the individual exports.

Wall fixtures can be placed into tunnel geometry, using the bracket as needed. `Wall_Mount_Bracket` has a centre pivot; its wall-contact plane is glTF Z=-0.012 m and the arm projects towards +Z. Floor grates use bottom-centred origins and are 0.06 m thick; recess them into sector floors as required. Vent origin is bottom centre.

## Gates

Place the fixed `*_Gate_Frame` at the desired left post centre. Its right post centre is 1.2 m along +X. The 1.36 m overall bounding width includes both footplates, not the repeat span.

Place its matching `*_Gate_Leaf` at **frame position + `(0.075, 0, 0.055)`**, rotated by the same parent orientation. The leaf origin is its vertical hinge axis at ground height. Rotate around **local +Y**, using negative angles to open towards the front. Sampled clearances were checked at 0°, -1°, -5°, -15°, -30°, -45°, -60°, -90°, and -110°. Hinges have actual sleeve bores; the pin and leaf do not intersect at these sampled positions. The latch/handle is static detail, not a separately animated lock.

The gate frame includes its posts and outward panel connectors. Do not add duplicate posts at its endpoints. Chain-link gate-frame left/right posts are intentionally separate components without an overhead bar.

## Materials

Each of the three finishes has:

- `*_basecolor.png`: sRGB base colour.
- `*_normal_opengl.png`: linear tangent-space normal, OpenGL Y+.
- `*_orm.png`: linear packed R=occlusion (white/neutral), G=roughness, B=metallic. Both metallic-roughness and occlusion bindings reference this same image.

No redundant roughness maps. Base-colour source swatches were generated using built-in imagegen; reproducible technical normal/ORM derivatives are in `scripts/make_maps.py`. The normal maps are subtle height approximations, not high-poly bakes. Imagegen prompts and source image are in `reference/`.

Geometry is modest: bar/lattice 4 m panels are 1,472 / 1,180 triangles; chain-link 4 m is 9,392 triangles because the open weave is geometry. Consider distance culling for many repeated chain-link spans. Thin wire may shimmer at distance depending on engine antialiasing.

## Verification and editable source

`grates_fences_reference.blend` contains the asset lineup and assembly scene, with packed textures. Source scripts live in `scripts/`. `build_all.py` is for a fresh Blender session and deliberately refuses to overwrite an existing kit scene.

Checks include closed manifold shells, nonzero triangles, UV/tangent validity, coplanar triangle overlap, component connections, assembled panel/post joins, sampled gate clearance, exported buffers and shared PNG paths, and reimport of every glTF. Reports are in the kit root. The coplanar audit includes pairs within a connected component, but is not a general collision/near-coplanar proof; visual inspections supplement it.

Previews use Cycles/AgX. They are not an ACES engine validation. No engine collision objects or animations are included; add collision blockers appropriate to your tunnel/gate gameplay.
