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
            engine::UIFloatInputState* hazeExtentScaleInput = nullptr;
            engine::UIFloatInputState* hazeHeightOffsetInput = nullptr;
            engine::UIFloatInputState* hazeDensityInput = nullptr;
            engine::UIFloatInputState* hazeEdgeSoftnessInput = nullptr;
            engine::UIFloatInputState* hazeNoiseAmountInput = nullptr;
            engine::UIFloatInputState* hazeNoiseScaleInput = nullptr;
            engine::UIFloatInputState* hazeFlowDirectionInput = nullptr;
            engine::UIFloatInputState* hazeFlowSpeedInput = nullptr;
            engine::UIIntInputState* hazeRedInput = nullptr;
            engine::UIIntInputState* hazeGreenInput = nullptr;
            engine::UIIntInputState* hazeBlueInput = nullptr;
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
    SectorEditorLightMutationResult AddDynamicLight(int sectorId, Vector2 mapPoint);
    SectorEditorLightMutationResult AddDynamicSpotLight(int sectorId, Vector2 mapPoint);
    SectorEditorLightMutationResult DeleteSelectedLightConfirmed();

    bool BeginLightDrag(TopologySelectionKind kind, int topologyLightId, SpotLightHandle spotHandle);
    SectorEditorLightMutationResult ApplyLightDragToSnappedPosition(Vector3 snappedPosition);
    SectorEditorLightMutationResult FinishLightDrag();
    SectorEditorLightMutationResult CancelLightDragData(const char* message);

    SectorEditorLightMutationResult ApplyLightPilot(Vector3 position, Vector3 target);
    SectorEditorLightMutationResult CancelLightPilotData(const char* message);

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
    bool SetDynamicLightFlicker(SectorTopologyDynamicPointLight& light, bool flicker);
    bool SetDynamicLightFlickerSpeed(SectorTopologyDynamicPointLight& light, float flickerSpeed);
    bool SetDynamicLightFlickerAmount(SectorTopologyDynamicPointLight& light, float flickerAmount);
    bool SetDynamicLightPosition(SectorTopologyDynamicPointLight& light, Vector3 position);
    bool SetDynamicLightIntensity(SectorTopologyDynamicPointLight& light, float intensity);
    bool SetDynamicLightRadius(SectorTopologyDynamicPointLight& light, float radius);
    bool SetDynamicLightColor(SectorTopologyDynamicPointLight& light, Color color);
    bool SetDynamicLightAtmosphere(
            SectorTopologyDynamicPointLight& light,
            SectorLightAtmosphereSettings settings);

    bool SetDynamicSpotLightEnabled(SectorTopologyDynamicSpotLight& light, bool enabled);
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
    bool SetDynamicSpotLightIntensity(SectorTopologyDynamicSpotLight& light, float intensity);
    bool SetDynamicSpotLightRange(SectorTopologyDynamicSpotLight& light, float range);
    bool SetDynamicSpotLightInnerCone(SectorTopologyDynamicSpotLight& light, float innerConeDegrees);
    bool SetDynamicSpotLightOuterCone(SectorTopologyDynamicSpotLight& light, float outerConeDegrees);
    bool SetDynamicSpotLightColor(SectorTopologyDynamicSpotLight& light, Color color);
    bool SetDynamicSpotLightAtmosphere(
            SectorTopologyDynamicSpotLight& light,
            SectorLightAtmosphereSettings settings);

private:
    void MarkEdited(const char* status);
    bool FinishTopologyActionResult(const SectorEditorTopologyActionResult& result);

    SectorEditorLightEditingServiceContext context_;
};

} // namespace game
