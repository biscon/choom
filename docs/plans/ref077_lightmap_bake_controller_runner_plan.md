# REF-077 LightmapBakeController Runner Plan

## Summary

Extract the lightmap bake worker/controller logic from `SectorEditor.cpp` into a dedicated editor service, tentatively:

- `sources/sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.h`
- `sources/sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.cpp`

This is an implementation plan only. The implementation must preserve current bake behavior and follow `docs/architecture/sector_editor_architectural_principles.md`: `SectorEditor` composes services, services do not include `SectorEditor.h`, services do not call `SectorEditor::` methods, and accepted direct topology writes are limited to bake result metadata/runtime output categories.

No architecture-contract conflict was found for this extraction. The controller boundary must avoid converting this into a generic topology service or a callback bundle back into `SectorEditor`.

## Non-Negotiable Behavior Guardrails

- Do not change bake output, source-hash policy, object probe sidecar format, lightmap metadata, or preview/runtime behavior.
- Do not let the worker thread read or mutate live `SectorEditorState`, `SectorTopologyMap`, `SectorMeshRenderer`, UI state, asset manager, or document state.
- Copy bake inputs before starting the worker. The copied `SectorTopologyMap` snapshot remains the worker input.
- Preserve main-thread result consumption, stale-result rejection, file installation, document dirtying, and preview rebuild behavior.
- Preserve cancellation, join, pending-result, and shutdown cleanup behavior.
- Preserve object probe sidecar install as part of lightmap result install. Do not casually split probe metadata from baked lightmap metadata.
- Preserve temp-file cleanup on pre-start, cancel, failure, stale rejection, install failure, pending-result shutdown, and editor shutdown.
- `SectorEditorLightmapModal.*` must not directly mutate controller internals. The modal should receive a read-only bake view/model and return explicit user intents such as cancel or acknowledge; `SectorEditor` and/or `SectorEditorLightmapBakeController` should apply those intents through controller methods. Do not let the modal mutate worker/progress/result fields, reach into controller internals, or own cancellation, acknowledgement, or terminal state transitions.
- `SectorEditor.cpp` must not grow new compatibility wrappers that survive a phase unless they are explicitly listed as temporary debt in that phase's final report and backlog notes.
- Each implementation phase must compile, pass the selected checks, and leave the editor usable.

Each implementation phase final report must state:

- what behavior moved into `SectorEditorLightmapBakeController`
- what behavior intentionally stayed in `SectorEditor`
- which wrappers remain and why
- whether any wrapper is temporary debt
- confirmation that source-hash behavior is unchanged
- confirmation that object probe sidecar behavior is unchanged
- confirmation that the worker never references live editor state

## Current Bake Flow Inventory

Bake-related functions currently living in `SectorEditor.cpp`:

- `BakeLightmaps()`: thin wrapper around `StartLightmapBake()`.
- `StartLightmapBake()`: validates editor state, builds level paths, computes expected source hash, copies `state.topologyMap` into `SectorTopologyLightmapBakeInput`, clears temp outputs, initializes `lightmapBake`, starts the worker thread, and wires progress/cancel callbacks into `BakeSectorLightmap()`.
- `PollLightmapBakeResult(engine::AssetManager&)`: consumes `pendingResult`, joins the worker, records completion time, and dispatches result consumption.
- `RequestLightmapBakeCancel()`: sets cancellation atomics, modal cancel state, and status text.
- `JoinLightmapBakeWorker()`: joins the worker if joinable.
- `ShutdownLightmapBake()`: requests cancellation, joins, deletes tracked temp lightmap/object-probe files, drops pending result, and resets modal/progress state.
- `IsLightmapBakeBlocking() const`: reports modal/running state.
- `ConsumeLightmapBakeResult(const SectorLightmapBakeAsyncResult&, engine::AssetManager&)`: maps cancelled/failed/succeeded async result into modal/status state, deletes temp files for cancelled/failed results, and calls install for successful results.
- `InstallLightmapBakeResult(const SectorLightmapBakeAsyncResult&, engine::AssetManager&)`: rejects stale results, validates temp lightmap and object-probe sidecar files, copies sidecar and lightmap into final paths, deletes temps, installs `state.topologyMap.bakedLightmap` metadata, marks document dirty, logs report, rebuilds preview meshes when appropriate, and sets status text.

Bake lifecycle state currently involved:

- `SectorEditor::lightmapBake`
- `LightmapBakeAsyncState::worker`
- `LightmapBakeAsyncState::progress`
- `LightmapBakeProgress::{phase, completedWork, totalWork, cancelRequested, running}`
- `LightmapBakeAsyncState::{modalOpen, awaitingAcknowledgement, cancelButtonPressed}`
- `LightmapBakeAsyncState::{startTimeSeconds, completedTimeSeconds}`
- `LightmapBakeAsyncState::{terminalMessage, terminalSuccess, terminalCancelled}`
- `LightmapBakeAsyncState::temporaryOutputPath`
- `LightmapBakeAsyncState::{resultMutex, pendingResult}`
- `SectorLightmapBakeAsyncResult::{succeeded, cancelled, errorMessage, bakeReportText, bakeResult, expectedSourceHash, sourceMapRevision, finalOutputPath, temporaryOutputPath}`
- Editor-owned state used by bake orchestration: `state.hasCurrentLevelPath`, `state.currentLevelName`, `state.topologyMap`, `state.hasUnsavedChanges`, `state.topologyDocumentDirty`, `state.mode`, `preview`, `engineContext`, and `statusText`.

Lower-level bake APIs involved:

- `ComputeSectorLightmapSourceHash(const SectorTopologyMap&)`
- `BakeSectorLightmap(const SectorTopologyLightmapBakeInput&, const SectorLightmapBakeCallbacks&, SectorLightmapBakeResult&, std::string&)`
- `FormatSectorLightmapBakeReport(const SectorLightmapBakeResult&)`
- `MakeSectorObjectProbeSidecarPathForLightmapPath(const std::string&)`
- object probe sidecar write/read/status/runtime helpers in `SectorLightmap.*`

## Proposed Controller Boundary

`SectorEditorLightmapBakeController` should own:

- worker thread lifecycle
- running/cancel/progress atomics
- pending result storage and result mutex
- start-time/completed-time values if the modal continues to read them through controller-owned view state
- cancellation request state
- join after completion and join during shutdown
- shutdown cleanup of tracked temporary files and pending-result temporary files
- pre-start temp-file cleanup for the requested temp lightmap and object probe sidecar
- completed async result storage and consume-once semantics
- stale-result classification if it has the current source hash passed in by `SectorEditor`

`SectorEditorState` or topology map should keep:

- source topology ownership: `state.topologyMap`
- level/document identity: `currentLevelName`, `hasCurrentLevelPath`
- document dirty state: `hasUnsavedChanges`, `topologyDocumentDirty`
- installed bake metadata: `state.topologyMap.bakedLightmap`
- preview/debug/runtime object state
- UI input state

`SectorEditor` should keep:

- menu/button/inspector request routing
- authoring-derived-topology gate checks unless a narrow request object makes moving them clearly behavior-neutral
- modal draw call orchestration, or a narrow modal view object if the modal stops taking `LightmapBakeAsyncState&`
- final preview renderer refresh and document dirty integration, unless a later phase deliberately leaves install fully editor-owned
- status text assignment if messages are returned from controller methods rather than owned inside the controller

Suggested concrete API shape:

- `bool CanStart() const`
- `bool StartBake(SectorEditorLightmapBakeRequest request, std::string& outStatus)`
- `std::optional<SectorLightmapBakeAsyncResult> PollCompletedResult()`
- `void RequestCancel()`
- `void Shutdown()`
- `void AcknowledgeTerminalState()`
- `bool IsBlocking() const`
- `bool HasRunningBake() const`
- `const LightmapBakeProgress& Progress() const`
- `SectorEditorLightmapBakeModalView BuildModalView() const`, only if replacing direct modal access is cleaner than exposing state

Request/result helper types should live with the controller and must not include `SectorEditor.h`.

## Source Hash Behavior To Preserve

Current start behavior:

- `StartLightmapBake()` calls `ComputeSectorLightmapSourceHash(state.topologyMap)` before the worker starts.
- The same value is stored as `input.expectedSourceHash`.
- The worker copies `input.expectedSourceHash` into `SectorLightmapBakeAsyncResult::expectedSourceHash`.
- `BakeSectorLightmap(input, ...)` forces `outResult.sourceHash` and `outResult.objectProbes.sourceHash` to `input.expectedSourceHash` when non-empty.

Current install/stale behavior:

- `InstallLightmapBakeResult()` recomputes `ComputeSectorLightmapSourceHash(state.topologyMap)` on the main thread.
- If the current hash differs from `result.expectedSourceHash`, the temporary lightmap and object probe sidecar are deleted and the result is rejected with `Bake discarded: document changed during bake`.
- On success, `state.topologyMap.bakedLightmap.sourceHash`, `state.topologyMap.bakedLightmap.objectProbes.sourceHash`, and `result.bakeResult.sourceHash` all remain tied to the expected source hash for the copied input.

Current hash sensitivity:

- Source-hash-sensitive: bake version/constants, atlas settings, world-unit conversion, AO/indirect settings, object probe spacing/height, directional light settings, coord subdivisions, referenced texture definitions, vertices, linedefs, sidedefs/materials/UVs including middle textures, sectors including heights/`ceilingSky`/materials/ambient/default wall settings, static point lights, and static spot lights.
- Source-hash-neutral: preview settings including object probe debug draw distance, sky visual settings, dynamic lights/dynamic spot lights, runtime objects/door UV settings that are not baked receiver geometry, baked lightmap metadata itself, and unreferenced texture definitions.

Do not propose or implement any source-hash policy change in REF-077 phases.

## Object Probe Behavior To Preserve

- Object probe settings are part of `SectorLightmapBakeSettings` and participate in `ComputeSectorLightmapSourceHash()`.
- `BakeSectorLightmap()` places and bakes object probes after lightmap image export, then writes a sidecar at `MakeSectorObjectProbeSidecarPathForLightmapPath(outputPath)`.
- Cancel/failure during object probe work deletes temporary outputs as currently done by the bake backend and editor cleanup.
- Install requires the temporary object probe sidecar to exist, copies it before copying the lightmap image, and removes the copied final sidecar if later lightmap copy fails.
- Installed topology metadata keeps `result.bakeResult.objectProbes`, replaces the path with `MakeSectorAssetRelativePath(finalObjectProbePath)`, and sets object probe source hash to `result.bakeResult.sourceHash`.
- Runtime/debug object probe loading and overlay visibility remain editor/preview/runtime owned; after successful install, the preview rebuild path must continue to refresh the rendered lightmap/probe runtime data as it does today.

## Phase 1

Goal: inventory-backed controller skeleton, no behavior moved.

Files to modify:

- Add `sources/sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.h`
- Add `sources/sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.cpp`
- Add these files to the relevant build source list only in the implementation task, not in this planning task.

Steps:

- Create a controller class with no dependency on `SectorEditor.h`.
- Move or mirror only passive request/result type declarations if needed; prefer leaving `SectorEditorLightmapAsyncTypes.h` untouched until behavior moves.
- Add compile-only methods returning inert state if needed for integration later.

Guardrails:

- No call sites changed unless required to compile the new files.
- No worker start, cancel, source hash, temp cleanup, or install behavior moved.

Verification:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`

Remains:

- All behavior still in `SectorEditor.cpp`.

## Phase 2

Goal: move worker lifecycle state and shutdown/cancel/join helpers behind the controller.

Files to modify:

- `SectorEditor.h`
- `SectorEditor.cpp`
- controller files
- `SectorEditorLightmapAsyncTypes.h` only if state types move with the controller
- `SectorEditorLightmapModal.*` only if the modal needs a controller-owned view type

Steps:

- Move `worker`, `progress`, `resultMutex`, `pendingResult`, temp path tracking, and cancel/join/shutdown helpers into the controller.
- Keep `StartLightmapBake()` request construction and `InstallLightmapBakeResult()` in `SectorEditor`.
- Make `SectorEditor::RequestLightmapBakeCancel()`, `JoinLightmapBakeWorker()`, and `ShutdownLightmapBake()` delegate to the controller or remove wrappers only when call sites are updated.
- Preserve modal state either in the controller or in a small view struct consumed by the modal.
- If `SectorEditorLightmapModal.*` is touched, replace direct bake-state mutation with read-only modal input plus explicit returned intents. The controller owns state transitions; the modal only draws and reports user intent.

Guardrails:

- Second start must still be refused while running, joinable, or modal-blocking.
- `Shutdown()` must request cancel, join if joinable, delete current temp lightmap, delete current temp object probe sidecar, delete pending-result temp outputs, reset running/cancel/phase state, and close/clear modal state.
- Worker must never capture `this` or live editor state.
- The modal must not mutate controller-owned worker/progress/result fields or terminal/cancel/acknowledgement state.

Verification:

- Full build/tests/checks.
- Add or update focused non-GUI tests only if the controller can be compiled into tests without dragging UI/renderer dependencies.

Remains:

- Start request construction, source-hash capture, result consumption, and install stay in `SectorEditor.cpp`.

## Phase 3

Goal: move `StartLightmapBake()` worker start mechanics and bake request execution.

Files to modify:

- `SectorEditor.cpp`
- `SectorEditor.h`
- controller files
- possibly `SectorEditorLightmapAsyncTypes.h`

Steps:

- Add `SectorEditorLightmapBakeRequest` containing copied `SectorTopologyMap`, expected source hash, final output path, temporary output path, and optional source revision.
- Keep editor-side preflight checks in `SectorEditor`: current level path, authoring-derived topology gate, no sectors, and `BuildLevelPaths()`.
- Either keep expected hash computation in `SectorEditor` and pass it in, or move it into the controller only if the controller receives a copied map and the call timing is identical.
- Move thread creation and `BakeSectorLightmap(input, callbacks, ...)` execution into the controller.

Guardrails:

- `ComputeSectorLightmapSourceHash()` must still be called before worker start against the same current topology snapshot.
- Temp lightmap and temp object probe sidecar must still be deleted before the worker starts.
- Progress phase, completed/total work, running, cancel, modal flags, terminal fields, and times must initialize to the same values.
- Worker captures only copied input plus controller-owned progress/result primitives.

Verification:

- Full build/tests/checks.
- Focused tests for refusing second start and consume-once pending result if practical.

Remains:

- `SectorEditor` still consumes results and installs metadata/files.

## Phase 4

Goal: move result polling/terminal lifecycle and stale-result classification while keeping editor-owned install.

Files to modify:

- `SectorEditor.cpp`
- `SectorEditor.h`
- controller files
- modal files if using a view object

Steps:

- Move pending-result extraction, join-after-result, completed-time recording, cancel/failure terminal state handling, and cancel/failure temp cleanup into controller methods.
- Add a result status object such as `SectorEditorLightmapBakePollResult` with states: none, cancelled, failed, completed.
- For successful results, expose the completed `SectorLightmapBakeAsyncResult` exactly once to `SectorEditor`.
- Optionally add `ClassifyCompletedResult(currentSourceHash)` for stale detection, but do not install topology metadata inside this phase.

Guardrails:

- Cancelled result still reports `Lightmap bake cancelled`, sets terminal cancelled/acknowledgement state, and deletes temp lightmap plus object probe path from the result.
- Failed result still reports the bake error or `Bake failed`, logs a warning, sets terminal failure/acknowledgement state, and deletes temp outputs.
- Successful result still transitions to installing before editor install begins.

Verification:

- Full build/tests/checks.
- Focused tests for cancel/failure consume-once behavior and safe repeated poll.

Remains:

- File install, metadata install, dirty state, report logging, and preview rebuild remain in `SectorEditor`.

## Phase 5

Goal: clarify and implement the result install boundary.

Risk callout: this is the highest-risk phase because it combines stale source-hash rejection, sidecar copy ordering, temp cleanup, baked metadata install, dirty flags, and preview refresh.

Files to modify:

- `SectorEditor.cpp`
- `SectorEditor.h`
- controller files
- tests as needed

Preferred boundary:

- Keep document/preview integration editor-owned.
- Extract a controller or helper method that validates and installs files into final paths and returns an install payload:
  - lightmap asset path or final filesystem path
  - object probe asset path or final filesystem path
  - copied `SectorLightmapBakeResult`
  - status/error text
- `SectorEditor` applies the payload to `state.topologyMap.bakedLightmap`, marks dirty, logs report, and rebuilds preview if needed.
- Preserve this payload/result boundary unless implementation proves direct controller install is cleaner without changing behavior.

Alternative boundary:

- Let the controller install directly into `SectorTopologyMap&` only if the API remains concrete and still does not include `SectorEditor.h`. Even then, `SectorEditor` must own dirty state, status text, asset manager interaction, and preview rebuild.

Guardrails:

- Stale rejection must compare current main-thread hash to `result.expectedSourceHash`.
- Missing temp lightmap or missing temp object probe sidecar must still fail with the current status text.
- Sidecar copy must happen before lightmap copy; if lightmap copy fails, final object probe sidecar must be deleted.
- Temp lightmap and temp sidecar must be deleted after success and every failure branch.
- Installed metadata must preserve width, height, `sourceHash`, object probe metadata, object probe asset-relative path, and object probe source hash exactly as today.
- `state.hasUnsavedChanges` and `state.topologyDocumentDirty` must still be set after successful install.
- Preview mesh rebuild remains conditional on `state.mode == Preview3D`, renderer ready, and `engineContext != nullptr`.

Verification:

- Full build/tests/checks.
- Focused tests for stale rejection, missing temp file cleanup, sidecar install failure cleanup, and successful metadata payload.

Remains:

- Thin wrappers and obsolete state cleanup.

## Phase 6

Goal: remove obsolete wrappers/state and update docs/backlog after implementation.

Files to modify:

- `SectorEditor.cpp`
- `SectorEditor.h`
- `SectorEditorLightmapAsyncTypes.h` if emptied or renamed
- `SectorEditorLightmapModal.*` if direct async state access was replaced
- controller files
- `docs/plans/codebase_refactor_backlog.md`
- optional implementation notes

Steps:

- Remove dead wrappers such as `JoinLightmapBakeWorker()` if no longer needed.
- Remove obsolete `LightmapBakeAsyncState lightmapBake` ownership from `SectorEditor` once fully controller-owned.
- Keep modal callback surface narrow; do not create a broad callback bundle back into `SectorEditor`.
- Update backlog: mark REF-062 complete only after implementation and verification, not during REF-077 planning.

Guardrails:

- Do not mix unrelated light inspector, object probe IO, or lightmap backend refactors into this phase.
- No source-hash policy change.
- No CMake/test-source churn beyond files required by this extraction.

Verification:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Manual smoke checklist below.

## Tests

Add focused non-GUI tests where practical. Prefer generated maps/temp paths and avoid user-edited levels under `assets/levels` or `assets/sector_demo`.

Suggested coverage:

- controller refuses a second start while a bake is running or pending/joinable.
- cancel request marks cancellation and repeated cancel/join/shutdown is safe.
- completed result can be consumed exactly once.
- cancelled and failed results delete temporary lightmap/object-probe sidecar paths.
- stale source hash result is rejected and temp outputs are deleted.
- successful result install/payload preserves width, height, source hash, lightmap asset path, object probe metadata, object probe source hash, and object probe asset-relative path.
- sidecar copy failure and lightmap copy failure clean up the same files as current code.
- source-hash tests continue to assert static lights, static spot lights, directional light, `ceilingSky`, and object probe spacing/height are sensitive; sky visual settings, preview settings, object probe debug distance, dynamic lights, and dynamic spot lights are neutral.

Existing useful tests include `tests/SectorTopologyLightmapTests.cpp`, `tests/SectorTopologySerializationTests.cpp`, `tests/SectorAuthoringGraphTests.cpp`, and `tests/SectorEditorUiLayoutTests.cpp`.

Do not add fragile GUI tests.

## Manual Smoke

After implementation, run the editor manually and verify:

- start a lightmap bake.
- progress updates while baking.
- cancel a bake and confirm the modal/status behavior remains correct.
- start a bake again after cancellation.
- complete a bake.
- lightmap appears in 3D preview.
- change a source-hash-sensitive light or setting during bake and verify the completed result is rejected as stale.
- verify object probes when enabled, including debug overlay/runtime status if currently supported.
- save and reload the baked result if currently supported.
- exit the editor during a bake and after a bake and verify no hang/crash.
- preview rendering, gameplay preview collision, sector lookup, and normal editor interaction still work.

## Backlog Updates

- REF-077 records this runner plan as planning complete only.
- REF-062 remains open as the implementation umbrella for the later extraction.
- REF-025 object probe sidecar IO remains open; do not fold it into the bake controller extraction unless a later task explicitly scopes it.
- REF-028 source-hash checkpoint remains open and should be referenced by implementation/test work.

## Open Questions / Stop Conditions

Stop and report instead of forcing the extraction if:

- moving a piece would require changing source-hash policy.
- the worker thread would need to reference live `SectorEditor` or `SectorEditorState`.
- object probe sidecar ownership or cleanup cannot be preserved exactly.
- result install cannot be separated from editor document/preview state without changing behavior.
- a phase cannot compile independently.
- tests expose unclear existing lifecycle behavior around cancel, failure, stale rejection, or temp cleanup.
- the only clean path would introduce a broad callback bundle or a service depending on `SectorEditor.h`.
