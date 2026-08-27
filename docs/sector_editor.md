# Sector Editor

The sector editor saves strict authoring-graph v4 documents and derives a
`SectorTopologyMap` for preview, runtime, collision, and baking. It is a
Doom-like editor built around vertices, lines, faces, global materials, static
baked lights, lightmap settings, and optional baked lightmap metadata.

The editor has a 2D authoring mode for topology creation and inspection, plus a
3D preview/edit mode for checking generated geometry and editing surface material
and UV settings.

## File Layout And JSON Format

Levels use one directory per level:

```text
assets/levels/<level_name>/<level_name>.json
assets/levels/<level_name>/<level_name>.lightmap.png
```

Level names may contain only letters, digits, underscores, and dashes.

Current level JSON is authoring-graph v4:

```text
formatVersion: 4
topology: "authoringGraph"
coordSubdivisions: 16
authoringGraph
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
- The numeric `Grid` size controls drawing and snapping and is saved per map;
  older maps default to `8`.
- Select tool: click editor objects, authoring vertices/lines, and derived
  authoring faces. Shift-click toggles additional authoring faces; clicking
  empty canvas clears selection unless Shift is held.
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
- `Delete`: for selected authoring faces, enter `Merge Selected Into...` target
  picking; for a selected authoring line, delete only when the resulting graph
  derives valid topology; for an isolated or degree-2 authoring vertex, delete
  or dissolve it transactionally. Existing light/object deletion behavior is
  unchanged.
- `Escape`: clear selection, then return to Select tool.
- `New`: confirm and reset to a blank topology level.
- `Load` (`Ctrl+O`): select a level under `assets/levels`; unsaved edits require
  confirmation before loading.
- `Save` (`Ctrl+S`): save directly to the current level path. An unnamed level
  opens `Save As...` instead.
- `Save As...`: open the level naming dialog and confirm before overwriting a
  different existing level.
- `Reload`: confirm and reload the current saved level.
- `Material Editor`: add, remove, rename, and edit global materials and their
  albedo/filter/PBR scalar properties.
- `Sound Editor`: add, remove, rename, retype, and replace map-local Sound and
  Music entries. Referenced IDs, types, and removals are locked while their
  underlying audio files may still be replaced.
- Billboard tool: place a generic authored billboard marker inside a sector.
- Door tool: place a portal-attached procedural door on a valid two-sided
  linedef.
- `Bake Lightmaps`: bake topology static lights into the level lightmap atlas.
- `3D Mode` (`Ctrl+D`, under `View`): rebuild the 3D preview from the current
  in-memory topology map, or return to 2D from preview mode.
- `Copy config` / `Paste config` (`Ctrl+C` / `Ctrl+V`): copy and paste the
  selected compatible editor configuration. Disabled commands do not fire.

The application-wide Graphics Settings screen includes an `FPS counter`
checkbox for raylib's green FPS display. It defaults off and is separate from
the F9 performance overlay and from level settings.

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

Sectors can mark their ceiling as sky/open. A sky ceiling does not generate the
normal ceiling surface. In 3D preview, sky ceilings show a basic visual-only
panorama cylinder when the map-level sky material ID resolves to a loaded global
material. Texture paths are not hardcoded. Sky albedos should be
horizontally seamless panoramas such as 4:1, with a small flat top band matching
the cap color; the renderer samples slightly inside that band to reduce cap seams.
Map-level sky settings are edited in `Preview Settings -> Sky`. The yaw offset
rotates the panorama around the camera, vertical offset/scale adjust the
panorama placement on the cylinder, and top color controls the solid top cap.
Missing, empty, unloaded, or failed sky textures still fall back to the existing
clear background behavior, and the clear color is unchanged.
Adjacent sectors that both have sky ceilings suppress the upper portal wall
strip between them, even when their numeric ceiling heights differ. The sky
renderer is visual-only and its persisted settings do not affect collision,
lightmaps, bloom, picking, or generated surface metadata; collision still uses
the sector's normal authored ceiling height for now. `ceilingSky` sectors are
still required for sky to appear through omitted ceilings.

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
configured step height. Small upward floor differences within step height snap
immediately. For downward transitions, a grounded controller keeps its current
supporting floor while any part of its circular footprint still overlaps that
sector; after the footprint clears, a small drop snaps down and a larger drop
starts falling under gravity instead of teleporting to the lower floor. Portal
step eligibility is measured from the controller's physical feet height so a
controller retained on a stair lip can move back onto that stair without a
spurious reverse-step push. A two-sided portal linedef with
`Blocks Player` enabled behaves like a blocking wall for Gameplay movement,
regardless of portal height passability. Middle textures do not independently
block movement or add collision; middle texture plus `Blocks Player` is the
intended grate/window/barrier workflow. One-sided walls already block. Projectile,
sight, and monster blocking flags remain deferred.

Runtime doors add a dynamic blocker layer after static topology movement. Closed
or sufficiently closed door slabs block the player with a portal-aligned OBB
footprint and vertical interval derived from the authored door size and current
runtime transform. Open doors allow passage once their dynamic collider no
longer blocks the crossing. Door collision uses ECS runtime door state; it does
not rebuild generated render meshes or mutate sector heights.

## Sector Inspector

Selecting a topology sector opens the sector inspector. It edits:

- sector name and stable integer ID display
- floor and ceiling heights
- ceiling sky/open toggle
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

Static rectangular lights additionally expose `width`, `height`, `range`, and
optional `startFeather`. Width and height define the sampled area emitter and
therefore its soft-shadow source size. `startFeather` is a separate authored
distance that fades direct light in from the front of the emitter plane; it
does not emit light behind the rectangle. A missing or zero value preserves the
hard start used by older maps.

Select mode picks lights before linedefs/sidedefs/sectors. The Move tool drags
lights in X/Z and keeps their Y value unchanged. The inspector edits position,
color, intensity, radius, and sourceRadius. Deleting a selected light removes it
from the topology document after confirmation.

## Global Materials And Material Picker

Global materials live in `assets/materials/materials.json`. Level fields store
material IDs, not file paths, raylib texture objects, or local material
definitions. The level v4 JSON therefore has no `textures` table. Every map
shares the same material-picker catalog, while preview/runtime resolve and load
only the materials referenced by that map's generated surfaces, decals, sky,
and procedural doors.

Material pickers are used for:

- sector floor and ceiling materials
- sector default wall/lower/upper materials
- sidedef wall/lower/upper/middle materials
- decals, sky, procedural doors, and 3D surface-panel targets

The shared material picker filters the global catalog by material ID as text is
entered in its case-insensitive Filter field.

Surface materials can be copied and pasted between matching surface types from
the 2D inspectors and the 3D surface panel. The copied material includes the
material ID and UV scale/offset only. Floor materials paste only to floors,
ceiling materials paste only to ceilings, and wall/lower/upper materials paste
only to the matching concrete sidedef wall part. Paste mutates only the selected
target surface; it does not edit the opposite sidedef, sector defaults, ambient
lighting, or geometry. `Ctrl+C` and `Ctrl+V` invoke the same enabled copy/paste
commands as the inspector and 3D panel buttons.

Selected 3D Props and Dynamic Props also support `Copy config` / `Paste config`.
Each copied prop config pastes only to the same prop type. Paste copies the
model, orientation, height offset, scale, collision, and shadow settings; a
Dynamic Prop additionally copies its animation and interaction settings. The
destination object ID, script instance ID, and world position remain unchanged.
Static-model bake fingerprints are transient and are not copied.

The Material Editor has a scrolling material list and an albedo preview. Each
definition exposes editable ID, albedo PNG selection, filtering, metalness,
roughness, and normal strength. The albedo button opens a modal that rescans
`assets/images` recursively, filters relative paths as you type, and previews
the selected image before it is applied. Automatic `_normal.png` companions are
excluded from the albedo list, and displayed paths omit the common
`assets/images/` prefix. IDs may be edited. Renaming an ID updates all valid
saved v4 levels transactionally and the open document; deleting a referenced
material is blocked. Saved albedo paths remain normalized PNG paths below
`assets/images`.

Normal maps retain the OpenGL Y+ filename convention: an albedo such as
`stone.png` discovers `stone_normal.png`. The editor displays whether that
companion is present. Normal strength is consumed by dynamic sector lighting;
metalness and roughness are stored now for the later sector GGX path. Sky,
decal, and procedural-door consumers currently use the material albedo only.

## Sound Editor

The left tools pane opens the Sound Editor directly below the Weapon Editor.
Its scrollable left pane lists every map-local Sound and Music ID; Add and
Remove stage registry changes until Save. The details pane edits the ID, picks
or replaces a file below `assets/audio`, previews it through the shared audio
picker, and chooses whether it is a buffered Sound or streaming Music.

Sound Emitters accept either type. Buffered Sound is suitable for short effects;
streaming Music is suitable for long positional sources such as radios. Each
streaming emitter owns independent playback, so it can coexist with roomtones
and other emitters using the same map audio entry. The emitter inspector accepts
an exact ID or opens a picker containing all registered entries; invalid text is
not committed and is restored to the authored value.

IDs used by sector roomtones, Sound Emitters, or door open/close sounds cannot
be renamed, retyped, or removed. The details pane reports those known level
references in a word-wrapped usage box. Replacing the audio file remains
available because it preserves the stable ID and loading role. Lua may refer to
IDs dynamically or from external scripts, so script strings are not treated as
statically discoverable references.

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
vertex dissolve use copy/validate/commit style topology edits. Direct topology
linedef deletion, direct topology sidedef deletion, standalone topology vertex
deletion, and undo/redo are not available yet. In graph-authoritative editing,
an isolated authoring vertex can be deleted and a degree-2 authoring vertex can
be dissolved from the `Authoring Vertex` inspector or with `Delete`. The
lower-ID incident line survives and connects the two neighboring vertices.

## 3D Mode

`3D Mode` builds the preview from the current in-memory topology map. It does
not require saving first, but unsaved changes remain unsaved until `Save`.

3D controls:

- `F11`: toggle mouse-look/captured cursor mode.
- `F3`: toggle `FreeFly` / `Gameplay` preview controls.
- `F1`: toggle baked ambient-occlusion display.
- `Tab` or `Escape`: return to 2D mode.
- The 3D overlay shows a compact status strip and debug tabs by default. Detailed
  diagnostics and the longer controls legend are available from the overlay tabs.
- Overlay tabs and controls are mouse-clickable when the cursor is unlocked with
  `F11`; locked mouse-look mode keeps normal 3D input focused on the preview.
- In `FreeFly` mouse-look mode: `WASD` move, mouse looks, `Space` moves up,
  `Ctrl` moves down, and holding `Shift` reduces movement speed by 10x for
  precise positioning.
- In `Gameplay` mouse-look mode: `WASD` moves horizontally relative to yaw,
  `Space` jumps, mouse looks, and `Shift` uses run speed. Gameplay mode follows
  the current sector floor, applies gravity while airborne, lands on floors,
  clamps against ceilings, and resolves horizontal cylinder collision against
  topology walls, height-valid portals, and portals marked `Blocks Player`.
  Crouching, slopes, projectile/sight/monster collision flags, and polished drop
  behavior are still deferred.
- In visible-cursor mode: click generated surfaces to select/edit them.

The left tools pane `Settings` button opens editor-session preview settings.
The same settings are available from the 3D preview overlay `Controls` tab while
its UI is visible. The modal edits walk speed, run speed, mouse sensitivity,
camera eye height, gravity, player radius, player height, step height, jump
height, head bob strength, head bob frequency, and solid NPC-to-NPC collision.
NPC-to-NPC collision defaults on for backward compatibility. Turning it off
keeps Crowd local avoidance and separation active but skips the final solid
NPC cylinder collision pass, allowing occasional overlap instead of forcing
contact resolution. NPC collision with the player remains enabled.
Gameplay Preview Settings use
runtime/world units or simple unitless multipliers, not authored units. The
gameplay controller stores a feet/body position; the camera eye is computed by
adding the configured eye height, while player height is the collision cylinder
height used for ceiling clearance. Player height is normalized to at least eye
height. Step height defaults to `0.25` world units. Jump height defaults to
`0.6` world units. Head bob strength defaults to `0.020` world units and head
bob frequency defaults to `2.0`. Gravity uses a positive magnitude; `0`
disables falling and also prevents jumps from adding lift.

Weapons are global game definitions stored in `assets/config/weapons.json`, not
map or preview settings. Open the Weapon Editor from the 2D tools pane or from
the 3D preview debug overlay's `Arms` tab. The editor uses a scrolling weapon
list on the left and the selected weapon's complete definition on the right.
It can add a default definition, duplicate a selected weapon, delete definitions,
and assign each weapon to one of the exclusive keyboard slots `1` through `6`.
Assigning an occupied slot is refused until the other weapon is changed or
unassigned. Multiple weapons may reference the same arms
model while retaining independent animation, grip, attachment, firing, recoil,
muzzle, crosshair, and presentation values. In 3D preview the editor stays
docked beside the live scene; `Preview Fire` and holster controls make tuning
visible immediately. Save writes the global registry, while Cancel discards the
draft. Legacy per-weapon application-setting overrides are folded into the
registry when the editor opens and removed on its next successful save.
The slot-1 weapon is selected but remains holstered when gameplay or 3D preview
starts. Press `H` to equip it. In gameplay and 3D Gameplay control mode, the
top-row keys `1` through `6` switch to assigned weapons by holstering the old
weapon completely before automatically unholstering the new weapon.
Firing definitions can optionally enable hitscan pellets and configure their
count and spread half-angle. `Noise radius` is the world-space radius published
to NPC hearing for each accepted shot; `0` makes the shot silent to AI. A pellet
weapon performs one normal firing event
for cooldown, sound, recoil, muzzle flash, and muzzle light, while each pellet
independently resolves its first collision, damage, knockback, stagger, and
impact effects. Weapons without pellets enabled retain one centered hitscan
ray. Magazine ammunition and manual reload behavior are not currently part of
the weapon system.

`Preview Settings -> Fog` configures map-level analytic distance fog. The fog
color is a scattering tint, not an emissive replacement color: distance fog
attenuates surfaces and uses the static illumination available to each receiver
to tint and gate its visible in-scattering. Lit surfaces can approach the
authored fog color, while scattering fades smoothly to zero as static
illumination approaches black. Completely dark sectors therefore remain black
instead of developing a self-lit colored veil. Dynamic-light shafts remain an
explicit per-light haze effect rather than being added automatically by
distance fog.

Grounded Gameplay movement snaps feet to same-height floors and small up/down
floor changes within step height. Downward snaps wait until the controller
footprint clears its previous supporting floor. The physics feet/body position
then snaps immediately for collision correctness, but the rendered Gameplay camera eases
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
applies a visual-only landing dip when landing from a jump or fall. The landing
dip is separate from step smoothing and headbob, and it uses a downward
impact-speed range curve: impacts below the minimum impact speed produce no
dip, impacts near the full-dip impact speed reach the maximum dip clamp, and
an ease-in curve power keeps normal jumps subtle while allowing larger drops
to feel more pronounced. The dip then recovers back to the normal eye height
at a fixed recovery rate. Typical jumps should stay restrained, larger drops
should show a clearer dip, and 3m-or-larger falls should reach or nearly reach
the full visual effect. Landing dip does not affect physics, collision, sector
lookup, or the stored feet/body position. Landing dip does not implement camera
shake, rebound above normal eye height, or weapon bob. The Gameplay overlay reports
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

## Runtime ECS Objects, Billboards, And Doors

Sector topology remains purpose-built non-ECS data. Vertices, linedefs,
sidedefs, sectors, generated geometry, lightmap atlas data, static draw records,
and portal visibility data are map/world structures, not entities.

Movable and high-level runtime objects use the small ECS. `EngineContext::world`
is the authoritative ECS world; `SectorMeshPreview` only observes it for
rendering and never owns, resets, spawns, or destroys runtime ECS entities. The
first sector-world consumer is the 3D preview billboard sprite path for future
props, NPCs, pickups, projectiles, effects, attached lights, and models.
Billboard sprites store asset handles and playback state, including
`SpriteAnimationHandle` for Aseprite animations. They do not use the older 2D
`SpriteRenderSystem` or the old rectangle sample renderer; those remain
legacy/test examples, not the sector-world billboard renderer.

Billboards render as alpha-tested cutout camera-facing quads in the 3D preview
after static sector geometry has populated world depth. The cutout shader
samples the current Aseprite atlas texture using the frame source rectangle UVs,
discards pixels below the sprite alpha cutoff, and writes surviving pixels as
opaque depth-writing pixels. Transparent particles, smoke, glass, spell effects,
and sorting remain deferred to a later transparent render pass. Missing,
failed, or not-ready assets are skipped safely.

Doors are authored runtime objects with `kind: "door"` and a portal anchor to a
two-sided linedef. The Door tool places a door only on a valid two-sided portal;
one-sided wall placement is rejected with editor status instead of creating an
invalid object. The saved anchor uses stable linedef, sidedef, and sector IDs,
plus exact endpoint coordinates for diagnostics. Door slabs are procedural 3D
runtime objects attached to the portal; the portal itself remains a zero-thick
logical connection.

Door dimensions are authored in runtime/world units. Width and height can derive
from the resolved portal opening, while thickness and normal offset control the
physical leaf around the portal plane. Implemented motion values are
`slide_vertical`, `slide_left`, `slide_right`, and `swing`. Door runtime state,
including current and target open fractions, transform, collider, and portal
visibility blocker state, lives in ECS. Authored `initialOpenFraction` remains a
level default and is not rewritten by preview interaction.

The default `visual` is `procedural`, preserving older map JSON. Procedural doors
render as opaque box/slab geometry and may use any motion type. Their map texture
ID and per-face UV controls remain available; missing or failed textures use the
default material. Selecting `visual: "model"` requires `motion: "swing"` and a
non-empty `modelAssetId` from `assets/models/doors/swing/catalog.json`. Unknown
catalog IDs remain loadable and produce an editor/runtime diagnostic plus an
animated procedural fallback instead of an invisible blocker or crash.

Model swing-door authored fields and defaults are:

- `modelFit`: `fit_inside` by default, or `fit_width` / `manual`
- `modelScale`: positive uniform multiplier, default `1.0`
- `hinge`: portal endpoint `start` by default, or `end`
- `swingSide`: into the portal `front` sector by default, or `back`
- `openAngleDegrees`: greater than zero and at most 170, default `90`
- `angularSpeedDegrees`: non-negative degrees per second, default `90`

All fit modes use one uniform scale. For styles with a frame, `fit_width`
matches the complete frame width and `fit_inside` constrains the complete frame
width and height to the target aperture. Frameless styles use the leaf bounds.
`manual` uses `modelScale` directly but reports overflow from the complete
assembly when a frame exists. The multiplier is applied after the base fit. The
inspector reports both the fitted assembly and actual leaf size; the runtime
never stretches individual axes. Generated catalog metadata preserves the
source leaf's horizontal hinge inset and bottom offset within its paired frame,
so the fixed frame remains centered at the portal while the leaf rotates from
its correctly aligned hinge. Pending or failed leaf assets retain the same
aligned procedural fallback, and a missing frame does not hide a ready leaf.

Swing rendering uses the existing world-model PBR path, object-probe or sector
ambient fallback lighting, dynamic point/spot lights, fog, environment
reflections, and opaque dynamic point/spot shadows. Leaves and frames are runtime
visuals: they are not static lightmap receivers or baked occluders and their
fields/assets do not affect the lightmap source hash.

Dynamic props and NPCs use the same world-model PBR path and always receive
available dynamic point/spot light shadows without darkening their baked
object-probe or sector-ambient contribution. Their inspector shadow dropdown is
`None`, `Contact`, or `Dynamic`. `None` disables casting, `Contact` draws only
the floor contact blob, and `Dynamic` submits the current posed model to the
shared point/spot shadow atlas. Atlas updates retain the configured per-frame
light budget and do not fall back to contact shadows when unavailable. Older
`projected_silhouette` map values load as `Dynamic` and save back as `dynamic`.
These runtime shadow choices do not affect baked lightmaps or their source hash.

Static 3D props and dynamic props have stable script instance IDs in one shared
prop namespace. Static props missing an ID in older maps receive a deterministic
`prop_<objectId>` ID during load. Editing a static prop ID uses the normal
runtime-object mutation path, so it dirties the document, invalidates the 2D
topology render cache, and refreshes preview objects.

## Items and inventory

Item definitions are application-global entries edited through **Editors >
Item Editor**. The Item tool places level-owned runtime objects that reference a
definition and expose quantity, pickup distance, transforms, shadow mode, an
optional pickup hook, and—for Object definitions—an optional carried-use hook.
Item placement edits use the normal runtime-object document-edited path, so the
2D topology cache is invalidated while the global registry remains separate
from the level document.

In a game session, E takes a centered item when its complete quantity fits the
campaign inventory and its optional pickup callback permits it. I opens the
inventory. Health items can heal immediately or over time, items can be dropped
only at a clear floor position, and Objects with a carried-use hook can be held
under the cursor and left-clicked on a ready static or dynamic prop. The target
receives the existing pulse highlight and the hook receives its stable string
instance ID. Escape or right-click cancels targeting; I cancels and reopens the
inventory. A yielding hook keeps controls locked until completion.

Items and drops remain non-collidable and do not participate in static
lightmaps, baked occlusion, or the lightmap source hash. Sector-editor Gameplay
preview renders authored items but intentionally does not provide pickup,
inventory, healing, dropping, or carried Object use.

All glTF model emission uses the authored emissive texture/factor and
`KHR_materials_emissive_strength`, followed by a lightweight HDR appearance
curve. Grazing angles are attenuated to 70 percent at the silhouette and strong
radiance is blended up to 70 percent toward an equal-channel white core between
linear values 1 and 4. The maximum channel is preserved, so the shaping does not
independently push a material across the bloom threshold. This applies to
static/dynamic models, model doors, and viewmodels without extra textures,
passes, draw calls, or model naming conventions. It does not repurpose
roughness, AO, metallic, or normal textures. Scene-wide bloom still depends on
HDR strength and screen coverage; its default soft threshold begins at linear
value 1.

Lua may change a static or dynamic prop's runtime emissive scale by stable ID.
The scale defaults to 1 after spawning, affects every emissive material in that
prop instance, and does not mutate the shared model asset. Script IDs and
runtime emissive scales are visual/gameplay state and are excluded from the
lightmap source hash.

Door interaction is deliberately small. Authored `autoOpen` doors open as the
player approaches and close when the player leaves the configured distance.
Non-auto doors can be targeted and toggled with the Interact key `F` when the
player is close enough. The selected-door inspector also exposes a runtime
debug open/close target control when a spawned ECS door exists; that control
changes only preview runtime state.

The same derived swing-leaf transform drives rendering, bounds, and its rotated
2D OBB collision at every open fraction. The open leaf therefore remains a
physical obstacle beside the doorway. If a closing swing step would sweep
through the player's cylinder, the step is rejected and the door retargets open;
the player is not pushed. Frames have no separate collider. Static topology
collision, sector lookup, and general player physics are unchanged by swing
doors.

Closed ECS doors also contribute dynamic portal visibility blockers. A closed
door blocks traversal across its anchored portal for the preview visibility
query, while partly open doors allow traversal conservatively. This is a dynamic
overlay on the static portal visibility graph, so animating a door does not
rebuild the graph or sector meshes.

Billboard baked lighting samples the baked object-probe payload through the
runtime object lighting component. The renderer uses a stable probe-derived
ambient value and does not use the camera-facing quad normal for baked lighting,
so rotating the camera should not pulse baked brightness. Selected runtime
dynamic point and spot light contribution is added on top in the billboard
cutout shader using each fragment's world position. Dynamic spotlights that
receive an existing sector shadow-map slot attenuate only their own dynamic
spotlight contribution on billboards; baked object-probe lighting remains
visible and is not darkened by runtime shadow maps. Dynamic point lights and
dynamic spotlights without a shadow slot remain unshadowed. Billboards still do
not cast dynamic shadows.

Runtime object authoring is persistent v1 level data. The `Billboard` 2D tool
places a generic authored `kind: "billboard"` object inside a sector, saves it
with a stable positive ID, position, yaw, and nested billboard payload, and draws
a 2D marker with a yaw tick. New billboards default to a bottom-center normalized
origin `{0.5, 1.0}`, width and height of `1.0` world unit, keep-aspect enabled,
playing enabled, non-directional mode, and an empty sprite path until a sprite is
chosen.

Selecting a billboard exposes Object ID, sprite/type status, Position X/Y/Z,
Yaw, Width, Height, Keep aspect ratio, Origin X/Y, Directional, Playing, sprite
and clip selection, and Delete. Width and height are runtime world units, not
sector authoring height units. With keep-aspect enabled, committed width edits
update height from the selected sprite's aspect ratio, and committed height edits
update width. If sprite metadata is not ready, the edited dimension is kept and
the paired dimension is left unchanged. With keep-aspect disabled, width and
height edit independently. Origin X/Y are normalized and clamped to `0..1`.

The sprite picker scans `assets/sprites` recursively for Aseprite JSON files,
uses each file's `meta.image` atlas for preview through `AssetManager`, and
offers discovered clip names. Single billboards store one selected clip.
Directional billboards store Front, Back, Left, and Right clip names and choose
among them at render time from camera-relative facing. Missing or unloaded
assets and missing clips are skipped or fall back safely instead of crashing.
Aseprite frame durations, reverse playback, and pingpong playback are respected
by the existing billboard runtime path where supported by the sprite animation
asset parser.

Selecting a door exposes Object ID, type, anchor status, line/sector pair,
width, height, thickness, normal offset, motion type, open distance, speed,
initial open fraction, texture status and picker, auto-open, auto-open distance,
interaction distance, runtime debug target state, and Delete. Door diagnostics
use compact secondary inspector text with wrapping so stale endpoints or invalid
anchors remain readable. Door authored-field edits use the same runtime-object
mutation path as billboards: they mark the document edited, invalidate the 2D
topology render cache, and refresh spawned ECS objects when preview runtime is
available. Runtime debug target changes do not mark the document dirty.

Editing, moving, selecting a sprite, changing clips, toggling directional or
playing state, or deleting a billboard mutates the authored placed-object list,
marks the topology document edited, invalidates the 2D topology render cache,
and refreshes the spawned ECS object when a preview runtime is available.
Runtime ECS entities are spawned from placed billboards into
`EngineContext::world` during explicit map load/reset or preview refresh. The
runtime object asset scope, object probe data, object sector lookup data, and
placed-ID-to-entity mapping live in higher-level runtime object state, not in
`SectorMeshPreview`. Runtime cleanup destroys entities identified by the
`SectorObject` component and leaves unrelated ECS entities alone.

`goblin` is no longer a special editor object type or hardcoded runtime object
definition. The checked-in goblin sprite files, if present, are ordinary sprite
assets that can be selected like any other Aseprite billboard asset. Legacy
external maps containing only `definitionId: "goblin"` are not migrated into
functional generic billboards by the editor. The old F5 temporary
non-serialized spawn path has been removed; placed billboards are the runtime
object authoring path. Placed NPCs can use the shared bounded navigation,
collision-constrained locomotion, door traversal, and Crowd avoidance service.
Unrestricted actor physics, attached lights, and transparent alpha-blended
sprites are still deferred.

NPC definitions are global JSON assets under `assets/npcs` and are edited from
the NPC Editor. Hostile/friendly remains faction and collision data. The AI Type
dropdown filters registered AI descriptors by that alignment and also permits
`None`. Per-definition perception fields author vision range, the full
horizontal vision-cone angle, hearing range, and investigation duration.

The semantic action list includes `Attack`. Like other actions it assigns a
model animation and playback speed; it additionally authors normalized hit
phase, world-space range, damage, optional knockback impulse, optional stun in
milliseconds, and an optional spatialized attack sound. The sound plays when
the NPC begins the committed attack animation, whether or not the later hit
connects. The separately authored player-impact sound plays only when the hit
connects and is spatialized from the attacker so concurrent hits remain
directionally distinct. A stun value of `0` disables stun and a knockback value
of `0` disables knockback.
The Attack action also has an optional `cameraImpact` object. Existing NPCs use
the default enabled response: a directional `2.5` degree pitch and `3.5` degree
roll impulse, recovering through a `4 Hz`, `0.75` damping-ratio spring and
clamped to `7.5` degrees pitch and `10` degrees roll. The NPC Editor exposes the
enabled state, both kick strengths, spring frequency/damping, and accumulated
pitch/roll limits. Front/back attacks map to pitch, left/right attacks map to
roll, repeated hits accumulate within the configured limits, and omitted
fields retain their defaults. This camera layer is visual-only with respect to
controller state, collision, sector lookup, and physics. Like weapon camera
recoil, pitch affects the effective center ray while active; roll changes only
the rendered camera orientation.
During a stun the game player cannot sprint and moves at half walk speed, but
may still jump.

Seek & Destroy is the first hostile AI type. In the real game it uses generic
vision/hearing and last-known-position investigation, runs toward a detected
player, and commits to melee animations until their hit/finish points. The
authored range remains the baseline center-to-center range; runtime uses a
`0.10` world-unit engagement tolerance, a `0.25` world-unit disengagement band,
and a bounded `0.25` world-unit committed-hit margin so navigation does not
jitter at the melee boundary and nearby attackers resolve independently. A
player who moves beyond that margin or behind cover still avoids the hit. AI
never runs in either editor 3D preview control mode. Footstep and landing noise
radii are application settings JSON values rather than editor controls. Runtime
testing exposes `/god [on|off]`, `/freezeai [on|off]`, and
`/debugai [on|off]`; omitting the argument toggles the current campaign-session
value. `/debugai` is game-mode only and draws fixed-size projected state labels
above AI NPCs plus depth-tested vision, hearing, melee-range,
last-known-player, active-sound-event, and path diagnostics. It is independent
from the F8 navigation diagnostics and remains enabled across `/reload` until
the campaign session ends or the command disables it.

When player health reaches zero, game simulation stops behind a Game Over
overlay. Its Main Menu button shuts down the level, clears the unsaved campaign
and persistent script state, and returns the menu to `Start New Game`.

## Baked Lightmaps

`Bake Lightmaps` uses topology generated geometry, topology static lights, and
the optional map-level outdoor directional light. Pressing any bake button first
opens an authoring-only quality dialog. The selected preset is stored per level
and becomes the default the next time that level is baked:

| Preset | Texels/world | Soft-shadow samples | AO samples | Bounce samples | Approximate ray work |
|---|---:|---:|---:|---:|---:|
| Draft | 4 | 4 | 6 | 4 | ~0.125x |
| Standard | 8 | 8 | 12 | 8 | 1x |
| High | 16 | 12 | 18 | 12 | ~6x |

The relative-work figures compare ray-heavy stages on the same map; fixed bake
work and light distribution make actual times vary. Existing levels without a
`qualityPreset` field use Standard, which matches the previous hard-coded bake.

Each atlas remains 2048x2048. The primary artifact is written to:

```text
assets/levels/<level_name>/<level_name>.lightmap.png
```

If packing needs more space, the baker continues in
`<level_name>.lightmap.1.png`, `<level_name>.lightmap.2.png`, and so on. There
is no fixed atlas-count cap; only a single chart larger than 2048x2048 is
rejected. The primary `bakedLightmap.path`, `width`, and `height` fields remain
backward-compatible, while the optional `additionalAtlases` array records the
ordered extra atlas paths and dimensions.

The topology JSON stores bake settings in `lightmapSettings` and installed bake
metadata in `bakedLightmap`. Bake settings include the optional quality preset,
ambient occlusion radius and strength, plus indirect bounce radius and strength.
Quality changes chart density for both topology surfaces and static models;
packing allocates another atlas only when the current one no longer fits the
charts. A 2048x2048 RGBA16F atlas uses about 32 MiB of disk/GPU payload. Higher
lightmap resolution does not increase the number of screen fragments, but it
does increase memory, upload, and texture-cache costs and can add atlas-bound
draw batches.

Generated sector surfaces use their geometric normals during baking. Automatic
`_normal` companion textures are runtime material detail for dynamic lighting;
they are deliberately not sampled into the low-resolution baked lightmap.
Directional baked normal response would require a directional-lightmap path and
is deferred.

`Preview Settings -> Lighting` edits the map-level outdoor directional light.
Its `directionToLight` vector points from the shaded surface toward the light
source. The light contributes only to baked lightmap samples whose generated
surface owner resolves to a topology sector with `ceilingSky == true`. It uses
the bake BVH for baked shadows. Static point lights and ambient occlusion still
work normally, and no runtime shadowmaps or dynamic runtime directional lighting
are added.

The bake runs asynchronously with progress and cancellation. It writes to a
temporary output first. When the worker finishes, the main thread installs the
result only if the current topology source hash still matches the snapshot used
for the bake. If the document changed during the bake, the temporary result is
discarded.

The source hash is deterministic over the topology lightmap bake version
(`16`), atlas constants, resolved quality density/sample counts, coordinate
subdivision value, global material path/filter definitions referenced by baked surface fields,
vertex/linedef/sidedef/sector
IDs and geometry, sector and sidedef material and UV fields, static lights, and
bake settings. Directional light enabled state, normalized direction, RGB color,
and intensity are included, so directional changes invalidate baked lightmaps.
Middle texture receiver data is included because it affects lightmap chart
layout. Sky visual settings do not invalidate baked lightmaps. The hash does not
include automatic companion normal-map presence/content or scalar metalness,
roughness, and normal-strength metadata because those values are runtime-only
for generated sector surfaces. It also does not include the
installed baked-lightmap metadata itself.

Each baked artifact stores direct static-light contribution and one-bounce indirect
light in RGB, and ambient occlusion in alpha. 3D Mode binds the atlas assigned
to each topology batch or static-model mesh. Baked lighting is used only when
metadata exists, every atlas file is present, and the stored source hash matches
the current topology. Otherwise the preview falls back safely to sector ambient
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
  Crouching, slopes, projectile/sight/monster collision flags, and polished
  drop behavior are deferred. NPCs use a separate cached Detour navigation
  world and still resolve locomotion through topology collision.
- No entity-attached gameplay lights or unrestricted general-purpose dynamic
  shadow system; preview dynamic lights use the documented bounded renderer
  paths.
- No alpha-based middle texture collision, translucent glass, depth sorting,
  middle texture decals, middle emissive/tint/bloom controls, or middle
  Copy/Paste Material controls.
- Split/double-leaf doors, dynamic sector-height doors, transparent/glass model
  doors, frame collision, locks, keys, and save-game door state are deferred.
  NPC pathfinding supports current portal-attached doors through typed links.
  Model leaves/frames do not enter
  static lightmaps, and middle/transparent alpha-aware door shadows are not
  supported.
- Material metalness/roughness metadata is authored but not yet consumed by the
  sector surface shader; the directional companion lightmap and sector GGX path
  remain future work. There is no material search/filter field yet.
- No 3D geometry editing beyond texture and UV edits on generated surfaces.
- No direct linedef or sidedef deletion.
- No standalone direct-topology vertex deletion; authoring vertices support
  isolated deletion and degree-2 dissolve.
- No automatic duplicate linedef or sidedef merge.
- No arbitrary line cutting or automatic overlap splitting.
