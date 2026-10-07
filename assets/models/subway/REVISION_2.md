# Revision 2

- Train: both cab ends rebuilt with trapezoidal inset windscreens, gasket lock strips, linked wipers, drip rails, door hinges/latch, grab rails and mounts, service panels/louvres, recessed lamp assemblies, destination-board brow, anti-climber ribs, treaded step, slotted lower apron, knuckle-style coupling, air hoses and roof horns. Body panel laps and fasteners added. Train now 32,604 triangles (previously 18,228).
- Dedicated worn TrainSteel / TrainTeal base colour, OpenGL Y+ normal and ORM maps, each 512×1024. Vertical UV mapping keeps rust/grime streaks vertical along the body. Darker opaque windows. Original generic steel maps retained for other station props.
- Train dimensions: 18.076 m over couplers, 3.05 m maximum width including steps, 3.858 m maximum height above track bed including horns. Track-bed pivot and 0.330 m tread height unchanged.
- Exit signs reduced from 0.80 to 0.60 m wide. UV layout cropped/redrawn around the arrow and text, with consistent world direction from both sides of the suspended sign.
- Signage.png replaced with aligned imagegen-weathered artwork: aged/abraded lettering and enamel surfaces. Update this shared texture in the engine along with the models; material filenames remain stable.
- Buffer shoes extended to 1.30 m, with individual brace feet and fasteners. Rear brace endpoints checked against the actual shoe footprint; both ends are supported.

All 24 glTF/BIN files refreshed against the shared maps. 21 external PNGs total. Geometry validation, 24-asset roundtrip, and texture/tangent checks pass. Coplanar audit reports zero overlaps; contact audit reports one connected contact group per asset. 14 modular/placement checks pass, including separate rear brace-foot checks. Final renders include front and rear cab close-ups, rear buffer detail and reverse exit signage.

The prior Blender source and relevant scripts are retained under reference/pre_revision_2/. No archives or GLB exports were generated.
