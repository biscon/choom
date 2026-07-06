# SectorEditor Direct Topology Edit Inventory

## Source-of-Truth Invariant

- Editable maps require authoring graph data.
- `SectorTopologyMap` is not an editable map model.
- No-authoring topology maps are an invalid editor edit state, not a supported edit mode.
- Import/migration may read topology-v2 data only to produce authoring graph data.
- After load/import, editor UI actions must edit authoring graph data or fail clearly.
- User-authored sector, line, face, portal, material, decal, and UV properties must not be reclassified as topology-owned state.

## Blocks Player Cleanup

- Old path: SideDef/material inspector and Preview3D UV/material panel called a generic `SetLineDefBlocksPlayer()` entry point for topology linedef IDs.
- New authoring-owned path: `SetAuthoringLineDefBlocksPlayer()` resolves the selected derived topology linedef through existing authoring derivation line mappings and mutates `SectorAuthoringLine::flags.blocksPlayer`.
- Invalid no-authoring edit state: Blocks Player edits now fail with `Cannot edit line flag: authoring data is required.` and do not mutate `SectorTopologyLineDef::flags.blocksPlayer`.
- Derivation path: `DeriveSectorTopologyMapFromAuthoringGraph()` copies `SectorAuthoringLine::flags` into each derived `SectorTopologyLineDef::flags`, including split derived linedefs.
- Serialization: authoring graph line `flags` already serialize via the existing linedef flag helpers; default false is omitted and missing fields load false.
- Manual smoke needed: toggle Blocks Player from the 2D SideDef/material inspector and Preview3D UV/material panel on an authoring-backed portal, save/reload, and optionally open an old topology-v2-only document to confirm editor edits fail clearly or import/migration produces authoring data.

## Remaining Direct Topology Access/Mutation Sites

| file/function | read or write | what it accesses/mutates | category | recommended follow-up | notes |
| --- | --- | --- | --- | --- | --- |
| `SectorEditor::SetAuthoringLineDefBlocksPlayer` / `SetSectorEditorAuthoringLineDefBlocksPlayer` | write | `SectorAuthoringLine::flags.blocksPlayer`, rederived into topology | authoring-owned edit | None for mapped authoring lines; investigate any mapping-gap failures | No-authoring edit state fails and does not mutate topology. |
| `DrawTopologySideDefMaterialInspector` | read | selected linedef/sidedef, portal flag, material display state | OK read-only consumer | Continue routing edits through callbacks/services | Blocks Player display reads derived topology. |
| `DrawSectorEditorPreviewUvPanel` | read | selected 3D surface, sidedef, portal flag, texture display | OK read-only consumer | Continue routing edits through callbacks/services | Blocks Player display reads derived topology. |
| `SectorEditorMaterialEditingService::ApplyAuthoringSideMaterialAction` | write | copies topology, applies material action, writes result back to authoring side | transitional material scratch/writeback | Replace with direct authoring material edits under REF-044 | Acceptable transition; does not leave topology as source of truth. |
| `ApplySectorEditorAuthoringFaceAnchorFlatMaterialAction` | write | copies topology, applies flat material action, writes result back to authoring face anchor | transitional material scratch/writeback | Replace with direct authoring flat material edits under REF-044 | Acceptable transition. |
| `SectorEditorMaterialPickerRouting` authoring routes | write | uses temporary topology picker assignment, then writes selected material back to authoring | transitional material scratch/writeback | Simplify into direct authoring picker apply paths | Some paths temporarily restore `state.topologyMap` after picker routing. |
| `SectorEditorMaterialEditingService::ApplyInspectorSideDefUvValue` / `ResetInspectorSideDefUv` | write | authoring side material UV, rederived into topology | Fixed in REF-068 | None for this path | SideDef inspector UV apply/reset resolves selected topology sidedef through derivation mapping, mutates `SectorAuthoringLineSide` material UV directly, and fails without authoring data or mapping. |
| `SectorEditor` sector inspector callbacks | write | authoring face-anchor height, sky, ambient, material, decal, and sector/default-wall UV fields | authoring-owned edit | None for existing fields; add/follow up any future field missing from authoring graph | Invalid no-authoring edit state fails with an authoring-required status and does not mutate `SectorTopologySector`. |
| `TryRenameSelectedDerivedSectorAuthoringName` | write | authoring face-anchor name | authoring-owned edit | None | Invalid no-authoring edit state fails with an authoring-required status and does not mutate `SectorTopologySector::name`. |
| `OpenMaterialPickerForDerivedSector` / `OpenMaterialPickerForDerivedSideDef` | write | authoring face-anchor/side material targets selected through derived topology IDs | authoring-owned edit | Continue REF-044 direct authoring material cleanup | Editor-facing names indicate derived target selection, not topology-map editing. |
| `SectorEditorTextureActions::RegisterMapTexture` | write | `topologyMap.texturesById` | topology-owned map asset catalog | None | Texture registry is map-level asset catalog state, not a sector/line/material property. |
| preview settings modal/apply path | write | `previewSettings`, sky visual settings, directional light, object probe preview settings | topology-owned preview/bake state | Keep source-hash rules explicit when lights change | Sky visuals are visual-only; directional light affects bake settings. |
| lightmap bake install path | write | `topologyMap.bakedLightmap` metadata | OK derived/cache/render/runtime | None | Bake result metadata is not authoring geometry. |
| light add/delete/move actions | write | static/dynamic light arrays | topology-owned light state for now | Future lights audit remains open | Light authoring currently lives on topology map. |
| runtime object edit paths | write | `topologyMap.runtimeObjects` | topology-owned runtime object state for now | Keep runtime object work separate from topology geometry cleanup | Placed runtime objects are not derived from authoring graph lines/faces. |
| selection/hit-test services and inspectors | read | topology IDs, surfaces, vertices, sectors, sidedefs, lights | OK read-only consumer | None | Reads derived topology for picking and display. |
| preview renderer, collision world, runtime spawn, lightmap bake | read | compiled topology geometry and settings | OK derived/cache/render/runtime | None | Consumption of derived topology is expected. |

## Follow-Up Recommendations

- Continue REF-044 material migration by removing topology scratch/writeback where direct authoring edits are straightforward.
- Treat any authoring-backed Blocks Player mapping failure as a derivation/mapping bug, not a reason to mutate derived topology.
- Audit additional portal/linedef properties before exposing new editable flags.
