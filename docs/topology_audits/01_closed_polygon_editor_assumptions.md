# Closed-Polygon Editor Assumptions Audit

## Summary

The current editor document is already a topology v2 `SectorTopologyMap`, not the old polygon `SectorMap`. However, the main sector authoring path still creates sectors from an ordered list of points that must become a closed polygon. Runtime-facing topology objects are vertices, linedefs, sidedefs, and sectors, but the editor generally treats a sector as a validated closed loop when drawing, picking sector interiors, rendering fills, deleting sectors, moving shared vertices, launching 3D preview, and saving/loading.

Loose lines would need to live outside the current persisted/runtime topology contract or be treated as invalid editor-only data. Today, `ValidateSectorTopologyMap()` and `ExtractSectorTopologyLoops()` reject open, branching, self-intersecting, zero-area, overlapping, or partially connected sector boundaries, and save/load and preview rebuild paths depend on that valid topology contract.

Implementation note (2026-06-28): This audit is historical context for the
pre-transition editor. The implemented graph-authoritative workflow now keeps
loose, open, crossing, and otherwise invalid authoring state in
`SectorAuthoringGraph`; `SectorTopologyMap` remains the strict derived product.
Normal authoring tools no longer need to force work-in-progress geometry through
closed-polygon topology mutation.

## Editable Source Of Truth Today

- `SectorEditorState::topologyMap` in `sources/sector_editor/SectorEditorTypes.h` is the editable document. It stores `SectorTopologyMap`, plus selection IDs, pending sector draw state, pending line split/merge/cut state, drag state, and cached render revision state.
- `SectorTopologyMap` in `sources/sector_demo/SectorTopologyMap.h` stores topology vertices, linedefs, sidedefs, sectors, static lights, textures, preview settings, sky settings, directional light, and lightmap data.
- `SectorTopologyTypes.h` defines the Doom-like persisted topology pieces: directed `SectorTopologyLineDef` endpoints plus front/back sidedef slots, `SectorTopologySideDef` pointing to a sector, and `SectorTopologySector` containing properties but no explicit authored ordered point list.
- New empty documents are created through `CreateEmptySectorTopologyDocument()` in `sources/sector_editor/SectorEditorDocumentActions.cpp`, which initializes texture defaults and an otherwise empty topology map.
- Current sector authoring bridges from an ordered temporary polygon (`PendingSectorDraw::points`) to the topology map by calling `CreateSectorTopologyPolygon()` or `InsertSectorTopologyPolygon()`.

## Closed Polygon Assumptions

- `PendingSectorDraw` in `SectorEditorTypes.h` stores `std::vector<SectorPoint> points`, not line segments or arbitrary graph edges. Its `kind` is either `NewSector` or `InsertInside`.
- `SectorEditor::FinalizePendingSector()` in `sources/sector_editor/SectorEditor.cpp` requires at least three points, builds topology points, then calls `CreateTopologySector()` or `InsertTopologySectorInside()`. Failed closed-polygon creation leaves the pending draw active with an error.
- `SectorEditor::CanClosePendingSectorAt()` only recognizes closure when the snapped cursor equals the first pending point after at least three points.
- `SectorEditor::ValidatePendingTopologyPoint()` rejects duplicate points and treats clicking the first point as a close action instead of adding that point to the editable data.
- `CreateSectorTopologyPolygon()` in `sources/sector_demo/SectorTopologyCreation.cpp` normalizes an input polygon, removes a duplicate closing point if present, requires at least three unique points, rejects repeated points and zero area, reverses clockwise input to CCW, creates or reuses vertices, creates or reuses boundary linedefs, assigns one sidedef per polygon edge, then validates and extracts loops before committing.
- `InsertSectorTopologyPolygon()` also starts from a normalized closed polygon. It requires the parent sector to already extract as loops, requires the child polygon to be strictly inside the parent outer loop and outside holes, rejects touching/crossing existing topology, creates child and parent hole sidedefs for every edge, then validates and extracts parent and child loops.
- `ExtractSectorTopologyLoops()` in `SectorTopologyValidation.cpp` assumes each sector's sidedefs form one or more closed boundary loops: each boundary vertex must have exactly one incoming and one outgoing edge, loops need at least three edges, loop area must be non-zero, one loop must be CCW outer, and clockwise loops are holes.

## In-Progress Drawing Assumptions

- Mouse handling in `SectorEditor::UpdateEdit2DInput()` adds snapped polygon points for the Sector and Insert Sector Inside tools. If `CanClosePendingSectorAt()` is true, the click finalizes the sector instead of adding a loose line or endpoint.
- `SectorEditor::DrawPendingSector()` draws existing point-to-point segments, a preview segment from the last point to the cursor, and a preview closing segment from the cursor to the first point. Once three points exist, it copies `state.topologyMap` and calls `CreateSectorTopologyPolygon()` or `InsertSectorTopologyPolygon()` on the candidate map to report whether the completed polygon would be valid.
- During pending drawing, invalidity is framed as invalid closed-sector creation. There is no representation for a valid partial loose edge beyond the temporary polyline in `PendingSectorDraw`.
- Insert-inside drawing assumes the selected parent sector remains valid and extractable; `StartInsertSectorInside()` stores a parent sector ID and label, and `InsertSectorTopologyPolygon()` requires valid parent loops.

## Selection / Inspector Assumptions

- Sector picking uses polygon containment. `SectorEditor::FindTopologySectorAt()` iterates sectors and calls `PointInTopologySector()`, which calls `ExtractSectorTopologyLoops()`, tests the outer loop with `StrictPointInPolygon()` / boundary checks, and excludes holes.
- Linedef picking can select a line or sidedef near the cursor, but sidedef selection assumes the picked line belongs to persisted topology and has front/back sidedef slots. It does not create or edit loose authoring-only lines.
- The selected-sector inspector in `SectorEditorSectorInspector.cpp` operates on a `SectorTopologySector&` and exposes sector properties, Delete Sector, Insert Sector Inside, and Cut Sector. Those actions assume a real closed sector ID, not a loose face candidate.
- `Cut Sector` starts from a selected topology sector and asks for boundary points on the selected sector's outer boundary. That is another closed-loop operation, not a general graph cut.
- `DrawTopologyVertexInspector()` and vertex selection can inspect/move topology vertices, but those vertices are persisted topology vertices belonging to validated linedef graphs. The move/merge/dissolve actions are transactional topology edits, not authoring graph edits.

## Save / Load Assumptions

- `LoadSectorTopologyDocumentFromAsset()` loads directly with `LoadSectorTopologyMap()`. `ParseMap()` requires `formatVersion == 2`, `topology == "linedef"`, `coordSubdivisions == SectorCoordSubdivisions`, and required arrays for `vertices`, `linedefs`, `sidedefs`, and `sectors`.
- `ParseMap()` calls `ValidateForSerialization()` before returning. That means loaded files must already satisfy `ValidateSectorTopologyMap()`, including closed sector loop extraction.
- `SerializeMap()` also calls `ValidateForSerialization()` before writing JSON. Invalid loose-line or open-boundary data cannot currently be saved as `SectorTopologyMap`.
- The persisted schema stores topology elements, not an editor-only pending polygon or loose authoring graph. Sector geometry is derived from sidedefs assigned to a sector, and those sidedefs must form extractable loops.

## What Would Break With Loose Lines

- Save/load would reject loose lines because `ValidateSectorTopologyMap()` reports linedefs with no sidedefs, missing sector loops, invalid intersections, open boundary loops, branching/touching boundaries, or sectors with no sidedefs.
- 2D sector fill rendering would skip sectors whose loops cannot be extracted. `BuildSectorEditorTopologyRenderCache()` validates the whole map, extracts loops per sector, triangulates extracted polygons with `mapbox::earcut`, and builds outline segments from loop vertices.
- Sector picking would fail for sectors not derivable into loops because `PointInTopologySector()` returns false when `ExtractSectorTopologyLoops()` fails.
- Erase-tool sector deletion would not find loose lines because it only calls `FindTopologySectorAt()` and then opens `DeleteTopologySector` confirmation.
- Vertex movement would reject many loose/intermediate graph states. `UpdateVertexDrag()` previews by copying the map and calling `MoveSectorTopologyVertex()`; `FinishVertexDrag()` commits through `MoveTopologyVertex()`. The backend move validates the candidate topology before replacing the map.
- 3D preview launch would fail if loose lines live in `state.topologyMap`. `TryEnterPreview3D()` calls `preview.RebuildRendererResources(..., state.topologyMap, ...)`, which expects buildable topology.
- Sector inspector actions such as Insert Inside, Cut, Delete, and material/property edits require an existing `SectorTopologySector` ID. Loose lines would need separate selection and inspector behavior.

## Low-Risk Seams For Introducing An Authoring Graph

- Keep `SectorTopologyMap` as derived/runtime topology and add a separate editor-only authoring source. Current mesh, preview, save, collision, and lightmap paths can continue consuming only validated topology.
- The temporary `PendingSectorDraw` path is a natural seam: it already stores editor-only points and converts them through a single finalization step. A future authoring graph could replace or extend this pending data without changing runtime consumers immediately.
- `SectorEditorTopologyRenderCache` is already derived editor-only state and has an explicit invalidation policy. It could later draw authoring graph overlays separately from derived topology fills.
- The topology action wrappers in `SectorEditorTopologyActions.cpp` are useful boundaries for transactional edits that currently mutate `SectorTopologyMap`. Similar wrappers could derive/update runtime topology from authoring data while preserving editor status and cache invalidation behavior.
- Selection state is already typed by `TopologySelectionKind`; a future authoring graph should likely introduce separate authoring selection kinds rather than overloading validated topology sector/line/sidedef selection.

## Risks / Unknowns

- It is unclear whether loose lines should be persisted as editor-only data alongside derived topology or derived on load from current topology. Current JSON has no authoring graph container.
- Sector identity/property preservation is unresolved: if closed sectors are derived from loose lines, stable sector IDs, sidedef IDs, material properties, lightmap references, and selected IDs need a mapping strategy.
- The current validation model has no "warning-only invalid authoring state" mode for loose lines. It validates persisted/runtime topology as all-or-nothing.
- Insert-inside and sector-cut workflows assume closed parent sectors. They may remain derived-topology operations or need authoring-graph equivalents.
- 2D rendering can already draw invalid linedefs with missing endpoints as warnings, but sector fills and labels are loop-derived. Drawing a mixed authoring graph plus derived sectors needs explicit layering and picking rules.

## Recommended Follow-Up Questions

- Should the future source of truth be an authoring graph only, or should files save both authoring graph and last-derived `SectorTopologyMap`?
- How should sector and sidedef properties anchor when a loose graph is re-derived into faces?
- Should loose lines be allowed to carry sidedef-like material/action metadata before they bound a valid sector?
- Which editor tools should operate on authoring graph data first: draw, split, move, erase, or inspector selection?
- Should 3D preview require a successful derivation step, or should it keep using the previous valid derived topology while the authoring graph is temporarily invalid?
