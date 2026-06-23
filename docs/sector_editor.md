# Sector Editor

The sector editor works on `SectorTopologyMap` topology v2 documents. It is a
Doom-like topology editor built around vertices, linedefs, sidedefs, sectors,
map-local texture definitions, static baked lights, lightmap settings, and
optional baked lightmap metadata.

The editor has a 2D authoring mode for topology creation and inspection, plus a
3D preview/edit mode for checking generated geometry and editing surface texture
and UV settings.

## File Layout And JSON Format

Levels use one directory per level:

```text
assets/levels/<level_name>/<level_name>.json
assets/levels/<level_name>/<level_name>.lightmap.png
```

Level names may contain only letters, digits, underscores, and dashes.

Current level JSON is topology v2:

```text
formatVersion: 2
topology: "linedef"
coordSubdivisions: 16
textures
vertices
linedefs
sidedefs
sectors
staticLights
lightmapSettings
bakedLightmap
```

`bakedLightmap` is written only after a successful bake has installed valid
metadata. The topology format does not store parent sector hole arrays; holes and
adjacency are implied by sidedefs around linedefs.

## Coordinates

Topology planar coordinates are stored as integer `SectorCoord` values. The
document uses `coordSubdivisions = 16`, so visible authoring X/Y coordinates are
converted to and from stored integer coordinates at 16 subdivisions per authoring
unit.

Sector floor/ceiling heights and static-light Y/radius/sourceRadius values
remain authoring floats. Geometry generation, 3D preview, and lightmap baking
convert authoring units to world units at their boundaries. The current world
scale is 8 authoring units per world unit.

Map X is horizontal in 2D. Map Y is the top-down second planar axis in authoring
views and maps to world Z for generated 3D geometry.

## 2D Controls

- `WASD`: pan the 2D view.
- Mouse wheel over the canvas: zoom.
- Select tool: click static lights first, then topology linedefs/sidedefs, then
  sectors. Clicking empty canvas clears selection.
- Sector tool: draw topology sectors. Finalized sectors reuse exact existing
  vertices and exact endpoint-pair linedefs when possible.
- Light tool: place a topology static baked light inside the clicked sector.
- Move tool: drag topology vertices by stable vertex ID, merge vertices by
  dragging exactly onto an existing vertex, or drag static lights by X/Z.
- Erase tool: click a sector to confirm deletion. Light deletion is available
  from selection/Delete.
- Click the first pending point or press `Enter`: finalize the pending sector.
- `Backspace`: remove the last pending point.
- Right click or `Escape`: cancel a pending sector, active vertex move, or active
  light move.
- `Delete`: delete the selected light, or confirm deletion for the selected
  sector. Direct linedef and sidedef deletion is not available yet.
- `Escape`: clear selection, then return to Select tool.
- `New`: confirm and reset to a blank topology level.
- `Load`: select a level under `assets/levels`; unsaved edits require
  confirmation before loading.
- `Save`: save the current topology document, or save as a named level.
- `Reload`: confirm and reload the current saved level.
- `Add Map Texture`: add or update a texture entry from a PNG under
  `assets/images`.
- `Bake Lightmaps`: bake topology static lights into the level lightmap atlas.
- `3D Mode`: rebuild the 3D preview from the current in-memory topology map.

## Topology Model

- Vertex: a stable positive integer ID plus an exact planar `SectorCoord`
  coordinate.
- Linedef: a physical line between two vertices, directed from start vertex to
  end vertex.
- Sidedef: a directed sector-owned side of a linedef. The front side follows the
  linedef start-to-end direction; the back side follows end-to-start. The owning
  sector lies to the left of the directed side.
- Sector: stores sector properties and is bounded by its sidedefs.
- Two-sided linedef: a portal/adjacency between two sectors.
- One-sided linedef: a solid boundary.

Two-sided portals generate lower and/or upper wall surfaces only where adjacent
sector heights differ. Equal-height two-sided portals remain visible and
editable in 2D, but produce no 3D wall surface to pick or texture in 3D.
Topology sidedefs can also store optional middle texture data for Doom-style
masked portal surfaces. A sidedef middle texture renders only on a two-sided
portal linedef, fills the visible opening from the higher floor to the lower
ceiling, and uses alpha testing: transparent pixels are discarded rather than
alpha-blended. Middle textures do not block by themselves; use the linedef
`Blocks Player` flag when a see-through grate, bars, or window should block
Gameplay movement.

## Sector Collision Query Layer

Gameplay/collision work now has a reusable `SectorCollisionWorld` query layer
built from the topology map. It derives collision data from 2D sector loops,
one-sided boundaries, two-sided portal adjacency, and sector floor/ceiling
heights.

The collision query layer does not use generated render triangles and does not
use Recast/Detour. It stores sector boundary loops in world X/Z coordinates for
point-in-sector lookup, classifies one-sided edges as blocking walls, and
classifies two-sided edges as portals with neighbor sector IDs.

Gameplay preview uses this query layer for current-sector floor/ceiling lookup,
horizontal player-cylinder collision, simple gravity, landing, step/drop floor
transitions, and ceiling clamp. Topology sector heights remain authored JSON values, but
collision floor/ceiling heights are converted to the same runtime world-space Y
units as rendered geometry.

Gameplay horizontal collision is based on 2D topology plus sector
floor/ceiling heights, not generated render meshes. One-sided edges block
movement. Two-sided portal edges are passable only when the destination sector
allows the current cylinder height and grounded upward steps are within the
configured step height. Small upward and downward floor differences within step
height snap immediately; larger downward drops start falling under gravity
instead of teleporting to the lower floor. A two-sided portal linedef with
`Blocks Player` enabled behaves like a blocking wall for Gameplay movement,
regardless of portal height passability. Middle textures do not independently
block movement or add collision; middle texture plus `Blocks Player` is the
intended grate/window/barrier workflow. One-sided walls already block. Projectile,
sight, and monster blocking flags remain deferred.

## Sector Inspector

Selecting a topology sector opens the sector inspector. It edits:

- sector name and stable integer ID display
- floor and ceiling heights
- floor and ceiling texture IDs and UV scale/offset
- ambient color and intensity
- default wall, lower, and upper texture IDs and UV scale/offset
- `Insert Sector Inside`
- `Cut Sector`
- `Delete Sector`

Sector default wall/lower/upper settings initialize future sidedefs created for
that sector. Editing those defaults does not rewrite existing concrete sidedefs.

Sector deletion is transactional. It removes sidedefs owned by the sector,
clears those slots from linedefs, removes linedefs with no remaining side, prunes
unreferenced vertices, validates the candidate topology, and commits only if it
is valid. Surviving opposite sides on shared linedefs keep their texture and UV
settings and become one-sided boundaries.

## Cut Sector

`Cut Sector` starts a pending two-click canvas action for the selected topology
sector. Click two points on the selected sector's outer boundary, or press
Escape/right click to cancel. Picking prefers existing selected-boundary
vertices near the cursor, then snapped points strictly inside selected outer
linedefs. Holes and unrelated linedefs are not valid endpoints.

The cut is topology-only and transactional. The original sector ID, name, and
properties are preserved for one result. One new sector ID is allocated for the
other result, copying the original sector fields and receiving a generated
non-duplicate sector name. Preview validation runs against a copied map and does
not mutate live topology.

Accepted cuts must stay inside the selected sector and produce two valid outer
loops with at least three vertices each. The editor/backend rejects same
endpoints, same-edge cuts, boundary-aligned cuts, cuts through unrelated
vertices, duplicate physical linedefs, crossing/touching/overlapping existing
topology, concave cuts outside the sector, hole crossings/touches, ambiguous
hole assignment, and any candidate that fails topology validation.

For now, cut endpoints must be simple one-sided boundary points on the selected
sector. Endpoints on shared portal boundaries, including equal-height portals,
are rejected; pick a solid outer boundary point instead.

Existing boundary sidedefs inherited by each result keep their concrete
wall/lower/upper texture IDs and UV settings. Existing holes are not cut: each
hole boundary must lie strictly inside exactly one result sector, or the cut is
rejected. The new cut edge is one two-sided portal linedef with independent
front/back sidedefs initialized from the two owning sectors' default wall,
lower, and upper settings. Like other two-sided portals, it produces lower/upper
3D wall surfaces only where adjacent sector heights differ.

## Join Sectors

Adjacent topology sectors can be joined through a selected two-sided portal.
Select the sidedef/portal boundary, then use `Join Sectors` in the
sidedef/linedef inspector. The selected side's sector is the winner: it keeps
its sector ID, name, floor/ceiling heights, floor/ceiling textures and UVs,
ambient settings, and default wall/lower/upper settings.

Join is topology-only and transactional. It removes the shared internal portal
boundary, reassigns surviving outside sidedefs from the removed sector to the
winner, prunes orphan vertices, validates the candidate topology, extracts the
surviving sector loops, and commits only if the result is valid. Surviving
outside sidedefs keep their concrete wall/lower/upper texture IDs and UV
settings; they are not reset to the winning sector defaults.

The first version is conservative. It rejects non-adjacent sectors, one-sided
boundaries, sectors that only touch at a vertex, disconnected or ambiguous
shared boundaries, closed shared boundaries, and any join that would produce
invalid resulting loops or invalid candidate topology. Sector-inspector
`Join With...` picking is not implemented yet; joining is currently driven by a
selected portal.

## Sidedef And Linedef Inspector

Selecting near a linedef in 2D selects the clicked side when that side has a
sidedef. If the clicked side is missing but the opposite side exists, the editor
selects the existing opposite sidedef and reports that the clicked side has no
sidedef. If neither side has a sidedef, the editor selects the linedef as a
line-only selection.

The sidedef/linedef inspector supports:

- front/back sidedef selection
- `Switch to opposite side` when the opposite sidedef exists
- `Join Sectors` when the selected portal has two different adjacent sectors
- `Blocks Player` on two-sided portal linedefs
- wall, lower, upper, and eligible two-sided middle texture selection
- wall, lower, upper, and middle UV scale/offset editing
- reset UV for the selected wall/lower/upper/middle part
- fit width, height, or both for the selected wall/lower/upper/middle part
- `Clear Middle` for the selected middle texture
- `Split Linedef`
- `Split At Point`
- line-only inspection and splitting when no sidedef is selected

Topology JSON can also store one optional decal layer for each floor, ceiling,
wall, lower, and upper surface material. Middle texture editing is Base-only
for now and does not expose middle decals, emissive, tint, or bloom. Surface
material panels expose
`Layer: Base | Decal`. Base edits the normal texture and UV settings. Decal
edits the optional overlay texture, UV, opacity, emissive mode, and tint for
the selected surface. `Clear Decal` removes the selected surface decal and
resets texture, UV, opacity, emissive, and tint. Decal assignment uses the
normal topology texture table and texture picker; decal textures are ordinary
texture IDs, not a separate texture category. Decal UVs are masked outside
`0..1`, so decals do not tile across the whole surface. Non-emissive decals
are composited over the base texture first, then lit like the base surface.
Emissive decals render unlit over the already-lit base surface and contribute to
a visual bloom post-process, but do not cast light or affect lightmap baking.
Bloom is sourced only from emissive decals; bright base textures, non-emissive
decals, editor UI, and debug overlays do not bloom. Decal opacity and tint affect
both the rendered emissive decal and its bloom source. Emissive decals also expose
a `Bloom` intensity value in the editor, stored as `bloomIntensity` in decal JSON,
which affects only the bloom source and not the main emissive decal render. Tint
multiplies decal RGB and is edited with a compact swatch that opens a modal color
dialog rather than direct inspector RGB fields. The single optional decal layer
limitation remains.

For manual decal verification, load `decal_test` from the `Load` dialog. The
sample level lives at `assets/levels/decal_test/decal_test.json` and uses
`assets/images/biker_chick.png` as a transparent wall and floor decal.

`Split Linedef` creates one exact midpoint vertex and replaces the selected line
with two new linedefs. Existing front/back sidedefs are duplicated onto both
replacement lines with fresh stable IDs, preserving sector ownership, side kind,
texture IDs, and UVs. Splitting fails if the midpoint cannot be represented
exactly on the integer coordinate grid.

Wall-like surfaces use distance-based generated base UVs. Wall, lower, upper,
and middle U spans are based on physical linedef length, and V spans are based
on the visible wall or portal-opening height. Reset UV restores scale `(1, 1)`
and offset `(0, 0)`, which
restarts the selected wall span's local texture coordinates and tiles the texture
every 2 world units. When Decal is the active layer and a decal is assigned,
the same UV tools operate on the selected decal UV instead of the base UV.
`Fit Width`, `Fit Height`, and `Fit Both` adjust only the selected
wall/lower/upper/middle part's active-layer UV scale and reset the fitted offset axis
so that the selected texture spans once across the selected width and/or height.
Middle Fit Height uses the portal opening from the higher adjacent floor to the
lower adjacent ceiling. Middle textures currently fill that whole opening.
`Align Vertical` adjusts only the selected wall/lower/upper part's V offset so
brick rows or wall courses line up by world height. Fit and Align Vertical
preserve the selected part's texture ID and do not change other wall parts, the
opposite sidedef, sector defaults, floors, or ceilings. Align Vertical also
preserves the selected part's UV scale and U offset.
`Align U Prev` and `Align U Next` adjust only the selected wall/lower/upper
part's active-layer U offset so the texture continues from the previous or next
visible compatible wall/lower/upper surface in the same sector loop, skipping
edges where that wall part is not visible. On the Decal layer, Align U also
skips visible neighbors that do not have an assigned decal for the same wall
part, uses the neighbor decal UV as the source, and never falls back to neighbor
base UV. They preserve texture ID, scale, V offset, other wall parts, and the
opposite sidedef. They do not copy material, scale, or texture from the
neighbor, and they are not full wall-chain alignment yet.

`Split At Point` starts a pending canvas action for the selected linedef. Click a
snapped point exactly on that linedef to split there, or press Escape/right click
to cancel. The clicked point must be strictly inside the linedef segment; endpoint
points and coordinates already occupied by an existing topology vertex are
rejected without changing the map.

## Topology Vertex Inspector

Topology vertices can be selected directly in the 2D editor. Vertex picking uses
the same small screen-space marker hit test as vertex movement and takes
priority over linedef/sidedef and sector picking when the click is on the vertex
marker. Selected vertices draw with a translucent green fill and stronger green
outline under the normal vertex marker.

The vertex inspector shows:

- stable topology vertex ID
- read-only X/Y coordinate in visible authoring units
- incident linedef count
- `Dissolve Vertex`
- `Merge Into...`

`Dissolve Vertex` removes a simple degree-2 topology vertex by replacing its two
incident linedefs with one replacement linedef. The edit is transactional:
topology is copied, edited, validated, sector loops are extracted, and the live
map is changed only on success. On success the replacement linedef is selected.
On failure the vertex remains selected and the status message explains the
rejection.

Dissolve is conservative. It rejects missing vertices, isolated or degree-1
vertices, branching degree-3+ vertices, collapsed replacements, duplicate
replacement physical linedefs, invalid/crossing/overlapping candidate topology,
and ambiguous sidedef transfer. Sidedefs are only merged when the two directed
segments belong to the same sector and have identical wall/lower/upper texture
IDs and UV settings.

## Insert Sector Inside

With a parent sector selected, `Insert Sector Inside` starts a contained-sector
draw using the normal pending-point controls. The inserted polygon must be
strictly inside the selected parent sector's usable area. It must not touch
existing topology vertices, touch or cross existing topology edges, overlap
existing topology, overlap an existing parent hole, or exactly match existing
topology.

Finalizing creates one child sector. Each inserted boundary edge creates:

- a child front sidedef
- a parent back sidedef
- one linedef shared by those two sides

Parent holes are implied by those parent back sidedefs; they are not stored as a
sector field in JSON. The child initially copies the parent sector's fields,
including heights, floor/ceiling texture and UV settings, ambient settings, and
default wall/lower/upper settings. Child/front and parent/back sidedefs are
independent concrete records after creation.

Nested inserts are allowed when the resulting topology validates. Deleting an
inserted child does not heal the former hole into the parent floor; the former
boundary remains as one-sided topology when validation succeeds.

## Static Baked Lights

Topology documents own static point lights directly. These lights are used by
the lightmap baker only; they are not dynamic runtime lights and do not cast
dynamic runtime shadows.

The Light tool places a light inside the clicked topology sector at the clicked
X/Z position and at the sector floor height plus 1.8 world units expressed in
authoring units. New lights use stable positive integer IDs.

Static light fields:

- integer `id`
- position X/Y/Z
- color
- intensity
- radius
- sourceRadius

Select mode picks lights before linedefs/sidedefs/sectors. The Move tool drags
lights in X/Z and keeps their Y value unchanged. The inspector edits position,
color, intensity, radius, and sourceRadius. Deleting a selected light removes it
from the topology document after confirmation.

## Texture Picker And Add Map Texture

Map textures live in `topologyMap.texturesById`. Texture fields store texture
IDs, not file paths or raylib texture objects. The texture picker choices come
from the current map texture table.

Texture pickers are used for:

- sector floor and ceiling textures
- sector default wall/lower/upper textures
- sidedef wall/lower/upper/middle textures
- 3D surface panel texture targets

Surface materials can be copied and pasted between matching surface types from
the 2D inspectors and the 3D surface panel. The copied material includes the
texture ID and UV scale/offset only. Floor materials paste only to floors,
ceiling materials paste only to ceilings, and wall/lower/upper materials paste
only to the matching concrete sidedef wall part. Paste mutates only the selected
target surface; it does not edit the opposite sidedef, sector defaults, ambient
lighting, or geometry. Keyboard shortcuts are not implemented for this workflow
yet, so use the inspector or 3D panel buttons.

`Add Map Texture` scans `assets/images` recursively for PNG files. It can add or
update a texture ID in the map texture table and choose point, bilinear,
trilinear, or anisotropic 8x filtering. Legacy saved `"bilinear"` texture
filters load as anisotropic 8x; exact bilinear filtering serializes as
`"linear"`. It does not copy external files into the project.

## Move, Split, And Delete Tools

Moving a topology vertex edits exactly one stable vertex ID. Connected linedefs
and all sector loops using that vertex update through the shared reference.
Movement previews validate a candidate topology before committing on mouse
release.

Topology vertices can be merged conservatively. With the Move tool, hover a
vertex and use `Merge Into...`, then click the target vertex. Dragging a vertex
so its snapped release point exactly matches another existing vertex also
attempts the same merge. The target vertex survives, the source vertex is
removed, and existing linedefs are rewired only if the candidate topology remains
valid.

Vertex merge rejects cases that would collapse a linedef, create duplicate
physical linedefs, require automatic sidedef merging, or invalidate topology.

Moving a light edits only its X/Z position during the drag. Y remains unchanged.

Splitting is inspector-driven. It always splits at the exact stored-coordinate
midpoint and fails if that midpoint is not representable on the integer grid.

Sector deletion, vertex movement, conservative vertex merge, and conservative
vertex dissolve use copy/validate/commit style topology edits. Direct linedef
deletion, direct sidedef deletion, standalone vertex deletion, and undo/redo are
not available yet.

## 3D Mode

`3D Mode` builds the preview from the current in-memory topology map. It does
not require saving first, but unsaved changes remain unsaved until `Save`.

3D controls:

- `F11`: toggle mouse-look/captured cursor mode.
- `F3`: toggle `FreeFly` / `Gameplay` preview controls.
- `F1`: toggle baked ambient-occlusion display.
- `Tab` or `Escape`: return to 2D mode.
- In `FreeFly` mouse-look mode: `WASD` move, mouse looks, `Space` moves up,
  `Ctrl` moves down.
- In `Gameplay` mouse-look mode: `WASD` moves horizontally relative to yaw,
  `Space` jumps, mouse looks, and `Shift` uses run speed. Gameplay mode follows
  the current sector floor, applies gravity while airborne, lands on floors,
  clamps against ceilings, and resolves horizontal cylinder collision against
  topology walls, height-valid portals, and portals marked `Blocks Player`.
  Crouching, slopes, projectile/sight/monster collision flags, and polished drop
  behavior are still deferred.
- In visible-cursor mode: click generated surfaces to select/edit them.

The left tools pane `Settings` button opens editor-session preview settings.
The same settings are available from the 3D preview overlay while its UI is
visible. The modal edits walk speed, run speed, mouse sensitivity, camera eye
height, gravity, player radius, player height, step height, jump height, head
bob strength, and head bob frequency. Gameplay Preview Settings use
runtime/world units or simple unitless multipliers, not authored units. The
gameplay controller stores a feet/body position; the camera eye is computed by
adding the configured eye height, while player height is the collision cylinder
height used for ceiling clearance. Player height is normalized to at least eye
height. Step height defaults to `0.25` world units. Jump height defaults to
`0.6` world units. Head bob strength defaults to `0.020` world units and head
bob frequency defaults to `2.0`. Gravity uses a positive magnitude; `0`
disables falling and also prevents jumps from adding lift.

Grounded Gameplay movement snaps feet to same-height floors and small up/down
floor changes within step height. The physics feet/body position still snaps
immediately for collision correctness, but the rendered Gameplay camera eases
small step-up and snap-down eye-height changes visually. Larger drops start
falling under gravity and are not step-smoothed; jumping is grounded-only and
sets vertical velocity from `sqrt(2 * gravity * jumpHeight)`. Airborne players
do not auto-step upward through higher-floor portals; they can pass only when
the current vertical cylinder already fits the destination sector. Same-floor
and small upward portals within step height are passable while grounded,
too-high upward portals block, low-ceiling portals block, and downward portals
are passable for now. Ceiling bonks clamp the player below the ceiling and
clear upward velocity. Step smoothing does not apply to jumps, landings,
ceiling bonks, cannot-fit cases, no-sector fallback, or FreeFly controls.
Gameplay mode also applies visual-only headbob while grounded and actually
moving horizontally. Headbob is layered after the physics eye pose and visual
step smoothing, and it does not affect collision, sector lookup, vertical
physics, or the stored feet/body position. Headbob is disabled while airborne,
falling, jumping, standing still, or in no-sector fallback. Gameplay mode also
applies a subtle visual-only landing dip when landing from a jump or fall. The
landing dip is separate from step smoothing and headbob, is based on downward
impact speed, is clamped to a small downward offset, and recovers back to the
normal eye height without affecting physics, collision, sector lookup, or the
stored feet/body position. Landing dip does not implement camera shake,
rebound above normal eye height, or weapon bob. The Gameplay overlay reports
collision state, current sector, grounded/jumping/falling state, recent
vertical transition, recent wall/step/ceiling blocks, radius, step height, jump
height, floor, feet, velocity, and gravity in runtime world-space values.

3D picking maps generated surfaces back to topology:

- floor and ceiling surfaces select the sector
- wall, lower, upper, and middle surfaces select the concrete sidedef and wall part

The 3D surface panel edits only the selected surface target. Floor/ceiling
targets edit sector texture and UV settings. Wall/lower/upper targets edit the
selected sidedef's matching wall part. Middle targets edit the selected
sidedef's middle texture as a Base-only material. The Texture button opens the topology
texture picker. `Layer: Base | Decal` chooses whether Texture, UV, Reset UV,
Fit, and Align controls edit the base material or the optional decal layer.
Middle targets hide the layer toggle and expose only Texture, UV scale/offset,
Reset UV, Fit Width, Fit Height, Fit Both, Clear Middle, and `Blocks Player`
when the selected surface belongs to a two-sided portal linedef.
When Decal is active and no decal texture is assigned, the panel shows
`No decal assigned` and only the Texture picker remains available. Assigned
decals expose UV, opacity, emissive, tint, and `Clear Decal`. Reset UV resets
only the selected surface target, Fit Width / Fit Height / Fit Both fit only
selected wall-like targets, Align Vertical shifts only selected wall-like V offsets to line up by
world height, Align U Prev / Align U Next shift only selected wall-like U
offsets to continue from the previous or next visible compatible
wall/lower/upper surface in the same sector loop, skipping edges where that wall
part is not visible, and Copy/Paste Material copies base texture ID plus UV
scale/offset between matching selected surface kinds.
Align Vertical preserves texture ID, scale, U offset, and other wall parts.
Align U Prev / Align U Next preserve texture ID, scale, V offset, other wall
parts, and the opposite sidedef; they do not copy material, scale, or texture
from the neighbor and are not full wall-chain alignment yet.

Equal-height portals with an assigned middle texture still generate a middle
portal-plane surface for 3D picking. Equal-height portals without middle texture
data have no generated 3D wall surface, so edit their sidedefs from the 2D
linedef/sidedef inspector. `Blocks Player` is available in the selected
sidedef/linedef inspector for two-sided portals even when no middle texture is
assigned.

## Baked Lightmaps

`Bake Lightmaps` uses topology generated geometry and topology static lights. It
writes the PNG to:

```text
assets/levels/<level_name>/<level_name>.lightmap.png
```

The topology JSON stores bake settings in `lightmapSettings` and installed bake
metadata in `bakedLightmap`. Bake settings include ambient occlusion radius and
strength, plus indirect bounce radius and strength.

The bake runs asynchronously with progress and cancellation. It writes to a
temporary output first. When the worker finishes, the main thread installs the
result only if the current topology source hash still matches the snapshot used
for the bake. If the document changed during the bake, the temporary result is
discarded.

The source hash is deterministic over the topology lightmap bake version
(`7`), atlas and sample constants, coordinate subdivision value, map texture
definitions referenced by baked surface fields, vertex/linedef/sidedef/sector
IDs and geometry, sector and sidedef texture and UV fields, static lights, and
bake settings. Middle texture receiver data is included because it affects
lightmap chart layout. The hash does not include the installed baked-lightmap
metadata itself.

The baked PNG stores direct static-light contribution and one-bounce indirect
light in RGB, and ambient occlusion in alpha. 3D Mode uses a baked atlas only
when metadata exists, the atlas file is present, and the stored source hash
matches the current topology. Otherwise the preview falls back to sector ambient
lighting.

Equal-height portals generate no wall surface and therefore no wall lightmap
chart unless they have an assigned middle texture.
Middle texture surfaces allocate lightmap charts and receive baked light on
their opaque alpha-tested pixels. They remain cutout surfaces: transparent
texture pixels are discarded during rendering, so the lightmap does not need
alpha for the holes. Middle texture surfaces do not cast baked shadows or
occlude lightmap rays yet; alpha-aware middle texture shadow casting is
deferred.

## Current Limitations

- No undo/redo.
- No external texture import/copy; texture files must already exist under
  `assets/images`.
- No player-start editing in the sector editor.
- Gameplay preview mode has topology-based horizontal cylinder collision,
  portal height checks, floor following, gravity, landing, ceiling clamp, and
  grounded-only jumping. Two-sided portal linedefs can opt into player blocking.
  Crouching, slopes, projectile/sight/monster collision flags, polished drop
  behavior, and NPC navigation are deferred.
- No dynamic runtime lights or dynamic shadows.
- No alpha-based middle texture collision, translucent glass, depth sorting,
  middle texture decals, middle emissive/tint/bloom controls, or middle
  Copy/Paste Material controls.
- No normal maps, material maps, PBR material editing, or texture search UI.
- Single fixed-size lightmap atlas; no multi-atlas packing.
- No 3D geometry editing beyond texture and UV edits on generated surfaces.
- No direct linedef or sidedef deletion.
- No standalone vertex deletion.
- No automatic duplicate linedef or sidedef merge.
- No arbitrary line cutting or automatic overlap splitting.
