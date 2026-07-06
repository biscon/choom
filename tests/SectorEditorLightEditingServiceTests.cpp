#include "sector_editor/services/lights/SectorEditorLightEditingService.h"

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
        game::SectorEditorUiState& uiState,
        std::string& statusText)
{
    return game::SectorEditorLightEditingService{
            game::SectorEditorLightEditingServiceContext{
                    state,
                    uiState,
                    statusText}};
}

void ResetDirty(game::SectorEditorState& state, std::string& statusText)
{
    state.topologyDocumentDirty = false;
    state.hasUnsavedChanges = false;
    state.topologyRenderCache.valid = true;
    state.topologyRenderRevision = 10;
    statusText = "old";
}

void CheckDirtyOnce(const game::SectorEditorState& state, const std::string& statusText, const char* expectedStatus)
{
    Check(state.topologyDocumentDirty, "mutation marks topology document dirty");
    Check(state.hasUnsavedChanges, "mutation marks unsaved changes");
    Check(!state.topologyRenderCache.valid, "mutation invalidates topology render cache");
    Check(state.topologyRenderRevision == 11, "mutation increments topology render revision once");
    Check(statusText == expectedStatus, "mutation preserves expected status text");
}

void CheckClean(const game::SectorEditorState& state, const std::string& statusText, const char* expectedStatus)
{
    Check(!state.topologyDocumentDirty, "no-op/cancel does not mark topology document dirty");
    Check(!state.hasUnsavedChanges, "no-op/cancel does not mark unsaved changes");
    Check(state.topologyRenderCache.valid, "no-op/cancel keeps topology render cache valid");
    Check(state.topologyRenderRevision == 10, "no-op/cancel does not increment topology render revision");
    Check(statusText == expectedStatus, "no-op/cancel preserves expected status text");
}

void TestAddStaticLightDirtiesAndSelects()
{
    game::SectorEditorState state;
    state.topologyMap = MakeMap();
    game::SectorEditorUiState uiState;
    std::string statusText;
    ResetDirty(state, statusText);

    game::SectorEditorLightEditingService service = MakeService(state, uiState, statusText);
    const game::SectorEditorLightMutationResult result = service.AddStaticLight(1, Vector2{2.0f, 3.0f});

    Check(result.changed, "add static light reports changed");
    Check(state.topologyMap.staticLights.size() == 1, "add static light creates light");
    Check(state.topologySelectionKind == game::TopologySelectionKind::StaticLight
                  && state.selectedTopologyLightId == state.topologyMap.staticLights.front().id,
          "add static light selects new light");
    Check(Near(state.topologyMap.staticLights.front().position.x, 2.0f)
                  && Near(state.topologyMap.staticLights.front().position.z, 3.0f),
          "add static light initializes position");
    CheckDirtyOnce(state, statusText, "Added static light 1");
}

void TestAddDynamicLightDirtiesAndSelects()
{
    game::SectorEditorState state;
    state.topologyMap = MakeMap();
    game::SectorEditorUiState uiState;
    std::string statusText;
    ResetDirty(state, statusText);

    game::SectorEditorLightEditingService service = MakeService(state, uiState, statusText);
    const game::SectorEditorLightMutationResult result = service.AddDynamicLight(1, Vector2{4.0f, 5.0f});

    Check(result.changed, "add dynamic light reports changed");
    Check(!result.dynamicLightRendererRefreshNeeded, "add dynamic light does not request renderer refresh");
    Check(state.topologyMap.dynamicPointLights.size() == 1, "add dynamic light creates light");
    Check(state.topologyMap.dynamicPointLights.front().enabled, "add dynamic light preserves enabled default");
    Check(state.topologySelectionKind == game::TopologySelectionKind::DynamicLight
                  && state.selectedTopologyDynamicLightId == state.topologyMap.dynamicPointLights.front().id,
          "add dynamic light selects new light");
    CheckDirtyOnce(state, statusText, "Added dynamic light 1");
}

void TestAddNoOpDoesNotDirty()
{
    game::SectorEditorState state;
    state.topologyMap = MakeMap();
    game::SectorEditorUiState uiState;
    std::string statusText;
    ResetDirty(state, statusText);

    game::SectorEditorLightEditingService service = MakeService(state, uiState, statusText);
    const game::SectorEditorLightMutationResult result = service.AddStaticLight(99, Vector2{2.0f, 3.0f});

    Check(!result.changed, "failed add reports unchanged");
    Check(state.topologyMap.staticLights.empty(), "failed add creates no light");
    CheckClean(state, statusText, "Static light placement failed: click inside a sector");
}

void TestDeleteSelectedStaticLightDirtiesAndClearsState()
{
    game::SectorEditorState state;
    state.topologyMap = MakeMap();
    state.topologyMap.staticLights.push_back(game::SectorTopologyStaticPointLight{7});
    state.topologySelectionKind = game::TopologySelectionKind::StaticLight;
    state.selectedTopologyLightId = 7;
    state.hoveredTopologyLightId = 7;
    state.lightDrag.active = true;
    state.lightDrag.topologyLightId = 7;
    state.lightEditing.active = true;
    state.lightEditing.topologyLightId = 7;
    game::SectorEditorUiState uiState;
    std::string statusText;
    ResetDirty(state, statusText);

    game::SectorEditorLightEditingService service = MakeService(state, uiState, statusText);
    const game::SectorEditorLightMutationResult result = service.DeleteSelectedLightConfirmed();

    Check(result.changed, "delete selected static light reports changed");
    Check(state.topologyMap.staticLights.empty(), "delete selected static light removes light");
    Check(state.topologySelectionKind == game::TopologySelectionKind::None
                  && state.selectedTopologyLightId < 0,
          "delete selected static light clears selection");
    Check(state.hoveredTopologyLightId < 0, "delete selected static light clears hover");
    Check(!state.lightDrag.active && !state.lightEditing.active, "delete selected static light clears drag/edit state");
    CheckDirtyOnce(state, statusText, "Deleted static light 7");
}

void TestDeleteSelectedDynamicLightDirties()
{
    game::SectorEditorState state;
    state.topologyMap = MakeMap();
    game::SectorTopologyDynamicPointLight light;
    light.id = 8;
    state.topologyMap.dynamicPointLights.push_back(light);
    state.topologySelectionKind = game::TopologySelectionKind::DynamicLight;
    state.selectedTopologyDynamicLightId = 8;
    game::SectorEditorUiState uiState;
    std::string statusText;
    ResetDirty(state, statusText);

    game::SectorEditorLightEditingService service = MakeService(state, uiState, statusText);
    const game::SectorEditorLightMutationResult result = service.DeleteSelectedLightConfirmed();

    Check(result.changed, "delete selected dynamic light reports changed");
    Check(!result.dynamicLightRendererRefreshNeeded, "delete selected dynamic light preserves old no-refresh behavior");
    Check(state.topologyMap.dynamicPointLights.empty(), "delete selected dynamic light removes light");
    CheckDirtyOnce(state, statusText, "Deleted dynamic light 8");
}

void TestLightDragApplyFinishAndCancelTiming()
{
    game::SectorEditorState state;
    state.topologyMap = MakeMap();
    game::SectorTopologyStaticSpotLight light;
    light.id = 9;
    light.position = Vector3{1.0f, 2.0f, 3.0f};
    light.target = Vector3{4.0f, 5.0f, 6.0f};
    state.topologyMap.staticSpotLights.push_back(light);
    game::SectorEditorUiState uiState;
    std::string statusText;
    ResetDirty(state, statusText);

    game::SectorEditorLightEditingService service = MakeService(state, uiState, statusText);
    Check(service.BeginLightDrag(game::TopologySelectionKind::StaticSpotLight, 9, game::SpotLightHandle::Origin),
          "begin static spot drag succeeds");
    service.ApplyLightDragToSnappedPosition(Vector3{3.0f, 0.0f, 8.0f});
    const game::SectorTopologyStaticSpotLight* dragged =
            game::FindSectorTopologyStaticSpotLight(state.topologyMap, 9);
    Check(dragged != nullptr
                  && Near(dragged->position.x, 3.0f)
                  && Near(dragged->position.y, 2.0f)
                  && Near(dragged->position.z, 8.0f)
                  && Near(dragged->target.x, 6.0f)
                  && Near(dragged->target.y, 5.0f)
                  && Near(dragged->target.z, 11.0f),
          "drag apply mutates live static spot data like old path");
    CheckClean(state, statusText, "Moving static spot 9");

    const game::SectorEditorLightMutationResult finish = service.FinishLightDrag();
    Check(finish.changed, "drag finish reports changed");
    CheckDirtyOnce(state, statusText, "Moved static spot 9");

    ResetDirty(state, statusText);
    Check(service.BeginLightDrag(game::TopologySelectionKind::StaticSpotLight, 9, game::SpotLightHandle::Origin),
          "begin second static spot drag succeeds");
    service.ApplyLightDragToSnappedPosition(Vector3{10.0f, 0.0f, 10.0f});
    service.CancelLightDragData("Cancelled light move");
    const game::SectorTopologyStaticSpotLight* cancelled =
            game::FindSectorTopologyStaticSpotLight(state.topologyMap, 9);
    Check(cancelled != nullptr
                  && Near(cancelled->position.x, 3.0f)
                  && Near(cancelled->position.z, 8.0f)
                  && Near(cancelled->target.x, 6.0f)
                  && Near(cancelled->target.z, 11.0f),
          "drag cancel restores original edit-state data");
    CheckClean(state, statusText, "Cancelled light move");
}

void TestLightDragFinishNoOpDoesNotDirty()
{
    game::SectorEditorState state;
    state.topologyMap = MakeMap();
    game::SectorTopologyStaticPointLight light;
    light.id = 10;
    light.position = Vector3{1.0f, 2.0f, 3.0f};
    state.topologyMap.staticLights.push_back(light);
    game::SectorEditorUiState uiState;
    std::string statusText;
    ResetDirty(state, statusText);

    game::SectorEditorLightEditingService service = MakeService(state, uiState, statusText);
    Check(service.BeginLightDrag(game::TopologySelectionKind::StaticLight, 10, game::SpotLightHandle::Origin),
          "begin static light drag succeeds");
    service.ApplyLightDragToSnappedPosition(Vector3{1.0f, 0.0f, 3.0f});
    const game::SectorEditorLightMutationResult result = service.FinishLightDrag();

    Check(!result.changed, "unchanged drag finish reports unchanged");
    CheckClean(state, statusText, "Static light unchanged");
}

void TestSpotLightPilotApplyAndCancelTiming()
{
    game::SectorEditorState state;
    state.topologyMap = MakeMap();
    game::SectorTopologyDynamicSpotLight light;
    light.id = 11;
    light.position = Vector3{1.0f, 2.0f, 3.0f};
    light.target = Vector3{4.0f, 5.0f, 6.0f};
    state.topologyMap.dynamicSpotLights.push_back(light);
    state.spotLightPilot.active = true;
    state.spotLightPilot.kind = game::SpotLightPilotKind::Dynamic;
    state.spotLightPilot.lightId = 11;
    state.spotLightPilot.originalPosition = light.position;
    state.spotLightPilot.originalTarget = light.target;
    game::SectorEditorUiState uiState;
    std::string statusText;
    ResetDirty(state, statusText);

    game::SectorEditorLightEditingService service = MakeService(state, uiState, statusText);
    const game::SectorEditorLightMutationResult apply =
            service.ApplySpotLightPilot(Vector3{7.0f, 8.0f, 9.0f}, Vector3{10.0f, 11.0f, 12.0f});
    const game::SectorTopologyDynamicSpotLight* applied =
            game::FindSectorTopologyDynamicSpotLight(state.topologyMap, 11);
    Check(apply.changed, "dynamic spotlight pilot apply reports changed");
    Check(apply.dynamicLightRendererRefreshNeeded, "dynamic spotlight pilot apply requests renderer refresh");
    Check(applied != nullptr
                  && Near(applied->position.x, 7.0f)
                  && Near(applied->target.z, 12.0f),
          "dynamic spotlight pilot apply mutates data");
    Check(!state.spotLightPilot.active, "dynamic spotlight pilot apply clears pilot state");
    CheckDirtyOnce(state, statusText, "Applied dynamic spot 11 pilot pose");

    ResetDirty(state, statusText);
    state.spotLightPilot.active = true;
    state.spotLightPilot.kind = game::SpotLightPilotKind::Dynamic;
    state.spotLightPilot.lightId = 11;
    state.spotLightPilot.originalPosition = Vector3{7.0f, 8.0f, 9.0f};
    state.spotLightPilot.originalTarget = Vector3{10.0f, 11.0f, 12.0f};
    service.ApplySpotLightPilot(Vector3{13.0f, 14.0f, 15.0f}, Vector3{16.0f, 17.0f, 18.0f});
    ResetDirty(state, statusText);
    state.spotLightPilot.active = true;
    state.spotLightPilot.kind = game::SpotLightPilotKind::Dynamic;
    state.spotLightPilot.lightId = 11;
    state.spotLightPilot.originalPosition = Vector3{7.0f, 8.0f, 9.0f};
    state.spotLightPilot.originalTarget = Vector3{10.0f, 11.0f, 12.0f};
    const game::SectorEditorLightMutationResult cancel =
            service.CancelSpotLightPilotData("Spotlight pilot cancelled");
    const game::SectorTopologyDynamicSpotLight* cancelled =
            game::FindSectorTopologyDynamicSpotLight(state.topologyMap, 11);
    Check(!cancel.changed, "dynamic spotlight pilot cancel reports unchanged");
    Check(!cancel.dynamicLightRendererRefreshNeeded, "dynamic spotlight pilot cancel preserves old no-refresh behavior");
    Check(cancel.previewPoseRestoreNeeded, "dynamic spotlight pilot cancel asks editor to restore preview pose");
    Check(cancelled != nullptr
                  && Near(cancelled->position.x, 7.0f)
                  && Near(cancelled->target.z, 12.0f),
          "dynamic spotlight pilot cancel restores original data");
    CheckClean(state, statusText, "Spotlight pilot cancelled");
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

    if (failures != 0) {
        std::cerr << failures << " SectorEditorLightEditingServiceTests failure(s)\n";
        return 1;
    }
    return 0;
}
