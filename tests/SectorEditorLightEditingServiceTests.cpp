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

game::SectorLightAtmosphereSettings TestAtmosphere()
{
    game::SectorLightAtmosphereSettings atmosphere;
    atmosphere.proxy.halo.enabled = true;
    atmosphere.proxy.halo.radiusWorld = 3.25f;
    atmosphere.proxy.halo.centerOffsetWorld = Vector3{0.25f, -0.5f, 0.75f};
    atmosphere.proxy.halo.brightness = 2.0f;
    atmosphere.proxy.halo.scatteringTint = Color{11, 22, 33, 255};
    atmosphere.proxy.shaft.enabled = true;
    atmosphere.proxy.shaft.originOffsetWorld = Vector3{-0.75f, 0.5f, 0.25f};
    atmosphere.proxy.shaft.lengthScale = 0.8f;
    atmosphere.proxy.shaft.widthScale = 0.6f;
    atmosphere.proxy.shaft.scatteringTint = Color{44, 55, 66, 255};
    atmosphere.dust.enabled = true;
    atmosphere.dust.amount = 17;
    atmosphere.dust.extentScale = 0.9f;
    atmosphere.dust.scatteringTint = Color{77, 88, 99, 255};
    return atmosphere;
}

void CheckAtmospherePreserved(
        const game::SectorLightAtmosphereSettings& atmosphere,
        const char* message)
{
    Check(atmosphere.proxy.halo.enabled
                  && Near(atmosphere.proxy.halo.radiusWorld, 3.25f)
                  && Near(atmosphere.proxy.halo.centerOffsetWorld.x, 0.25f)
                  && atmosphere.proxy.halo.scatteringTint.g == 22
                  && atmosphere.proxy.shaft.enabled
                  && Near(atmosphere.proxy.shaft.originOffsetWorld.x, -0.75f)
                  && Near(atmosphere.proxy.shaft.lengthScale, 0.8f)
                  && atmosphere.proxy.shaft.scatteringTint.b == 66
                  && atmosphere.dust.enabled
                  && atmosphere.dust.amount == 17
                  && Near(atmosphere.dust.extentScale, 0.9f)
                  && atmosphere.dust.scatteringTint.r == 77,
          message);
}

void TestConvertPointLightsPreservesSharedFieldsAndHandlesIdCollision()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SectorTopologyStaticPointLight source;
    source.id = 7;
    source.position = Vector3{1.0f, 2.0f, 3.0f};
    source.color = Color{20, 40, 60, 255};
    source.intensity = 2.5f;
    source.radius = 48.0f;
    source.sourceRadius = 4.0f;
    source.atmosphere = TestAtmosphere();
    source.castsShadow = true;
    documentState.map.topologyMap.staticLights.push_back(source);
    documentState.map.topologyMap.dynamicPointLights.push_back(
            game::SectorTopologyDynamicPointLight{7});

    game::SelectionState selectionState;
    selectionState.topologySelectionKind = game::TopologySelectionKind::StaticLight;
    selectionState.selectedTopologyLightId = 7;
    selectionState.hoveredTopologyLightId = 7;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    lightState.lightPilot.active = true;
    lightState.lightPilot.kind = game::LightPilotKind::StaticPoint;
    lightState.lightPilot.lightId = 7;
    lightState.lightPilot.originalPosition = source.position;
    std::string statusText;
    ResetDirty(state, documentState, statusText);
    game::SectorEditorLightEditingService service = MakeService(
            state, documentState, documentState.map.topologyMap,
            TestPreviewSelectionState(), selectionState, manipulationState,
            lightState, uiState, inspectorIdUiState, statusText);

    const game::SectorEditorLightMutationResult converted = service.ConvertSelectedLight();
    const game::SectorTopologyDynamicPointLight* dynamic =
            game::FindSectorTopologyDynamicLight(documentState.map.topologyMap, 8);
    Check(converted.changed && converted.dynamicLightRendererRefreshNeeded
                  && converted.previewPoseRestoreNeeded && !lightState.lightPilot.active,
          "static point conversion reports change and safely ends an active pilot");
    Check(documentState.map.topologyMap.staticLights.empty()
                  && documentState.map.topologyMap.dynamicPointLights.size() == 2
                  && dynamic != nullptr,
          "static point conversion replaces the source and allocates around a destination ID collision");
    Check(dynamic != nullptr
                  && Near(dynamic->position.x, 1.0f)
                  && dynamic->color.g == 40
                  && Near(dynamic->intensity, 2.5f)
                  && Near(dynamic->radius, 48.0f)
                  && dynamic->castsShadow,
          "static point conversion preserves shared light fields");
    Check(dynamic != nullptr && dynamic->enabled && !dynamic->flicker
                  && Near(dynamic->flickerSpeed, game::DynamicLightFlickerDefaultSpeed)
                  && Near(dynamic->shadowBias, game::DynamicSpotLightDefaultShadowBias),
          "static point conversion uses dynamic-only defaults");
    if (dynamic != nullptr) CheckAtmospherePreserved(
            dynamic->atmosphere, "static point conversion preserves haze shaft and dust");
    Check(selectionState.topologySelectionKind == game::TopologySelectionKind::DynamicLight
                  && selectionState.selectedTopologyDynamicLightId == 8
                  && selectionState.hoveredTopologyLightId < 0,
          "static point conversion selects the destination and clears stale hover");
    CheckDirtyOnce(state, documentState, statusText,
            "Converted static light 7 to dynamic light 8");

    ResetDirty(state, documentState, statusText);
    const game::SectorEditorLightMutationResult convertedBack = service.ConvertSelectedLight();
    const game::SectorTopologyStaticPointLight* staticLight =
            game::FindSectorTopologyStaticLight(documentState.map.topologyMap, 8);
    Check(convertedBack.changed && staticLight != nullptr
                  && Near(staticLight->sourceRadius, 0.0f)
                  && staticLight->castsShadow,
          "dynamic point conversion retains its ID and uses the static source-radius default");
    if (staticLight != nullptr) CheckAtmospherePreserved(
            staticLight->atmosphere, "dynamic point conversion preserves atmosphere");
    CheckDirtyOnce(state, documentState, statusText,
            "Converted dynamic light 8 to static light 8");
}

void TestConvertSpotAndRectLightsPreservesShapeAndAtmosphere()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SectorTopologyStaticSpotLight spot;
    spot.id = 12;
    spot.position = Vector3{1.0f, 2.0f, 3.0f};
    spot.target = Vector3{4.0f, 5.0f, 6.0f};
    spot.color = Color{12, 34, 56, 255};
    spot.intensity = 3.0f;
    spot.range = 72.0f;
    spot.innerConeDegrees = 14.0f;
    spot.outerConeDegrees = 39.0f;
    spot.sourceRadius = 5.0f;
    spot.castsShadow = false;
    spot.atmosphere = TestAtmosphere();
    documentState.map.topologyMap.staticSpotLights.push_back(spot);

    game::SelectionState selectionState;
    selectionState.topologySelectionKind = game::TopologySelectionKind::StaticSpotLight;
    selectionState.selectedTopologyStaticSpotLightId = 12;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);
    game::SectorEditorLightEditingService service = MakeService(
            state, documentState, documentState.map.topologyMap,
            TestPreviewSelectionState(), selectionState, manipulationState,
            lightState, uiState, inspectorIdUiState, statusText);

    service.ConvertSelectedLight();
    const game::SectorTopologyDynamicSpotLight* dynamicSpot =
            game::FindSectorTopologyDynamicSpotLight(documentState.map.topologyMap, 12);
    Check(dynamicSpot != nullptr
                  && Near(dynamicSpot->target.z, 6.0f)
                  && Near(dynamicSpot->range, 72.0f)
                  && Near(dynamicSpot->innerConeDegrees, 14.0f)
                  && Near(dynamicSpot->outerConeDegrees, 39.0f)
                  && !dynamicSpot->castsShadow,
          "static spot conversion preserves target cones range and shadow intent");
    if (dynamicSpot != nullptr) CheckAtmospherePreserved(
            dynamicSpot->atmosphere, "static spot conversion preserves shaft haze and dust");
    ResetDirty(state, documentState, statusText);
    service.ConvertSelectedLight();
    const game::SectorTopologyStaticSpotLight* staticSpot =
            game::FindSectorTopologyStaticSpotLight(documentState.map.topologyMap, 12);
    Check(staticSpot != nullptr && Near(staticSpot->sourceRadius, 0.0f)
                  && Near(staticSpot->target.z, 6.0f),
          "dynamic spot conversion uses the static source-radius default and preserves its target");
    CheckDirtyOnce(state, documentState, statusText,
            "Converted dynamic spot 12 to static spot 12");

    game::SectorTopologyDynamicRectLight rect;
    rect.id = 21;
    rect.position = Vector3{7.0f, 8.0f, 9.0f};
    rect.target = Vector3{10.0f, 11.0f, 12.0f};
    rect.rollDegrees = -32.0f;
    rect.width = 13.0f;
    rect.height = 2.0f;
    rect.color = Color{90, 80, 70, 255};
    rect.intensity = 1.75f;
    rect.range = 96.0f;
    rect.enabled = false;
    rect.flicker = true;
    rect.castsShadow = true;
    rect.atmosphere = TestAtmosphere();
    documentState.map.topologyMap.dynamicRectLights.push_back(rect);
    selectionState.topologySelectionKind = game::TopologySelectionKind::DynamicRectLight;
    selectionState.selectedTopologyDynamicSpotLightId = 21;
    selectionState.selectedTopologyDynamicLightId = -1;
    ResetDirty(state, documentState, statusText);

    service.ConvertSelectedLight();
    const game::SectorTopologyStaticRectLight* staticRect =
            game::FindSectorTopologyStaticRectLight(documentState.map.topologyMap, 21);
    Check(staticRect != nullptr
                  && Near(staticRect->position.y, 8.0f)
                  && Near(staticRect->target.x, 10.0f)
                  && Near(staticRect->rollDegrees, -32.0f)
                  && Near(staticRect->width, 13.0f)
                  && Near(staticRect->height, 2.0f)
                  && Near(staticRect->range, 96.0f)
                  && staticRect->castsShadow,
          "dynamic rect conversion preserves its shared shape and lighting fields");
    if (staticRect != nullptr) CheckAtmospherePreserved(
            staticRect->atmosphere, "dynamic rect conversion preserves shaft haze and dust");
    Check(selectionState.topologySelectionKind == game::TopologySelectionKind::StaticRectLight
                  && selectionState.selectedTopologyStaticSpotLightId == 21,
          "dynamic rect conversion selects the static rect destination");
    CheckDirtyOnce(state, documentState, statusText,
            "Converted dynamic rect light 21 to static rect light 21");

    ResetDirty(state, documentState, statusText);
    service.ConvertSelectedLight();
    const game::SectorTopologyDynamicRectLight* dynamicRect =
            game::FindSectorTopologyDynamicRectLight(documentState.map.topologyMap, 21);
    Check(dynamicRect != nullptr && dynamicRect->enabled && !dynamicRect->flicker
                  && Near(dynamicRect->width, 13.0f)
                  && Near(dynamicRect->rollDegrees, -32.0f)
                  && dynamicRect->castsShadow,
          "static rect conversion preserves shape and uses dynamic runtime defaults");
    if (dynamicRect != nullptr) CheckAtmospherePreserved(
            dynamicRect->atmosphere, "static rect conversion preserves shaft haze and dust");
    CheckDirtyOnce(state, documentState, statusText,
            "Converted static rect light 21 to dynamic rect light 21");
}

void TestConvertSelectedLightFailureIsClean()
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
    game::SectorEditorLightEditingService service = MakeService(
            state, documentState, documentState.map.topologyMap,
            TestPreviewSelectionState(), selectionState, manipulationState,
            lightState, uiState, inspectorIdUiState, statusText);

    const game::SectorEditorLightMutationResult result = service.ConvertSelectedLight();
    Check(!result.changed, "conversion without a selected light reports unchanged");
    CheckClean(state, documentState, statusText,
            "Light conversion failed: no light selected");
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

void TestAddRectLightsPointDownByDefault()
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

    game::SectorEditorLightEditingService service = MakeService(
            state, documentState, documentState.map.topologyMap,
            TestPreviewSelectionState(), selectionState, manipulationState,
            lightState, uiState, inspectorIdUiState, statusText);
    const auto staticResult = service.AddStaticRectLight(1, Vector2{2.0f, 3.0f});
    const game::SectorTopologyStaticRectLight& staticRect =
            documentState.map.topologyMap.staticRectLights.front();
    Check(staticResult.changed
                  && Near(staticRect.position.x, staticRect.target.x)
                  && Near(staticRect.position.z, staticRect.target.z)
                  && Near(staticRect.position.y - staticRect.target.y,
                          game::SectorWorldToAuthoringDistance(0.8f))
                  && Near(staticRect.rollDegrees, 0.0f),
          "new static rect lights point directly down with zero roll");
    CheckDirtyOnce(state, documentState, statusText, "Added static rect light 1");

    ResetDirty(state, documentState, statusText);
    const auto dynamicResult = service.AddDynamicRectLight(1, Vector2{4.0f, 5.0f});
    const game::SectorTopologyDynamicRectLight& dynamicRect =
            documentState.map.topologyMap.dynamicRectLights.front();
    Check(dynamicResult.changed
                  && Near(dynamicRect.position.x, dynamicRect.target.x)
                  && Near(dynamicRect.position.z, dynamicRect.target.z)
                  && Near(dynamicRect.position.y - dynamicRect.target.y,
                          game::SectorWorldToAuthoringDistance(0.8f))
                  && Near(dynamicRect.rollDegrees, 0.0f),
          "new dynamic rect lights point directly down with zero roll");
    CheckDirtyOnce(state, documentState, statusText, "Added dynamic rect light 1");

    const game::SectorTopologyStaticRectLight defaultStatic;
    const game::SectorTopologyDynamicRectLight defaultDynamic;
    Check(Near(defaultStatic.position.x, defaultStatic.target.x)
                  && Near(defaultStatic.position.z, defaultStatic.target.z)
                  && defaultStatic.target.y < defaultStatic.position.y
                  && Near(defaultDynamic.position.x, defaultDynamic.target.x)
                  && Near(defaultDynamic.position.z, defaultDynamic.target.z)
                  && defaultDynamic.target.y < defaultDynamic.position.y,
          "default-constructed rect lights point directly down");
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

void TestRectLightPilotUsesRelativeRoll()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    documentState.map.topologyMap = MakeMap();
    game::SectorTopologyStaticRectLight staticRect;
    staticRect.id = 27;
    staticRect.position = {1.0f, 2.0f, 3.0f};
    staticRect.target = {1.0f, -6.0f, 3.0f};
    staticRect.rollDegrees = 90.0f;
    documentState.map.topologyMap.staticRectLights.push_back(staticRect);
    game::SectorTopologyDynamicRectLight dynamicRect;
    dynamicRect.id = 28;
    dynamicRect.position = {4.0f, 5.0f, 6.0f};
    dynamicRect.target = {4.0f, -3.0f, 6.0f};
    dynamicRect.rollDegrees = 170.0f;
    documentState.map.topologyMap.dynamicRectLights.push_back(dynamicRect);

    game::SelectionState selectionState;
    game::ManipulationState manipulationState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    game::LightEditingState lightState;
    std::string statusText;
    ResetDirty(state, documentState, statusText);
    game::SectorEditorLightEditingService service = MakeService(
            state, documentState, documentState.map.topologyMap,
            TestPreviewSelectionState(), selectionState, manipulationState,
            lightState, uiState, inspectorIdUiState, statusText);

    lightState.lightPilot.active = true;
    lightState.lightPilot.kind = game::LightPilotKind::StaticRect;
    lightState.lightPilot.lightId = 27;
    lightState.lightPilot.originalPosition = staticRect.position;
    lightState.lightPilot.originalTarget = staticRect.target;
    lightState.lightPilot.originalRollDegrees = staticRect.rollDegrees;
    service.ApplyLightPilot({2.0f, 3.0f, 4.0f}, {2.0f, -5.0f, 4.0f}, 0.0f);
    Check(Near(documentState.map.topologyMap.staticRectLights.front().rollDegrees, 90.0f),
          "applying an upright rect pilot preserves the authored roll");
    CheckDirtyOnce(state, documentState, statusText, "Applied static rect light 27 pilot pose");

    ResetDirty(state, documentState, statusText);
    lightState.lightPilot.active = true;
    lightState.lightPilot.kind = game::LightPilotKind::DynamicRect;
    lightState.lightPilot.lightId = 28;
    lightState.lightPilot.originalPosition = dynamicRect.position;
    lightState.lightPilot.originalTarget = dynamicRect.target;
    lightState.lightPilot.originalRollDegrees = dynamicRect.rollDegrees;
    service.ApplyLightPilot({5.0f, 6.0f, 7.0f}, {5.0f, -2.0f, 7.0f}, 25.0f);
    Check(Near(documentState.map.topologyMap.dynamicRectLights.front().rollDegrees, -165.0f),
          "rect pilot roll is a normalized delta from the authored roll");
    CheckDirtyOnce(state, documentState, statusText, "Applied dynamic rect light 28 pilot pose");

    ResetDirty(state, documentState, statusText);
    game::SectorTopologyStaticRectLight& editedStatic =
            documentState.map.topologyMap.staticRectLights.front();
    editedStatic.position = {8.0f, 9.0f, 10.0f};
    editedStatic.target = {8.0f, 1.0f, 10.0f};
    editedStatic.rollDegrees = 45.0f;
    lightState.lightPilot.active = true;
    lightState.lightPilot.kind = game::LightPilotKind::StaticRect;
    lightState.lightPilot.lightId = 27;
    lightState.lightPilot.originalPosition = {2.0f, 3.0f, 4.0f};
    lightState.lightPilot.originalTarget = {2.0f, -5.0f, 4.0f};
    lightState.lightPilot.originalRollDegrees = 90.0f;
    const auto cancel = service.CancelLightPilotData("Light pilot cancelled");
    Check(cancel.previewPoseRestoreNeeded
                  && Near(editedStatic.position.x, 2.0f)
                  && Near(editedStatic.target.y, -5.0f)
                  && Near(editedStatic.rollDegrees, 90.0f),
          "cancelling rect pilot restores position, target, and authored roll");
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
    staticSpot.atmosphere.proxy.shaft.enabled = true;
    staticSpot.atmosphere.proxy.shaft.brightness = 0.35f;
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

    game::SectorTopologyStaticRectLight staticRect;
    staticRect.id = 25;
    staticRect.position = Vector3{4.0f, 18.0f, 6.0f};
    staticRect.target = Vector3{7.0f, 22.0f, 6.0f};
    staticRect.rollDegrees = 90.0f;
    documentState.map.topologyMap.staticRectLights.push_back(staticRect);

    game::SectorTopologyDynamicRectLight dynamicRect;
    dynamicRect.id = 26;
    dynamicRect.position = Vector3{-4.0f, 10.0f, 2.0f};
    dynamicRect.target = dynamicRect.position;
    dynamicRect.rollDegrees = -35.0f;
    documentState.map.topologyMap.dynamicRectLights.push_back(dynamicRect);

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
                  && editedStatic.atmosphere.proxy.shaft.enabled
                          == staticBefore.atmosphere.proxy.shaft.enabled
                  && Near(editedStatic.atmosphere.proxy.shaft.brightness,
                          staticBefore.atmosphere.proxy.shaft.brightness),
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

    ResetDirty(state, documentState, statusText);
    game::SectorTopologyStaticRectLight& editedStaticRect =
            documentState.map.topologyMap.staticRectLights.front();
    Check(service.PointStaticRectLightDown(editedStaticRect)
                  && Near(editedStaticRect.target.x, editedStaticRect.position.x)
                  && Near(editedStaticRect.target.y, 13.0f)
                  && Near(editedStaticRect.target.z, editedStaticRect.position.z)
                  && Near(editedStaticRect.rollDegrees, 90.0f),
          "static rect Point Down preserves target distance and authored roll");
    CheckDirtyOnce(state, documentState, statusText, "Pointed static rect light 25 down");

    ResetDirty(state, documentState, statusText);
    game::SectorTopologyDynamicRectLight& editedDynamicRect =
            documentState.map.topologyMap.dynamicRectLights.front();
    Check(service.PointDynamicRectLightDown(editedDynamicRect)
                  && Near(editedDynamicRect.target.x, editedDynamicRect.position.x)
                  && Near(editedDynamicRect.target.y,
                          editedDynamicRect.position.y - fallbackDistance)
                  && Near(editedDynamicRect.target.z, editedDynamicRect.position.z)
                  && Near(editedDynamicRect.rollDegrees, -35.0f),
          "coincident dynamic rect Point Down uses the fallback and preserves roll");
    CheckDirtyOnce(state, documentState, statusText, "Pointed dynamic rect light 26 down");

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
    atmosphere.proxy.halo.enabled = true;
    atmosphere.proxy.halo.brightness = 0.4f;
    atmosphere.proxy.halo.maxExtinction = 0.6f;
    atmosphere.proxy.halo.scatteringTint = Color{180, 210, 240, 255};
    atmosphere.dust.enabled = true;
    atmosphere.dust.amount = 35;
    Check(service.SetStaticLightAtmosphere(
                  documentState.map.topologyMap.staticLights.front(), atmosphere),
          "atmosphere edit reports a change");
    Check(documentState.map.topologyMap.staticLights.front().atmosphere.proxy.halo.enabled
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
          "enabled haze begins placement");
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
          "enabled shaft begins placement for a spotlight");
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

void TestConfigClipboardPreservesPlacementAndCopiesRelativeAim()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorTopologyMap map = MakeMap();
    game::SectorTopologyStaticSpotLight source;
    source.id = 20;
    source.position = Vector3{2.0f, 3.0f, 4.0f};
    source.target = Vector3{7.0f, 1.0f, 2.0f};
    source.intensity = 9.0f;
    source.range = 33.0f;
    source.innerConeDegrees = 11.0f;
    source.outerConeDegrees = 22.0f;
    source.sourceRadius = 0.75f;
    source.castsShadow = false;
    source.atmosphere.proxy.halo.enabled = true;
    source.atmosphere.proxy.halo.brightness = 0.8f;
    source.atmosphere.proxy.shaft.enabled = true;
    source.atmosphere.proxy.shaft.widthScale = 1.4f;
    source.atmosphere.dust.enabled = true;
    source.atmosphere.dust.amount = 77;
    game::SectorTopologyStaticSpotLight destination;
    destination.id = 21;
    destination.position = Vector3{100.0f, 200.0f, 300.0f};
    destination.target = Vector3{100.0f, 199.0f, 300.0f};
    map.staticSpotLights = {source, destination};

    game::SectorEditorPreviewSelectionState previewSelectionState;
    game::SelectionState selectionState;
    selectionState.topologySelectionKind = game::TopologySelectionKind::StaticSpotLight;
    selectionState.selectedTopologyStaticSpotLightId = source.id;
    game::ManipulationState manipulationState;
    game::LightEditingState lightState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    std::string statusText;
    game::SectorEditorLightEditingService service = MakeService(
            state, documentState, map, previewSelectionState, selectionState,
            manipulationState, lightState, uiState, inspectorIdUiState, statusText);

    game::SectorEditorConfigClipboardState clipboard;
    Check(service.CopySelectedConfig(clipboard),
          "copying a selected static spot config succeeds");
    Check(clipboard.kind == game::SectorEditorConfigKind::StaticSpotLight,
          "static spot copy records the exact light subtype");

    selectionState.selectedTopologyStaticSpotLightId = destination.id;
    ResetDirty(state, documentState, statusText);
    const game::SectorEditorLightMutationResult pasted =
            service.PasteSelectedConfig(clipboard);
    const game::SectorTopologyStaticSpotLight& result = map.staticSpotLights[1];
    Check(pasted.changed && pasted.dynamicLightRendererRefreshNeeded,
          "static spot config paste reports a preview light refresh");
    Check(result.id == 21
                  && Near(result.position.x, 100.0f)
                  && Near(result.position.y, 200.0f)
                  && Near(result.position.z, 300.0f),
          "static spot config paste preserves destination identity and position");
    Check(Near(result.target.x, 105.0f)
                  && Near(result.target.y, 198.0f)
                  && Near(result.target.z, 298.0f),
          "static spot config paste rebuilds the copied relative aim at the destination");
    Check(Near(result.intensity, 9.0f)
                  && Near(result.range, 33.0f)
                  && Near(result.sourceRadius, 0.75f)
                  && result.atmosphere.proxy.halo.enabled
                  && result.atmosphere.proxy.shaft.enabled
                  && result.atmosphere.dust.enabled
                  && result.atmosphere.dust.amount == 77,
          "static spot config paste copies light and atmosphere settings");
    CheckDirtyOnce(state, documentState, statusText, "Pasted static spot light config");

    ResetDirty(state, documentState, statusText);
    Check(!service.PasteSelectedConfig(clipboard).changed,
          "repeating an identical light config paste is a no-op");
    CheckClean(state, documentState, statusText,
               "Selected light already matches copied config.");
}

void TestDynamicConfigClipboardPreservesInstanceIdAndRejectsOtherTypes()
{
    game::SectorEditorState state;
    game::SectorEditorDocumentState documentState;
    game::SectorTopologyMap map = MakeMap();
    game::SectorTopologyDynamicPointLight source;
    source.id = 30;
    source.position = Vector3{1.0f, 2.0f, 3.0f};
    source.instanceId = "source_light";
    source.enabled = false;
    source.flicker = true;
    source.flickerSpeed = 2.5f;
    source.flickerAmount = 0.8f;
    source.castsShadow = true;
    source.shadowPriority = 17;
    source.shadowStrength = 0.4f;
    source.atmosphere.dust.enabled = true;
    game::SectorTopologyDynamicPointLight destination;
    destination.id = 31;
    destination.position = Vector3{9.0f, 8.0f, 7.0f};
    destination.instanceId = "destination_light";
    map.dynamicPointLights = {source, destination};
    game::SectorTopologyStaticPointLight staticDestination;
    staticDestination.id = 32;
    map.staticLights.push_back(staticDestination);

    game::SectorEditorPreviewSelectionState previewSelectionState;
    game::SelectionState selectionState;
    selectionState.topologySelectionKind = game::TopologySelectionKind::DynamicLight;
    selectionState.selectedTopologyDynamicLightId = source.id;
    game::ManipulationState manipulationState;
    game::LightEditingState lightState;
    game::SectorEditorUiState uiState;
    game::InspectorIdUiState inspectorIdUiState;
    std::string statusText;
    game::SectorEditorLightEditingService service = MakeService(
            state, documentState, map, previewSelectionState, selectionState,
            manipulationState, lightState, uiState, inspectorIdUiState, statusText);

    game::SectorEditorConfigClipboardState clipboard;
    Check(service.CopySelectedConfig(clipboard),
          "copying a dynamic point light config succeeds");
    selectionState.selectedTopologyDynamicLightId = destination.id;
    ResetDirty(state, documentState, statusText);
    Check(service.PasteSelectedConfig(clipboard).changed,
          "pasting a matching dynamic point light config succeeds");
    const game::SectorTopologyDynamicPointLight& result = map.dynamicPointLights[1];
    Check(result.id == 31
                  && result.instanceId == "destination_light"
                  && Near(result.position.x, 9.0f)
                  && !result.enabled
                  && result.flicker
                  && result.shadowPriority == 17
                  && Near(result.shadowStrength, 0.4f)
                  && result.atmosphere.dust.enabled,
          "dynamic light paste preserves identity and placement while copying behavior");

    selectionState.topologySelectionKind = game::TopologySelectionKind::StaticLight;
    selectionState.selectedTopologyLightId = staticDestination.id;
    ResetDirty(state, documentState, statusText);
    Check(!service.PasteSelectedConfig(clipboard).changed,
          "a copied dynamic point config is rejected for a static point light");
    CheckClean(state, documentState, statusText,
               "Copied config does not match the selected light type.");
}

} // namespace

int main()
{
    TestConvertPointLightsPreservesSharedFieldsAndHandlesIdCollision();
    TestConvertSpotAndRectLightsPreservesShapeAndAtmosphere();
    TestConvertSelectedLightFailureIsClean();
    TestAddStaticLightDirtiesAndSelects();
    TestAddDynamicLightDirtiesAndSelects();
    TestAddRectLightsPointDownByDefault();
    TestAddNoOpDoesNotDirty();
    TestDeleteSelectedStaticLightDirtiesAndClearsState();
    TestDeleteSelectedDynamicLightDirties();
    TestLightDragApplyFinishAndCancelTiming();
    TestLightDragFinishNoOpDoesNotDirty();
    TestSpotLightPilotApplyAndCancelTiming();
    TestPointLightPilotApplyAndCancelTiming();
    TestRectLightPilotUsesRelativeRoll();
    TestPointSpotLightsDown();
    TestStaticShadowEditsUseDocumentMutationBoundary();
    TestAtmosphereEditUsesDocumentMutationBoundary();
    TestProxyPlacementApplyAndCancelTiming();
    TestConfigClipboardPreservesPlacementAndCopiesRelativeAim();
    TestDynamicConfigClipboardPreservesInstanceIdAndRejectsOtherTypes();

    if (failures != 0) {
        std::cerr << failures << " SectorEditorLightEditingServiceTests failure(s)\n";
        return 1;
    }
    return 0;
}
