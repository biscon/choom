#include "sector_editor/services/lights/SectorEditorLightEditingService.h"

#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/preview/SectorEditorPreviewState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_demo/SectorUnits.h"

#include <raymath.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool Near(float lhs, float rhs, float epsilon = 0.0001f)
{
    return std::fabs(lhs - rhs) <= epsilon;
}

game::SectorEditorPreviewSelectionState& TestPreviewSelectionState()
{
    static game::SectorEditorPreviewSelectionState previewSelectionState;
    return previewSelectionState;
}

game::RuntimeObjectDragState& TestRuntimeObjectDragState()
{
    static game::RuntimeObjectDragState runtimeObjectDragState;
    return runtimeObjectDragState;
}

game::SectorTopologyMap MakeMap()
{
    game::SectorTopologyMap map;
    game::SectorTopologySector sector;
    sector.id = 1;
    sector.floorZ = 0.0f;
    sector.ceilingZ = game::SectorWorldToAuthoringDistance(3.0f);
    map.sectors.push_back(sector);
    return map;
}

game::SectorEditorLightEditingService MakeService(
        game::SectorEditorState& state,
        game::SectorEditorDocumentState& documentState,
        game::SectorTopologyMap& topologyMap,
        game::SectorEditorPreviewSelectionState& previewSelectionState,
        game::SelectionState& selectionState,
        game::ManipulationState& manipulationState,
        game::LightEditingState& lightState,
        game::SectorEditorUiState& uiState,
        game::InspectorIdUiState& inspectorIdUiState,
        std::string& statusText)
{
    TestRuntimeObjectDragState() = game::RuntimeObjectDragState{};
    return game::SectorEditorLightEditingService{
            game::SectorEditorLightEditingServiceContext{
                    topologyMap,
                    lightState,
                    game::MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle),
                    state.topologyRenderRevision,
                    state.topologyRenderCache,
                    {
                            manipulationState,
                            TestRuntimeObjectDragState(),
                            selectionState.topologySelectionKind,
                            selectionState.selectedTopologySectorId,
                            selectionState.selectedTopologyVertexId,
                            selectionState.selectedTopologySideDefId,
                            selectionState.selectedTopologyLineDefId,
                            selectionState.selectedTopologyLightId,
                            selectionState.selectedTopologyStaticSpotLightId,
                            selectionState.selectedTopologyDynamicLightId,
                            selectionState.selectedTopologyDynamicSpotLightId,
                            selectionState.selectedRuntimeObjectId,
                            selectionState.selectedTopologySideKind,
                            selectionState.inspectedTopologyVertexId,
                            previewSelectionState.selectedSurface3D,
                            previewSelectionState.selectedTopologySurface3D,
                            selectionState.selectedAuthoring,
                            selectionState.hoveredTopologyLightId,
                            selectionState.hoveredTopologyStaticSpotLightId,
                            selectionState.hoveredTopologyDynamicLightId,
                            selectionState.hoveredTopologyDynamicSpotLightId,
                    },
                    {
                            uiState.inspectorScroll,
                            uiState.lightXInput,
                            uiState.lightYInput,
                            uiState.lightZInput,
                            uiState.lightTargetXInput,
                            uiState.lightTargetYInput,
                            uiState.lightTargetZInput,
                            uiState.lightIntensityInput,
                            uiState.lightRadiusInput,
                            uiState.lightInnerConeInput,
                            uiState.lightOuterConeInput,
                            uiState.lightSourceRadiusInput,
                            uiState.lightFlickerSpeedInput,
                            uiState.lightFlickerAmountInput,
                            uiState.lightShadowPriorityInput,
                            uiState.lightShadowBiasInput,
                            uiState.lightShadowStrengthInput,
                            uiState.lightShadowSoftnessInput,
                            uiState.lightRedInput,
                            uiState.lightGreenInput,
                            uiState.lightBlueInput,
                            inspectorIdUiState,
                    },
                    statusText}};
}

void ResetDirty(game::SectorEditorState& state, game::SectorEditorDocumentState& documentState, std::string& statusText)
{
    documentState.lifecycle.topologyDocumentDirty = false;
    documentState.lifecycle.hasUnsavedChanges = false;
    state.topologyRenderCache.valid = true;
    state.topologyRenderRevision = 10;
    statusText = "old";
}

void CheckDirtyOnce(const game::SectorEditorState& state, const game::SectorEditorDocumentState& documentState, const std::string& statusText, const char* expectedStatus)
{
    Check(documentState.lifecycle.topologyDocumentDirty, "mutation marks topology document dirty");
    Check(documentState.lifecycle.hasUnsavedChanges, "mutation marks unsaved changes");
    Check(!state.topologyRenderCache.valid, "mutation invalidates topology render cache");
    Check(state.topologyRenderRevision == 11, "mutation increments topology render revision once");
    Check(statusText == expectedStatus, "mutation preserves expected status text");
}

void CheckClean(const game::SectorEditorState& state, const game::SectorEditorDocumentState& documentState, const std::string& statusText, const char* expectedStatus)
{
    Check(!documentState.lifecycle.topologyDocumentDirty, "no-op/cancel does not mark topology document dirty");
    Check(!documentState.lifecycle.hasUnsavedChanges, "no-op/cancel does not mark unsaved changes");
    Check(state.topologyRenderCache.valid, "no-op/cancel keeps topology render cache valid");
    Check(state.topologyRenderRevision == 10, "no-op/cancel does not increment topology render revision");
    Check(statusText == expectedStatus, "no-op/cancel preserves expected status text");
}

void TestAddStaticLightDirtiesAndSelects()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service =
            MakeService(state, documentState, documentState.map.topologyMap, TestPreviewSelectionState(), selectionState, manipulationState, lightState, uiState, inspectorIdUiState, statusText);
    const game::SectorEditorLightMutationResult result = service.AddStaticLight(1, Vector2{2.0f, 3.0f});

    Check(result.changed, "add static light reports changed");
    Check(documentState.map.topologyMap.staticLights.size() == 1, "add static light creates light");
    Check(selectionState.topologySelectionKind == game::TopologySelectionKind::StaticLight
                  && selectionState.selectedTopologyLightId == documentState.map.topologyMap.staticLights.front().id,
          "add static light selects new light");
    Check(Near(documentState.map.topologyMap.staticLights.front().position.x, 2.0f)
                  && Near(documentState.map.topologyMap.staticLights.front().position.z, 3.0f),
          "add static light initializes position");
    CheckDirtyOnce(state, documentState, statusText, "Added static light 1");
}

void TestAddDynamicLightDirtiesAndSelects()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service =
            MakeService(state, documentState, documentState.map.topologyMap, TestPreviewSelectionState(), selectionState, manipulationState, lightState, uiState, inspectorIdUiState, statusText);
    const game::SectorEditorLightMutationResult result = service.AddDynamicLight(1, Vector2{4.0f, 5.0f});

    Check(result.changed, "add dynamic light reports changed");
    Check(result.dynamicLightRendererRefreshNeeded,
          "add dynamic light requests preview lighting and atmosphere source refresh");
    Check(documentState.map.topologyMap.dynamicPointLights.size() == 1, "add dynamic light creates light");
    Check(documentState.map.topologyMap.dynamicPointLights.front().enabled, "add dynamic light preserves enabled default");
    Check(selectionState.topologySelectionKind == game::TopologySelectionKind::DynamicLight
                  && selectionState.selectedTopologyDynamicLightId == documentState.map.topologyMap.dynamicPointLights.front().id,
          "add dynamic light selects new light");
    CheckDirtyOnce(state, documentState, statusText, "Added dynamic light 1");
}

void TestAddNoOpDoesNotDirty()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service =
            MakeService(state, documentState, documentState.map.topologyMap, TestPreviewSelectionState(), selectionState, manipulationState, lightState, uiState, inspectorIdUiState, statusText);
    const game::SectorEditorLightMutationResult result = service.AddStaticLight(99, Vector2{2.0f, 3.0f});

    Check(!result.changed, "failed add reports unchanged");
    Check(documentState.map.topologyMap.staticLights.empty(), "failed add creates no light");
    CheckClean(state, documentState, statusText, "Static light placement failed: click inside a sector");
}

void TestDeleteSelectedStaticLightDirtiesAndClearsState()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    documentState.map.topologyMap.staticLights.push_back(game::SectorTopologyStaticPointLight{7});
    game::SelectionState selectionState;
    selectionState.topologySelectionKind = game::TopologySelectionKind::StaticLight;
    selectionState.selectedTopologyLightId = 7;
    selectionState.hoveredTopologyLightId = 7;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    lightState.lightDrag.active = true;
    lightState.lightDrag.topologyLightId = 7;
    lightState.lightEdit.active = true;
    lightState.lightEdit.topologyLightId = 7;
    lightState.lightPilot.active = true;
    lightState.lightPilot.kind = game::LightPilotKind::StaticPoint;
    lightState.lightPilot.lightId = 7;
    lightState.proxyPlacement.active = true;
    lightState.proxyPlacement.proxyKind = game::LightProxyPlacementKind::Halo;
    lightState.proxyPlacement.kind = game::LightPilotKind::StaticPoint;
    lightState.proxyPlacement.lightId = 7;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service =
            MakeService(state, documentState, documentState.map.topologyMap, TestPreviewSelectionState(), selectionState, manipulationState, lightState, uiState, inspectorIdUiState, statusText);
    const game::SectorEditorLightMutationResult result = service.DeleteSelectedLightConfirmed();

    Check(result.changed, "delete selected static light reports changed");
    Check(documentState.map.topologyMap.staticLights.empty(), "delete selected static light removes light");
    Check(selectionState.topologySelectionKind == game::TopologySelectionKind::None
                  && selectionState.selectedTopologyLightId < 0,
          "delete selected static light clears selection");
    Check(selectionState.hoveredTopologyLightId < 0, "delete selected static light clears hover");
    Check(!lightState.lightDrag.active && !lightState.lightEdit.active, "delete selected static light clears drag/edit state");
    Check(result.previewPoseRestoreNeeded && !lightState.lightPilot.active,
          "delete selected piloted point light requests preview-pose restoration");
    Check(!lightState.proxyPlacement.active,
          "delete selected light clears its proxy placement transaction");
    CheckDirtyOnce(state, documentState, statusText, "Deleted static light 7");
}

void TestDeleteSelectedDynamicLightDirties()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SectorTopologyDynamicPointLight light;
    light.id = 8;
    documentState.map.topologyMap.dynamicPointLights.push_back(light);
    game::SelectionState selectionState;
    selectionState.topologySelectionKind = game::TopologySelectionKind::DynamicLight;
    selectionState.selectedTopologyDynamicLightId = 8;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service =
            MakeService(state, documentState, documentState.map.topologyMap, TestPreviewSelectionState(), selectionState, manipulationState, lightState, uiState, inspectorIdUiState, statusText);
    const game::SectorEditorLightMutationResult result = service.DeleteSelectedLightConfirmed();

    Check(result.changed, "delete selected dynamic light reports changed");
    Check(result.dynamicLightRendererRefreshNeeded,
          "delete selected dynamic light requests preview lighting and atmosphere source refresh");
    Check(documentState.map.topologyMap.dynamicPointLights.empty(), "delete selected dynamic light removes light");
    CheckDirtyOnce(state, documentState, statusText, "Deleted dynamic light 8");
}

void TestLightDragApplyFinishAndCancelTiming()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SectorTopologyStaticSpotLight light;
    light.id = 9;
    light.position = Vector3{1.0f, 2.0f, 3.0f};
    light.target = Vector3{4.0f, 5.0f, 6.0f};
    documentState.map.topologyMap.staticSpotLights.push_back(light);
    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service =
            MakeService(state, documentState, documentState.map.topologyMap, TestPreviewSelectionState(), selectionState, manipulationState, lightState, uiState, inspectorIdUiState, statusText);
    Check(service.BeginLightDrag(game::TopologySelectionKind::StaticSpotLight, 9, game::SpotLightHandle::Origin),
          "begin static spot drag succeeds");
    service.ApplyLightDragToSnappedPosition(Vector3{3.0f, 0.0f, 8.0f});
    const game::SectorTopologyStaticSpotLight* dragged =
            game::FindSectorTopologyStaticSpotLight(documentState.map.topologyMap, 9);
    Check(dragged != nullptr
                  && Near(dragged->position.x, 3.0f)
                  && Near(dragged->position.y, 2.0f)
                  && Near(dragged->position.z, 8.0f)
                  && Near(dragged->target.x, 6.0f)
                  && Near(dragged->target.y, 5.0f)
                  && Near(dragged->target.z, 11.0f),
          "drag apply mutates live static spot data like old path");
    CheckClean(state, documentState, statusText, "Moving static spot 9");

    const game::SectorEditorLightMutationResult finish = service.FinishLightDrag();
    Check(finish.changed, "drag finish reports changed");
    CheckDirtyOnce(state, documentState, statusText, "Moved static spot 9");

    ResetDirty(state, documentState, statusText);
    Check(service.BeginLightDrag(game::TopologySelectionKind::StaticSpotLight, 9, game::SpotLightHandle::Origin),
          "begin second static spot drag succeeds");
    service.ApplyLightDragToSnappedPosition(Vector3{10.0f, 0.0f, 10.0f});
    service.CancelLightDragData("Cancelled light move");
    const game::SectorTopologyStaticSpotLight* cancelled =
            game::FindSectorTopologyStaticSpotLight(documentState.map.topologyMap, 9);
    Check(cancelled != nullptr
                  && Near(cancelled->position.x, 3.0f)
                  && Near(cancelled->position.z, 8.0f)
                  && Near(cancelled->target.x, 6.0f)
                  && Near(cancelled->target.z, 11.0f),
          "drag cancel restores original edit-state data");
    CheckClean(state, documentState, statusText, "Cancelled light move");
}

void TestLightDragFinishNoOpDoesNotDirty()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SectorTopologyStaticPointLight light;
    light.id = 10;
    light.position = Vector3{1.0f, 2.0f, 3.0f};
    documentState.map.topologyMap.staticLights.push_back(light);
    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service =
            MakeService(state, documentState, documentState.map.topologyMap, TestPreviewSelectionState(), selectionState, manipulationState, lightState, uiState, inspectorIdUiState, statusText);
    Check(service.BeginLightDrag(game::TopologySelectionKind::StaticLight, 10, game::SpotLightHandle::Origin),
          "begin static light drag succeeds");
    service.ApplyLightDragToSnappedPosition(Vector3{1.0f, 0.0f, 3.0f});
    const game::SectorEditorLightMutationResult result = service.FinishLightDrag();

    Check(!result.changed, "unchanged drag finish reports unchanged");
    CheckClean(state, documentState, statusText, "Static light unchanged");
}

void TestSpotLightPilotApplyAndCancelTiming()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SectorTopologyDynamicSpotLight light;
    light.id = 11;
    light.position = Vector3{1.0f, 2.0f, 3.0f};
    light.target = Vector3{4.0f, 5.0f, 6.0f};
    documentState.map.topologyMap.dynamicSpotLights.push_back(light);
    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    lightState.lightPilot.active = true;
    lightState.lightPilot.kind = game::LightPilotKind::DynamicSpot;
    lightState.lightPilot.lightId = 11;
    lightState.lightPilot.originalPosition = light.position;
    lightState.lightPilot.originalTarget = light.target;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service =
            MakeService(state, documentState, documentState.map.topologyMap, TestPreviewSelectionState(), selectionState, manipulationState, lightState, uiState, inspectorIdUiState, statusText);
    const game::SectorEditorLightMutationResult apply =
            service.ApplyLightPilot(Vector3{7.0f, 8.0f, 9.0f}, Vector3{10.0f, 11.0f, 12.0f});
    const game::SectorTopologyDynamicSpotLight* applied =
            game::FindSectorTopologyDynamicSpotLight(documentState.map.topologyMap, 11);
    Check(apply.changed, "dynamic spotlight pilot apply reports changed");
    Check(apply.dynamicLightRendererRefreshNeeded, "dynamic spotlight pilot apply requests renderer refresh");
    Check(applied != nullptr
                  && Near(applied->position.x, 7.0f)
                  && Near(applied->target.z, 12.0f),
          "dynamic spotlight pilot apply mutates data");
    Check(!lightState.lightPilot.active, "dynamic spotlight pilot apply clears pilot state");
    CheckDirtyOnce(state, documentState, statusText, "Applied dynamic spot 11 pilot pose");

    ResetDirty(state, documentState, statusText);
    lightState.lightPilot.active = true;
    lightState.lightPilot.kind = game::LightPilotKind::DynamicSpot;
    lightState.lightPilot.lightId = 11;
    lightState.lightPilot.originalPosition = Vector3{7.0f, 8.0f, 9.0f};
    lightState.lightPilot.originalTarget = Vector3{10.0f, 11.0f, 12.0f};
    service.ApplyLightPilot(Vector3{13.0f, 14.0f, 15.0f}, Vector3{16.0f, 17.0f, 18.0f});
    ResetDirty(state, documentState, statusText);
    lightState.lightPilot.active = true;
    lightState.lightPilot.kind = game::LightPilotKind::DynamicSpot;
    lightState.lightPilot.lightId = 11;
    lightState.lightPilot.originalPosition = Vector3{7.0f, 8.0f, 9.0f};
    lightState.lightPilot.originalTarget = Vector3{10.0f, 11.0f, 12.0f};
    const game::SectorEditorLightMutationResult cancel =
            service.CancelLightPilotData("Light pilot cancelled");
    const game::SectorTopologyDynamicSpotLight* cancelled =
            game::FindSectorTopologyDynamicSpotLight(documentState.map.topologyMap, 11);
    Check(!cancel.changed, "dynamic spotlight pilot cancel reports unchanged");
    Check(!cancel.dynamicLightRendererRefreshNeeded, "dynamic spotlight pilot cancel preserves old no-refresh behavior");
    Check(cancel.previewPoseRestoreNeeded, "dynamic spotlight pilot cancel asks editor to restore preview pose");
    Check(cancelled != nullptr
                  && Near(cancelled->position.x, 7.0f)
                  && Near(cancelled->target.z, 12.0f),
          "dynamic spotlight pilot cancel restores original data");
    CheckClean(state, documentState, statusText, "Light pilot cancelled");
}

void TestPointLightPilotApplyAndCancelTiming()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SectorTopologyStaticPointLight staticLight;
    staticLight.id = 21;
    staticLight.position = Vector3{1.0f, 2.0f, 3.0f};
    documentState.map.topologyMap.staticLights.push_back(staticLight);
    game::SectorTopologyDynamicPointLight dynamicLight;
    dynamicLight.id = 22;
    dynamicLight.position = Vector3{4.0f, 5.0f, 6.0f};
    documentState.map.topologyMap.dynamicPointLights.push_back(dynamicLight);
    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service =
            MakeService(state, documentState, documentState.map.topologyMap, TestPreviewSelectionState(), selectionState, manipulationState, lightState, uiState, inspectorIdUiState, statusText);
    lightState.lightPilot.active = true;
    lightState.lightPilot.kind = game::LightPilotKind::StaticPoint;
    lightState.lightPilot.lightId = 21;
    lightState.lightPilot.originalPosition = staticLight.position;
    const game::SectorEditorLightMutationResult staticApply =
            service.ApplyLightPilot(Vector3{7.0f, 8.0f, 9.0f}, Vector3{90.0f, 91.0f, 92.0f});
    const game::SectorTopologyStaticPointLight* appliedStatic =
            game::FindSectorTopologyStaticLight(documentState.map.topologyMap, 21);
    Check(staticApply.changed && staticApply.dynamicLightRendererRefreshNeeded,
          "static point pilot apply reports change and requests renderer refresh");
    Check(appliedStatic != nullptr && Near(appliedStatic->position.x, 7.0f)
                  && Near(appliedStatic->position.y, 8.0f)
                  && Near(appliedStatic->position.z, 9.0f),
          "static point pilot apply writes only the supplied position");
    CheckDirtyOnce(state, documentState, statusText, "Applied static light 21 pilot pose");

    ResetDirty(state, documentState, statusText);
    lightState.lightPilot.active = true;
    lightState.lightPilot.kind = game::LightPilotKind::DynamicPoint;
    lightState.lightPilot.lightId = 22;
    lightState.lightPilot.originalPosition = dynamicLight.position;
    const game::SectorEditorLightMutationResult dynamicApply =
            service.ApplyLightPilot(Vector3{10.0f, 11.0f, 12.0f}, Vector3{93.0f, 94.0f, 95.0f});
    const game::SectorTopologyDynamicPointLight* appliedDynamic =
            game::FindSectorTopologyDynamicLight(documentState.map.topologyMap, 22);
    Check(dynamicApply.changed && dynamicApply.dynamicLightRendererRefreshNeeded,
          "dynamic point pilot apply reports change and requests renderer refresh");
    Check(appliedDynamic != nullptr && Near(appliedDynamic->position.x, 10.0f)
                  && Near(appliedDynamic->position.y, 11.0f)
                  && Near(appliedDynamic->position.z, 12.0f),
          "dynamic point pilot apply writes the camera position");
    CheckDirtyOnce(state, documentState, statusText, "Applied dynamic light 22 pilot pose");

    ResetDirty(state, documentState, statusText);
    lightState.lightPilot.active = true;
    lightState.lightPilot.kind = game::LightPilotKind::DynamicPoint;
    lightState.lightPilot.lightId = 22;
    lightState.lightPilot.originalPosition = dynamicLight.position;
    const game::SectorEditorLightMutationResult cancel =
            service.CancelLightPilotData("Light pilot cancelled");
    const game::SectorTopologyDynamicPointLight* cancelled =
            game::FindSectorTopologyDynamicLight(documentState.map.topologyMap, 22);
    Check(!cancel.changed && cancel.previewPoseRestoreNeeded,
          "point pilot cancel remains clean and requests preview-pose restoration");
    Check(cancelled != nullptr && Near(cancelled->position.x, 4.0f)
                  && Near(cancelled->position.y, 5.0f)
                  && Near(cancelled->position.z, 6.0f),
          "point pilot cancel restores the original light position");
    CheckClean(state, documentState, statusText, "Light pilot cancelled");
}

void TestPointSpotLightsDown()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();

    game::SectorTopologyStaticSpotLight staticSpot;
    staticSpot.id = 23;
    staticSpot.position = Vector3{10.0f, 20.0f, 30.0f};
    staticSpot.target = Vector3{13.0f, 24.0f, 30.0f};
    staticSpot.color = Color{10, 20, 30, 255};
    staticSpot.intensity = 2.5f;
    staticSpot.range = 48.0f;
    staticSpot.innerConeDegrees = 18.0f;
    staticSpot.outerConeDegrees = 42.0f;
    staticSpot.sourceRadius = 3.0f;
    staticSpot.castsShadow = false;
    staticSpot.atmosphere.haze.enabled = true;
    staticSpot.atmosphere.haze.density = 0.35f;
    documentState.map.topologyMap.staticSpotLights.push_back(staticSpot);

    game::SectorTopologyDynamicSpotLight dynamicSpot;
    dynamicSpot.id = 24;
    dynamicSpot.position = Vector3{-2.0f, 8.0f, 5.0f};
    dynamicSpot.target = dynamicSpot.position;
    dynamicSpot.color = Color{40, 50, 60, 255};
    dynamicSpot.intensity = 1.75f;
    dynamicSpot.range = 64.0f;
    dynamicSpot.innerConeDegrees = 12.0f;
    dynamicSpot.outerConeDegrees = 28.0f;
    dynamicSpot.enabled = false;
    dynamicSpot.flicker = true;
    dynamicSpot.flickerSpeed = 3.0f;
    dynamicSpot.flickerAmount = 0.4f;
    dynamicSpot.castsShadow = true;
    dynamicSpot.shadowPriority = 7;
    dynamicSpot.shadowBias = 0.002f;
    dynamicSpot.shadowStrength = 0.8f;
    dynamicSpot.shadowSoftness = 0.6f;
    dynamicSpot.atmosphere.dust.enabled = true;
    dynamicSpot.atmosphere.dust.amount = 12;
    documentState.map.topologyMap.dynamicSpotLights.push_back(dynamicSpot);

    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service = MakeService(
            state,
            documentState,
            documentState.map.topologyMap,
            TestPreviewSelectionState(),
            selectionState,
            manipulationState,
            lightState,
            uiState,
            inspectorIdUiState,
            statusText);

    game::SectorTopologyStaticSpotLight& editedStatic =
            documentState.map.topologyMap.staticSpotLights.front();
    const game::SectorTopologyStaticSpotLight staticBefore = editedStatic;
    Check(service.PointStaticSpotLightDown(editedStatic),
          "point-down changes a non-vertical static spotlight");
    Check(Near(editedStatic.target.x, editedStatic.position.x)
                  && Near(editedStatic.target.y, 15.0f)
                  && Near(editedStatic.target.z, editedStatic.position.z)
                  && Near(Vector3Distance(editedStatic.position, editedStatic.target), 5.0f),
          "static spotlight points along world negative Y while preserving target distance");
    Check(Near(editedStatic.position.x, staticBefore.position.x)
                  && Near(editedStatic.position.y, staticBefore.position.y)
                  && Near(editedStatic.position.z, staticBefore.position.z)
                  && editedStatic.color.r == staticBefore.color.r
                  && editedStatic.color.g == staticBefore.color.g
                  && editedStatic.color.b == staticBefore.color.b
                  && Near(editedStatic.intensity, staticBefore.intensity)
                  && Near(editedStatic.range, staticBefore.range)
                  && Near(editedStatic.innerConeDegrees, staticBefore.innerConeDegrees)
                  && Near(editedStatic.outerConeDegrees, staticBefore.outerConeDegrees)
                  && Near(editedStatic.sourceRadius, staticBefore.sourceRadius)
                  && editedStatic.castsShadow == staticBefore.castsShadow
                  && editedStatic.atmosphere.haze.enabled == staticBefore.atmosphere.haze.enabled
                  && Near(editedStatic.atmosphere.haze.density, staticBefore.atmosphere.haze.density),
          "static point-down preserves all non-target spotlight settings");
    CheckDirtyOnce(state, documentState, statusText, "Pointed static spot 23 down");

    ResetDirty(state, documentState, statusText);
    Check(!service.PointStaticSpotLightDown(editedStatic),
          "point-down is a no-op for an already downward static spotlight");
    CheckClean(state, documentState, statusText, "old");

    ResetDirty(state, documentState, statusText);
    game::SectorTopologyDynamicSpotLight& editedDynamic =
            documentState.map.topologyMap.dynamicSpotLights.front();
    const game::SectorTopologyDynamicSpotLight dynamicBefore = editedDynamic;
    Check(service.PointDynamicSpotLightDown(editedDynamic),
          "point-down repairs a coincident dynamic spotlight target");
    const float fallbackDistance = game::SectorWorldToAuthoringDistance(1.0f);
    Check(Near(editedDynamic.target.x, editedDynamic.position.x)
                  && Near(editedDynamic.target.y, editedDynamic.position.y - fallbackDistance)
                  && Near(editedDynamic.target.z, editedDynamic.position.z)
                  && Near(Vector3Distance(editedDynamic.position, editedDynamic.target), fallbackDistance),
          "coincident dynamic spotlight target uses the one-world-meter fallback");
    Check(Near(editedDynamic.position.x, dynamicBefore.position.x)
                  && Near(editedDynamic.position.y, dynamicBefore.position.y)
                  && Near(editedDynamic.position.z, dynamicBefore.position.z)
                  && editedDynamic.color.r == dynamicBefore.color.r
                  && editedDynamic.color.g == dynamicBefore.color.g
                  && editedDynamic.color.b == dynamicBefore.color.b
                  && Near(editedDynamic.intensity, dynamicBefore.intensity)
                  && Near(editedDynamic.range, dynamicBefore.range)
                  && Near(editedDynamic.innerConeDegrees, dynamicBefore.innerConeDegrees)
                  && Near(editedDynamic.outerConeDegrees, dynamicBefore.outerConeDegrees)
                  && editedDynamic.enabled == dynamicBefore.enabled
                  && editedDynamic.flicker == dynamicBefore.flicker
                  && Near(editedDynamic.flickerSpeed, dynamicBefore.flickerSpeed)
                  && Near(editedDynamic.flickerAmount, dynamicBefore.flickerAmount)
                  && editedDynamic.castsShadow == dynamicBefore.castsShadow
                  && editedDynamic.shadowPriority == dynamicBefore.shadowPriority
                  && Near(editedDynamic.shadowBias, dynamicBefore.shadowBias)
                  && Near(editedDynamic.shadowStrength, dynamicBefore.shadowStrength)
                  && Near(editedDynamic.shadowSoftness, dynamicBefore.shadowSoftness)
                  && editedDynamic.atmosphere.dust.enabled == dynamicBefore.atmosphere.dust.enabled
                  && editedDynamic.atmosphere.dust.amount == dynamicBefore.atmosphere.dust.amount,
          "dynamic point-down preserves all non-target spotlight settings");
    CheckDirtyOnce(state, documentState, statusText, "Pointed dynamic spot 24 down");

    editedStatic.target.x = std::numeric_limits<float>::infinity();
    ResetDirty(state, documentState, statusText);
    Check(service.PointStaticSpotLightDown(editedStatic)
                  && Near(Vector3Distance(editedStatic.position, editedStatic.target), fallbackDistance),
          "point-down replaces a non-finite target with the one-world-meter fallback");
    CheckDirtyOnce(state, documentState, statusText, "Pointed static spot 23 down");
}

void TestStaticShadowEditsUseDocumentMutationBoundary()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SectorTopologyStaticPointLight point;
    point.id = 31;
    documentState.map.topologyMap.staticLights.push_back(point);
    game::SectorTopologyStaticSpotLight spot;
    spot.id = 32;
    documentState.map.topologyMap.staticSpotLights.push_back(spot);
    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service =
            MakeService(state, documentState, documentState.map.topologyMap, TestPreviewSelectionState(), selectionState, manipulationState, lightState, uiState, inspectorIdUiState, statusText);
    Check(service.SetStaticLightCastsShadow(
                  documentState.map.topologyMap.staticLights.front(), false),
          "static point shadow edit reports changed");
    Check(!documentState.map.topologyMap.staticLights.front().castsShadow,
          "static point shadow edit writes the flag");
    CheckDirtyOnce(state, documentState, statusText, "Updated static light 31 shadow");

    ResetDirty(state, documentState, statusText);
    Check(!service.SetStaticLightCastsShadow(
                  documentState.map.topologyMap.staticLights.front(), false),
          "unchanged static point shadow edit reports no change");
    CheckClean(state, documentState, statusText, "old");

    ResetDirty(state, documentState, statusText);
    Check(service.SetStaticSpotLightCastsShadow(
                  documentState.map.topologyMap.staticSpotLights.front(), false),
          "static spot shadow edit reports changed");
    Check(!documentState.map.topologyMap.staticSpotLights.front().castsShadow,
          "static spot shadow edit writes the flag");
    CheckDirtyOnce(state, documentState, statusText, "Updated static spot 32 shadow");
}

void TestAtmosphereEditUsesDocumentMutationBoundary()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SectorTopologyStaticPointLight light;
    light.id = 12;
    documentState.map.topologyMap.staticLights.push_back(light);
    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service = MakeService(
            state,
            documentState,
            documentState.map.topologyMap,
            TestPreviewSelectionState(),
            selectionState,
            manipulationState,
            lightState,
            uiState,
            inspectorIdUiState,
            statusText);
    game::SectorLightAtmosphereSettings atmosphere;
    atmosphere.haze.enabled = true;
    atmosphere.haze.heightOffsetWorld = 0.75f;
    atmosphere.proxy.halo.enabled = true;
    atmosphere.proxy.halo.brightness = 0.4f;
    atmosphere.proxy.halo.maxExtinction = 0.6f;
    atmosphere.proxy.halo.scatteringTint = Color{180, 210, 240, 255};
    atmosphere.dust.enabled = true;
    atmosphere.dust.amount = 35;
    Check(service.SetStaticLightAtmosphere(
                  documentState.map.topologyMap.staticLights.front(), atmosphere),
          "atmosphere edit reports a change");
    Check(documentState.map.topologyMap.staticLights.front().atmosphere.haze.enabled
                  && Near(documentState.map.topologyMap.staticLights.front().atmosphere.haze.heightOffsetWorld, 0.75f)
                  && documentState.map.topologyMap.staticLights.front().atmosphere.proxy.halo.enabled
                  && Near(documentState.map.topologyMap.staticLights.front().atmosphere.proxy.halo.brightness, 0.4f)
                  && Near(documentState.map.topologyMap.staticLights.front().atmosphere.proxy.halo.maxExtinction, 0.6f)
                  && documentState.map.topologyMap.staticLights.front().atmosphere.proxy.halo.scatteringTint.g == 210
                  && documentState.map.topologyMap.staticLights.front().atmosphere.dust.enabled
                  && documentState.map.topologyMap.staticLights.front().atmosphere.dust.amount == 35,
          "atmosphere edit writes normalized light data");
    CheckDirtyOnce(state, documentState, statusText, "Updated static light 12 atmosphere");

    ResetDirty(state, documentState, statusText);
    Check(!service.SetStaticLightAtmosphere(
                  documentState.map.topologyMap.staticLights.front(), atmosphere),
          "unchanged atmosphere edit reports no change");
    CheckClean(state, documentState, statusText, "old");
}

void TestProxyPlacementApplyAndCancelTiming()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SectorTopologyStaticPointLight light;
    light.id = 12;
    light.atmosphere.proxy.halo.enabled = true;
    documentState.map.topologyMap.staticLights.push_back(light);
    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service = MakeService(
            state,
            documentState,
            documentState.map.topologyMap,
            TestPreviewSelectionState(),
            selectionState,
            manipulationState,
            lightState,
            uiState,
            inspectorIdUiState,
            statusText);
    Check(service.BeginProxyPlacement(
                  game::LightProxyPlacementKind::Halo,
                  game::LightPilotKind::StaticPoint,
                  12),
          "enabled analytic halo begins placement");
    const game::SectorEditorLightMutationResult preview =
            service.PreviewProxyPlacement(Vector3{1.0f, -2.0f, 3.0f});
    Check(preview.changed && preview.dynamicLightRendererRefreshNeeded,
          "halo placement preview requests only a renderer source refresh");
    Check(Near(documentState.map.topologyMap.staticLights.front()
                           .atmosphere.proxy.halo.centerOffsetWorld.x,
                       1.0f),
          "halo placement preview stages the offset on the selected light");
    CheckClean(state, documentState, statusText, "Placing static light 12 halo");

    const game::SectorEditorLightMutationResult applied = service.ApplyProxyPlacement();
    Check(applied.changed && applied.dynamicLightRendererRefreshNeeded
                  && !lightState.proxyPlacement.active,
          "applying halo placement finishes the transaction");
    CheckDirtyOnce(state, documentState, statusText, "Placed static light 12 halo");

    ResetDirty(state, documentState, statusText);
    Check(service.BeginProxyPlacement(
                  game::LightProxyPlacementKind::Halo,
                  game::LightPilotKind::StaticPoint,
                  12),
          "placed halo can begin another placement transaction");
    service.PreviewProxyPlacement(Vector3{4.0f, 5.0f, 6.0f});
    const game::SectorEditorLightMutationResult cancelled =
            service.CancelProxyPlacementData("Halo placement cancelled");
    const Vector3 restored = documentState.map.topologyMap.staticLights.front()
                                     .atmosphere.proxy.halo.centerOffsetWorld;
    Check(cancelled.changed && cancelled.dynamicLightRendererRefreshNeeded
                  && !lightState.proxyPlacement.active,
          "cancelling halo placement finishes the transaction and refreshes preview");
    Check(Near(restored.x, 1.0f) && Near(restored.y, -2.0f) && Near(restored.z, 3.0f),
          "cancelling halo placement restores the original offset");
    CheckClean(state, documentState, statusText, "Halo placement cancelled");

    documentState.map.topologyMap.staticLights.front().atmosphere.proxy.halo.enabled = false;
    Check(!service.BeginProxyPlacement(
                  game::LightProxyPlacementKind::Halo,
                  game::LightPilotKind::StaticPoint,
                  12),
          "disabled halo cannot begin placement");
    Check(!service.BeginProxyPlacement(
                  game::LightProxyPlacementKind::Halo,
                  game::LightPilotKind::StaticPoint,
                  999),
          "missing light cannot begin halo placement");

    game::SectorTopologyStaticSpotLight spot;
    spot.id = 13;
    spot.atmosphere.proxy.shaft.enabled = true;
    documentState.map.topologyMap.staticSpotLights.push_back(spot);
    ResetDirty(state, documentState, statusText);
    Check(service.BeginProxyPlacement(
                  game::LightProxyPlacementKind::Shaft,
                  game::LightPilotKind::StaticSpot,
                  13),
          "enabled analytic shaft begins placement for a spotlight");
    const game::SectorEditorLightMutationResult shaftPreview =
            service.PreviewProxyPlacement(Vector3{0.25f, 0.75f, -0.5f});
    const Vector3 stagedShaftOffset = documentState.map.topologyMap.staticSpotLights.front()
                                                .atmosphere.proxy.shaft.originOffsetWorld;
    Check(shaftPreview.changed && shaftPreview.dynamicLightRendererRefreshNeeded
                  && Near(stagedShaftOffset.x, 0.25f)
                  && Near(stagedShaftOffset.y, 0.75f)
                  && Near(stagedShaftOffset.z, -0.5f),
          "shaft placement preview stages its independent origin offset");
    CheckClean(state, documentState, statusText, "Placing static spot 13 shaft");
    const game::SectorEditorLightMutationResult shaftApplied = service.ApplyProxyPlacement();
    Check(shaftApplied.changed && shaftApplied.dynamicLightRendererRefreshNeeded
                  && !lightState.proxyPlacement.active,
          "applying shaft placement finishes the transaction");
    CheckDirtyOnce(state, documentState, statusText, "Placed static spot 13 shaft");

    ResetDirty(state, documentState, statusText);
    Check(service.BeginProxyPlacement(
                  game::LightProxyPlacementKind::Shaft,
                  game::LightPilotKind::StaticSpot,
                  13),
          "placed shaft can begin another placement transaction");
    service.PreviewProxyPlacement(Vector3{5.0f, 6.0f, 7.0f});
    const game::SectorEditorLightMutationResult shaftCancelled =
            service.CancelProxyPlacementData("Shaft placement cancelled");
    const Vector3 restoredShaftOffset = documentState.map.topologyMap.staticSpotLights.front()
                                                 .atmosphere.proxy.shaft.originOffsetWorld;
    Check(shaftCancelled.changed && shaftCancelled.dynamicLightRendererRefreshNeeded
                  && Near(restoredShaftOffset.x, 0.25f)
                  && Near(restoredShaftOffset.y, 0.75f)
                  && Near(restoredShaftOffset.z, -0.5f),
          "cancelling shaft placement restores its original offset");
    CheckClean(state, documentState, statusText, "Shaft placement cancelled");

    Check(!service.BeginProxyPlacement(
                  game::LightProxyPlacementKind::Shaft,
                  game::LightPilotKind::StaticPoint,
                  12),
          "point lights cannot begin shaft placement");
    documentState.map.topologyMap.staticSpotLights.front().atmosphere.proxy.shaft.enabled = false;
    Check(!service.BeginProxyPlacement(
                  game::LightProxyPlacementKind::Shaft,
                  game::LightPilotKind::StaticSpot,
                  13),
          "disabled shaft cannot begin placement");

    game::SectorTopologyDynamicSpotLight dynamicSpot;
    dynamicSpot.id = 14;
    dynamicSpot.atmosphere.proxy.shaft.enabled = true;
    documentState.map.topologyMap.dynamicSpotLights.push_back(dynamicSpot);
    Check(service.BeginProxyPlacement(
                  game::LightProxyPlacementKind::Shaft,
                  game::LightPilotKind::DynamicSpot,
                  14),
          "enabled dynamic spotlight shaft begins placement");
    service.CancelProxyPlacementData(nullptr);
}

} // namespace

int main()
{
    TestAddStaticLightDirtiesAndSelects();
    TestAddDynamicLightDirtiesAndSelects();
    TestAddNoOpDoesNotDirty();
    TestDeleteSelectedStaticLightDirtiesAndClearsState();
    TestDeleteSelectedDynamicLightDirties();
    TestLightDragApplyFinishAndCancelTiming();
    TestLightDragFinishNoOpDoesNotDirty();
    TestSpotLightPilotApplyAndCancelTiming();
    TestPointLightPilotApplyAndCancelTiming();
    TestPointSpotLightsDown();
    TestStaticShadowEditsUseDocumentMutationBoundary();
    TestAtmosphereEditUsesDocumentMutationBoundary();
    TestProxyPlacementApplyAndCancelTiming();

    if (failures != 0) {
        std::cerr << failures << " SectorEditorLightEditingServiceTests failure(s)\n";
        return 1;
    }
    return 0;
}
