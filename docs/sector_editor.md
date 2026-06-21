# Sector Editor

The sector editor is a 2D editor for `SectorMap` JSON data with an integrated
3D Mode. The app starts with a blank, unmodified level containing the default
texture table required by newly created sectors.

Levels use one directory per level:

```text
assets/levels/<level_name>/<level_name>.json
assets/levels/<level_name>/<level_name>.lightmap.png
```

Level names may contain only letters, digits, underscores, and dashes.

## Coordinates

- Sector map JSON uses authoring units. `1` authoring unit is `0.125` world
  units.
- Grid `8` corresponds to the previous 1-world-unit snap spacing. Grid `1`
  enables fine 1/8-scale detail.
- The default grid size is `8`.
- Map `x` is horizontal.
- Map `y` is top-down vertical and is drawn downward on screen to match the JSON
  point layout directly.
- The view center and zoom are calibrated to world/editor scale, while status
  text and saved values remain authoring units.

Maps authored before this calibration must multiply authored spatial values by
eight to preserve their physical size. This includes sector points,
floor/ceiling heights, player and light positions, light radius/source radius,
ambient occlusion radius, and indirect bounce radius. Colors, intensities,
texture paths/IDs/filtering, and UV scale/offset values are unchanged.

## 2D Editor Controls

- `WASD`: pan the 2D view.
- Mouse wheel over the canvas: zoom.
- Left click with Select tool: select an edge, select a sector, or clear
  selection. Static lights take priority when the cursor is directly over a
  light icon; otherwise edges take priority over sectors when the cursor is
  near both.
- Left click with Sector tool: add a sector point.
- Left click with Light tool: add a baked static point light inside the clicked
  sector.
- Left drag with Move tool: move a static light icon or an existing sector
  vertex on the snapped grid.
- Left click with Erase tool: delete the clicked light or sector.
- Click the first point, or press `Enter`: close the pending sector.
- Right click or `Escape`: cancel a pending sector, active light move, or active
  vertex move.
- `Backspace`: remove the last pending sector point.
- `Delete`: delete the selected light or sector.
- `Escape`: clear selection, then return to Select tool.
- `New`: confirm and reset to a blank level without deleting files.
- `Load`: select a canonical level under `assets/levels`; unsaved edits require
  confirmation before loading.
- `Save`: open the level-name dialog. Changing the current name performs Save
  As, with confirmation before overwriting another level.
- `Reload`: confirm and reload the current saved level. It is unavailable for
  an unsaved blank level.
- `Add Map Texture`: add an existing project PNG from `assets/images` to the
  current map texture table.
- `Bake Lightmaps`: bake static point-light direct lighting and ambient
  occlusion to a shared atlas.

## Sector Inspector

- The right inspector no longer contains a sector list. Select sectors by
  clicking them in the graph/canvas; sector IDs can still be shown in the map
  view.
- Inspector content is vertically scrollable and should avoid horizontal
  scrolling at the normal right-panel width.
- Edit the selected sector `Id` field and press `Enter` to rename it.
- Sector IDs must be unique and non-empty.
- Sector IDs may contain letters, digits, underscores, and dashes.
- Sector floor, ceiling, wall, lower wall, and upper wall textures are editable
  with compact `>` buttons. The editor opens a centered modal texture picker instead
  of using a dropdown, so large texture lists can scroll without clipping off
  screen.
- The `Lighting` section edits the selected sector's ambient intensity and RGB
  tint. The swatch shows the final clamped tint that will be applied to generated
  3D mesh vertices.
- New sectors still receive generated IDs such as `sector_001`, using the first
  available generated ID that does not collide with existing renamed sectors.
- Deletion has no undo/redo yet. Reloading before saving can recover unsaved
  deletes; saving persists the current sector list.

### Insert Sector Inside

With a sector selected, `Insert Sector Inside` starts a contained-sector draw.
Left click adds snapped points, clicking the first point or pressing `Enter`
finalizes, `Backspace` removes the last point, and right click or `Escape`
cancels. The polygon must be strictly inside the selected sector's usable area;
it may not touch the outer boundary, an existing hole, or another sector.

Finalizing adds the reversed polygon as a hole in the parent and creates a new
selected child sector from the forward polygon. The child inherits the parent's
heights, surface and wall texture defaults, ambient lighting, and floor/ceiling
UV defaults. It does not inherit holes or edge overrides. Repeating the workflow
on the child creates nested platforms, pits, or pedestals.

Outer rings are stored counter-clockwise and hole rings clockwise. This makes a
parent hole edge the exact directed reverse of its child outer edge, allowing
the existing portal lower/upper wall generation to handle height differences.
Parent hole edges and child outer edges are independently editable directed
sides. Both initially use their owning sector defaults, and selecting either
side exposes the normal wall/lower/upper texture, UV, reset, and split controls.

Sector JSON stores holes only when present:

```json
"holes": [
  [[24, 24], [24, 40], [40, 40], [40, 24]]
]
```

Older maps without `holes` continue to load with no holes. Deleting a child
does not automatically remove or heal its parent hole.

## Sector Ambient Lighting

Sector ambient lighting is the first simple lighting layer for mood, darkness,
and local color tinting. It is stored per sector and applied when 3D sector
meshes are generated.

- `ambientIntensity`: range `0.0` to `1.0`; `1.0` keeps normal texture
  brightness and `0.0` makes the surface black for this phase.
- `ambientColor`: RGB tint stored as three `0..255` channel values.
- Defaults are white `{255, 255, 255}` and intensity `1.0`.

Generated floor, ceiling, solid wall, lower portal wall, and upper portal wall
vertices receive `ambientColor * ambientIntensity`. Portal wall spans use the
owning directed sector side's lighting, so two sides of a shared portal may have
different tints. This lighting only tints/darkens the base texture; it is not
dynamic lighting, shadows, GI, lightmaps, probes, SSAO, PBR, or fog. Future
lighting work may add static lightmaps and cheap dynamic lights.

Sector JSON saves lighting fields on each sector:

```json
"ambientColor": [90, 130, 115],
"ambientIntensity": 0.35
```

Older maps without these fields still load with the default white/full-bright
ambient lighting.

## Static Baked Lights

The Light tool places static point lights used only by the lightmap baker. They
are not dynamic runtime lights and do not cast dynamic shadows. Click inside a
sector to add a light at the snapped map X/Z position and at the sector floor
height plus `1.8`.

Static light defaults:

- Color: white.
- Intensity: `1.0`, clamped to `0.0..8.0`.
- Radius: `64.0` authoring units, equivalent to `8.0` world units.
- Source radius: new lights default to `2.0` authoring units, equivalent to
  `0.25` world units, and is capped to half the light radius.
- Generated IDs use `light_001`, `light_002`, and so on.

In Select mode, clicking near a light icon selects it before sector or edge
selection. The inspector shows ID, X/Y/Z position, intensity, radius, RGB
channels, source radius, and a color swatch. `sourceRadius` controls the baked
emitting size: `0.0` keeps the hard point-light shadow path, while nonzero values
use deterministic finite-source samples to create baked soft penumbrae. The
Erase tool or `Delete` removes the selected or clicked light without
confirmation. There is still no undo/redo.

Static lights are saved in map JSON as `staticLights`. Older maps without the
field still load normally. Older static lights without `sourceRadius` load as
`0.0`, preserving the previous hard-shadow look until edited.

```json
{
  "id": "light_001",
  "position": [4.0, 2.0, 3.0],
  "color": [255, 185, 110],
  "intensity": 2.0,
  "radius": 8.0,
  "sourceRadius": 0.35
}
```

## Baked Lightmaps

`Bake Lightmaps` starts an asynchronous worker-thread bake of direct colored
lighting from static point lights, one-bounce indirect fill, and ambient
occlusion into a single shared lightmap atlas. The editor window keeps
rendering while the bake runs. The output path is derived from the current map
identity. Save the level before baking; unsaved levels do not use a temporary
lightmap path. For example:

```text
assets/levels/awesome_level/awesome_level.json
assets/levels/awesome_level/awesome_level.lightmap.png
```

The left tools panel includes compact lightmap settings:

- `AO radius`: world-space ray distance for baked ambient occlusion, clamped to
  `0.05..16.0`.
- `AO strength`: how much gathered occlusion darkens sector ambient, clamped to
  `0.0..1.0`; `0.0` disables AO ray work.
- `Bounce radius`: maximum world-space distance for one-bounce indirect gather
  rays, clamped to `0.05..16.0`.
- `Bounce strength`: artist-facing multiplier for indirect fill, clamped to
  `0.0..1.0`; `0.0` disables bounce ray work.

Map JSON saves these settings as:

```json
"lightmapSettings": {
  "ambientOcclusionRadius": 1.25,
  "ambientOcclusionStrength": 0.55,
  "indirectBounceRadius": 4.0,
  "indirectBounceStrength": 0.2
}
```

The baked PNG stores:

```text
RGB = baked direct static point-light contribution + one-bounce indirect light
A   = baked ambient-occlusion factor, where 255 is fully open
```

The map stores `bakedLightmap` metadata with the asset-relative atlas path,
dimensions, and a deterministic source hash. The hash includes the bake format
version, sector geometry, floor and ceiling heights, static light values
including `sourceRadius`, AO settings, bounce settings, fixed bake sample
counts, and the neutral bounce albedo constant. It does not include the baked
metadata itself.

3D Mode uses the baked atlas only when the metadata exists, the atlas file can
be found, and the stored source hash matches the current in-memory map. If a
geometry or static-light edit changes the hash, the bake is reported as stale
and rendering falls back to sector ambient lighting only. When an asynchronous
bake finishes, the main thread validates this same source hash before
installing the generated PNG. If the map changed during the bake, the temporary
output is discarded and the previous lightmap remains intact.

Lighting combines as:

```text
bakedSample = texture(lightmapAtlas, secondaryUv)
ambient = sectorAmbientVertexColor * bakedSample.a
direct = bakedSample.rgb
finalColor = baseTexture * clamp(ambient + direct, 0..1)
```

Ambient occlusion modulates only the sector ambient layer; it does not darken
the baked direct or indirect contribution. Sector ambient remains the baseline
mood/darkness layer. A sector with ambient intensity `0.0` can still be lit by
baked light. Maps without valid baked data continue to render with ambient
vertex lighting only.

The indirect pass is currently one bounce only. It shoots 8 deterministic
cosine-weighted hemisphere rays from each valid chart texel, finds the first
nearby visible surface hit, maps that hit back through its secondary lightmap
UV, and samples only the direct-light float buffer. It uses a fixed neutral grey
reflectance for now. It does not recursively feed indirect results back into
the bake, and it does not use sector ambient or base-texture pixels as bounce
sources.

In 3D Mode, `F1` toggles baked ambient occlusion on and off for visual
debugging. This only changes the shader's use of lightmap alpha: AO off renders
sector ambient without AO darkening while keeping baked direct RGB lighting
visible. The toggle does not rebake lighting, reload textures, rebuild meshes,
mark the map dirty, or save to level JSON.

The bake format version is currently `3`. Existing version-2 bakes contain
direct RGB lighting plus AO alpha, so they intentionally become `Lightmap stale
- rebake required` after this update. The maps still load normally; rebake to
generate indirect RGB.

Current bake characteristics:

- Fixed `2048 x 2048` atlas.
- About `8` texels per world unit.
- At least `2` texels of chart gutter.
- Wall charts are rectangular; floor and ceiling charts are one chart per
  generated triangle.
- Colored direct point-light contribution in RGB.
- One-bounce indirect light is added to RGB.
- Ambient occlusion factor in alpha.
- Hard static shadows when `sourceRadius` is `0.0`.
- Baked soft shadows from 8 deterministic finite-source samples when
  `sourceRadius` is nonzero.
- Baked AO from 12 deterministic cosine-weighted hemisphere samples.
- Baked indirect fill from 8 deterministic cosine-weighted hemisphere samples.
- Bake-local static BVH acceleration for direct shadow rays, soft-shadow source
  samples, AO rays, and one-bounce indirect gather rays.
- Worker-thread execution for the CPU bake, with one bake worker at a time.
- No recursive bounces, path tracing, texture/material-colored bounce, dynamic
  light contribution, dynamic shadows, shadow maps, SSAO, probes, normal maps,
  PBR, GI denoising, parallel ray batches, runtime BVH usage, or multi-atlas
  support.

Every bake builds a static BVH from that bake's generated triangle list after
lightmap layout exists and before lighting rays are cast. The BVH stores only
triangle indices; the generated bake triangles remain the source of
world-space positions, normals, and lightmap UVs. This is a CPU bake
optimization only. It is not reused between bakes, and it is not used by
runtime rendering, gameplay collision, or editor picking.

While baking, a centered modal shows the current phase, approximate progress,
and elapsed time. Phases include layout, BVH build, direct lighting, ambient
occlusion, indirect bounce, and dilation/output. Progress is coarse and
weighted by phase, so it is intended to show forward movement rather than exact
timing. The modal blocks normal editor interaction: map edits, texture pickers,
save/reload, and 3D Mode entry are unavailable until the bake completes,
fails, or is cancelled.

`Cancel` requests cancellation and waits for the worker to exit at the next
coarse chunk boundary. Cancellation does not replace the current lightmap PNG
or baked metadata, and temporary bake output is removed. If the bake fails, the
previous valid lightmap remains intact.

The worker performs CPU-only work and writes a temporary PNG. GPU upload and
texture refresh happen later on the main thread through the existing asset
manager. Newly baked lightmaps keep bilinear filtering. If 3D Mode is entered
after a bake, its preview scope requests the freshly installed atlas; if a
future bake completes while 3D Mode is active, the preview is rebuilt on the
main thread.

The 2D status bar and 3D overlay show `Lightmap valid`, `Lightmap stale -
rebake required`, or `No baked lightmap`. Re-bake after geometry, static-light,
source-radius, AO-setting, or bounce-setting changes.

After a successful bake, the console prints a report with atlas dimensions,
valid chart texels, valid atlas occupancy, allocated chart rectangle area,
chart rectangle occupancy, chart payload efficiency, static geometry triangle
count, BVH node/leaf counts, BVH leaf statistics, static light count, per-pass
ray/AABB/triangle-test statistics, and timings for layout, BVH build, direct
lighting, AO, indirect bounce, gutter dilation/export, and total bake time.

## Edge Inspector

With the Select tool, click near a sector edge to select that directed edge.
The selected edge is highlighted in the 2D canvas and the inspector shows its
own sector, edge index, endpoints, selected side, and opposite side when an
exact reversed-edge neighbor exists.

When two opposite directed edges share the same physical line, selection uses
the raw mouse position to choose the side whose sector contains the cursor. The
selected side is drawn with a slight inward offset and normal marker so shared
edges remain visually distinguishable. Shared edges also expose `Edit opposite
side`, which switches the inspector to the reversed edge without changing map
data.

Edge texture rows show compact `Wall`, `Lower`, and `Upper` labels plus the
effective texture. Inherited texture text uses the muted text color; overridden
texture text uses the accent color. Use `>` to override wall, lower wall, or
upper wall textures for the selected side only. In edge texture pickers, the
first row is `<sector default>`; selecting it clears that one edge texture
override and returns that field to the sector-wide texture.

Edge overrides are directed sector-side data. If two neighboring sectors share a
physical edge, each sector side can use different textures and UV settings.
This is still a transitional sector polygon format, not a full linedef/sidedef
topology system.

The edge inspector also exposes numeric UV controls split by wall part. Use the
`Wall`, `Lower`, and `Upper` toggle buttons to choose which wall part is being
edited:

- `Scale U/V`: scales texture coordinates for the selected wall part.
- `Offset U/V`: offsets texture coordinates for the selected wall part.
- `Reset <part> UV`: clears UV overrides only for the selected wall part and
  returns it to scale `{1, 1}` and offset `{0, 0}`.

Solid outer wall spans use `Wall` UV settings. Lower wall spans use `Lower` UV
settings. Upper wall spans use `Upper` UV settings. 3D Mode uses unsaved
in-memory UV changes when it rebuilds and can edit floor, ceiling, wall, lower
wall, and upper wall UVs directly from the 3D view.

Current JSON writes per-part UV fields only when that part has an override:
`wallUvScale`, `wallUvOffset`, `lowerUvScale`, `lowerUvOffset`,
`upperUvScale`, and `upperUvOffset`. Older maps with edge-level `uvScale` or
`uvOffset` still load; those legacy values are applied to Wall, Lower, and
Upper to preserve the previous shared-UV behavior.

Edge overrides identify their boundary ring explicitly. Outer overrides use
`"ring": "outer"`; hole overrides use `"ring": "hole"` plus a zero-based
`"hole"` index. Both use `"edge"` for the edge index within that ring. Legacy
entries without `ring` continue to load as outer-ring overrides.

Floor and ceiling UV overrides are sector-level fields. JSON writes
`floorUvScale`, `floorUvOffset`, `ceilingUvScale`, and `ceilingUvOffset` only
when those values are overridden. Older maps without these fields load with
scale `{1, 1}` and offset `{0, 0}`.

## Texture Picker

Texture fields use a modal picker with a dim backdrop, a scrollable
single-selection texture ID list, a preview image, path text, `Select`, and
`Cancel`. `Cancel` and `Escape` close without changing the map. `Select` and
`Enter` apply the current selection and mark the map dirty only if the value
changed.

The preview uses the editor asset scope and the existing asset manager. Missing
or not-yet-ready textures show the existing placeholder image widget output.

## Add Map Texture

`Add Map Texture` opens a centered modal for adding an existing project PNG to
the current map texture table. This is not an external import flow: it does not
open native file dialogs, copy files, or browse outside the project assets
directory.

The modal scans `assets/images` recursively and lists PNG files only. Extension
matching is case-insensitive, so `.png`, `.PNG`, and mixed-case variants are
accepted. Stored paths are asset-relative, for example
`assets/images/walls/brick_wall.png`, and never absolute filesystem paths.

Selecting a file generates a default texture ID from the filename stem, with
spaces and punctuation normalized to underscores. Generated IDs are made unique
with numeric suffixes such as `_001`. The ID can be edited before adding, but it
must be non-empty, unique in the current map texture table, and contain only
letters, digits, underscores, and dashes.

Each added texture stores a filter mode:

- `Point`: nearest-neighbor sampling for pixel art.
- `Bilinear`: smoothed sampling and the default for old texture JSON entries.

After adding, the new texture ID is immediately available in the existing
sector, edge, and 3D Mode texture pickers. Texture assignments still store only
texture ID references on sectors and edges.

Texture JSON now saves in object form:

```json
"textures": {
  "brick_wall": {
    "path": "assets/images/walls/brick_wall.png",
    "filter": "point"
  }
}
```

Older string texture entries such as `"wall": "assets/images/wall.png"` still
load and are treated as bilinear. Saving rewrites all texture entries using the
new object format.

## Move Tool

The Move tool moves static lights and reshapes existing sectors. Hover a light
icon or vertex in the canvas, then drag with the left mouse button. The target
position always snaps to the grid, with nearby existing vertices used as snap
targets when moving vertices.

Static lights take priority over vertices when both are under the pointer.
Dragging a static light changes only authored X/Z. Height/Y, radius, source
radius, intensity, and color remain edited numerically in the inspector.

If multiple sector point entries share the same coordinate, they move together.
For example, dragging a corner shared by two adjacent sectors updates both
matching point entries, preserving exact shared edges and portal adjacency.

Moves are validated before they are committed. Invalid moves are rejected and
the current map remains unchanged. Rejected cases include duplicate adjacent
points, collapsed or zero-area sectors, self-intersections, edge crossings,
partial shared-edge overlaps, T-junctions, and sector interior overlaps.

The Move tool does not split edges, insert vertices, delete vertices, move
whole sectors, move lights in 3D Mode, edit light height/radius handles, create
holes/islands, or provide undo/redo yet.

## 3D Mode

Click `3D Mode` to rebuild raylib meshes from the current in-memory
`SectorMap` and enter the 3D editor mode. 3D Mode uses unsaved editor
changes, including newly drawn sectors, unsaved floor/ceiling edits, sector
texture edits, directed edge texture overrides, and edge UV edits. Saving is not
required before entering 3D Mode.

3D Mode controls:

- Hidden cursor: fly only. `WASD` moves, mouse look rotates, and
  `Space` / `Ctrl` move up/down. Hover, selection, and the UV panel are hidden.
- Visible cursor: camera is locked. Hover surfaces to highlight them and click a
  highlighted surface to select it.
- `F11`: toggle between hidden-cursor fly mode and visible-cursor edit mode.
- `F1`: toggle baked ambient occlusion for visual comparison. The toggle affects
  sector ambient lighting only; baked direct light remains visible.
- `Tab` or `Escape`: return to the 2D editor.

Selectable 3D surfaces are floor, ceiling, solid wall, lower portal wall, and
upper portal wall. Wall-like selections map to the directed sector edge and the
same `Wall`, `Lower`, or `Upper` UV data used by the 2D edge inspector. Floor
and ceiling selections map to the sector-level floor or ceiling UV override.

When a surface is selected in visible-cursor mode, a bottom UV panel shows the
surface type, sector id, edge index for wall-like surfaces, current texture id,
numeric UV scale/offset fields, `Texture`, and `Reset UV`. Numeric scale uses
`0.01..64`; numeric offset uses `-1024..1024`.

UV and texture edits update the in-memory `SectorMap`, mark the map dirty, and
rebuild the 3D Mode meshes immediately without leaving 3D Mode. The current
camera pose and selected surface are preserved when the rebuild succeeds. Use
`Save` later in the 2D editor to persist changes.

The `Texture` button opens the same modal picker used by the 2D inspector.
Floor and ceiling selections assign sector floor/ceiling textures. Wall, lower
wall, and upper wall selections assign directed edge texture overrides for the
selected wall part; choosing `<sector default>` clears that edge override.

Returning to the 2D editor keeps the current map, selection, dirty state, pan,
zoom, and last 3D Mode position/look direction. The first 3D Mode entry after
loading or reloading a map starts at the player start; later entries in the
same editor session restore the last 3D pose. Meshes are reused while open,
unloaded on editor shutdown, and rebuilt every time `3D Mode` is clicked or a
3D UV edit changes the map.

## Current Limitations

- No collision or floor-constrained movement.
- No player-start editing.
- No true gameplay mode.
- No full Doom-style linedef/sidedef system.
- No vertex insertion/deletion, edge splitting, whole-sector movement, or
  undo/redo.
- Lightmaps are single-atlas, synchronous, fixed-resolution, CPU ray traced
  with a bake-local BVH, and direct plus one indirect bounce; there is no bake
  progress UI, threading, GPU acceleration, BVH reuse between bakes, dynamic
  lighting, or multi-atlas support.
- No external texture importing, texture copying, texture removal, unused
  texture cleanup, normal/material maps, thumbnail grid browser, texture search,
  3D texture painting, 3D geometry editing, doors, lifts, dynamic lights,
  dynamic shadowing, mesh culling, or file dialog.
