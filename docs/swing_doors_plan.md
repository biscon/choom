# Swing Doors And Model Door Assets Plan

## How To Use This Living Plan

This document is the execution plan and compacted investigation record for
adding single-leaf, portal-attached swing doors backed by prepared model
assets. It extends the completed procedural sliding-door system; it does not
replace it.

When executing this plan:

1. Read this document, `AGENTS.md`, and
   `docs/architecture/sector_editor_architectural_principles.md` before making
   changes.
2. Execute only the requested slice. Do not silently combine slices or begin
   the next slice.
3. Set the selected slice to **In Progress** before implementation. On the same
   run, update it to **Completed**, **Partial**, or **Blocked** and append an
   execution-log entry with the date, changes, tests, and any decisions or debt.
4. Treat the contracts and decisions below as the current specification. When
   implementation reveals incorrect assumptions, update the relevant section
   before recording the slice result so this remains the source of truth for a
   fresh-context run.
5. Keep every completed slice buildable and backward-compatible with existing
   procedural doors and old map JSON.
6. Do not modify or delete the three downloaded source pack GLBs. They are
   intentionally retained until the whole feature is accepted and the user
   removes them.
7. Do not perform interactive/xdotool smoke tests. Headless Blender inspection
   and deterministic asset verification are allowed. f3d is installed and be used to render model screenshots for visual evaluation if necessary. Do not claim manual GUI
   verification unless the user actually performs it and reports the result.
8. At the end of every slice run the project checks unless the slice explicitly
   records why one is inapplicable:

   ```sh
   cmake --build cmake-build-debug -j2
   ctest --test-dir cmake-build-debug --output-on-failure
   git diff --check
   git diff --stat
   git status --short
   ```

### Slice state

| Slice | Title | Status | Completed |
|---|---|---|---|
| 1 | Prepare and verify separated door assets | Completed | 2026-08-12 |
| 2 | Add door-asset catalog and backward-compatible authored data | Not Started | — |
| 3 | Add swing kinematics, collision, obstruction handling, and runtime spawning | Not Started | — |
| 4 | Render model leaves/frames with PBR lighting and dynamic shadows | Not Started | — |
| 5 | Add editor authoring, diagnostics, cached 2D footprint, and picking | Not Started | — |
| 6 | Integration hardening, documentation, and acceptance coverage | Not Started | — |

## Goal And Acceptance Criteria

Add regular single-leaf doors which remain anchored to a two-sided portal,
rotate procedurally about a hinge, use a prepared model leaf and optional fixed
frame, and participate in the same interaction, sound, visibility, collision,
lighting, and editor workflows as the existing sliding doors.

Completion requires all of the following:

- Existing procedural `slide_vertical`, `slide_left`, and `slide_right` doors
  load, save, render, animate, collide, and interact without behavior changes.
- A map can select a prepared swing-door style, a uniform fit policy/scale,
  hinge endpoint, swing sector, open angle, and angular speed.
- The leaf and handle rotate together while the frame remains fixed.
- The exact same derived leaf transform drives rendering, bounds, collision,
  dynamic spotlight shadows, and the selected-door runtime debug target.
- Closed swing doors block movement and portal visibility. Partly/open doors
  use their rotated physical OBB for movement and conservatively allow portal
  visibility traversal, matching existing door policy.
- A closing swing door does not rotate through the player; it reverses to its
  open target when the player's cylinder intersects the swept leaf.
- Model fitting is always uniform. No portal fit mode may stretch one axis.
- Missing catalog entries and pending/failed model requests never crash. The
  procedural slab is the visible/collision fallback and diagnostics explain the
  problem.
- The fixed frame and moving leaf use the existing world-model PBR path with
  object-probe/sector fallback lighting, dynamic lights, fog, and environment
  reflections. They do not enter static lightmap geometry.
- Door edits invalidate the 2D topology render cache through the existing
  document-edited path. Steady 2D drawing performs no topology validation,
  loop extraction, index rebuild, triangulation, catalog file I/O, or model
  inspection.
- Prepared assets are reproducible from an in-repo headless Blender tool, use
  clean origins/axes, share external atlas textures, retain attribution, and
  can be verified without the original scene hierarchy at runtime.

## Fixed Scope And Decisions

### Included

- Single-leaf swing doors.
- Optional fixed frames.
- Procedural rotation controlled by the existing door `openFraction` and
  interaction/audio systems.
- Left/right choice represented stably as portal **Start** or **End** hinge,
  using the resolved endpoint A/B ordering. The editor may explain these as
  left/right from the portal front, but serialized data must use `start`/`end`.
- Swing direction represented as **Front** or **Back**, using the existing
  anchor convention where `normal` points front sector to back sector.
- Manual, Fit Width, and Fit Inside uniform scale modes plus an authored scale
  multiplier.
- Existing auto-open and `F` interaction.
- Existing open/close sound IDs.
- Player-cylinder obstruction reversal while closing.
- Model leaf collision using one rotated 2D OBB plus its vertical interval.

### Explicitly deferred

- Double/split doors. The industrial `Metal Door_007` pair and double-width
  `Metal Doorframe_005` demonstrate this case but are not catalogued as a V1
  usable style. Do not force them into a single-leaf collider or stretch a
  single leaf across the double frame.
- Door pushing, crushing, damage, locks, keys, scripts, NPC/pathfinding hooks,
  save-game runtime state, or network replication.
- Exact triangle/convex collision or collision for visible frame/jamb trim.
  Surrounding sector walls remain responsible for the opening boundary.
- Baking model frames or leaves into static lightmaps, baked occlusion, or the
  lightmap source hash.
- Alpha-aware door shadow casting and transparent/glass doors. The inspected
  source materials are opaque.
- A general retained glTF scene graph or general node-animation system.
- Rewriting `SectorEditor.cpp` or broadly refactoring model/door renderers.

### Behavioral defaults

- Existing/newly placed doors remain procedural vertical sliders unless the
  author explicitly selects model visual + swing motion. This preserves old
  editor behavior.
- New model swing doors default to `fit_inside`, scale multiplier `1.0`, hinge
  `start`, swing into `front`, open angle `90` degrees, angular speed `90`
  degrees/second.
- A model visual is valid only with `swing` motion in this plan. Procedural
  visuals may use any existing slide motion or the new swing motion (and supply
  the fallback whenever a model cannot be drawn). Selecting Model in the editor
  switches motion to Swing; the model inspector does not offer slide modes.
- Frames use the same effective uniform scale as their leaf and remain centered
  on the portal midpoint at `openBottom`, shifted by `normalOffset`.
- Fully closed means `openFraction <= kSectorDoorPortalBlockEpsilon`, preserving
  the existing visibility policy. Any meaningful opening stops logically
  blocking portal traversal; the leaf still occludes visually through depth and
  still collides physically at its rotated pose.
- When a swing door's attempted closing step intersects the player, do not push
  the player. Leave that frame's fraction unchanged and set the target back to
  fully open. Existing slide-door closing behavior remains unchanged.

## Compacted Investigation Record

### Current door implementation

- Authored runtime objects use `kind: "door"` and `SectorPlacedDoor` in
  `sources/sector_demo/SectorTopologyMap.h`. The only motion enum values are
  vertical, left, and right slides.
- `ResolveSectorDoorAnchor()` already supplies stable portal endpoint A/B,
  midpoint, front-to-back normal, portal tangent, open bottom/top, portal width,
  and portal height. Authored zero width/height resolves to portal width/height.
- `SpawnPlacedRuntimeObjects()` builds one ECS entity per valid door with door,
  anchor, motion, audio, interaction, procedural render, collider, blocker,
  transform, sector, and baked-object-lighting components.
- `AdvanceSectorDoorMotionSystem()` advances a normalized fraction using a
  full-travel amount and speed. `SectorDoorMotionOffset()` converts that fraction
  to translation. `UpdateSectorDoorDerivedStateSystem()` then updates the world
  transform, OBB collider, vertical interval, and dynamic portal blocker.
- Collision already supports an arbitrary oriented 2D box: player movement is
  tested against `SectorDynamicDoorCollider` tangent/normal axes and a vertical
  interval. Swing doors can reuse this solver by rotating those axes and center
  around a hinge; render triangles must not become gameplay collision.
- Runtime colliders and portal blockers are recollected only when
  `doorSpatialStateChanged` or the cache is uninitialized. Preserve that
  allocation/caching behavior.
- Interaction and auto-open target the anchored portal segment rather than the
  moving slab. This is desirable for swing doors and should not be changed.
- The procedural `SectorDoorRenderer` creates/caches one box mesh per door,
  updates baked probe colors, draws it after static sectors, and supplies its
  meshes to the dynamic spotlight shadow pass.
- The existing PBR world-model renderer already draws both static and dynamic
  model ECS objects with glTF materials, object probes/sector fallback,
  environment lighting, dynamic point/spot lights, fog, and spotlight shadow
  receiving. Model doors should reuse that shader/path rather than duplicate a
  PBR shader inside the procedural door renderer.
- Current dynamic spotlight shadow maps become non-cacheable whenever runtime
  door casters exist. Supplying model-door casters retains that conservative
  behavior and introduces no new cache contract.
- `SectorEditorRuntimeObjectEditingService::MutateSelected()` calls the normal
  topology-document-edited helper and refreshes runtime objects. That helper
  invalidates the derived 2D cache. New door inspector edits must use this route.
- The 2D topology render cache currently stores a closed rectangular door
  footprint derived from resolved width/thickness. Picking consumes cached draw
  data. Swing guides and model-derived dimensions belong in that cache, not in
  expensive steady draw-time topology work.
- Runtime objects are presently an intentionally topology-map-owned category in
  the editor architecture contract. This feature does not need to move them to
  the authoring graph.
- `ComputeSectorLightmapSourceHash()` currently includes static model geometry
  but ignores procedural doors and door UV edits. Model swing-door data must
  remain ignored because leaf and frame remain runtime probe-lit visuals, not
  lightmap receivers or baked occluders.

### Why source packs cannot be used directly

Raylib 6's `LoadGLTF()` visits each glTF node, applies the node's world transform
to mesh vertices/normals/tangents, and then discards names and hierarchy. Its
own source comment states that parent-child relations are applied but the
hierarchy is not retained (`cmake-build-debug/_deps/raylib-src/src/rmodels.c`,
near `LoadGLTF()` lines 5396-5666). The engine's `ModelAsset` consequently owns
flattened meshes/materials/animations and whole-model bounds but no node graph
or source-node-to-mesh mapping.

The source GLBs do contain the structure needed for offline preparation:

- Door leaves are separate parent nodes.
- Handles/knobs are children of their matching leaf.
- Frames are separate nodes, except wooden door 002 which is nested under its
  frame and must be explicitly excluded when exporting that frame.
- There are no skins, bones, or animation clips.
- Leaf object origins are generally at a hinge edge, although their vertical
  component is inconsistent and must not be trusted as the floor origin.
- All three use one opaque material and one embedded 1024x256 RGB PNG atlas.
  Wooden-interior and worn-wooden packs embed byte-identical atlas images.

Downloaded source files and immutable SHA-256 values at investigation time:

| Source | SHA-256 |
|---|---|
| `industrial_metal_doors_pack.glb` | `bd6cdbc1783313e6199cbdb7ebbff48cb8674f1fdf752a0917933f9c04213adc` |
| `wooden_interior_doors_pack.glb` | `1d679dfadb8b5b8d7162eeec816496db6ce05ebae78f7d38b9ca10e983767633` |
| `worn_wooden_doors_pack.glb` | `0c71d08f201a4121546fa51081b70c150eb7ef448e26e11acf2fe101ff735f59` |

All are by
`vanillao03 (https://sketchfab.com/vanillao03)`, licensed CC-BY-4.0. Preserve
the title, author, license, and source URL from each GLB's `asset.extras` in the
generated catalog/attribution file and generated root extras.

### Source dimensions and pairings

Dimensions below are approximate world-space source dimensions after evaluating
the glTF hierarchy. Use the preparation script's measured panel-only values as
the authoritative generated catalog values; do not copy rounded values into
runtime code.

- Wooden/worn standard leaf panel: about `0.756 W x 1.806 H x 0.086 D` before
  handle protrusion.
- Wooden/worn frame outer bounds: about
  `0.897 W x 1.940 H x 0.148 D`.
- Industrial leaves vary: about `0.871-1.021 W x 1.882-2.125 H x 0.103-0.112 D`
  before handle/knob protrusion.
- Industrial single frames are about `1.072 W x 1.998 H`; frame 003 is deeper
  and frame 005 is the deferred double-width frame.
- A blanket 1.5x default is not appropriate. It would make a common wooden
  leaf roughly `1.13 x 2.71` world units. Portal-relative fitting plus an
  authored multiplier is the intended solution.

Use these explicit source-node pairings. Include all descendant mesh objects of
the leaf (panel plus handle/knob) unless noted. Frame is optional.

| Catalog ID | Leaf source node | Frame source node | Notes |
|---|---|---|---|
| `wooden_interior_001` | `Wooden Door_001` | `Wooden Doorframe_001` | — |
| `wooden_interior_002` | `Wooden Door_002` | `Wooden Doorframe_002` | Leaf is nested under frame; split explicitly. |
| `wooden_interior_003` | `Wooden Door_003` | `Wooden Doorframe_003` | — |
| `wooden_interior_004` | `Wooden Door_004` | `Wooden Doorframe_004` | — |
| `wooden_interior_005` | `Wooden Door_005` | `Wooden Doorframe_005` | — |
| `wooden_interior_006` | `Wooden Door_006` | `Wooden Doorframe_005.001` | — |
| `wooden_interior_007` | `Wooden Door_007` | `Wooden Doorframe_009.001` | Paired spatially, not by number. |
| `wooden_interior_008` | `Wooden Door_008` | `Wooden Doorframe_005.002` | Paired spatially, not by number. |
| `wooden_interior_009` | `Wooden Door_009` | `Wooden Doorframe_009` | — |
| `worn_wooden_001` | `Worn Door_001` | — | Leaf-only source style. |
| `worn_wooden_002` | `Worn Door_002` | — | Leaf-only source style. |
| `worn_wooden_004` | `Worn Door_004` | `Worn Doorframe_004` | — |
| `worn_wooden_005` | `Worn Door_005` | `Worn Doorframe_005` | — |
| `worn_wooden_006` | `Worn Door_006` | `Worn Doorframe_006` | — |
| `industrial_metal_001` | `Metal Door_001` | — | Leaf-only source style. |
| `industrial_metal_002` | `Metal Door_002` | — | Leaf-only source style. |
| `industrial_metal_003` | `Metal Door_003` | `Metal Doorframe_002` | Includes `Circle.004`. |
| `industrial_metal_004` | `Metal Door_004` | `Metal Doorframe_003` | Includes `Door Handle.023`. |
| `industrial_metal_005` | `Metal Door_005` | `Metal Doorframe_002.001` | Includes `Circle.003`. |
| `industrial_metal_007` | `Metal Door_007` | — | Export one closed canonical leaf; do not use rotated `.001` duplicate or double frame. |

Deferred double-door source nodes are `Metal Door_007`,
`Metal Door_007.001`, their handle descendants, and
`Metal Doorframe_005`. Preserve the original source GLB so a future plan can
prepare these with a two-leaf contract.

## Target Contracts

### Prepared asset layout

Generate reproducible outputs below `assets/models/doors/swing/` while leaving
the three source pack GLBs in `assets/models/doors/`:

```text
assets/models/doors/
  industrial_metal_doors_pack.glb        # immutable source, retained
  wooden_interior_doors_pack.glb          # immutable source, retained
  worn_wooden_doors_pack.glb              # immutable source, retained
  swing/
    catalog.json
    ATTRIBUTION.md
    doors_wood_base_color.png             # shared by wooden + worn assets
    doors_metal_base_color.png
    <catalog-id>_leaf.gltf
    <catalog-id>_leaf.bin
    <catalog-id>_frame.gltf                # only when the catalog frame is present
    <catalog-id>_frame.bin
```

Use separated `.gltf` + `.bin` outputs instead of one GLB per leaf/frame. The
reason is resource ownership, not preference: every original pack embeds the
same atlas into all of its meshes, while the current model asset manager keys
embedded images by model path and image index. Generating many GLBs would
duplicate identical GPU textures. All prepared glTF files must reference one of
the two shared PNG paths so the existing `ModelAssets` shared-texture cache can
deduplicate them by source path.

Each output obeys this engine-space model convention after raylib reimport:

- `+X` runs from the canonical hinge toward the free edge.
- `+Y` is up.
- `+Z` is the leaf/frame depth axis.
- Leaf origin is `(0, 0, 0)` on the hinge axis at the panel bottom and closed
  depth midplane. The panel occupies approximately `[0, nominalWidth]` in X,
  `[0, nominalHeight]` in Y, and is centered around Z. Handle meshes retain
  their correct placement relative to the panel.
- Frame origin is `(0, 0, 0)` at the frame's bottom center and depth midplane.
- Root transforms reimport as identity. Apply source hierarchy transforms into
  vertices during preparation; do not depend on raylib retaining nodes.
- Preserve UV0, normals, tangents, material assignment, and opaque PBR factors.
  Do not include cameras, lights, armatures, animations, or unrelated pack
  pieces.
- Generated root extras retain source title/author/license/URL and add the
  source pack SHA-256 and preparation-tool version.

`catalog.json` is a versioned asset catalog, not map data. Minimum shape:

```json
{
  "formatVersion": 1,
  "assets": [
    {
      "id": "wooden_interior_001",
      "displayName": "Wooden Interior 001",
      "leafModelPath": "assets/models/doors/swing/wooden_interior_001_leaf.gltf",
      "frameModelPath": "assets/models/doors/swing/wooden_interior_001_frame.gltf",
      "nominalWidth": 0.7558,
      "nominalHeight": 1.8060,
      "nominalThickness": 0.0857,
      "frameOuterWidth": 0.8970,
      "frameOuterHeight": 1.9396,
      "sourcePack": "wooden_interior_doors_pack.glb"
    }
  ]
}
```

The numeric example is illustrative. The tool writes full finite measured
values. `frameModelPath`, `frameOuterWidth`, and `frameOuterHeight` are optional
together. IDs are stable and unique. Paths are repository asset paths using `/`.
Reject unknown versions, duplicate/empty IDs, missing paths, non-positive/non-
finite nominal dimensions, half-present frame metadata, and paths outside
`assets/models/doors/swing/`.

### Authored map data

Extend `SectorPlacedDoor` with small enum/value fields; do not add a new runtime
object kind:

- `visual`: `procedural` (default) or `model`.
- `modelAssetId`: catalog ID; required for an authored model visual to resolve.
- `modelFit`: `manual`, `fit_width`, or `fit_inside` (default `fit_inside` for
  new model selections).
- `modelScale`: positive finite uniform scale/multiplier, default `1.0`.
- `hinge`: `start` (default) or `end`.
- `swingSide`: `front` (default) or `back`.
- `openAngleDegrees`: finite `(0, 170]`, default `90`.
- `angularSpeedDegrees`: finite non-negative degrees/second, default `90`.
- Add `swing` to `SectorDoorMotionType` and JSON motion values.

Backward compatibility and omission rules:

- Missing new fields load exactly as the current procedural door.
- Save default values only when needed to describe a model/swing door; continue
  omitting defaults in the existing serializer style.
- Existing `width`/`height` remain the resolved target aperture used by the
  procedural slab and fit calculation. Existing `thickness`, texture, and face
  UVs remain procedural-visual fields. Existing `openDistance` and `speed`
  retain slide semantics. Do not reinterpret their saved values for swing.
- `angularSpeedDegrees` exclusively controls swing travel. A value of zero
  leaves the door stationary just as zero slide speed does.
- Invalid enum text fails map load with an actionable field error. A syntactically
  valid but unknown `modelAssetId` does not make the map unreadable; runtime and
  editor surface a missing-style diagnostic and use the slab fallback.
- Reject the unsupported cross-field combination `visual: model` with a slide
  motion. Procedural swing is valid and is also the runtime fallback path for a
  missing/pending/failed model leaf.

### Uniform fitting

Given catalog nominal leaf width `W`, height `H`, resolved target door width
`Tw`, target height `Th`, and authored `modelScale` multiplier `M`:

```text
manual:     effectiveScale = M
fit_width:  effectiveScale = (Tw / W) * M
fit_inside: effectiveScale = min(Tw / W, Th / H) * M

actualWidth     = W * effectiveScale
actualHeight    = H * effectiveScale
actualThickness = nominalThickness * effectiveScale
```

Never compute independent X/Y/Z scale. Reject/fallback when any input or result
is non-finite/non-positive. `fit_width` deliberately prioritizes exact width;
if its resulting height exceeds the opening, keep the uniform result and emit a
clear fit diagnostic rather than silently turning it into `fit_inside`.
`fit_inside` may leave side or top gaps when aspect ratios differ. The inspector
must report actual dimensions and gaps/overflow so the author can choose a
better portal or asset. Wide portals require authored wall geometry or the
deferred double-door feature; do not stretch a single leaf.

The catalog's nominal panel bounds drive fit and collision. Handle protrusions
and frame outer trim affect render/receiver bounds but not leaf fit or the
player-blocking OBB.

### Runtime components and transforms

Keep authored data separate from handles. The map stores `modelAssetId`; the
runtime component stores resolved catalog data and `ModelHandle`s requested in
the existing runtime-object asset scope. A focused POD component such as
`SectorDoorModelRender` should contain at minimum leaf/frame handles, effective
scale, nominal/actual dimensions, fixed frame matrix, moving leaf matrix,
visibility/readiness flags, and fallback/diagnostic state. Reserve its component
pool before registration is locked.

Do not create leaf and frame child entities. One logical door remains one ECS
entity. Do not put `Model`, `Mesh`, raw manager pointers, methods, or STL element
pointers in components.

For a canonical leaf whose local hinge is `(0,0,0)` and free edge is `+X`:

1. Resolve the world hinge at endpoint A for `start`, endpoint B for `end`, then
   apply the assembly `normalOffset`.
2. Closed across-direction is portal tangent for `start` and negative tangent
   for `end`. Build an orthonormal closed basis without negative scale so mesh
   winding remains valid.
3. Determine angular sign from geometry, not a hardcoded coordinate guess:
   evaluate the two signed small rotations and choose the sign whose free-edge
   displacement has negative dot with front-to-back normal for `front`, positive
   dot for `back`.
4. `angle = SmootherStep01(openFraction) * openAngleRadians * sign`.
5. Build the leaf matrix as uniform scale + hinge rotation + hinge translation.
   Derive leaf center, tangent/across axis, thickness axis, vertical bounds,
   receiver bounds, and shadow matrix from this same result.
6. Frame matrix remains at portal midpoint/open bottom with portal tangent,
   up, and normal basis, using the same effective uniform scale and
   `normalOffset`.

Expose pure helpers for fit, hinge resolution, swing sign, leaf/frame matrices,
and collider derivation. Unit tests should call these helpers without GPU/window
state. Both vertical and horizontal portals and reversed endpoint order must be
covered.

For runtime fraction advancement, replace the misleading assumption that every
motion's travel is a world translation with an internal resolved travel amount:
slides use linear distance and world-units/second; swing uses angle radians and
radians/second converted from authored degrees. Preserve the normalized
fraction behavior and smoother-step visual/spatial curve.

### Collision and closing obstruction

- The moving leaf uses the existing dynamic OBB collision solver at every
  fraction, including fully open. Fully open means rotated out of the aperture,
  not collision-disabled.
- OBB center and axes come from the swing leaf matrix; half extents are
  `actualWidth/2` and `actualThickness/2`; vertical interval is
  `[openBottom, openBottom + actualHeight]`.
- Procedural slides retain their existing collider derivation and parking
  epsilon.
- Frame trim gets no additional dynamic collider. Static topology around the
  portal owns the opening boundary.
- Extend the scene/runtime update input with an optional POD player obstacle:
  feet position/XZ, current effective radius, and current effective height.
  Game and editor gameplay-preview call sites construct it from their current
  controller state/config; free-fly/no-player updates pass null.
- During a closing swing step, test the player's vertical overlap and circle
  against the swept leaf. Use allocation-free adaptive angular samples no more
  than 5 degrees apart, including candidate pose, with a bounded maximum that
  covers the allowed 170-degree range. If any sample overlaps, keep the previous
  fraction and retarget fully open. This avoids frame-rate-dependent tunneling
  without introducing general swept-convex collision.
- Audio continues to derive events from target transitions, so obstruction
  reversal naturally produces an open transition. Add tests to prevent repeated
  retriggering each blocked frame.
- This changes dynamic swing-door/player collision only. Static topology
  collision, sector lookup, step handling, and general player physics remain
  unchanged.

### Rendering and fallback

- Keep `SectorDoorRenderer` responsible for procedural slab GPU meshes only.
  Skip model visuals in its slab draw/cache/shadow-caster traversal except when
  the model door is explicitly in fallback state.
- Add a focused model-door traversal to the existing world-model PBR renderer.
  Reuse its material/shader upload path with the world-dynamic/object-probe
  lighting policy; make only the small extraction needed to avoid duplicating
  its per-material PBR setup. Do not create a second PBR shader.
- Draw frame with the fixed frame matrix and leaf with the current leaf matrix.
  Request models on spawn/load, never during steady draw. The catalog is parsed
  during explicit scene/runtime-object rebuild, never update/render.
- A door is visibility-eligible when either anchored adjacent sector is visible;
  do not assign it solely to one side and make it disappear when viewed from the
  other.
- Resample the model door's object-probe/sector fallback lighting only when its
  leaf spatial state changes, using the leaf center and the existing collision
  world to prefer the containing adjacent sector. The fixed frame may share the
  door sample in V1. Avoid per-frame allocation.
- Extend door receiver bounds to union transformed leaf and ready frame model
  bounds. Until assets are ready, use analytic leaf/frame bounds from catalog.
- Extend the dynamic spotlight door-caster context so it can draw every opaque
  mesh in leaf/frame model assets with their matrices while preserving the
  procedural slab resolver. Any active door caster continues to invalidate the
  spotlight shadow cache as it does now.
- Pending or failed catalog/model states render the existing procedural slab
  with resolved actual/target dimensions and the authored procedural texture or
  default material. Missing frame alone does not fail the leaf. Record one
  actionable warning rather than logging every frame.

### Editor and cache behavior

- Add a focused style selector from the loaded swing-door catalog; do not use
  the generic single-model picker because a door style resolves leaf + optional
  frame + nominal bounds.
- Door inspector rows:
  - Visual: Procedural / Model
  - Model Style and readiness/fit diagnostic when Model
  - Fit and Scale when Model
  - Motion adds Swing
  - Hinge, Swing Into, Open Angle, Angular Speed when Swing
  - Open Distance and linear Speed only for slide modes
  - procedural texture/face UV controls only for Procedural visual
  - existing auto-open, use distance, sounds, initial fraction, runtime target,
    and delete remain available
- Keep Width/Height as target aperture controls, relabeling if necessary to
  avoid implying non-uniform model deformation. Show portal opening, effective
  scale, actual leaf dimensions, and side/top gap or overflow.
- Extend cached 2D door data with actual closed leaf rectangle, hinge point,
  open free-edge/end pose, and swing arc parameters. The solid closed leaf is
  the pick target. The arc/open outline is a clearly styled non-pickable guide.
  Picking therefore remains consistent with the solid shape presented as the
  authored object.
- Cache building may consult the already-loaded catalog passed explicitly from
  the editor/runtime state. It must not read/parse catalog files or query GPU
  model bounds. Missing catalog metadata uses resolved authored dimensions and
  marks the cached object invalid/missing.
- Every authored door mutation must go through
  `SectorEditorRuntimeObjectEditingService::MutateSelected()` or the matching
  existing add/delete service path. This marks the topology document edited,
  invalidates the 2D topology render cache, and refreshes preview ECS objects.
  Catalog reload/change must also invalidate the cache. Runtime debug open/close
  changes ECS state only and does not dirty the map or 2D cache.

### Lightmap source-hash policy

Model door leaf/frame models remain runtime visuals and are excluded from:

- static lightmap receiver chart preparation,
- baked ray occluders,
- static-model geometry sidecars,
- the lightmap source hash.

Therefore `visual`, catalog ID, fit, scale, hinge, swing side, angle, and model
paths must not affect `ComputeSectorLightmapSourceHash()`. Add explicit hash
coverage to lock this behavior. If a future task bakes fixed frames, it must add
frame path, geometry fingerprint, scale, transform, and receiver/occluder policy
to the source hash; that is not part of this plan.

## Implementation Slices

### Slice 1 — Prepare And Verify Separated Door Assets

**Intent:** establish a deterministic asset contract before any engine schema or
runtime code depends on it.

Implement `tools/prepare_swing_door_assets.py`, following the useful conventions
in `tools/prepare_pistol_asset.py` without coupling the scripts. It must run
under Blender:

```sh
blender --background --factory-startup \
  --python tools/prepare_swing_door_assets.py -- --mode inspect
blender --background --factory-startup \
  --python tools/prepare_swing_door_assets.py -- --mode prepare
blender --background --factory-startup \
  --python tools/prepare_swing_door_assets.py -- --mode verify
```

Also support `--mode all` and an overrideable work directory defaulting below
`build/swing_door_asset_work/`. Blender may print the known optional extension
module/PipeWire warnings; the script must still fail nonzero on its own
validation errors.

Required implementation:

- Encode the source hashes and explicit pairing table above. Verify hashes both
  before and after preparation so sources cannot be overwritten accidentally.
- In inspect mode, parse GLB metadata, enumerate hierarchy/material/image data,
  measure panel-only and full leaf/frame evaluated bounds, and write
  machine-readable JSON under the work directory.
- Extract the two shared atlas PNGs and assert wooden/worn embedded image bytes
  are identical. Give generated glTF files stable relative references to them.
- For every table row, isolate only the configured leaf descendants and optional
  frame descendants. Explicitly separate nested wooden 002. Fail if source node
  names, mesh counts, materials, or expected descendant relationships drift.
- Canonicalize axes/origins according to the prepared-asset contract. Use the
  panel mesh rather than handle protrusions to determine hinge, bottom,
  nominal width/height/thickness. Preserve evaluated child placement before
  removing wrappers. Apply transforms before export.
- Export one leaf and optional frame as `GLTF_SEPARATE`, with shared external
  texture URI, UV0, normals, generated/preserved tangents, materials, extras,
  and no animations/cameras/lights.
- Generate sorted deterministic `catalog.json` and `ATTRIBUTION.md` from source
  metadata. Re-running prepare without source changes must produce byte-stable
  JSON/text and equivalent assets; avoid timestamps in generated files.
- Verify by clearing Blender and reimporting every generated model. Assert clean
  root transforms, positive dimensions, canonical hinge/bottom bounds within a
  small documented tolerance, expected mesh/material counts, UVs/normals/
  tangents, opaque material, shared external texture URI, no unrelated scene
  objects, and exact catalog/path agreement.
- Produce front and top-down headless verification contact sheets under the
  build work directory showing every leaf closed, partly open around its origin,
  and aligned with its optional frame. These are diagnostic build artifacts,
  not tracked assets. Record whether they were inspected; do not call that an
  interactive game smoke test.

Acceptance:

- All 20 single-leaf catalog rows prepare and reimport successfully.
- Each leaf handle/knob remains attached when a verification transform rotates
  the leaf about origin.
- Frames do not contain leaf meshes; leaves do not contain frame meshes or
  unrelated pack variants.
- The source hashes remain unchanged and source GLBs remain present.
- Generated assets reference only the two shared PNGs and do not duplicate
  embedded textures.
- The plan state/log is updated with exact generated counts, any pairing changes,
  validation commands, and contact-sheet paths.

### Slice 2 — Door Catalog And Backward-Compatible Authored Data

**Intent:** make prepared door styles and authored swing/model choices available
as validated CPU data without enabling incomplete runtime behavior.

Required implementation:

- Add the enum/field contract above to `SectorPlacedDoor` and JSON
  serialization/validation/default omission.
- Add a small `SectorSwingDoorCatalog` module under `sources/sector_demo/` with
  POD asset records, stable-ID lookup, strict parser, and diagnostics. No
  polymorphism, global singleton, raw STL pointers, or generic asset-registry
  abstraction.
- Load `assets/models/doors/swing/catalog.json` during explicit scene/runtime
  object rebuild and retain it in `SectorRuntimeObjectState`. Do not perform
  catalog I/O in `UpdateSectorRuntimeObjects()`, renderer draw, or 2D draw.
- Validate catalog structure/path constraints and surface one stable warning on
  failure. A failed catalog must not prevent procedural doors or unrelated
  runtime objects from spawning.
- Add pure fit calculation helpers implementing the exact formulas above and
  returning actual leaf dimensions plus diagnostic status.
- Until Slice 3, model/swing records must fail closed into an explicit
  procedural-slab fallback/diagnostic rather than accidentally using a slide
  transform. Keep the source buildable at this boundary.
- Update exhaustive switches and ECS component-type reservation as needed; do
  not create model GPU resources in this slice.

Tests:

- Old door JSON without new fields loads exact old defaults and saves without
  schema churn.
- Every new enum/string and non-default value round-trips; invalid strings and
  invalid numeric ranges fail with field-specific errors.
- Unknown catalog ID remains valid map data but produces a lookup diagnostic.
- Catalog accepts the generated file and rejects duplicate IDs, bad versions,
  invalid dimensions, escaping paths, and partial frame metadata using
  temporary test JSON/fixtures outside user-edited level assets.
- Manual/Fit Width/Fit Inside calculations are uniform and cover invalid input,
  wide/short portal overflow, and a normal matching portal.
- Changing any new door/model/swing field leaves the lightmap source hash
  unchanged.

### Slice 3 — Swing Kinematics, Collision, Obstruction, And Runtime Spawning

**Intent:** make swing motion physically correct and safe, with the procedural
slab fallback providing a visible stand-in before model rendering lands.

Required implementation:

- Add/reserve the model-door runtime component and request catalog leaf/optional
  frame handles in the existing runtime-object asset scope during spawn. Store
  handles in ECS only; asset resources remain owned by `AssetManager`.
- Resolve effective scale and actual dimensions once per explicit spawn/refresh.
  Missing/invalid catalog or null handles selects fallback and records a stable
  diagnostic without skipping the door entity.
- Implement pure canonical leaf/frame matrix helpers and resolved motion travel
  described above. Keep existing slide motion/parking epsilon unchanged.
- Update derived state from one canonical swing pose: entity transform, model
  leaf/frame matrices, collider center/axes/vertical interval, receiver fallback
  bounds, and blocker state.
- Keep one OBB collider active through the full open range. Collection and
  movement resolution continue using `SectorDynamicDoorCollider`.
- Add optional player-obstacle input through scene runtime, game session, and
  editor preview update call sites. Implement bounded <=5-degree adaptive sweep
  sampling and reopen-on-obstruction only for closing swing doors.
- Recollect dynamic colliders/blockers on any swing fraction or obstruction
  target/spatial change, with existing pre-reserved vectors and no steady-frame
  allocation.
- Keep interaction point/facing tests portal-based and retain existing auto-open
  and `F` target behavior/audio transitions.

Tests in `SectorRuntimeObjectTests.cpp` and focused collision/visibility tests:

- Start/end hinges remain fixed at all fractions for vertical, horizontal, and
  reversed portals.
- Front/back swing choices move the leaf center to the requested side.
- Closed, half-open, and open render matrices and collider OBBs agree on center
  and axes.
- A fully open leaf still collides where physically present but clears the
  aperture for a standard fitted door.
- Closed blocks portal traversal; epsilon-open and partially open retain the
  existing conservative unblock behavior.
- Player outside sweep allows closing; player in current/candidate/intermediate
  swept pose reverses target and prevents fraction change; vertical non-overlap
  permits closing; large `dt` cannot tunnel through the player.
- Obstruction reversal generates one target/audio transition, not one per frame.
- Slide transforms, collision fixtures, auto-open, interaction, and audio tests
  remain unchanged.
- Collision/sector lookup/general physics code outside the dynamic door layer is
  unchanged; record this explicitly in the slice log.

### Slice 4 — PBR Model Rendering And Dynamic Shadows

**Intent:** replace the fallback slab with the prepared leaf/frame models when
ready while retaining all current lighting and failure behavior.

Required implementation:

- Add a model-door traversal to the existing PBR world-model renderer. Extract
  only the small shared material draw helper necessary to use the same shader,
  material maps, dynamic lights, static specular selection, fog, environment,
  and object-probe policy for static/dynamic props and model doors.
- Draw all ready frame meshes with fixed matrix and leaf meshes with current
  swing matrix. Use adjacent-sector visibility eligibility.
- Skip the procedural slab only when the model leaf is ready and valid. Pending,
  failed, missing, or invalid leaf continues drawing the fallback. A missing
  optional/declared frame never hides a ready leaf.
- Finalize model resources through the existing explicit main-thread asset
  update/finalization path; never request/load during draw.
- Update/resample model-door lighting only on spawn or door spatial changes,
  without allocating during the steady frame.
- Extend receiver bounds and dynamic spotlight shadow caster records for all
  model meshes. Preserve the procedural caster route and the existing
  non-cacheable-while-doors-exist shadow policy.
- Do not add alpha-tested shadows, static lightmap charts, baked door occlusion,
  or render-triangle collision.

Tests/verification:

- CPU tests cover adjacent-sector visibility eligibility, transformed bounds,
  fallback state transitions, and model-caster matrices without requiring a
  window.
- Pending/failed/null leaf and frame handles do not crash and choose documented
  fallbacks.
- Existing PBR policy tests and procedural door mesh/shadow tests pass.
- Build-time shader validation target, if present in the configured build,
  remains green.
- User performs the eventual in-engine visual smoke test; the agent only records
  automated/headless verification unless the user reports results.

### Slice 5 — Editor Authoring, Diagnostics, Cached 2D Draw, And Picking

**Intent:** make swing/model doors fully authorable without direct topology
mutation or steady-frame derived work.

Required implementation:

- Add conditional inspector controls and status text exactly as specified in
  the editor contract. Reuse existing UI patterns and the runtime object editing
  service; do not add one callback per operation or route through broad
  `SectorEditor` wrappers.
- Populate the style selector from the already-loaded catalog in stable display
  order. Selecting the first model style initializes model/swing defaults but
  does not overwrite existing interaction, sound, target width/height, or
  initial fraction.
- Display source nominal dimensions, effective scale, actual dimensions,
  portal dimensions, and explicit side/top gap or overflow diagnostics.
- Extend inspector measurement/layout helpers and focused UI layout tests so
  conditional rows do not overlap or clip.
- Extend topology render-cache build data for exact catalog-resolved closed leaf
  footprint, hinge, open guide, and swing arc. Pass the loaded catalog
  explicitly; never parse files or query `AssetManager` from cache build/draw.
- Draw a distinct hinge marker and front/back swing arc/open outline. Keep the
  solid closed rectangle as the pickable authored footprint; test picking at
  both hinge orientations and ensure the guide alone is not a pick hit.
- Door add/delete and all authored field changes must call the existing
  document-edited/cache invalidation route and refresh preview ECS state.
  Runtime target open/close remains non-authored and does not invalidate.
- Catalog reload/change invalidates the 2D cache. Texture changes still require
  no door-cache invalidation unless cached 2D data begins storing texture state.

Tests:

- Inspector row/height calculation for procedural slide, procedural swing, and
  model swing conditions; model visual must not expose slide choices.
- Editing each new field updates map data, marks dirty, invalidates the cache,
  and refreshes the runtime entity while preserving unrelated door fields.
- Cached footprint uses catalog actual dimensions and correct Start/End hinge;
  missing catalog falls back/marks invalid deterministically.
- Picking matches the cached solid closed footprint at multiple zoom levels.
- Existing procedural door texture and UV modal behavior remains available and
  unchanged for procedural visuals.

Cache note required in the slice report: enumerate the mutation paths touched
and confirm they all use `MarkSectorEditorTopologyDocumentEdited()` via the
runtime-object editing service; state that over-invalidation is retained.

### Slice 6 — Integration Hardening, Documentation, And Acceptance Coverage

**Intent:** close cross-subsystem gaps after all feature paths exist; do not add
new product scope.

Required implementation:

- Audit all switches/defaults, ECS reservations, spawn/reset/shutdown paths,
  asset-scope unload, diagnostics counters, renderer reset/shutdown, shadow
  cache invalidation, selection refresh, and serialization tests for both visual
  types and all motion types.
- Strengthen focused tests for mixed maps containing procedural sliders and
  model swing doors, repeated runtime-object refresh, pending-to-ready asset
  transitions, invalid style recovery, initial partial-open fraction, auto-open,
  manual interaction, and scene shutdown/rebuild.
- Verify no normal update/render path performs file I/O, asset requests, model
  loads, vector growth caused by missing reserve calls, topology validation,
  loop extraction, index rebuild, or triangulation.
- Update `docs/sector_editor.md` from its current statement that glTF/hinged
  doors are unsupported. Document authored JSON defaults, fit behavior,
  collision/obstruction behavior, fallback behavior, and current limitations.
- Re-run the asset verifier and all project checks. Record the user-facing
  manual smoke checklist below without claiming it was executed.
- Update this plan's state and execution log. Do not delete the source pack
  GLBs; the user will decide when to remove them after acceptance.

Manual checklist for the user:

1. Place a model swing door on normal-width portals of different orientations.
2. Try Start/End hinge and Front/Back swing choices and inspect the 2D arc.
3. Compare Manual, Fit Width, and Fit Inside; confirm no visible axis stretch.
4. Open/close with runtime inspector target, `F`, and auto-open.
5. Walk through an open doorway and collide with the open leaf from the side.
6. Stand in the closing sweep and confirm the door reopens without pushing or
   tunneling through the player.
7. View from both connected sectors; confirm frame stays fixed and the leaf,
   handle, lighting, fog, environment response, and dynamic spotlight shadows
   look correct.
8. Save/reload, rebuild preview, and revisit existing procedural sliding-door
   maps.
9. Temporarily select a bad/missing model style/path and confirm a stable
   fallback/diagnostic rather than a crash or invisible blocker.

Final report requirements:

- Explicitly state that topology-map runtime-object edits invalidate the 2D
  cache through the normal document-edited path.
- Explicitly state that model door fields/assets remain excluded from the
  lightmap source hash and that doors are probe/dynamic-lit runtime visuals.
- Explicitly state that only dynamic swing-door collision changed; static
  topology collision, sector lookup, and general player physics did not.
- List automated commands actually run and keep manual verification identified
  as user-owned unless results were reported.

## Cross-Slice Test Matrix

| Area | Required coverage |
|---|---|
| Asset preparation | Source hashes, hierarchy drift, pair isolation, canonical bounds/origin, shared texture references, material/UV/normal/tangent preservation, attribution, deterministic catalog. |
| Serialization | Old defaults, default omission, every new enum/value, invalid numeric/string data, unknown catalog ID behavior. |
| Fit | Manual, width, inside, multiplier, impossible aspect ratio warning, invalid/non-finite data, no non-uniform scale. |
| Kinematics | Start/end, front/back, horizontal/vertical/reversed portal, 0/0.5/1 fraction, hinge invariance, angle/speed zero. |
| Collision | Closed/aperture/open-leaf OBB, vertical overlap, swept obstruction, large dt, no player push, existing slides unchanged. |
| Visibility | Closed blocks, epsilon-open unblocks, either adjacent sector draws model door. |
| Assets/runtime | Catalog missing/invalid, null/pending/failed/ready leaf, optional frame missing/ready, repeated refresh and scope unload. |
| Rendering policy | PBR world-model path, object-probe fallback, fog/environment/dynamic light receiving, true model dynamic shadow caster, slab fallback. |
| Editor/cache | Conditional layout, style diagnostics, mutation dirty/invalidation, cached footprint/arc, pick agreement, catalog reload invalidation. |
| Lightmaps | All new model/swing fields leave source hash unchanged; no model door charts/occluders. |

Tests must use generated maps, temporary JSON, pure catalog records, or dedicated
immutable fixtures outside `assets/levels` and `assets/sector_demo`. Do not make
C++ tests depend on the user's editable levels or on a particular generated
door asset being present unless the test is explicitly the separate asset-tool
verification.

## Execution Log

Append one entry per attempted slice in this format:

```text
### YYYY-MM-DD — Slice N — Completed|Partial|Blocked

- Summary:
- Decisions/deviations folded back into plan:
- Files/modules materially affected:
- Automated verification:
- Manual verification: Not performed (user-owned), unless reported otherwise.
- Cache invalidation behavior:
- Lightmap source-hash behavior:
- Collision/sector lookup/physics behavior:
- Remaining follow-up within this plan:
```

### 2026-08-12 — Slice 1 — Completed

- Summary: Added a deterministic Blender inspection/preparation/verification
  tool and generated all 20 catalogued single-leaf styles as 20 separated leaf
  models plus 15 optional separated frame models. The 35 glTF files reference
  only two shared external 1024x256 atlas PNGs. Generated the version-1 catalog
  and source-derived CC-BY-4.0 attribution, retained all three immutable source
  GLBs, and confirmed their recorded SHA-256 values before and after each tool
  run.
- Decisions/deviations folded back into plan: No pairing changes or contract
  deviations were needed. Wooden door 002 is explicitly excluded from its
  frame export, and only the closed canonical `Metal Door_007` hierarchy is
  exported; its rotated duplicate and double-width frame remain deferred in the
  unchanged source GLB. Blender 5.2 emits the known optional extension-module,
  MeshOptimizer, and PipeWire diagnostics, but the tool's validations and exit
  status remain authoritative.
- Files/modules materially affected: `tools/prepare_swing_door_assets.py`,
  `assets/models/doors/swing/`, and this plan. Generated output consists of 20
  leaf `.gltf`/`.bin` pairs, 15 frame `.gltf`/`.bin` pairs, two shared PNGs,
  `catalog.json`, and `ATTRIBUTION.md`.
- Automated verification: Blender `inspect`, `prepare`, and `verify` modes all
  passed; verify cleared and reimported every generated model and checked clean
  roots, canonical bounds, mesh isolation/counts, opaque materials, UV0,
  normals, tangents, shared texture URIs, provenance extras, catalog agreement,
  and attachment rotation. A second `prepare` run followed by a SHA-256 manifest
  comparison was byte-identical. `cmake --build cmake-build-debug -j2` passed;
  `ctest --test-dir cmake-build-debug --output-on-failure` passed all 21 tests;
  `git diff --check`, `git diff --stat`, and `git status --short` passed/reported
  cleanly as recorded at handoff.
- Manual verification: No interactive/game smoke test was performed
  (user-owned). The agent visually inspected the headless front and top-down
  contact sheets at
  `build/swing_door_asset_work/contact_sheets/swing_doors_front.png` and
  `build/swing_door_asset_work/contact_sheets/swing_doors_top.png`; all 20 styles
  show closed and 55-degree poses around the same hinge marker, attached
  handles/knobs, and fixed optional frames.
- Cache invalidation behavior: No topology, editor mutation, or 2D render-cache
  code changed in this asset-only slice, so no cache invalidation path changed.
- Lightmap source-hash behavior: No lightmap or runtime schema code changed;
  prepared model assets remain unused by and excluded from the lightmap source
  hash until later explicitly scoped slices.
- Collision/sector lookup/physics behavior: Unchanged; this slice added no
  runtime, collision, sector lookup, or physics code.
- Remaining follow-up within this plan: Slices 2 through 6 remain Not Started.
  The three source pack GLBs remain present for the user to remove only after
  the complete feature is accepted.
