# Sector Editor

The sector editor is a 2D editor for `SectorMap` JSON data with an integrated
3D mesh preview. The app starts in the editor and loads
`assets/sector_demo/sector_editor_working_level.json`.

## Coordinates

- Map `x` is horizontal.
- Map `y` is top-down vertical and is drawn downward on screen to match the JSON
  point layout directly.
- The view center is the map coordinate at the center of the canvas.
- Zoom is measured in pixels per map unit.

## 2D Editor Controls

- `WASD`: pan the 2D view.
- Mouse wheel over the canvas: zoom.
- Left click with Select tool: select an edge, select a sector, or clear
  selection. Edges take priority over sectors when the cursor is near both.
- Left click with Sector tool: add a sector point.
- Left drag with Move tool: move an existing sector vertex on the snapped grid.
- Left click with Erase tool: delete the clicked sector.
- Click the first point, or press `Enter`: close the pending sector.
- Right click or `Escape`: cancel a pending sector or active vertex move.
- `Backspace`: remove the last pending sector point.
- `Delete`: delete the selected sector.
- `Escape`: clear selection, then return to Select tool.
- `Save` and `Reload`: write/read the current map JSON path.

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
- New sectors still receive generated IDs such as `sector_001`, using the first
  available generated ID that does not collide with existing renamed sectors.
- Deletion has no undo/redo yet. Reloading before saving can recover unsaved
  deletes; saving persists the current sector list.

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
settings. Upper wall spans use `Upper` UV settings. The 3D preview uses unsaved
in-memory UV changes when it rebuilds; graphical UV editing or wall selection in
the 3D view is planned but not implemented.

Current JSON writes per-part UV fields only when that part has an override:
`wallUvScale`, `wallUvOffset`, `lowerUvScale`, `lowerUvOffset`,
`upperUvScale`, and `upperUvOffset`. Older maps with edge-level `uvScale` or
`uvOffset` still load; those legacy values are applied to Wall, Lower, and
Upper to preserve the previous shared-UV behavior.

## Texture Picker

Texture fields use a modal picker with a dim backdrop, a scrollable
single-selection texture ID list, a preview image, path text, `Select`, and
`Cancel`. `Cancel` and `Escape` close without changing the map. `Select` and
`Enter` apply the current selection and mark the map dirty only if the value
changed.

The preview uses the editor asset scope and the existing asset manager. Missing
or not-yet-ready textures show the existing placeholder image widget output.

## Move Tool

The Move tool reshapes existing sectors by dragging vertices. Hover a vertex in
the canvas, then drag with the left mouse button. The target position always
snaps to the grid, with nearby existing vertices used as snap targets when
available.

If multiple sector point entries share the same coordinate, they move together.
For example, dragging a corner shared by two adjacent sectors updates both
matching point entries, preserving exact shared edges and portal adjacency.

Moves are validated before they are committed. Invalid moves are rejected and
the current map remains unchanged. Rejected cases include duplicate adjacent
points, collapsed or zero-area sectors, self-intersections, edge crossings,
partial shared-edge overlaps, T-junctions, and sector interior overlaps.

The Move tool only moves existing vertices. It does not split edges, insert
vertices, delete vertices, move whole sectors, create holes/islands, or provide
undo/redo yet.

## 3D Preview

Click `Preview 3D` to rebuild raylib meshes from the current in-memory
`SectorMap` and enter the free-fly preview. The preview uses unsaved editor
changes, including newly drawn sectors, unsaved floor/ceiling edits, sector
texture edits, directed edge texture overrides, and edge UV edits. Saving is not
required before previewing.

Preview controls:

- `WASD`: move.
- Mouse look: look around.
- `Space` / `Ctrl`: move up/down.
- `F11`: toggle cursor capture.
- `Tab` or `Escape`: return to the 2D editor.

Returning to the 2D editor keeps the current map, selection, dirty state, pan,
zoom, and last 3D preview position/look direction. The first preview after
loading or reloading a map starts at the player start; later previews in the
same editor session restore the last preview pose. Preview meshes are reused
while open, unloaded on editor shutdown, and rebuilt every time `Preview 3D` is
clicked.

## Current Limitations

- No collision or floor-constrained movement.
- No player-start editing.
- No true gameplay mode.
- No full Doom-style linedef/sidedef system.
- No vertex insertion/deletion, edge splitting, whole-sector movement, or
  undo/redo.
- No texture importing, thumbnail grid browser, 3D wall selection, graphical UV
  editing, doors, lifts, lighting, mesh culling, or file dialog.
