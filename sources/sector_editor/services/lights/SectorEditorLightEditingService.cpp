#include "sector_editor/services/lights/SectorEditorLightEditingService.h"

#include "sector_editor/SectorEditorDirtyState.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <string_view>

namespace game {
namespace {

bool SameVector3(Vector3 lhs, Vector3 rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool SameColor(Color lhs, Color rhs)
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

template<typename Value>
bool SetValue(Value& target, Value value)
{
    if (target == value) {
        return false;
    }
    target = value;
    return true;
}

bool SetVector3(Vector3& target, Vector3 value)
{
    if (SameVector3(target, value)) {
        return false;
    }
    target = value;
    return true;
}

bool SetColorValue(Color& target, Color value)
{
    value.a = 255;
    if (SameColor(target, value)) {
        return false;
    }
    target = value;
    return true;
}

float ClampConeDegrees(float value)
{
    return std::clamp(value, 0.0f, 179.0f);
}

Vector3 TargetPointingDown(Vector3 position, Vector3 target)
{
    constexpr float MinimumUsableDistance = 0.0001f;
    float distance = Vector3Distance(position, target);
    if (!std::isfinite(distance) || distance <= MinimumUsableDistance) {
        distance = SectorWorldToAuthoringDistance(1.0f);
    }
    return Vector3{position.x, position.y - distance, position.z};
}

float AddNormalizedDegrees(float baseDegrees, float deltaDegrees)
{
    if (!std::isfinite(baseDegrees)) baseDegrees = 0.0f;
    if (!std::isfinite(deltaDegrees)) deltaDegrees = 0.0f;
    return std::remainder(baseDegrees + deltaDegrees, 360.0f);
}

SectorLightAtmosphereSettings* FindAtmosphere(
        SectorTopologyMap& map,
        LightPilotKind kind,
        int lightId)
{
    if (kind == LightPilotKind::StaticPoint) {
        SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(map, lightId);
        return light != nullptr ? &light->atmosphere : nullptr;
    }
    if (kind == LightPilotKind::StaticSpot) {
        SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(map, lightId);
        return light != nullptr ? &light->atmosphere : nullptr;
    }
    if (kind == LightPilotKind::DynamicPoint) {
        SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(map, lightId);
        return light != nullptr ? &light->atmosphere : nullptr;
    }
    if (kind == LightPilotKind::DynamicSpot) {
        SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(map, lightId);
        return light != nullptr ? &light->atmosphere : nullptr;
    }
    if (kind == LightPilotKind::StaticRect) {
        auto* light = FindSectorTopologyStaticRectLight(map, lightId);
        return light != nullptr ? &light->atmosphere : nullptr;
    }
    if (kind == LightPilotKind::DynamicRect) {
        auto* light = FindSectorTopologyDynamicRectLight(map, lightId);
        return light != nullptr ? &light->atmosphere : nullptr;
    }
    return nullptr;
}

Vector3* FindProxyOffset(
        SectorTopologyMap& map,
        LightProxyPlacementKind proxyKind,
        LightPilotKind lightKind,
        int lightId)
{
    SectorLightAtmosphereSettings* atmosphere = FindAtmosphere(map, lightKind, lightId);
    if (atmosphere == nullptr) return nullptr;
    if (proxyKind == LightProxyPlacementKind::Halo) {
        return atmosphere->proxy.halo.enabled
                ? &atmosphere->proxy.halo.centerOffsetWorld
                : nullptr;
    }
    const bool spotLight = lightKind == LightPilotKind::StaticSpot
            || lightKind == LightPilotKind::DynamicSpot
            || lightKind == LightPilotKind::StaticRect
            || lightKind == LightPilotKind::DynamicRect;
    if (proxyKind == LightProxyPlacementKind::Shaft
            && spotLight
            && atmosphere->proxy.shaft.enabled) {
        return &atmosphere->proxy.shaft.originOffsetWorld;
    }
    return nullptr;
}

const char* ProxyName(LightProxyPlacementKind kind)
{
    switch (kind) {
        case LightProxyPlacementKind::Halo: return "halo";
        case LightProxyPlacementKind::Shaft: return "shaft";
        case LightProxyPlacementKind::None: return "proxy";
    }
    return "proxy";
}

const char* LightName(LightPilotKind kind)
{
    switch (kind) {
        case LightPilotKind::StaticPoint: return "static light";
        case LightPilotKind::StaticSpot: return "static spot";
        case LightPilotKind::DynamicPoint: return "dynamic light";
        case LightPilotKind::DynamicSpot: return "dynamic spot";
        case LightPilotKind::StaticRect: return "static rect light";
        case LightPilotKind::DynamicRect: return "dynamic rect light";
        case LightPilotKind::None: return "light";
    }
    return "light";
}

void ResetLightInspectorUiState(SectorEditorLightEditingServiceContext::UiRefs& uiState)
{
    uiState.inspectorScroll.offset = Vector2{};
    uiState.lightXInput = engine::UIFloatInputState{};
    uiState.lightYInput = engine::UIFloatInputState{};
    uiState.lightZInput = engine::UIFloatInputState{};
    uiState.lightTargetXInput = engine::UIFloatInputState{};
    uiState.lightTargetYInput = engine::UIFloatInputState{};
    uiState.lightTargetZInput = engine::UIFloatInputState{};
    uiState.lightIntensityInput = engine::UIFloatInputState{};
    uiState.lightRadiusInput = engine::UIFloatInputState{};
    uiState.lightInnerConeInput = engine::UIFloatInputState{};
    uiState.lightOuterConeInput = engine::UIFloatInputState{};
    uiState.lightSourceRadiusInput = engine::UIFloatInputState{};
    uiState.lightFlickerSpeedInput = engine::UIFloatInputState{};
    uiState.lightFlickerAmountInput = engine::UIFloatInputState{};
    uiState.lightShadowPriorityInput = engine::UIIntInputState{};
    uiState.lightShadowBiasInput = engine::UIFloatInputState{};
    uiState.lightShadowStrengthInput = engine::UIFloatInputState{};
    uiState.lightShadowSoftnessInput = engine::UIFloatInputState{};
    const auto resetFloat = [](engine::UIFloatInputState* state) {
        if (state != nullptr) *state = engine::UIFloatInputState{};
    };
    const auto resetInt = [](engine::UIIntInputState* state) {
        if (state != nullptr) *state = engine::UIIntInputState{};
    };
    resetFloat(uiState.atmosphere.proxyHaloRadiusInput);
    resetFloat(uiState.atmosphere.proxyHaloOffsetXInput);
    resetFloat(uiState.atmosphere.proxyHaloOffsetYInput);
    resetFloat(uiState.atmosphere.proxyHaloOffsetZInput);
    resetFloat(uiState.atmosphere.proxyHaloBrightnessInput);
    resetFloat(uiState.atmosphere.proxyHaloMaxExtinctionInput);
    resetFloat(uiState.atmosphere.proxyHaloSoftnessInput);
    resetInt(uiState.atmosphere.proxyHaloRedInput);
    resetInt(uiState.atmosphere.proxyHaloGreenInput);
    resetInt(uiState.atmosphere.proxyHaloBlueInput);
    resetFloat(uiState.atmosphere.proxyShaftOffsetXInput);
    resetFloat(uiState.atmosphere.proxyShaftOffsetYInput);
    resetFloat(uiState.atmosphere.proxyShaftOffsetZInput);
    resetFloat(uiState.atmosphere.proxyShaftLengthInput);
    resetFloat(uiState.atmosphere.proxyShaftWidthInput);
    resetFloat(uiState.atmosphere.proxyShaftBrightnessInput);
    resetFloat(uiState.atmosphere.proxyShaftMaxExtinctionInput);
    resetFloat(uiState.atmosphere.proxyShaftSoftnessInput);
    resetInt(uiState.atmosphere.proxyShaftRedInput);
    resetInt(uiState.atmosphere.proxyShaftGreenInput);
    resetInt(uiState.atmosphere.proxyShaftBlueInput);
    resetInt(uiState.atmosphere.dustAmountInput);
    resetFloat(uiState.atmosphere.dustExtentScaleInput);
    resetFloat(uiState.atmosphere.dustMinimumSizeInput);
    resetFloat(uiState.atmosphere.dustMaximumSizeInput);
    resetFloat(uiState.atmosphere.dustOpacityInput);
    resetFloat(uiState.atmosphere.dustDriftSpeedInput);
    resetFloat(uiState.atmosphere.dustTurbulenceInput);
    resetInt(uiState.atmosphere.dustRedInput);
    resetInt(uiState.atmosphere.dustGreenInput);
    resetInt(uiState.atmosphere.dustBlueInput);
    uiState.lightRedInput = engine::UIIntInputState{};
    uiState.lightGreenInput = engine::UIIntInputState{};
    uiState.lightBlueInput = engine::UIIntInputState{};
}

void ClearLightSelection(
        SectorEditorLightEditingServiceContext::SelectionRefs& state,
        SectorEditorLightEditingServiceContext::UiRefs& uiState)
{
    state.manipulationState.selectDragArm = SelectDragArmState{};
    state.manipulationState.authoringVertexDrag = AuthoringVertexDragState{};
    state.runtimeObjectDrag = RuntimeObjectDragState{};
    state.topologySelectionKind = TopologySelectionKind::None;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologyLightId = -1;
    state.selectedTopologyStaticSpotLightId = -1;
    state.selectedTopologyDynamicLightId = -1;
    state.selectedTopologyDynamicSpotLightId = -1;
    state.selectedRuntimeObjectId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    state.selectedAuthoring = SectorAuthoringSelectionTarget{};
    ResetLightInspectorUiState(uiState);
    uiState.inspectorIdUiState.idBufferSectorIndex = -1;
    uiState.inspectorIdUiState.idBufferLightIndex = -1;
    uiState.inspectorIdUiState.selectedSectorIdBuffer[0] = '\0';
    uiState.inspectorIdUiState.selectedLightIdBuffer[0] = '\0';
}

void SelectLight(
        SectorEditorLightEditingServiceContext::SelectionRefs& state,
        SectorEditorLightEditingServiceContext::UiRefs& uiState,
        TopologySelectionKind kind,
        int lightId)
{
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedRuntimeObjectId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    state.selectedAuthoring = SectorAuthoringSelectionTarget{};
    ResetLightInspectorUiState(uiState);

    state.topologySelectionKind = kind;
    state.selectedTopologyLightId = kind == TopologySelectionKind::StaticLight ? lightId : -1;
    state.selectedTopologyStaticSpotLightId = (kind == TopologySelectionKind::StaticSpotLight
            || kind == TopologySelectionKind::StaticRectLight) ? lightId : -1;
    state.selectedTopologyDynamicLightId = kind == TopologySelectionKind::DynamicLight ? lightId : -1;
    state.selectedTopologyDynamicSpotLightId = (kind == TopologySelectionKind::DynamicSpotLight
            || kind == TopologySelectionKind::DynamicRectLight) ? lightId : -1;
    std::snprintf(uiState.inspectorIdUiState.selectedLightIdBuffer, sizeof(uiState.inspectorIdUiState.selectedLightIdBuffer), "%d", lightId);
    uiState.inspectorIdUiState.idBufferLightIndex = lightId;
    uiState.inspectorIdUiState.idBufferSectorIndex = -1;
    uiState.inspectorIdUiState.selectedSectorIdBuffer[0] = '\0';
    uiState.inspectorIdUiState.idEditError.clear();
}

SectorEditorLightMutationResult FinishLightMutationResult(bool changed)
{
    SectorEditorLightMutationResult result;
    result.changed = changed;
    result.dynamicLightRendererRefreshNeeded = changed;
    return result;
}

bool SameAtmosphere(
        const SectorLightAtmosphereSettings& leftSource,
        const SectorLightAtmosphereSettings& rightSource)
{
    const SectorLightAtmosphereSettings left = NormalizeSectorLightAtmosphereSettings(leftSource);
    const SectorLightAtmosphereSettings right = NormalizeSectorLightAtmosphereSettings(rightSource);
    const auto sameColor = [](Color a, Color b) {
        return a.r == b.r && a.g == b.g && a.b == b.b;
    };
    return left.proxy.halo.enabled == right.proxy.halo.enabled
            && left.proxy.halo.radiusWorld == right.proxy.halo.radiusWorld
            && left.proxy.halo.centerOffsetWorld.x == right.proxy.halo.centerOffsetWorld.x
            && left.proxy.halo.centerOffsetWorld.y == right.proxy.halo.centerOffsetWorld.y
            && left.proxy.halo.centerOffsetWorld.z == right.proxy.halo.centerOffsetWorld.z
            && left.proxy.halo.brightness == right.proxy.halo.brightness
            && left.proxy.halo.maxExtinction == right.proxy.halo.maxExtinction
            && left.proxy.halo.edgeSoftness == right.proxy.halo.edgeSoftness
            && sameColor(left.proxy.halo.scatteringTint, right.proxy.halo.scatteringTint)
            && left.proxy.shaft.enabled == right.proxy.shaft.enabled
            && left.proxy.shaft.originOffsetWorld.x == right.proxy.shaft.originOffsetWorld.x
            && left.proxy.shaft.originOffsetWorld.y == right.proxy.shaft.originOffsetWorld.y
            && left.proxy.shaft.originOffsetWorld.z == right.proxy.shaft.originOffsetWorld.z
            && left.proxy.shaft.lengthScale == right.proxy.shaft.lengthScale
            && left.proxy.shaft.widthScale == right.proxy.shaft.widthScale
            && left.proxy.shaft.brightness == right.proxy.shaft.brightness
            && left.proxy.shaft.maxExtinction == right.proxy.shaft.maxExtinction
            && left.proxy.shaft.edgeSoftness == right.proxy.shaft.edgeSoftness
            && sameColor(left.proxy.shaft.scatteringTint, right.proxy.shaft.scatteringTint)
            && left.dust.enabled == right.dust.enabled
            && left.dust.amount == right.dust.amount
            && left.dust.extentScale == right.dust.extentScale
            && left.dust.minimumSizeWorld == right.dust.minimumSizeWorld
            && left.dust.maximumSizeWorld == right.dust.maximumSizeWorld
            && left.dust.opacity == right.dust.opacity
            && left.dust.driftSpeedWorld == right.dust.driftSpeedWorld
            && left.dust.turbulenceWorld == right.dust.turbulenceWorld
            && sameColor(left.dust.scatteringTint, right.dust.scatteringTint);
}

SectorTopologyDynamicPointLight ToDynamicLight(
        const SectorTopologyStaticPointLight& source,
        int id)
{
    SectorTopologyDynamicPointLight result;
    result.id = id;
    result.position = source.position;
    result.color = source.color;
    result.intensity = source.intensity;
    result.radius = source.radius;
    result.atmosphere = source.atmosphere;
    result.castsShadow = source.castsShadow;
    return result;
}

SectorTopologyStaticPointLight ToStaticLight(
        const SectorTopologyDynamicPointLight& source,
        int id)
{
    SectorTopologyStaticPointLight result;
    result.id = id;
    result.position = source.position;
    result.color = source.color;
    result.intensity = source.intensity;
    result.radius = source.radius;
    result.atmosphere = source.atmosphere;
    result.castsShadow = source.castsShadow;
    return result;
}

SectorTopologyDynamicSpotLight ToDynamicSpotLight(
        const SectorTopologyStaticSpotLight& source,
        int id)
{
    SectorTopologyDynamicSpotLight result;
    result.id = id;
    result.position = source.position;
    result.target = source.target;
    result.color = source.color;
    result.intensity = source.intensity;
    result.range = source.range;
    result.innerConeDegrees = source.innerConeDegrees;
    result.outerConeDegrees = source.outerConeDegrees;
    result.castsShadow = source.castsShadow;
    result.atmosphere = source.atmosphere;
    return result;
}

SectorTopologyStaticSpotLight ToStaticSpotLight(
        const SectorTopologyDynamicSpotLight& source,
        int id)
{
    SectorTopologyStaticSpotLight result;
    result.id = id;
    result.position = source.position;
    result.target = source.target;
    result.color = source.color;
    result.intensity = source.intensity;
    result.range = source.range;
    result.innerConeDegrees = source.innerConeDegrees;
    result.outerConeDegrees = source.outerConeDegrees;
    result.atmosphere = source.atmosphere;
    result.castsShadow = source.castsShadow;
    return result;
}

SectorTopologyDynamicRectLight ToDynamicRectLight(
        const SectorTopologyStaticRectLight& source,
        int id)
{
    SectorTopologyDynamicRectLight result;
    result.id = id;
    result.position = source.position;
    result.target = source.target;
    result.rollDegrees = source.rollDegrees;
    result.width = source.width;
    result.height = source.height;
    result.color = source.color;
    result.intensity = source.intensity;
    result.range = source.range;
    result.castsShadow = source.castsShadow;
    result.atmosphere = source.atmosphere;
    return result;
}

SectorTopologyStaticRectLight ToStaticRectLight(
        const SectorTopologyDynamicRectLight& source,
        int id)
{
    SectorTopologyStaticRectLight result;
    result.id = id;
    result.position = source.position;
    result.target = source.target;
    result.rollDegrees = source.rollDegrees;
    result.width = source.width;
    result.height = source.height;
    result.color = source.color;
    result.intensity = source.intensity;
    result.range = source.range;
    result.atmosphere = source.atmosphere;
    result.castsShadow = source.castsShadow;
    return result;
}

LightPilotKind PilotKindForSelection(TopologySelectionKind kind)
{
    switch (kind) {
        case TopologySelectionKind::StaticLight: return LightPilotKind::StaticPoint;
        case TopologySelectionKind::StaticSpotLight: return LightPilotKind::StaticSpot;
        case TopologySelectionKind::StaticRectLight: return LightPilotKind::StaticRect;
        case TopologySelectionKind::DynamicLight: return LightPilotKind::DynamicPoint;
        case TopologySelectionKind::DynamicSpotLight: return LightPilotKind::DynamicSpot;
        case TopologySelectionKind::DynamicRectLight: return LightPilotKind::DynamicRect;
        default: return LightPilotKind::None;
    }
}

} // namespace

SectorEditorLightEditingService::SectorEditorLightEditingService(
        SectorEditorLightEditingServiceContext context)
    : context_(std::move(context))
{
}

void SectorEditorLightEditingService::MarkEdited(const char* status)
{
    context_.lifecycle.topologyDocumentDirty = true;
    context_.lifecycle.hasUnsavedChanges = true;
    ++context_.topologyRenderRevision;
    context_.topologyRenderCache.valid = false;
    if (status != nullptr && status[0] != '\0') {
        context_.statusText = status;
    }
}

bool SectorEditorLightEditingService::FinishTopologyActionResult(
        const SectorEditorTopologyActionResult& result)
{
    if (!result.changed) {
        if (!result.status.empty()) {
            context_.statusText = result.status;
        }
        return false;
    }

    MarkEdited(result.status.c_str());
    return true;
}

SectorEditorLightMutationResult SectorEditorLightEditingService::AddStaticLight(
        int sectorId,
        Vector2 mapPoint)
{
    const SectorEditorAddStaticLightResult add = AddStaticLightToSector(
            context_.map,
            sectorId,
            mapPoint);
    if (!add.changed) {
        if (!add.status.empty()) {
            context_.statusText = add.status;
        }
        return {};
    }

    SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticLight, add.lightId);
    FinishTopologyActionResult(SectorEditorTopologyActionResult{true, add.status});
    return FinishLightMutationResult(true);
}

SectorEditorLightMutationResult SectorEditorLightEditingService::AddStaticSpotLight(
        int sectorId,
        Vector2 mapPoint)
{
    const SectorEditorAddStaticSpotLightResult add = AddStaticSpotLightToSector(
            context_.map,
            sectorId,
            mapPoint);
    if (!add.changed) {
        if (!add.status.empty()) {
            context_.statusText = add.status;
        }
        return {};
    }

    SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticSpotLight, add.lightId);
    FinishTopologyActionResult(SectorEditorTopologyActionResult{true, add.status});
    return FinishLightMutationResult(true);
}

SectorEditorLightMutationResult SectorEditorLightEditingService::AddDynamicLight(
        int sectorId,
        Vector2 mapPoint)
{
    const SectorEditorAddDynamicLightResult add = AddDynamicLightToSector(
            context_.map,
            sectorId,
            mapPoint);
    if (!add.changed) {
        if (!add.status.empty()) {
            context_.statusText = add.status;
        }
        return {};
    }

    SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicLight, add.lightId);
    FinishTopologyActionResult(SectorEditorTopologyActionResult{true, add.status});
    return FinishLightMutationResult(true);
}

SectorEditorLightMutationResult SectorEditorLightEditingService::AddDynamicSpotLight(
        int sectorId,
        Vector2 mapPoint)
{
    const SectorEditorAddDynamicSpotLightResult add = AddDynamicSpotLightToSector(
            context_.map,
            sectorId,
            mapPoint);
    if (!add.changed) {
        if (!add.status.empty()) {
            context_.statusText = add.status;
        }
        return {};
    }

    SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicSpotLight, add.lightId);
    FinishTopologyActionResult(SectorEditorTopologyActionResult{true, add.status});
    return FinishLightMutationResult(true);
}

SectorEditorLightMutationResult SectorEditorLightEditingService::AddStaticRectLight(
        int sectorId, Vector2 mapPoint)
{
    const auto add = AddStaticRectLightToSector(context_.map, sectorId, mapPoint);
    if (!add.changed) { context_.statusText = add.status; return {}; }
    SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticRectLight, add.lightId);
    FinishTopologyActionResult({true, add.status});
    return FinishLightMutationResult(true);
}

SectorEditorLightMutationResult SectorEditorLightEditingService::AddDynamicRectLight(
        int sectorId, Vector2 mapPoint)
{
    const auto add = AddDynamicRectLightToSector(context_.map, sectorId, mapPoint);
    if (!add.changed) { context_.statusText = add.status; return {}; }
    SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicRectLight, add.lightId);
    FinishTopologyActionResult({true, add.status});
    return FinishLightMutationResult(true);
}

SectorEditorLightMutationResult SectorEditorLightEditingService::ConvertSelectedLight()
{
    const TopologySelectionKind sourceKind = context_.selection.topologySelectionKind;
    int sourceId = -1;
    TopologySelectionKind destinationKind = TopologySelectionKind::None;
    int destinationId = -1;

    if (sourceKind == TopologySelectionKind::StaticLight
            && context_.selection.selectedTopologyLightId >= 0) {
        sourceId = context_.selection.selectedTopologyLightId;
        destinationKind = TopologySelectionKind::DynamicLight;
        if (FindSectorTopologyStaticLight(context_.map, sourceId) != nullptr) {
            destinationId = FindSectorTopologyDynamicLight(context_.map, sourceId) == nullptr
                    ? sourceId
                    : AllocateSectorTopologyDynamicLightId(context_.map);
        }
    } else if (sourceKind == TopologySelectionKind::DynamicLight
            && context_.selection.selectedTopologyDynamicLightId >= 0) {
        sourceId = context_.selection.selectedTopologyDynamicLightId;
        destinationKind = TopologySelectionKind::StaticLight;
        if (FindSectorTopologyDynamicLight(context_.map, sourceId) != nullptr) {
            destinationId = FindSectorTopologyStaticLight(context_.map, sourceId) == nullptr
                    ? sourceId
                    : AllocateSectorTopologyStaticLightId(context_.map);
        }
    } else if (sourceKind == TopologySelectionKind::StaticSpotLight
            && context_.selection.selectedTopologyStaticSpotLightId >= 0) {
        sourceId = context_.selection.selectedTopologyStaticSpotLightId;
        destinationKind = TopologySelectionKind::DynamicSpotLight;
        if (FindSectorTopologyStaticSpotLight(context_.map, sourceId) != nullptr) {
            destinationId = FindSectorTopologyDynamicSpotLight(context_.map, sourceId) == nullptr
                    ? sourceId
                    : AllocateSectorTopologyDynamicSpotLightId(context_.map);
        }
    } else if (sourceKind == TopologySelectionKind::DynamicSpotLight
            && context_.selection.selectedTopologyDynamicSpotLightId >= 0) {
        sourceId = context_.selection.selectedTopologyDynamicSpotLightId;
        destinationKind = TopologySelectionKind::StaticSpotLight;
        if (FindSectorTopologyDynamicSpotLight(context_.map, sourceId) != nullptr) {
            destinationId = FindSectorTopologyStaticSpotLight(context_.map, sourceId) == nullptr
                    ? sourceId
                    : AllocateSectorTopologyStaticSpotLightId(context_.map);
        }
    } else if (sourceKind == TopologySelectionKind::StaticRectLight
            && context_.selection.selectedTopologyStaticSpotLightId >= 0) {
        sourceId = context_.selection.selectedTopologyStaticSpotLightId;
        destinationKind = TopologySelectionKind::DynamicRectLight;
        if (FindSectorTopologyStaticRectLight(context_.map, sourceId) != nullptr) {
            destinationId = FindSectorTopologyDynamicRectLight(context_.map, sourceId) == nullptr
                    ? sourceId
                    : AllocateSectorTopologyDynamicRectLightId(context_.map);
        }
    } else if (sourceKind == TopologySelectionKind::DynamicRectLight
            && context_.selection.selectedTopologyDynamicSpotLightId >= 0) {
        sourceId = context_.selection.selectedTopologyDynamicSpotLightId;
        destinationKind = TopologySelectionKind::StaticRectLight;
        if (FindSectorTopologyDynamicRectLight(context_.map, sourceId) != nullptr) {
            destinationId = FindSectorTopologyStaticRectLight(context_.map, sourceId) == nullptr
                    ? sourceId
                    : AllocateSectorTopologyStaticRectLightId(context_.map);
        }
    }

    if (!IsValidSectorTopologyId(destinationId)) {
        context_.statusText = sourceId < 0
                ? "Light conversion failed: no light selected"
                : TextFormat("Light conversion failed for light %d", sourceId);
        return {};
    }

    SectorEditorLightMutationResult result;
    const LightPilotKind sourcePilotKind = PilotKindForSelection(sourceKind);
    if (context_.lightState.lightPilot.active
            && context_.lightState.lightPilot.kind == sourcePilotKind
            && context_.lightState.lightPilot.lightId == sourceId) {
        result = CancelLightPilotData(nullptr);
    }
    if (context_.lightState.proxyPlacement.active
            && context_.lightState.proxyPlacement.kind == sourcePilotKind
            && context_.lightState.proxyPlacement.lightId == sourceId) {
        const SectorEditorLightMutationResult cancel = CancelProxyPlacementData(nullptr);
        result.dynamicLightRendererRefreshNeeded =
                result.dynamicLightRendererRefreshNeeded || cancel.dynamicLightRendererRefreshNeeded;
    }
    if (context_.lightState.lightEdit.active
            && context_.lightState.lightEdit.kind == sourceKind
            && context_.lightState.lightEdit.topologyLightId == sourceId) {
        CancelLightDragData(nullptr);
    }
    if (context_.lightState.lightDrag.topologyLightId == sourceId) {
        context_.lightState.lightDrag = LightDragState{};
    }

    if (sourceKind == TopologySelectionKind::StaticLight) {
        const SectorTopologyStaticPointLight source =
                *FindSectorTopologyStaticLight(context_.map, sourceId);
        SectorTopologyDynamicPointLight converted = ToDynamicLight(source, destinationId);
        converted.instanceId = AllocateSectorDynamicLightInstanceId(
                context_.map, "point", destinationId);
        context_.map.dynamicPointLights.push_back(std::move(converted));
        RemoveSectorTopologyStaticLight(context_.map, sourceId);
    } else if (sourceKind == TopologySelectionKind::DynamicLight) {
        const SectorTopologyDynamicPointLight source =
                *FindSectorTopologyDynamicLight(context_.map, sourceId);
        context_.map.staticLights.push_back(ToStaticLight(source, destinationId));
        RemoveSectorTopologyDynamicLight(context_.map, sourceId);
    } else if (sourceKind == TopologySelectionKind::StaticSpotLight) {
        const SectorTopologyStaticSpotLight source =
                *FindSectorTopologyStaticSpotLight(context_.map, sourceId);
        SectorTopologyDynamicSpotLight converted = ToDynamicSpotLight(source, destinationId);
        converted.instanceId = AllocateSectorDynamicLightInstanceId(
                context_.map, "spot", destinationId);
        context_.map.dynamicSpotLights.push_back(std::move(converted));
        RemoveSectorTopologyStaticSpotLight(context_.map, sourceId);
    } else if (sourceKind == TopologySelectionKind::DynamicSpotLight) {
        const SectorTopologyDynamicSpotLight source =
                *FindSectorTopologyDynamicSpotLight(context_.map, sourceId);
        context_.map.staticSpotLights.push_back(ToStaticSpotLight(source, destinationId));
        RemoveSectorTopologyDynamicSpotLight(context_.map, sourceId);
    } else if (sourceKind == TopologySelectionKind::StaticRectLight) {
        const SectorTopologyStaticRectLight source =
                *FindSectorTopologyStaticRectLight(context_.map, sourceId);
        SectorTopologyDynamicRectLight converted = ToDynamicRectLight(source, destinationId);
        converted.instanceId = AllocateSectorDynamicLightInstanceId(
                context_.map, "rect", destinationId);
        context_.map.dynamicRectLights.push_back(std::move(converted));
        RemoveSectorTopologyStaticRectLight(context_.map, sourceId);
    } else {
        const SectorTopologyDynamicRectLight source =
                *FindSectorTopologyDynamicRectLight(context_.map, sourceId);
        context_.map.staticRectLights.push_back(ToStaticRectLight(source, destinationId));
        RemoveSectorTopologyDynamicRectLight(context_.map, sourceId);
    }

    if (context_.selection.hoveredTopologyLightId == sourceId) {
        context_.selection.hoveredTopologyLightId = -1;
    }
    if (context_.selection.hoveredTopologyStaticSpotLightId == sourceId) {
        context_.selection.hoveredTopologyStaticSpotLightId = -1;
    }
    if (context_.selection.hoveredTopologyDynamicLightId == sourceId) {
        context_.selection.hoveredTopologyDynamicLightId = -1;
    }
    if (context_.selection.hoveredTopologyDynamicSpotLightId == sourceId) {
        context_.selection.hoveredTopologyDynamicSpotLightId = -1;
    }
    SelectLight(context_.selection, context_.ui, destinationKind, destinationId);
    MarkEdited(TextFormat(
            "Converted %s %d to %s %d",
            LightName(sourcePilotKind),
            sourceId,
            LightName(PilotKindForSelection(destinationKind)),
            destinationId));
    result.changed = true;
    result.dynamicLightRendererRefreshNeeded = true;
    return result;
}

SectorEditorLightMutationResult SectorEditorLightEditingService::DeleteSelectedLightConfirmed()
{
    SectorEditorTopologyActionResult deleteResult;
    int lightId = -1;
    TopologySelectionKind kind = context_.selection.topologySelectionKind;

    if (kind == TopologySelectionKind::StaticLight && context_.selection.selectedTopologyLightId >= 0) {
        lightId = context_.selection.selectedTopologyLightId;
        deleteResult = DeleteStaticLight(context_.map, lightId);
    } else if (kind == TopologySelectionKind::StaticSpotLight
            && context_.selection.selectedTopologyStaticSpotLightId >= 0) {
        lightId = context_.selection.selectedTopologyStaticSpotLightId;
        deleteResult = DeleteStaticSpotLight(context_.map, lightId);
    } else if (kind == TopologySelectionKind::DynamicLight
            && context_.selection.selectedTopologyDynamicLightId >= 0) {
        lightId = context_.selection.selectedTopologyDynamicLightId;
        deleteResult = DeleteDynamicLight(context_.map, lightId);
    } else if (kind == TopologySelectionKind::DynamicSpotLight
            && context_.selection.selectedTopologyDynamicSpotLightId >= 0) {
        lightId = context_.selection.selectedTopologyDynamicSpotLightId;
        deleteResult = DeleteDynamicSpotLight(context_.map, lightId);
    } else if (kind == TopologySelectionKind::StaticRectLight
            && context_.selection.selectedTopologyStaticSpotLightId >= 0) {
        lightId = context_.selection.selectedTopologyStaticSpotLightId;
        deleteResult = DeleteStaticRectLight(context_.map, lightId);
    } else if (kind == TopologySelectionKind::DynamicRectLight
            && context_.selection.selectedTopologyDynamicSpotLightId >= 0) {
        lightId = context_.selection.selectedTopologyDynamicSpotLightId;
        deleteResult = DeleteDynamicRectLight(context_.map, lightId);
    } else {
        return {};
    }

    SectorEditorLightMutationResult result;
    if (!deleteResult.changed) {
        FinishTopologyActionResult(deleteResult);
        return result;
    }

    if ((kind == TopologySelectionKind::StaticLight && context_.selection.selectedTopologyLightId == lightId)
            || (kind == TopologySelectionKind::StaticSpotLight
                    && context_.selection.selectedTopologyStaticSpotLightId == lightId)
            || (kind == TopologySelectionKind::DynamicLight
                    && context_.selection.selectedTopologyDynamicLightId == lightId)
            || (kind == TopologySelectionKind::DynamicSpotLight
                    && context_.selection.selectedTopologyDynamicSpotLightId == lightId)
            || (kind == TopologySelectionKind::StaticRectLight
                    && context_.selection.selectedTopologyStaticSpotLightId == lightId)
            || (kind == TopologySelectionKind::DynamicRectLight
                    && context_.selection.selectedTopologyDynamicSpotLightId == lightId)) {
        const LightPilotKind pilotKind = kind == TopologySelectionKind::StaticLight
                ? LightPilotKind::StaticPoint
                : (kind == TopologySelectionKind::StaticSpotLight
                        ? LightPilotKind::StaticSpot
                        : (kind == TopologySelectionKind::DynamicLight
                                ? LightPilotKind::DynamicPoint
                                : (kind == TopologySelectionKind::DynamicSpotLight
                                        ? LightPilotKind::DynamicSpot
                                        : (kind == TopologySelectionKind::StaticRectLight
                                                ? LightPilotKind::StaticRect
                                                : LightPilotKind::DynamicRect))));
        if (context_.lightState.lightPilot.active
                && context_.lightState.lightPilot.kind == pilotKind
                && context_.lightState.lightPilot.lightId == lightId) {
            result.restoredLightPilot = context_.lightState.lightPilot;
            result.previewPoseRestoreNeeded = true;
            context_.lightState.lightPilot = LightPilotLightState{};
        }
        if (context_.lightState.proxyPlacement.active
                && context_.lightState.proxyPlacement.kind == pilotKind
                && context_.lightState.proxyPlacement.lightId == lightId) {
            context_.lightState.proxyPlacement = LightProxyPlacementState{};
        }
        ClearLightSelection(context_.selection, context_.ui);
    }

    if (context_.selection.hoveredTopologyLightId == lightId) {
        context_.selection.hoveredTopologyLightId = -1;
    }
    if (context_.selection.hoveredTopologyStaticSpotLightId == lightId) {
        context_.selection.hoveredTopologyStaticSpotLightId = -1;
    }
    if (context_.selection.hoveredTopologyDynamicLightId == lightId) {
        context_.selection.hoveredTopologyDynamicLightId = -1;
    }
    if (context_.selection.hoveredTopologyDynamicSpotLightId == lightId) {
        context_.selection.hoveredTopologyDynamicSpotLightId = -1;
    }
    if (context_.lightState.lightDrag.topologyLightId == lightId) {
        context_.lightState.lightDrag = LightDragState{};
    }
    if (context_.lightState.lightEdit.topologyLightId == lightId) {
        context_.lightState.lightEdit = LightEditTransactionState{};
    }

    result.changed = FinishTopologyActionResult(deleteResult);
    result.dynamicLightRendererRefreshNeeded = result.changed;
    return result;
}

bool SectorEditorLightEditingService::BeginLightDrag(
        TopologySelectionKind kind,
        int topologyLightId,
        SpotLightHandle spotHandle)
{
    LightEditTransactionState edit;
    edit.active = true;
    edit.kind = kind;
    edit.topologyLightId = topologyLightId;
    edit.spotHandle = spotHandle;

    if (kind == TopologySelectionKind::StaticSpotLight) {
        const SectorTopologyStaticSpotLight* light =
                FindSectorTopologyStaticSpotLight(context_.map, topologyLightId);
        if (light == nullptr) {
            return false;
        }
        edit.originalPosition = light->position;
        edit.originalTarget = light->target;
        context_.statusText = spotHandle == SpotLightHandle::Target
                ? TextFormat("Aiming static spot %d", light->id)
                : TextFormat("Moving static spot %d", light->id);
    } else if (kind == TopologySelectionKind::DynamicSpotLight) {
        const SectorTopologyDynamicSpotLight* light =
                FindSectorTopologyDynamicSpotLight(context_.map, topologyLightId);
        if (light == nullptr) {
            return false;
        }
        edit.originalPosition = light->position;
        edit.originalTarget = light->target;
        context_.statusText = spotHandle == SpotLightHandle::Target
                ? TextFormat("Aiming dynamic spot %d", light->id)
                : TextFormat("Moving dynamic spot %d", light->id);
    } else if (kind == TopologySelectionKind::StaticRectLight) {
        const auto* light = FindSectorTopologyStaticRectLight(context_.map, topologyLightId);
        if (light == nullptr) return false;
        edit.originalPosition = light->position;
        edit.originalTarget = light->target;
        context_.statusText = TextFormat("Moving static rect light %d", light->id);
    } else if (kind == TopologySelectionKind::DynamicRectLight) {
        const auto* light = FindSectorTopologyDynamicRectLight(context_.map, topologyLightId);
        if (light == nullptr) return false;
        edit.originalPosition = light->position;
        edit.originalTarget = light->target;
        context_.statusText = TextFormat("Moving dynamic rect light %d", light->id);
    } else if (kind == TopologySelectionKind::DynamicLight) {
        const SectorTopologyDynamicPointLight* light =
                FindSectorTopologyDynamicLight(context_.map, topologyLightId);
        if (light == nullptr) {
            return false;
        }
        edit.originalPosition = light->position;
        context_.statusText = TextFormat("Moving dynamic light %d", light->id);
    } else {
        const SectorTopologyStaticPointLight* light =
                FindSectorTopologyStaticLight(context_.map, topologyLightId);
        if (light == nullptr) {
            return false;
        }
        edit.kind = TopologySelectionKind::StaticLight;
        edit.originalPosition = light->position;
        context_.statusText = TextFormat("Moving static light %d", light->id);
    }

    context_.lightState.lightEdit = edit;
    return true;
}

SectorEditorLightMutationResult SectorEditorLightEditingService::ApplyLightDragToSnappedPosition(
        Vector3 snappedPosition)
{
    const LightEditTransactionState& edit = context_.lightState.lightEdit;
    if (!edit.active) {
        return {};
    }

    if (edit.kind == TopologySelectionKind::DynamicLight) {
        SectorTopologyDynamicPointLight* light =
                FindSectorTopologyDynamicLight(context_.map, edit.topologyLightId);
        if (light == nullptr) {
            return {};
        }
        light->position.x = snappedPosition.x;
        light->position.y = edit.originalPosition.y;
        light->position.z = snappedPosition.z;
        context_.statusText = TextFormat("Moving dynamic light %d", light->id);
        return {};
    }

    if (edit.kind == TopologySelectionKind::StaticRectLight
            || edit.kind == TopologySelectionKind::DynamicRectLight) {
        Vector3* position = nullptr;
        Vector3* target = nullptr;
        if (edit.kind == TopologySelectionKind::StaticRectLight) {
            if (auto* light = FindSectorTopologyStaticRectLight(context_.map, edit.topologyLightId)) {
                position = &light->position; target = &light->target;
            }
        } else if (auto* light = FindSectorTopologyDynamicRectLight(context_.map, edit.topologyLightId)) {
            position = &light->position; target = &light->target;
        }
        if (position == nullptr || target == nullptr) return {};
        if (edit.spotHandle == SpotLightHandle::Target) {
            target->x = snappedPosition.x; target->z = snappedPosition.z;
        } else {
            const float dx = snappedPosition.x - edit.originalPosition.x;
            const float dz = snappedPosition.z - edit.originalPosition.z;
            position->x = snappedPosition.x; position->z = snappedPosition.z;
            target->x = edit.originalTarget.x + dx; target->z = edit.originalTarget.z + dz;
        }
        return {};
    }

    if (edit.kind == TopologySelectionKind::DynamicSpotLight) {
        SectorTopologyDynamicSpotLight* light =
                FindSectorTopologyDynamicSpotLight(context_.map, edit.topologyLightId);
        if (light == nullptr) {
            return {};
        }
        if (edit.spotHandle == SpotLightHandle::Target) {
            light->target.x = snappedPosition.x;
            light->target.y = edit.originalTarget.y;
            light->target.z = snappedPosition.z;
            context_.statusText = TextFormat("Aiming dynamic spot %d", light->id);
            return {};
        }
        const float dx = snappedPosition.x - edit.originalPosition.x;
        const float dz = snappedPosition.z - edit.originalPosition.z;
        light->position.x = snappedPosition.x;
        light->position.y = edit.originalPosition.y;
        light->position.z = snappedPosition.z;
        light->target.x = edit.originalTarget.x + dx;
        light->target.y = edit.originalTarget.y;
        light->target.z = edit.originalTarget.z + dz;
        context_.statusText = TextFormat("Moving dynamic spot %d", light->id);
        return {};
    }

    if (edit.kind == TopologySelectionKind::StaticSpotLight) {
        SectorTopologyStaticSpotLight* light =
                FindSectorTopologyStaticSpotLight(context_.map, edit.topologyLightId);
        if (light == nullptr) {
            return {};
        }
        if (edit.spotHandle == SpotLightHandle::Target) {
            light->target.x = snappedPosition.x;
            light->target.y = edit.originalTarget.y;
            light->target.z = snappedPosition.z;
            context_.statusText = TextFormat("Aiming static spot %d", light->id);
            return {};
        }
        const float dx = snappedPosition.x - edit.originalPosition.x;
        const float dz = snappedPosition.z - edit.originalPosition.z;
        light->position.x = snappedPosition.x;
        light->position.y = edit.originalPosition.y;
        light->position.z = snappedPosition.z;
        light->target.x = edit.originalTarget.x + dx;
        light->target.y = edit.originalTarget.y;
        light->target.z = edit.originalTarget.z + dz;
        context_.statusText = TextFormat("Moving static spot %d", light->id);
        return {};
    }

    SectorTopologyStaticPointLight* light =
            FindSectorTopologyStaticLight(context_.map, edit.topologyLightId);
    if (light == nullptr) {
        return {};
    }
    light->position.x = snappedPosition.x;
    light->position.y = edit.originalPosition.y;
    light->position.z = snappedPosition.z;
    context_.statusText = TextFormat("Moving static light %d", light->id);
    return {};
}

SectorEditorLightMutationResult SectorEditorLightEditingService::FinishLightDrag()
{
    const LightEditTransactionState edit = context_.lightState.lightEdit;
    if (!edit.active) {
        return {};
    }
    context_.lightState.lightEdit = LightEditTransactionState{};

    if (edit.kind == TopologySelectionKind::DynamicLight) {
        SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicLight, edit.topologyLightId);
        return FinishLightMutationResult(FinishTopologyActionResult(
                FinishMoveDynamicLight(context_.map, edit.topologyLightId, edit.originalPosition)));
    }

    if (edit.kind == TopologySelectionKind::StaticRectLight
            || edit.kind == TopologySelectionKind::DynamicRectLight) {
        const Vector3* position = nullptr;
        const Vector3* target = nullptr;
        if (edit.kind == TopologySelectionKind::StaticRectLight) {
            if (const auto* light = FindSectorTopologyStaticRectLight(context_.map, edit.topologyLightId)) {
                position = &light->position; target = &light->target;
            }
        } else if (const auto* light = FindSectorTopologyDynamicRectLight(context_.map, edit.topologyLightId)) {
            position = &light->position; target = &light->target;
        }
        if (position == nullptr || target == nullptr) return {};
        SelectLight(context_.selection, context_.ui, edit.kind, edit.topologyLightId);
        const bool changed = !SameVector3(*position, edit.originalPosition)
                || !SameVector3(*target, edit.originalTarget);
        return FinishLightMutationResult(FinishTopologyActionResult({changed,
                changed ? TextFormat("Moved rect light %d", edit.topologyLightId)
                        : TextFormat("Rect light %d unchanged", edit.topologyLightId)}));
    }

    if (edit.kind == TopologySelectionKind::DynamicSpotLight) {
        const SectorTopologyDynamicSpotLight* light =
                FindSectorTopologyDynamicSpotLight(context_.map, edit.topologyLightId);
        if (light == nullptr) {
            return {};
        }
        SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicSpotLight, edit.topologyLightId);
        const bool movedOrigin = std::fabs(light->position.x - edit.originalPosition.x) > GeometryEpsilon
                || std::fabs(light->position.z - edit.originalPosition.z) > GeometryEpsilon;
        const bool movedTarget = std::fabs(light->target.x - edit.originalTarget.x) > GeometryEpsilon
                || std::fabs(light->target.z - edit.originalTarget.z) > GeometryEpsilon;
        SectorEditorTopologyActionResult finish;
        finish.changed = edit.spotHandle == SpotLightHandle::Target
                ? movedTarget
                : (movedOrigin || movedTarget);
        finish.status = finish.changed
                ? TextFormat("Moved dynamic spot %d", edit.topologyLightId)
                : TextFormat("Dynamic spot %d unchanged", edit.topologyLightId);
        return FinishLightMutationResult(FinishTopologyActionResult(finish));
    }

    if (edit.kind == TopologySelectionKind::StaticSpotLight) {
        const SectorTopologyStaticSpotLight* light =
                FindSectorTopologyStaticSpotLight(context_.map, edit.topologyLightId);
        if (light == nullptr) {
            return {};
        }
        SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticSpotLight, edit.topologyLightId);
        const bool movedOrigin = std::fabs(light->position.x - edit.originalPosition.x) > GeometryEpsilon
                || std::fabs(light->position.z - edit.originalPosition.z) > GeometryEpsilon;
        const bool movedTarget = std::fabs(light->target.x - edit.originalTarget.x) > GeometryEpsilon
                || std::fabs(light->target.z - edit.originalTarget.z) > GeometryEpsilon;
        SectorEditorTopologyActionResult finish;
        finish.changed = edit.spotHandle == SpotLightHandle::Target
                ? movedTarget
                : (movedOrigin || movedTarget);
        finish.status = finish.changed
                ? TextFormat("Moved static spot %d", edit.topologyLightId)
                : TextFormat("Static spot %d unchanged", edit.topologyLightId);
        return FinishLightMutationResult(FinishTopologyActionResult(finish));
    }

    SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticLight, edit.topologyLightId);
    return FinishLightMutationResult(FinishTopologyActionResult(
            FinishMoveStaticLight(context_.map, edit.topologyLightId, edit.originalPosition)));
}

SectorEditorLightMutationResult SectorEditorLightEditingService::CancelLightDragData(const char* message)
{
    const LightEditTransactionState edit = context_.lightState.lightEdit;
    if (edit.active) {
        if (edit.kind == TopologySelectionKind::DynamicLight) {
            if (SectorTopologyDynamicPointLight* light =
                        FindSectorTopologyDynamicLight(context_.map, edit.topologyLightId)) {
                light->position = edit.originalPosition;
                SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicLight, light->id);
            }
        } else if (edit.kind == TopologySelectionKind::DynamicSpotLight) {
            if (SectorTopologyDynamicSpotLight* light =
                        FindSectorTopologyDynamicSpotLight(context_.map, edit.topologyLightId)) {
                light->position = edit.originalPosition;
                light->target = edit.originalTarget;
                SelectLight(context_.selection, context_.ui, TopologySelectionKind::DynamicSpotLight, light->id);
            }
        } else if (edit.kind == TopologySelectionKind::StaticSpotLight) {
            if (SectorTopologyStaticSpotLight* light =
                        FindSectorTopologyStaticSpotLight(context_.map, edit.topologyLightId)) {
                light->position = edit.originalPosition;
                light->target = edit.originalTarget;
                SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticSpotLight, light->id);
            }
        } else if (edit.kind == TopologySelectionKind::StaticRectLight) {
            if (auto* light = FindSectorTopologyStaticRectLight(context_.map, edit.topologyLightId)) {
                light->position = edit.originalPosition; light->target = edit.originalTarget;
                SelectLight(context_.selection, context_.ui, edit.kind, light->id);
            }
        } else if (edit.kind == TopologySelectionKind::DynamicRectLight) {
            if (auto* light = FindSectorTopologyDynamicRectLight(context_.map, edit.topologyLightId)) {
                light->position = edit.originalPosition; light->target = edit.originalTarget;
                SelectLight(context_.selection, context_.ui, edit.kind, light->id);
            }
        } else if (SectorTopologyStaticPointLight* light =
                       FindSectorTopologyStaticLight(context_.map, edit.topologyLightId)) {
            light->position = edit.originalPosition;
            SelectLight(context_.selection, context_.ui, TopologySelectionKind::StaticLight, light->id);
        }
    }
    context_.lightState.lightEdit = LightEditTransactionState{};
    if (message != nullptr && message[0] != '\0') {
        context_.statusText = message;
    }
    return {};
}

SectorEditorLightMutationResult SectorEditorLightEditingService::ApplyLightPilot(
        Vector3 position,
        Vector3 target,
        float rectRollDeltaDegrees)
{
    const LightPilotLightState pilot = context_.lightState.lightPilot;
    if (!pilot.active) {
        return {};
    }

    SectorTopologyStaticPointLight* staticPoint = pilot.kind == LightPilotKind::StaticPoint
            ? FindSectorTopologyStaticLight(context_.map, pilot.lightId)
            : nullptr;
    SectorTopologyStaticSpotLight* staticSpot = pilot.kind == LightPilotKind::StaticSpot
            ? FindSectorTopologyStaticSpotLight(context_.map, pilot.lightId)
            : nullptr;
    SectorTopologyDynamicPointLight* dynamicPoint = pilot.kind == LightPilotKind::DynamicPoint
            ? FindSectorTopologyDynamicLight(context_.map, pilot.lightId)
            : nullptr;
    SectorTopologyDynamicSpotLight* dynamicSpot = pilot.kind == LightPilotKind::DynamicSpot
            ? FindSectorTopologyDynamicSpotLight(context_.map, pilot.lightId)
            : nullptr;
    SectorTopologyStaticRectLight* staticRect = pilot.kind == LightPilotKind::StaticRect
            ? FindSectorTopologyStaticRectLight(context_.map, pilot.lightId) : nullptr;
    SectorTopologyDynamicRectLight* dynamicRect = pilot.kind == LightPilotKind::DynamicRect
            ? FindSectorTopologyDynamicRectLight(context_.map, pilot.lightId) : nullptr;
    if (staticPoint == nullptr && staticSpot == nullptr
            && dynamicPoint == nullptr && dynamicSpot == nullptr
            && staticRect == nullptr && dynamicRect == nullptr) {
        return CancelLightPilotData("Light pilot cancelled: light missing");
    }

    const int lightId = pilot.lightId;
    const char* lightName = nullptr;
    if (staticPoint != nullptr) {
        staticPoint->position = position;
        lightName = "static light";
    } else if (staticSpot != nullptr) {
        staticSpot->position = position;
        staticSpot->target = target;
        lightName = "static spot";
    } else if (dynamicPoint != nullptr) {
        dynamicPoint->position = position;
        lightName = "dynamic light";
    } else if (dynamicSpot != nullptr) {
        dynamicSpot->position = position;
        dynamicSpot->target = target;
        lightName = "dynamic spot";
    } else if (staticRect != nullptr) {
        staticRect->position = position;
        staticRect->target = target;
        staticRect->rollDegrees = AddNormalizedDegrees(
                pilot.originalRollDegrees, rectRollDeltaDegrees);
        lightName = "static rect light";
    } else {
        dynamicRect->position = position;
        dynamicRect->target = target;
        dynamicRect->rollDegrees = AddNormalizedDegrees(
                pilot.originalRollDegrees, rectRollDeltaDegrees);
        lightName = "dynamic rect light";
    }
    context_.lightState.lightPilot = LightPilotLightState{};
    MarkEdited(TextFormat("Piloted %s %d", lightName, lightId));
    context_.statusText = TextFormat("Applied %s %d pilot pose", lightName, lightId);

    SectorEditorLightMutationResult result;
    result.changed = true;
    result.dynamicLightRendererRefreshNeeded = true;
    return result;
}

SectorEditorLightMutationResult SectorEditorLightEditingService::CancelLightPilotData(const char* message)
{
    if (!context_.lightState.lightPilot.active) {
        return {};
    }

    SectorEditorLightMutationResult result;
    const LightPilotLightState pilot = context_.lightState.lightPilot;
    result.restoredLightPilot = pilot;
    result.previewPoseRestoreNeeded = true;
    context_.lightState.lightPilot = LightPilotLightState{};
    if (pilot.kind == LightPilotKind::StaticPoint) {
        if (SectorTopologyStaticPointLight* light =
                    FindSectorTopologyStaticLight(context_.map, pilot.lightId)) {
            light->position = pilot.originalPosition;
        }
    } else if (pilot.kind == LightPilotKind::StaticSpot) {
        if (SectorTopologyStaticSpotLight* light =
                    FindSectorTopologyStaticSpotLight(context_.map, pilot.lightId)) {
            light->position = pilot.originalPosition;
            light->target = pilot.originalTarget;
        }
    } else if (pilot.kind == LightPilotKind::DynamicPoint) {
        if (SectorTopologyDynamicPointLight* light =
                    FindSectorTopologyDynamicLight(context_.map, pilot.lightId)) {
            light->position = pilot.originalPosition;
        }
    } else if (pilot.kind == LightPilotKind::DynamicSpot) {
        if (SectorTopologyDynamicSpotLight* light =
                    FindSectorTopologyDynamicSpotLight(context_.map, pilot.lightId)) {
            light->position = pilot.originalPosition;
            light->target = pilot.originalTarget;
        }
    } else if (pilot.kind == LightPilotKind::StaticRect) {
        if (auto* light = FindSectorTopologyStaticRectLight(context_.map, pilot.lightId)) {
            light->position = pilot.originalPosition; light->target = pilot.originalTarget;
            light->rollDegrees = pilot.originalRollDegrees;
        }
    } else if (pilot.kind == LightPilotKind::DynamicRect) {
        if (auto* light = FindSectorTopologyDynamicRectLight(context_.map, pilot.lightId)) {
            light->position = pilot.originalPosition; light->target = pilot.originalTarget;
            light->rollDegrees = pilot.originalRollDegrees;
        }
    }
    if (message != nullptr && message[0] != '\0') {
        context_.statusText = message;
    }
    return result;
}

bool SectorEditorLightEditingService::BeginProxyPlacement(
        LightProxyPlacementKind proxyKind,
        LightPilotKind kind,
        int lightId)
{
    Vector3* offset = FindProxyOffset(context_.map, proxyKind, kind, lightId);
    if (offset == nullptr) return false;
    context_.lightState.proxyPlacement = LightProxyPlacementState{};
    context_.lightState.proxyPlacement.active = true;
    context_.lightState.proxyPlacement.proxyKind = proxyKind;
    context_.lightState.proxyPlacement.kind = kind;
    context_.lightState.proxyPlacement.lightId = lightId;
    context_.lightState.proxyPlacement.originalOffsetWorld = *offset;
    context_.statusText = TextFormat(
            "Placing %s %d %s", LightName(kind), lightId, ProxyName(proxyKind));
    return true;
}

SectorEditorLightMutationResult SectorEditorLightEditingService::PreviewProxyPlacement(
        Vector3 offsetWorld)
{
    const LightProxyPlacementState placement = context_.lightState.proxyPlacement;
    if (!placement.active) return {};
    SectorLightAtmosphereSettings* atmosphere = FindAtmosphere(
            context_.map, placement.kind, placement.lightId);
    if (atmosphere == nullptr) {
        return CancelProxyPlacementData("Proxy placement cancelled: light missing");
    }
    SectorLightProxySettings normalized = atmosphere->proxy;
    if (placement.proxyKind == LightProxyPlacementKind::Halo) {
        normalized.halo.centerOffsetWorld = offsetWorld;
    } else if (placement.proxyKind == LightProxyPlacementKind::Shaft) {
        normalized.shaft.originOffsetWorld = offsetWorld;
    } else {
        return CancelProxyPlacementData("Proxy placement cancelled: invalid effect");
    }
    normalized = NormalizeSectorLightProxySettings(normalized);
    Vector3* currentOffset = FindProxyOffset(
            context_.map, placement.proxyKind, placement.kind, placement.lightId);
    const Vector3 normalizedOffset = placement.proxyKind == LightProxyPlacementKind::Halo
            ? normalized.halo.centerOffsetWorld
            : normalized.shaft.originOffsetWorld;
    if (currentOffset == nullptr || !SetVector3(*currentOffset, normalizedOffset)) {
        return {};
    }
    SectorEditorLightMutationResult result;
    result.changed = true;
    result.dynamicLightRendererRefreshNeeded = true;
    return result;
}

SectorEditorLightMutationResult SectorEditorLightEditingService::ApplyProxyPlacement()
{
    const LightProxyPlacementState placement = context_.lightState.proxyPlacement;
    if (!placement.active) return {};
    Vector3* offset = FindProxyOffset(
            context_.map, placement.proxyKind, placement.kind, placement.lightId);
    if (offset == nullptr) {
        return CancelProxyPlacementData("Proxy placement cancelled: light missing");
    }
    const bool changed = !SameVector3(*offset, placement.originalOffsetWorld);
    context_.lightState.proxyPlacement = LightProxyPlacementState{};
    SectorEditorLightMutationResult result;
    result.changed = changed;
    result.dynamicLightRendererRefreshNeeded = changed;
    if (changed) {
        MarkEdited(TextFormat(
                "Placed %s %d %s",
                LightName(placement.kind),
                placement.lightId,
                ProxyName(placement.proxyKind)));
    } else {
        context_.statusText = TextFormat(
                "%s placement unchanged", ProxyName(placement.proxyKind));
    }
    return result;
}

SectorEditorLightMutationResult SectorEditorLightEditingService::CancelProxyPlacementData(
        const char* message)
{
    const LightProxyPlacementState placement = context_.lightState.proxyPlacement;
    if (!placement.active) return {};
    context_.lightState.proxyPlacement = LightProxyPlacementState{};
    SectorEditorLightMutationResult result;
    Vector3* offset = FindProxyOffset(
            context_.map, placement.proxyKind, placement.kind, placement.lightId);
    if (offset != nullptr) {
        result.changed = SetVector3(*offset, placement.originalOffsetWorld);
        result.dynamicLightRendererRefreshNeeded = result.changed;
    }
    if (message != nullptr && message[0] != '\0') {
        context_.statusText = message;
    }
    return result;
}

bool SectorEditorLightEditingService::SetStaticLightPosition(
        SectorTopologyStaticPointLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightIntensity(
        SectorTopologyStaticPointLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightRadius(
        SectorTopologyStaticPointLight& light,
        float radius)
{
    radius = ClampLightRadius(radius);
    if (!SetValue(light.radius, radius)) {
        return false;
    }
    light.sourceRadius = ClampLightSourceRadius(light.sourceRadius, light.radius);
    MarkEdited(TextFormat("Updated static light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightSourceRadius(
        SectorTopologyStaticPointLight& light,
        float sourceRadius)
{
    sourceRadius = ClampLightSourceRadius(sourceRadius, light.radius);
    if (!SetValue(light.sourceRadius, sourceRadius)) {
        return false;
    }
    MarkEdited("Updated light source radius");
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightCastsShadow(
        SectorTopologyStaticPointLight& light,
        bool castsShadow)
{
    if (!SetValue(light.castsShadow, castsShadow)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static light %d shadow", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightColor(
        SectorTopologyStaticPointLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static light %d color", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightAtmosphere(
        SectorTopologyStaticPointLight& light,
        SectorLightAtmosphereSettings settings)
{
    settings = NormalizeSectorLightAtmosphereSettings(settings);
    if (SameAtmosphere(light.atmosphere, settings)) return false;
    light.atmosphere = settings;
    MarkEdited(TextFormat("Updated static light %d atmosphere", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightPosition(
        SectorTopologyStaticSpotLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightTarget(
        SectorTopologyStaticSpotLight& light,
        Vector3 target)
{
    if (!SetVector3(light.target, target)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::PointStaticSpotLightDown(
        SectorTopologyStaticSpotLight& light)
{
    if (!SetVector3(light.target, TargetPointingDown(light.position, light.target))) {
        return false;
    }
    MarkEdited(TextFormat("Pointed static spot %d down", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightRange(
        SectorTopologyStaticSpotLight& light,
        float range)
{
    range = ClampLightRadius(range);
    if (!SetValue(light.range, range)) {
        return false;
    }
    light.sourceRadius = ClampLightSourceRadius(light.sourceRadius, light.range);
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightSourceRadius(
        SectorTopologyStaticSpotLight& light,
        float sourceRadius)
{
    sourceRadius = ClampLightSourceRadius(sourceRadius, light.range);
    if (!SetValue(light.sourceRadius, sourceRadius)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d source radius", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightInnerCone(
        SectorTopologyStaticSpotLight& light,
        float innerConeDegrees)
{
    innerConeDegrees = ClampConeDegrees(innerConeDegrees);
    if (!SetValue(light.innerConeDegrees, innerConeDegrees)) {
        return false;
    }
    light.outerConeDegrees = ClampConeDegrees(std::max(light.outerConeDegrees, light.innerConeDegrees));
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightOuterCone(
        SectorTopologyStaticSpotLight& light,
        float outerConeDegrees)
{
    outerConeDegrees = ClampConeDegrees(std::max(outerConeDegrees, light.innerConeDegrees));
    if (!SetValue(light.outerConeDegrees, outerConeDegrees)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightCastsShadow(
        SectorTopologyStaticSpotLight& light,
        bool castsShadow)
{
    if (!SetValue(light.castsShadow, castsShadow)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d shadow", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightIntensity(
        SectorTopologyStaticSpotLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightColor(
        SectorTopologyStaticSpotLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d color", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightAtmosphere(
        SectorTopologyStaticSpotLight& light,
        SectorLightAtmosphereSettings settings)
{
    settings = NormalizeSectorLightAtmosphereSettings(settings);
    if (SameAtmosphere(light.atmosphere, settings)) return false;
    light.atmosphere = settings;
    MarkEdited(TextFormat("Updated static spot %d atmosphere", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightEnabled(
        SectorTopologyDynamicPointLight& light,
        bool enabled)
{
    if (!SetValue(light.enabled, enabled)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d enabled", light.id));
    return true;
}

namespace {

bool DynamicLightInstanceIdAvailable(
        const SectorTopologyMap& map,
        std::string_view value,
        const void* selected)
{
    for (const SectorTopologyDynamicPointLight& light : map.dynamicPointLights) {
        if (&light != selected && light.instanceId == value) return false;
    }
    for (const SectorTopologyDynamicSpotLight& light : map.dynamicSpotLights) {
        if (&light != selected && light.instanceId == value) return false;
    }
    for (const SectorTopologyDynamicRectLight& light : map.dynamicRectLights) {
        if (&light != selected && light.instanceId == value) return false;
    }
    return true;
}

template <typename Light>
bool SetDynamicLightInstanceIdValue(
        SectorTopologyMap& map,
        Light& light,
        const std::string& value,
        std::string& error)
{
    if (!IsValidSectorScriptInstanceId(value)) {
        error = "ID must contain 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    if (!DynamicLightInstanceIdAvailable(map, value, &light)) {
        error = "ID must be unique among all dynamic lights";
        return false;
    }
    error.clear();
    if (light.instanceId == value) return false;
    light.instanceId = value;
    return true;
}

} // namespace

bool SectorEditorLightEditingService::SetDynamicLightInstanceId(
        SectorTopologyDynamicPointLight& light,
        const std::string& value,
        std::string& error)
{
    if (!SetDynamicLightInstanceIdValue(context_.map, light, value, error)) return false;
    MarkEdited(TextFormat("Updated dynamic light %d instance ID", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightInstanceId(
        SectorTopologyDynamicSpotLight& light,
        const std::string& value,
        std::string& error)
{
    if (!SetDynamicLightInstanceIdValue(context_.map, light, value, error)) return false;
    MarkEdited(TextFormat("Updated dynamic spot %d instance ID", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightInstanceId(
        SectorTopologyDynamicRectLight& light,
        const std::string& value,
        std::string& error)
{
    if (!SetDynamicLightInstanceIdValue(context_.map, light, value, error)) return false;
    MarkEdited(TextFormat("Updated dynamic rect %d instance ID", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightFlicker(
        SectorTopologyDynamicPointLight& light,
        bool flicker)
{
    if (!SetValue(light.flicker, flicker)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d flicker", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightFlickerSpeed(
        SectorTopologyDynamicPointLight& light,
        float flickerSpeed)
{
    flickerSpeed = ClampDynamicLightFlickerSpeed(flickerSpeed);
    if (!SetValue(light.flickerSpeed, flickerSpeed)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightFlickerAmount(
        SectorTopologyDynamicPointLight& light,
        float flickerAmount)
{
    flickerAmount = ClampDynamicLightFlickerAmount(flickerAmount);
    if (!SetValue(light.flickerAmount, flickerAmount)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightCastsShadow(
        SectorTopologyDynamicPointLight& light, bool castsShadow)
{
    if (!SetValue(light.castsShadow, castsShadow)) return false;
    MarkEdited(TextFormat("Updated dynamic light %d shadows", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightShadowPriority(
        SectorTopologyDynamicPointLight& light, int shadowPriority)
{
    shadowPriority = ClampDynamicSpotLightShadowPriority(shadowPriority);
    if (!SetValue(light.shadowPriority, shadowPriority)) return false;
    MarkEdited(TextFormat("Updated dynamic light %d shadow priority", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightShadowBias(
        SectorTopologyDynamicPointLight& light, float shadowBias)
{
    shadowBias = ClampDynamicSpotLightShadowBias(shadowBias);
    if (!SetValue(light.shadowBias, shadowBias)) return false;
    MarkEdited(TextFormat("Updated dynamic light %d shadow bias", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightShadowStrength(
        SectorTopologyDynamicPointLight& light, float shadowStrength)
{
    shadowStrength = ClampDynamicSpotLightShadowStrength(shadowStrength);
    if (!SetValue(light.shadowStrength, shadowStrength)) return false;
    MarkEdited(TextFormat("Updated dynamic light %d shadow strength", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightShadowSoftness(
        SectorTopologyDynamicPointLight& light, float shadowSoftness)
{
    shadowSoftness = ClampDynamicSpotLightShadowSoftness(shadowSoftness);
    if (!SetValue(light.shadowSoftness, shadowSoftness)) return false;
    MarkEdited(TextFormat("Updated dynamic light %d shadow softness", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightPosition(
        SectorTopologyDynamicPointLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightIntensity(
        SectorTopologyDynamicPointLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightRadius(
        SectorTopologyDynamicPointLight& light,
        float radius)
{
    radius = ClampLightRadius(radius);
    if (!SetValue(light.radius, radius)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightColor(
        SectorTopologyDynamicPointLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d color", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightAtmosphere(
        SectorTopologyDynamicPointLight& light,
        SectorLightAtmosphereSettings settings)
{
    settings = NormalizeSectorLightAtmosphereSettings(settings);
    if (SameAtmosphere(light.atmosphere, settings)) return false;
    light.atmosphere = settings;
    MarkEdited(TextFormat("Updated dynamic light %d atmosphere", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightEnabled(
        SectorTopologyDynamicSpotLight& light,
        bool enabled)
{
    if (!SetValue(light.enabled, enabled)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d enabled", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightFlicker(
        SectorTopologyDynamicSpotLight& light,
        bool flicker)
{
    if (!SetValue(light.flicker, flicker)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d flicker", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightFlickerSpeed(
        SectorTopologyDynamicSpotLight& light,
        float flickerSpeed)
{
    flickerSpeed = ClampDynamicLightFlickerSpeed(flickerSpeed);
    if (!SetValue(light.flickerSpeed, flickerSpeed)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightFlickerAmount(
        SectorTopologyDynamicSpotLight& light,
        float flickerAmount)
{
    flickerAmount = ClampDynamicLightFlickerAmount(flickerAmount);
    if (!SetValue(light.flickerAmount, flickerAmount)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightCastsShadow(
        SectorTopologyDynamicSpotLight& light,
        bool castsShadow)
{
    if (!SetValue(light.castsShadow, castsShadow)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow request", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowPriority(
        SectorTopologyDynamicSpotLight& light,
        int shadowPriority)
{
    shadowPriority = ClampDynamicSpotLightShadowPriority(shadowPriority);
    if (!SetValue(light.shadowPriority, shadowPriority)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow priority", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowBias(
        SectorTopologyDynamicSpotLight& light,
        float shadowBias)
{
    shadowBias = ClampDynamicSpotLightShadowBias(shadowBias);
    if (!SetValue(light.shadowBias, shadowBias)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow bias", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowStrength(
        SectorTopologyDynamicSpotLight& light,
        float shadowStrength)
{
    shadowStrength = ClampDynamicSpotLightShadowStrength(shadowStrength);
    if (!SetValue(light.shadowStrength, shadowStrength)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow strength", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowSoftness(
        SectorTopologyDynamicSpotLight& light,
        float shadowSoftness)
{
    shadowSoftness = ClampDynamicSpotLightShadowSoftness(shadowSoftness);
    if (!SetValue(light.shadowSoftness, shadowSoftness)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow softness", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightPosition(
        SectorTopologyDynamicSpotLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightTarget(
        SectorTopologyDynamicSpotLight& light,
        Vector3 target)
{
    if (!SetVector3(light.target, target)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::PointDynamicSpotLightDown(
        SectorTopologyDynamicSpotLight& light)
{
    if (!SetVector3(light.target, TargetPointingDown(light.position, light.target))) {
        return false;
    }
    MarkEdited(TextFormat("Pointed dynamic spot %d down", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightIntensity(
        SectorTopologyDynamicSpotLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightRange(
        SectorTopologyDynamicSpotLight& light,
        float range)
{
    range = ClampLightRadius(range);
    if (!SetValue(light.range, range)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightInnerCone(
        SectorTopologyDynamicSpotLight& light,
        float innerConeDegrees)
{
    innerConeDegrees = ClampConeDegrees(innerConeDegrees);
    if (!SetValue(light.innerConeDegrees, innerConeDegrees)) {
        return false;
    }
    light.outerConeDegrees = ClampConeDegrees(std::max(light.outerConeDegrees, light.innerConeDegrees));
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightOuterCone(
        SectorTopologyDynamicSpotLight& light,
        float outerConeDegrees)
{
    outerConeDegrees = ClampConeDegrees(std::max(outerConeDegrees, light.innerConeDegrees));
    if (!SetValue(light.outerConeDegrees, outerConeDegrees)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightColor(
        SectorTopologyDynamicSpotLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d color", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightAtmosphere(
        SectorTopologyDynamicSpotLight& light,
        SectorLightAtmosphereSettings settings)
{
    settings = NormalizeSectorLightAtmosphereSettings(settings);
    if (SameAtmosphere(light.atmosphere, settings)) return false;
    light.atmosphere = settings;
    MarkEdited(TextFormat("Updated dynamic spot %d atmosphere", light.id));
    return true;
}

#define RECT_SETTER(method, Type, field, ValueType, clampExpression, label) \
bool SectorEditorLightEditingService::method(Type& light, ValueType value) \
{ \
    value = (clampExpression); \
    if (!SetValue(light.field, value)) return false; \
    MarkEdited(TextFormat("Updated %s %d", label, light.id)); \
    return true; \
}

bool SectorEditorLightEditingService::SetStaticRectLightPosition(SectorTopologyStaticRectLight& light, Vector3 value)
{ if (!SetVector3(light.position, value)) return false; MarkEdited(TextFormat("Updated static rect light %d", light.id)); return true; }
bool SectorEditorLightEditingService::SetStaticRectLightTarget(SectorTopologyStaticRectLight& light, Vector3 value)
{ if (!SetVector3(light.target, value)) return false; MarkEdited(TextFormat("Updated static rect light %d", light.id)); return true; }
bool SectorEditorLightEditingService::PointStaticRectLightDown(SectorTopologyStaticRectLight& light)
{
    if (!SetVector3(light.target, TargetPointingDown(light.position, light.target))) return false;
    MarkEdited(TextFormat("Pointed static rect light %d down", light.id));
    return true;
}
RECT_SETTER(SetStaticRectLightRoll, SectorTopologyStaticRectLight, rollDegrees, float, value, "static rect light")
RECT_SETTER(SetStaticRectLightWidth, SectorTopologyStaticRectLight, width, float, ClampLightRadius(value), "static rect light")
RECT_SETTER(SetStaticRectLightHeight, SectorTopologyStaticRectLight, height, float, ClampLightRadius(value), "static rect light")
RECT_SETTER(SetStaticRectLightRange, SectorTopologyStaticRectLight, range, float, ClampLightRadius(value), "static rect light")
RECT_SETTER(SetStaticRectLightIntensity, SectorTopologyStaticRectLight, intensity, float, ClampLightIntensity(value), "static rect light")
RECT_SETTER(SetStaticRectLightCastsShadow, SectorTopologyStaticRectLight, castsShadow, bool, value, "static rect light")
bool SectorEditorLightEditingService::SetStaticRectLightColor(SectorTopologyStaticRectLight& light, Color value)
{ if (!SetColorValue(light.color, value)) return false; MarkEdited(TextFormat("Updated static rect light %d", light.id)); return true; }
bool SectorEditorLightEditingService::SetStaticRectLightAtmosphere(SectorTopologyStaticRectLight& light, SectorLightAtmosphereSettings value)
{ value = NormalizeSectorLightAtmosphereSettings(value); if (SameAtmosphere(light.atmosphere, value)) return false; light.atmosphere = value; MarkEdited(TextFormat("Updated static rect light %d atmosphere", light.id)); return true; }

bool SectorEditorLightEditingService::SetDynamicRectLightPosition(SectorTopologyDynamicRectLight& light, Vector3 value)
{ if (!SetVector3(light.position, value)) return false; MarkEdited(TextFormat("Updated dynamic rect light %d", light.id)); return true; }
bool SectorEditorLightEditingService::SetDynamicRectLightTarget(SectorTopologyDynamicRectLight& light, Vector3 value)
{ if (!SetVector3(light.target, value)) return false; MarkEdited(TextFormat("Updated dynamic rect light %d", light.id)); return true; }
bool SectorEditorLightEditingService::PointDynamicRectLightDown(SectorTopologyDynamicRectLight& light)
{
    if (!SetVector3(light.target, TargetPointingDown(light.position, light.target))) return false;
    MarkEdited(TextFormat("Pointed dynamic rect light %d down", light.id));
    return true;
}
RECT_SETTER(SetDynamicRectLightRoll, SectorTopologyDynamicRectLight, rollDegrees, float, value, "dynamic rect light")
RECT_SETTER(SetDynamicRectLightWidth, SectorTopologyDynamicRectLight, width, float, ClampLightRadius(value), "dynamic rect light")
RECT_SETTER(SetDynamicRectLightHeight, SectorTopologyDynamicRectLight, height, float, ClampLightRadius(value), "dynamic rect light")
RECT_SETTER(SetDynamicRectLightRange, SectorTopologyDynamicRectLight, range, float, ClampLightRadius(value), "dynamic rect light")
RECT_SETTER(SetDynamicRectLightIntensity, SectorTopologyDynamicRectLight, intensity, float, ClampLightIntensity(value), "dynamic rect light")
RECT_SETTER(SetDynamicRectLightEnabled, SectorTopologyDynamicRectLight, enabled, bool, value, "dynamic rect light")
RECT_SETTER(SetDynamicRectLightFlicker, SectorTopologyDynamicRectLight, flicker, bool, value, "dynamic rect light")
RECT_SETTER(SetDynamicRectLightFlickerSpeed, SectorTopologyDynamicRectLight, flickerSpeed, float, ClampDynamicLightFlickerSpeed(value), "dynamic rect light")
RECT_SETTER(SetDynamicRectLightFlickerAmount, SectorTopologyDynamicRectLight, flickerAmount, float, ClampDynamicLightFlickerAmount(value), "dynamic rect light")
RECT_SETTER(SetDynamicRectLightCastsShadow, SectorTopologyDynamicRectLight, castsShadow, bool, value, "dynamic rect light")
RECT_SETTER(SetDynamicRectLightShadowPriority, SectorTopologyDynamicRectLight, shadowPriority, int, ClampDynamicSpotLightShadowPriority(value), "dynamic rect light")
RECT_SETTER(SetDynamicRectLightShadowBias, SectorTopologyDynamicRectLight, shadowBias, float, ClampDynamicSpotLightShadowBias(value), "dynamic rect light")
RECT_SETTER(SetDynamicRectLightShadowStrength, SectorTopologyDynamicRectLight, shadowStrength, float, ClampDynamicSpotLightShadowStrength(value), "dynamic rect light")
RECT_SETTER(SetDynamicRectLightShadowSoftness, SectorTopologyDynamicRectLight, shadowSoftness, float, ClampDynamicSpotLightShadowSoftness(value), "dynamic rect light")
bool SectorEditorLightEditingService::SetDynamicRectLightColor(SectorTopologyDynamicRectLight& light, Color value)
{ if (!SetColorValue(light.color, value)) return false; MarkEdited(TextFormat("Updated dynamic rect light %d", light.id)); return true; }
bool SectorEditorLightEditingService::SetDynamicRectLightAtmosphere(SectorTopologyDynamicRectLight& light, SectorLightAtmosphereSettings value)
{ value = NormalizeSectorLightAtmosphereSettings(value); if (SameAtmosphere(light.atmosphere, value)) return false; light.atmosphere = value; MarkEdited(TextFormat("Updated dynamic rect light %d atmosphere", light.id)); return true; }

#undef RECT_SETTER

} // namespace game
