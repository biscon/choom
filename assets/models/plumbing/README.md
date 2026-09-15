# Modular industrial plumbing kit

97 individual glTF + BIN exports, eight shared external PNG textures. Meter scale, glTF +Y up. No GLBs. The Blender reference is a placement example; its wall, floor, lighting and display arrangement are not exported.

Copy `exports/gltf` including its `textures` subfolder. Deduplicate textures in the engine by resolved file path. Base color is sRGB; normal and ORM are linear. ORM channels: R occlusion (white here), G roughness, B metallic. The glTF metallic-roughness texture uses G/B; no separate roughness map. Normals are tangent-space OpenGL +Y.

## Pieces

| Family | Count | Options |
|---|---:|---|
| Straight pipe | 36 | 50/100/200 mm OD × 0.25/0.5/1/2 m × Steel/Green/Oxide |
| 90° elbow | 9 | Three diameters × three finishes |
| Reducer | 9 | 100→50, 200→100, 200→50 mm × three finishes |
| Assembled valve | 9 | Three diameters × three finishes, separate handwheel node |
| Valve body only | 9 | Matching bodies without wheels, for independent puzzle entities |
| Handwheel | 3 | One correctly centered wheel per valve size |
| Mount | 9 | Wall/floor/ceiling × three diameters |
| Union flange | 3 | One per diameter |
| Repair coupling | 3 | One per diameter |
| End cap | 3 | One per diameter |
| Pressure gauge | 2 | 80 and 130 mm nominal can diameter, movable needle node |
| Gauge needle | 2 | Separate centered needles |

74,944 triangles summed over all export files, including repeated finish variants and the body-only alternatives. Shared textures: three 1024² base colors, three 1024² ORM maps, one 512² normal, one 512² original gauge face. Steel has localized oxidation; painted finishes have restrained wear and exposed chips.

## Snapping and orientation

`manifest.json` contains each asset's ports, outward port axes, dimensions and moving-part pivots. `gltf_ports` and `gltf_port_axes` are ready for the engine coordinate system; fields ending `_blender` are authoring coordinates. Connection data also appears in the root node's glTF extras.

- Straight pieces start at `(0,0,0)` and end at `(length,0,0)`. Their axis is +X. Place the next section at the preceding end, with opposing port axes.
- Pipe sizes are outside diameters: 0.05, 0.10, 0.20 m. The bores are modeled and visible through open ends.
- Elbows start at the origin, tangent along +X. Their second port is `(1.5D, 0, -1.5D)` in glTF coordinates, tangent along -Z. Rotate around the first pipe's axis to turn upward/downward or against a wall. Centerline bend radius is 1.5 × pipe OD.
- Reducers run along +X from their larger end at the origin. Length is 1.5 × larger OD: 0.15 or 0.30 m. Reverse their placement to expand a line.
- Valves run along +X with port-to-port length 3 × OD: 0.15, 0.30, 0.60 m.
- Flanges and couplings pivot at the pipe center of a butt joint, spanning both sides of it. Couplings have 0.25 mm radial fitting clearance. Valves already include flange hardware.
- End caps pivot at the terminating pipe center and extend along +X. Rotate them for the other end of a run.
- Mount origins are the supported pipe center. Floor and wall versions extend toward local -Y in glTF. Floor mounting plates are at `-(OD/2 + 0.09)` m; wall plates at `-(OD/2 + 0.06)` m. Rotate wall mounts around local X to face the wall. The ceiling version extends toward +Y; its ceiling plate center is `OD/2 + 0.403` m. Clamp bore clearance is 0.4 mm.
- Gauge origin is its threaded stem base, and its stem points +Y. Seat it on the pipe exterior. The dial faces +Z. No transparent glass shader is required.

## Puzzle valves

Two supported workflows:

1. Import `Valve_100_Green.gltf`, preserve its node hierarchy, and rotate the child named `Handwheel` around its local Y axis. Its local origin is centered on the spindle. No animation is baked; game logic controls angle.
2. Place `Valve_100_Green_BodyOnly.gltf`, then place `Valve_Wheel_100.gltf` at the matching `wheel_pivot_gltf` offset, transformed by the body's world transform. Rotate this separate wheel around local Y. Body-only exports do not contain a second wheel.

The equivalent needle node is named `Needle`, with pivot from `needle_pivot_gltf`. Gauge needles rotate around local Z (the stored authoring +Y axis maps to -Z). Initial pointer geometry is already posed; apply a delta rotation from that default. The gauge's printed range is 0–12 bar; it is decorative game art rather than a calibrated instrument.

## Verification

- All exports reimported in Blender: mesh counts, dimensions, triangles and UVs checked.
- Binary glTF indices, nondegenerate triangles, finite/unit normals and tangents, moving pivots, and shared PNG paths validated.
- Closed mesh components checked; hollow pipes have annular rims, not missing shell faces.
- Coplanar overlap scan reports zero overlaps across disconnected components, including moving parts in their default position.
- Contact checks and front/rear/underside renders reviewed. Fasteners intentionally enter the surfaces they attach to. Pipe joins and clamp bores retain normal manufacturing clearances.
- Not tested inside the user's engine. PBR appearance depends on its lighting and reflection probes.

## Sources

`plumbing_reference.blend` contains packed textures. `scripts/` contains the build, texture-map preparation and checks. `reference/approved_concept.png` is the approved design direction. `reference/material_source.png` is the built-in imagegen material source. Printed gauge artwork was drawn specifically for this kit. `reference/prompts.txt` records the image prompt.
