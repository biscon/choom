#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/inspector/SectorEditorInspectorUiState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorSurfaceTypes.h"
#include "sector_editor/SectorEditorTopologyRenderCacheTypes.h"
#include "sector_editor/selection/SectorEditorManipulationState.h"
#include "sector_editor/services/lights/SectorEditorLightEditingState.h"
#include "sector_editor/SectorEditorTopologyActions.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace game {

struct SectorEditorLightEditingServiceContext {
    SectorTopologyMap& map;
    LightEditingState& lightState;
    SectorEditorDocumentLifecycleAccess lifecycle;
    uint64_t& topologyRenderRevision;
    SectorEditorTopologyRenderCache& topologyRenderCache;
    struct SelectionRefs {
        ManipulationState& manipulationState;
        RuntimeObjectDragState& runtimeObjectDrag;
        TopologySelectionKind& topologySelectionKind;
        int& selectedTopologySectorId;
        int& selectedTopologyVertexId;
        int& selectedTopologySideDefId;
        int& selectedTopologyLineDefId;
        int& selectedTopologyLightId;
        int& selectedTopologyStaticSpotLightId;
        int& selectedTopologyDynamicLightId;
        int& selectedTopologyDynamicSpotLightId;
        int& selectedRuntimeObjectId;
        SectorTopologySideKind& selectedTopologySideKind;
        int& inspectedTopologyVertexId;
        SectorSurfaceRef& selectedSurface3D;
        TopologySurfaceEditTarget& selectedTopologySurface3D;
        SectorAuthoringSelectionTarget& selectedAuthoring;
        int& hoveredTopologyLightId;
        int& hoveredTopologyStaticSpotLightId;
        int& hoveredTopologyDynamicLightId;
        int& hoveredTopologyDynamicSpotLightId;
    } selection;
    struct UiRefs {
        engine::UIScrollState& inspectorScroll;
        engine::UIFloatInputState& lightXInput;
        engine::UIFloatInputState& lightYInput;
        engine::UIFloatInputState& lightZInput;
        engine::UIFloatInputState& lightTargetXInput;
        engine::UIFloatInputState& lightTargetYInput;
        engine::UIFloatInputState& lightTargetZInput;
        engine::UIFloatInputState& lightIntensityInput;
        engine::UIFloatInputState& lightRadiusInput;
        engine::UIFloatInputState& lightInnerConeInput;
        engine::UIFloatInputState& lightOuterConeInput;
        engine::UIFloatInputState& lightSourceRadiusInput;
        engine::UIFloatInputState& lightFlickerSpeedInput;
        engine::UIFloatInputState& lightFlickerAmountInput;
        engine::UIIntInputState& lightShadowPriorityInput;
        engine::UIFloatInputState& lightShadowBiasInput;
        engine::UIFloatInputState& lightShadowStrengthInput;
        engine::UIFloatInputState& lightShadowSoftnessInput;
        engine::UIIntInputState& lightRedInput;
        engine::UIIntInputState& lightGreenInput;
        engine::UIIntInputState& lightBlueInput;
        InspectorIdUiState& inspectorIdUiState;
        struct AtmosphereRefs {
            engine::UIIntInputState* dustAmountInput = nullptr;
            engine::UIFloatInputState* dustExtentScaleInput = nullptr;
            engine::UIFloatInputState* dustMinimumSizeInput = nullptr;
            engine::UIFloatInputState* dustMaximumSizeInput = nullptr;
            engine::UIFloatInputState* dustOpacityInput = nullptr;
            engine::UIFloatInputState* dustDriftSpeedInput = nullptr;
            engine::UIFloatInputState* dustTurbulenceInput = nullptr;
            engine::UIIntInputState* dustRedInput = nullptr;
            engine::UIIntInputState* dustGreenInput = nullptr;
            engine::UIIntInputState* dustBlueInput = nullptr;
            engine::UIFloatInputState* proxyHaloRadiusInput = nullptr;
            engine::UIFloatInputState* proxyHaloOffsetXInput = nullptr;
            engine::UIFloatInputState* proxyHaloOffsetYInput = nullptr;
            engine::UIFloatInputState* proxyHaloOffsetZInput = nullptr;
            engine::UIFloatInputState* proxyHaloBrightnessInput = nullptr;
            engine::UIFloatInputState* proxyHaloMaxExtinctionInput = nullptr;
            engine::UIFloatInputState* proxyHaloSoftnessInput = nullptr;
            engine::UIIntInputState* proxyHaloRedInput = nullptr;
            engine::UIIntInputState* proxyHaloGreenInput = nullptr;
            engine::UIIntInputState* proxyHaloBlueInput = nullptr;
            engine::UIFloatInputState* proxyShaftOffsetXInput = nullptr;
            engine::UIFloatInputState* proxyShaftOffsetYInput = nullptr;
            engine::UIFloatInputState* proxyShaftOffsetZInput = nullptr;
            engine::UIFloatInputState* proxyShaftLengthInput = nullptr;
            engine::UIFloatInputState* proxyShaftWidthInput = nullptr;
            engine::UIFloatInputState* proxyShaftBrightnessInput = nullptr;
            engine::UIFloatInputState* proxyShaftMaxExtinctionInput = nullptr;
            engine::UIFloatInputState* proxyShaftSoftnessInput = nullptr;
            engine::UIIntInputState* proxyShaftRedInput = nullptr;
            engine::UIIntInputState* proxyShaftGreenInput = nullptr;
            engine::UIIntInputState* proxyShaftBlueInput = nullptr;
        } atmosphere;
    } ui;
    std::string& statusText;
};

struct SectorEditorLightMutationResult {
    bool changed = false;
    bool dynamicLightRendererRefreshNeeded = false;
    bool previewPoseRestoreNeeded = false;
    LightPilotLightState restoredLightPilot;
};

class SectorEditorLightEditingService {
public:
    explicit SectorEditorLightEditingService(SectorEditorLightEditingServiceContext context);

    SectorEditorLightMutationResult AddStaticLight(int sectorId, Vector2 mapPoint);
    SectorEditorLightMutationResult AddStaticSpotLight(int sectorId, Vector2 mapPoint);
    SectorEditorLightMutationResult AddStaticRectLight(int sectorId, Vector2 mapPoint);
    SectorEditorLightMutationResult AddDynamicLight(int sectorId, Vector2 mapPoint);
    SectorEditorLightMutationResult AddDynamicSpotLight(int sectorId, Vector2 mapPoint);
    SectorEditorLightMutationResult AddDynamicRectLight(int sectorId, Vector2 mapPoint);
    SectorEditorLightMutationResult ConvertSelectedLight();
    SectorEditorLightMutationResult DeleteSelectedLightConfirmed();

    bool BeginLightDrag(TopologySelectionKind kind, int topologyLightId, SpotLightHandle spotHandle);
    SectorEditorLightMutationResult ApplyLightDragToSnappedPosition(Vector3 snappedPosition);
    SectorEditorLightMutationResult FinishLightDrag();
    SectorEditorLightMutationResult CancelLightDragData(const char* message);

    SectorEditorLightMutationResult ApplyLightPilot(
            Vector3 position,
            Vector3 target,
            float rectRollDeltaDegrees = 0.0f);
    SectorEditorLightMutationResult CancelLightPilotData(const char* message);

    bool BeginProxyPlacement(
            LightProxyPlacementKind proxyKind,
            LightPilotKind kind,
            int lightId);
    SectorEditorLightMutationResult PreviewProxyPlacement(Vector3 offsetWorld);
    SectorEditorLightMutationResult ApplyProxyPlacement();
    SectorEditorLightMutationResult CancelProxyPlacementData(const char* message);

    bool SetStaticLightPosition(SectorTopologyStaticPointLight& light, Vector3 position);
    bool SetStaticLightIntensity(SectorTopologyStaticPointLight& light, float intensity);
    bool SetStaticLightRadius(SectorTopologyStaticPointLight& light, float radius);
    bool SetStaticLightSourceRadius(SectorTopologyStaticPointLight& light, float sourceRadius);
    bool SetStaticLightCastsShadow(SectorTopologyStaticPointLight& light, bool castsShadow);
    bool SetStaticLightColor(SectorTopologyStaticPointLight& light, Color color);
    bool SetStaticLightAtmosphere(
            SectorTopologyStaticPointLight& light,
            SectorLightAtmosphereSettings settings);

    bool SetStaticSpotLightPosition(SectorTopologyStaticSpotLight& light, Vector3 position);
    bool SetStaticSpotLightTarget(SectorTopologyStaticSpotLight& light, Vector3 target);
    bool PointStaticSpotLightDown(SectorTopologyStaticSpotLight& light);
    bool SetStaticSpotLightRange(SectorTopologyStaticSpotLight& light, float range);
    bool SetStaticSpotLightSourceRadius(SectorTopologyStaticSpotLight& light, float sourceRadius);
    bool SetStaticSpotLightInnerCone(SectorTopologyStaticSpotLight& light, float innerConeDegrees);
    bool SetStaticSpotLightOuterCone(SectorTopologyStaticSpotLight& light, float outerConeDegrees);
    bool SetStaticSpotLightCastsShadow(SectorTopologyStaticSpotLight& light, bool castsShadow);
    bool SetStaticSpotLightIntensity(SectorTopologyStaticSpotLight& light, float intensity);
    bool SetStaticSpotLightColor(SectorTopologyStaticSpotLight& light, Color color);
    bool SetStaticSpotLightAtmosphere(
            SectorTopologyStaticSpotLight& light,
            SectorLightAtmosphereSettings settings);

    bool SetDynamicLightEnabled(SectorTopologyDynamicPointLight& light, bool enabled);
    bool SetDynamicLightInstanceId(SectorTopologyDynamicPointLight& light, const std::string& value, std::string& error);
    bool SetDynamicLightFlicker(SectorTopologyDynamicPointLight& light, bool flicker);
    bool SetDynamicLightFlickerSpeed(SectorTopologyDynamicPointLight& light, float flickerSpeed);
    bool SetDynamicLightFlickerAmount(SectorTopologyDynamicPointLight& light, float flickerAmount);
    bool SetDynamicLightCastsShadow(SectorTopologyDynamicPointLight& light, bool castsShadow);
    bool SetDynamicLightShadowPriority(SectorTopologyDynamicPointLight& light, int shadowPriority);
    bool SetDynamicLightShadowBias(SectorTopologyDynamicPointLight& light, float shadowBias);
    bool SetDynamicLightShadowStrength(SectorTopologyDynamicPointLight& light, float shadowStrength);
    bool SetDynamicLightShadowSoftness(SectorTopologyDynamicPointLight& light, float shadowSoftness);
    bool SetDynamicLightPosition(SectorTopologyDynamicPointLight& light, Vector3 position);
    bool SetDynamicLightIntensity(SectorTopologyDynamicPointLight& light, float intensity);
    bool SetDynamicLightRadius(SectorTopologyDynamicPointLight& light, float radius);
    bool SetDynamicLightColor(SectorTopologyDynamicPointLight& light, Color color);
    bool SetDynamicLightAtmosphere(
            SectorTopologyDynamicPointLight& light,
            SectorLightAtmosphereSettings settings);

    bool SetDynamicSpotLightEnabled(SectorTopologyDynamicSpotLight& light, bool enabled);
    bool SetDynamicLightInstanceId(SectorTopologyDynamicSpotLight& light, const std::string& value, std::string& error);
    bool SetDynamicSpotLightFlicker(SectorTopologyDynamicSpotLight& light, bool flicker);
    bool SetDynamicSpotLightFlickerSpeed(SectorTopologyDynamicSpotLight& light, float flickerSpeed);
    bool SetDynamicSpotLightFlickerAmount(SectorTopologyDynamicSpotLight& light, float flickerAmount);
    bool SetDynamicSpotLightCastsShadow(SectorTopologyDynamicSpotLight& light, bool castsShadow);
    bool SetDynamicSpotLightShadowPriority(SectorTopologyDynamicSpotLight& light, int shadowPriority);
    bool SetDynamicSpotLightShadowBias(SectorTopologyDynamicSpotLight& light, float shadowBias);
    bool SetDynamicSpotLightShadowStrength(SectorTopologyDynamicSpotLight& light, float shadowStrength);
    bool SetDynamicSpotLightShadowSoftness(SectorTopologyDynamicSpotLight& light, float shadowSoftness);
    bool SetDynamicSpotLightPosition(SectorTopologyDynamicSpotLight& light, Vector3 position);
    bool SetDynamicSpotLightTarget(SectorTopologyDynamicSpotLight& light, Vector3 target);
    bool PointDynamicSpotLightDown(SectorTopologyDynamicSpotLight& light);
    bool SetDynamicSpotLightIntensity(SectorTopologyDynamicSpotLight& light, float intensity);
    bool SetDynamicSpotLightRange(SectorTopologyDynamicSpotLight& light, float range);
    bool SetDynamicSpotLightInnerCone(SectorTopologyDynamicSpotLight& light, float innerConeDegrees);
    bool SetDynamicSpotLightOuterCone(SectorTopologyDynamicSpotLight& light, float outerConeDegrees);
    bool SetDynamicSpotLightColor(SectorTopologyDynamicSpotLight& light, Color color);
    bool SetDynamicSpotLightAtmosphere(
            SectorTopologyDynamicSpotLight& light,
            SectorLightAtmosphereSettings settings);

    bool SetStaticRectLightPosition(SectorTopologyStaticRectLight& light, Vector3 value);
    bool SetStaticRectLightTarget(SectorTopologyStaticRectLight& light, Vector3 value);
    bool PointStaticRectLightDown(SectorTopologyStaticRectLight& light);
    bool SetStaticRectLightRoll(SectorTopologyStaticRectLight& light, float value);
    bool SetStaticRectLightWidth(SectorTopologyStaticRectLight& light, float value);
    bool SetStaticRectLightHeight(SectorTopologyStaticRectLight& light, float value);
    bool SetStaticRectLightRange(SectorTopologyStaticRectLight& light, float value);
    bool SetStaticRectLightIntensity(SectorTopologyStaticRectLight& light, float value);
    bool SetStaticRectLightCastsShadow(SectorTopologyStaticRectLight& light, bool value);
    bool SetStaticRectLightColor(SectorTopologyStaticRectLight& light, Color value);
    bool SetStaticRectLightAtmosphere(SectorTopologyStaticRectLight& light, SectorLightAtmosphereSettings value);
    bool SetDynamicRectLightPosition(SectorTopologyDynamicRectLight& light, Vector3 value);
    bool SetDynamicLightInstanceId(SectorTopologyDynamicRectLight& light, const std::string& value, std::string& error);
    bool SetDynamicRectLightTarget(SectorTopologyDynamicRectLight& light, Vector3 value);
    bool PointDynamicRectLightDown(SectorTopologyDynamicRectLight& light);
    bool SetDynamicRectLightRoll(SectorTopologyDynamicRectLight& light, float value);
    bool SetDynamicRectLightWidth(SectorTopologyDynamicRectLight& light, float value);
    bool SetDynamicRectLightHeight(SectorTopologyDynamicRectLight& light, float value);
    bool SetDynamicRectLightRange(SectorTopologyDynamicRectLight& light, float value);
    bool SetDynamicRectLightIntensity(SectorTopologyDynamicRectLight& light, float value);
    bool SetDynamicRectLightEnabled(SectorTopologyDynamicRectLight& light, bool value);
    bool SetDynamicRectLightFlicker(SectorTopologyDynamicRectLight& light, bool value);
    bool SetDynamicRectLightFlickerSpeed(SectorTopologyDynamicRectLight& light, float value);
    bool SetDynamicRectLightFlickerAmount(SectorTopologyDynamicRectLight& light, float value);
    bool SetDynamicRectLightCastsShadow(SectorTopologyDynamicRectLight& light, bool value);
    bool SetDynamicRectLightShadowPriority(SectorTopologyDynamicRectLight& light, int value);
    bool SetDynamicRectLightShadowBias(SectorTopologyDynamicRectLight& light, float value);
    bool SetDynamicRectLightShadowStrength(SectorTopologyDynamicRectLight& light, float value);
    bool SetDynamicRectLightShadowSoftness(SectorTopologyDynamicRectLight& light, float value);
    bool SetDynamicRectLightColor(SectorTopologyDynamicRectLight& light, Color value);
    bool SetDynamicRectLightAtmosphere(SectorTopologyDynamicRectLight& light, SectorLightAtmosphereSettings value);

private:
    void MarkEdited(const char* status);
    bool FinishTopologyActionResult(const SectorEditorTopologyActionResult& result);

    SectorEditorLightEditingServiceContext context_;
};

} // namespace game
