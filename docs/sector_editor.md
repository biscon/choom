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
- Move tool: drag topology vertices by stable vertex ID or drag static lights by
  X/Z.
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

## Sector Inspector

Selecting a topology sector opens the sector inspector. It edits:

- sector name and stable integer ID display
- floor and ceiling heights
- floor and ceiling texture IDs and UV scale/offset
- ambient color and intensity
- default wall, lower, and upper texture IDs and UV scale/offset
- `Insert Sector Inside`
- `Delete Sector`

Sector default wall/lower/upper settings initialize future sidedefs created for
that sector. Editing those defaults does not rewrite existing concrete sidedefs.

Sector deletion is transactional. It removes sidedefs owned by the sector,
clears those slots from linedefs, removes linedefs with no remaining side, prunes
unreferenced vertices, validates the candidate topology, and commits only if it
is valid. Surviving opposite sides on shared linedefs keep their texture and UV
settings and become one-sided boundaries.

## Sidedef And Linedef Inspector

Selecting near a linedef in 2D selects the clicked side when that side has a
sidedef. If the clicked side is missing but the opposite side exists, the editor
selects the existing opposite sidedef and reports that the clicked side has no
sidedef. If neither side has a sidedef, the editor selects the linedef as a
line-only selection.

The sidedef/linedef inspector supports:

- front/back sidedef selection
- `Switch to opposite side` when the opposite sidedef exists
- wall, lower, and upper texture selection
- wall, lower, and upper UV scale/offset editing
- reset UV for the selected wall/lower/upper part
- `Split Linedef`
- `Split At Point`
- line-only inspection and splitting when no sidedef is selected

`Split Linedef` creates one exact midpoint vertex and replaces the selected line
with two new linedefs. Existing front/back sidedefs are duplicated onto both
replacement lines with fresh stable IDs, preserving sector ownership, side kind,
texture IDs, and UVs. Splitting fails if the midpoint cannot be represented
exactly on the integer coordinate grid.

`Split At Point` starts a pending canvas action for the selected linedef. Click a
snapped point exactly on that linedef to split there, or press Escape/right click
to cancel. The clicked point must be strictly inside the linedef segment; endpoint
points and coordinates already occupied by an existing topology vertex are
rejected without changing the map.

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
- sidedef wall/lower/upper textures
- 3D surface panel texture targets

`Add Map Texture` scans `assets/images` recursively for PNG files. It can add or
update a texture ID in the map texture table and choose point or bilinear
filtering. It does not copy external files into the project.

## Move, Split, And Delete Tools

Moving a topology vertex edits exactly one stable vertex ID. Connected linedefs
and all sector loops using that vertex update through the shared reference.
Movement previews validate a candidate topology before committing on mouse
release. Moving a vertex onto another existing vertex is rejected because vertex
merge is not implemented.

Moving a light edits only its X/Z position during the drag. Y remains unchanged.

Splitting is inspector-driven. It always splits at the exact stored-coordinate
midpoint and fails if that midpoint is not representable on the integer grid.

Sector deletion and vertex movement use copy/validate/commit style topology
edits. Direct linedef deletion, direct sidedef deletion, standalone vertex
deletion, vertex merge, and undo/redo are not available yet.

## 3D Mode

`3D Mode` builds the preview from the current in-memory topology map. It does
not require saving first, but unsaved changes remain unsaved until `Save`.

3D controls:

- `F11`: toggle mouse-look/captured cursor mode.
- `F1`: toggle baked ambient-occlusion display.
- `Tab` or `Escape`: return to 2D mode.
- In mouse-look mode: `WASD` move, mouse looks, `Space` moves up, `Ctrl` moves
  down.
- In visible-cursor mode: click generated surfaces to select/edit them.

3D picking maps generated surfaces back to topology:

- floor and ceiling surfaces select the sector
- wall, lower, and upper surfaces select the concrete sidedef and wall part

The 3D surface panel edits only the selected surface target. Floor/ceiling
targets edit sector texture and UV settings. Wall/lower/upper targets edit the
selected sidedef's matching wall part. The Texture button opens the topology
texture picker, and Reset UV resets only the selected surface target.

Equal-height portals have no generated 3D wall surface, so edit their sidedefs
from the 2D linedef/sidedef inspector.

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
(`6`), atlas and sample constants, coordinate subdivision value, map texture
table, vertex/linedef/sidedef/sector IDs and geometry, sector and sidedef texture
and UV fields, static lights, and bake settings. It does not include the
installed baked-lightmap metadata itself.

The baked PNG stores direct static-light contribution and one-bounce indirect
light in RGB, and ambient occlusion in alpha. 3D Mode uses a baked atlas only
when metadata exists, the atlas file is present, and the stored source hash
matches the current topology. Otherwise the preview falls back to sector ambient
lighting.

Equal-height portals generate no wall surface and therefore no wall lightmap
chart.

## Current Limitations

- No undo/redo.
- No external texture import/copy; texture files must already exist under
  `assets/images`.
- No player-start editing in the sector editor.
- No gameplay mode.
- No collision or floor-constrained 3D preview movement.
- No dynamic runtime lights or dynamic shadows.
- No normal maps, material maps, PBR material editing, or texture search UI.
- Single fixed-size lightmap atlas; no multi-atlas packing.
- No 3D geometry editing beyond texture and UV edits on generated surfaces.
- No direct linedef or sidedef deletion.
- No standalone vertex deletion or vertex merge.
- No arbitrary line cutting or automatic overlap splitting.
