# SectorEditorTypes Dependency Audit

## Summary

REF-008 through REF-011 reduced `SectorEditorTypes.h` from the broad 784-line header described in `docs/audit/codebase_architecture_audit.md` to a 263-line compatibility umbrella plus two remaining aggregate structs. The split moved cached topology draw data, modal/picker state, passive surface targets, selection/picking/drag state, preview pilot/control enums, and async lightmap bake state into focused headers.

The improvement is real: narrow headers now exist and several editor public headers already include them directly. The remaining broad dependency is mostly `SectorEditorState` and `SectorEditorUiState`, not the previously extracted passive type clusters. Another immediate passive-state split is optional rather than urgent; the highest-value next step is a small include cleanup/verification pass, then move on to REF-013 unless editor-header churn is specifically desired.

## Scope And Method

Files inspected:

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorTopologyRenderCacheTypes.h`
- `sources/sector_editor/SectorEditorModalTypes.h`
- `sources/sector_editor/SectorEditorSurfaceTypes.h`
- `sources/sector_editor/SectorEditorSelectionTypes.h`
- `sources/sector_editor/SectorEditorPreviewTypes.h`
- `sources/sector_editor/SectorEditorLightmapAsyncTypes.h`
- direct include users under `sources/sector_editor`
- relevant references in `docs/audit/codebase_architecture_audit.md`
- REF-008 through REF-012 entries in `docs/plans/codebase_refactor_backlog.md`

Commands used are listed in the appendix. This is regex/static include inspection only. It does not prove that an include is removable without compiling after source edits, and no source edits were made.

## Current Header Layout

`SectorEditorTypes.h`

- Responsibility: compatibility umbrella plus core editor state: editor tool/mode enums, authoring derivation state enum, UV action enums, `TopologyMaterialPayload`, `SectorEditorState`, and `SectorEditorUiState`.
- Important dependencies/includes: re-exports all split editor type headers at lines 4-9; still directly includes UI, collision, authoring graph, controllers, runtime objects, texture/topology creation/edit/map, and raylib at lines 3 and 10-21.
- Assessment: still broad, but much narrower than before. Its current breadth is mostly explained by concrete fields in `SectorEditorState` and `SectorEditorUiState`.

`SectorEditorTopologyRenderCacheTypes.h`

- Responsibility: cached 2D topology/editor draw structs and `SectorEditorTopologyRenderCache`.
- Important dependencies/includes: `SectorAuthoringGraph.h`, `SectorTopologyCreation.h`, raylib, strings/vectors at lines 3-10.
- Assessment: acceptable. It is render-cache specific and no longer drags the full editor state header by itself.

`SectorEditorModalTypes.h`

- Responsibility: texture picker, add-texture modal, sprite picker/catalog, save/load/confirmation modal, decal tint modal, door texture settings modal, and preview settings modal state.
- Important dependencies/includes: asset handles, UI state, `SectorEditorSurfaceTypes.h`, FPS controller config, lightmap bake settings, texture/topology map/types, raylib, functional/string/vector at lines 3-16.
- Assessment: acceptable but moderately broad. The preview settings modal state makes this header pull in FPS, sky, directional light, and lightmap settings.

`SectorEditorSurfaceTypes.h`

- Responsibility: passive surface/material target enums and `TopologySurfaceEditTarget`.
- Important dependencies/includes: only `SectorTopologyTypes.h` at line 3.
- Assessment: narrow.

`SectorEditorSelectionTypes.h`

- Responsibility: pending authoring tool state, topology/authoring selection targets, 3D surface hit/ref, picking candidates, select/drag state.
- Important dependencies/includes: `SectorTopologyCreation.h`, raylib, string at lines 3-7.
- Assessment: acceptable. It is canvas/selection focused, though it still uses topology coordinate types.

`SectorEditorPreviewTypes.h`

- Responsibility: preview control/debug overlay enums and spotlight pilot state.
- Important dependencies/includes: `SectorViewPose.h` and raylib at lines 3-5.
- Assessment: narrow.

`SectorEditorLightmapAsyncTypes.h`

- Responsibility: async lightmap bake result, atomic progress, and worker/result state.
- Important dependencies/includes: `SectorLightmap.h`, atomic/mutex/optional/string/thread at lines 3-10.
- Assessment: narrow by ownership, but intentionally heavy because it owns a thread, mutex, atomics, and full bake result.

## SectorEditorTypes.h Remaining Contents

What remains:

- Editor tool/mode state: `SectorEditorTool` and `SectorEditorMode` at lines 31-49.
- Authoring derivation state: `SectorEditorAuthoringDerivationState` at lines 51-56.
- Material/UV action enums: `TopologyUvFitMode` and `TopologyUAlignDirection` at lines 58-67.
- Material clipboard payload: `TopologyMaterialPayload` at lines 69-74.
- Broad editor state aggregate: `SectorEditorState` at lines 76-190.
- UI input/scroll/buffer state aggregate: `SectorEditorUiState` at lines 192-261.
- Compatibility includes for all split headers: lines 4-9.

Notable `SectorEditorState` groups:

- Document and authoring state: topology map, authoring graph, derivation result, last-valid topology, document dirty/status fields at lines 77-89.
- Render-cache state: topology render revision/cache at lines 89-90.
- Tool/view/grid state: current tool/mode, camera pan/zoom, grid at lines 92-97.
- Selection/hover/drag/pending authoring state: lines 99-132.
- Default texture/height authoring state: lines 133-140.
- UI-visible flags and current level/document status: lines 141-149.
- Runtime/preview/collision/controller state: runtime objects, debug overlay, freefly/FPS controller state, collision world/results, visual step/headbob/landing, preview pose, spotlight pilot at lines 150-169.
- 3D surface/material state: hovered/selected 3D surfaces, selected edit target, copied material at lines 170-173.
- Editor asset handles and modal/picker states: lines 175-189.

Candidate future splits:

- `SectorEditorUiState` could become `SectorEditorUiStateTypes.h` if inspector/modals keep causing public include pressure.
- `TopologyMaterialPayload`, `TopologyUvFitMode`, and `TopologyUAlignDirection` could move next to material action types, possibly into a small material payload/action type header.
- `SectorEditorTool`, `SectorEditorMode`, and `SectorEditorAuthoringDerivationState` could move to a small editor core/tool state header if `SectorEditorTopologyRenderCache.h` or helper headers need them without full editor state.
- `SectorEditorState` itself should probably remain broad until a larger editor-state plan exists.

## Include / Dependency Review

Current direct include users of `SectorEditorTypes.h` under `sources/sector_editor`:

- `SectorEditor.h:11`
- `SectorEditorAuthoringState.h:5`
- `SectorEditorDocumentActions.h:4`
- `SectorEditorHelpers.h:7`
- `SectorEditorLightInspector.h:6`
- `SectorEditorLightmapModal.h:7`
- `SectorEditorMaterialActions.h:6`
- `SectorEditorPreviewActions.h:3`
- `SectorEditorPreviewSettingsModal.h:7`
- `SectorEditorSectorInspector.h:6`
- `SectorEditorTextureModals.h:8`
- `SectorEditorTopologyRenderCache.h:5`
- `SectorEditorVertexInspector.h:6`

Current direct include users of the split headers:

- `SectorEditorTopologyRenderCacheTypes.h`: `SectorEditorTypes.h:9`, `SectorEditorTopologyRenderCache.h:4`.
- `SectorEditorModalTypes.h`: `SectorEditorTypes.h:5`, `SectorEditorDocumentActions.h:3`, `SectorEditorHelpers.h:3`, `SectorEditorMaterialActions.h:3`, `SectorEditorPreviewSettingsModal.h:6`, `SectorEditorTextureModals.h:6`.
- `SectorEditorSurfaceTypes.h`: `SectorEditorTypes.h:8`, `SectorEditorModalTypes.h:5`, `SectorEditorHelpers.h:6`, `SectorEditorMaterialActions.h:5`, `SectorEditorTextureModals.h:7`.
- `SectorEditorSelectionTypes.h`: `SectorEditorTypes.h:7`, `SectorEditor.h:9`, `SectorEditorAuthoringState.h:4`, `SectorEditorHelpers.h:5`, `SectorEditorMaterialActions.h:4`, `SectorEditorTopologyRenderCache.h:3`.
- `SectorEditorPreviewTypes.h`: `SectorEditorTypes.h:6`, `SectorEditorHelpers.h:4`.
- `SectorEditorLightmapAsyncTypes.h`: `SectorEditorTypes.h:4`, `SectorEditor.h:7`, `SectorEditorLightmapModal.h:6`.

High-value candidate direct include migrations:

- `SectorEditorLightmapModal.h` appears to need `LightmapBakeAsyncState` only from split editor type headers at line 27; its `SectorEditorTypes.h` include at line 7 is likely compatibility-only and could be tested for removal.
- `SectorEditorPreviewSettingsModal.h` uses `SectorPreviewSettingsModalState` and topology/lightmap settings helpers at lines 16-72. It already includes `SectorEditorModalTypes.h`; its `SectorEditorTypes.h` include at line 7 is likely removable if transitive topology declarations are made explicit.
- `SectorEditorTopologyRenderCache.h` uses `SectorEditorTool` at line 19, selection types at lines 20 and 34-35, render-cache types at lines 38-94, and topology/authoring types at lines 39-41. It currently includes the umbrella for `SectorEditorTool` and backend topology/authoring types; a small `SectorEditorToolTypes.h` or explicit backend includes would let it avoid the umbrella.
- `SectorEditorMaterialActions.h` uses `TopologyMaterialPayload`, `TopologyUvFitMode`, and `TopologyUAlignDirection` at lines 48, 53, 102, and 111. Moving those small material action types out of `SectorEditorTypes.h` would let this header stop including the umbrella.
- `SectorEditorHelpers.h` uses many small editor enums from split headers and from `SectorEditorTypes.h` at lines 41-122 and 172. It would benefit from a small editor core/tool/material action type header, but it is not a low-risk one-line include removal.

Includes that are necessary in `SectorEditorTypes.h` today:

- `engine/ui/UI.h`: needed by `SectorEditorUiState` at lines 193-254.
- `SectorEditorTopologyRenderCacheTypes.h`: needed by `SectorEditorState::topologyRenderCache` at line 90.
- `SectorEditorSelectionTypes.h`: needed by selection/drag/surface fields at lines 99-132 and 170-171.
- `SectorEditorSurfaceTypes.h`: needed by selected wall/material fields and `TopologyMaterialPayload` at lines 71, 105-106, and 172-173.
- `SectorEditorModalTypes.h`: needed by modal/picker fields at lines 177-189.
- `SectorEditorPreviewTypes.h`: needed by preview/debug/pilot fields at lines 152-153 and 169.
- `SectorCollisionWorld.h`, `SectorFpsController.h`, `SectorFreeflyController.h`, `SectorRuntimeObjects.h`, and topology/map/authoring headers: needed by concrete `SectorEditorState` fields.
- Standard library includes for optional/string/unordered_map/vector/cstdint are all used by remaining fields.

Includes that are compatibility-only or probably redundant:

- `SectorEditorLightmapAsyncTypes.h` is compatibility-only inside `SectorEditorTypes.h` in the current header: `SectorEditorState` no longer has `LightmapBakeAsyncState`. It is still included at line 4 so umbrella consumers can see async bake types.
- `SectorEditorSurfaceTypes.h` is both necessary and re-exported. Because `SectorEditorModalTypes.h` already includes it, one of the two include paths is redundant from a transitive perspective, but direct inclusion in `SectorEditorTypes.h` is still appropriate while `TopologyMaterialPayload` and `SectorEditorState` use those types.
- `raylib.h` is necessary for `Vector2` in `SectorEditorState` and may also be pulled transitively by split headers; the direct include is still justified.

Unexpected heavy dependencies still pulled in:

- Any file including `SectorEditorTypes.h` still pulls in collision world, controller state, runtime object state, topology map/edit/creation, UI state, and all modal/render-cache/selection/preview type headers.
- `SectorEditorModalTypes.h` pulls in FPS controller, lightmap bake settings, full topology map/types, texture types, asset handles, and UI. That is acceptable for modal state, but it means "modal types" is not a tiny header.

## Compatibility Umbrella Assessment

Keeping `SectorEditorTypes.h` as an umbrella is still useful for now. Many public editor headers expose `SectorEditorState&` or `SectorEditorUiState&`, and those aggregates need most split headers as concrete member types. Removing all umbrella behavior immediately would cause broad include churn without much behavior or architecture benefit.

Files relying on umbrella behavior include the 13 direct include users listed above. Some also include split headers directly, but still need the umbrella for `SectorEditorState`, `SectorEditorUiState`, `SectorEditorTool`, material payload/action enums, or backend topology/authoring types.

Low-risk removals should be limited to candidate headers that appear not to use `SectorEditorState`, `SectorEditorUiState`, or small enums still defined in `SectorEditorTypes.h`. `SectorEditorLightmapModal.h` is the clearest candidate. Removing compatibility includes from `SectorEditorTypes.h` itself should wait until direct include migration is tested with a full build.

Safest next step: do a narrow compile-verified Codex task that removes one or two redundant `SectorEditorTypes.h` includes from public headers, adds explicit narrow includes where needed, and stops there.

## Remaining Risk Areas

Medium: `SectorEditorState` remains a broad aggregate.

- Evidence: `SectorEditorState` starts at `sources/sector_editor/SectorEditorTypes.h:76` and mixes document/authoring/cache/tool/selection/runtime/preview/collision/material/modal fields through line 190.
- Why it matters: any header exposing the state inherits the editor, topology, runtime, preview, collision, modal, and UI-adjacent dependency stack.
- Recommended action: defer a true state split unless a feature needs it. A full document-state vs preview-state rewrite maps to the deferred REF-036 level of risk, not a quick header cleanup.

Medium: `SectorEditorUiState` remains a large UI inventory.

- Evidence: `sources/sector_editor/SectorEditorTypes.h:192-261` contains all inspector input fields, scroll states, ID edit buffers, and keyboard capture state.
- Why it matters: inspectors such as `SectorEditorLightInspector.h:36`, `SectorEditorSectorInspector.h:49`, and `SectorEditor.h:430` need the full UI state definition.
- Recommended action: audit first if this starts blocking include cleanup. A split by inspector family may help, but it can easily create churn in `SectorEditor.cpp` and inspector modules.

Low: Material payload/action helper types remain in the umbrella.

- Evidence: `TopologyUvFitMode`, `TopologyUAlignDirection`, and `TopologyMaterialPayload` live at `sources/sector_editor/SectorEditorTypes.h:58-74`; `SectorEditorMaterialActions.h:48-53` and lines 99-112 depend on them.
- Why it matters: material actions cannot include only material/surface headers while these small types stay in the umbrella.
- Recommended action: small Codex task candidate: extract these into a narrow material payload/action types header if include cleanup remains a priority.

Low: `SectorEditorLightmapAsyncTypes.h` is still re-exported by the umbrella.

- Evidence: `SectorEditorTypes.h:4` includes it, but no `LightmapBakeAsyncState` field remains in `SectorEditorState`; `SectorEditor.h:431` owns `LightmapBakeAsyncState` directly.
- Why it matters: umbrella users inherit thread/mutex/atomic/lightmap bake result dependencies even when they only need editor state.
- Recommended action: leave for compatibility until direct include cleanup is attempted; removal from the umbrella should be build-verified.

Low: implementation/public headers still include the umbrella by habit.

- Evidence: direct umbrella include users are listed above; several also include split headers directly, such as `SectorEditorHelpers.h:3-7`, `SectorEditorMaterialActions.h:3-6`, `SectorEditorTextureModals.h:6-8`, and `SectorEditor.h:7-11`.
- Why it matters: direct split includes do not fully reduce blast radius if the umbrella remains beside them.
- Recommended action: perform a small "direct include cleanup" task, not a broad refactor.

Low: circular include risk appears controlled.

- Evidence: the observed split headers include backend or lower editor type headers, while `SectorEditorTypes.h` includes them as an umbrella. No obvious split header includes `SectorEditorTypes.h`.
- Why it matters: the current direction avoids direct header cycles, but adding `SectorEditorTypes.h` to split headers would immediately re-tighten the graph.
- Recommended action: keep split headers independent of `SectorEditorTypes.h`.

## Recommended Follow-Up Items

1. Direct include cleanup for redundant umbrella users.

- Suggested backlog ID: new REF item or fold into a small future editor-header cleanup task; do not expand REF-012.
- Task type: Codex task.
- Risk: Low/Medium.
- Expected benefit: proves the split in real include users and trims accidental umbrella usage.
- Likely files: `SectorEditorLightmapModal.h`, `SectorEditorPreviewSettingsModal.h`, possibly `SectorEditorTopologyRenderCache.h`.
- Tests/verification: `cmake --build cmake-build-debug -j2`, `ctest --test-dir cmake-build-debug --output-on-failure`, `git diff --check`.

2. Extract material payload/action enum types if include cleanup needs it.

- Suggested backlog ID: new REF item.
- Task type: Codex task.
- Risk: Low.
- Expected benefit: lets `SectorEditorMaterialActions.h` include material/surface types without full editor state.
- Likely files: new `SectorEditorMaterialTypes.h` or similar, `SectorEditorTypes.h`, `SectorEditorMaterialActions.h`, `SectorEditorHelpers.h`, `SectorEditor.h`.
- Tests/verification: full build and editor-related tests.

3. Audit `SectorEditorUiState` split only if UI include pressure persists.

- Suggested backlog ID: new REF item, audit first.
- Task type: Audit first.
- Risk: Medium.
- Expected benefit: could reduce inspector/header dependencies, but may create broad UI churn.
- Likely files: `SectorEditorTypes.h`, inspector headers/cpps, `SectorEditor.cpp`.
- Tests/verification: build, tests, UI layout tests if available, no behavior changes.

4. Leave `SectorEditorState` broad for now.

- Suggested backlog ID: REF-036 remains deferred unless revived.
- Task type: Defer.
- Risk: High if attempted casually.
- Expected benefit: high only with a dedicated editor-state plan.
- Likely files: broad editor modules and preview/document/action helpers.
- Tests/verification: would need runner plan or careful multi-phase work plus manual editor smoke.

5. Proceed to REF-013 MeshPreview audit.

- Suggested backlog ID: REF-013.
- Task type: Audit first.
- Risk: Medium.
- Expected benefit: likely higher than another immediate passive type split, because `SectorEditorTypes.h` is now good enough as a temporary umbrella.
- Likely files: `SectorMeshPreview.h/.cpp` and related renderer/runtime modules.
- Tests/verification: audit-only validation first; no behavior testing unless source changes happen later.

## Backlog Update Recommendation

REF-012 should be marked complete after this audit file is written. Completion notes should point to `docs/audit/sector_editor_types_dependency_audit.md` and state that the split helped, with no immediate further `SectorEditorTypes.h` split required beyond optional include cleanup.

## Appendix: Evidence

Commands used:

```sh
rg -n '#include "sector_editor/SectorEditorTypes.h"|#include "sector_editor/SectorEditor.*Types.h"' sources tests
rg -n "SectorEditorTypes.h" sources tests docs
rg -n "SectorEditorModalTypes.h|SectorEditorSelectionTypes.h|SectorEditorPreviewTypes.h|SectorEditorLightmapAsyncTypes.h|SectorEditorSurfaceTypes.h|SectorEditorTopologyRenderCacheTypes.h" sources tests docs
rg -n "struct |enum class |enum " sources/sector_editor/SectorEditorTypes.h sources/sector_editor/SectorEditorTopologyRenderCacheTypes.h sources/sector_editor/SectorEditorModalTypes.h sources/sector_editor/SectorEditorSurfaceTypes.h sources/sector_editor/SectorEditorSelectionTypes.h sources/sector_editor/SectorEditorPreviewTypes.h sources/sector_editor/SectorEditorLightmapAsyncTypes.h
wc -l sources/sector_editor/SectorEditorTypes.h sources/sector_editor/SectorEditorTopologyRenderCacheTypes.h sources/sector_editor/SectorEditorModalTypes.h sources/sector_editor/SectorEditorSurfaceTypes.h sources/sector_editor/SectorEditorSelectionTypes.h sources/sector_editor/SectorEditorPreviewTypes.h sources/sector_editor/SectorEditorLightmapAsyncTypes.h
rg -n "SectorEditorState&|const SectorEditorState&|SectorEditorUiState&|const SectorEditorUiState&" sources/sector_editor/*.h
rg -n "SectorEditorTypes.h" tests sources/sector_editor/*.cpp sources/sector_editor/*.h
```

Line-count summary:

```text
263 sources/sector_editor/SectorEditorTypes.h
129 sources/sector_editor/SectorEditorTopologyRenderCacheTypes.h
186 sources/sector_editor/SectorEditorModalTypes.h
 46 sources/sector_editor/SectorEditorSurfaceTypes.h
143 sources/sector_editor/SectorEditorSelectionTypes.h
 42 sources/sector_editor/SectorEditorPreviewTypes.h
 50 sources/sector_editor/SectorEditorLightmapAsyncTypes.h
859 total
```

Direct include count summary from `rg -l "#include \"sector_editor/$h\"" sources tests | wc -l`:

```text
SectorEditorTypes.h 13
SectorEditorTopologyRenderCacheTypes.h 2
SectorEditorModalTypes.h 6
SectorEditorSurfaceTypes.h 5
SectorEditorSelectionTypes.h 6
SectorEditorPreviewTypes.h 2
SectorEditorLightmapAsyncTypes.h 3
```

Selected include references:

- `SectorEditorTypes.h:3-21`: current broad include list.
- `SectorEditorTypes.h:76-190`: remaining `SectorEditorState`.
- `SectorEditorTypes.h:192-261`: remaining `SectorEditorUiState`.
- `SectorEditorLightmapModal.h:6-7`: split async header plus umbrella include.
- `SectorEditorMaterialActions.h:3-6`: split modal/selection/surface headers plus umbrella include.
- `SectorEditorTopologyRenderCache.h:3-5`: split selection/render-cache headers plus umbrella include.
- `SectorEditorHelpers.h:3-7`: several split headers plus umbrella include.
- `SectorEditor.h:7-11`: split async/selection headers plus umbrella include.

Architecture audit comparison:

- `docs/audit/codebase_architecture_audit.md:20-22` described the old 784-line `SectorEditorTypes.h`.
- `docs/audit/codebase_architecture_audit.md:42-43` recommended splitting render cache, modal, selection/picking, and async lightmap state.
- `docs/audit/codebase_architecture_audit.md:804-816` recommended the exact style of split now completed, with the umbrella kept temporarily if needed.
- Current inspection shows that temporary umbrella is still useful, but the original passive type clusters have moved out.
