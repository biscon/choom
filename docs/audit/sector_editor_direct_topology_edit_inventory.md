# SectorEditor Direct Topology Edit Inventory

## Summary

- Blocks Player is authoring-owned when authoring graph data exists.
- `SectorTopologyMap` remains derived/compiled data for rendering, runtime, preview, collision, lightmaps, and legacy/no-authoring fallback.
- Direct topology mutation should be limited to legacy fallback, topology-only map state, or render/runtime/cache/bake consumption.

## Blocks Player Cleanup

- Old path: SideDef/material inspector and Preview3D UV/material panel called a generic `SetLineDefBlocksPlayer()` entry point for topology linedef IDs.
- New authoring-owned path: `SetAuthoringOrLegacyLineDefBlocksPlayer()` resolves the selected topology linedef through existing authoring derivation line mappings and mutates `SectorAuthoringLine::flags.blocksPlayer`.
- Fallback path: `SetLegacyTopologyPortalBlocksPlayer()` is retained only for true no-authoring topology maps. If authoring graph data exists and the linedef has no mapping, the edit fails with a mapping-gap status instead of mutating derived topology.
- Derivation path: `DeriveSectorTopologyMapFromAuthoringGraph()` copies `SectorAuthoringLine::flags` into each derived `SectorTopologyLineDef::flags`, including split derived linedefs.
- Serialization: authoring graph line `flags` already serialize via the existing linedef flag helpers; default false is omitted and missing fields load false.
- Manual smoke needed: toggle Blocks Player from the 2D SideDef/material inspector and Preview3D UV/material panel on an authoring-backed portal, save/reload, and verify a no-authoring fallback map if one is available.

## Remaining Direct Topology Access/Mutation Sites

| file/function | read or write | what it accesses/mutates | category | recommended follow-up | notes |
| --- | --- | --- | --- | --- | --- |
| `SectorEditor::SetAuthoringOrLegacyLineDefBlocksPlayer` | write | authoring line flags, or topology linedef flags only without authoring data | OK legacy fallback | None for mapped authoring lines; investigate any mapping-gap failures | Missing mapping with authoring data must not fallback. |
| `SetLegacyTopologyPortalBlocksPlayer` | write | `SectorTopologyLineDef::flags.blocksPlayer` | OK legacy fallback | Keep narrowly named | Used only when no authoring graph data exists. |
| `DrawTopologySideDefMaterialInspector` | read | selected linedef/sidedef, portal flag, material display state | OK read-only consumer | Continue routing edits through callbacks/services | Blocks Player display reads derived topology. |
| `DrawSectorEditorPreviewUvPanel` | read | selected 3D surface, sidedef, portal flag, texture display | OK read-only consumer | Continue routing edits through callbacks/services | Blocks Player display reads derived topology. |
| `SectorEditorMaterialEditingService::ApplyAuthoringSideMaterialAction` | write | copies topology, applies material action, writes result back to authoring side | transitional material scratch/writeback | Replace with direct authoring material edits under REF-044 | Acceptable transition; does not leave topology as source of truth. |
| `ApplySectorEditorAuthoringFaceAnchorFlatMaterialAction` | write | copies topology, applies flat material action, writes result back to authoring face anchor | transitional material scratch/writeback | Replace with direct authoring flat material edits under REF-044 | Acceptable transition. |
| `SectorEditorMaterialPickerRouting` authoring routes | write | uses temporary topology picker assignment, then writes selected material back to authoring | transitional material scratch/writeback | Simplify into direct authoring picker apply paths | Some paths temporarily restore `state.topologyMap` after picker routing. |
| `SectorEditorMaterialEditingService::ApplyInspectorSideDefUvValue` / `ResetInspectorSideDefUv` | write | authoring side material UV, rederived into topology | Fixed in REF-068 | None for this path | SideDef inspector UV apply/reset resolves selected topology sidedef through derivation mapping, mutates `SectorAuthoringLineSide` material UV directly, and fails without authoring data or mapping. No no-authoring topology fallback remains for this path. |
| `SectorEditor` sector inspector callbacks | write | sector height, sky, ambient, and sector/default-wall UV fields | OK legacy fallback | Keep authoring-first; audit any missed sector fields when touched | These try authoring face-anchor mutation first and fallback only when authoring target is unavailable. |
| `TryRenameSelectedTopologySector` | write | sector name or authoring face-anchor name | OK legacy fallback | Keep authoring-first | Fails when authoring exists but derived topology is not current or mapping is missing. |
| `SectorEditorTextureActions::RegisterMapTexture` | write | `topologyMap.texturesById` | OK global topology-only setting | None | Texture registry is map-level compiled/editor state, not authoring geometry. |
| preview settings modal/apply path | write | `previewSettings`, sky visual settings, directional light, object probe preview settings | OK global topology-only setting | Keep source-hash rules explicit when lights change | Sky visuals are visual-only; directional light affects bake settings. |
| lightmap bake install path | write | `topologyMap.bakedLightmap` metadata | OK derived/cache/render/runtime | None | Bake result metadata is not authoring geometry. |
| light add/delete/move actions | write | static/dynamic light arrays | OK global topology-only setting | Future lights audit remains open | Light authoring currently lives on topology map. |
| runtime object edit paths | write | `topologyMap.runtimeObjects` | OK global topology-only setting | Keep runtime object work separate from topology geometry cleanup | Placed runtime objects are not derived from authoring graph lines/faces. |
| selection/hit-test services and inspectors | read | topology IDs, surfaces, vertices, sectors, sidedefs, lights | OK read-only consumer | None | Reads derived topology for picking and display. |
| preview renderer, collision world, runtime spawn, lightmap bake | read | compiled topology geometry and settings | OK derived/cache/render/runtime | None | Consumption of derived topology is expected. |

## Follow-Up Recommendations

- Remove or quarantine remaining legacy/no-authoring topology-edit fallback support where authoring-backed maps should fail instead.
- Continue REF-044 material migration by removing topology scratch/writeback where direct authoring edits are straightforward.
- Treat any authoring-backed Blocks Player mapping failure as a derivation/mapping bug, not a reason to mutate derived topology.
- Keep legacy topology fallback names explicit and narrow.
- Audit additional portal/linedef properties before exposing new editable flags.
