#include "sector_editor/services/lights/SectorEditorLightEditingService.h"

#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/preview/SectorEditorPreviewState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_demo/SectorUnits.h"

#include <cmath>
#include <iostream>
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
    lightState.spotLightPilot.active = true;
    lightState.spotLightPilot.kind = game::SpotLightPilotKind::Dynamic;
    lightState.spotLightPilot.lightId = 11;
    lightState.spotLightPilot.originalPosition = light.position;
    lightState.spotLightPilot.originalTarget = light.target;
    std::string statusText;
    ResetDirty(state, documentState, statusText);

    game::SectorEditorLightEditingService service =
            MakeService(state, documentState, documentState.map.topologyMap, TestPreviewSelectionState(), selectionState, manipulationState, lightState, uiState, inspectorIdUiState, statusText);
    const game::SectorEditorLightMutationResult apply =
            service.ApplySpotLightPilot(Vector3{7.0f, 8.0f, 9.0f}, Vector3{10.0f, 11.0f, 12.0f});
    const game::SectorTopologyDynamicSpotLight* applied =
            game::FindSectorTopologyDynamicSpotLight(documentState.map.topologyMap, 11);
    Check(apply.changed, "dynamic spotlight pilot apply reports changed");
    Check(apply.dynamicLightRendererRefreshNeeded, "dynamic spotlight pilot apply requests renderer refresh");
    Check(applied != nullptr
                  && Near(applied->position.x, 7.0f)
                  && Near(applied->target.z, 12.0f),
          "dynamic spotlight pilot apply mutates data");
    Check(!lightState.spotLightPilot.active, "dynamic spotlight pilot apply clears pilot state");
    CheckDirtyOnce(state, documentState, statusText, "Applied dynamic spot 11 pilot pose");

    ResetDirty(state, documentState, statusText);
    lightState.spotLightPilot.active = true;
    lightState.spotLightPilot.kind = game::SpotLightPilotKind::Dynamic;
    lightState.spotLightPilot.lightId = 11;
    lightState.spotLightPilot.originalPosition = Vector3{7.0f, 8.0f, 9.0f};
    lightState.spotLightPilot.originalTarget = Vector3{10.0f, 11.0f, 12.0f};
    service.ApplySpotLightPilot(Vector3{13.0f, 14.0f, 15.0f}, Vector3{16.0f, 17.0f, 18.0f});
    ResetDirty(state, documentState, statusText);
    lightState.spotLightPilot.active = true;
    lightState.spotLightPilot.kind = game::SpotLightPilotKind::Dynamic;
    lightState.spotLightPilot.lightId = 11;
    lightState.spotLightPilot.originalPosition = Vector3{7.0f, 8.0f, 9.0f};
    lightState.spotLightPilot.originalTarget = Vector3{10.0f, 11.0f, 12.0f};
    const game::SectorEditorLightMutationResult cancel =
            service.CancelSpotLightPilotData("Spotlight pilot cancelled");
    const game::SectorTopologyDynamicSpotLight* cancelled =
            game::FindSectorTopologyDynamicSpotLight(documentState.map.topologyMap, 11);
    Check(!cancel.changed, "dynamic spotlight pilot cancel reports unchanged");
    Check(!cancel.dynamicLightRendererRefreshNeeded, "dynamic spotlight pilot cancel preserves old no-refresh behavior");
    Check(cancel.previewPoseRestoreNeeded, "dynamic spotlight pilot cancel asks editor to restore preview pose");
    Check(cancelled != nullptr
                  && Near(cancelled->position.x, 7.0f)
                  && Near(cancelled->target.z, 12.0f),
          "dynamic spotlight pilot cancel restores original data");
    CheckClean(state, documentState, statusText, "Spotlight pilot cancelled");
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
    atmosphere.dust.enabled = true;
    atmosphere.dust.amount = 35;
    Check(service.SetStaticLightAtmosphere(
                  documentState.map.topologyMap.staticLights.front(), atmosphere),
          "atmosphere edit reports a change");
    Check(documentState.map.topologyMap.staticLights.front().atmosphere.haze.enabled
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
    TestAtmosphereEditUsesDocumentMutationBoundary();

    if (failures != 0) {
        std::cerr << failures << " SectorEditorLightEditingServiceTests failure(s)\n";
        return 1;
    }
    return 0;
}
