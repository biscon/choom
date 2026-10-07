# Ashdown subway station kit

Revision 3: rebuilt vending machines with deep stocked display cavities, folded/crimped snack packaging, labelled cans and bottles, dispensing spirals, selection/price strips, payment hardware, service details, and worn paint. Revision 2 train, signs and buffer refinements are preserved.

24 individual static glTF 2.0 assets, 101,316 triangles across the complete inventory. Metre scale, midpoly PBR, used teal paint and worn steel. Editable source: `subway_reference.blend`. Concept and generated texture sources/prompts are in `reference/`.

## Import

Copy `exports/gltf/` together with its `textures/` subfolder. Each `.gltf` has a matching `.bin`. All assets reference the same 22 referenced external PNGs; cache textures by resolved file path and compatible colour-space/upload settings to share VRAM. There are no GLBs, animations, scene lights, cameras, collision meshes or room geometry in the exports.

The models use glTF **+Y up**, **+Z front**, +X right. Blender source uses +Z up and -Y front. Materials are standard metallic-roughness PBR, opaque; no transmission/SSR/planar reflections required. Windows are intentionally opaque dusty panes. Vending product displays use visible packet/can geometry against a dark opaque backing, without a transparent glass sheet.

Base colour and emissive textures are sRGB. OpenGL Y+ normal maps and ORM are linear. ORM: R occlusion, G roughness, B metallic; the glTF binds both metallic-roughness and occlusion to the shared ORM image. R is neutral white. No redundant standalone roughness map. Four surface sets at 512², two weathered train sets at 512×1024, two 1024² vending atlases (products and control legends), and two 1024² station print atlases. The old unused Products.png is retained as a legacy source. Several small materials use factors instead of extra image maps.

Exit faces, vending headers and train destination signs use restrained emission strength 0.65. Train headlamp lenses use 1.4 and red marker lamps 0.8. Their housings do not emit. Place any actual illuminating light sources in the engine. The source/render previews use Cycles/AgX and do not validate the engine's ACES exposure or reflection probes.

## Tracks and train placement

- `Track_Straight_8m` and `Track_Straight_16m` have their origin at the **start of the section, on the sleeper bottom**. Repeat along local **glTF -Z**, not +Z. For example place 16 m at `(0,0,0)`, then 8 m at `(0,0,-16)`. Exact lengths are 8 and 16 m; the sleeper pitch remains 0.5 m across joins.
- Two rails, 1.435 m nominal gauge measured between inner rail-head faces. Overall sleeper width 2.35 m. Rail top is **0.330 m above the track-bed origin**. Sections include sleepers and fasteners; create the track bed/ballast in sector geometry as desired.
- The car's origin is the **track-bed centre under its longitudinal midpoint**. Its wheel treads already sit at local Y=0.330 m; do not add another rail-height offset. Use the same vertical placement as the track. Position its midpoint anywhere along the track centreline.
- The car is 16.6 m long at the body and **18.076 m including couplers**, maximum width **3.05 m including steps**, and reaches **3.858 m above the track bed**. Front is +Z. Both ends are finished; windows and doors are closed, with no playable interior. Use sector collision or an engine collision proxy to enforce a tunnel blockage; the glTF does not encode gameplay collision.
- In the reference arrangement the platform top is 1.0 m above the track bed. The train body side is about 1.34 m from centreline, with steps extending to 1.525 m; account for these projections when placing platform edges. Reference geometry is only a placement illustration, not an exported station.
- `Track_Buffer_Stop` shares the track-bed origin convention. Its clamp bottoms sit at 0.325 m and engage the rail heads. Forward is +Z; its small longitudinal base extends -1.04 to +0.26 m in local Z.

## Platform furniture, signs and pivots

Floor furniture uses bottom-centred origins: benches, vending machines and both bins. Bench seating is about 0.47 m high; bench width is 1.834 m. Vending cabinet bodies remain 0.94 m wide and 1.91 m high (0.976 m overall including side vent plates), with the same bottom-centred pivots. Each has 12 selection lanes and 24 products, stocked two deep. The drink machine mixes capped water/tea bottles and pull-tab cans; packets have gussets and crimped seals. Display backing is opaque; no glass transmission is required. The small credit displays emit at 0.35 and internal light strips at 1.4; no scene lights are exported. Static details such as doors, clock hands and call buttons are not separate animated objects.

Wall signs, maps and ad panels have bottom-centred origins with their **rear mounting plane at local Z=-0.052 m**. Offset the origin 0.052 m out from the wall so the back touches it. Clock: rear plane Z=-0.045 m. Intercom: -0.0525 m. Service cabinet: -0.15 m. Exact mesh bounds and offsets are in `manifest.json`.

Suspended signs use the **ceiling mounting plane as origin**. Set that point at ceiling height; the assembly extends downward. The Line 03 assembly hangs 0.81 m, and the Exit sign 0.69 m. Both exit signs are now 0.60 m wide, with closely fitted artwork. Both are readable from either side. Exit arrows point toward local **-X** on both sides of the suspended sign; rotate the whole asset about +Y to choose the exit direction. The wall exit also points -X.

Tactile strips repeat at **1 m or 2 m** along local -Z, with a start-centred surface origin. They are 0.40 m wide and 0.040 m tall including raised studs. Place them at the finished platform surface. Orient them along the platform and position beside the drop.

Fictional route: Rivermoor — Caldwell — **Ashdown** — Brentwood — Fairview. The ads are original SOLTON citrus-soda and NORTH COAST rail-travel artwork. Text lives in shared atlases and can be replaced there.

## Inventory

Dimensions below are glTF X × Y × Z (width × height × depth/length). For the train and buffer, height is mesh extent; track-bed placement is described above. Suspended sign origin is at the top.

| Asset | Triangles | Dimensions, metres |
|---|---:|---|
| `Track_Straight_8m` | 5,016 | 2.350 × 0.330 × 8.000 |
| `Track_Straight_16m` | 9,944 | 2.350 × 0.330 × 16.000 |
| `Subway_Car_Ashdown` | 32,604 | 3.050 × 3.551 × 18.076 |
| `Bench_Three_Seat` | 1,164 | 1.834 × 0.900 × 0.580 |
| `Bench_Backless` | 1,248 | 1.834 × 0.702 × 0.600 |
| `Trash_Bin_Round` | 3,324 | 0.504 × 0.834 × 0.504 |
| `Recycling_Bin` | 408 | 0.494 × 0.881 × 0.451 |
| `Station_Sign_Wall` | 56 | 2.400 × 0.340 × 0.062 |
| `Line_03_Wall` | 56 | 2.300 × 0.360 × 0.062 |
| `Route_Map_Wall` | 56 | 0.640 × 1.100 × 0.062 |
| `Timetable_Wall` | 56 | 0.640 × 1.100 × 0.062 |
| `Advertisement_Solton` | 56 | 1.020 × 1.550 × 0.062 |
| `Advertisement_North_Coast` | 56 | 1.020 × 1.550 × 0.062 |
| `Line_03_Suspended` | 228 | 2.300 × 0.810 × 0.104 |
| `Exit_Sign_Suspended` | 228 | 0.600 × 0.690 × 0.104 |
| `Exit_Sign_Wall` | 56 | 0.600 × 0.240 × 0.062 |
| `Station_Clock` | 908 | 0.582 × 0.582 × 0.144 |
| `Emergency_Intercom` | 324 | 0.240 × 0.460 × 0.120 |
| `Electrical_Service_Cabinet` | 308 | 0.690 × 1.300 × 0.316 |
| `Platform_Edge_Tactile_1m` | 1,164 | 0.400 × 0.040 × 1.000 |
| `Platform_Edge_Tactile_2m` | 2,284 | 0.400 × 0.040 × 2.000 |
| `Track_Buffer_Stop` | 732 | 2.350 × 0.955 × 1.300 |
| `Vending_Snacks` | 13,824 | 0.976 × 1.910 × 0.931 |
| `Vending_Drinks` | 27,216 | 0.976 × 1.910 × 0.931 |

## Verification and source workflow

- All 24 assets exported as selected meshes and reimported individually; geometry counts, bounds and UVs matched.
- Closed component shells, nonzero triangles, finite unit normals/tangents and UVs verified. Long train bevels are segmented to avoid zero tangent weights on very acute triangles.
- Coplanar triangle overlap audit, including same-component pairs: zero detected. Stable projection-axis selection avoids false positives at 45-degree bevels.
- Component surface-contact audit: one connected contact group per asset after correcting window, handle, rim, label and keypad gaps. Rear buffer brace endpoints were additionally checked against their individual shoe footprints. This is geometric contact, not a physical structural simulation.
- Modular track profiles/lengths, mixed-length join pitch, tactile repeat lengths, eight wheel tread heights and floor-furniture pivots checked.
- Front/back/above/underneath and close-up renders were inspected. These checks do not replace testing lighting, collision and depth precision in the engine.

Reports: `mesh_validation.json`, `surface_audit.json`, `contact_audit.json`, `modular_validation.json`, `export_validation.json`, `roundtrip_validation.json`. Final previews are `renders/01_station.png` through `23_vending_low.png`; `01_station_concept_match.png` is an earlier material review.

`ASHDOWN SUBWAY` contains the source assets and inspection setup; originals occupy local export coordinates and are hidden from renders while not under inspection. `ASHDOWN Station reference` contains linked instances and the simple reference room. The unrelated starting scene is preserved.

Rebuild only in a fresh Blender session: `scripts/build_all.py` calls the staged source scripts through Blender Python, schedules renders, and saves the source. It refuses to overwrite an existing kit scene. Maps must already exist; `scripts/make_maps.py` recreates the base sets from retained generated sources using Pillow/numpy and deterministic sign artwork. Run `scripts/make_vending_controls.py` to regenerate the control atlas; the retained `reference/vending_product_art_source.png` is resized to 1024² for `VendingProducts.png`. Run `scripts/make_revision_maps.py` afterward to restore the final imagegen-weathered signage and train maps. `scripts/rebuild_assets.py` is for an active build namespace only, preserves linked instance placement, and regenerates our meshes (discarding manual kit edits). Do not run it blindly on an edited source.

After exporting, run `scripts/audit_surfaces.py`, `scripts/verify_exports.py`, then `scripts/finalize_docs.py` with system Python. Blender-side inspection/check scripts run in the shared build namespace. No generated source image is required at a machine-specific external path; final textures are packed into the saved .blend as well as kept in `textures/`.
