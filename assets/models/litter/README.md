# Litter / trash kit

24 individual glTF models, 23,428 triangles across the entire kit, and 12 shared PNG maps. Created in Blender through MCP from the approved concept. No GLB or archive.

## Import

Copy `exports/gltf/` as a unit. Each `.gltf` needs its matching `.bin` and the shared `textures/` directory. Cache textures by resolved path so repeated models reuse GPU images. Material definitions are included in each glTF; the PNG image payloads are shared.

One unit is one metre. Exported axes are +Y up, with +Z corresponding to Blender -Y. Each prop has an identity transform and a bottom-centred bounding-box pivot. Set the pivot at floor height. Bottles are standing; broken bottles, cans and the paper cup are exported in resting orientations. When tipping a standing bottle, lower its rotated bounds to the floor. The shard cluster contains six individually grounded pieces.

## Contents

- Three tied refuse bags: large (67 cm tall), slumped (40 cm), small (36 cm).
- Green and amber glass bottles (27.5 and 24.5 cm tall), plus a dented capped plastic bottle (21.3 cm).
- Broken bottle body and neck, three individual glass shards and a six-shard cluster.
- Red and blue crushed drink cans with real drinking apertures and pull tabs.
- Three folded candy/snack/foil wrappers, roughly 10–19 cm across.
- Used paper cup and separate lid.
- Crumpled paper ball, dirty curled sheet and discarded newspaper.
- Flattened cardboard and an open discarded carton with bent flaps.

Exact dimensions and triangle counts are in `manifest.json`. The 24 props are unique meshes; no color-only duplicate files are counted.

## Materials

Four shared PBR sets: bag plastic (512²), cardboard (512²), dusty glass substrate (512²), and packaging atlas (1024²). Every set contains base color, OpenGL Y+ tangent normal and ORM. ORM channels: R occlusion (neutral white), G roughness, B metallic. No redundant roughness maps. Base colors are sRGB; normal and ORM images are linear. Apply glTF material factors and normal scale; bottle tints depend on `baseColorFactor`.

The glass uses an **opaque artistic approximation**, with tint, muted surface wear and broad reflection response. It needs no transparency sorting, transmission extension, refraction or SSR. The small metallic contribution improves the tinted reflective appearance under coarse probes; it is not physically accurate glass. All kit materials are opaque and non-emissive. Plain tin, foil, caps and lid use factors instead of extra texture files.

Imagegen supplied the surface and generic packaging artwork. `scripts/make_maps.py` extracts the atlas, compresses baked contrast on plastic, and derives subtle height-based normals and packed material channels. These are artistic maps, not scanned measurements. `reference/prompts.txt` records the built-in generation prompt and provenance. Generated source and approved concept are preserved alongside it.

## Inspection and corrections

Reviewed front/three-quarter, rear, top, underside, ground-level, glass, bags and small-litter views. Thin sheet fronts and backs use matching triangle diagonals: an initial nonplanar-quad tessellation mismatch allowed the reverse face to poke through and was corrected. Bottle break contours were simplified from excessive spikes. Bag folds and material response were revised after the first preview.

- Export meshes: closed manifold shell components, nonzero triangle areas, finite UVs, normals and tangents.
- Coplanar audit: zero positive-area overlaps detected, checking within and between connected components at 1e-6 metre plane quantization / 1e-10 projected area threshold.
- Contact audit: all bags, caps, can lids/tabs and carton flaps form their expected contact groups. The six shards intentionally remain separate and each touches the floor plane.
- All 24 glTF files reimported successfully with matching triangle counts and bounds.
- Shared exported PNG pixels match the final source PNGs; packed Blender images were refreshed and synchronized explicitly.

Audit tolerances do not prove the absence of every near-coplanar depth conflict or intersection. The preview uses Cycles/AgX; it is not a validation inside the user's ACES engine. Review under the engine's lighting/probes as usual.

## Files

- `litter_reference.blend`: editable assets, studio lineup and a simple corridor placement scene; textures packed.
- `renders/01_overview.png`: full kit.
- `renders/07_corridor_placement.png`: example game-scale placement.
- `exports/gltf/`: the deliverable prop files and shared images.
- `scripts/build_all.py`: rebuild through Blender MCP in a fresh file (preserves unrelated scenes and refuses to duplicate this kit).
- `scripts/06_revise.py`: historical targeted rebuild after initial visual review; current foundation/build scripts already include its geometry changes.
- `mesh_validation.json`, `surface_audit.json`, `contact_audit.json`, `export_validation.json`, `roundtrip_validation.json`: verification records.

Room geometry, lighting and display placements are presentation aids and are not included in the prop exports. No collision meshes or gameplay behaviour are embedded.
