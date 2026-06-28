# Phase 13 Inspector Hookup Audit

## Current Working Paths

- 3D wall selection -> side material controls -> authoring side material write.
  - Source selection type: `SectorSurfaceRef` wall kinds selected by `SelectSurface3D()`; this also selects the derived topology sidedef.
  - Inspector/control entry point: preview UV panel material controls in `SectorEditor::DrawPreviewUvPanel()` and the derived sidedef inspector in `SectorEditor::DrawTopologySideDefInspector()`.
  - Authoring target type: `SectorAuthoringLineSide`, resolved from derived `topologySideDefId`.
  - Mutation helper used: `ApplyAuthoringSideMaterialAction()` / `FinishAuthoringSideMaterialActionResult()`, ending in `MutateSectorEditorAuthoringSideForTopologySideDef()`.
  - Rederives through authoring graph: yes. The helper marks the authoring graph edited and calls `RefreshSectorEditorAuthoringDerivation()`, which replaces `state.topologyMap` from the rederived topology and invalidates the 2D topology render cache.

- 3D floor/ceiling selection -> flat material controls -> face anchor material write.
  - Source selection type: `SectorSurfaceRef` floor or ceiling selected by `SelectSurface3D()`.
  - Inspector/control entry point: preview UV panel material controls in `SectorEditor::DrawPreviewUvPanel()`.
  - Authoring target type: `SectorAuthoringFaceAnchor`, resolved from derived `topologySectorId`.
  - Mutation helper used: `ApplyAuthoringFaceAnchorFlatMaterialAction()`, wrapping `game::ApplySectorEditorAuthoringFaceAnchorFlatMaterialAction()`, ending in `MutateSectorEditorAuthoringFaceAnchorForTopologySector()`.
  - Rederives through authoring graph: yes.

- Derived topology sector selection with a current face-anchor mapping -> sector property controls -> face anchor property write.
  - Source selection type: `TopologySelectionKind::Sector`, including the sector selected as a side effect of selecting a 3D floor/ceiling surface.
  - Inspector/control entry point: `SectorEditor::DrawSectorsPanel()` calls `DrawTopologySectorInspector()`.
  - Authoring target type: `SectorAuthoringFaceAnchor`, resolved from selected topology sector ID.
  - Mutation helper used: inline `mutateSelectedAuthoringFaceAnchor`, ending in `MutateSectorEditorAuthoringFaceAnchorForTopologySector()`.
  - Rederives through authoring graph: yes.
  - Hooked properties: floor/ceiling height, `ceilingSky`, ambient intensity, ambient color, and sector UV fields. Texture picker paths can also write floor/ceiling/default materials through authoring face anchors.

- 3D wall/portal selection -> `Blocks Player` checkbox -> authoring line flag write.
  - Source selection type: 3D wall surface with a two-sided derived linedef, or derived sidedef inspector selection.
  - Inspector/control entry point: `SectorEditor::DrawPreviewUvPanel()` and `SectorEditor::DrawTopologySideDefInspector()`.
  - Authoring target type: `SectorAuthoringLine`, resolved from derived `topologyLineDefId`.
  - Mutation helper used: `SetLineDefBlocksPlayer()`, ending in `MutateSectorEditorAuthoringLineForTopologyLineDef()`.
  - Rederives through authoring graph: yes. It also rebuilds the collision world after a changed flag.

- Texture picker authoring write-through.
  - Source selection type: topology sector/sidedef targets opened while authoring graph data exists, plus 3D flat/wall material targets.
  - Inspector/control entry point: `OpenTopologyTexturePicker()`, `OpenTopologySideDefTexturePicker()`, and preview UV panel texture button.
  - Authoring target type: `SectorAuthoringFaceAnchor` or `SectorAuthoringLineSide`.
  - Mutation helper used: `OpenAuthoringFaceAnchorTexturePicker()`, `OpenAuthoringSideTexturePicker()`, and `ApplyAuthoringTexturePickerSelection()`, ending in the face-anchor or side mutation helpers.
  - Rederives through authoring graph: yes. Stale or missing mappings close/reject the picker instead of mutating derived topology.

## Missing or Incomplete Paths

- 2D selected authoring line -> inspector is missing.
  - `SectorAuthoringSelectionTarget` only stores `None`, `Line`, and `Vertex`; selected line ID is stored in `state.selectedAuthoring.lineId`.
  - Graph Select sets this state via `SelectAuthoringLine()` and the bottom/status panel reports `authoring line N`.
  - `DrawSectorsPanel()` does not branch on `state.selectedAuthoring`, so with no topology sector/sidedef/vertex/light selected it reaches `Selected: none`.

- 2D selected authoring line -> authoring side/material target is missing.
  - There is no inspector path that turns `state.selectedAuthoring.lineId` into front/back `SectorAuthoringSideId` controls.
  - Existing side material controls are reachable through derived topology sidedefs or 3D wall surfaces, not directly from graph line selection.

- 2D selected authoring line -> `blocksPlayer` is missing.
  - The flag write exists, but the UI path takes a derived topology linedef ID.
  - Graph line selection has the authoring line ID directly, but no inspector branch uses it.

- 2D click inside derived face -> face/room/anchor inspector is missing.
  - Graph Select picking only checks nearby authoring vertices first and nearby authoring lines second.
  - There is no selected authoring face, room, or anchor kind in `SectorAuthoringSelectionTarget`.
  - Clicking empty interior area clears selection and reports `Selected authoring: none`.

- 2D selected face anchor -> sector/floor/ceiling controls is missing.
  - Face anchor editing exists through selected derived topology sectors and 3D floor/ceiling selection.
  - There is no 2D authoring face-anchor selection model or inspector branch.

- 2D selected authoring vertex -> inspector is also missing.
  - Vertex selection exists for graph editing/deletion and status display, but no authoring vertex inspector branch exists. This is probably lower priority than line/face property editing for phase 13.

## Source-of-Truth Risks

- No authoring-backed phase 13 property path inspected here appears to directly mutate `state.topologyMap` as the final source of truth when authoring graph data is active.
- The authoring-backed paths mutate `SectorAuthoringGraph`, call `MarkSectorEditorAuthoringGraphEdited()`, rederive topology, and invalidate the 2D topology render cache through `RefreshSectorEditorAuthoringDerivation()`.
- Direct topology mutation fallbacks still exist in topology inspectors when there is no authoring graph data or when the selected topology item has no authoring mapping. For mapped authoring data, missing/stale mapping generally blocks edits with a status message instead of mutating derived topology.
- Texture picker legacy fallback still exists for non-authoring targets, but authoring face-anchor and authoring side picker targets route through `ApplyAuthoringTexturePickerSelection()`.

## 2D Graph Selection State

- Selected authoring line ID is stored at `state.selectedAuthoring.lineId`.
- Selected authoring vertex ID is stored at `state.selectedAuthoring.vertexId`.
- `SectorAuthoringSelectionKind` has no face, room, side, or anchor variant.
- Graph Select cannot pick a derived face/sector/room anchor by clicking inside a face. It only picks near authoring vertices or near authoring lines.
- Authoring selection is not passed to inspector drawing code. It is used for hover overlays, delete actions, and bottom/status text, but `DrawSectorsPanel()` only handles topology sector, topology sidedef/linedef, topology vertex, topology light, and inspected topology vertex before falling back to `Selected: none`.

## Inspector Entry Point

- The inspector entry point is `SectorEditor::DrawSectorsPanel()`.
- It handles:
  - selected topology sector through `DrawTopologySectorInspector()`
  - selected topology sidedef/linedef through `DrawTopologySideDefInspector()`
  - selected static light through `DrawSelectedStaticLightInspector()`
  - selected/inspected topology vertex through `DrawTopologyVertexInspector()`
- It does not handle authoring line selection.
- It does not handle authoring side selection.
- It does not handle face-anchor or room-anchor selection.
- It still primarily handles topology selections and 3D selected surfaces that have been converted into topology sector/sidedef selection by `SelectSurface3D()`.
- The 2D authoring line case shows `Selected: none` because graph selection clears topology selection, stores only `state.selectedAuthoring`, and no inspector branch consumes that authoring selection.

## 3D Surface Authoring Mapping

- Wall surfaces map to `SectorAuthoringSideId` through `ResolveSectorEditorAuthoringSurfaceTarget()` using `state.authoringDerivation.mapping.sides`.
- Floor and ceiling surfaces map to `SectorAuthoringFaceAnchor` through `state.authoringDerivation.mapping.sectors`.
- Missing, stale, invalid, or ambiguous mapping blocks selection/editing with status text and clears the selected 3D surface when needed.
- Controls are effectively enabled only when the selected derived topology item has a current authoring mapping:
  - wall material controls require a current side mapping;
  - flat material controls require a current face-anchor mapping;
  - `Blocks Player` requires a two-sided derived linedef and an authoring line mapping.

## Old Direct-Topology Action Buttons

- `Insert Sector Inside`, `Cut Sector`, and `Delete Sector` still appear in `DrawTopologySectorInspector()`.
- `Split Linedef` still appears in `DrawTopologySideDefInspector()`.
- `Merge Into` and `Dissolve Vertex` still appear in `DrawTopologyVertexInspector()`.
- These are still legacy/topology inspector operations. They are not mixed into an authoring inspector path, because no authoring inspector path exists yet.
- The follow-up should not resurrect or port these buttons into authoring inspectors as part of phase 13 hookup.

## Minimal Fix Recommendation

Smallest safe follow-up task before phase 14 legacy retirement:

**Add 2D graph authoring inspector targets for selected authoring line and selected face anchor.**

Scope:

- Add an authoring-line inspector branch in `DrawSectorsPanel()` for `state.selectedAuthoring.kind == Line`.
- Route selected authoring line to:
  - `blocksPlayer` editing on the `SectorAuthoringLine`;
  - front/back authoring side material controls, reusing the existing authoring side mutation/write-through behavior where practical.
- Add 2D face-anchor selection by clicking inside a currently derived face/sector, or add an equivalent authoring face-anchor selection path that maps the derived sector under the cursor to its face anchor.
- Route selected face anchor to the existing sector/floor/ceiling property controls.
- Keep old direct-topology action buttons out of authoring inspectors.
- Do not implement phase 14 legacy retirement in this fix.

This is broader than only "show selected authoring line in the inspector": the line/property gap is real, but face-anchor editing also lacks a 2D selection route. The 3D path is largely hooked up; the missing surface is 2D authoring selection-to-inspector routing.

## Suggested Tests for Follow-up Fix

- Add a helper/unit test that selecting an authoring line produces an inspector target that can resolve the selected authoring line ID without requiring a derived topology selection.
- Add a helper/unit test for selected authoring line `blocksPlayer` mutation that verifies:
  - `SectorAuthoringLine::flags.blocksPlayer` changes;
  - topology is rederived;
  - the projected derived linedef reflects the flag.
- Add helper/unit tests for selected authoring line front/back side material writes that verify authoring side data changes and projects to one or more derived sidedefs.
- Add a helper/unit test for 2D point-inside-derived-face selection resolving to a face-anchor ID.
- Add a helper/unit test for selected face-anchor floor/ceiling/ambient/sky mutation through the new 2D inspector target.
- Add stale/missing mapping tests that verify 2D inspector edits are blocked rather than mutating derived topology.

No GUI, screenshot, or `xdotool` tests are recommended for this follow-up.
