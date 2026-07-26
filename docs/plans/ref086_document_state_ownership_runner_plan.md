# REF-086 DocumentState Ownership Runner Plan

## How To Use This Plan

This is a living execution plan.

When an agent is asked to execute this plan, it must:

1. Read this section first.
2. Read the `plan-state-json` block.
3. Identify the selected phase/pass.
4. Execute only that selected phase/pass.
5. Do not skip ahead.
6. Do not execute multiple phases/passes in one run unless the selected item explicitly says it is a combined pass.
7. If the selected item is too broad, update this plan with smaller child passes and stop.
8. If smaller passes are added, do not also implement source changes in the same run unless explicitly instructed.
9. After executing a phase/pass, update this plan with status, date, summary, verification results, behavior notes, remaining debt, and any phase-specific follow-up.
10. Do not claim manual verification unless it was actually performed.
11. Keep this plan self-tracking so future fresh-context runs can resume from it.

```plan-state-json id="ref086-document-state-ownership"
{
  "plan_id": "ref086_document_state_ownership",
  "status_values": [
    "Not Started",
    "Planned",
    "In Progress",
    "Completed",
    "Deferred",
    "Blocked",
    "Partial"
  ],
  "items": [
    {
      "id": "phase_01",
      "title": "DocumentState Inventory And Root/Sub-State Skeleton",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02",
      "title": "Authoring Source State",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02a",
      "title": "Authoring Graph Access Boundary Prep",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02b",
      "title": "Authoring Helpers And Document Actions Retarget",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02c",
      "title": "Central Editor And Tool Authoring Graph Retarget",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02d",
      "title": "Inspector Material Preview And Tests Retarget",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e",
      "title": "AuthoringGraph Ownership Move",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e1",
      "title": "Authoring Document State Storage And Editor Wiring",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e1a",
      "title": "Authoring Document Graph Storage Field",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e1b",
      "title": "Central Authoring Graph Accessor Wiring",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e1c",
      "title": "Already-Retargeted Context Graph Transfer",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e1c1",
      "title": "Authoring Mutation Wrapper Transfer Prep",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e1c2",
      "title": "Material And Inspector Mutation Transfer Prep",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e1c3",
      "title": "Central Accessor Storage Switch",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e2",
      "title": "Authoring State Compatibility Wrapper Retarget",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e3",
      "title": "Material Picker And Document Action Compatibility Retarget",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e4",
      "title": "Authoring Graph Test Fixture Retarget",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02e5",
      "title": "Remove SectorEditorState AuthoringGraph Storage",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "Document Map And Derivation State",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_03a",
      "title": "Derivation Bookkeeping State",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03a1",
      "title": "Derivation Document Storage And Access Boundary Prep",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03a2",
      "title": "Derivation Helper And Context Retarget",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03a3",
      "title": "Central Derivation Storage Switch",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03a4",
      "title": "Derivation Compatibility Cleanup And Tests",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b",
      "title": "Document Map State / topologyMap Ownership",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b1",
      "title": "Document Map Storage And Access Boundary Prep",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b2",
      "title": "Central Topology Map Accessor And Storage Switch",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b2a",
      "title": "Topology Map Install Target Prep",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b2b",
      "title": "Central Map Accessor Wiring",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b2c",
      "title": "Document Map Storage Switch",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b3",
      "title": "Map Mutation Metadata And Preview Integration Retarget",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b4",
      "title": "TopologyMap Compatibility Cleanup And Tests",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03c",
      "title": "Derived Map Consumer Retargeting",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_04",
      "title": "Document Lifecycle Dirty/Path/Status State",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_05",
      "title": "Load/Save/Reset/Import/Migration Retargeting",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_06",
      "title": "Topology Render Cache And Invalidation Ownership",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_07",
      "title": "Document Dependency Cleanup",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_07a",
      "title": "Selection And Tool Context Dependency Narrowing",
      "type": "pass",
      "parent": "phase_07",
      "status": "Completed"
    },
    {
      "id": "phase_07b",
      "title": "Material Editing And Picker Routing Dependency Narrowing",
      "type": "pass",
      "parent": "phase_07",
      "status": "Completed"
    },
    {
      "id": "phase_07c",
      "title": "Inspector Panel And Material Inspector Dependency Narrowing",
      "type": "pass",
      "parent": "phase_07",
      "status": "Completed"
    },
    {
      "id": "phase_07d",
      "title": "Placed Object And Runtime-State Dependency Audit",
      "type": "pass",
      "parent": "phase_07",
      "status": "Completed"
    },
    {
      "id": "phase_08",
      "title": "Final Dependency Audit And Backlog Refresh",
      "type": "phase",
      "status": "Completed"
    }
  ]
}
```

## Current Progress

| Phase | Status | Date | Summary | Verification | Behavior Notes | Remaining Debt |
| --- | --- | --- | --- | --- | --- | --- |
| Phase 1: DocumentState Inventory And Root/Sub-State Skeleton | Completed | 2026-07-07 | Source changed. Added header-only `sources/sector_editor/document/SectorEditorDocumentState.h` with empty responsibility-based skeleton structs for authoring/source, derivation, document map, lifecycle, and the root `SectorEditorDocumentState`; composed an unused `documentState` member in `SectorEditor`. No document fields moved. Render-cache ownership remains deferred for Phase 6 rather than being absorbed into the document root. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 1 grep checks reviewed. | No behavior changes intended. Authoring graph remains the editable source of truth. Existing topology render-cache invalidation behavior, lightmap source-hash behavior, serialization/schema/save/load/import behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Later phases still need to move authoring graph ownership, derivation bookkeeping, document map/topology ownership, lifecycle dirty/path/status state, load/save/reset/import retargeting, and the cache ownership decision. |
| Phase 2: Authoring Source State | Completed | 2026-07-07 | Source changed across child passes. `authoringGraph` ownership moved out of `SectorEditorState` into `SectorEditorAuthoringDocumentState`; central editor wiring, tools, selection/manipulation, inspector, material editing/picker routing, document actions, preview panels, and tests now use document-owned or explicit `SectorAuthoringGraph&` / `const SectorAuthoringGraph&` access. Phase 2E5 removed the final `SectorEditorState::authoringGraph` storage and obsolete broad authoring-state compatibility wrappers. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 2/2E5 grep checks reviewed. `state.authoringGraph` no longer appears in `sources/sector_editor` or `tests`; `SectorAuthoringGraph authoringGraph` remains only in document-owned state and authoring document payloads. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes intended. Authoring graph remains the editable source of truth, now owned by `SectorEditorDocumentState::authoring.authoringGraph`. Dirty/cache invalidation behavior was preserved; authoring mutations still route through existing dirty/stale/invalidation and derivation refresh paths. Serialization/schema/save/load/import behavior and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 3A1 next. Later phases still need derivation bookkeeping ownership, document map/topology ownership, lifecycle dirty/path/status state, load/save/reset/import retargeting, and the cache ownership decision. |
| Phase 2A: Authoring Graph Access Boundary Prep | Completed | 2026-07-07 | Source changed. Added narrow `SectorEditorAuthoringDocumentAccess` / `SectorEditorConstAuthoringDocumentAccess` reference views and `MakeSectorEditorAuthoringDocumentAccess()` helpers in `sources/sector_editor/document/SectorEditorDocumentState.h`. Routed the central selection-service composition point through the authoring access view while keeping `SectorEditorState::authoringGraph` as the backing storage. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 2 grep checks reviewed. `state.authoringGraph` matches remain expected because this pass does not move storage or broadly retarget consumers. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes intended. `authoringGraph` storage did not move in Phase 2A. Authoring graph remains the editable source of truth. Dirty/cache invalidation behavior, serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2B next. Later passes still need authoring helper/document-action retargeting, central/tool retargeting, inspector/material/preview/test retargeting, and the final authoring graph ownership move. |
| Phase 2B: Authoring Helpers And Document Actions Retarget | Completed | 2026-07-07 | Source changed. Added explicit authoring graph overloads for focused authoring helpers: authoring selection pruning, map-point face/selection resolution, topology-to-authoring ID mapping, and inspector target resolution. Retargeted the document save construction path so `SaveSectorEditorAuthoringDocument()` can receive explicit `SectorAuthoringGraph`, `SectorTopologyMap`, and `SectorAuthoringDerivationResult` references; the existing `SectorEditorState&` wrappers now delegate. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 2 grep checks reviewed. `state.authoringGraph` matches remain expected in central editor, tools, inspector/material routing, and tests for later Phase 2C/2D/2E passes. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes intended. `authoringGraph` storage did not move in Phase 2B. Authoring graph remains the editable source of truth. Dirty/cache invalidation behavior and authoring derivation stale/current/last-valid behavior were preserved; no topology mutation or cache invalidation path was changed. Serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2C next. Later passes still need central editor/tool retargeting, inspector/material/preview/test retargeting, and the final authoring graph ownership move. |
| Phase 2C: Central Editor And Tool Authoring Graph Retarget | Completed | 2026-07-07 | Source changed. Added explicit `SectorAuthoringGraph&` references to `SectorEditorToolContext` and `SectorEditorManipulationServiceContext`, wired them from `SectorEditor::BuildToolContext()` / `BuildManipulationServiceContext()`, and retargeted select, insert-vertex, and manipulation tool consumers away from `context.state.authoringGraph`. Central `SectorEditor.cpp` authoring hover, snap, drag, insert-vertex, authoring-line/vertex picking, and authoring-data checks now use local explicit graph references where practical while keeping storage in `SectorEditorState`. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 2 grep checks reviewed. `context.state.authoringGraph` no longer appears in `sources/sector_editor/tools` or `sources/sector_editor/selection`. Remaining `state.authoringGraph` matches are expected in central composition/storage wiring, preview/inspector/material/test consumers for Phase 2D/2E, and pre-existing authoring helper wrappers. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes intended. `authoringGraph` storage did not move in Phase 2C. Authoring graph remains the editable source of truth. Dirty/cache invalidation behavior was preserved; no topology mutation or cache invalidation path was changed. Serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2D next. Later passes still need inspector/material/preview/test retargeting and the final authoring graph ownership move. |
| Phase 2D: Inspector Material Preview And Tests Retarget | Completed | 2026-07-07 | Source changed. Added explicit authoring graph references to `SectorEditorInspectorPanelContext`, `SectorEditorMaterialEditingServiceContext`, and `SectorEditorMaterialPickerRoutingContext`; inspector authoring target display and material picker routing now use those graph references for authoring lookups while keeping `SectorEditorState::authoringGraph` as backing storage. Preview overlay/UV panel consumers were reviewed and already use explicit `SectorAuthoringGraph&` contexts. Retargeted the material service test helper and one selection/inspector target test that only needed a graph. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); Phase 2 grep checks reviewed. `state.authoringGraph` matches remain expected in central storage/wiring, compatibility wrappers, authoring helper wrappers/mutations, document load/save wrappers, and stateful tests until Phase 2E. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes intended. `authoringGraph` storage did not move in Phase 2D. Authoring graph remains the editable source of truth. Material and sector property edits still route through existing authoring mutation helpers; dirty/cache invalidation behavior was preserved and no topology mutation or cache invalidation path was changed. Serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. Preview ownership was not moved. No manual GUI verification was performed. | Execute Phase 2E next. The final ownership move still needs to move `SectorEditorState::authoringGraph` into `SectorEditorAuthoringDocumentState` and remove the compatibility storage dependency. |
| Phase 2E: AuthoringGraph Ownership Move | Completed | 2026-07-07 | Parent/meta pass plus child passes completed. Phase 2E was split into Phase 2E1 through Phase 2E5; those child passes introduced document-owned authoring graph storage, switched central editor access to it, retargeted compatibility wrappers/tests, and removed final `SectorEditorState::authoringGraph` storage. | Passed across child passes: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 2/2E grep checks reviewed. `state.authoringGraph` no longer appears in `sources/sector_editor` or `tests`. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes intended. Authoring graph source-of-truth behavior, dirty/cache invalidation behavior, serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | All Phase 2E child passes are complete. Continue with Phase 3A1. |
| Phase 2E1: Authoring Document State Storage And Editor Wiring | Completed | 2026-07-07 | Documentation-only split/meta pass. Phase 2E1 proved too broad as one implementation pass because moving/wiring `documentState.authoring.authoringGraph` before the remaining broad compatibility wrappers are retargeted would create two live `SectorAuthoringGraph` storages and risk split source-of-truth behavior. Added Phase 2E1A through Phase 2E1C as direct Phase 2 child passes and stopped without source implementation. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 2 grep checks reviewed. | No behavior changes. `authoringGraph` storage did not move in this run. Authoring graph source-of-truth behavior, dirty/cache invalidation behavior, serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2E1A next. Phase 2 remains incomplete until Phase 2E1A through Phase 2E1C and the remaining Phase 2E passes complete or are explicitly deferred. |
| Phase 2E1A: Authoring Document Graph Storage Field | Completed | 2026-07-07 | Source changed. Added `SectorAuthoringGraph authoringGraph` to `SectorEditorAuthoringDocumentState` in `sources/sector_editor/document/SectorEditorDocumentState.h` and included the authoring graph definition there because the document state now owns a concrete graph field. No mutable editor contexts, accessors, load/save/import/reset paths, tests, or compatibility wrappers were retargeted in this pass. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 2E1A grep reviewed. `SectorEditorAuthoringDocumentState::authoringGraph` now exists, `SectorEditorState::authoringGraph` remains present, and no `documentState.authoring.authoringGraph` routing was introduced. | No behavior changes intended. `SectorEditorState::authoringGraph` remains the live compatibility storage after Phase 2E1A; the new document-owned field is future storage and is not wired into normal editor mutation paths yet. Authoring graph remains the editable source of truth. Dirty/cache invalidation behavior, serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2E1B next. Temporary duplicate storage exists only as an unwired future field until the central accessor/transfer passes. |
| Phase 2E1B: Central Authoring Graph Accessor Wiring | Completed | 2026-07-07 | Source changed. Added private mutable/const `SectorEditor::AuthoringGraph()` accessors backed by the current live `SectorEditorState::authoringGraph`. Retargeted central `SectorEditor` graph composition and already-retargeted context builders through that named path, including tool, manipulation, preview overlay/UV, topology render cache, inspector panel, material editing service, picker texture query context, authoring hover/drag/insert-vertex helpers, authoring picking, and graph-data checks. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 2 grep checks reviewed. Remaining `state.authoringGraph` matches are expected in the central accessor/free selection helper/load assignment, Phase 2E2 authoring-state compatibility wrappers, Phase 2E3 material-picker/document-action compatibility wrappers, and Phase 2E4 stateful tests. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes intended. The live backing storage did not transfer in Phase 2E1B: `SectorEditorState::authoringGraph` remains the active mutable graph and `documentState.authoring.authoringGraph` remains future storage. Authoring graph remains the editable source of truth. Dirty/cache invalidation behavior was preserved; no topology mutation or cache invalidation path was changed. Serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2E1C next. Temporary duplicate storage remains until the transfer pass; compatibility wrappers and stateful tests still need later Phase 2E2 through Phase 2E5 retargeting. |
| Phase 2E1C: Already-Retargeted Context Graph Transfer | Completed | 2026-07-07 | Documentation-only split/meta pass. Phase 2E1C was too broad as one implementation pass because switching the central `SectorEditor::AuthoringGraph()` accessor directly to `documentState.authoring.authoringGraph` would leave normal editor paths reading the document graph while remaining compatibility helpers still mutate and derive from `SectorEditorState::authoringGraph`. Added Phase 2E1C1 through Phase 2E1C3 as direct Phase 2 child passes and stopped without source implementation. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 2 grep checks reviewed. Remaining `state.authoringGraph` matches are expected because no source transfer happened in this split pass; they define the Phase 2E1C1 through Phase 2E1C3 work and later Phase 2E2 through Phase 2E5 work. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes. The live backing storage did not transfer in this run: `SectorEditorState::authoringGraph` remains active and `documentState.authoring.authoringGraph` remains future storage. Dirty/cache invalidation behavior, serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2E1C1 next. Temporary duplicate storage remains until the prep passes make the central accessor switch single-source-of-truth safe. |
| Phase 2E1C1: Authoring Mutation Wrapper Transfer Prep | Completed | 2026-07-07 | Source changed. Added explicit `SectorAuthoringGraph&` overloads for central/tool authoring mutation helpers: line segment insertion, line-tool click commit, rectangle commit, insert-vertex commit, vertex move, selected line/vertex deletion, and authoring derivation refresh. Threaded the explicit graph through derived face-anchor reconciliation so derivation refresh derives and reconciles against the same graph reference. Retargeted the already-accessor-backed central commit/delete/drag/load-derive call sites to pass `AuthoringGraph()` explicitly. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); Phase 2 grep checks reviewed. Remaining `state.authoringGraph` matches are expected in the central accessor/load assignment, broad compatibility wrappers, material-picker/document-action paths for Phase 2E1C2/2E3, and stateful tests for Phase 2E4. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes intended. The central accessor storage did not switch in Phase 2E1C1: `SectorEditorState::authoringGraph` remains the live backing storage and `documentState.authoring.authoringGraph` remains future storage. Authoring graph remains the editable source of truth. Dirty/cache invalidation behavior was preserved; authoring mutations still call existing dirty/stale/invalidation paths and no topology mutation or cache invalidation path was changed. Serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2E1C2 next. Temporary compatibility storage and wrappers remain for material/inspector mutation prep, document-action/material-picker compatibility retargeting, stateful test fixture retargeting, and final `SectorEditorState::authoringGraph` removal. |
| Phase 2E1C2: Material And Inspector Mutation Transfer Prep | Completed | 2026-07-07 | Source changed. Added focused explicit-graph overloads for authoring mutation helpers used by material/inspector paths: face-anchor, side, line, topology-sector/topology-sidedef/topology-linedef mutation wrappers, and Blocks Player toggling. Retargeted material editing service authoring material/UV/decal paths, material picker routing apply paths, and inspector sector rename/face-anchor/line/side mutation paths to pass their existing `SectorAuthoringGraph&` context through mutations. Post-review repair retargeted explicit-graph availability and topology-to-authoring mapping prechecks to use the same explicit graph plus `state.authoringDerivation` instead of reading `SectorEditorState::authoringGraph`. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; Phase 2 grep checks reviewed. Remaining `state.authoringGraph` matches are expected in the central accessor/load assignment, broad compatibility wrappers, material-picker compatibility overloads for Phase 2E3, document-action compatibility wrappers, and stateful tests for Phase 2E4. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes intended. The central accessor storage did not switch in Phase 2E1C2: `SectorEditorState::authoringGraph` remains the live backing storage and `documentState.authoring.authoringGraph` remains future storage. Authoring graph remains the editable source of truth. Dirty/cache invalidation behavior was preserved; explicit-graph mutation overloads still call existing dirty/stale/invalidation and derivation refresh paths. Material picker semantics, preview rebuild behavior, serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2E1C3 next. Temporary compatibility storage and wrappers remain for central accessor storage switch, authoring-state compatibility retargeting, material-picker/document-action compatibility retargeting, stateful test fixture retargeting, and final `SectorEditorState::authoringGraph` removal. |
| Phase 2E1C3: Central Accessor Storage Switch | Completed | 2026-07-07 | Source changed. Switched `SectorEditor::AuthoringGraph()` to `documentState.authoring.authoringGraph`. Retargeted the central selection-service context builder to receive the accessor-backed graph explicitly, and updated central blank reset, authoring-document load, legacy-topology import initialization, and save construction to populate/read the same document-owned graph. Added narrow explicit-graph overloads for reset/import initialization while preserving the broad compatibility wrappers for later passes. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 2 grep checks reviewed. Remaining `state.authoringGraph` matches are expected in Phase 2E2 authoring-state compatibility wrappers, Phase 2E3 material-picker/document-action compatibility overloads, and Phase 2E4 stateful tests. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes intended. The live `SectorEditor` authoring graph backing storage after Phase 2E1C3 is `SectorEditorDocumentState::authoring.authoringGraph`; `SectorEditorState::authoringGraph` remains temporary compatibility storage for later Phase 2E2 through Phase 2E5 work. Authoring graph remains the editable source of truth. Dirty/cache invalidation behavior was preserved; central authoring mutations still use existing dirty/stale/invalidation paths and no topology mutation or cache invalidation path was changed. Serialization/schema/save/load/import behavior and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2E2 next. Temporary compatibility storage and wrappers remain for authoring-state compatibility retargeting, material-picker/document-action compatibility retargeting, stateful test fixture retargeting, and final `SectorEditorState::authoringGraph` removal. |
| Phase 2E2: Authoring State Compatibility Wrapper Retarget | Completed | 2026-07-07 | Source changed. Retargeted production authoring-state compatibility call sites that still reached broad state for authoring graph access: central Blocks Player editing, central derived-sector authoring rename/mapping, inspector target resolution, and sector-inspector material availability status now use explicit `SectorAuthoringGraph&` / `const SectorAuthoringGraph&` paths. Added an explicit-graph overload for `ClearSelectedSectorEditorSurface3DIfAuthoringMappingUnavailable()` while preserving the broad wrapper for compatibility. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 2E2 grep checks reviewed. Remaining `state.authoringGraph` matches are expected in `SectorEditorAuthoringState.*` compatibility wrappers kept for Phase 2E4 test fixtures, Phase 2E3 material-picker/document-action compatibility wrappers, and tests. | No behavior changes intended. Authoring graph remains the editable source of truth and live `SectorEditor` storage remains `SectorEditorDocumentState::authoring.authoringGraph`; temporary `SectorEditorState::authoringGraph` compatibility storage remains for later passes. Dirty/cache invalidation behavior was preserved; authoring mutations still route through existing dirty/stale/invalidation and derivation refresh paths. Serialization/schema/save/load/import behavior and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2E3 next. Remaining broad-state wrappers are retained where they still provide compatibility for material-picker/document-action wrappers and stateful tests until Phase 2E3 and Phase 2E4; final storage removal remains Phase 2E5. |
| Phase 2E3: Material Picker And Document Action Compatibility Retarget | Completed | 2026-07-07 | Source changed. Retargeted document-action compatibility save/reset overloads to receive `SectorEditorAuthoringDocumentState&` / `const SectorEditorAuthoringDocumentState&` instead of reading `SectorEditorState::authoringGraph`. Retargeted material-picker state-only convenience overloads to receive `SectorEditorAuthoringDocumentAccess` / `SectorEditorConstAuthoringDocumentAccess`, with explicit `SectorAuthoringGraph&` overloads still the primary path. Updated generic texture-modal helpers, the inspector door-texture picker helper, central `SectorEditor` texture wrapper calls, and focused material picker tests to pass explicit graph storage. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); Phase 2 grep checks reviewed. `state.authoringGraph` no longer appears in `sources/sector_editor/document`, `sources/sector_editor/services/material_edit`, `SectorEditorTextureActions.cpp`, `SectorEditorTextureModals.h`, or the touched inspector picker helper. Remaining `state.authoringGraph` matches are expected in `SectorEditorAuthoringState.*` compatibility wrappers and stateful tests for Phase 2E4/2E5. The existing `LegacyTopology` inspector-target grep match remains pre-existing and was not expanded. | No behavior changes intended. Authoring graph remains the editable source of truth and live `SectorEditor` storage remains `SectorEditorDocumentState::authoring.authoringGraph`; temporary `SectorEditorState::authoringGraph` compatibility storage remains for authoring-state wrappers and tests. Dirty/cache invalidation behavior was preserved; material picker mutations still route through existing explicit-graph mutation helpers. Save/load/import JSON behavior and serialization/schema were preserved; the save helper still writes graph-native authoring documents from the supplied graph/topology/derivation. Lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2E4 next. Remaining broad-state authoring graph dependencies are retained for stateful test fixtures and final storage removal. |
| Phase 2E4: Authoring Graph Test Fixture Retarget | Completed | 2026-07-07 | Source changed. Retargeted `tests/SectorAuthoringGraphTests.cpp` away from `SectorEditorState::authoringGraph` by adding `SectorEditorDocumentState`-backed authoring graph fixtures, explicit state/graph save/load helpers, and test-only adapters for selection, inspector-target, 3D surface, and label resolution. Updated authoring mutation, material picker, line-tool, save/load round-trip, and generated test-data paths to pass explicit graph storage or document-backed graph references. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; targeted grep confirmed no `state.authoringGraph`, `loadedState.authoringGraph`, or `MakeEditorStateWithAuthoringGraph` remains in `tests/SectorAuthoringGraphTests.cpp`. | No behavior changes intended. Production topology mutation code was not changed in this pass; tests now exercise explicit graph/document-backed paths instead of compatibility storage. Dirty/cache invalidation behavior remains production-owned and unchanged, while the retargeted tests still verify existing invalidation expectations. Serialization/schema/save/load/import behavior and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 2E5 next. Temporary `SectorEditorState::authoringGraph` compatibility storage remains in production wrappers until final removal. |
| Phase 2E5: Remove SectorEditorState AuthoringGraph Storage | Completed | 2026-07-07 | Source changed. Removed `SectorAuthoringGraph authoringGraph` from `SectorEditorState` in `sources/sector_editor/SectorEditorTypes.h`; `SectorEditorAuthoringDocumentState::authoringGraph` in `sources/sector_editor/document/SectorEditorDocumentState.h` is the document-owned storage. Removed obsolete `SectorEditorAuthoringState.*` broad compatibility overloads that only existed to read `SectorEditorState::authoringGraph`, and retargeted final stale central/material-picker calls to explicit graph references. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; grep checks: `rg -n "authoringGraph" sources/sector_editor tests`, `rg -n "state\\.authoringGraph" sources/sector_editor tests`, `rg -n "no-authoring|NoAuthoring|TopologyOnly|Legacy|Scratch|writeback" sources/sector_editor/services/material_edit sources/sector_editor/tools sources/sector_editor/SectorEditorAuthoringState.*`, and storage confirmation grep. `state.authoringGraph` produced no matches; `SectorEditorState` no longer contains an `authoringGraph` member. | No behavior changes intended. Authoring graph remains the editable source of truth and is now document-owned. Dirty/cache invalidation behavior was preserved; authoring mutations still call existing dirty/stale/invalidation and derivation refresh paths, and no topology mutation or cache invalidation path was changed. Serialization/schema/save/load/import behavior and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Phase 2 and Phase 2E are complete. Execute Phase 3A1 next. |
| Phase 3: Document Map And Derivation State | Completed | 2026-07-07 | Source changed across child passes. Phase 3A moved derivation bookkeeping into `SectorEditorDocumentState::derivation`; Phase 3B moved `topologyMap` into `SectorEditorDocumentState::map.topologyMap`; Phase 3C narrowed remaining derived-map consumer dependencies where practical by removing broad `SectorEditorState&` access from the topology vertex inspector and from the material picker current-texture helper path. | Passed across child passes, including Phase 3C: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 3 grep checks reviewed. Exact grep for obsolete `state.topologyMap` and obsolete `state.authoringDerivation*` / `state.lastValidAuthoringDerivedTopology` member access returned no matches after Phase 3C. Source-hash/global metadata grep was reviewed. | No behavior changes intended. Authoring graph remains the editable source of truth. `SectorTopologyMap` remains document-owned live storage and remains derived output plus documented topology-owned global metadata/runtime definitions. Texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data. Topology render-cache invalidation behavior, serialization/schema/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Phase 3 is complete. Continue with Phase 4 lifecycle dirty/path/status state next. Remaining broad dependencies caused by preview state, modal UI state, inspector input buffers, tool transaction state, runtime object editing state, render-cache state, and renderer ownership are intentionally deferred. |
| Phase 3A: Derivation Bookkeeping State | Completed | 2026-07-07 | Documentation-only split/meta pass plus child passes now completed. Phase 3A proved too broad as one implementation pass and was split into Phase 3A1 through Phase 3A4; those child passes added document derivation storage, retargeted helper/context boundaries, switched central storage, and removed obsolete `SectorEditorState` derivation bookkeeping storage. | Passed across child passes: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 3 grep checks reviewed. The five derivation bookkeeping fields no longer appear as members of `SectorEditorState`; exact `state.authoringDerivation*` / `state.lastValidAuthoringDerivedTopology` grep returned no matches after Phase 3A4. Expected `state.topologyMap` matches remain for Phase 3B. Source-hash/global metadata grep was reviewed. | No behavior changes intended. The five derivation bookkeeping fields now live in `SectorEditorDocumentState::derivation`; `topologyMap` did not move in Phase 3A. Authoring graph source-of-truth behavior, derivation timing/stale/current/invalid status semantics, last-valid topology behavior, topology ID mapping, authoring target mapping, topology render-cache invalidation behavior, serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Phase 3A implementation is complete. Execute Phase 3B1 next; Phase 3B topology map ownership passes and Phase 3C remain for later. |
| Phase 3A1: Derivation Document Storage And Access Boundary Prep | Completed | 2026-07-07 | Source changed. Added the five derivation bookkeeping fields to `SectorEditorDerivationState` in `sources/sector_editor/document/SectorEditorDocumentState.h`: `authoringDerivation`, `lastValidAuthoringDerivedTopology`, `authoringDerivationState`, `authoringDerivedTopologyStale`, and `authoringDerivationStatus`. Moved the small `SectorEditorAuthoringDerivationState` enum definition into the document-state header so the document-owned derivation state can own a concrete status field, and included that header from `SectorEditorTypes.h`. No broad consumers were retargeted. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); Phase 3 grep checks reviewed. Expected `state.authoringDerivation*`, `state.lastValidAuthoringDerivedTopology`, and `state.topologyMap` matches remain for Phase 3A2 through Phase 3B4. Source-hash/global metadata grep was reviewed. | No behavior changes intended. Live derivation bookkeeping storage did not switch in Phase 3A1; `SectorEditorState` remains the active backing storage for the five derivation fields. `topologyMap` did not move. Derivation timing, stale/current/invalid status semantics, last-valid topology behavior, topology ID mapping, authoring target mapping, topology render-cache invalidation behavior, serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 3A2 next. Temporary duplicate document-owned derivation storage exists only as unwired future storage until Phase 3A3/3A4. |
| Phase 3A2: Derivation Helper And Context Retarget | Completed | 2026-07-07 | Source changed. Added mutable/const `SectorEditorDerivationDocumentAccess` views plus `MakeSectorEditorDerivationDocumentAccess()` and `IsSectorEditorAuthoringDerivationCurrent()` helpers. Retargeted focused live-storage-backed derivation boundaries through explicit derivation references: central selection-service composition, preview overlay/UV/render-cache contexts, selected 3D surface mapping checks, material editing service context, material picker routing helpers/context, inspector panel context and mapping helpers, derivation refresh helper overloads, preview/lightmap currentness helpers, save document boundary, and the affected material-service test fixture. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 3 grep checks reviewed. Expected `state.authoringDerivation*`, `state.lastValidAuthoringDerivedTopology`, and `state.topologyMap` matches remain for Phase 3A3/3A4 and Phase 3B passes because live storage did not switch and `topologyMap` did not move. Source-hash/global metadata grep was reviewed. | No behavior changes intended. Live derivation bookkeeping storage did not switch in Phase 3A2; `SectorEditorState` remains the active backing storage for the five derivation fields, exposed to focused consumers through access views. `topologyMap` did not move and remains derived output plus documented topology-owned global metadata/runtime definitions. Derivation timing, stale/current/invalid status semantics, last-valid topology behavior, topology ID mapping, authoring target mapping, topology render-cache invalidation behavior, serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 3A3 next. Later passes still need the central derivation storage switch, removal of obsolete `SectorEditorState` derivation bookkeeping storage, and the Phase 3B topology map ownership passes. |
| Phase 3A3: Central Derivation Storage Switch | Completed | 2026-07-07 | Source changed. Switched central `SectorEditor` derivation access helpers/composition paths to `SectorEditorDocumentState::derivation`, added document-derivation access overloads for reset, authoring mutation, material editing, picker routing, and refresh integration, and updated load/reset to use document-owned derivation bookkeeping. Baked lightmap restoration for loaded authoring documents now restores into `state.topologyMap`, current document derivation topology, and document last-valid topology. Compatibility wrappers remain for Phase 3A4 and tests; `topologyMap` was not moved. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 3 grep checks reviewed. Expected `state.authoringDerivation*` and `state.lastValidAuthoringDerivedTopology` matches remain in compatibility wrappers and tests for Phase 3A4; expected `state.topologyMap` matches remain for Phase 3B. Source-hash/global metadata grep was reviewed. | No behavior changes intended. Live central `SectorEditor` derivation bookkeeping now uses `documentState.derivation`. `topologyMap` remains in `SectorEditorState` and remains derived output plus documented topology-owned global metadata/runtime definitions. Derivation timing, stale/current/invalid status semantics, last-valid topology behavior, topology ID mapping, authoring target mapping, topology render-cache invalidation behavior, serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 3A4 next. Remaining debt: remove obsolete `SectorEditorState` derivation bookkeeping storage and retarget remaining compatibility/test paths; then continue with Phase 3B topology map ownership passes. |
| Phase 3A4: Derivation Compatibility Cleanup And Tests | Completed | 2026-07-07 | Source changed. Removed the five obsolete derivation bookkeeping fields from `SectorEditorState`: `authoringDerivation`, `lastValidAuthoringDerivedTopology`, `authoringDerivationState`, `authoringDerivedTopologyStale`, and `authoringDerivationStatus`. Retargeted remaining compatibility and test paths to `SectorEditorDocumentState::derivation`, explicit `SectorEditorDerivationDocumentAccess`, or explicit `SectorEditorConstDerivationDocumentAccess`. Added a narrow mutable-to-const derivation access conversion helper for read-only routing. `topologyMap` was not moved. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 3 grep checks reviewed. Exact grep for `state.authoringDerivation`, `state.lastValidAuthoringDerivedTopology`, `state.authoringDerivationState`, `state.authoringDerivedTopologyStale`, and `state.authoringDerivationStatus` in `sources/sector_editor tests` returned no matches. Expected `state.topologyMap` matches remain for Phase 3B. Source-hash/global metadata grep was reviewed. | No behavior changes intended. Live derivation bookkeeping remains document-owned in `SectorEditorDocumentState::derivation`; `topologyMap` remains in `SectorEditorState` and remains derived output plus documented topology-owned global metadata/runtime definitions. Derivation timing, stale/current/invalid status semantics, last-valid topology behavior, topology ID mapping, authoring target mapping, topology render-cache invalidation behavior, serialization/schema/save/load/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Phase 3A implementation is complete. Continue with Phase 3B1 next. Later work still needs topology map ownership passes Phase 3B1 through Phase 3B4 and Phase 3C consumer retargeting. |
| Phase 3B: Document Map State / topologyMap Ownership | Completed | 2026-07-07 | Documentation-only split/meta pass. Phase 3B proved too broad as one implementation pass because moving `topologyMap` touches central editor draw/update paths, load/reset/import integration, derivation output installation, metadata copying, preview/render-cache/lightmap/collision inputs, runtime object/light editing, texture registry access, material services, selection/tool contexts, and tests. Added Phase 3B1 through Phase 3B4 as child passes and stopped without source implementation. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 3 grep checks reviewed. `topologyMap` remains in `SectorEditorState`; derivation bookkeeping matches remain expected until Phase 3A1 through Phase 3A4 are implemented. Source-hash/global metadata grep was reviewed. | No behavior changes. `topologyMap` did not move in this run. `SectorTopologyMap` remains derived output plus documented topology-owned global metadata/runtime definitions. Texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data. Topology render-cache invalidation behavior, serialization/schema/import behavior, metadata copying, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 3B1 after the required earlier Phase 3A child passes are complete. Phase 3B remains implementation-incomplete until Phase 3B1 through Phase 3B4 complete or are explicitly deferred. Phase 3C remains for later. |
| Phase 3B1: Document Map Storage And Access Boundary Prep | Completed | 2026-07-07 | Source changed. Added future document-owned `SectorTopologyMap topologyMap` storage to `SectorEditorDocumentMapState` in `sources/sector_editor/document/SectorEditorDocumentState.h`, plus narrow mutable/const `SectorEditorDocumentMapAccess` views and `MakeSectorEditorDocumentMapAccess()` helpers for future incremental retargeting. No broad consumers were retargeted. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 3 grep checks reviewed. Expected `state.topologyMap` matches remain because live map storage did not switch in Phase 3B1; derivation bookkeeping remains document-owned from Phase 3A. Source-hash/global metadata grep was reviewed. | No behavior changes intended. Live topology map storage did not switch; `SectorEditorState::topologyMap` remains the active backing storage for editor behavior. `SectorTopologyMap` remains derived output plus documented topology-owned global metadata/runtime definitions. Texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data. Metadata copying, topology render-cache invalidation behavior, serialization/schema/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 3B2A next. Phase 3B topology map ownership remains incomplete until Phase 3B2A through Phase 3B2C, Phase 3B3, and Phase 3B4 complete or are explicitly deferred; Phase 3C remains for later. |
| Phase 3B2: Central Topology Map Accessor And Storage Switch | Completed | 2026-07-07 | Documentation-only split/meta pass. Phase 3B2 proved too broad as one implementation pass because switching central `SectorEditor` access to `documentState.map.topologyMap` before retargeting reset/load/import, derivation install, and central authoring/material mutation write paths would create split live topology map storage between document reads and remaining `SectorEditorState::topologyMap` writes. Added Phase 3B2A through Phase 3B2C as direct Phase 3 child passes and stopped without source implementation. | Passed: `git diff --check`; `git diff --stat`; `git status --short`. Build and ctest were skipped because this run changed documentation only. | No behavior changes. Live topology map storage did not switch in this run; `SectorEditorState::topologyMap` remains the active backing storage and `SectorEditorDocumentMapState::topologyMap` remains future storage. `SectorTopologyMap` remains derived output plus documented topology-owned global metadata/runtime definitions. Texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data. Metadata copying, topology render-cache invalidation behavior, serialization/schema/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 3B2A next. Phase 3B topology map ownership remains incomplete until Phase 3B2A through Phase 3B2C, Phase 3B3, and Phase 3B4 complete or are explicitly deferred. |
| Phase 3B2A: Topology Map Install Target Prep | Completed | 2026-07-07 | Source changed. Added explicit `SectorTopologyMap&` install-target overloads for `ResetEditorTopologyDocumentState()` and `RefreshSectorEditorAuthoringDerivation()`, with existing compatibility wrappers still delegating to `SectorEditorState::topologyMap`. Retargeted central reset/new-document and authoring-document load derivation install paths to pass the current live `state.topologyMap` target explicitly. Legacy topology import load now uses a local explicit live map reference for install/initialization. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; focused grep/review of reset, load/import, and derivation-install call sites. | No behavior changes intended. Live topology map storage did not switch in Phase 3B2A; `SectorEditorState::topologyMap` remains the active backing storage and `SectorEditorDocumentMapState::topologyMap` remains future storage. `SectorTopologyMap` remains derived output plus documented topology-owned global metadata/runtime definitions. Texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data. Metadata copying via `CopyEditorMapLevelFields()`, baked metadata restoration after authoring-document load, topology render-cache invalidation behavior, serialization/schema/import behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 3B2B next. Compatibility wrappers and non-central authoring/material mutation helpers may still call the old `RefreshSectorEditorAuthoringDerivation()` signature until later Phase 3B passes retarget or remove them. |
| Phase 3B2B: Central Map Accessor Wiring | Completed | 2026-07-07 | Source changed. Added private mutable/const `SectorEditor::TopologyMap()` accessors backed by current live `SectorEditorState::topologyMap`; routed central `SectorEditor.cpp` topology map composition points through the named accessor, including update/runtime object refresh, snap/pick/overlay paths, render-cache/preview contexts, lightmap bake/install/status paths, load/reset/install targets, preview rebuild/spawn, preview settings, selection lookup, collision/renderer rebuild inputs, and central texture/status paths. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 3 grep checks reviewed. Focused `state.topologyMap` grep still reports the central accessor backing and a free compatibility helper in `SectorEditor.cpp`, plus external service/test compatibility paths intentionally left for Phase 3B3/3B4. | No behavior changes intended. Live topology map storage did not switch in Phase 3B2B: `SectorEditorState::topologyMap` remains the active backing storage and `SectorEditorDocumentMapState::topologyMap` remains future storage. `SectorTopologyMap` remains derived output plus documented topology-owned global metadata/runtime definitions. Texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data. Metadata copying, baked metadata restoration, topology render-cache invalidation behavior, serialization/schema/import behavior, and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 3B2C next. Remaining `state.topologyMap` compatibility paths outside central `SectorEditor` composition are deferred to Phase 3B3/3B4; storage switch remains deferred to Phase 3B2C. |
| Phase 3B2C: Document Map Storage Switch | Completed | 2026-07-07 | Source changed. Switched private mutable/const `SectorEditor::TopologyMap()` accessors from obsolete `SectorEditorState::topologyMap` storage to document-owned `SectorEditorDocumentState::map.topologyMap`. Updated the central selection-service context helper to receive the live topology map explicitly through `TopologyMap()` so central selection lookups do not read stale compatibility storage after the switch. Retargeted the `ResetEditorTopologyDocumentState(SectorEditorDocumentState&)` wrapper to install into `documentState.map.topologyMap`; prepared central reset/load/import/derivation-install paths already pass `TopologyMap()` explicitly and now install into document-owned map state. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; focused grep/review of `state.topologyMap`, `documentState.map.topologyMap`, `SectorEditor::TopologyMap()`, reset wrappers, and derivation-install call sites. | No behavior changes intended. Live central `SectorEditor` topology map storage switched to `SectorEditorDocumentState::map.topologyMap`; obsolete `SectorEditorState::topologyMap` compatibility storage remains for Phase 3B3/3B4 paths. `SectorTopologyMap` remains derived output plus documented topology-owned global metadata/runtime definitions. Texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data. Metadata copying via `CopyEditorMapLevelFields()`, baked metadata restoration after authoring-document load, topology render-cache invalidation behavior, serialization/schema/import behavior, and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 3B3 next. Remaining `state.topologyMap` matches are compatibility wrappers/external helper paths deferred to Phase 3B3/3B4, including non-central authoring/material mutation helpers and obsolete state-backed document action wrappers. |
| Phase 3B3: Map Mutation Metadata And Preview Integration Retarget | Completed | 2026-07-07 | Source changed. Retargeted focused live map consumers that still reached obsolete `SectorEditorState::topologyMap`: authoring side/material mutation helpers and Blocks Player editing now have explicit topology-map overloads used by central editor, inspector, material editing service, and material picker routing; material and vertex inspector paths now receive/use explicit topology-map references; the authoring-document save compatibility helper now takes document map state instead of broad state for map access. Compatibility wrappers that still delegate through `SectorEditorState::topologyMap` remain for Phase 3B4 test/cleanup scope. No new fields moved. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 3 grep checks reviewed. Remaining `state.topologyMap` matches are compatibility delegates/wrappers in `SectorEditorAuthoringState.cpp` / `SectorEditorDocumentActions.cpp` and stateful tests, deferred to Phase 3B4. Source-hash/global metadata grep was reviewed. | No behavior changes intended. `SectorTopologyMap` remains document-owned live central storage in `SectorEditorDocumentState::map.topologyMap` and remains derived output plus documented topology-owned global metadata/runtime definitions. Texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data. Metadata copying, derivation output installation, last-valid topology behavior, preview rebuild inputs, collision inputs, texture registry behavior, topology render-cache invalidation behavior, serialization/schema/import behavior, and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 3B4 next. Remaining debt: remove obsolete `SectorEditorState::topologyMap` storage, retarget compatibility wrappers/tests to document-owned map or explicit map references, and confirm `topologyMap` no longer appears as a member of `SectorEditorState`. |
| Phase 3B4: TopologyMap Compatibility Cleanup And Tests | Completed | 2026-07-07 | Source changed. Removed obsolete `SectorEditorState::topologyMap` storage. Removed obsolete broad reset/authoring compatibility wrappers that could only delegate through the old state-owned map, added explicit `SectorTopologyMap&` targets to remaining authoring geometry mutation helpers, and retargeted central/editor test paths to document-owned `SectorEditorDocumentState::map.topologyMap` or explicit map references. Light-editing and authoring graph test fixtures now provide document-owned map storage explicitly. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; focused grep checks reviewed. `rg -n "state\\.topologyMap|SectorTopologyMap topologyMap|struct SectorEditorState" sources/sector_editor tests` now reports only `struct SectorEditorState` and the document-owned `SectorEditorDocumentState::map.topologyMap` field. Source-hash/global metadata grep was reviewed. | No behavior changes intended. `SectorTopologyMap` remains document-owned live storage in `SectorEditorDocumentState::map.topologyMap` and remains derived output plus documented topology-owned global metadata/runtime definitions. Texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data. Metadata copying, derivation output installation, baked metadata restoration, preview rebuild inputs, collision inputs, texture registry behavior, topology render-cache invalidation behavior, serialization/schema/import behavior, and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Phase 3B topology map ownership implementation is complete. Continue with Phase 3C next; remaining broad map/derivation consumer dependency cleanup is deferred to that phase. |
| Phase 3C: Derived Map Consumer Retargeting | Completed | 2026-07-07 | Source changed. Retargeted two remaining narrow consumers away from broad editor-state access where practical: `DrawTopologyVertexInspector()` now receives `const SectorTopologyMap&` directly instead of `SectorEditorState&`, and `CurrentSectorEditorMaterialPickerTexture()` now receives explicit topology map, authoring graph/access, derivation access, and picker state without an unused `SectorEditorState&`. Added the direct `SectorEditorSurfaceTypes.h` include to `SelectionState` after removing an umbrella include from the vertex inspector exposed the transitive dependency. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 3 grep checks reviewed. Exact grep for obsolete `state.topologyMap` and obsolete `state.authoringDerivation*` / `state.lastValidAuthoringDerivedTopology` member access returned no matches. Source-hash/global metadata grep was reviewed. | No behavior changes intended. No new fields moved. REF-086 remained executable independently of REF-085 implementation status. `SectorTopologyMap` remains derived output plus documented topology-owned global metadata/runtime definitions, and texture registry/global metadata behavior was unchanged. Topology render-cache invalidation behavior was unchanged; this pass did not add topology mutation paths. Serialization/schema/import behavior and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Phase 3C and Phase 3 are complete. Broad dependencies remain where caused by preview state, modal UI state, inspector input buffers, tool transaction state, runtime object editing state, render-cache state, or renderer ownership. Execute Phase 4 next. |
| Phase 4: Document Lifecycle Dirty/Path/Status State | Completed | 2026-07-07 | Source changed. Moved `topologyDocumentInitialized`, `topologyDocumentDirty`, `topologyDocumentStatus`, `currentLevelName`, `currentLevelPath`, `hasCurrentLevelPath`, and `hasUnsavedChanges` out of `SectorEditorState` into `SectorEditorDocumentLifecycleState`. Added narrow lifecycle access views and routed central editor lifecycle reads/writes, dirty helpers, document reset, authoring mutation/derivation helpers, inspector/material/light editing services, texture-picker apply paths, lightmap bake/install checks, preview overlay dirty display, save/load/reload modal paths, status bar/window-title text, and tests through document-owned lifecycle state or explicit lifecycle references. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 4 grep checks reviewed. `rg -n "state\\.topologyDocument|state\\.currentLevel|state\\.hasCurrentLevelPath|state\\.hasUnsavedChanges" sources/sector_editor tests` returned no matches; moved lifecycle fields now appear in `SectorEditorDocumentLifecycleState` / lifecycle accessors and intentional consumer contexts. | No behavior changes intended. Dirty marker behavior, unsaved prompts/checks, current path/name behavior, document initialized behavior, status text, and save/load confirmation behavior were preserved. Topology render-cache invalidation behavior was preserved by keeping cache ownership in `SectorEditorState` and passing render-cache revision/cache explicitly where dirty helpers need them. REF-086 remained executable independently of REF-085; preview ownership was not moved. Serialization/schema/import behavior and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 5 next. Remaining broad dependencies include load/save/reset/import/migration helper retargeting, topology render-cache ownership decision, and cleanup of remaining document dependencies in tools/panels/services. |
| Phase 5: Load/Save/Reset/Import/Migration Retargeting | Completed | 2026-07-07 | Source changed. Retargeted reset/import initialization boundaries away from broad `SectorEditorState&`: `InitializeSectorEditorAuthoringStateFromTopology()` now receives only the authoring graph, derivation access, and source map it actually uses, and `ResetEditorTopologyDocumentState()` now receives document lifecycle/map/authoring/derivation state or the document root plus preview controller state. `SectorEditor::SaveLevelFromModal()` now calls the document sub-state save overload using `documentState.authoring`, `documentState.map`, and `documentState.derivation`. Tests were retargeted to the narrower helper signatures. No `SectorEditorDocumentController` was added. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 5 grep checks reviewed. `rg -n "SectorEditorState&\|const SectorEditorState&" sources/sector_editor/document sources/sector_editor/SectorEditorDirtyState.*` returned no matches. `LoadSectorEditorDocumentFromAsset()` still detects authoring graph and topology-v2 import formats, and topology-v2 import still routes through `ImportSectorTopologyMapToAuthoringGraph()`. | No behavior changes intended. JSON schema, save/load JSON format, format detection, topology-v2 migration/import behavior, reset/new document defaults, status/error strings, texture registry behavior, and map-level metadata copying were preserved. Topology render-cache invalidation behavior was preserved; reset still rebuilds document state and authoring edits still use existing dirty/invalidation paths. Lightmap metadata/source-hash behavior was unchanged, including loaded baked lightmap restoration in the central load path. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Execute Phase 6 next. Remaining work is topology render-cache ownership decision, dependency cleanup in tools/panels/services, and final audit/backlog refresh. |
| Phase 6: Topology Render Cache And Invalidation Ownership | Completed | 2026-07-07 | Documentation/decision pass completed. Re-evaluated `topologyRenderWarning`, `topologyRenderRevision`, and `topologyRenderCache`; no source fields moved. The cache remains in `SectorEditorState` for now because inspection confirmed it is derived editor-only 2D draw/picking/warning state and ownership is ambiguous between central composition and a future 2D view/cache state rather than clearly document-owned. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 6 grep checks reviewed. Cache fields still appear as `SectorEditorState` members by design because no move occurred. Validation/index/earcut calls remain in the existing render-cache build path; the grep also shows the pre-existing `SectorEditor::PointInTopologySector()` loop extraction used by sector picking. No new steady 2D draw-path rebuilds were added. | Topology render-cache invalidation behavior was preserved: topology/document edits still dirty the document, increment the render revision, and invalidate the cache; authoring mutations and material/light edits still use the existing invalidation paths. Rebuild timing remains lazy through `EnsureTopologyRenderCache()` when the cache is invalid or revision-mismatched. Warning/status behavior was unchanged. Serialization/schema behavior and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Future debt: create a dedicated 2D editor view/cache state or keep central composition until that state exists; Phase 7 may narrow dependencies around the chosen central/deferred ownership without moving cache fields into DocumentState. |
| Phase 7: Document Dependency Cleanup | Completed | 2026-07-07 | Documentation-only split/meta pass. Phase 7 proved too broad as one implementation pass because the remaining broad `SectorEditorState&` / `SectorEditorUiState&` dependencies span selection/manipulation contexts, tool contexts, material editing and picker routing, inspector panels, material inspector UI, and placed-object/runtime-object editing paths. Added Phase 7A through Phase 7D as ordered child passes and stopped without source implementation. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 7 grep checks reviewed. Grep inventory found broad dependencies in `selection/SectorEditorMoveContext.h`, `selection/SectorEditorManipulationService.h`, `tools/SectorEditorToolModule.h`, material editing/picker routing, inspector panel/material inspector contexts, and placed-object/runtime object tool paths. Existing `std::function` callbacks remain for current scoped callbacks/modals and runtime-object paths; no new callback bridge was added. | No behavior changes. No document fields moved. REF-086 remains executable independently of REF-085 implementation status. Preview ownership, modal state, inspector input state, tool transaction state, runtime object editing state, renderer ownership, topology render-cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior were unchanged. No callback bridges were added. No manual GUI verification was performed. | Execute Phase 7A next. Later Phase 7 child passes must narrow only their scoped dependencies and record broad dependencies left because of preview, modal, inspector input, tool transaction, runtime-object editing, or renderer state. |
| Phase 7A: Selection And Tool Context Dependency Narrowing | Completed | 2026-07-07 | Source changed. Narrowed selection/manipulation and core authoring tool context dependencies: `SectorEditorManipulationServiceContext` no longer carries broad `SectorEditorState&`, `SectorEditorMoveContext` no longer carries broad `SectorEditorState&`, and placed-object drag/move now receives explicit `SectorTopologyMap&` plus `RuntimeObjectDragState&` where required by the selected-move path. `SectorEditorToolContext` now exposes explicit current-tool and pending line/rectangle/insert-vertex references, and the line, rectangle, and insert-vertex tools use those references instead of broad state for their tool transaction data. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; Phase 7 grep checks reviewed. Remaining broad-state grep matches are expected in material editing/picker routing, inspector/material inspector, placed-object action/inspector paths, and the transitional `SectorEditorToolContext` field left for later passes. `git diff --stat` and `git status --short` reviewed after plan update. | No behavior changes intended. Selection behavior, drag behavior, picking behavior, authoring mutation behavior, runtime-object behavior, topology render-cache invalidation behavior, preview rebuild behavior, serialization/schema behavior, and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. REF-086 remains executable independently of REF-085 implementation status. No callback bridges were added. No manual GUI verification was performed. | Phase 7B still needs material editing and picker routing dependency narrowing. Phase 7C still needs inspector/material-inspector narrowing. Phase 7D still needs placed-object/runtime-state audit; broad dependencies remain there because runtime-object editing state, modal/UI state, inspector input buffers, and feature routing are intentionally out of Phase 7A scope. |
| Phase 7B: Material Editing And Picker Routing Dependency Narrowing | Completed | 2026-07-07 | Source changed. Narrowed material editing and material picker routing dependencies: `SectorEditorMaterialEditingServiceContext` and `SectorEditorMaterialPickerRoutingContext` no longer carry broad `SectorEditorState&`; material services now receive explicit lifecycle, topology map, authoring graph, derivation, topology render warning/cache fields, texture picker state, decal tint modal state, material/selection UI state, status text, and preview rebuild callback. Added narrow authoring mutation/derivation overloads that take render-cache references so material paths can preserve dirty/cache behavior without broad editor state. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 7 grep checks reviewed. `services/material_edit` has no remaining broad `SectorEditorState&`/`context.state` dependency or `SectorEditor.h` include. Remaining broad-state grep matches are expected in inspector/material-inspector contexts, placed-object/runtime-object paths, and the transitional generic tool context for later Phase 7C/7D. | No behavior changes intended. Material picker target semantics, texture selection behavior, material/sector property edit routing, authoring graph source-of-truth behavior, topology render-cache invalidation paths, and preview rebuild requests were preserved. Save/load JSON format, serialization/schema, import/migration behavior, topology-only writeback behavior, and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No callback bridges were added. No manual GUI verification was performed. | Phase 7C still needs inspector panel/material inspector dependency narrowing. Phase 7D still needs placed-object/runtime-state audit; texture picker modal lifecycle, UI/modal state, inspector buffers, runtime-object editing state, preview ownership, and renderer ownership remain outside Phase 7B scope. |
| Phase 7C: Inspector Panel And Material Inspector Dependency Narrowing | Completed | 2026-07-07 | Source changed. Narrowed the topology sidedef material inspector context so `SectorEditorMaterialInspectorContext` no longer carries broad `SectorEditorState&` or `SectorEditorUiState&`; it now receives explicit `SectorTopologyMap&`, `const SectorAuthoringGraph&`, `SelectionState&`, `engine::UIScrollState&` for inspector scroll positioning, material UI state, material editing service, texture catalog, callbacks, and status text. The inspector panel builder passes those narrow references into the material inspector. Existing inspector-panel broad state/UI dependencies remain only where this pass left modal/runtime-object/current-tool/inspector-input-buffer/light/sector inspector paths in scope-limited central composition. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 7 grep checks reviewed. `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.*` has no remaining `SectorEditorState&`, `SectorEditorUiState&`, `context.state`, or `context.uiState` matches. Remaining broad-state grep matches are expected in `inspector/SectorEditorInspectorPanel.*`, generic tool context, and placed-object/runtime-object paths for Phase 7D/deferred modal and input-buffer debt. | No behavior changes intended. Inspector display/edit behavior, material inspector target behavior, placed-object inspector requests, light edit requests, bake requests, dirty/cache invalidation paths, and preview rebuild requests were preserved. Topology render-cache invalidation behavior was unchanged; no topology mutation path was added. Serialization/schema behavior and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No callback bridges were added. REF-086 remained executable independently of REF-085 implementation status. No manual GUI verification was performed. | Phase 7D still needs the placed-object/runtime-state dependency audit. Broad `SectorEditorState&` and `SectorEditorUiState&` dependencies remain in the main inspector panel and placed-object inspectors/actions because runtime-object editing state, modal/UI state, inspector input buffers, current-tool routing, preview/runtime state, and renderer ownership are outside Phase 7C scope. |
| Phase 7D: Placed Object And Runtime-State Dependency Audit | Completed | 2026-07-07 | Documentation-only audit/defer pass. Re-ran the Phase 7 broad-dependency grep checks and inspected the remaining placed-object, billboard, door, drag, and runtime-object paths. No source changes were made because the remaining placed-object/runtime broad dependencies are caused by runtime-object editing state, modal/UI state, inspector input buffers, top-level tool routing, and sprite/billboard metadata repair state rather than document-owned state. | Passed: `cmake --build cmake-build-debug -j2` (no work to do); `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; Phase 7 grep checks reviewed. Remaining broad-state matches are expected in `inspector/SectorEditorInspectorPanel.*`, `tools/SectorEditorToolModule.h`, `tools/placed_objects/SectorEditorPlacedObjectActions.h`, and `tools/placed_objects/SectorEditorPlacedObjectInspector.h`. | No behavior changes. Runtime-object creation, selection, dragging, cached draw updates, deletion, door/billboard inspector behavior, dirty/cache invalidation paths, preview/collision refresh behavior, topology render-cache invalidation behavior, serialization/schema behavior, and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. REF-086 remained executable independently of REF-085 implementation status. No callback bridges were added. No manual GUI verification was performed. | Phase 8 remains. Deferred dependency cleanup: `SectorEditorPlacedObjectActionContext` still carries broad `SectorEditorState&` only to clear `runtimeObjectDrag`; placed-object/billboard/door inspector contexts still carry broad `SectorEditorState&` / `SectorEditorUiState&` for sprite metadata repair/cache fields, door texture settings modal routing, and runtime-object inspector input buffers; `SectorEditorToolContext` still carries broad state for top-level tool transaction/routing debt; the main inspector panel still carries broad state/UI for modal, current-tool, input-buffer, and runtime-object routing. These are future runtime-object editing/tool/modal/UI-state cleanup items, not DocumentState work. |
| Phase 8: Final Dependency Audit And Backlog Refresh | Completed | 2026-07-07 | Documentation-only final audit/backlog refresh completed. Ran the final moved-field and broad-dependency greps, refreshed `docs/audit/sector_editor_state_ownership_and_remaining_map.md` with the post-REF-086 ownership map, and updated REF-086 backlog notes from plan-only to planning-and-implementation complete. Confirmed `SectorEditorDocumentState` owns authoring source, derivation bookkeeping, document map, and lifecycle state; `SectorEditorState` still owns only the deferred 2D topology render cache fields from the original document candidate list. | Passed before final plan update: `cmake --build cmake-build-debug -j2` (no work to do); `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; final Phase 8 grep checks reviewed. A final post-plan-update `git diff --check`, `git diff --stat`, and `git status --short` were also run. | No behavior changes. REF-085 preview/runtime/controller/collision/camera state remained separate and renderer ownership stayed outside DocumentState. Phase 3 split derivation bookkeeping from `topologyMap`; `SectorTopologyMap` remains compiled/derived output plus documented map-level metadata/runtime definitions, and still contains texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata. No `SectorEditorDocumentController` was created. REF-086 remained executable independently of REF-085 implementation status. Topology render-cache invalidation behavior, serialization/schema/import behavior, and lightmap source-hash behavior were unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Remaining debt: topology render cache ownership is deferred to a future 2D view/cache state if needed; modal/UI state boundaries, runtime-object editing boundaries, tool transaction/routing state, non-material inspector input buffers, and manual editor smoke remain future work. |

## Execution Tracking Rules

- Each phase must compile and pass tests before moving to the next phase.
- Each implementation phase must be independently buildable and testable.
- Each phase must update the `plan-state-json` item status and the Current Progress table before finishing.
- Use `In Progress` only while source/doc changes are actively incomplete.
- Mark a phase `Completed` only after implementation, verification, behavior notes, and plan updates are done.
- If a phase is intentionally skipped, mark it `Deferred` and record why.
- If a phase cannot proceed without a decision, mark it `Blocked` and record the exact blocker.
- If a phase proves too broad, split it into child pass items in the JSON block and Current Progress table, then stop.
- If Phase 2, Phase 5, or Phase 7 would touch too many unrelated call sites in one run, split that phase into child passes in the `plan-state-json` block and Current Progress table, then stop without source changes.
- Phase 3 is pre-split into Phase 3A, 3B, and 3C. Each child pass must compile and pass tests before the next begins. Do not merge these child passes during execution.
- Suggested additional child-pass examples, if needed: `phase_02a` authoringGraph field move only, `phase_02b` authoring graph helper/context retargeting, `phase_05a` save/load helpers retarget, `phase_05b` reset/new document/import retarget, `phase_05c` migration/legacy topology conversion retarget, `phase_07a` tool context document dependency narrowing, and `phase_07b` inspector/service context document dependency narrowing.
- Do not rewrite unrelated phase text while updating progress.
- Do not claim manual GUI verification unless it was actually performed.
- Do not mark any DocumentState implementation phase complete from the original planning task alone.

## Summary

REF-086 prepares the sector editor document/source-of-truth state for ownership outside the monolithic `SectorEditorState`.

The implementation target is a small document-state root composed by `SectorEditor`, with narrow named sub-states for authoring source data, derivation bookkeeping, compiled/document map state, lifecycle dirty/path/status state, and cautiously deferred topology render/cache state. The goal is to make tools, panels, services, preview helpers, load/save/reset/import code, and cache invalidation code receive explicit document state or document views where needed.

This is not a feature plan and not a generic document manager plan. It must preserve current editor behavior, current JSON schema, current authoring graph source-of-truth rules, current topology render-cache invalidation contracts, and current lightmap source-hash behavior.

## Architecture Contract

- `SectorEditor` composes document state, preview state, renderer, services, and UI/modal state.
- `SectorEditor` remains responsible for top-level app/editor orchestration.
- `SectorEditorDocumentState` may be a small root aggregate composed by `SectorEditor`.
- `SectorEditorDocumentState` must group state into narrow named sub-states by responsibility.
- `SectorEditorDocumentState` must not become everything that used to be in `SectorEditorState` with a nicer name.
- Authoring graph data remains the editable source of truth for normal geometry/material editing.
- `SectorTopologyMap` remains derived output plus documented topology-owned global map metadata/runtime definitions for now.
- Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.
- REF-086 must be executable whether REF-085 implementation is complete or not.
- Do not assume PreviewState fields have already moved. If preview/runtime/controller/collision fields still live in `SectorEditorState`, leave them untouched.
- DocumentState work may pass document/map references to preview call sites, but must not move preview ownership.
- Tools, panels, services, preview helpers, and document helpers receive explicit document state, document sub-state, or read-only document views where needed.
- Document modules must not include `SectorEditor.h`.
- Document helpers/services must not call `SectorEditor::` methods.
- Do not create callback bridges back into `SectorEditor` to hide ownership problems.

## Current Problem

`SectorEditorState` still owns document/source-of-truth state mixed with preview state, 2D view state, modal state, tool transaction state, runtime object editing state, and top-level coordinator state.

Current document-owned candidates in `sources/sector_editor/SectorEditorTypes.h` include:

- `topologyMap`
- `authoringGraph`
- `authoringDerivation`
- `lastValidAuthoringDerivedTopology`
- `authoringDerivationState`
- `authoringDerivedTopologyStale`
- `authoringDerivationStatus`
- `topologyDocumentInitialized`
- `topologyDocumentDirty`
- `topologyDocumentStatus`
- `currentLevelName`
- `currentLevelPath`
- `hasCurrentLevelPath`
- `hasUnsavedChanges`
- `topologyRenderWarning`
- `topologyRenderRevision`
- `topologyRenderCache`

Current important helper boundaries that still take broad state include `SectorEditorAuthoringState.*`, `SectorEditorDirtyState.*`, `SectorEditorDocumentActions.*`, topology render-cache build paths, material editing/picker routing, selection/manipulation contexts, inspector panels, preview rebuild/bake paths, and central lifecycle methods in `SectorEditor.cpp`.

## Target Document State Ownership Model

Default state shape:

- `SectorEditorDocumentState`: small aggregate/root composed by `SectorEditor`.
- `SectorEditorAuthoringDocumentState`: owns `SectorAuthoringGraph authoringGraph`, unless a later implementation phase proves the root should embed the source graph directly because the group is tiny.
- `SectorEditorDerivationState`: owns `SectorAuthoringDerivationResult authoringDerivation`, `std::optional<SectorTopologyMap> lastValidAuthoringDerivedTopology`, `SectorEditorAuthoringDerivationState authoringDerivationState`, `bool authoringDerivedTopologyStale`, and `std::string authoringDerivationStatus`.
- `SectorEditorDocumentMapState` or `SectorEditorCompiledMapState`: owns `SectorTopologyMap topologyMap` as compiled/derived topology output plus documented topology-owned map metadata/runtime definitions for now.
- `SectorEditorDocumentLifecycleState`: owns `topologyDocumentInitialized`, `topologyDocumentDirty`, `topologyDocumentStatus`, `currentLevelName`, `currentLevelPath`, `hasCurrentLevelPath`, and `hasUnsavedChanges`.
- `SectorEditorDocumentRenderCacheState` or `SectorEditorTopologyCacheState`: considered only if Phase 6 proves `topologyRenderWarning`, `topologyRenderRevision`, and `topologyRenderCache` are clearly document-owned rather than view-owned. The default is to defer them to a future 2D view/cache state or leave them centrally composed.

The plan may choose different names only if the implementation phase explains why in the Current Progress notes and final report. Avoid naming the `topologyMap` owner `SectorEditorDerivedTopologyState` unless the implementation explicitly states that the state includes documented map-level metadata/runtime definitions, not just derived geometry. `SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions for now. Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.

The split must remain responsibility-based. Preview runtime/controller/collision/camera state, modal UI state, inspector input buffers, tool transaction state, texture picker modal state, renderer ownership, and runtime object editing state stay outside DocumentState.

## Non-Negotiable Guardrails

- Do not change editor behavior intentionally.
- Do not implement game mode.
- Do not move `PreviewState` as part of REF-086.
- Do not move preview runtime/controller/collision/camera state as part of REF-086.
- REF-086 must be executable whether REF-085 implementation is complete or not.
- Do not assume PreviewState fields have already moved.
- If preview/runtime/controller/collision fields still live in `SectorEditorState`, leave them untouched.
- Do not retarget preview state as part of document work.
- DocumentState work may pass document/map references to preview call sites, but must not move preview ownership.
- Do not move modal UI state into DocumentState.
- Do not move inspector input buffers into DocumentState.
- Do not move tool transaction state into DocumentState.
- Do not move renderer ownership into DocumentState.
- Do not move source-of-truth map editing into `SectorTopologyMap`.
- Do not reintroduce no-authoring topology edit fallback.
- Authoring graph remains the editable source of truth for normal geometry/material editing.
- `SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions for now.
- Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.
- Do not change serialization/schema.
- Do not change save/load JSON format.
- Do not change import/migration behavior.
- Do not change topology render-cache invalidation behavior.
- Do not change lightmap/source-hash behavior.
- Do not change rendering, collision, sector lookup, physics, or camera behavior.
- Do not make document modules depend on `SectorEditor.h`.
- Do not call `SectorEditor::` methods from document helpers/services.
- Do not create callback bridges back into `SectorEditor` to hide ownership problems.
- Do not create another god state object under a new name.
- Do not add a generic `EditorStateService`, generic `DocumentManager`, generic `SceneManager`, generic `WorldManager`, generic `TopologyService`, service locator, event bus, plugin architecture, command bus, undo framework, broad repository abstraction, new serialization framework, new asset/resource manager abstraction, callback bundle layer, broad inheritance hierarchy, or abstract `IDocumentSomething` interface hierarchy.

## Baseline Files To Inspect

Before implementing any phase, inspect at least:

- `docs/architecture/sector_editor_architectural_principles.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`
- `docs/plans/ref084_service_state_ownership_runner_plan.md`
- `docs/plans/ref085_preview_state_ownership_runner_plan.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/runner_compatible_plans.md`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_editor/SectorEditorTextureModals.*`
- `sources/sector_editor/SectorEditorSectorInspector.*`
- `sources/sector_editor/SectorEditorMaterialActions.*`
- `sources/sector_editor/document/`
- `sources/sector_editor/inspector/`
- `sources/sector_editor/tools/`
- `sources/sector_editor/selection/`
- `sources/sector_editor/preview/`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/services/texture_catalog/`
- `sources/sector_editor/services/lights/`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_demo/SectorAuthoringGraph.*`
- `sources/sector_demo/SectorTopologyMap.*`
- `sources/sector_demo/SectorTopologyTypes.*`
- `sources/sector_demo/SectorLightmap.*`
- `sources/sector_demo/SectorRuntimeObjects.*`
- `sources/sector_demo/renderer/`

## Phase 1: DocumentState Inventory And Root/Sub-State Skeleton

### Goal

Create the initial document-state ownership skeleton and confirm the exact sub-state grouping. Phase 1 is skeleton/inventory only.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/document/SectorEditorDocumentActions.*`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`

### Files Likely To Modify

- Add `sources/sector_editor/document/SectorEditorDocumentState.h` or `sources/sector_editor/SectorEditorDocumentState.h`.
- Possibly add narrow sub-state headers under `sources/sector_editor/document/`.
- Modify `sources/sector_editor/SectorEditor.h` to compose the new document state.
- Modify `sources/sector_editor/SectorEditorTypes.h` only for declarations/includes needed by the skeleton.
- Do not edit CMake unless a later implementation task explicitly permits it. If new `.cpp` files would require CMake, prefer header-only state structs for this phase or stop.

### Exact Implementation Steps

1. Re-inventory the candidate document fields in `SectorEditorState` and confirm whether each is document/source data, derivation bookkeeping, compiled/document map data, lifecycle state, cache state, or editor-only transient state.
2. Decide the exact initial split. Default to `SectorEditorDocumentState` with authoring/source, derivation, document map, lifecycle, and deferred render-cache sub-states.
3. Add only plain data structs with no behavior.
4. Phase 1 may compose an empty or not-yet-used `SectorEditorDocumentState documentState;` in `SectorEditor` only if that compiles cleanly.
5. Do not move `authoringGraph`, `topologyMap`, derivation fields, lifecycle fields, render-cache fields, load/save behavior, dirty behavior, or cache invalidation behavior.
6. Field moves begin in later dedicated phases only.
7. If adding the skeleton alone requires broad call-site edits, split Phase 1 or stop without source changes.
8. Compile and pass tests before moving to Phase 2.

### Behavior Guardrails

- No editor behavior should change.
- No serialization/schema/save/load/import behavior should change.
- No topology render-cache invalidation behavior should change.
- No lightmap source-hash behavior should change.
- No preview/runtime/controller/collision/camera state should move.
- DocumentState must not absorb modal, inspector input, tool transaction, texture picker, or renderer state.
- No document fields or document behavior may move in Phase 1 unless a human later amends this plan.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add or update tests only if a source field move requires fixture retargeting. Do not add asset-tree-dependent tests.

### Grep Checks

- `rg -n "struct SectorEditorState|topologyMap|authoringGraph|authoringDerivation|lastValidAuthoringDerivedTopology|topologyDocument|currentLevel|hasUnsavedChanges|topologyRender" sources/sector_editor`
- `rg -n "SectorEditorDocumentState|SectorEditorAuthoringDocumentState|SectorEditorDerivationState|SectorEditorDocumentMapState|SectorEditorCompiledMapState|SectorEditorDocumentLifecycleState|SectorEditorDocumentRenderCacheState|SectorEditorTopologyCacheState" sources/sector_editor`
- Confirm any moved fields no longer appear as members of `SectorEditorState`.

### Stop Conditions

- Stop if adding the skeleton requires CMake edits.
- Stop if adding the skeleton alone requires broad call-site edits.
- Stop if the state split would create a flat god `SectorEditorDocumentState`.
- Stop if the phase requires moving `authoringGraph`, `topologyMap`, derivation fields, lifecycle fields, render-cache fields, load/save behavior, dirty behavior, cache invalidation behavior, authoring mutation behavior, preview state, modal state, inspector buffers, tool transaction state, or renderer ownership.

### Final Report Requirements

- List exact document state/sub-state structs added.
- Explicitly state that no document fields moved, unless this plan is later amended by a human to allow Phase 1 field movement.
- State source struct/file and destination state object/file only for skeleton structs and composition.
- State that the authoring graph source-of-truth contract was preserved.
- Mention topology render-cache invalidation behavior.
- Mention lightmap source-hash behavior.
- State that serialization/schema, rendering, collision, sector lookup, physics, and camera behavior were unchanged.

### What Remains For Later Phases

- Authoring graph ownership move.
- Derived topology and derivation ownership move.
- Lifecycle dirty/path/status move if not done here.
- Load/save/reset/import retargeting.
- Cache ownership decision.

### Phase Answers

- Candidate fields are from `SectorEditorState` in `SectorEditorTypes.h` to document state under `sources/sector_editor/document/`.
- `SectorEditor` owns the new root state and passes sub-states later.
- No state fields move in Phase 1 unless this plan is later amended by a human.
- Serialized data does not move in format; only in-memory ownership may move.
- Cache invalidation, serialization, and lightmap source-hash behavior must remain unchanged.

## Phase 2: Authoring Source State

### Goal

Move `authoringGraph` ownership out of `SectorEditorState` into document-owned state while preserving the authoring graph as the editable source of truth.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/tools/line/`
- `sources/sector_editor/tools/rectangle/`
- `sources/sector_editor/tools/insert_vertex/`
- `sources/sector_editor/tools/select/`
- `sources/sector_editor/selection/`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/SectorEditorSectorInspector.*`
- `sources/sector_demo/SectorAuthoringGraph.*`

### Files Likely To Modify

- `sources/sector_editor/document/SectorEditorDocumentState.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- Focused tool, selection, material, and inspector context call sites that read or mutate `authoringGraph`.

### Exact Implementation Steps

1. Move `SectorAuthoringGraph authoringGraph` from `SectorEditorState` to the chosen authoring/source document sub-state.
2. Update direct reads such as selection validation, authoring overlays, material target resolution, inspector target resolution, and save document construction to use explicit document state or `SectorAuthoringGraph&`.
3. Update direct mutations such as line draw, rectangle, insert vertex, delete, move, face-anchor reconciliation, and material/sector property mutation helpers to use explicit document state.
4. Keep all normal geometry/material editing writes in authoring graph data.
5. Preserve dirty/cache invalidation calls after authoring graph mutations.
6. Preserve selection mapping and stale selection pruning behavior.
7. Do not add any fallback that edits `SectorTopologyMap` when authoring graph data is missing.
8. Compile and pass tests before moving to Phase 3.

### Behavior Guardrails

- Sector draw, rectangle, line, insert-vertex, select, and authoring selection mapping behavior must remain unchanged.
- Authoring graph mutation helpers must still mark document dirty and invalidate derived/cache state as before.
- No direct topology edit fallback may return.
- Material and sector property edits must still write authoring data where they do today.
- Serialization/schema and saved JSON shape must not change.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update targeted authoring graph tests only if existing fixtures need retargeting.

### Grep Checks

- `rg -n "authoringGraph" sources/sector_editor tests`
- `rg -n "state\\.authoringGraph" sources/sector_editor tests`
- `rg -n "no-authoring|NoAuthoring|TopologyOnly|Legacy|Scratch|writeback" sources/sector_editor/services/material_edit sources/sector_editor/tools sources/sector_editor/SectorEditorAuthoringState.*`
- Confirm `authoringGraph` no longer appears as a member of `SectorEditorState`.

### Stop Conditions

- Stop if the phase would touch too many unrelated call sites; split into child passes and stop without source changes.
- Stop if moving `authoringGraph` requires moving all derivation and topology map ownership in the same run.
- Stop if a caller would need a callback bridge to `SectorEditor`.
- Stop if normal editor actions would edit `SectorTopologyMap` because authoring state is unavailable.

### Final Report Requirements

- List the exact field moved and new owner.
- State which helpers/contexts now receive authoring document state or `SectorAuthoringGraph&`.
- State that authoring graph remains the editable source of truth.
- Mention dirty/cache invalidation behavior.
- Mention serialization/schema behavior.
- Mention lightmap source-hash behavior.
- State that rendering, collision, sector lookup, physics, and camera behavior were unchanged.

### What Remains For Later Phases

- Derived topology ownership.
- Lifecycle path/dirty/status state.
- Load/save/import retargeting may still depend on broader document state.
- Broad tool/service dependencies may remain until Phase 7.

### Phase Answers

- `authoringGraph` moves from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorAuthoringDocumentState` or the equivalent source sub-state.
- `SectorEditor` owns the document root; tools, panels, services, and helpers receive explicit graph/document references.
- The moved state is document/source data and is serialized as part of the authoring document, but the JSON schema must not change.
- The move can affect cache invalidation if dirty paths are missed; preserve existing `MarkSectorEditorAuthoringGraphEdited()` / invalidation behavior.

### Phase 2A: Authoring Graph Access Boundary Prep

Goal:

Prepare narrow authoring graph access boundaries so later passes can retarget call sites incrementally before the actual ownership move. Do not move `authoringGraph` in this pass.

Exact implementation steps:

1. Add only small explicit access/context helpers if needed to pass `SectorAuthoringGraph&` / `const SectorAuthoringGraph&` from `SectorEditor` composition points.
2. Prefer direct graph references over broad `SectorEditorState&` where a caller only needs authoring graph access.
3. Do not change storage ownership yet; `SectorEditorState::authoringGraph` remains the backing storage for this prep pass.
4. Do not change mutation behavior, dirty/cache invalidation behavior, save/load JSON shape, import behavior, or authoring source-of-truth semantics.
5. Compile and pass tests before Phase 2B.

Stop if the prep layer starts becoming a generic document manager, callback bundle, service locator, or broad state wrapper.

Final report requirements:

- List any access/context helpers added.
- State that `authoringGraph` storage did not move in Phase 2A.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2B: Authoring Helpers And Document Actions Retarget

Goal:

Retarget authoring helper and document action boundaries that currently reach through broad editor state primarily for authoring graph access. Do not move `authoringGraph` in this pass.

Exact implementation steps:

1. Retarget focused functions in `SectorEditorAuthoringState.*` to receive explicit graph references or narrow authoring document state where practical.
2. Retarget document save/load construction helpers in `sources/sector_editor/document/SectorEditorDocumentActions.*` only where they access `authoringGraph`.
3. Preserve authoring derivation, stale/current/last-valid behavior, and topology map ownership until Phase 3.
4. Preserve dirty/cache invalidation calls after authoring graph mutations.
5. Compile and pass tests before Phase 2C.

Stop if this pass requires moving derivation fields, `topologyMap`, lifecycle state, or changing serialization/import behavior.

Final report requirements:

- List helper/document action signatures retargeted.
- State that `authoringGraph` storage did not move in Phase 2B.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2C: Central Editor And Tool Authoring Graph Retarget

Goal:

Retarget central `SectorEditor` composition and focused authoring tool call sites to use explicit graph references/context instead of directly reaching through `state.authoringGraph`. Do not move `authoringGraph` in this pass.

Exact implementation steps:

1. Retarget focused `SectorEditor.cpp` authoring graph reads/mutations to local explicit references where practical.
2. Retarget line, rectangle, insert-vertex, select, selection/manipulation, and related tool contexts that currently access `context.state.authoringGraph`.
3. Preserve selection mapping and stale selection pruning behavior.
4. Preserve dirty/cache invalidation calls after all authoring mutations.
5. Compile and pass tests before Phase 2D.

Stop if this pass requires moving tool transaction state, selection state, preview state, derivation fields, `topologyMap`, or lifecycle state.

Final report requirements:

- List central/tool contexts retargeted.
- State that `authoringGraph` storage did not move in Phase 2C.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2D: Inspector Material Preview And Tests Retarget

Goal:

Retarget remaining authoring graph consumers in inspector, material routing, preview panels, and tests so the final ownership move is mostly storage/wiring. Do not move `authoringGraph` in this pass.

Exact implementation steps:

1. Retarget `SectorEditorSectorInspector.*`, `inspector/`, `services/material_edit/`, and preview UI contexts that currently access authoring graph through broad editor state.
2. Retarget tests that construct `SectorEditorState` only to reach `authoringGraph`, using explicit `SectorAuthoringGraph` or the future authoring document state shape where practical.
3. Preserve material and sector property edits as authoring graph writes.
4. Preserve no-authoring fallback removal; do not add topology-only writeback routes.
5. Compile and pass tests before Phase 2E.

Stop if this pass requires changing material picker semantics, preview ownership, serialization/schema, import behavior, or source-of-truth rules.

Final report requirements:

- List inspector/material/preview/test consumers retargeted.
- State that `authoringGraph` storage did not move in Phase 2D.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E: AuthoringGraph Ownership Move

Goal:

Split the final `SectorAuthoringGraph authoringGraph` ownership move into small executable passes after earlier Phase 2 passes narrowed call sites. This parent/meta pass is complete; do not select it for implementation again.

Exact implementation steps:

1. Execute Phase 2E1 through Phase 2E5 as direct child passes of Phase 2.
2. Do not execute Phase 2E as a monolithic implementation pass.
3. Keep Phase 2 `Partial` until Phase 2E1 through Phase 2E5 complete or are explicitly deferred.
4. Preserve dirty/cache invalidation, selection mapping, stale selection pruning, serialization/schema, save/load/import behavior, and authoring graph source-of-truth behavior throughout the child passes.
5. Compile and pass tests for each child pass before moving to the next.

Stop if any child pass requires moving derivation fields, `topologyMap`, lifecycle state, preview state, modal state, inspector buffers, or tool transaction state.

Final report requirements:

- State which Phase 2E child pass was executed.
- State whether `authoringGraph` storage moved in that child pass.
- State that authoring graph remains the editable source of truth.
- State which helpers/contexts now receive authoring document state or `SectorAuthoringGraph&`, if any.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E1: Authoring Document State Storage And Editor Wiring

Goal:

Split document authoring graph storage and central wiring into small executable passes. This parent/meta pass is complete; do not select it for implementation again.

Exact implementation steps:

1. Execute Phase 2E1A, Phase 2E1B, and Phase 2E1C as direct child passes of Phase 2.
2. Do not execute Phase 2E1 as a monolithic implementation pass.
3. Keep Phase 2 `Partial` until Phase 2E1A through Phase 2E1C and the remaining Phase 2E passes complete or are explicitly deferred.
4. Preserve dirty/cache invalidation, serialization/schema/save/load/import behavior, authoring source-of-truth behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior throughout the child passes.
5. Compile and pass tests for each child pass before moving to the next.

Stop if any child pass requires retargeting all authoring-state wrappers, material-picker compatibility wrappers, document-action compatibility wrappers, or stateful test fixtures in the same run.

Final report requirements:

- State which Phase 2E1 child pass was executed.
- State exact live authoring graph storage after that child pass.
- State whether temporary compatibility storage or wrappers remain.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E1A: Authoring Document Graph Storage Field

Goal:

Introduce the document-owned authoring graph field without changing the live editor graph routing yet.

Exact implementation steps:

1. Add `SectorAuthoringGraph authoringGraph` to `SectorEditorAuthoringDocumentState`.
2. Add only narrowly required includes or document access helper overloads.
3. Keep `SectorEditorState::authoringGraph` as the live compatibility storage for this pass.
4. Do not wire mutable editor contexts to `documentState.authoring.authoringGraph` yet.
5. Do not change load/save/import/reset behavior, dirty/cache invalidation behavior, authoring source-of-truth behavior, or tests beyond compile-required include fixes.
6. Compile and pass tests before Phase 2E1B.

Stop if adding the field requires retargeting broad authoring-state wrappers, material-picker compatibility wrappers, document-action compatibility wrappers, or stateful test fixtures.

Final report requirements:

- State that `SectorEditorAuthoringDocumentState` now contains the future document-owned authoring graph field.
- State that `SectorEditorState::authoringGraph` remains the live compatibility storage after Phase 2E1A.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E1B: Central Authoring Graph Accessor Wiring

Goal:

Prepare central `SectorEditor` wiring so the later transfer can change one named access path instead of mixing direct storage references.

Exact implementation steps:

1. Add narrow `SectorEditor` accessors or local composition helpers for mutable and const authoring graph access.
2. Retarget central `SectorEditor` composition points and already-retargeted context builders to the accessor/helper where practical.
3. Keep the accessor/helper backed by the current live graph until Phase 2E1C.
4. Do not change which storage receives runtime mutations in this pass.
5. Preserve dirty/cache invalidation, serialization/schema/save/load/import behavior, authoring source-of-truth behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior.
6. Compile and pass tests before Phase 2E1C.

Stop if this pass requires moving or duplicating load/save/import ownership, retargeting all authoring-state wrappers, or changing material picker/document action compatibility semantics.

Final report requirements:

- List the central accessors/helpers and context builders retargeted.
- State that the live backing storage did not transfer in Phase 2E1B.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E1C: Already-Retargeted Context Graph Transfer

Goal:

Split the already-retargeted `SectorEditor` graph transfer into small executable passes. This parent/meta pass is complete; do not select it for implementation again.

Exact implementation steps:

1. Execute Phase 2E1C1, Phase 2E1C2, and Phase 2E1C3 as direct child passes of Phase 2.
2. Do not execute Phase 2E1C as a monolithic implementation pass.
3. Keep Phase 2 `Partial` until Phase 2E1C1 through Phase 2E1C3 and the remaining Phase 2E passes complete or are explicitly deferred.
4. Preserve dirty/cache invalidation, serialization/schema/save/load/import behavior, authoring source-of-truth behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior throughout the child passes.
5. Compile and pass tests for each child pass before moving to the next.

Stop if any child pass would leave two unsynchronized live authoring graphs on normal editor mutation paths, or if it requires moving derivation fields, `topologyMap`, lifecycle state, preview state, modal state, inspector buffers, or tool transaction state.

Final report requirements:

- State which Phase 2E1C child pass was executed.
- State the exact live authoring graph backing storage after that child pass.
- State whether temporary compatibility storage or wrappers remain.
- List already-retargeted contexts switched to the document-owned graph, if any.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E1C1: Authoring Mutation Wrapper Transfer Prep

Goal:

Retarget the authoring-state mutation and derivation helpers already reached by central/tool context paths so they can operate on an explicit `SectorAuthoringGraph&` before the central accessor storage switch. Keep `SectorEditorState::authoringGraph` as the live backing storage in this prep pass.

Exact implementation steps:

1. Add focused explicit-graph overloads for authoring mutation helpers that central/tool paths already call, including line/rectangle insertion and mutation helpers as needed for normal authoring edits.
2. Add focused explicit-graph support for derivation refresh paths only where required to keep mutations, reconciliation, stale flags, and last-valid behavior tied to the same graph reference.
3. Retarget already-retargeted central/tool call sites to pass the explicit graph while the central accessor still returns `SectorEditorState::authoringGraph`.
4. Do not switch `SectorEditor::AuthoringGraph()` to `documentState.authoring.authoringGraph` in this pass.
5. Do not retarget material picker compatibility overloads, document-action compatibility overloads, or stateful test fixtures unless required by these exact central/tool call sites.
6. Preserve dirty/cache invalidation, serialization/schema/save/load/import behavior, authoring source-of-truth behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior.
7. Compile and pass tests before Phase 2E1C2.

Stop if this prep requires broad derivation ownership moves, `topologyMap` ownership moves, lifecycle ownership moves, or a callback bridge back into `SectorEditor`.

Final report requirements:

- List mutation/derivation helpers retargeted to explicit graph references.
- State that the central accessor storage did not switch in Phase 2E1C1.
- State the exact live backing storage after the pass.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E1C2: Material And Inspector Mutation Transfer Prep

Goal:

Retarget material editing, material picker routing, and inspector paths that already receive explicit graph contexts but still call broad authoring mutation compatibility helpers, so those paths will not read one graph and mutate another after the transfer. Keep `SectorEditorState::authoringGraph` as the live backing storage in this prep pass.

Exact implementation steps:

1. Retarget material editing and material picker routing paths that already receive `SectorAuthoringGraph&` to pass that graph through mutation helpers instead of relying on `SectorEditorState::authoringGraph`.
2. Retarget inspector paths that already receive `SectorAuthoringGraph&` to pass that graph through mutation helpers where practical.
3. Preserve material picker semantics, material and sector property edit routing, preview rebuild behavior, and authoring document construction behavior.
4. Do not switch `SectorEditor::AuthoringGraph()` to `documentState.authoring.authoringGraph` in this pass.
5. Do not change save/load JSON format, import/migration behavior, source-of-truth rules, derivation ownership, topology map ownership, or lifecycle ownership.
6. Compile and pass tests before Phase 2E1C3.

Stop if this prep requires changing material picker behavior, save/load JSON shape, import behavior, topology-only writeback behavior, or preview ownership.

Final report requirements:

- List material/inspector paths retargeted to pass explicit graph references through mutations.
- State that the central accessor storage did not switch in Phase 2E1C2.
- State the exact live backing storage after the pass.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E1C3: Central Accessor Storage Switch

Goal:

Switch `SectorEditor::AuthoringGraph()` to `documentState.authoring.authoringGraph` after the prep passes make normal editor reads and writes single-source-of-truth safe.

Exact implementation steps:

1. Switch the central accessor/helper from Phase 2E1B to the document-owned graph.
2. Wire already-retargeted tool, selection, manipulation, inspector, material, preview overlay/UV, topology render-cache, and central composition contexts through the document-owned graph via that access path.
3. Update central load/reset/install points already covered by the accessor path so new, loaded, or imported authoring documents populate the same document-owned graph that tools and panels read.
4. Keep temporary compatibility storage or wrappers only where later Phase 2E2 through Phase 2E5 work still requires them.
5. Do not retarget all broad compatibility wrappers, material-picker compatibility overloads, document-action compatibility overloads, or stateful test fixtures in this pass unless the call site is already locally covered by the central access path.
6. Preserve dirty/cache invalidation, serialization/schema/save/load/import behavior, authoring source-of-truth behavior, lightmap source-hash behavior, rendering, collision, sector lookup, physics, and camera behavior.
7. Compile and pass tests before Phase 2E2.

Stop if the switch would leave two unsynchronized live authoring graphs on normal editor mutation paths, or if it requires moving derivation fields, `topologyMap`, lifecycle state, preview state, modal state, inspector buffers, or tool transaction state.

Final report requirements:

- State the exact live authoring graph backing storage after Phase 2E1C3.
- State whether temporary compatibility storage or wrappers remain.
- List already-retargeted contexts switched to the document-owned graph.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E2: Authoring State Compatibility Wrapper Retarget

Goal:

Retarget `SectorEditorAuthoringState.*` compatibility wrappers that still take broad `SectorEditorState&` for authoring graph access.

Exact implementation steps:

1. Retarget wrappers that only need authoring graph access to receive `SectorAuthoringGraph&`, `const SectorAuthoringGraph&`, or authoring document state.
2. Preserve derivation, topology map, and lifecycle access through existing state until later REF-086 phases.
3. Preserve authoring mutation behavior, selection pruning, stale/current derivation behavior, and existing dirty/cache invalidation calls.
4. Do not move derivation fields, `topologyMap`, lifecycle state, preview state, modal state, inspector buffers, or tool transaction state.
5. Compile and pass tests before Phase 2E3.

Stop if this pass requires broad load/save/import retargeting, topology map ownership changes, or lifecycle ownership changes.

Final report requirements:

- List authoring-state wrappers retargeted.
- State remaining broad-state wrappers and why they remain.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E3: Material Picker And Document Action Compatibility Retarget

Goal:

Retarget remaining material-picker and document-action compatibility overloads that still reach through `SectorEditorState::authoringGraph`.

Exact implementation steps:

1. Keep explicit graph overloads as the primary path.
2. Retarget broad compatibility overloads to document-owned authoring graph access where practical.
3. Preserve material picker semantics, material and sector property edit routing, save/load/import JSON behavior, and authoring document construction behavior.
4. Do not change serialization/schema, import/migration behavior, source-of-truth rules, derivation ownership, topology map ownership, or lifecycle ownership.
5. Compile and pass tests before Phase 2E4.

Stop if this pass requires changing material picker behavior, save/load JSON shape, import behavior, or topology-only writeback behavior.

Final report requirements:

- List material-picker and document-action compatibility paths retargeted.
- State that save/load/import JSON behavior was preserved.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E4: Authoring Graph Test Fixture Retarget

Goal:

Retarget stateful authoring graph tests so fixtures provide authoring graph storage explicitly instead of relying on `SectorEditorState::authoringGraph`.

Exact implementation steps:

1. Retarget tests that construct `SectorEditorState` only to reach `authoringGraph`.
2. Prefer test-owned `SectorAuthoringGraph` or `SectorEditorDocumentState` fixtures, matching the production ownership shape after Phase 2E1.
3. Use generated test data, temporary JSON files, or existing immutable fixtures outside the mutable engine asset tree.
4. Do not depend on user-edited levels under `assets/levels` or `assets/sector_demo`.
5. Compile and pass tests before Phase 2E5.

Stop if test retargeting requires production behavior changes outside the authoring graph ownership boundary.

Final report requirements:

- List test fixtures retargeted.
- State that tests do not depend on mutable user-edited level assets.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

### Phase 2E5: Remove SectorEditorState AuthoringGraph Storage

Goal:

Remove `SectorAuthoringGraph authoringGraph` from `SectorEditorState` after remaining call sites have explicit document-owned authoring graph access.

Exact implementation steps:

1. Remove `SectorAuthoringGraph authoringGraph` from `SectorEditorState`.
2. Retarget any final remaining call sites to `documentState.authoring.authoringGraph`, explicit `SectorAuthoringGraph&`, or `const SectorAuthoringGraph&`.
3. Confirm `authoringGraph` no longer appears as a member of `SectorEditorState`.
4. Preserve dirty/cache invalidation, selection mapping, stale selection pruning, serialization/schema/save/load/import behavior, and authoring graph source-of-truth behavior.
5. Complete Phase 2 only if Phase 2E5 leaves no remaining Phase 2 child pass incomplete or explicitly deferred.
6. Compile and pass tests before Phase 3.

Stop if removing the storage requires moving derivation fields, `topologyMap`, lifecycle state, preview state, modal state, inspector buffers, or tool transaction state.

Final report requirements:

- List exact field removed and destination owner/file.
- State that authoring graph remains the editable source of truth.
- State which helpers/contexts now receive authoring document state or `SectorAuthoringGraph&`.
- Mention dirty/cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

## Phase 3: Document Map And Derivation State

### Goal

Move derivation bookkeeping and document map ownership into document-owned state through the pre-split child passes below. Phase 3 is not executable as one monolithic pass; execute Phase 3A, then Phase 3B, then Phase 3C.

`SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions for now. Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/SectorEditorMaterialActions.*`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/services/texture_catalog/`
- `sources/sector_editor/services/lights/`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_editor/preview/`
- `sources/sector_demo/SectorTopologyMap.*`
- `sources/sector_demo/SectorTopologyTypes.*`
- `sources/sector_demo/SectorRuntimeObjects.*`
- `sources/sector_demo/SectorLightmap.*`

### Files Likely To Modify

- `sources/sector_editor/document/SectorEditorDocumentState.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- Focused material, light, preview, bake, selection, inspector, and texture call sites.

### Exact Implementation Steps

1. Execute only the selected child pass.
2. Phase 3A moves derivation bookkeeping state and must not move `topologyMap`.
3. Phase 3B moves `topologyMap` to document map state and must preserve its classification as compiled/derived output plus documented map-level metadata/runtime definitions.
4. Phase 3C retargets remaining consumers to explicit document map/derivation references and must not move new fields unless fixing missed integration from Phase 3A or Phase 3B.
5. Each child pass must compile and pass tests before the next begins.

### Behavior Guardrails

- Authoring graph remains the source of truth for editable geometry/material data.
- `SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions.
- Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.
- Do not move source-of-truth map editing into `SectorTopologyMap`.
- Do not change derivation timing, last-valid behavior, ID mapping, preview rebuild inputs, bake inputs, or texture registry behavior.
- Do not change light/runtime object/topology-owned metadata behavior.
- Do not change serialization/schema or save/load format.
- REF-086 must be executable whether REF-085 implementation is complete or not. If preview/runtime/controller/collision fields still live in `SectorEditorState`, leave them untouched.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update targeted derivation or material mapping tests only if existing tests need retargeting.

### Grep Checks

- `rg -n "topologyMap|authoringDerivation|lastValidAuthoringDerivedTopology|authoringDerivationState|authoringDerivedTopologyStale|authoringDerivationStatus" sources/sector_editor tests`
- `rg -n "state\\.topologyMap|state\\.authoringDerivation|state\\.lastValidAuthoringDerivedTopology|state\\.authoringDerivationState|state\\.authoringDerivedTopologyStale|state\\.authoringDerivationStatus" sources/sector_editor tests`
- `rg -n "ComputeSectorLightmapSourceHash|bakedLightmap|lightmapSettings|directionalLight|skySettings|previewSettings|texturesById|runtimeObjects" sources/sector_editor sources/sector_demo`
- Confirm moved fields no longer appear as members of `SectorEditorState`.

### Stop Conditions

- Stop if the selected child pass would touch too many unrelated call sites; split that child pass further and stop without source changes.
- Stop if moving `topologyMap` would require changing serialization/schema or import behavior.
- Stop if map-level metadata ownership is unclear and would be reclassified casually.
- Stop if source-hash affecting settings might be dropped from bake input/hash behavior.
- Stop if preview ownership would need to move or if the pass assumes REF-085 implementation has completed.

### Final Report Requirements

- List exact derivation or map fields moved and new owner.
- State how `topologyMap` is classified: derived output plus documented topology-owned global metadata/runtime definitions.
- State that moving `topologyMap` did not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.
- State derivation timing and last-valid behavior.
- State texture registry/global metadata behavior.
- State whether the current child pass was Phase 3A, 3B, or 3C and whether the later Phase 3 child passes remain.
- Mention topology render-cache invalidation behavior.
- Mention lightmap source-hash behavior explicitly.
- State serialization/schema, rendering, collision, sector lookup, physics, and camera behavior.

### What Remains For Later Phases

- Lifecycle dirty/path/status state if not moved.
- Load/save/reset/import retargeting.
- Cache ownership decision.
- Remaining broad dependencies in tools/panels/services.

### Phase Answers

- Derivation bookkeeping fields move from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorDerivationState` or equivalent in Phase 3A.
- `topologyMap` moves from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorDocumentMapState` or `SectorEditorCompiledMapState` or equivalent in Phase 3B.
- `SectorEditor` owns the document root; derivation helpers, preview, render-cache, bake, material, light, and selection call sites receive explicit derivation state or document map references.
- Moved derivation state is derived/bookkeeping data. Moved map state is compiled/derived topology output plus documented map-level metadata/runtime definitions. `SectorTopologyMap` is serialized through existing document save/load machinery, but the JSON schema must not change.
- The move can affect cache invalidation, serialization, and lightmap source-hash behavior if call sites are missed; preserve all existing contracts.

### Phase 3A: Derivation Bookkeeping State

Goal:

Move `authoringDerivation`, `lastValidAuthoringDerivedTopology`, `authoringDerivationState`, `authoringDerivedTopologyStale`, and `authoringDerivationStatus` into `SectorEditorDerivationState` or equivalent. Do not move `topologyMap` in this pass.

Files to inspect and likely modify are the Phase 3 files above, focused on derivation status, last-valid topology, authoring target mapping, and derivation refresh helpers.

Exact implementation steps:

1. Move only `authoringDerivation`, `lastValidAuthoringDerivedTopology`, `authoringDerivationState`, `authoringDerivedTopologyStale`, and `authoringDerivationStatus`.
2. Retarget `RefreshSectorEditorAuthoringDerivation()`, derivation-current checks, authoring target resolution, inspector target resolution, material mapping, and status display call sites to `SectorEditorDerivationState` or explicit references.
3. Do not move `topologyMap`.
4. Preserve derivation timing, stale/current/invalid status semantics, last-valid topology behavior, topology ID mapping, and authoring target mapping.
5. Compile and pass tests before Phase 3B.

Stop if `topologyMap` movement becomes necessary, if this pass would change serialization/schema/import behavior, or if the pass would require preview ownership changes.

Final report requirements:

- List the five derivation bookkeeping fields moved.
- State that `topologyMap` did not move in Phase 3A.
- State derivation timing, stale/current/invalid status, and last-valid topology behavior.
- Mention topology render-cache invalidation, serialization/schema, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

Phase 3A was split on 2026-07-07 because the five derivation bookkeeping fields have a broad read/write footprint across central editor orchestration, authoring helpers, inspector/material contexts, document actions, preview/render-cache inputs, and tests. Execute the following child passes in order. Do not execute Phase 3A as one monolithic implementation pass.

#### Phase 3A1: Derivation Document Storage And Access Boundary Prep

Goal:

Prepare document-owned derivation storage and narrow access boundaries without changing live storage behavior.

Exact implementation steps:

1. Add only `authoringDerivation`, `lastValidAuthoringDerivedTopology`, `authoringDerivationState`, `authoringDerivedTopologyStale`, and `authoringDerivationStatus` to `SectorEditorDerivationState` or the chosen equivalent.
2. Add narrow helper/access views only if needed to pass `SectorEditorDerivationState&`, `const SectorEditorDerivationState&`, or explicit derivation references from central composition points.
3. Do not switch live storage yet; the existing `SectorEditorState` derivation bookkeeping remains the active backing storage for this prep pass.
4. Do not retarget broad consumer sets unless required to compile the storage/access prep.
5. Do not move `topologyMap`.
6. Compile and pass the Phase 3 checks before Phase 3A2.

Stop if the prep layer starts becoming a generic document manager, callback bundle, service locator, or broad state wrapper.

Final report requirements:

- List the derivation fields added to document-owned state.
- State that live derivation bookkeeping storage did not switch in Phase 3A1.
- State that `topologyMap` did not move.
- Mention topology render-cache invalidation, serialization/schema, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

#### Phase 3A2: Derivation Helper And Context Retarget

Goal:

Retarget focused derivation helper and context consumers to explicit derivation state/references while preserving current live storage.

Exact implementation steps:

1. Retarget `RefreshSectorEditorAuthoringDerivation()` boundaries, derivation-current checks, authoring target resolution, inspector target resolution, material mapping, document action boundaries, preview/render-cache contexts, and service contexts where practical to `SectorEditorDerivationState` or explicit derivation references.
2. Keep the current live backing storage unchanged until Phase 3A3.
3. Preserve derivation timing, stale/current/invalid status semantics, last-valid topology behavior, topology ID mapping, authoring target mapping, and dirty/cache invalidation behavior.
4. Do not move `topologyMap`.
5. Compile and pass the Phase 3 checks before Phase 3A3.

Stop if this pass would require moving `topologyMap`, changing save/load/import/schema behavior, moving preview ownership, or touching unrelated lifecycle/cache ownership.

Final report requirements:

- List the helper/context boundaries retargeted.
- State that live derivation bookkeeping storage did not switch in Phase 3A2.
- State that `topologyMap` did not move.
- Mention topology render-cache invalidation, serialization/schema, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

#### Phase 3A3: Central Derivation Storage Switch

Goal:

Switch central editor derivation access to document-owned derivation bookkeeping storage.

Exact implementation steps:

1. Switch central `SectorEditor` derivation accessors/composition points to `documentState.derivation`.
2. Update reset/load/import/refresh integration so one live derivation bookkeeping source is used by central editor behavior.
3. Preserve baked lightmap restoration into current and last-valid derived topology where it exists today.
4. Preserve derivation timing, stale/current/invalid status semantics, last-valid topology behavior, topology ID mapping, authoring target mapping, and dirty/cache invalidation behavior.
5. Do not move `topologyMap`.
6. Compile and pass the Phase 3 checks before Phase 3A4.

Stop if switching storage would require moving `topologyMap`, changing serialization/schema/import behavior, or moving preview ownership.

Final report requirements:

- State that live `SectorEditor` derivation bookkeeping storage switched to `SectorEditorDocumentState::derivation`.
- List reset/load/import/refresh integration points updated.
- State that `topologyMap` did not move.
- Mention topology render-cache invalidation, serialization/schema, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

#### Phase 3A4: Derivation Compatibility Cleanup And Tests

Goal:

Remove obsolete `SectorEditorState` derivation bookkeeping storage and retarget remaining compatibility/test paths.

Exact implementation steps:

1. Remove the five derivation bookkeeping fields from `SectorEditorState`.
2. Retarget any final remaining call sites and tests to document-owned derivation state or explicit derivation references.
3. Confirm `authoringDerivation`, `lastValidAuthoringDerivedTopology`, `authoringDerivationState`, `authoringDerivedTopologyStale`, and `authoringDerivationStatus` no longer appear as members of `SectorEditorState`.
4. Preserve derivation timing, stale/current/invalid status semantics, last-valid topology behavior, topology ID mapping, authoring target mapping, and dirty/cache invalidation behavior.
5. Do not move `topologyMap`.
6. Compile and pass the Phase 3 checks before Phase 3B.

Stop if cleanup would require moving `topologyMap`, changing serialization/schema/import behavior, or moving preview ownership.

Final report requirements:

- List the five fields removed from `SectorEditorState` and their document-owned destination.
- State that `topologyMap` did not move.
- State derivation timing, stale/current/invalid status, and last-valid topology behavior.
- Mention topology render-cache invalidation, serialization/schema, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

### Phase 3B: Document Map State / topologyMap Ownership

Goal:

Move `topologyMap` into `SectorEditorDocumentMapState` or equivalent, preserving its classification as compiled/derived output plus documented map-level metadata/runtime definitions. Do not change serialization/schema, metadata copying, source-hash behavior, or import behavior.

`SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions for now. Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.

Exact implementation steps:

1. Move only `SectorTopologyMap topologyMap` from `SectorEditorState` to `SectorEditorDocumentMapState`, `SectorEditorCompiledMapState`, or equivalent.
2. Preserve `CopyEditorMapLevelFields()` behavior for texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked lightmap metadata.
3. Preserve derivation output installation, last-valid topology updates, preview/render/cache/lightmap/collision inputs, texture registry behavior, light/runtime object/topology-owned metadata behavior, and baked metadata restoration after load.
4. Do not move derivation bookkeeping fields unless fixing missed integration from Phase 3A.
5. Do not move preview/runtime/controller/collision ownership even if preview call sites still read the map.
6. Compile and pass tests before Phase 3C.

Stop if `topologyMap` ownership is treated as purely derived geometry, if metadata would be reclassified as authoring source data, if source-hash-affecting data could be dropped, or if serialization/import behavior would change.

Final report requirements:

- State exact `topologyMap` destination owner/file.
- State that `SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions.
- State that texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data.
- Mention serialization/schema, import behavior, metadata copying, topology render-cache invalidation, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

Phase 3B was split on 2026-07-07 because `topologyMap` has a broad footprint across central editor orchestration, load/reset/import integration, derivation output installation, metadata copying, preview/render-cache/lightmap/collision inputs, runtime object/light editing, texture registry access, material services, selection/tool contexts, and tests. Execute the following child passes in order. Do not execute Phase 3B as one monolithic implementation pass.

#### Phase 3B1: Document Map Storage And Access Boundary Prep

Goal:

Prepare document-owned topology map storage and narrow access boundaries without changing live storage behavior.

Exact implementation steps:

1. Add only `SectorTopologyMap topologyMap` to `SectorEditorDocumentMapState`, `SectorEditorCompiledMapState`, or the chosen equivalent.
2. Add narrow helper/access views only if needed to pass `SectorEditorDocumentMapState&`, `const SectorEditorDocumentMapState&`, `SectorTopologyMap&`, or `const SectorTopologyMap&` from central composition points.
3. Do not switch live storage yet; the existing `SectorEditorState::topologyMap` remains the active backing storage for this prep pass.
4. Do not retarget broad consumer sets unless required to compile the storage/access prep.
5. Do not move derivation bookkeeping fields unless fixing missed integration from Phase 3A.
6. Compile and pass the Phase 3 checks before Phase 3B2.

Stop if the prep layer starts becoming a generic document manager, callback bundle, service locator, broad state wrapper, or if the pass would change serialization/schema/import behavior.

Final report requirements:

- State the map field added to document-owned state and its owner/file.
- State that live topology map storage did not switch in Phase 3B1.
- State that `SectorTopologyMap` remains derived output plus documented topology-owned global metadata/runtime definitions.
- State that texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data.
- Mention metadata copying, topology render-cache invalidation, serialization/schema/import behavior, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

#### Phase 3B2: Central Topology Map Accessor And Storage Switch

Goal:

Switch central editor topology map access to document-owned map storage.

Exact implementation steps:

1. Add or switch central `SectorEditor` map accessors/composition points to `documentState.map.topologyMap` or the chosen equivalent.
2. Update reset/load/import/derivation-output installation integration so one live topology map source is used by central editor behavior.
3. Preserve `CopyEditorMapLevelFields()` behavior for texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked lightmap metadata.
4. Preserve baked lightmap restoration after authoring-document load and legacy topology load.
5. Preserve preview rebuild inputs, collision inputs, render-cache inputs, lightmap bake inputs, and source-hash-affecting data.
6. Do not remove `SectorEditorState::topologyMap` until Phase 3B4 unless all compatibility paths are already retargeted safely.
7. Do not move preview/runtime/controller/collision ownership.
8. Compile and pass the Phase 3 checks before Phase 3B3.

Stop if switching storage would require changing serialization/schema/import behavior, reclassifying map-level metadata as authoring source data, dropping source-hash-affecting data, or moving preview ownership.

Final report requirements:

- State that live `SectorEditor` topology map storage switched to document-owned map state.
- List reset/load/import/derivation-install integration points updated.
- State that `SectorTopologyMap` remains derived output plus documented topology-owned global metadata/runtime definitions.
- State that texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data.
- Mention metadata copying, topology render-cache invalidation, serialization/schema/import behavior, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

Phase 3B2 was split on 2026-07-07 because the monolithic storage switch would leave central
`SectorEditor` paths reading `documentState.map.topologyMap` while reset/load/import,
authoring derivation installation, and existing authoring/material mutation helpers still write
`SectorEditorState::topologyMap`. Execute the following child passes in order. Do not execute
Phase 3B2 as one monolithic implementation pass.

#### Phase 3B2A: Topology Map Install Target Prep

Goal:

Prepare explicit topology-map install targets for reset/load/import and derivation-output paths
without switching live storage.

Exact implementation steps:

1. Add focused overloads or parameters so reset/new-document helpers can receive the live
   `SectorTopologyMap&` install target explicitly.
2. Add focused overloads or parameters so authoring derivation refresh/installation can copy
   map-level metadata from, and install derived topology into, an explicit live `SectorTopologyMap&`.
3. Retarget only the central call sites needed to keep behavior compiling while still passing
   `SectorEditorState::topologyMap` as the live storage.
4. Preserve `CopyEditorMapLevelFields()` behavior for texture registry, lights, runtime objects,
   preview settings, sky settings, directional light, lightmap settings, and baked lightmap metadata.
5. Preserve baked lightmap restoration after authoring-document load and legacy topology load.
6. Do not switch `SectorEditor::TopologyMap()` storage and do not remove
   `SectorEditorState::topologyMap`.
7. Do not move preview/runtime/controller/collision ownership.
8. Compile and pass the Phase 3 checks before Phase 3B2B.

Stop if this prep requires changing serialization/schema/import behavior, reclassifying map-level
metadata as authoring source data, dropping source-hash-affecting data, or moving preview ownership.

Final report requirements:

- List reset/load/import and derivation-install helper signatures retargeted.
- State that live topology map storage did not switch in Phase 3B2A.
- State that `SectorTopologyMap` remains derived output plus documented topology-owned global
  metadata/runtime definitions.
- State that texture registry, lights, runtime objects, preview settings, sky settings,
  directional light, lightmap settings, and baked metadata were not reclassified as authoring graph
  source data.
- Mention metadata copying, baked metadata restoration, topology render-cache invalidation,
  serialization/schema/import behavior, lightmap source-hash, rendering, collision, sector lookup,
  physics, and camera behavior.

#### Phase 3B2B: Central Map Accessor Wiring

Goal:

Route central `SectorEditor` map composition points through named topology-map accessors while
keeping the live backing storage in `SectorEditorState::topologyMap`.

Exact implementation steps:

1. Add private mutable/const `SectorEditor::TopologyMap()` accessors backed by
   `SectorEditorState::topologyMap`.
2. Retarget central `SectorEditor.cpp` map reads/writes and composition points through the
   accessors where practical, including save, load, reset, preview rebuild, collision, render-cache,
   lightmap bake/install, overlay/picking, and status paths.
3. Use the Phase 3B2A explicit install-target helpers where applicable, still passing
   `TopologyMap()` backed by `SectorEditorState::topologyMap`.
4. Leave broad external service/test compatibility paths for Phase 3B3/3B4 when they are not
   central composition points.
5. Preserve metadata copying, baked metadata restoration, preview rebuild inputs, collision inputs,
   render-cache inputs, lightmap bake inputs, and source-hash-affecting data.
6. Do not switch accessor backing storage to `documentState.map.topologyMap`.
7. Do not move preview/runtime/controller/collision ownership.
8. Compile and pass the Phase 3 checks before Phase 3B2C.

Stop if accessor wiring requires changing serialization/schema/import behavior, changing topology
render-cache invalidation behavior, or moving preview ownership.

Final report requirements:

- List central `SectorEditor` composition points routed through `TopologyMap()`.
- State that live topology map storage did not switch in Phase 3B2B.
- State that `SectorTopologyMap` remains derived output plus documented topology-owned global
  metadata/runtime definitions.
- State that texture registry, lights, runtime objects, preview settings, sky settings,
  directional light, lightmap settings, and baked metadata were not reclassified as authoring graph
  source data.
- Mention metadata copying, baked metadata restoration, topology render-cache invalidation,
  serialization/schema/import behavior, lightmap source-hash, rendering, collision, sector lookup,
  physics, and camera behavior.

#### Phase 3B2C: Document Map Storage Switch

Goal:

Switch central `SectorEditor` topology map storage to `SectorEditorDocumentMapState::topologyMap`
after the install-target and accessor prep passes have made the switch single-source-of-truth safe.

Exact implementation steps:

1. Switch private mutable/const `SectorEditor::TopologyMap()` accessors from
   `SectorEditorState::topologyMap` to `documentState.map.topologyMap`.
2. Update prepared reset/load/import/derivation-install call sites to install into `TopologyMap()`
   backed by document-owned map state.
3. Preserve `CopyEditorMapLevelFields()` behavior for texture registry, lights, runtime objects,
   preview settings, sky settings, directional light, lightmap settings, and baked lightmap metadata.
4. Preserve baked lightmap restoration after authoring-document load and legacy topology load.
5. Preserve preview rebuild inputs, collision inputs, render-cache inputs, lightmap bake inputs, and
   source-hash-affecting data.
6. Keep `SectorEditorState::topologyMap` as obsolete compatibility storage until Phase 3B4 unless
   all compatibility paths are already retargeted safely.
7. Do not move preview/runtime/controller/collision ownership.
8. Compile and pass the Phase 3 checks before Phase 3B3.

Stop if switching storage would require changing serialization/schema/import behavior,
reclassifying map-level metadata as authoring source data, dropping source-hash-affecting data, or
moving preview ownership.

Final report requirements:

- State that live central `SectorEditor` topology map storage switched to document-owned map state.
- List reset/load/import/derivation-install integration points updated.
- State that `SectorTopologyMap` remains derived output plus documented topology-owned global
  metadata/runtime definitions.
- State that texture registry, lights, runtime objects, preview settings, sky settings,
  directional light, lightmap settings, and baked metadata were not reclassified as authoring graph
  source data.
- Mention metadata copying, baked metadata restoration, topology render-cache invalidation,
  serialization/schema/import behavior, lightmap source-hash, rendering, collision, sector lookup,
  physics, and camera behavior.

#### Phase 3B3: Map Mutation Metadata And Preview Integration Retarget

Goal:

Retarget focused topology map mutation and metadata consumers to explicit document map or topology map references while preserving existing ownership boundaries.

Exact implementation steps:

1. Retarget focused consumers that only need map access to `SectorEditorDocumentMapState&`, `SectorTopologyMap&`, or `const SectorTopologyMap&` where practical.
2. Cover texture registry, static/dynamic lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, baked metadata, preview rebuild, render-cache, collision, lightmap bake, material service, selection, and tool contexts as scope allows.
3. Leave broad `SectorEditorState&` dependencies in place when they are caused by preview state, modal UI state, inspector input buffers, tool transaction state, runtime object editing state, or renderer ownership.
4. Preserve metadata copying, source-hash behavior, derivation output installation, last-valid topology behavior, preview rebuild inputs, collision inputs, and texture registry behavior.
5. Do not move preview/runtime/controller/collision ownership.
6. Compile and pass the Phase 3 checks before Phase 3B4.

Stop if dependency cleanup requires moving new fields, changing serialization/schema/import behavior, changing topology render-cache invalidation behavior, or moving preview ownership.

Final report requirements:

- List map consumers retargeted and broad dependencies intentionally left.
- State that no new fields moved except missed integration fixes from Phase 3A or 3B2.
- State that `SectorTopologyMap` remains derived output plus documented topology-owned global metadata/runtime definitions.
- State that texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data.
- Mention metadata copying, topology render-cache invalidation, serialization/schema/import behavior, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

#### Phase 3B4: TopologyMap Compatibility Cleanup And Tests

Goal:

Remove obsolete `SectorEditorState::topologyMap` storage and retarget remaining compatibility/test paths.

Exact implementation steps:

1. Remove `SectorTopologyMap topologyMap` from `SectorEditorState`.
2. Retarget any final remaining call sites and tests to document-owned map state or explicit topology map references.
3. Confirm `topologyMap` no longer appears as a member of `SectorEditorState`.
4. Preserve serialization/schema/import behavior, metadata copying, derivation output installation, baked metadata restoration, preview rebuild inputs, collision inputs, texture registry behavior, topology render-cache invalidation, and lightmap source-hash behavior.
5. Do not move derivation bookkeeping fields unless fixing missed integration from Phase 3A.
6. Do not move preview/runtime/controller/collision ownership.
7. Compile and pass the Phase 3 checks before Phase 3C.

Stop if cleanup would require changing serialization/schema/import behavior, changing topology render-cache invalidation behavior, reclassifying map-level metadata, dropping source-hash-affecting data, or moving preview ownership.

Final report requirements:

- List the field removed from `SectorEditorState` and its document-owned destination.
- State that `SectorTopologyMap` remains derived output plus documented topology-owned global metadata/runtime definitions.
- State that texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data.
- Mention metadata copying, topology render-cache invalidation, serialization/schema/import behavior, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

### Phase 3C: Derived Map Consumer Retargeting

Goal:

Retarget remaining consumers to explicit document map/derivation state references and reduce broad `SectorEditorState` dependency where practical. Do not move new fields unless fixing missed integration from Phase 3A or Phase 3B.

Exact implementation steps:

1. Inventory remaining `SectorEditorState&` dependencies that are now present only for derivation state or document map access.
2. Retarget those consumers to explicit `SectorEditorDerivationState&`, `SectorEditorDocumentMapState&`, `SectorTopologyMap&`, or `const SectorTopologyMap&` references where practical.
3. Leave broad dependencies in place when they are caused by preview state, modal UI state, inspector input buffers, tool transaction state, runtime object editing state, or renderer ownership.
4. Preserve preview separation and REF-085 independence. Do not assume PreviewState fields have moved.
5. Compile and pass tests before Phase 4.

Stop if dependency cleanup requires moving new fields, preview ownership, modal state, inspector buffers, tool transaction state, runtime object editing state, or renderer ownership.

Final report requirements:

- List consumers retargeted and broad dependencies intentionally left.
- State no new fields moved except missed integration fixes from Phase 3A or 3B.
- State REF-086 remained executable independently of REF-085 implementation status.
- Mention topology render-cache invalidation, serialization/schema, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

## Phase 4: Document Lifecycle Dirty/Path/Status State

### Goal

Move dirty/path/status/init state into document-owned lifecycle state.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/document/SectorEditorDocumentActions.*`
- `sources/sector_editor/document/SectorEditorDocumentModals.*`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_editor/SectorEditorPreviewActions.*`

### Files Likely To Modify

- `sources/sector_editor/document/SectorEditorDocumentState.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/document/SectorEditorDocumentActions.*`
- Modal/status panel call sites.

### Exact Implementation Steps

1. Move `topologyDocumentInitialized`, `topologyDocumentDirty`, `topologyDocumentStatus`, `currentLevelName`, `currentLevelPath`, `hasCurrentLevelPath`, and `hasUnsavedChanges` to the document lifecycle sub-state.
2. Retarget `MarkSectorEditorTopologyDocumentEdited()` to receive explicit lifecycle state and render-cache/invalidation state rather than broad `SectorEditorState&`, or provide a narrow document-state helper if that is clearer.
3. Update save/load/new/reload modal open paths to read lifecycle state explicitly.
4. Update status panel/window/title/status text paths that display current level, dirty marker, document status, and unsaved changes.
5. Update lightmap bake install and preview/settings mutation paths that mark unsaved changes.
6. Preserve dirty marker behavior, unsaved prompts, current path/name behavior, and initialization behavior.
7. Compile and pass tests before moving to Phase 5.

### Behavior Guardrails

- Save/load prompts and overwrite confirmation behavior must remain unchanged.
- Dirty marker and unsaved-change checks must remain unchanged.
- Current path/name behavior must remain unchanged.
- Document initialized behavior must remain unchanged.
- Status/error message text should remain unchanged unless an exact existing bug is documented and explicitly in scope.
- REF-086 must be executable whether REF-085 implementation is complete or not. If preview/runtime/controller/collision fields still live in `SectorEditorState`, leave them untouched.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update document action tests only if existing tests need retargeting.

### Grep Checks

- `rg -n "topologyDocumentInitialized|topologyDocumentDirty|topologyDocumentStatus|currentLevelName|currentLevelPath|hasCurrentLevelPath|hasUnsavedChanges" sources/sector_editor tests`
- `rg -n "state\\.topologyDocument|state\\.currentLevel|state\\.hasCurrentLevelPath|state\\.hasUnsavedChanges" sources/sector_editor tests`
- Confirm moved fields no longer appear as members of `SectorEditorState`.

### Stop Conditions

- Stop if moving lifecycle state requires moving modal UI state into DocumentState.
- Stop if dirty/status helpers would need callback bridges to `SectorEditor`.
- Stop if status text behavior would change.
- Stop if unsaved prompt behavior would change.
- Stop if the phase assumes REF-085 implementation has completed or tries to move preview ownership.

### Final Report Requirements

- List exact lifecycle fields moved and new owner.
- State which helpers now receive lifecycle state explicitly.
- State dirty marker, unsaved prompt, current path/name, and document initialized behavior.
- State REF-086 remained executable independently of REF-085 implementation status if preview call sites were touched.
- Mention topology render-cache invalidation behavior.
- Mention serialization/schema and lightmap source-hash behavior.
- State rendering, collision, sector lookup, physics, and camera behavior.

### What Remains For Later Phases

- Load/save/reset/import/migration helper retargeting.
- Cache ownership decision if not done.
- Remaining broad document dependencies in tools/panels/services.

### Phase Answers

- Lifecycle fields move from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorDocumentLifecycleState` or equivalent.
- `SectorEditor` owns lifecycle state; document actions, modals, status panel, bake install, and mutation helpers receive explicit lifecycle references.
- Moved state is document lifecycle state. It is not directly serialized as map data, though current path/name control save/load behavior.
- The move can affect cache invalidation if dirty helpers are wrong; preserve existing invalidation and source-hash behavior.

## Phase 5: Load/Save/Reset/Import/Migration Retargeting

### Goal

Move or prepare narrow document lifecycle helpers for existing load/save/reset/import/migration behavior.

### Files To Inspect

- `sources/sector_editor/document/SectorEditorDocumentActions.*`
- `sources/sector_editor/document/SectorEditorDocumentModals.*`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/services/texture_catalog/`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_demo/SectorTopologyMap.*`
- `sources/sector_demo/SectorAuthoringGraph.*`
- `sources/sector_demo/SectorLightmap.*`

### Files Likely To Modify

- `sources/sector_editor/document/SectorEditorDocumentActions.*`
- `sources/sector_editor/document/SectorEditorDocumentState.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- Focused helper/context headers that still accept `SectorEditorState&` only for document fields.

### Exact Implementation Steps

1. Retarget `BuildSectorAuthoringDocumentFromEditorState()`, `SaveSectorEditorAuthoringDocument()`, `ResetEditorTopologyDocumentState()`, and load/import document install paths to receive explicit document state or document sub-states.
2. Preserve `LoadSectorEditorDocumentFromAsset()` format detection for authoring graph documents and topology-v2 import documents.
3. Preserve old topology-v2 import conversion into authoring graph first.
4. Preserve reset/new document behavior, default texture population, missing-file/sample fallback behavior where currently implemented, and status/error messages.
5. Preserve texture registry behavior and map-level metadata copying during derivation and save.
6. Preserve lightmap metadata/source hash behavior, including loaded baked lightmap restoration after successful derivation.
7. Do not introduce `SectorEditorDocumentController` by default.
8. Prefer retargeting existing document action/helper functions to explicit document state/sub-state references.
9. If a controller seems necessary, stop and add a future backlog item or child planning pass. Do not implement it casually inside REF-086.
10. A controller may only be introduced in REF-086 if a human explicitly amends this plan to allow it after reviewing the proposed shape.
11. Keep `SectorEditor` responsible for top-level orchestration around world/runtime cleanup, asset scopes, preview rebuild, and UI modal routing.
12. REF-086 must remain executable whether REF-085 implementation is complete or not; do not move or retarget preview state as part of this phase.
13. Compile and pass tests before moving to Phase 6.

### Behavior Guardrails

- Do not change current JSON schema.
- Do not change save/load JSON format.
- Do not change import/migration behavior.
- Do not reintroduce no-authoring fallback.
- Missing-file/sample fallback and status/error messages must remain unchanged.
- Texture registry, baked lightmap metadata, and source-hash behavior must remain unchanged.
- Runtime/preview cleanup orchestration remains top-level unless explicitly proven document-only.
- `SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions for now. Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update tests with generated temporary JSON or immutable fixtures outside the engine asset tree if coverage is needed. Do not depend on user-edited levels under `assets/levels` or `assets/sector_demo`.

### Grep Checks

- `rg -n "ResetEditorTopologyDocumentState|SaveSectorEditorAuthoringDocument|LoadSectorEditorDocumentFromAsset|BuildSectorAuthoringDocumentFromEditorState|InitializeSectorEditorAuthoringStateFromTopology|ImportSectorTopologyMapToAuthoringGraph" sources/sector_editor sources/sector_demo tests`
- `rg -n "formatVersion|authoringGraph|linedef|TopologyV2Import|LoadSectorTopologyMapFromJsonString|SaveSectorAuthoringDocument" sources/sector_editor sources/sector_demo tests`
- `rg -n "SectorEditorState&|const SectorEditorState&" sources/sector_editor/document sources/sector_editor/SectorEditorDirtyState.*`

### Stop Conditions

- Stop if the phase would touch too many unrelated call sites; split into child passes and stop without source changes.
- Stop if a controller seems necessary; add a future backlog item or child planning pass instead of implementing it casually inside REF-086.
- Stop if a controller/helper starts becoming a generic `DocumentManager`.
- Stop if schema, save/load output, or migration behavior would change.
- Stop if import would install topology data without authoring graph conversion.
- Stop if preview/runtime/controller cleanup would need to move into DocumentState.
- Stop if the phase assumes REF-085 implementation has completed.

### Final Report Requirements

- State that no `SectorEditorDocumentController` was added, unless a human-amended plan explicitly allowed one.
- If a controller seemed necessary, state that the phase stopped and recorded future planning/backlog debt.
- List exact load/save/reset/import helpers retargeted.
- State JSON schema/save/load/import behavior.
- State old topology-v2 import still converts into authoring graph first.
- State texture registry and lightmap metadata/source hash behavior.
- Mention topology render-cache invalidation behavior.
- State rendering, collision, sector lookup, physics, and camera behavior.

### What Remains For Later Phases

- Topology render cache ownership decision.
- Dependency cleanup in tools/panels/services.
- Audit/backlog refresh.

### Phase Answers

- This phase may move no fields if earlier phases already moved ownership; it retargets lifecycle behavior from broad `SectorEditorState&` to document state.
- `SectorEditor` remains top-level owner/orchestrator and document helpers receive document state explicitly.
- Moved or retargeted state is document source, derived, lifecycle, and serialized map state depending on helper.
- The phase is serialization and source-hash sensitive; behavior must remain unchanged and tests/grep checks must prove no schema fallback returned.

## Phase 6: Topology Render Cache And Invalidation Ownership

### Goal

Preserve and document topology render-cache invalidation/rebuild contracts. The default expectation is that topology render cache probably belongs to a future 2D view/cache state or remains centrally composed until that exists.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/tools/`
- `sources/sector_editor/services/lights/`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/services/texture_catalog/`

### Files Likely To Modify

- `sources/sector_editor/document/SectorEditorDocumentState.h`
- Possibly a new `sources/sector_editor/document/SectorEditorDocumentRenderCacheState.h` or `sources/sector_editor/SectorEditorTopologyCacheState.h`.
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- Focused invalidation/build call sites in `SectorEditor.cpp` and authoring helpers.

### Exact Implementation Steps

1. Re-evaluate `topologyRenderWarning`, `topologyRenderRevision`, and `topologyRenderCache` ownership.
2. Default to leaving `topologyRenderWarning`, `topologyRenderRevision`, and `topologyRenderCache` where they are, or deferring them to a future 2D view/cache state.
3. Do not move these fields into DocumentState unless inspection proves the cache is document-owned rather than view-owned and the move is small.
4. If ownership is ambiguous, leave the fields where they are and record debt.
5. Retarget `InvalidateTopologyRenderCache()`, `MarkSectorEditorTopologyDocumentEdited()`, `MarkSectorEditorAuthoringGraphEdited()`, `EnsureTopologyRenderCache()`, and cache draw paths to explicit cache/document inputs only if that retargeting is needed and small.
6. Preserve explicit invalidation behavior and over-invalidation tolerance.
7. Preserve no expensive steady draw-path rebuild behavior: do not call `ValidateSectorTopologyMap()`, `ExtractSectorTopologyLoops()`, `BuildSectorTopologyIndexes()`, or `mapbox::earcut()` from the steady 2D frame draw path except through the existing cache rebuild path.
8. Preserve warning/status behavior and topology render output.
9. The most important outcome of Phase 6 is preserving and documenting invalidation/rebuild contracts, not forcing a state move.
10. Compile and pass tests before moving to Phase 7.

### Behavior Guardrails

- Topology render-cache invalidation behavior must not change.
- Over-invalidation remains acceptable; missed invalidation remains the bug.
- No expensive derived topology/cache rebuild may be added to the steady draw path.
- Picking behavior must remain consistent with what is drawn.
- Texture registry changes still do not require 2D cache invalidation unless display-state caching changes.
- Baked lightmap result changes should not invalidate the 2D topology render cache unless that cache starts drawing baked data.
- Do not move cache fields into DocumentState unless they are clearly document-owned rather than view-owned and the move is small.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update render-cache tests only if existing tests need retargeting.

### Grep Checks

- `rg -n "topologyRenderWarning|topologyRenderRevision|topologyRenderCache|InvalidateTopologyRenderCache|MarkSectorEditorTopologyDocumentEdited|MarkSectorEditorAuthoringGraphEdited|EnsureTopologyRenderCache" sources/sector_editor tests`
- `rg -n "ValidateSectorTopologyMap\\(|ExtractSectorTopologyLoops\\(|BuildSectorTopologyIndexes\\(|earcut" sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditorTopologyRenderCache.*`
- Confirm moved cache fields no longer appear as members of `SectorEditorState`, if moved.

### Stop Conditions

- Stop if ownership is ambiguous between document and 2D view state; defer the move and record the debt.
- Stop if moving cache fields would be the main objective rather than preserving/documenting invalidation contracts.
- Stop if invalidation ordering would change.
- Stop if cache rebuilds would move into steady frame draw.
- Stop if draw output or picking behavior would change.

### Final Report Requirements

- State whether cache fields moved or were deferred, and why.
- If deferred, state whether the likely future owner is a 2D view/cache state or central composition until that exists.
- List exact cache fields moved and new owner, if any.
- State invalidation behavior and rebuild timing.
- State warning/status behavior.
- Mention lightmap source-hash behavior.
- State rendering, collision, sector lookup, physics, and camera behavior.

### What Remains For Later Phases

- Dependency cleanup based on the chosen cache ownership.
- Any future 2D view/cache state plan if cache ownership is deferred.

### Phase Answers

- Candidate cache fields default to remaining central with documented debt or deferring to a future 2D view/cache state. They move from `SectorEditorState` only if proven document-owned and small.
- `SectorEditor` owns the cache state; render-cache builders/draw paths receive explicit document and cache references.
- Moved state is derived editor-only cache state, not serialized.
- The phase directly affects cache invalidation risk; serialization and lightmap source hash must remain unchanged.

## Phase 7: Document Dependency Cleanup

### Goal

Reduce broad `SectorEditorState` dependencies in tools, panels, services, and helpers now that document state is explicit.

### Files To Inspect

- `sources/sector_editor/tools/`
- `sources/sector_editor/inspector/`
- `sources/sector_editor/selection/`
- `sources/sector_editor/preview/`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/services/texture_catalog/`
- `sources/sector_editor/services/lights/`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/document/`

### Files Likely To Modify

- Tool/service/panel context headers that still accept broad `SectorEditorState&` for document fields.
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- Focused helper headers in `sources/sector_editor/document/`, `selection/`, `tools/`, `inspector/`, and `services/`.

### Exact Implementation Steps

1. Inventory remaining broad `SectorEditorState&` and `SectorEditorUiState&` dependencies caused by document state.
2. Replace broad dependencies with explicit `SectorEditorDocumentState&`, sub-state references, `SectorAuthoringGraph&`, `SectorTopologyMap&`, lifecycle state, cache state, selection state, material state, light state, or services as appropriate.
3. Keep dependencies that are broad because of preview state, modal state, inspector buffers, runtime object editing state, or tool transaction state out of scope and record them as debt.
4. REF-086 must remain executable whether REF-085 implementation is complete or not. If preview/runtime/controller/collision fields still live in `SectorEditorState`, leave them untouched.
5. DocumentState work may pass document/map references to preview call sites, but must not move preview ownership.
6. Do not create callback bridges to route document actions back through `SectorEditor`.
7. Do not create generic state/service infrastructure.
8. Remove unnecessary `SectorEditorTypes.h` or `SectorEditor.h` includes from document-adjacent modules where practical.
9. Compile and pass tests before moving to Phase 8.

### Behavior Guardrails

- Tool/service/panel behavior must remain unchanged.
- Authoring graph source-of-truth contract must remain unchanged.
- Preview separation must remain unchanged.
- Do not assume PreviewState fields have already moved.
- Modal and inspector input state must not move into DocumentState.
- No callback bridges.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update tests only if existing context fixtures need retargeting.

### Grep Checks

- `rg -n "SectorEditorState&|const SectorEditorState&|SectorEditorState\\*|SectorEditorUiState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/document sources/sector_editor/tools sources/sector_editor/inspector sources/sector_editor/selection sources/sector_editor/preview sources/sector_editor/services`
- `rg -n "DocumentState|AuthoringDocumentState|DerivationState|DocumentMapState|CompiledMapState|DocumentLifecycleState|DocumentRenderCacheState|TopologyCacheState" sources/sector_editor`
- `rg -n "callback|std::function" sources/sector_editor/document sources/sector_editor/services/material_edit sources/sector_editor/tools sources/sector_editor/selection`

### Stop Conditions

- Stop if the phase would touch too many unrelated call sites; split into child passes and stop without source changes.
- Stop if dependency cleanup requires moving preview, modal, inspector buffer, tool transaction, runtime object editing, or renderer state.
- Stop if the cleanup assumes REF-085 implementation has completed.
- Stop if a callback bridge to `SectorEditor` would be needed.
- Stop if a helper starts becoming a generic manager/service locator.

### Final Report Requirements

- List broad dependencies removed.
- List broad dependencies intentionally left and why.
- State exact document state/sub-states received by tools/panels/services.
- State REF-086 remained executable independently of REF-085 implementation status.
- State no callback bridges were added, or list any unavoidable temporary bridge as debt.
- Mention topology render-cache invalidation behavior.
- Mention serialization/schema and lightmap source-hash behavior.
- State rendering, collision, sector lookup, physics, and camera behavior.

### What Remains For Later Phases

- Final audit/backlog refresh.
- Any deferred preview/modal/inspector/tool/runtime-object dependency cleanup.

### Phase Answers

- This phase should move no new document fields unless a missed field is discovered; it narrows receivers from broad state to explicit document state and related narrow states.
- `SectorEditor` remains composer; tools/panels/services receive explicit references.
- Dependency cleanup state classifications depend on the referenced sub-state and must be recorded per changed context.
- Serialization, cache invalidation, and source-hash behavior must remain unchanged.

Phase 7 was split on 2026-07-07 because the remaining broad dependencies span unrelated call-site families: selection/manipulation and tool contexts, material editing and picker routing, inspector/material UI contexts, and placed-object/runtime-object editing paths. Execute the following child passes in order. Do not execute Phase 7 as one monolithic implementation pass.

#### Phase 7A: Selection And Tool Context Dependency Narrowing

Goal:

Narrow document-caused `SectorEditorState&` dependencies in selection/manipulation and generic tool contexts without moving preview, tool transaction, runtime-object, or renderer ownership.

Exact implementation steps:

1. Inspect `SectorEditorToolContext`, `SectorEditorManipulationServiceContext`, `SectorEditorMoveContext`, their builders in `SectorEditor.cpp`, and tool modules that consume `context.state`.
2. Replace broad state access only where the dependency is now document-owned or already available as explicit references, such as authoring graph, topology map, lifecycle/status text, selection state, manipulation state, light state, or cache invalidation callbacks.
3. Keep broad dependencies that are caused by tool transaction state, preview/runtime/collision state, runtime-object editing, modal UI state, or renderer ownership out of scope and record them as debt.
4. Do not change selection behavior, drag behavior, picking behavior, authoring mutation behavior, runtime-object behavior, topology render-cache invalidation behavior, or preview rebuild behavior.
5. Do not add callback bridges to `SectorEditor` beyond existing narrow callbacks already owned by the contexts.
6. Compile and pass the Phase 7 checks before Phase 7B.

Stop if this pass requires moving preview state, tool transaction state, runtime-object editing state, modal state, inspector input buffers, renderer ownership, or if the cleanup touches material/inspector call sites beyond what is needed to compile.

Final report requirements:

- List selection/tool broad dependencies removed.
- List selection/tool broad dependencies intentionally left and why.
- State exact document state/sub-state or explicit references received by selection/tools.
- State REF-086 remained executable independently of REF-085 implementation status.
- State no callback bridges were added.
- Mention topology render-cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

#### Phase 7B: Material Editing And Picker Routing Dependency Narrowing

Goal:

Narrow document-caused `SectorEditorState&` dependencies in material editing services and material picker routing.

Exact implementation steps:

1. Inspect `SectorEditorMaterialEditingServiceContext`, `SectorEditorMaterialPickerRoutingContext`, material picker open/apply helpers, and their central builders/call sites.
2. Replace broad state access only where material paths already receive or can receive explicit document lifecycle, topology map, authoring graph, derivation, picker/material UI state, status text, texture registry/map references, or preview-rebuild callbacks.
3. Keep broad dependencies caused by texture picker modal state, preview ownership, or UI/modal state out of scope and record them as debt.
4. Preserve material picker target semantics, texture selection behavior, material and sector property edit routing, authoring graph source-of-truth behavior, dirty/cache invalidation paths, and preview rebuild requests.
5. Do not change save/load JSON format, serialization/schema, import/migration behavior, topology-only writeback behavior, or lightmap source-hash behavior.
6. Compile and pass the Phase 7 checks before Phase 7C.

Stop if this pass requires changing material picker behavior, moving texture picker modal state, moving preview ownership, or changing document save/load/import behavior.

Final report requirements:

- List material editing/picker broad dependencies removed.
- List material editing/picker broad dependencies intentionally left and why.
- State exact document state/sub-state or explicit references received by material services.
- State no callback bridges were added.
- Mention topology render-cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

#### Phase 7C: Inspector Panel And Material Inspector Dependency Narrowing

Goal:

Narrow document-caused `SectorEditorState&` and `SectorEditorUiState&` dependencies in inspector panel and material inspector contexts while leaving inspector input buffers and modal/UI state outside DocumentState.

Exact implementation steps:

1. Inspect `SectorEditorInspectorPanelContext`, `SectorEditorMaterialInspectorContext`, their builders in `SectorEditor.cpp`, and inspector/material UI consumers that read broad state.
2. Replace broad state access only where the dependency is document-owned or already available as explicit references, such as lifecycle, topology map, authoring graph, derivation, selection state, inspector ID UI state, material UI state, services, status text, or texture catalog.
3. Keep broad dependencies caused by inspector input buffers, modal UI state, runtime-object editing, preview state, or renderer ownership out of scope and record them as debt.
4. Preserve inspector display/edit behavior, material inspector target behavior, placed-object inspector requests, light edit requests, bake requests, dirty/cache invalidation paths, and preview rebuild requests.
5. Do not move inspector buffers or modal/UI state into DocumentState.
6. Compile and pass the Phase 7 checks before Phase 7D.

Stop if this pass requires moving inspector input buffers, modal/UI state, placed-object/runtime-object editing state, preview state, or renderer ownership.

Final report requirements:

- List inspector/material-inspector broad dependencies removed.
- List inspector/material-inspector broad dependencies intentionally left and why.
- State exact document state/sub-state or explicit references received by inspector contexts.
- State no callback bridges were added.
- Mention topology render-cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

#### Phase 7D: Placed Object And Runtime-State Dependency Audit

Goal:

Handle or explicitly defer remaining Phase 7 broad dependencies in placed-object, billboard, door, drag, and runtime-object editing paths after the document-focused passes have run.

Exact implementation steps:

1. Re-run the Phase 7 grep checks and inspect remaining broad `SectorEditorState&` / `SectorEditorUiState&` dependencies in placed-object actions, placed-object drag, placed-object inspectors, billboard/door inspectors, and related runtime-object paths.
2. Remove broad dependencies only when they are demonstrably document-caused and can be replaced by explicit document/map/lifecycle references or existing narrow callbacks without moving runtime-object state.
3. Defer broad dependencies caused by runtime-object editing state, preview/runtime/collision state, modal state, inspector buffers, tool transaction state, or renderer ownership and record exact files/reasons.
4. Preserve runtime-object creation, selection, dragging, cached draw updates, deletion, door/billboard inspector behavior, dirty/cache invalidation paths, and preview/collision refresh behavior.
5. Do not move runtime-object editing state, preview state, collision state, modal state, inspector buffers, tool transaction state, or renderer ownership.
6. Compile and pass the Phase 7 checks before Phase 8.

Stop if resolving the remaining dependencies requires moving runtime-object state, preview/runtime/collision state, modal state, inspector buffers, tool transaction state, renderer ownership, or adding a callback bridge to `SectorEditor`.

Final report requirements:

- List placed-object/runtime broad dependencies removed.
- List placed-object/runtime broad dependencies intentionally left and why.
- State exact document state/sub-state or explicit references received by any changed runtime/placed-object contexts.
- State REF-086 remained executable independently of REF-085 implementation status.
- State no callback bridges were added.
- Mention topology render-cache invalidation behavior, serialization/schema behavior, lightmap source-hash behavior, and rendering/collision/sector lookup/physics/camera behavior.

## Phase 8: Final Dependency Audit And Backlog Refresh

### Goal

Update the audit and backlog after implementation phases complete.

### Files To Inspect

- `docs/plans/ref086_document_state_ownership_runner_plan.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/document/`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/tools/`
- `sources/sector_editor/inspector/`
- `sources/sector_editor/services/`
- `sources/sector_editor/preview/`

### Files Likely To Modify

- `docs/plans/ref086_document_state_ownership_runner_plan.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`

### Exact Implementation Steps

1. Run grep checks for all moved fields and broad document dependencies.
2. Update Current Progress for every completed/deferred/blocked phase.
3. Update the backlog REF-086 completion notes only after implementation phases are truly complete.
4. Refresh `docs/audit/sector_editor_state_ownership_and_remaining_map.md`.
5. The refreshed audit must include post-REF-086 `SectorEditorState` document fields remaining, new document state/sub-state objects composed by `SectorEditor`, remaining broad `SectorEditorState` / `SectorEditorUiState` dependencies caused by document state, remaining DocumentState debt, whether preview state remained separate, whether renderer ownership stayed outside DocumentState, whether authoring graph remains source of truth, whether Phase 3 split `topologyMap` from derivation bookkeeping, whether `SectorTopologyMap` remains derived output plus documented map metadata/runtime definitions, whether `topologyMap` still contains documented map-level metadata/runtime definitions, whether topology render cache was moved or deferred and why, whether any `SectorEditorDocumentController` was created with the expected normal answer being no, whether REF-086 was executable independently of REF-085 implementation status, whether topology render-cache invalidation behavior was unchanged, whether serialization/schema behavior was unchanged, whether lightmap source-hash behavior was unchanged, and whether rendering/collision/sector lookup/physics/camera behavior was unchanged.
6. Record any remaining document debt, especially cache ownership if deferred, modal/UI state boundaries, runtime object editing boundaries, preview-blocked dependencies, and manual smoke gaps.
7. Do not make source code changes in this final audit phase unless fixing a build break from the immediately preceding phase.
8. Compile and pass tests before closing the plan.

### Behavior Guardrails

- No behavior changes in the final audit phase.
- Do not mark unrelated backlog items complete.
- Do not mark preview implementation phases complete.
- Do not claim manual GUI verification unless performed.
- Do not assume REF-085 implementation has completed.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`

### Grep Checks

- `rg -n "topologyMap|authoringGraph|authoringDerivation|lastValidAuthoringDerivedTopology|authoringDerivationState|authoringDerivedTopologyStale|authoringDerivationStatus|topologyDocumentInitialized|topologyDocumentDirty|topologyDocumentStatus|currentLevelName|currentLevelPath|hasCurrentLevelPath|hasUnsavedChanges|topologyRenderWarning|topologyRenderRevision|topologyRenderCache" sources/sector_editor/SectorEditorTypes.h`
- `rg -n "SectorEditorState&|const SectorEditorState&|SectorEditorState\\*|SectorEditorUiState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/document sources/sector_editor/tools sources/sector_editor/inspector sources/sector_editor/selection sources/sector_editor/preview sources/sector_editor/services`
- `rg -n "REF-085|REF-086|DocumentState|PreviewState" docs/plans/codebase_refactor_backlog.md docs/audit/sector_editor_state_ownership_and_remaining_map.md`

### Stop Conditions

- Stop if implementation phases are incomplete; mark this phase `Blocked` or `Partial` instead of closing the plan.
- Stop if validation fails and source changes are required beyond small immediate fixes.
- Stop if backlog changes would imply preview implementation completion.
- Stop if audit refresh cannot accurately classify remaining document ownership.

### Final Report Requirements

- Summarize final DocumentState ownership.
- List remaining document debt.
- State whether REF-085 implementation remains separate.
- State whether Phase 3 split `topologyMap` from derivation bookkeeping.
- State whether `topologyMap` still contains documented map-level metadata/runtime definitions.
- State whether topology render cache was moved or deferred, and why.
- State whether any `SectorEditorDocumentController` was created; expected answer should normally be no.
- State whether REF-086 was executable independently of REF-085 implementation status.
- Summarize the required audit refresh.
- Mention topology render-cache invalidation behavior.
- Mention serialization/schema behavior.
- Mention lightmap source-hash behavior.
- State rendering, collision, sector lookup, physics, and camera behavior.
- Report build, ctest, diff check, diff stat, and status results.

### What Remains For Later Phases

- Any deferred preview/modal/inspector/tool/runtime-object editing state plan.
- Any future document lifecycle controller planning item only if a human later approves that direction.
- Any future 2D view/cache state plan if topology cache ownership was deferred.

### Phase Answers

- This phase moves no fields unless correcting immediately discovered integration mistakes.
- It updates plan/backlog/audit documentation only.
- It verifies the plan is complete and self-tracking.
- It records remaining debt after the document state migration.

## Tests And Verification

Every implementation phase must run:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Documentation-only phase updates may skip build/ctest only when no source, test, CMake, generated data, or behavior changed, and the final report must say they were skipped.

The planning task that created this file must run:

```bash
git diff --check
git diff --stat
git status --short
```

`cmake --build cmake-build-debug -j2` is optional for the planning task because it changes documentation only.

## Manual Smoke Suggestions

Manual editor smoke is recommended after phases that touch authoring, derivation, lifecycle, cache invalidation, load/save/import, material routing, or status behavior:

- Create a new blank map, draw lines, draw rectangles, insert vertices, move vertices, and delete authoring elements.
- Edit sector names, floor/ceiling heights, sky, Blocks Player, and material/UV/decal values through existing UI paths.
- Save a new level, reload it, overwrite it, and verify dirty markers/status text.
- Load an existing authoring graph level.
- Import a topology-v2 level and verify it converts into authoring graph data before editing.
- Enter and leave Preview3D after document edits.
- Start a lightmap bake only when intentionally verifying bake-sensitive document changes.

Do not claim manual smoke verification unless it was actually performed.

## Backlog Updates

- The planning task that created this file may mark REF-086 as runner-plan written/planning complete only.
- Implementation phases must not be marked complete until this runner plan executes those phases.
- REF-085 must not be modified except to preserve its existing status if already complete as runner-plan written.
- REF-086 implementation must not depend on REF-085 implementation status.
- Do not mark unrelated backlog items complete.
- After final implementation, update REF-086 completion notes with exact moved state, verification results, behavior notes, and remaining debt.

## Stop Conditions

- Stop if a requested phase requires moving preview/runtime/controller/collision/camera state.
- Stop if a phase assumes REF-085 implementation has completed.
- Stop if a phase would retarget preview state as part of document work. Passing document/map references to preview call sites is allowed; moving preview ownership is not.
- Stop if a phase would change serialization/schema or save/load JSON format.
- Stop if a phase would change import/migration behavior.
- Stop if a phase would change topology render-cache invalidation behavior.
- Stop if a phase would change lightmap/source-hash behavior.
- Stop if a phase would change rendering, collision, sector lookup, physics, or camera behavior intentionally.
- Stop if a phase would implement game mode.
- Stop if a phase would introduce broad infrastructure listed in the guardrails.
- Stop if a phase requires document modules to include `SectorEditor.h` or call `SectorEditor::` methods.
- Stop if a phase needs callback bridges back into `SectorEditor` to hide ownership problems.
- Stop if a phase would create a flat god `SectorEditorDocumentState` instead of narrow responsibility-based sub-states.
- Stop if a phase would move modal UI state, inspector input buffers, tool transaction state, texture picker modal state, runtime object editing state, or renderer ownership into DocumentState.
- Stop if Phase 2, Phase 5, or Phase 7 would touch too many unrelated call sites in one run; split that phase into child passes in the `plan-state-json` block and Current Progress table, then stop without source changes. Phase 3 is already split into child passes and must stay split.

## Per-Phase Final Report Requirements

Each phase final report must include:

- exact fields moved
- source struct/file and destination state object/file
- changed owners/receivers
- major call sites changed
- behavior preserved
- tests/checks run
- grep checks run
- remaining debt
- whether moved state is document/source data, derived data, lifecycle state, cache state, or editor-only transient state
- whether moved state is serialized
- whether cache invalidation, serialization, or lightmap source-hash behavior could be affected and how it was preserved
- topology render-cache invalidation behavior
- lightmap source-hash behavior
- rendering, collision, sector lookup, physics, and camera behavior
- REF-085 implementation independence when the phase touches preview call sites or broad state shared with preview
- manual verification performed, or a clear statement that none was performed
