#include "sector_editor/preview/SectorEditorPreviewOverlay.h"
#include "sector_demo/SectorRectLight.h"
#include "sector_editor/preview/SectorEditorLightProxyPlacement.h"
#include "sector_editor/preview/SectorEditorPreviewOverlayLayout.h"

#include "engine/render/ColorTransfer.h"
#include "game/navigation/SectorNavigationDebugDraw.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorPreviewActions.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingService.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace game {
namespace {

struct OverlayLine {
    std::string text;
    Color color;
    bool wrap;
};

Color LinearOverlaySwatch(Color color)
{
    return engine::SrgbColorBytesToLinearSceneUnorm(color);
}

const char* FpsFireRejectReasonLabel(FpsFireRejectReason reason)
{
    switch (reason) {
        case FpsFireRejectReason::None: return "none";
        case FpsFireRejectReason::NotInGameplay3D: return "not in gameplay 3D";
        case FpsFireRejectReason::MouseInputInactive: return "gameplay mouse inactive";
        case FpsFireRejectReason::UiCaptured: return "UI captured input";
        case FpsFireRejectReason::NoActiveWeapon: return "no active weapon";
        case FpsFireRejectReason::WeaponNotReady: return "weapon not ready";
        case FpsFireRejectReason::Cooldown: return "cooldown active";
    }
    return "unknown";
}

bool IsPreviewOverlayMouseInteractive(const SectorEditorPreviewControllerState& controllerState)
{
    return !controllerState.freeflyController.mouseLookEnabled;
}

bool SelectedLightProxyPlacementInfo(
        const SectorTopologyMap& topologyMap,
        const SelectionState& selectionState,
        LightPilotKind& outKind,
        int& outLightId,
        Vector3& outLightPositionWorld,
        const SectorLightProxySettings*& outProxy,
        bool& outSpotLight)
{
    if (selectionState.topologySelectionKind == TopologySelectionKind::StaticLight) {
        const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
                topologyMap, selectionState.selectedTopologyLightId);
        if (light == nullptr) return false;
        outKind = LightPilotKind::StaticPoint;
        outLightId = light->id;
        outLightPositionWorld = SectorAuthoringToWorldPosition(light->position);
        outProxy = &light->atmosphere.proxy;
        outSpotLight = false;
        return true;
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::StaticSpotLight) {
        const SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(
                topologyMap, selectionState.selectedTopologyStaticSpotLightId);
        if (light == nullptr) return false;
        outKind = LightPilotKind::StaticSpot;
        outLightId = light->id;
        outLightPositionWorld = SectorAuthoringToWorldPosition(light->position);
        outProxy = &light->atmosphere.proxy;
        outSpotLight = true;
        return true;
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicLight) {
        const SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                topologyMap, selectionState.selectedTopologyDynamicLightId);
        if (light == nullptr) return false;
        outKind = LightPilotKind::DynamicPoint;
        outLightId = light->id;
        outLightPositionWorld = SectorAuthoringToWorldPosition(light->position);
        outProxy = &light->atmosphere.proxy;
        outSpotLight = false;
        return true;
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicSpotLight) {
        const SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(
                topologyMap, selectionState.selectedTopologyDynamicSpotLightId);
        if (light == nullptr) return false;
        outKind = LightPilotKind::DynamicSpot;
        outLightId = light->id;
        outLightPositionWorld = SectorAuthoringToWorldPosition(light->position);
        outProxy = &light->atmosphere.proxy;
        outSpotLight = true;
        return true;
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::StaticRectLight) {
        const SectorTopologyStaticRectLight* light = FindSectorTopologyStaticRectLight(
                topologyMap, selectionState.selectedTopologyStaticSpotLightId);
        if (light == nullptr) return false;
        outKind = LightPilotKind::StaticRect;
        outLightId = light->id;
        outLightPositionWorld = SectorAuthoringToWorldPosition(light->position);
        outProxy = &light->atmosphere.proxy;
        outSpotLight = true;
        return true;
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicRectLight) {
        const SectorTopologyDynamicRectLight* light = FindSectorTopologyDynamicRectLight(
                topologyMap, selectionState.selectedTopologyDynamicSpotLightId);
        if (light == nullptr) return false;
        outKind = LightPilotKind::DynamicRect;
        outLightId = light->id;
        outLightPositionWorld = SectorAuthoringToWorldPosition(light->position);
        outProxy = &light->atmosphere.proxy;
        outSpotLight = true;
        return true;
    }
    return false;
}

bool IsValidPreviewSurfaceRef(
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        RuntimeObjectDragState& runtimeObjectDrag,
        SectorEditorPreviewSelectionState& previewSelectionState,
        SelectionState& selectionState,
        ManipulationState& manipulationState,
        SectorEditorSelectionUiDependencies selectionUi,
        MaterialEditingUiState& materialUiState,
        SectorSurfaceRef surface)
{
    SectorEditorSelectionServiceContext context{
            topologyMap,
            authoringGraph,
            authoringDerivation,
            authoringDerivationCurrent,
            selectionState,
            previewSelectionState.selectedSurface3D,
            previewSelectionState.selectedTopologySurface3D,
            manipulationState,
            runtimeObjectDrag,
            selectionUi,
            materialUiState,
            nullptr,
            nullptr,
            nullptr};
    return IsValidSectorEditorSurfaceRef(context, surface);
}

const SectorTopologyStaticSpotLight* SelectedTopologyStaticSpotLight(
        const SectorTopologyMap& topologyMap,
        const SelectionState& selectionState)
{
    return selectionState.topologySelectionKind == TopologySelectionKind::StaticSpotLight
            ? FindSectorTopologyStaticSpotLight(topologyMap, selectionState.selectedTopologyStaticSpotLightId)
            : nullptr;
}

const SectorTopologyStaticPointLight* SelectedTopologyStaticPointLight(
        const SectorTopologyMap& topologyMap,
        const SelectionState& selectionState)
{
    return selectionState.topologySelectionKind == TopologySelectionKind::StaticLight
            ? FindSectorTopologyStaticLight(topologyMap, selectionState.selectedTopologyLightId)
            : nullptr;
}

const SectorTopologyDynamicSpotLight* SelectedTopologyDynamicSpotLight(
        const SectorTopologyMap& topologyMap,
        const SelectionState& selectionState)
{
    return selectionState.topologySelectionKind == TopologySelectionKind::DynamicSpotLight
            ? FindSectorTopologyDynamicSpotLight(topologyMap, selectionState.selectedTopologyDynamicSpotLightId)
            : nullptr;
}

const SectorTopologyDynamicPointLight* SelectedTopologyDynamicPointLight(
        const SectorTopologyMap& topologyMap,
        const SelectionState& selectionState)
{
    return selectionState.topologySelectionKind == TopologySelectionKind::DynamicLight
            ? FindSectorTopologyDynamicLight(topologyMap, selectionState.selectedTopologyDynamicLightId)
            : nullptr;
}

Color ColorFromObjectProbeAmbientCube(const SectorBakedObjectLightProbe& probe)
{
    Vector3 rgb = {};
    for (const Vector3& face : probe.ambientCube) {
        rgb = Vector3Add(rgb, face);
    }
    rgb = Vector3Scale(rgb, 1.0f / 6.0f);
    return Color{
            static_cast<unsigned char>(std::round(Clamp(rgb.x, 0.0f, 1.0f) * 255.0f)),
            static_cast<unsigned char>(std::round(Clamp(rgb.y, 0.0f, 1.0f) * 255.0f)),
            static_cast<unsigned char>(std::round(Clamp(rgb.z, 0.0f, 1.0f) * 255.0f)),
            235};
}

bool ShouldDrawObjectProbeDebugMarker(
        Vector3 referencePosition,
        Vector3 probePosition,
        float maxDistanceWorld)
{
    if (maxDistanceWorld <= 0.0f) {
        return true;
    }
    const Vector3 delta = Vector3Subtract(probePosition, referencePosition);
    return Vector3LengthSqr(delta) <= maxDistanceWorld * maxDistanceWorld;
}

size_t CountVisibleObjectProbeDebugMarkers(
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        Vector3 referencePosition,
        float maxDistanceWorld)
{
    size_t count = 0;
    for (const SectorBakedObjectLightProbe& probe : objectLightProbes.probes) {
        if (ShouldDrawObjectProbeDebugMarker(referencePosition, probe.position, maxDistanceWorld)) {
            ++count;
        }
    }
    return count;
}

std::string FormatViewmodelTransform(Matrix value)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(3)
           << "[" << value.m0 << ' ' << value.m4 << ' ' << value.m8
           << " | " << value.m12 << "] "
           << "[" << value.m1 << ' ' << value.m5 << ' ' << value.m9
           << " | " << value.m13 << "] "
           << "[" << value.m2 << ' ' << value.m6 << ' ' << value.m10
           << " | " << value.m14 << ']';
    return output.str();
}

void DrawSpotLightConeRing(
        Vector3 origin,
        Vector3 forward,
        Vector3 right,
        Vector3 up,
        float range,
        float coneDegrees,
        Color color,
        bool drawSpokes)
{
    if (!std::isfinite(range) || range <= 0.0f || !std::isfinite(coneDegrees)) {
        return;
    }

    color = LinearOverlaySwatch(color);
    constexpr int SegmentCount = 32;
    const float halfAngleRadians = std::clamp(coneDegrees, 0.0f, 179.0f) * DEG2RAD * 0.5f;
    const float radius = std::tan(halfAngleRadians) * range;
    const Vector3 center = Vector3Add(origin, Vector3Scale(forward, range));

    Vector3 previous = Vector3Add(center, Vector3Scale(right, radius));
    for (int i = 1; i <= SegmentCount; ++i) {
        const float angle = static_cast<float>(i) * 2.0f * PI / static_cast<float>(SegmentCount);
        const Vector3 radial = Vector3Add(
                Vector3Scale(right, std::cos(angle) * radius),
                Vector3Scale(up, std::sin(angle) * radius));
        const Vector3 current = Vector3Add(center, radial);
        DrawLine3D(previous, current, color);
        previous = current;
    }

    if (drawSpokes) {
        DrawLine3D(origin, Vector3Add(center, Vector3Scale(right, radius)), color);
        DrawLine3D(origin, Vector3Subtract(center, Vector3Scale(right, radius)), color);
        DrawLine3D(origin, Vector3Add(center, Vector3Scale(up, radius)), color);
        DrawLine3D(origin, Vector3Subtract(center, Vector3Scale(up, radius)), color);
    }
}

} // namespace

Rectangle BuildSectorEditorPreviewOverlayInteractionRect(PreviewDebugOverlayTab activeTab)
{
    constexpr float x = 32.0f;
    constexpr float y = 32.0f;
    constexpr float width = 700.0f;
    constexpr float collapsedHeight = 78.0f;
    const float expandedHeight = SectorEditorPreviewOverlayExpandedHeight(activeTab);
    return Rectangle{
            x,
            y,
            width,
            activeTab == PreviewDebugOverlayTab::None ? collapsedHeight : expandedHeight};
}

Rectangle BuildSectorEditorPreviewAdjustmentPanelRect()
{
    return Rectangle{EditorWidth - 392.0f, EditorMainMenuHeight + 18.0f,
            360.0f, 286.0f};
}

void DrawSectorEditorPreviewSurfaceHighlights(
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        RuntimeObjectDragState& runtimeObjectDrag,
        SectorEditorPreviewSelectionState& previewSelectionState,
        const SectorEditorPreviewControllerState& previewControllerState,
        SelectionState& selectionState,
        ManipulationState& manipulationState,
        SectorEditorSelectionUiDependencies selectionUi,
        MaterialEditingUiState& materialUiState,
        const SectorMeshRenderer& preview)
{
    if (!preview.IsRendererReady() || previewControllerState.freeflyController.mouseLookEnabled) {
        return;
    }

    auto drawSurface = [
                               &topologyMap,
                               &authoringGraph,
                               &authoringDerivation,
                               authoringDerivationCurrent,
                               &runtimeObjectDrag,
                               &previewSelectionState,
                               &selectionState,
                               &manipulationState,
                               selectionUi,
                               &materialUiState,
                               &preview](
                               SectorSurfaceRef surface,
                               Color color,
                               float thickness) {
        if (!IsValidPreviewSurfaceRef(
                    topologyMap,
                    authoringGraph,
                    authoringDerivation,
                    authoringDerivationCurrent,
                    runtimeObjectDrag,
                    previewSelectionState,
                    selectionState,
                    manipulationState,
                    selectionUi,
                    materialUiState,
                    surface)) {
            return;
        }
        const float lift = IsWallSurface(surface.kind) ? PreviewHighlightLift : PreviewHighlightLift * 2.0f;
        for (const SectorGeneratedSurface& generated : preview.RenderedGeometry().surfaces) {
            if (!ShouldIncludeSectorGeneratedSurfaceForVisibility(generated, preview.VisibilityResult())) {
                continue;
            }
            const SectorSurfaceRef generatedRef = ToEditorSurfaceRef(generated.ref);
            if (!SameSectorEditorSurfaceRef(surface, generatedRef)) {
                continue;
            }
            const Vector3 offset = Vector3Scale(generated.normal, lift);
            for (size_t i = 0; i + 2 < generated.vertices.size(); i += 3) {
                const Vector3 a = Vector3Add(generated.vertices[i + 0].position, offset);
                const Vector3 b = Vector3Add(generated.vertices[i + 1].position, offset);
                const Vector3 c = Vector3Add(generated.vertices[i + 2].position, offset);
                const Color linearColor = LinearOverlaySwatch(color);
                DrawLine3D(a, b, linearColor);
                DrawLine3D(b, c, linearColor);
                DrawLine3D(c, a, linearColor);
            }
        }
        (void)thickness;
    };

    BeginMode3D(preview.RenderCamera());
    if (previewSelectionState.hoveredSurface3D.hit
            && !SameSectorEditorSurfaceRef(previewSelectionState.hoveredSurface3D.surface, previewSelectionState.selectedSurface3D)) {
        drawSurface(previewSelectionState.hoveredSurface3D.surface, Color{248, 238, 124, 235}, 2.0f);
    }
    if (previewSelectionState.selectedSurface3D.kind != SectorSurfaceKind::None) {
        drawSurface(previewSelectionState.selectedSurface3D, Color{84, 204, 255, 255}, 3.0f);
    }
    EndMode3D();
}

void DrawSectorEditorPreviewSpotLightOverlay(
        const SectorTopologyMap& topologyMap,
        const SectorEditorPreviewControllerState& previewControllerState,
        const SelectionState& selectionState,
        const SectorMeshRenderer& preview)
{
    if (!preview.IsRendererReady() || previewControllerState.freeflyController.mouseLookEnabled) {
        return;
    }

    const SectorTopologyStaticRectLight* staticRect =
            selectionState.topologySelectionKind == TopologySelectionKind::StaticRectLight
            ? FindSectorTopologyStaticRectLight(topologyMap,
                    selectionState.selectedTopologyStaticSpotLightId) : nullptr;
    const SectorTopologyDynamicRectLight* dynamicRect =
            selectionState.topologySelectionKind == TopologySelectionKind::DynamicRectLight
            ? FindSectorTopologyDynamicRectLight(topologyMap,
                    selectionState.selectedTopologyDynamicSpotLightId) : nullptr;
    if (staticRect != nullptr || dynamicRect != nullptr) {
        const Vector3 authoredPosition = staticRect != nullptr ? staticRect->position : dynamicRect->position;
        const Vector3 authoredTarget = staticRect != nullptr ? staticRect->target : dynamicRect->target;
        const float roll = staticRect != nullptr ? staticRect->rollDegrees : dynamicRect->rollDegrees;
        const float width = SectorAuthoringToWorldDistance(staticRect != nullptr ? staticRect->width : dynamicRect->width);
        const float height = SectorAuthoringToWorldDistance(staticRect != nullptr ? staticRect->height : dynamicRect->height);
        const float range = SectorAuthoringToWorldDistance(staticRect != nullptr ? staticRect->range : dynamicRect->range);
        const Vector3 origin = SectorAuthoringToWorldPosition(authoredPosition);
        const Vector3 target = SectorAuthoringToWorldPosition(authoredTarget);
        const SectorRectLightBasis basis = BuildSectorRectLightBasis(origin, target, roll);
        const Vector3 right = Vector3Scale(basis.right, width * 0.5f);
        const Vector3 up = Vector3Scale(basis.up, height * 0.5f);
        const Vector3 corners[4] = {
                Vector3Add(origin, Vector3Add(right, up)),
                Vector3Add(origin, Vector3Subtract(right, up)),
                Vector3Subtract(origin, Vector3Add(right, up)),
                Vector3Add(origin, Vector3Subtract(up, right))};
        const Color color = staticRect != nullptr ? Color{112, 232, 204, 255} : Color{255, 190, 82, 255};
        BeginMode3D(preview.RenderCamera());
        for (int i = 0; i < 4; ++i) DrawLine3D(corners[i], corners[(i + 1) % 4], LinearOverlaySwatch(color));
        DrawLine3D(origin, target, LinearOverlaySwatch(color));
        for (int i = 0; i < 4; ++i) {
            const Vector3 farCorner = Vector3Add(corners[i], Vector3Scale(basis.forward, range));
            DrawLine3D(corners[i], farCorner, LinearOverlaySwatch(WithAlpha(color, 150)));
            DrawLine3D(farCorner,
                    Vector3Add(corners[(i + 1) % 4], Vector3Scale(basis.forward, range)),
                    LinearOverlaySwatch(WithAlpha(color, 150)));
        }
        // Width and height resize handles.
        DrawSphereWires(Vector3Add(origin, right), 0.08f, 6, 8, RED);
        DrawSphereWires(Vector3Subtract(origin, right), 0.08f, 6, 8, RED);
        DrawSphereWires(Vector3Add(origin, up), 0.08f, 6, 8, BLUE);
        DrawSphereWires(Vector3Subtract(origin, up), 0.08f, 6, 8, BLUE);
        EndMode3D();
        return;
    }

    Vector3 lightPosition = {};
    Vector3 lightTarget = {};
    float lightRange = 0.0f;
    float innerConeDegrees = 0.0f;
    float outerConeDegrees = 0.0f;
    bool selectedStaticSpotLight = false;
    if (const SectorTopologyStaticSpotLight* light = SelectedTopologyStaticSpotLight(topologyMap, selectionState)) {
        lightPosition = light->position;
        lightTarget = light->target;
        lightRange = light->range;
        innerConeDegrees = light->innerConeDegrees;
        outerConeDegrees = light->outerConeDegrees;
        selectedStaticSpotLight = true;
    } else if (const SectorTopologyDynamicSpotLight* light = SelectedTopologyDynamicSpotLight(topologyMap, selectionState)) {
        lightPosition = light->position;
        lightTarget = light->target;
        lightRange = light->range;
        innerConeDegrees = light->innerConeDegrees;
        outerConeDegrees = light->outerConeDegrees;
    } else {
        return;
    }

    const Vector3 origin = SectorAuthoringToWorldPosition(lightPosition);
    const Vector3 target = SectorAuthoringToWorldPosition(lightTarget);
    Vector3 forward = Vector3Subtract(target, origin);
    if (Vector3LengthSqr(forward) <= 0.000001f) {
        forward = Vector3{1.0f, 0.0f, 0.0f};
    } else {
        forward = Vector3Normalize(forward);
    }

    Vector3 basisUp = Vector3{0.0f, 1.0f, 0.0f};
    if (std::fabs(Vector3DotProduct(forward, basisUp)) > 0.98f) {
        basisUp = Vector3{0.0f, 0.0f, 1.0f};
    }
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(basisUp, forward));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(forward, right));
    const float range = SectorAuthoringToWorldDistance(lightRange);
    const Vector3 rangeEnd = Vector3Add(origin, Vector3Scale(forward, range));

    constexpr float OriginMarkerRadius = 0.12f;
    constexpr float TargetMarkerRadius = 0.09f;
    const Color originColor = selectedStaticSpotLight ? Color{112, 232, 204, 255} : Color{255, 236, 122, 255};
    const Color targetColor = selectedStaticSpotLight ? Color{92, 194, 255, 255} : Color{255, 170, 82, 255};
    const Color directionColor = selectedStaticSpotLight ? Color{112, 232, 204, 255} : Color{255, 216, 88, 255};
    const Color rangeColor = selectedStaticSpotLight ? Color{112, 232, 204, 170} : Color{255, 216, 88, 170};
    const Color outerConeColor = selectedStaticSpotLight ? Color{112, 232, 204, 235} : Color{255, 216, 88, 235};
    const Color innerConeColor = selectedStaticSpotLight ? Color{178, 246, 255, 220} : Color{110, 218, 255, 220};

    BeginMode3D(preview.RenderCamera());
    DrawSphereWires(origin, OriginMarkerRadius, 8, 12, LinearOverlaySwatch(originColor));
    DrawSphereWires(target, TargetMarkerRadius, 8, 12, LinearOverlaySwatch(targetColor));
    DrawLine3D(origin, target, LinearOverlaySwatch(directionColor));
    DrawLine3D(origin, rangeEnd, LinearOverlaySwatch(rangeColor));
    DrawSpotLightConeRing(origin, forward, right, up, range, outerConeDegrees, outerConeColor, true);
    DrawSpotLightConeRing(origin, forward, right, up, range, innerConeDegrees, innerConeColor, false);
    EndMode3D();
}

void DrawSectorEditorPreviewObjectProbeOverlay(
        const SectorTopologyMap& topologyMap,
        const SectorEditorPreviewState& previewState,
        const SectorRuntimeObjectState& runtimeObjects,
        const SectorMeshRenderer& preview)
{
    if (!preview.IsRendererReady()
            || !previewState.overlay.showObjectProbeDebugOverlay
            || runtimeObjects.objectLightProbes.probes.empty()) {
        return;
    }

    constexpr float MarkerRadius = 0.08f;
    const Vector3 referencePosition = preview.RendererPose().position;
    const float maxDistanceWorld = NormalizeSectorPreviewSettings(
            topologyMap.previewSettings).objectProbeDebugDrawMaxDistanceWorld;
    BeginMode3D(preview.RenderCamera());
    for (const SectorBakedObjectLightProbe& probe : runtimeObjects.objectLightProbes.probes) {
        if (!ShouldDrawObjectProbeDebugMarker(referencePosition, probe.position, maxDistanceWorld)) {
            continue;
        }
        const Color color = ColorFromObjectProbeAmbientCube(probe);
        DrawSphere(probe.position, MarkerRadius, color);
        DrawSphereWires(
                probe.position,
                MarkerRadius * 1.65f,
                8,
                8,
                LinearOverlaySwatch(Color{255, 255, 255, 155}));
    }
    EndMode3D();
}

void DrawSectorEditorPreviewReflectionProbeOverlay(
        const SectorTopologyMap& topologyMap,
        const SectorEditorPreviewState& previewState,
        const SelectionState& selectionState,
        const SectorMeshRenderer& preview)
{
    if (!preview.IsRendererReady()
            || previewState.overlay.activePreviewDebugOverlayTab
                    != PreviewDebugOverlayTab::Probes
            || selectionState.selectedAuthoring.kind
                    != SectorAuthoringSelectionKind::ReflectionProbe) {
        return;
    }

    const int selectedId = selectionState.selectedAuthoring.reflectionProbeId;
    const auto selected = std::find_if(
            topologyMap.compiledReflectionProbes.begin(),
            topologyMap.compiledReflectionProbes.end(),
            [selectedId](const SectorCompiledReflectionProbe& probe) {
                return probe.sourceAuthoringProbeId == selectedId;
            });
    if (selected == topologyMap.compiledReflectionProbes.end()) return;

    const SectorCompiledReflectionProbe& probe = *selected;
    const float c = std::cos(probe.yawRadians);
    const float s = std::sin(probe.yawRadians);
    Vector3 corners[8]{};
    for (int index = 0; index < 8; ++index) {
        const Vector3 local{
                (index & 1) != 0
                        ? probe.halfExtentsWorld.x : -probe.halfExtentsWorld.x,
                (index & 2) != 0
                        ? probe.halfExtentsWorld.y : -probe.halfExtentsWorld.y,
                (index & 4) != 0
                        ? probe.halfExtentsWorld.z : -probe.halfExtentsWorld.z};
        corners[index] = Vector3Add(
                probe.influenceCenterWorld,
                Vector3{
                        local.x * c - local.z * s,
                        local.y,
                        local.x * s + local.z * c});
    }

    const Color boxColor = LinearOverlaySwatch(
            probe.enabled
                    ? Color{204, 126, 255, 245}
                    : Color{150, 150, 160, 205});
    const Color linkColor = LinearOverlaySwatch(Color{255, 204, 92, 220});
    const Color captureColor = LinearOverlaySwatch(Color{255, 224, 122, 255});
    const Color centerColor = LinearOverlaySwatch(Color{114, 224, 255, 255});

    BeginMode3D(preview.RenderCamera());
    for (int index = 0; index < 8; ++index) {
        for (int axis = 0; axis < 3; ++axis) {
            const int neighbor = index ^ (1 << axis);
            if (index < neighbor) DrawLine3D(corners[index], corners[neighbor], boxColor);
        }
    }
    DrawLine3D(
            probe.capturePositionWorld,
            probe.influenceCenterWorld,
            linkColor);
    DrawSphereWires(
            probe.capturePositionWorld, 0.12f, 8, 12, captureColor);
    DrawSphereWires(
            probe.influenceCenterWorld, 0.08f, 8, 12, centerColor);
    EndMode3D();
}

void DrawSectorEditorPreviewNavigationOverlay(
        const SectorEditorPreviewOverlayState& overlayState,
        const SectorNavigationWorld& navigation,
        const NpcNavigationRuntime& npcNavigation,
        int selectedRuntimeObjectId,
        const SectorMeshRenderer& preview)
{
    if (overlayState.activePreviewDebugOverlayTab
            != PreviewDebugOverlayTab::Navigation) {
        return;
    }
    DrawSectorNavigationDebugWorld(
            SectorNavigationDebugDrawSettings{
                    overlayState.showNavigationSurface,
                    overlayState.showNavigationEdges,
                    overlayState.showNavigationTileBounds,
                    overlayState.showNavigationStaticObstacles,
                    overlayState.showNavigationDynamicObstacles,
                    overlayState.showNavigationDoorPlaceholders,
                    overlayState.showNavigationStepConnections,
                    overlayState.showNavigationNpcPaths,
                    overlayState.showNavigationNpcAgents,
                    overlayState.showNavigationSelectedNpcOnly,
                    selectedRuntimeObjectId},
            navigation,
            npcNavigation,
            preview);
}

SectorEditorPreviewOverlayResult DrawSectorEditorPreviewOverlay(
        SectorEditorPreviewOverlayContext& context)
{
    engine::UIContext& ui = context.ui;
    const engine::UIConfig& config = context.config;
    engine::Input& input = context.input;
    engine::AssetManager& assets = context.assets;
    const engine::FontHandle smallFont = context.smallFont;
    SectorTopologyMap& topologyMap = context.topologyMap;
    SectorEditorPreviewOverlayState& overlayState = context.previewState.overlay;
    SectorEditorPreviewControllerState& controllerState = context.previewState.controller;
    SectorEditorPreviewCollisionState& collisionState = context.previewState.collision;
    SelectionState& selectionState = context.selectionState;
    MaterialEditingUiState& materialUiState = context.materialUiState;
    SectorMeshRenderer& preview = context.preview;
    SectorEditorPreviewOverlayResult result;

    const bool adjustmentActive =
            context.runtimeObjectEditingState.previewAdjustment.active
            || context.surfaceHeightAdjustmentState.active;
    const bool adjustmentMouseInteractive =
            IsPreviewOverlayMouseInteractive(controllerState);
    const bool mouseInteractive =
            adjustmentMouseInteractive && !adjustmentActive;
    const bool drawExpanded = overlayState.activePreviewDebugOverlayTab != PreviewDebugOverlayTab::None;
    const float panelW = 700.0f;
    const float padding = 10.0f;
    const float gap = 6.0f;
    const float stripH = 26.0f;
    const float tabH = 30.0f;
    const float rowH = 24.0f;
    const Rectangle basePanel{
            32.0f,
            EditorMainMenuHeight + 32.0f,
            panelW,
            0.0f};
    const float contentW = panelW - padding * 2.0f;
    engine::UIConfig smallConfig = SectorEditorSmallFontConfig(config, assets, smallFont);

    const SectorViewPose pose = ActiveSectorEditorPreviewPose(controllerState, preview);
    const Vector3 position = pose.position;
    const RuntimePortalVisibilityResult& visibility = preview.VisibilityResult();
    const int compactSectorId =
            controllerState.previewControlMode == SectorPreviewControlMode::Gameplay
                    ? controllerState.fpsControllerState.currentSectorId
                    : (visibility.validStartSector ? visibility.startSectorId : 0);
    std::string compactSector = compactSectorId > 0
            ? TextFormat("sector %d", compactSectorId)
            : "sector none";
    const char* dirtyText = context.topologyDocumentDirty ? "unsaved" : "saved";
    const std::string compactStatus = TextFormat(
            "3D %s | %s | assets %.0f%% | lightmap %s | %s",
            PreviewControlModeName(controllerState.previewControlMode),
            compactSector.c_str(),
            preview.RendererAssetProgress(assets) * 100.0f,
            preview.RendererLightmapStatusText(),
            dirtyText);

    std::string collisionStatus;
    if (controllerState.previewControlMode == SectorPreviewControlMode::Gameplay) {
        if (collisionState.sectorCollisionWorldValid) {
            if (collisionState.previewCollisionNoclipFallback) {
                collisionStatus = "mode: gameplay collision | status: no sector / noclip";
            } else if (controllerState.fpsControllerState.currentSectorId == 0
                    || !collisionState.previewVerticalResult.hasSector) {
                collisionStatus = "mode: gameplay collision | status: no sector";
            } else {
                std::string blockText;
                if (collisionState.previewMoveResult.hitWall) {
                    blockText += "wall ";
                }
                if (collisionState.previewMoveResult.blockedByStep) {
                    blockText += "step ";
                }
                if (collisionState.previewMoveResult.blockedByCeiling) {
                    blockText += "ceiling ";
                }
                if (blockText.empty()) {
                    blockText = "clear";
                }
                const char* verticalState = collisionState.previewVerticalResult.cannotFit
                        ? "cannot fit"
                        : (controllerState.fpsControllerState.grounded
                                ? "grounded"
                                : (controllerState.fpsControllerState.verticalVelocity > 0.0f ? "jumping" : "falling"));
                const float crouchAmount = controllerState.fpsControllerState.crouchAmount;
                const char* stance = crouchAmount >= 0.999f
                        ? "crouched"
                        : (crouchAmount <= 0.001f
                                ? "standing"
                                : (controllerState.fpsControllerState.crouchTargeted
                                        ? "crouching"
                                        : "standing up"));
                const bool crawling = IsSectorDuctCrawling(
                        controllerState.ductTraversal);
                const SectorFpsControllerConfig effectiveConfig =
                        SectorDuctViewControllerConfig(
                                EffectiveSectorFpsControllerConfig(
                                        controllerState.fpsControllerState,
                                        controllerState.fpsControllerConfig),
                                controllerState.ductTraversal);
                collisionStatus = TextFormat(
                        "mode: gameplay collision | sector: %d | vertical: %s / %s | stance: %s %.2fm | block: %s | radius: %.2f | step: %.2f | jump: %.2f",
                        controllerState.fpsControllerState.currentSectorId,
                        verticalState,
                        VerticalTransitionName(collisionState.previewVerticalResult.transition),
                        crawling ? "crawling" : stance,
                        effectiveConfig.playerHeight,
                        blockText.c_str(),
                        effectiveConfig.playerRadius,
                        effectiveConfig.stepHeight,
                        effectiveConfig.jumpHeight);
            }
        } else {
            collisionStatus = "mode: gameplay collision | status: unavailable";
        }
    } else {
        collisionStatus = "mode: FreeFly | collision: noclip";
    }

    std::vector<OverlayLine> lines;
    const auto addLine = [&](const std::string& text, Color color, bool wrap) {
        lines.push_back(OverlayLine{text, color, wrap});
    };
    const auto addWrappedLine = [&](const std::string& text) {
        addLine(text, smallConfig.mutedTextColor, true);
    };
    const auto addKeyValue = [&](const char* key, const std::string& value) {
        addLine(std::string(key) + ": " + value, smallConfig.mutedTextColor, false);
    };
    const auto addKeyValueStyled = [&](const char* key, const std::string& value, Color color, bool wrap) {
        addLine(std::string(key) + ": " + value, color, wrap);
    };

    if (drawExpanded) {
        switch (overlayState.activePreviewDebugOverlayTab) {
            case PreviewDebugOverlayTab::View:
                addKeyValue("mode", PreviewControlModeName(controllerState.previewControlMode));
                addKeyValue("position", TextFormat("%.2f, %.2f, %.2f", position.x, position.y, position.z));
                addKeyValue("sector", compactSector);
                addWrappedLine(collisionStatus);
                if (!collisionState.sectorCollisionWorldWarning.empty()) {
                    addKeyValueStyled(
                            "warning",
                            collisionState.sectorCollisionWorldWarning,
                            Color{236, 92, 92, 245},
                            true);
                }
                break;
            case PreviewDebugOverlayTab::Render:
                addKeyValue("sectors", TextFormat("%zu", preview.SectorCount()));
                addKeyValue("batches", TextFormat("%zu", preview.BatchCount()));
                addKeyValue("triangles", TextFormat("%d", preview.TriangleCount()));
                addKeyValueStyled("render", preview.RenderDebugText(), smallConfig.mutedTextColor, true);
                break;
            case PreviewDebugOverlayTab::Visibility:
                addKeyValue("valid start", visibility.validStartSector ? "yes" : "no");
                addKeyValue("visible", TextFormat("%zu / %zu", visibility.visibleSectorIds.size(), visibility.totalSectorCount));
                addKeyValue("fallback", visibility.fallbackDrawAll ? "draw all" : "none");
                addKeyValueStyled("details", preview.PortalVisibilityDebugText(), smallConfig.mutedTextColor, true);
                break;
            case PreviewDebugOverlayTab::Lighting:
                addKeyValue("dynamic", preview.DynamicLightingEnabled() ? "on" : "off");
                addKeyValue("AO", overlayState.useBakedAmbientOcclusion ? "on" : "off");
                addKeyValue("lightmap", preview.RendererLightmapStatusText());
                addKeyValue("dynamic lights", TextFormat(
                        "selected %zu | portal eligible %zu | sources %zu",
                        preview.SelectedDynamicLights().size(),
                        preview.DynamicLightCandidateCount(),
                        preview.DynamicLightSourceCount()));
                if (!preview.SelectedDynamicLightKeys().empty()) {
                    std::ostringstream ids;
                    for (size_t i = 0; i < preview.SelectedDynamicLightKeys().size(); ++i) {
                        if (i > 0) {
                            ids << ",";
                        }
                        const SectorPreviewDynamicLightKey& key =
                                preview.SelectedDynamicLightKeys()[i];
                        ids << (key.kind == SectorPreviewDynamicLightKind::Spot
                                        ? "spot:"
                                        : "point:")
                                << key.lightId;
                    }
                    addKeyValue("selected ids", ids.str());
                }
                {
                    const SectorDynamicLightSelectionStats& stats =
                            preview.DynamicLightSelectionStats();
                    addKeyValue("lighting reach", TextFormat(
                            "sectors %zu | %s | start %d | blockers %zu%s",
                            stats.reachableSectorCount,
                            stats.reachabilityCacheHit ? "cached" : "rebuilt",
                            stats.lightingStartSectorId,
                            stats.dynamicPortalBlockerCount,
                            stats.cameraVisibilityFallback
                                    ? " | camera fallback ignored"
                                    : ""));
                    addKeyValue("receiver light refs", TextFormat(
                            "total/max %zu/%zu | bounds %zu",
                            stats.visibleReceiverLightReferences,
                            stats.maxVisibleReceiverLights,
                            stats.visibleReceiverCount));
                }
                if (!preview.SelectedDynamicLights().empty()) {
                    const SectorPreviewDynamicPointLightUniform& light = preview.SelectedDynamicLights().front();
                    addKeyValue("first light", TextFormat(
                            "%s id %d | intensity %.2f | radius %.2f | pos %.2f, %.2f, %.2f",
                            light.kind == SectorPreviewDynamicLightKind::Spot ? "spot" : "point",
                            light.lightId,
                            light.intensity,
                            light.radius,
                            light.position.x,
                            light.position.y,
                            light.position.z));
                }
                {
                    const SectorDynamicShadowRenderStats& stats =
                            preview.DynamicShadowRenderStats();
                    const char* atlasState = !stats.enabled
                            ? "off"
                            : stats.cacheHit
                                    ? "cache hit"
                                    : stats.atlasRendered ? "rebuilt" : "idle";
                    addKeyValue("shadow atlas", TextFormat(
                            "%s | point/spot/rect %zu/%zu/%zu | slots %zu",
                            atlasState,
                            stats.pointLights,
                            stats.spotLights,
                            stats.rectLights,
                            stats.occupiedTiles));
                    addKeyValue("shadow updates", TextFormat(
                            "valid/dirty/queued %zu/%zu/%zu | lights/tiles rebuilt %zu/%zu | CPU %.3f ms",
                            stats.validLights,
                            stats.dirtyLights,
                            stats.queuedLights,
                            stats.updatedLights,
                            stats.renderedTiles,
                            stats.cpuMilliseconds));
                    addKeyValue("shadow geometry", TextFormat(
                            "sector draw/cull %zu/%zu",
                            stats.sectorBatchesDrawn,
                            stats.sectorBatchesCulled));
                    addKeyValue("shadow objects", TextFormat(
                            "draw/cull %zu/%zu | casters dynamic %zu",
                            stats.objectCastersDrawn,
                            stats.objectCastersCulled,
                            preview.DynamicModelShadowCasterCount()));
                    addKeyValue("shadow revisions", TextFormat(
                            "door %llu | static %llu | dynamic %llu",
                            static_cast<unsigned long long>(stats.doorCasterRevision),
                            static_cast<unsigned long long>(stats.staticModelCasterRevision),
                            static_cast<unsigned long long>(stats.dynamicModelCasterRevision)));
                    addKeyValue("depth pre-pass",
                            preview.DepthPrepassEnabled() ? "on" : "off");
                }
                addKeyValue("doors drawn now", TextFormat(
                        "%zu / %zu, skipped %zu",
                        preview.DoorDrawnCount(),
                        preview.DoorConsideredCount(),
                        preview.DoorSkippedCount()));
                addKeyValue("doors authored/valid", TextFormat(
                        "%zu / %zu",
                        context.runtimeObjects.doorObjectCount,
                        context.runtimeObjects.validDoorAnchorCount));
                break;
            case PreviewDebugOverlayTab::Pbr: {
                const SectorPbrContributionSettings settings =
                        preview.PbrContributionSettings();
                addKeyValue("mode", SectorPbrDiagnosticModeName(
                        settings.diagnosticMode));
                addKeyValue("world scales", TextFormat(
                        "indirect %.2f | environment specular %.2f",
                        settings.worldIndirectDiffuseScale,
                        settings.worldEnvironmentSpecularScale));
                addKeyValue("environment", preview.PbrEnvironmentActive()
                        ? (preview.PbrEnvironmentUsesSky()
                                ? "active real sky (hardware sRGB decode)"
                                : "active real source")
                        : "inactive (exactly zero radiance)");
                const SectorPbrDrawDiagnostics& diagnostic =
                        preview.WorldPbrDiagnostics();
                if (!diagnostic.valid) {
                    addKeyValue("world draw", selectionState.selectedRuntimeObjectId >= 0
                            ? "selected object has no active PBR draw"
                            : "no active world-model PBR draw");
                } else {
                    addKeyValue("world draw", TextFormat(
                            "%s | object %d | material %d",
                            SectorPbrLightingPathName(diagnostic.state.path),
                            diagnostic.placedObjectId,
                            diagnostic.materialIndex));
                    addKeyValue("indirect source", TextFormat(
                            "%s | probe %s | linear HDR",
                            SectorPbrIndirectSourceName(
                                    diagnostic.state.indirectSource),
                            diagnostic.state.useObjectProbe ? "valid" : "not selected"));
                    addKeyValue("active state", TextFormat(
                            "environment %s @ %.3f | override %s | brightness %.3f",
                            diagnostic.state.environmentActive ? "on" : "off",
                            diagnostic.state.environmentExposure,
                            diagnostic.state.materialOverrideActive ? "on" : "off",
                            diagnostic.state.outputBrightnessMultiplier));
                    std::ostringstream staticSpecular;
                    staticSpecular
                            << (diagnostic.state.staticSpecularEligible
                                    ? "eligible"
                                    : "disabled")
                            << " | "
                            << diagnostic.staticSpecularLights.lightCount
                            << " selected";
                    if (diagnostic.staticSpecularLights.lightCount > 0) {
                        staticSpecular << " | ids ";
                        for (int lightIndex = 0;
                                lightIndex < diagnostic.staticSpecularLights.lightCount;
                                ++lightIndex) {
                            if (lightIndex > 0) staticSpecular << ',';
                            staticSpecular
                                    << diagnostic.staticSpecularLights.lightIds[
                                            static_cast<size_t>(lightIndex)];
                        }
                    }
                    addKeyValue("static specular", staticSpecular.str());
                    std::ostringstream roles;
                    for (size_t roleIndex = 0;
                            roleIndex < engine::ModelMaterialTextureRoleCount;
                            ++roleIndex) {
                        const auto role = static_cast<engine::ModelMaterialTextureRole>(
                                roleIndex);
                        const engine::ModelMaterialTextureInfo& texture =
                                diagnostic.material.textureInfo[roleIndex];
                        if (roleIndex > 0) roles << " | ";
                        roles << engine::ModelMaterialTextureRoleName(role)
                              << ':' << (texture.present
                                      ? "bound"
                                      : (texture.declared ? "missing" : "none"));
                        if (texture.present) {
                            roles << "/0x" << std::hex << texture.internalFormat
                                  << std::dec << '/';
                            if (texture.transfer
                                    == engine::ModelTextureTransfer::ExplicitSrgbDecode) {
                                roles << (texture.hardwareSrgbDecode
                                        ? "hardware sRGB"
                                        : "shader sRGB");
                            } else {
                                roles << "raw linear";
                            }
                        }
                    }
                    addKeyValueStyled(
                            "textures",
                            roles.str(),
                            smallConfig.mutedTextColor,
                            true);
                    addKeyValue("factors", TextFormat(
                            "base %.2f %.2f %.2f | metallic %.2f | roughness %.2f | AO %.2f",
                            diagnostic.material.baseColorFactor.x,
                            diagnostic.material.baseColorFactor.y,
                            diagnostic.material.baseColorFactor.z,
                            diagnostic.material.metallicFactor,
                            diagnostic.material.roughnessFactor,
                            diagnostic.material.occlusionStrength));
                    const bool emissiveEligible =
                            diagnostic.material.emissiveStrength > 0.0f
                            && (diagnostic.material.emissiveFactor.x > 0.0f
                                    || diagnostic.material.emissiveFactor.y > 0.0f
                                    || diagnostic.material.emissiveFactor.z > 0.0f);
                    addKeyValue("emissive", TextFormat(
                            "factor %.2f %.2f %.2f | strength %.2f | scene bloom %s",
                            diagnostic.material.emissiveFactor.x,
                            diagnostic.material.emissiveFactor.y,
                            diagnostic.material.emissiveFactor.z,
                            diagnostic.material.emissiveStrength,
                            emissiveEligible ? "eligible per fragment" : "no"));
                }
                const SectorPbrDrawDiagnostics& viewmodelDiagnostic =
                        preview.ViewmodelPbrDiagnostics();
                if (viewmodelDiagnostic.valid) {
                    addKeyValue("viewmodel path", TextFormat(
                            "%s | brightness %.3f | override %s | world scales isolated",
                            SectorPbrLightingPathName(
                                    viewmodelDiagnostic.state.path),
                            viewmodelDiagnostic.state.outputBrightnessMultiplier,
                            viewmodelDiagnostic.state.materialOverrideActive
                                    ? "on" : "off"));
                    std::ostringstream viewmodelStaticSpecular;
                    viewmodelStaticSpecular
                            << (viewmodelDiagnostic.state.staticSpecularEligible
                                    ? "eligible"
                                    : "disabled")
                            << " | "
                            << viewmodelDiagnostic.staticSpecularLights.lightCount
                            << " selected";
                    if (viewmodelDiagnostic.staticSpecularLights.lightCount > 0) {
                        viewmodelStaticSpecular << " | ids ";
                        for (int lightIndex = 0;
                                lightIndex < viewmodelDiagnostic.staticSpecularLights.lightCount;
                                ++lightIndex) {
                            if (lightIndex > 0) viewmodelStaticSpecular << ',';
                            viewmodelStaticSpecular
                                    << viewmodelDiagnostic.staticSpecularLights.lightIds[
                                            static_cast<size_t>(lightIndex)];
                        }
                    }
                    addKeyValue(
                            "viewmodel static specular",
                            viewmodelStaticSpecular.str());
                }
                addWrappedLine("Metallic / Roughness uses R=metallic and G=effective roughness. Tangent-Space Normal shows raw normal RGB; magenta means missing or not ready. Shading Normal shows the final world-space normal. Diagnostics bypass fog and still pass through the shared HDR presentation transform.");
                break;
            }
            case PreviewDebugOverlayTab::Objects: {
                const SectorRuntimeObjectState& objects = context.runtimeObjects;
                addKeyValue("placed/spawned/skipped", TextFormat(
                        "%zu / %zu / %zu",
                        objects.placedObjectCount,
                        objects.spawnedObjectCount,
                        objects.skippedObjectCount));
                addKeyValue("sprites ready/pending/failed", TextFormat(
                        "%zu / %zu / %zu",
                        objects.spriteAnimationReadyCount,
                        objects.spriteAnimationPendingCount,
                        objects.spriteAnimationFailedCount));
                addKeyValue("directional clips", TextFormat(
                        "resolved %zu | missing %zu | fallback %zu",
                        objects.directionalClipResolvedCount,
                        objects.directionalClipMissingCount,
                        objects.directionalClipFallbackCount));
                addKeyValue("single clips", TextFormat(
                        "resolved %zu | missing %zu | fallback %zu",
                        objects.singleClipResolvedCount,
                        objects.singleClipMissingCount,
                        objects.singleClipFallbackCount));
                addKeyValueStyled("billboards", preview.RenderDebugText(), smallConfig.mutedTextColor, true);
                if (!objects.placedObjectWarning.empty()) {
                    addKeyValueStyled("warning", objects.placedObjectWarning, Color{236, 92, 92, 245}, true);
                }
                break;
            }
            case PreviewDebugOverlayTab::Probes: {
                const char* objectProbeStatus = context.runtimeObjects.objectProbeStatus.empty()
                        ? "none"
                        : context.runtimeObjects.objectProbeStatus.c_str();
                addKeyValueStyled("object probe status", objectProbeStatus, smallConfig.mutedTextColor, true);
                const size_t totalProbeCount = context.runtimeObjects.objectLightProbes.probes.size();
                addKeyValue("object probe count", TextFormat("%zu", totalProbeCount));
                const std::size_t reflectionProbeCount = topologyMap.compiledReflectionProbes.size();
                addKeyValue("reflection probes", TextFormat(
                        "placed %zu | baked %d",
                        reflectionProbeCount,
                        topologyMap.bakedReflectionProbes.count));
                const bool selectedReflectionProbe =
                        selectionState.selectedAuthoring.kind
                                == SectorAuthoringSelectionKind::ReflectionProbe;
                addKeyValue("selected reflection", selectedReflectionProbe
                        ? TextFormat("%d", selectionState.selectedAuthoring.reflectionProbeId)
                        : "none");
                if (overlayState.showObjectProbeDebugOverlay) {
                    const float maxDistanceWorld = NormalizeSectorPreviewSettings(
                            topologyMap.previewSettings).objectProbeDebugDrawMaxDistanceWorld;
                    const size_t visibleProbeCount = CountVisibleObjectProbeDebugMarkers(
                            context.runtimeObjects.objectLightProbes,
                            preview.RendererPose().position,
                            maxDistanceWorld);
                    addKeyValue("drawn", TextFormat("%zu / %zu", visibleProbeCount, totalProbeCount));
                }
                if (!context.runtimeObjects.objectSectorLookupWarning.empty()) {
                    addKeyValueStyled("lookup warning", context.runtimeObjects.objectSectorLookupWarning, Color{236, 92, 92, 245}, true);
                }
                break;
            }
            case PreviewDebugOverlayTab::Viewmodel: {
                const FpsViewmodelRuntimeState& vm = context.viewmodel;
                const char* loadState = vm.loadState == FpsViewmodelLoadState::Ready ? "ready"
                        : vm.loadState == FpsViewmodelLoadState::Pending ? "pending"
                        : vm.loadState == FpsViewmodelLoadState::Failed ? "failed" : "inactive";
                const char* equipState = vm.equipState
                                == FpsViewmodelEquipState::Holstered
                        ? "holstered"
                        : vm.equipState
                                        == FpsViewmodelEquipState::Unholstering
                                ? "unholstering"
                                : vm.equipState
                                                == FpsViewmodelEquipState::Holstering
                                        ? "holstering"
                                        : "equipped";
                const Vector3 proceduralRotation = Vector3Scale(
                        QuaternionToEuler(vm.holsterPose.rotation),
                        RAD2DEG);
                addKeyValue("weapon", vm.activeWeaponId.empty() ? "none" : vm.activeWeaponId);
                addKeyValue("equip state", equipState);
                addKeyValue("equip progress", TextFormat(
                        "raw %.4f | hidden eased %.4f | ready %s",
                        vm.equipProgress,
                        vm.holsterPose.hiddenAmount,
                        IsFpsViewmodelReadyForUse(vm) ? "yes" : "no"));
                addKeyValue("transition timing", TextFormat(
                        "holster %.3fs | unholster %.3fs",
                        vm.holsterTransition.holsterDurationSeconds,
                        vm.holsterTransition.unholsterDurationSeconds));
                addKeyValue("hidden translation", TextFormat(
                        "%.3f, %.3f, %.3f",
                        vm.holsterTransition.hiddenTranslation.x,
                        vm.holsterTransition.hiddenTranslation.y,
                        vm.holsterTransition.hiddenTranslation.z));
                addKeyValue("hidden rotation", TextFormat(
                        "%.2f, %.2f, %.2f",
                        vm.holsterTransition.hiddenRotationDegrees.x,
                        vm.holsterTransition.hiddenRotationDegrees.y,
                        vm.holsterTransition.hiddenRotationDegrees.z));
                addKeyValue("procedural pose", TextFormat(
                        "T %.3f, %.3f, %.3f | R %.2f, %.2f, %.2f",
                        vm.holsterPose.translation.x,
                        vm.holsterPose.translation.y,
                        vm.holsterPose.translation.z,
                        proceduralRotation.x,
                        proceduralRotation.y,
                        proceduralRotation.z));
                addKeyValue("load", loadState);
                addKeyValueStyled("model", vm.resolvedModelPath, smallConfig.mutedTextColor, true);
                addKeyValue("animation", vm.animationName.empty() ? "none" : vm.animationName);
                addKeyValue("cursor", TextFormat("source %.3f | raylib %.3f", vm.sourceFrameCursor, vm.raylibFrame));
                addKeyValue("geometry", TextFormat("meshes %d | triangles %d | bones %d", vm.meshCount, vm.triangleCount, vm.boneCount));
                addKeyValue("position", TextFormat("%.3f, %.3f, %.3f", vm.presentation.position.x, vm.presentation.position.y, vm.presentation.position.z));
                addKeyValue("rotation", TextFormat("%.2f, %.2f, %.2f", vm.presentation.rotationDegrees.x, vm.presentation.rotationDegrees.y, vm.presentation.rotationDegrees.z));
                addKeyValue("projection", TextFormat("scale %.3f | vertical FOV %.2f", vm.presentation.scale, vm.presentation.verticalFovDegrees));
                addKeyValue("lighting", TextFormat(
                        "environment %.3f | material override %s",
                        vm.environmentExposure,
                        vm.materialOverride.enabled ? "on" : "off"));
                addKeyValue("brightness", TextFormat(
                        "adjustment %+.3f | multiplier %.3fx",
                        vm.brightnessAdjustment,
                        vm.brightnessMultiplier));
                if (vm.materialOverride.enabled) {
                    addKeyValue("material", TextFormat(
                            "metallic %.3f | roughness %.3f | packed texture %s",
                            vm.materialOverride.metallicFactor,
                            vm.materialOverride.roughnessFactor,
                            vm.materialOverride.useMetallicRoughnessTexture ? "on" : "off"));
                }
                const FpsViewmodelAttachmentRuntimeState& attachment =
                        vm.attachment;
                const char* attachmentLoadState = attachment.loadState
                                == FpsViewmodelAttachmentLoadState::Ready
                        ? "ready"
                        : attachment.loadState
                                        == FpsViewmodelAttachmentLoadState::Pending
                                ? "pending"
                                : attachment.loadState
                                                == FpsViewmodelAttachmentLoadState::Failed
                                        ? "failed"
                                        : "inactive";
                addKeyValueStyled(
                        "attachment model",
                        attachment.resolvedModelPath.empty()
                                ? "none"
                                : attachment.resolvedModelPath,
                        smallConfig.mutedTextColor,
                        true);
                addKeyValue("attachment load", attachmentLoadState);
                addKeyValue("attachment geometry", TextFormat(
                        "meshes %d | triangles %d | materials %d",
                        attachment.meshCount,
                        attachment.triangleCount,
                        attachment.materialCount));
                addKeyValue("attachment lighting default", TextFormat(
                        "brightness %+.3f | metallic %.3f | roughness %.3f",
                        attachment.lightingDefaults.brightnessAdjustment,
                        attachment.lightingDefaults.materialOverride
                                .metallicFactor,
                        attachment.lightingDefaults.materialOverride
                                .roughnessFactor));
                addKeyValue("attachment lighting effective", TextFormat(
                        "brightness %+.3f (x%.3f) | metallic %.3f | roughness %.3f | packed texture %s",
                        attachment.lighting.brightnessAdjustment,
                        attachment.brightnessMultiplier,
                        attachment.lighting.materialOverride.metallicFactor,
                        attachment.lighting.materialOverride.roughnessFactor,
                        attachment.lighting.materialOverride
                                        .useMetallicRoughnessTexture
                                ? "on"
                                : "off"));
                addKeyValue("attachment bone", TextFormat(
                        "configured %s | resolved %s | index %d",
                        attachment.configuredBoneName.empty()
                                ? "none"
                                : attachment.configuredBoneName.c_str(),
                        attachment.resolvedBoneName.empty()
                                ? "none"
                                : attachment.resolvedBoneName.c_str(),
                        attachment.boneIndex));
                const char* poseSpace = attachment.poseSpace
                                == FpsViewmodelBonePoseSpace::Model
                        ? "model-space"
                        : attachment.poseSpace == FpsViewmodelBonePoseSpace::Local
                                ? "local (parent accumulated)"
                                : "unknown";
                addKeyValue("pose space", poseSpace);
                addKeyValue("grip translation", TextFormat(
                        "%.4f, %.4f, %.4f",
                        attachment.gripCorrection.translation.x,
                        attachment.gripCorrection.translation.y,
                        attachment.gripCorrection.translation.z));
                addKeyValue("grip rotation", TextFormat(
                        "%.3f, %.3f, %.3f",
                        attachment.gripCorrection.rotationDegrees.x,
                        attachment.gripCorrection.rotationDegrees.y,
                        attachment.gripCorrection.rotationDegrees.z));
                addKeyValue("grip scale", TextFormat(
                        "%.4f", attachment.gripCorrection.scale));
                if (attachment.handPoseValid) {
                    addKeyValueStyled(
                            "hand transform",
                            FormatViewmodelTransform(
                                    attachment.handModelTransform),
                            smallConfig.mutedTextColor,
                            true);
                }
                if (attachment.attachmentWorldTransformValid) {
                    addKeyValueStyled(
                            "attachment transform",
                            FormatViewmodelTransform(
                                    attachment.attachmentWorldTransform),
                            smallConfig.mutedTextColor,
                            true);
                }
                if (!IsFpsViewmodelAttachmentRenderable(vm)) {
                    const char* reason = vm.equipState
                                            == FpsViewmodelEquipState::Holstered
                            ? "viewmodel is holstered"
                            : attachment.loadState
                                            == FpsViewmodelAttachmentLoadState::Pending
                                    ? "attachment resource or bone is pending"
                                    : !attachment.error.empty()
                                            ? attachment.error.c_str()
                                            : !attachment.handPoseValid
                                                    ? "current hand pose is unavailable"
                                                    : "attachment is not ready";
                    addKeyValueStyled(
                            "attachment hidden",
                            reason,
                            Color{236, 92, 92, 245},
                            true);
                }
                if (!attachment.error.empty()) {
                    addKeyValueStyled(
                            "attachment error",
                            attachment.error,
                            Color{236, 92, 92, 245},
                            true);
                }
                const FpsWeaponFiringRuntimeState& firing = vm.firing;
                addKeyValue("fire", TextFormat(
                        "shots %llu | cooldown %.3f | ready %s",
                        static_cast<unsigned long long>(firing.shotSequence),
                        firing.cooldownRemainingSeconds,
                        IsFpsViewmodelReadyForUse(vm) ? "yes" : "no"));
                addKeyValue(
                        "last fire gate",
                        FpsFireRejectReasonLabel(firing.lastRejectReason));
                addKeyValue("recoil", TextFormat(
                        "T %.4f %.4f %.4f | R %.3f %.3f %.3f",
                        firing.recoil.translation.x,
                        firing.recoil.translation.y,
                        firing.recoil.translation.z,
                        firing.recoil.rotationDegrees.x,
                        firing.recoil.rotationDegrees.y,
                        firing.recoil.rotationDegrees.z));
                addKeyValue("camera recoil config", TextFormat(
                        "%s | kick %.3f +/- %.3f | yaw %.3f | roll %.3f",
                        firing.definition.cameraRecoil.enabled ? "on" : "off",
                        firing.definition.cameraRecoil.pitchKickDegrees,
                        firing.definition.cameraRecoil.pitchVariationDegrees,
                        firing.definition.cameraRecoil.yawVariationDegrees,
                        firing.definition.cameraRecoil.rollVariationDegrees));
                addKeyValue("camera recoil spring", TextFormat(
                        "%.2f Hz zeta %.2f | max %.2f %.2f %.2f",
                        firing.definition.cameraRecoil.springFrequencyHz,
                        firing.definition.cameraRecoil.springDampingRatio,
                        firing.definition.cameraRecoil.maxPitchDegrees,
                        firing.definition.cameraRecoil.maxYawDegrees,
                        firing.definition.cameraRecoil.maxRollDegrees));
                addKeyValue("camera recoil state", TextFormat(
                        "R %.3f %.3f %.3f | V %.3f %.3f %.3f | kick %.3f %.3f %.3f",
                        firing.cameraRecoil.rotationDegrees.x,
                        firing.cameraRecoil.rotationDegrees.y,
                        firing.cameraRecoil.rotationDegrees.z,
                        firing.cameraRecoil.rotationVelocityDegrees.x,
                        firing.cameraRecoil.rotationVelocityDegrees.y,
                        firing.cameraRecoil.rotationVelocityDegrees.z,
                        firing.cameraRecoil.lastKickDegrees.x,
                        firing.cameraRecoil.lastKickDegrees.y,
                        firing.cameraRecoil.lastKickDegrees.z));
                addKeyValue("muzzle effects", TextFormat(
                        "socket %s | flash %s %.3f/%.3f (%.2f) soft %.2f radiance %.2f bloom %s | light %s %.3f",
                        firing.muzzleWorldTransformValid ? "valid" : "invalid",
                        firing.flash.active ? "active" : "off",
                        firing.flash.ageSeconds,
                        firing.flash.lifetimeSeconds,
                        firing.flash.lifetimeSeconds > 0.0f
                                ? std::clamp(
                                        firing.flash.ageSeconds
                                                / firing.flash.lifetimeSeconds,
                                        0.0f,
                                        1.0f)
                                : 1.0f,
                        firing.flash.edgeSoftness,
                        firing.flash.radianceStrength,
                        firing.flash.active && firing.flash.radianceStrength > 0.0f
                                ? "eligible" : "no",
                        firing.light.active ? "active" : "off",
                        FpsMuzzleLightCurrentIntensity(firing.light)));
                addKeyValue("flash shape", TextFormat(
                        "seed %u | lobes %d | scale %.3f | phase %.1f deg | stretch %.2f | rear %.2f",
                        firing.flash.shape.seed,
                        firing.flash.shape.lobeCount,
                        firing.flash.shape.overallScale,
                        firing.flash.shape.phaseRadians * RAD2DEG,
                        firing.flash.forwardStretch,
                        firing.flash.rearSuppression));
                if (firing.muzzleWorldTransformValid) {
                    addKeyValueStyled(
                            "muzzle transform",
                            FormatViewmodelTransform(firing.muzzleWorldTransform),
                            smallConfig.mutedTextColor,
                            true);
                }
                addKeyValue(
                        "flash capture space",
                        firing.emission.valid
                                ? "camera/viewmodel local"
                                : "no valid capture");
                if (firing.emission.valid) {
                    const Matrix capturedWorld =
                            ResolveFpsMuzzleEmissionTransform(
                                    firing.emission,
                                    preview.RenderCamera());
                    const Vector3 capturedPosition = Vector3Transform(
                            Vector3{}, capturedWorld);
                    const Vector3 capturedForward = Vector3Normalize(
                            Vector3Subtract(
                                    Vector3Transform(
                                            Vector3{0.0f, 0.0f, 1.0f},
                                            capturedWorld),
                                    capturedPosition));
                    addKeyValue("captured emission", TextFormat(
                            "P %.3f %.3f %.3f | F %.3f %.3f %.3f",
                            capturedPosition.x,
                            capturedPosition.y,
                            capturedPosition.z,
                            capturedForward.x,
                            capturedForward.y,
                            capturedForward.z));
                    if (firing.muzzleWorldTransformValid) {
                        const Vector3 livePosition = Vector3Transform(
                                Vector3{}, firing.muzzleWorldTransform);
                        addKeyValue("live/captured muzzle", TextFormat(
                                "live P %.3f %.3f %.3f | separation %.4f",
                                livePosition.x,
                                livePosition.y,
                                livePosition.z,
                                Vector3Distance(
                                        livePosition,
                                        capturedPosition)));
                    }
                    addKeyValue(
                            "muzzle-light origin",
                            "captured camera-local emission");
                }
                if (firing.hasLastShot) {
                    addKeyValue("last shot", TextFormat(
                            "%s | distance %.3f | sector %d line %d side %d",
                            firing.lastShot.hit ? "hit" : "miss",
                            firing.lastShot.distance,
                            firing.lastShot.sectorId,
                            firing.lastShot.lineDefId,
                            firing.lastShot.sideDefId));
                    if (firing.lastShot.hit) {
                        addKeyValue("last hit", TextFormat(
                                "P %.3f %.3f %.3f | N %.3f %.3f %.3f",
                                firing.lastShot.position.x,
                                firing.lastShot.position.y,
                                firing.lastShot.position.z,
                                firing.lastShot.normal.x,
                                firing.lastShot.normal.y,
                                firing.lastShot.normal.z));
                    }
                }
                if (!vm.error.empty()) addKeyValueStyled("error", vm.error, Color{236, 92, 92, 245}, true);
                break;
            }
            case PreviewDebugOverlayTab::Navigation: {
                const SectorNavigationBuildStatistics& stats = context.navigation.BuildStatistics();
                addKeyValue("state", SectorNavigationStateName(context.navigation.State()));
                addKeyValue("stage", SectorNavigationBuildStageName(context.navigation.BuildStage()));
                addKeyValue("source hash", TextFormat("%016llx",
                        static_cast<unsigned long long>(context.navigation.SourceHash())));
                addKeyValue("progress", TextFormat("%d / %d tile coordinates | %d layers",
                        stats.builtTileCoordinateCount,
                        stats.tileCoordinateCount,
                        stats.builtLayerCount));
                addKeyValue("build time", TextFormat("%.3f / %.3f ms last/peak",
                        stats.lastBuildMilliseconds,
                        stats.peakBuildMilliseconds));
                addKeyValue("mesh", TextFormat("%d tiles | %d polygons | %.3f MiB compressed",
                        stats.navMeshTileCount,
                        stats.navMeshPolygonCount,
                        static_cast<double>(stats.compressedLayerBytes) / (1024.0 * 1024.0)));
                addKeyValue("capacity", TextFormat("%d layer tiles | %d polys/tile | refs %d/%d bits",
                        stats.tileLayerCapacity,
                        context.navigation.Capacities().plannedMaximumPolygonsPerTile,
                        stats.tileReferenceBits,
                        stats.polygonReferenceBits));
                addKeyValue("tile memory", TextFormat("%.3f / %.3f MiB temporary peak/cap",
                        static_cast<double>(stats.tileTemporaryBytes) / (1024.0 * 1024.0),
                        static_cast<double>(context.navigation.Capacities().tileCacheTemporaryBytes)
                                / (1024.0 * 1024.0)));
                addKeyValue("voxel/tile", TextFormat("cs %.3f | ch %.3f | %d cells / %.2fm",
                        context.navigation.Settings().cellSize,
                        context.navigation.Settings().cellHeight,
                        context.navigation.Settings().tileSizeCells,
                        stats.tileWorldSize));
                addKeyValue("agent", TextFormat("radius %.3f | height %.3f | climb %.3f (map step)",
                        context.navigation.Settings().agentRadius,
                        context.navigation.Settings().agentHeight,
                        context.navigation.Settings().agentMaximumClimb));
                const SectorNavigationDynamicObstacleStatistics& obstacleStats =
                        context.navigation.DynamicObstacleStatistics();
                addKeyValue("dynamic obstacles", TextFormat(
                        "%zu active | %zu pending | %zu removing | %zu fast | %zu failed",
                        obstacleStats.activeCount,
                        obstacleStats.pendingCount,
                        obstacleStats.removingCount,
                        obstacleStats.fastSuppressedCount,
                        obstacleStats.failedCount));
                addKeyValue("obstacle updates", TextFormat(
                        "backlog %zu | %llu tiles | %.3f / %.3f ms last/peak",
                        obstacleStats.backlogCount,
                        static_cast<unsigned long long>(obstacleStats.updatedTiles),
                        obstacleStats.lastUpdateMilliseconds,
                        obstacleStats.peakUpdateMilliseconds));
                const SectorNavigationCrowdStatistics& crowdStats =
                        context.navigation.CrowdStatistics();
                addKeyValue("Crowd", TextFormat(
                        "%zu / %zu active | %s avoidance | %d velocity samples",
                        crowdStats.activeAgentCount,
                        context.navigation.Capacities().agentCapacity,
                        SectorNavigationAvoidanceQualityName(
                                context.navigation.CrowdSettings()
                                        .avoidanceQuality),
                        crowdStats.lastVelocitySampleCount));
                addKeyValue("Crowd updates", TextFormat(
                        "%llu sync | %llu failures | %.3f / %.3f ms last/peak",
                        static_cast<unsigned long long>(
                                crowdStats.reconciliations),
                        static_cast<unsigned long long>(
                                crowdStats.attachmentFailures),
                        crowdStats.lastUpdateMilliseconds,
                        crowdStats.peakUpdateMilliseconds));
                size_t shownObstacles = 0;
                for (const SectorNavigationDebugDynamicObstacle& obstacle :
                        context.navigation.DebugCache().dynamicObstacles) {
                    if (shownObstacles++ >= 8) break;
                    addKeyValue("dynamic obstacle", TextFormat(
                            "ID %d | %s | %.2f %.2f | %.2fx%.2f | Y %.2f..%.2f",
                            obstacle.placedObjectId,
                            SectorNavigationDynamicObstacleStateName(
                                    obstacle.state),
                            obstacle.center.x,
                            obstacle.center.y,
                            obstacle.halfExtents.x * 2.0f,
                            obstacle.halfExtents.y * 2.0f,
                            obstacle.bottom,
                            obstacle.top));
                }
                size_t shownUpdatedTiles = 0;
                for (auto tile = context.navigation.DebugCache()
                                     .recentlyUpdatedTiles.rbegin();
                        tile != context.navigation.DebugCache()
                                        .recentlyUpdatedTiles.rend()
                                && shownUpdatedTiles++ < 6;
                        ++tile) {
                    addKeyValue("updated tile", TextFormat(
                            "%d,%d layer %d | revision %llu",
                            tile->key.x,
                            tile->key.y,
                            tile->key.layer,
                            static_cast<unsigned long long>(tile->revision)));
                }
                const auto& diagnostics = context.navigation.Diagnostics();
                const SectorNavigationCounters& navigationCounters =
                        context.navigation.Counters();
                addKeyValue("lifecycle", TextFormat(
                        "%llu queued | %llu complete | %llu failed | revisions %llu/%llu/%llu",
                        static_cast<unsigned long long>(navigationCounters.rebuildRequests),
                        static_cast<unsigned long long>(navigationCounters.completedBuilds),
                        static_cast<unsigned long long>(navigationCounters.failedBuilds),
                        static_cast<unsigned long long>(context.navigation.SourceRevision()),
                        static_cast<unsigned long long>(context.navigation.BuildRevision()),
                        static_cast<unsigned long long>(
                                context.navigation.DebugCache().navigationRevision)));
                addKeyValue("diagnostics", TextFormat(
                        "%zu / %zu retained | %llu truncated | %llu dropped",
                        diagnostics.size(),
                        context.navigation.Capacities().diagnosticCapacity,
                        static_cast<unsigned long long>(
                                navigationCounters.truncatedDiagnostics),
                        static_cast<unsigned long long>(
                                navigationCounters.droppedDiagnostics)));
                if (!diagnostics.empty()) {
                    addKeyValueStyled("latest", diagnostics.back().message,
                            diagnostics.back().severity == SectorNavigationDiagnosticSeverity::Error
                                    ? Color{236, 92, 92, 245}
                                    : smallConfig.mutedTextColor,
                            true);
                }
                const NpcNavigationRecord* selectedAgent = nullptr;
                for (const NpcNavigationRecord& agent : context.npcNavigation.records) {
                    if (agent.occupied
                            && agent.placedObjectId
                                    == selectionState.selectedRuntimeObjectId) {
                        selectedAgent = &agent;
                        break;
                    }
                }
                size_t activeNpcRecords = 0;
                for (const NpcNavigationRecord& agent :
                        context.npcNavigation.records) {
                    if (agent.occupied) ++activeNpcRecords;
                }
                addKeyValue("NPC agents", TextFormat(
                        "%zu active / %zu slots | %llu requests | %llu replans | %llu stalls",
                        activeNpcRecords,
                        context.npcNavigation.records.size(),
                        static_cast<unsigned long long>(context.npcNavigation.counters.requests),
                        static_cast<unsigned long long>(context.npcNavigation.counters.replans),
                        static_cast<unsigned long long>(context.npcNavigation.counters.stalls)));
                size_t clearDoorLinks = 0;
                size_t openingDoorLinks = 0;
                size_t disabledDoorLinks = 0;
                uint32_t doorHolders = 0;
                for (const SectorNavigationDebugDoorLink& link :
                        context.navigation.DebugCache().doorLinks) {
                    doorHolders += link.holderCount;
                    if (link.state == SectorNavigationDoorLinkState::Clear) ++clearDoorLinks;
                    else if (link.state == SectorNavigationDoorLinkState::Disabled) ++disabledDoorLinks;
                    else ++openingDoorLinks;
                }
                addKeyValue("door links", TextFormat(
                        "%zu clear | %zu require open | %zu disabled | %u holders",
                        clearDoorLinks, openingDoorLinks, disabledDoorLinks,
                        doorHolders));
                size_t shownDoorLinks = 0;
                for (const SectorNavigationDebugDoorLink& link :
                        context.navigation.DebugCache().doorLinks) {
                    if (shownDoorLinks++ >= 8) break;
                    addKeyValue("door link", TextFormat(
                            "ID %d | %s | front <-> back | %u holders",
                            link.placedObjectId,
                            SectorNavigationDoorLinkStateName(link.state),
                            link.holderCount));
                }
                if (selectedAgent != nullptr) {
                    addKeyValue("selected NPC", TextFormat(
                            "%s | %s | %s | %s | request %llu",
                            selectedAgent->instanceId.c_str(),
                            NpcMoveAuthorityName(selectedAgent->authority),
                            NpcMovePhaseName(selectedAgent->phase),
                            NpcMoveGaitName(selectedAgent->gait),
                            static_cast<unsigned long long>(selectedAgent->requestId)));
                    addKeyValue("destination", TextFormat(
                            "%.2f %.2f | %zu corners remain | %s",
                            selectedAgent->requestedDestinationXZ.x,
                            selectedAgent->requestedDestinationXZ.y,
                            selectedAgent->cornerCount > selectedAgent->nextCorner
                                    ? selectedAgent->cornerCount - selectedAgent->nextCorner : 0,
                            SectorNavigationQueryStatusName(
                                    selectedAgent->lastQueryStatus)));
                    addKeyValue("motion", TextFormat(
                            "preferred %.2f | steered %.2f | actual %.2f | stall %.2fs",
                            Vector2Length(selectedAgent->preferredVelocity),
                            Vector2Length(selectedAgent->desiredVelocity),
                            Vector2Length(selectedAgent->actualVelocity),
                            selectedAgent->stallSeconds));
                    addKeyValue("Crowd agent", TextFormat(
                            "%s | %d neighbors | nearest %.2f | player avoid %s | %u replans",
                            selectedAgent->crowdAttached ? "attached" : "fallback",
                            selectedAgent->crowdNeighborCount,
                            selectedAgent->crowdNearestNeighborDistance,
                            selectedAgent->playerAvoidanceActive
                                    ? "active" : "off",
                            selectedAgent->replanCount));
                    addKeyValue("physical/visual Y", TextFormat(
                            "%.3f / %.3f | %s",
                            selectedAgent->physicalPosition.y,
                            selectedAgent->visualPosition.y,
                            selectedAgent->diagnostic.data()));
                    addKeyValue("door traversal", TextFormat(
                            "%s | door %d | direction %s | wait %.2fs | %s",
                            NpcDoorTraversalPhaseName(selectedAgent->doorPhase),
                            selectedAgent->doorId,
                            SectorNavigationDoorDirectionName(
                                    selectedAgent->doorDirection),
                            selectedAgent->doorWaitSeconds,
                            selectedAgent->holdsDoor ? "holding" : "not holding"));
                }
                addWrappedLine("Crowd handles NPC-to-NPC avoidance. Friendly NPCs additionally avoid the player; hostile NPCs approach until solid contact.");
                break;
            }
            case PreviewDebugOverlayTab::Controls:
                if (context.lightState.lightPilot.active) {
                    addWrappedLine("pilot light: WASD move, mouse look, Space/Ctrl up/down, hold Shift for precision movement. Unlock cursor with F11 to click Apply or Cancel.");
                } else if (context.lightState.proxyPlacement.active) {
                    const char* proxyName = context.lightState.proxyPlacement.proxyKind
                                    == LightProxyPlacementKind::Shaft
                            ? "shaft"
                            : "halo";
                    addWrappedLine(TextFormat(
                            "place %s: drag the %s handle across the view, use the mouse wheel for depth, and hold Shift for precision. Apply saves the offset; Cancel restores it.",
                            proxyName,
                            proxyName));
                } else if (controllerState.previewControlMode == SectorPreviewControlMode::Gameplay) {
                    addWrappedLine("movement: WASD move, Space jump, Shift run, Ctrl toggle crouch, mouse look. F11 unlocks cursor for UI tabs.");
                } else {
                    addWrappedLine("movement: WASD move, mouse look, Space/Ctrl up/down, hold Shift for precision movement. F11 unlocks cursor for UI tabs.");
                }
                addWrappedLine("hotkeys: left mouse fire, 1-6 weapon slots, H holster/equip viewmodel, F1 AO, F2 hide/show 3D UI, F3 control mode, F4 dynamic lights, F10 borderless window, Tab/Esc return to 2D.");
                break;
            case PreviewDebugOverlayTab::None:
                break;
        }
    }

    float contentH = 0.0f;
    for (const OverlayLine& line : lines) {
        contentH += line.wrap
                ? MeasureSectorEditorWrappedTextHeight(smallConfig, assets, smallFont, line.text.c_str(), contentW, 1)
                : rowH;
        contentH += 4.0f;
    }
    if (contentH > 0.0f) {
        contentH += gap;
    }
    if (drawExpanded && overlayState.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Probes) {
        contentH += (rowH + 6.0f) * 2.0f;
    }
    if (drawExpanded && overlayState.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Lighting) {
        contentH += rowH + 6.0f;
    }
    if (drawExpanded && overlayState.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Pbr) {
        contentH += (rowH + 6.0f) * 4.0f;
    }
    if (drawExpanded && overlayState.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Controls) {
        contentH += rowH + 6.0f;
    }
    if (drawExpanded && overlayState.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Navigation) {
        contentH += (rowH + 6.0f) * 6.0f;
    }
    const Rectangle panel{
            basePanel.x,
            basePanel.y,
            basePanel.width,
            padding + stripH + gap + tabH + contentH + padding};

    LightPilotKind selectedLightKind = LightPilotKind::None;
    int selectedLightId = -1;
    Vector3 selectedLightPositionWorld = {};
    const SectorLightProxySettings* selectedProxy = nullptr;
    bool selectedSpotLight = false;
    const bool hasSelectedLight = SelectedLightProxyPlacementInfo(
            topologyMap,
            selectionState,
            selectedLightKind,
            selectedLightId,
            selectedLightPositionWorld,
            selectedProxy,
            selectedSpotLight);
    const bool hasSelectedHalo = hasSelectedLight
            && selectedProxy != nullptr
            && selectedProxy->halo.enabled;
    const bool hasSelectedShaft = hasSelectedLight
            && selectedSpotLight
            && selectedProxy != nullptr
            && selectedProxy->shaft.enabled;

    LightProxyPlacementState& proxyPlacement = context.lightState.proxyPlacement;
    if (proxyPlacement.active) {
        const bool selectedEffectAvailable = proxyPlacement.proxyKind
                        == LightProxyPlacementKind::Halo
                ? hasSelectedHalo
                : hasSelectedShaft;
        if (!selectedEffectAvailable
                || proxyPlacement.kind != selectedLightKind
                || proxyPlacement.lightId != selectedLightId) {
            result.requestCancelProxyPlacement = true;
            proxyPlacement.dragging = false;
        } else {
            const bool placingShaft = proxyPlacement.proxyKind
                    == LightProxyPlacementKind::Shaft;
            const Vector3 offsetWorld = placingShaft
                    ? selectedProxy->shaft.originOffsetWorld
                    : selectedProxy->halo.centerOffsetWorld;
            const Camera3D camera = preview.RenderCamera();
            Vector3 cameraForward = Vector3Subtract(camera.target, camera.position);
            cameraForward = Vector3LengthSqr(cameraForward) > 0.000001f
                    ? Vector3Normalize(cameraForward)
                    : Vector3{0.0f, 0.0f, -1.0f};
            Vector3 previewCenterWorld = Vector3Add(
                    selectedLightPositionWorld, offsetWorld);
            const Vector3 centerFromCamera = Vector3Subtract(
                    previewCenterWorld, camera.position);
            const bool centerInFront = Vector3DotProduct(
                    centerFromCamera, cameraForward) > 0.001f;
            const Vector2 centerScreen = GetWorldToScreenEx(
                    previewCenterWorld,
                    camera,
                    static_cast<int>(EditorWidth),
                    static_cast<int>(EditorHeight));
            const Vector2 lightScreen = GetWorldToScreenEx(
                    selectedLightPositionWorld,
                    camera,
                    static_cast<int>(EditorWidth),
                    static_cast<int>(EditorHeight));
            const Rectangle previewViewport{0.0f, 0.0f, EditorWidth, EditorHeight};
            const Vector2 mousePosition = input.MousePosition();
            const bool mouseOverWorld = Contains(previewViewport, mousePosition)
                    && !Contains(panel, mousePosition);
            const float handleDx = mousePosition.x - centerScreen.x;
            const float handleDy = mousePosition.y - centerScreen.y;
            constexpr float ProxyHandleRadius = 18.0f;
            const bool handleHovered = centerInFront
                    && handleDx * handleDx + handleDy * handleDy
                            <= ProxyHandleRadius * ProxyHandleRadius;
            const bool precision = input.IsKeyDown(KEY_LEFT_SHIFT)
                    || input.IsKeyDown(KEY_RIGHT_SHIFT);

            input.ForEachEvent(
                    engine::InputEventType::MouseButtonPressed,
                    true,
                    [&](engine::InputEvent& event) {
                        if (event.mouseButton.button != MOUSE_LEFT_BUTTON
                                || !mouseOverWorld || !handleHovered) return;
                        const Ray ray = GetScreenToWorldRayEx(
                                event.mouseButton.position,
                                camera,
                                static_cast<int>(EditorWidth),
                                static_cast<int>(EditorHeight));
                        Vector3 intersection = {};
                        if (!IntersectSectorEditorLightProxyPlacementPlane(
                                    ray,
                                    previewCenterWorld,
                                    cameraForward,
                                    intersection)) return;
                        proxyPlacement.dragging = true;
                        proxyPlacement.dragPlanePointWorld = previewCenterWorld;
                        proxyPlacement.dragPlaneNormalWorld = cameraForward;
                        proxyPlacement.dragStartIntersectionWorld = intersection;
                        proxyPlacement.dragStartCenterWorld = previewCenterWorld;
                        engine::ConsumeEvent(event);
                    });

            if (proxyPlacement.dragging && input.IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                const Ray ray = GetScreenToWorldRayEx(
                        mousePosition,
                        camera,
                        static_cast<int>(EditorWidth),
                        static_cast<int>(EditorHeight));
                Vector3 intersection = {};
                if (IntersectSectorEditorLightProxyPlacementPlane(
                            ray,
                            proxyPlacement.dragPlanePointWorld,
                            proxyPlacement.dragPlaneNormalWorld,
                            intersection)) {
                    previewCenterWorld = ApplySectorEditorLightProxyPlacementDrag(
                            proxyPlacement.dragStartCenterWorld,
                            proxyPlacement.dragStartIntersectionWorld,
                            intersection,
                            precision);
                    result.previewProxyOffsetChanged = true;
                    result.previewProxyOffsetWorld = Vector3Subtract(
                            previewCenterWorld, selectedLightPositionWorld);
                }
            }
            input.ForEachEvent(
                    engine::InputEventType::MouseButtonReleased,
                    true,
                    [&](engine::InputEvent& event) {
                        if (event.mouseButton.button != MOUSE_LEFT_BUTTON
                                || !proxyPlacement.dragging) return;
                        proxyPlacement.dragging = false;
                        engine::ConsumeEvent(event);
                    });
            input.ForEachEvent(
                    engine::InputEventType::MouseWheel,
                    true,
                    [&](engine::InputEvent& event) {
                        if (!mouseOverWorld) return;
                        previewCenterWorld = ApplySectorEditorLightProxyPlacementDepth(
                                previewCenterWorld,
                                camera.position,
                                cameraForward,
                                event.wheel.value,
                                precision);
                        result.previewProxyOffsetChanged = true;
                        result.previewProxyOffsetWorld = Vector3Subtract(
                                previewCenterWorld, selectedLightPositionWorld);
                        engine::ConsumeEvent(event);
                    });

            if (centerInFront) {
                DrawLineEx(lightScreen, centerScreen, 2.0f, Color{255, 190, 72, 210});
                DrawCircleV(centerScreen, ProxyHandleRadius, Color{20, 22, 28, 220});
                DrawCircleLines(
                        static_cast<int>(std::round(centerScreen.x)),
                        static_cast<int>(std::round(centerScreen.y)),
                        ProxyHandleRadius,
                        handleHovered || proxyPlacement.dragging
                                ? Color{255, 236, 122, 255}
                                : Color{255, 190, 72, 245});
                DrawLineEx(
                        Vector2{centerScreen.x - 8.0f, centerScreen.y},
                        Vector2{centerScreen.x + 8.0f, centerScreen.y},
                        2.0f,
                        Color{255, 236, 122, 255});
                DrawLineEx(
                        Vector2{centerScreen.x, centerScreen.y - 8.0f},
                        Vector2{centerScreen.x, centerScreen.y + 8.0f},
                        2.0f,
                        Color{255, 236, 122, 255});
                engine::Text(
                        smallConfig,
                        assets,
                        Rectangle{centerScreen.x + 24.0f, centerScreen.y - 12.0f, 190.0f, 24.0f},
                        smallFont,
                        placingShaft ? "Shaft origin" : "Halo center",
                        engine::UITextJustify::Left,
                        Color{255, 236, 122, 255},
                        true);
            }
        }
    }

    DrawRectangleRec(panel, Color{12, 15, 20, 205});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    const float actionY = panel.y + padding - 2.0f;
    const SectorEditorPreviewLightStartActionLayout lightStartActions =
            BuildSectorEditorPreviewLightStartActionLayout(
                    panel,
                    padding,
                    actionY,
                    hasSelectedHalo,
                    hasSelectedShaft);
    float actionReservedWidth = 0.0f;
    if (mouseInteractive && context.lightState.lightPilot.active) {
        actionReservedWidth = 158.0f;
    } else if (mouseInteractive && context.lightState.proxyPlacement.active) {
        actionReservedWidth = 234.0f;
    } else if (mouseInteractive
            && hasSelectedLight
            && controllerState.previewControlMode == SectorPreviewControlMode::FreeFly) {
        actionReservedWidth = lightStartActions.reservedWidth + 10.0f;
    }
    engine::Text(
            smallConfig,
            assets,
            Rectangle{
                    panel.x + padding,
                    panel.y + padding,
                    actionReservedWidth > 0.0f
                            ? contentW - actionReservedWidth
                            : contentW,
                    stripH},
            smallFont,
            compactStatus.c_str(),
            engine::UITextJustify::Left,
            context.topologyDocumentDirty ? Color{236, 196, 92, 255} : smallConfig.textColor,
            true);

    float actionsRight = panel.x + panel.width - padding;
    if (mouseInteractive) {
        if (context.lightState.lightPilot.active) {
            if (engine::Button(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        "sector_editor_preview_light_pilot_cancel",
                        Rectangle{actionsRight - 72.0f, actionY, 72.0f, 28.0f},
                        smallFont,
                        "Cancel")) {
                result.requestCancelLightPilot = true;
            }
            actionsRight -= 82.0f;
            if (engine::Button(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        "sector_editor_preview_light_pilot_apply",
                        Rectangle{actionsRight - 66.0f, actionY, 66.0f, 28.0f},
                        smallFont,
                        "Apply")) {
                result.requestApplyLightPilot = true;
            }
        } else if (context.lightState.proxyPlacement.active) {
            const bool placingShaft = context.lightState.proxyPlacement.proxyKind
                    == LightProxyPlacementKind::Shaft;
            const char* proxyId = placingShaft ? "shaft" : "halo";
            if (engine::Button(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        TextFormat("sector_editor_preview_%s_place_cancel", proxyId),
                        Rectangle{actionsRight - 72.0f, actionY, 72.0f, 28.0f},
                        smallFont,
                        "Cancel")) {
                result.requestCancelProxyPlacement = true;
            }
            actionsRight -= 82.0f;
            if (engine::Button(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        TextFormat("sector_editor_preview_%s_place_apply", proxyId),
                        Rectangle{actionsRight - 66.0f, actionY, 66.0f, 28.0f},
                        smallFont,
                        "Apply")) {
                result.requestApplyProxyPlacement = true;
            }
            actionsRight -= 76.0f;
            if (engine::Button(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        TextFormat("sector_editor_preview_%s_place_reset", proxyId),
                        Rectangle{actionsRight - 66.0f, actionY, 66.0f, 28.0f},
                        smallFont,
                        "Reset")) {
                result.previewProxyOffsetChanged = true;
                result.previewProxyOffsetWorld = {};
            }
        } else if (hasSelectedLight && controllerState.previewControlMode == SectorPreviewControlMode::FreeFly) {
            if (engine::Button(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        "sector_editor_preview_light_pilot_start",
                        lightStartActions.pilot,
                        smallFont,
                        "Pilot")) {
                result.requestStartLightPilot = true;
            }
            if (hasSelectedHalo) {
                if (engine::Button(
                            ui,
                            smallConfig,
                            input,
                            assets,
                            "sector_editor_preview_halo_place_start",
                            lightStartActions.halo,
                            smallFont,
                            "Place Halo")) {
                    result.requestStartProxyPlacement = LightProxyPlacementKind::Halo;
                }
            }
            if (hasSelectedShaft) {
                if (engine::Button(
                            ui,
                            smallConfig,
                            input,
                            assets,
                            "sector_editor_preview_shaft_place_start",
                            lightStartActions.shaft,
                            smallFont,
                            "Place Shaft")) {
                    result.requestStartProxyPlacement = LightProxyPlacementKind::Shaft;
                }
            }
        }
    }

    const float tabY = panel.y + padding + stripH + gap;
    const float tabGap = 6.0f;
    for (size_t index = 0; index < SectorEditorPreviewDebugTabs.size(); ++index) {
        const Rectangle tabRect = BuildSectorEditorPreviewDebugTabRect(
                panel, padding, stripH, gap, tabH, tabGap, index);
        const SectorEditorPreviewDebugTabDefinition& tab = SectorEditorPreviewDebugTabs[index];
        const bool selected = overlayState.activePreviewDebugOverlayTab == tab.tab;
        if (mouseInteractive) {
            if (engine::ToolButton(ui, smallConfig, input, assets, tab.id, tabRect, smallFont, tab.label, selected)) {
                overlayState.activePreviewDebugOverlayTab = selected
                        ? PreviewDebugOverlayTab::None
                        : tab.tab;
            }
        } else {
            DrawRectangleRec(tabRect, selected ? Color{48, 68, 86, 210} : Color{24, 30, 38, 185});
            DrawRectangleLinesEx(tabRect, config.borderThickness, config.borderColor);
            engine::Text(smallConfig, assets, tabRect, smallFont, tab.label, engine::UITextJustify::Center, smallConfig.mutedTextColor);
        }
    }

    float y = tabY + tabH + gap;
    if (drawExpanded
            && overlayState.activePreviewDebugOverlayTab
                    == PreviewDebugOverlayTab::Viewmodel
            && mouseInteractive) {
        if (engine::Button(
                    ui,
                    smallConfig,
                    input,
                    assets,
                    "sector_editor_preview_open_weapon_editor",
                    Rectangle{panel.x + padding, y, 190.0f, rowH},
                    smallFont,
                    "Open Weapon Editor")) {
            result.openWeaponEditor = true;
        }
        y += rowH + gap;
    }
    if (drawExpanded && overlayState.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Pbr) {
        SectorPbrContributionSettings settings = preview.PbrContributionSettings();
        const char* modeOptions[] = {
                "Full PBR",
                "Base Color",
                "Direct Diffuse",
                "Direct Specular",
                "Probe / Indirect Diffuse",
                "Environment Specular",
                "Emissive",
                "Material AO",
                "Metallic / Roughness",
                "Shading Normal",
                "Tangent-Space Normal"};
        int selectedMode = static_cast<int>(settings.diagnosticMode);
        engine::Text(smallConfig, assets,
                Rectangle{panel.x + padding, y, 94.0f, rowH},
                smallFont, "PBR Output", engine::UITextJustify::Left,
                smallConfig.textColor);
        const Rectangle modeRect{panel.x + padding + 100.0f, y, 250.0f, rowH};
        if (mouseInteractive) {
            if (engine::Option(
                        ui, smallConfig, input, assets,
                        "sector_editor_preview_pbr_diagnostic_mode",
                        modeRect, smallFont, modeOptions,
                        sizeof(modeOptions) / sizeof(modeOptions[0]),
                        selectedMode)) {
                settings.diagnosticMode = static_cast<SectorPbrDiagnosticMode>(
                        selectedMode);
                preview.SetPbrContributionSettings(settings);
            }
        } else {
            DrawRectangleRec(modeRect, Color{24, 30, 38, 155});
            DrawRectangleLinesEx(modeRect, config.borderThickness, config.borderColor);
            engine::Text(smallConfig, assets, modeRect, smallFont,
                    SectorPbrDiagnosticModeName(settings.diagnosticMode),
                    engine::UITextJustify::Center, smallConfig.mutedTextColor);
        }
        y += rowH + 6.0f;

        const char* bloomViews[] = {
                "Normal", "Scene Before", "HDR Prefilter", "Blurred Bloom",
                "Bloom Only", "Scene After"};
        int bloomView=static_cast<int>(preview.BloomDebugView());
        engine::Text(smallConfig, assets,
                Rectangle{panel.x+padding,y,94.0f,rowH},smallFont,"Bloom View",
                engine::UITextJustify::Left,smallConfig.textColor);
        const Rectangle bloomViewRect{panel.x+padding+100.0f,y,250.0f,rowH};
        if(mouseInteractive) {
            if(engine::Option(ui,smallConfig,input,assets,
                        "sector_editor_preview_bloom_debug_view",bloomViewRect,
                        smallFont,bloomViews,sizeof(bloomViews)/sizeof(bloomViews[0]),
                        bloomView)) {
                preview.SetBloomDebugView(static_cast<SectorBloomDebugView>(bloomView));
            }
        } else {
            DrawRectangleRec(bloomViewRect,Color{24,30,38,155});
            DrawRectangleLinesEx(bloomViewRect,config.borderThickness,config.borderColor);
            engine::Text(smallConfig,assets,bloomViewRect,smallFont,
                    SectorBloomDebugViewName(preview.BloomDebugView()),
                    engine::UITextJustify::Center,smallConfig.mutedTextColor);
        }
        y+=rowH+6.0f;

        const SectorBloomDiagnostics& bloomDiagnostics=preview.BloomDiagnostics();
        const char* bloomStatus=TextFormat(
                "%dx%d -> %dx%d RGBA16F; T %.2f K %.2f I %.2f R %.2f; half guard; %s",
                bloomDiagnostics.sceneWidth,bloomDiagnostics.sceneHeight,
                bloomDiagnostics.bloomWidth,bloomDiagnostics.bloomHeight,
                bloomDiagnostics.settings.threshold,
                bloomDiagnostics.settings.softKnee,
                bloomDiagnostics.settings.intensity,
                bloomDiagnostics.settings.radius,
                bloomDiagnostics.status.c_str());
        engine::Text(smallConfig,assets,
                Rectangle{panel.x+padding,y,contentW,rowH},smallFont,
                bloomStatus,engine::UITextJustify::Left,
                bloomDiagnostics.disabled?Color{235,145,110,255}:smallConfig.mutedTextColor);
        y+=rowH+6.0f;

        const char* atmosphereStatus=TextFormat(
                "dust=%s; scratch=%s",
                preview.DustResourceDiagnostic().c_str(),
                preview.HdrSceneScratchDiagnostic().c_str());
        engine::Text(smallConfig,assets,
                Rectangle{panel.x+padding,y,contentW,rowH},smallFont,
                atmosphereStatus,engine::UITextJustify::Left,
                smallConfig.mutedTextColor);
        y+=rowH+6.0f;

        const auto drawScalePresets = [&](const char* idPrefix,
                                          const char* label,
                                          float value,
                                          bool environmentScale) {
            engine::Text(smallConfig, assets,
                    Rectangle{panel.x + padding, y, 132.0f, rowH},
                    smallFont, label, engine::UITextJustify::Left,
                    smallConfig.textColor);
            constexpr float Values[] = {0.0f, 0.25f, 0.5f, 1.0f};
            constexpr const char* Labels[] = {"0", ".25", ".5", "1"};
            for (int i = 0; i < 4; ++i) {
                const Rectangle buttonRect{
                        panel.x + padding + 138.0f + i * 58.0f,
                        y,
                        52.0f,
                        rowH};
                const std::string id = std::string(idPrefix) + Labels[i];
                const bool selected = std::fabs(value - Values[i]) < 0.0001f;
                if (mouseInteractive) {
                    if (engine::ToolButton(
                                ui, smallConfig, input, assets,
                                id.c_str(), buttonRect, smallFont,
                                Labels[i], selected)) {
                        SectorPbrContributionSettings edited =
                                preview.PbrContributionSettings();
                        if (environmentScale) {
                            edited.worldEnvironmentSpecularScale = Values[i];
                        } else {
                            edited.worldIndirectDiffuseScale = Values[i];
                        }
                        preview.SetPbrContributionSettings(edited);
                    }
                } else {
                    DrawRectangleRec(buttonRect, selected
                            ? Color{48, 68, 86, 210}
                            : Color{24, 30, 38, 155});
                    DrawRectangleLinesEx(buttonRect, config.borderThickness, config.borderColor);
                    engine::Text(smallConfig, assets, buttonRect, smallFont,
                            Labels[i], engine::UITextJustify::Center,
                            smallConfig.mutedTextColor);
                }
            }
            y += rowH + 6.0f;
        };
        settings = preview.PbrContributionSettings();
        drawScalePresets(
                "sector_editor_preview_pbr_indirect_",
                "World Indirect",
                settings.worldIndirectDiffuseScale,
                false);
        settings = preview.PbrContributionSettings();
        drawScalePresets(
                "sector_editor_preview_pbr_environment_",
                "World Env Spec",
                settings.worldEnvironmentSpecularScale,
                true);
        const Rectangle resetRect{panel.x + padding, y, 112.0f, rowH};
        if (mouseInteractive && engine::Button(
                    ui, smallConfig, input, assets,
                    "sector_editor_preview_pbr_reset",
                    resetRect, smallFont, "Reset PBR")) {
            preview.SetPbrContributionSettings(SectorPbrContributionSettings{});
        } else if (!mouseInteractive) {
            DrawRectangleRec(resetRect, Color{24, 30, 38, 155});
            DrawRectangleLinesEx(resetRect, config.borderThickness, config.borderColor);
            engine::Text(smallConfig, assets, resetRect, smallFont,
                    "Reset PBR", engine::UITextJustify::Center,
                    smallConfig.mutedTextColor);
        }
        y += rowH + 6.0f;
    }
    if (drawExpanded && overlayState.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Probes) {
        const Rectangle checkboxRect{panel.x + padding, y, 240.0f, rowH};
        if (mouseInteractive) {
            engine::Checkbox(
                    ui,
                    smallConfig,
                    input,
                    assets,
                    "sector_editor_show_object_probe_debug_overlay",
                    checkboxRect,
                    smallFont,
                    "Show Object Probes",
                    overlayState.showObjectProbeDebugOverlay);
        } else {
            DrawRectangleRec(checkboxRect, Color{24, 30, 38, 155});
            DrawRectangleLinesEx(checkboxRect, config.borderThickness, config.borderColor);
            const float boxSize = 12.0f;
            const Rectangle box{
                    checkboxRect.x + smallConfig.paddingX,
                    checkboxRect.y + (checkboxRect.height - boxSize) * 0.5f,
                    boxSize,
                    boxSize};
            DrawRectangleRec(box, Color{12, 15, 20, 205});
            DrawRectangleLinesEx(box, config.borderThickness, config.borderColor);
            if (overlayState.showObjectProbeDebugOverlay) {
                constexpr float markPadding = 3.0f;
                const Rectangle mark{
                        box.x + markPadding,
                        box.y + markPadding,
                        box.width - markPadding * 2.0f,
                        box.height - markPadding * 2.0f};
                DrawRectangleRec(mark, smallConfig.accentColor);
            }
            const float labelX = box.x + box.width + smallConfig.paddingX;
            engine::Text(
                    smallConfig,
                    assets,
                    Rectangle{labelX, checkboxRect.y, checkboxRect.x + checkboxRect.width - labelX, checkboxRect.height},
                    smallFont,
                    "Show Object Probes",
                    engine::UITextJustify::Left,
                    smallConfig.mutedTextColor);
        }
        y += rowH + 6.0f;

        const bool selectedReflectionProbe =
                selectionState.selectedAuthoring.kind
                        == SectorAuthoringSelectionKind::ReflectionProbe;
        const float bakeGap = 8.0f;
        const float bakeWidth = (contentW - bakeGap) * 0.5f;
        const Rectangle bakeSelectedRect{panel.x + padding, y, bakeWidth, rowH};
        const Rectangle bakeAllRect{
                bakeSelectedRect.x + bakeWidth + bakeGap, y, bakeWidth, rowH};
        if (mouseInteractive && selectedReflectionProbe) {
            if (engine::Button(
                        ui, smallConfig, input, assets,
                        "sector_editor_preview_bake_selected_reflection_probe",
                        bakeSelectedRect, smallFont, "Bake Selected Reflection")) {
                result.requestBakeSelectedReflectionProbe = true;
            }
        } else {
            DrawRectangleRec(bakeSelectedRect, Color{24, 30, 38, 155});
            DrawRectangleLinesEx(
                    bakeSelectedRect, config.borderThickness, config.borderColor);
            engine::Text(
                    smallConfig, assets, bakeSelectedRect, smallFont,
                    selectedReflectionProbe
                            ? "Bake Selected Reflection"
                            : "Select Reflection in 2D",
                    engine::UITextJustify::Center,
                    smallConfig.mutedTextColor);
        }
        if (mouseInteractive && !topologyMap.compiledReflectionProbes.empty()) {
            if (engine::Button(
                        ui, smallConfig, input, assets,
                        "sector_editor_preview_bake_all_reflection_probes",
                        bakeAllRect, smallFont, "Bake All Reflections")) {
                result.requestBakeAllReflectionProbes = true;
            }
        } else {
            DrawRectangleRec(bakeAllRect, Color{24, 30, 38, 155});
            DrawRectangleLinesEx(
                    bakeAllRect, config.borderThickness, config.borderColor);
            engine::Text(
                    smallConfig, assets, bakeAllRect, smallFont,
                    "Bake All Reflections",
                    engine::UITextJustify::Center,
                    smallConfig.mutedTextColor);
        }
        y += rowH + 6.0f;

        const float distanceLabelW = 144.0f;
        const float distanceInputW = 104.0f;
        const Rectangle distanceLabelRect{panel.x + padding, y, distanceLabelW, rowH};
        const Rectangle distanceInputRect{
                panel.x + padding + distanceLabelW + gap,
                y,
                distanceInputW,
                rowH};
        const SectorPreviewSettings normalizedPreviewSettings =
                NormalizeSectorPreviewSettings(topologyMap.previewSettings);
        if (mouseInteractive) {
            const SectorEditorFloatInputResult distanceResult = DrawLabeledFloatInput(
                    ui,
                    smallConfig,
                    input,
                    assets,
                    smallFont,
                    "sector_editor_object_probe_debug_draw_max_distance",
                    "Probe Draw Distance",
                    distanceLabelRect,
                    distanceInputRect,
                    engine::UITextJustify::Left,
                    normalizedPreviewSettings.objectProbeDebugDrawMaxDistanceWorld,
                    context.objectProbeDebugDrawMaxDistanceInput,
                    0.0f,
                    512.0f,
                    1);
            if (distanceResult.changed) {
                SectorPreviewSettings editedPreviewSettings = topologyMap.previewSettings;
                editedPreviewSettings.objectProbeDebugDrawMaxDistanceWorld =
                        distanceResult.finite
                        ? distanceResult.value
                        : normalizedPreviewSettings.objectProbeDebugDrawMaxDistanceWorld;
                topologyMap.previewSettings =
                        NormalizeSectorPreviewSettings(editedPreviewSettings);
                result.markTopologyDocumentEdited = true;
                result.topologyDocumentEditStatus = "Object probe debug draw distance updated";
            }
        } else {
            engine::Text(
                    smallConfig,
                    assets,
                    distanceLabelRect,
                    smallFont,
                    "Probe Draw Distance",
                    engine::UITextJustify::Left,
                    smallConfig.mutedTextColor);
            DrawRectangleRec(distanceInputRect, Color{24, 30, 38, 155});
            DrawRectangleLinesEx(distanceInputRect, config.borderThickness, config.borderColor);
            const float distanceValue = normalizedPreviewSettings.objectProbeDebugDrawMaxDistanceWorld;
            engine::Text(
                    smallConfig,
                    assets,
                    distanceInputRect,
                    smallFont,
                    distanceValue <= 0.0f ? "All" : TextFormat("%.1f", distanceValue),
                    engine::UITextJustify::Center,
                    smallConfig.mutedTextColor);
        }
        y += rowH + 6.0f;
    }
    if (drawExpanded && overlayState.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Navigation) {
        const Rectangle rebuildRect{panel.x + padding, y, 132.0f, rowH};
        const bool rebuildAvailable = context.navigation.State()
                        != SectorNavigationState::Queued
                && context.navigation.State() != SectorNavigationState::Building;
        if (mouseInteractive && rebuildAvailable) {
            if (engine::Button(
                        ui, smallConfig, input, assets,
                        "sector_editor_navigation_rebuild", rebuildRect,
                        smallFont, "Rebuild Nav")) {
                result.requestNavigationRebuild = true;
            }
        } else {
            DrawRectangleRec(rebuildRect, Color{24, 30, 38, 155});
            DrawRectangleLinesEx(rebuildRect, config.borderThickness, config.borderColor);
            engine::Text(smallConfig, assets, rebuildRect, smallFont,
                    rebuildAvailable ? "Rebuild Nav" : "Building...",
                    engine::UITextJustify::Center, smallConfig.mutedTextColor);
        }
        y += rowH + 6.0f;

        const auto drawNavigationCheckbox = [&](const char* id, const char* label,
                                                 Rectangle rect, bool& value) {
            if (mouseInteractive) {
                engine::Checkbox(ui, smallConfig, input, assets, id, rect, smallFont, label, value);
            } else {
                DrawRectangleRec(rect, Color{24, 30, 38, 155});
                DrawRectangleLinesEx(rect, config.borderThickness, config.borderColor);
                const std::string text = std::string(value ? "[x] " : "[ ] ") + label;
                engine::Text(smallConfig, assets, rect, smallFont, text.c_str(),
                        engine::UITextJustify::Left, smallConfig.mutedTextColor);
            }
        };
        const float checkboxWidth = (contentW - gap) * 0.5f;
        drawNavigationCheckbox(
                "sector_editor_navigation_surface", "Walkable Surface",
                Rectangle{panel.x + padding, y, checkboxWidth, rowH},
                overlayState.showNavigationSurface);
        drawNavigationCheckbox(
                "sector_editor_navigation_edges", "Polygon Edges",
                Rectangle{panel.x + padding + checkboxWidth + gap, y, checkboxWidth, rowH},
                overlayState.showNavigationEdges);
        y += rowH + 6.0f;
        drawNavigationCheckbox(
                "sector_editor_navigation_tiles", "Tile Bounds",
                Rectangle{panel.x + padding, y, checkboxWidth, rowH},
                overlayState.showNavigationTileBounds);
        drawNavigationCheckbox(
                "sector_editor_navigation_obstacles", "Static Obstacles",
                Rectangle{panel.x + padding + checkboxWidth + gap, y, checkboxWidth, rowH},
                overlayState.showNavigationStaticObstacles);
        y += rowH + 6.0f;
        drawNavigationCheckbox(
                "sector_editor_navigation_dynamic_obstacles", "Dynamic Obstacles",
                Rectangle{panel.x + padding, y, checkboxWidth, rowH},
                overlayState.showNavigationDynamicObstacles);
        y += rowH + 6.0f;
        drawNavigationCheckbox(
                "sector_editor_navigation_doors", "Door Placeholders",
                Rectangle{panel.x + padding, y, checkboxWidth, rowH},
                overlayState.showNavigationDoorPlaceholders);
        drawNavigationCheckbox(
                "sector_editor_navigation_steps", "Step Connections",
                Rectangle{panel.x + padding + checkboxWidth + gap, y, checkboxWidth, rowH},
                overlayState.showNavigationStepConnections);
        y += rowH + 6.0f;
        drawNavigationCheckbox(
                "sector_editor_navigation_npc_paths", "NPC Paths",
                Rectangle{panel.x + padding, y, checkboxWidth, rowH},
                overlayState.showNavigationNpcPaths);
        drawNavigationCheckbox(
                "sector_editor_navigation_npc_agents", "NPC Agents",
                Rectangle{panel.x + padding + checkboxWidth + gap, y, checkboxWidth, rowH},
                overlayState.showNavigationNpcAgents);
        y += rowH + 6.0f;
        drawNavigationCheckbox(
                "sector_editor_navigation_selected_npc", "Selected NPC Only",
                Rectangle{panel.x + padding, y, checkboxWidth, rowH},
                overlayState.showNavigationSelectedNpcOnly);
        y += rowH + 6.0f;
    }
    if (drawExpanded && overlayState.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Controls) {
        const Rectangle settingsRect{panel.x + padding, y, 112.0f, rowH};
        if (mouseInteractive) {
            if (engine::Button(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        "sector_editor_preview_settings",
                        settingsRect,
                        smallFont,
                        "Settings")) {
                result.openPreviewSettings = true;
            }
        } else {
            DrawRectangleRec(settingsRect, Color{24, 30, 38, 155});
            DrawRectangleLinesEx(settingsRect, config.borderThickness, config.borderColor);
            engine::Text(
                    smallConfig,
                    assets,
                    settingsRect,
                    smallFont,
                    "Settings",
                    engine::UITextJustify::Center,
                    smallConfig.mutedTextColor);
        }
        y += rowH + 6.0f;
    }
    for (const OverlayLine& line : lines) {
        const float lineH = line.wrap
                ? MeasureSectorEditorWrappedTextHeight(smallConfig, assets, smallFont, line.text.c_str(), contentW, 1)
                : rowH;
        engine::Text(
                smallConfig,
                assets,
                Rectangle{panel.x + padding, y, contentW, lineH},
                smallFont,
                line.text.c_str(),
                engine::UITextJustify::Left,
                line.color,
                line.wrap);
        y += lineH + 4.0f;
    }

    PreviewObjectAdjustmentState& adjustment =
            context.runtimeObjectEditingState.previewAdjustment;
    if (adjustment.active) {
        const Rectangle adjustmentPanel =
                BuildSectorEditorPreviewAdjustmentPanelRect();
        DrawRectangleRec(adjustmentPanel, Color{12, 15, 20, 225});
        DrawRectangleLinesEx(
                adjustmentPanel,
                config.borderThickness,
                Color{84, 204, 255, 255});
        const SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(
                topologyMap, adjustment.objectId);
        const float textX = adjustmentPanel.x + 14.0f;
        float adjustmentY = adjustmentPanel.y + 12.0f;
        const float adjustmentWidth = adjustmentPanel.width - 28.0f;
        engine::Text(
                smallConfig,
                assets,
                Rectangle{textX, adjustmentY, adjustmentWidth, rowH},
                smallFont,
                object != nullptr
                        ? TextFormat("Adjust %s %d",
                                SectorEditorPreviewObjectKindName(*object),
                                object->id)
                        : "Adjustment target missing",
                engine::UITextJustify::Left,
                Color{84, 204, 255, 255});
        adjustmentY += rowH + 4.0f;
        if (object != nullptr) {
            engine::Text(
                    smallConfig,
                    assets,
                    Rectangle{textX, adjustmentY, adjustmentWidth, rowH},
                    smallFont,
                    TextFormat("X %.2f m   Z %.2f m   Height %+.2f m",
                            SectorAuthoringToWorldDistance(object->position.x),
                            SectorAuthoringToWorldDistance(object->position.z),
                            SectorEditorPreviewObjectHeightOffsetWorld(*object)),
                    engine::UITextJustify::Left);
            adjustmentY += rowH + 2.0f;
            engine::Text(
                    smallConfig,
                    assets,
                    Rectangle{textX, adjustmentY, adjustmentWidth, rowH},
                    smallFont,
                    TextFormat("Yaw %.2f deg", object->yawRadians * RAD2DEG),
                    engine::UITextJustify::Left);
            adjustmentY += rowH + 8.0f;
        }

        constexpr float presetGap = 6.0f;
        const float presetWidth =
                (adjustmentWidth - presetGap * 2.0f) / 3.0f;
        const auto presetButton = [&](const char* id, const char* label,
                                      PreviewObjectNudgePreset preset,
                                      int index) {
            const Rectangle bounds{
                    textX + static_cast<float>(index)
                                    * (presetWidth + presetGap),
                    adjustmentY,
                    presetWidth,
                    rowH};
            const bool selected = adjustment.preset == preset;
            if (engine::Button(
                        ui, smallConfig, input, assets, id, bounds, smallFont,
                        selected ? TextFormat("%s *", label) : label,
                        engine::UITextJustify::Center,
                        adjustmentMouseInteractive)) {
                adjustment.preset = preset;
            }
        };
        presetButton("sector_editor_object_nudge_fine", "Fine",
                PreviewObjectNudgePreset::Fine, 0);
        presetButton("sector_editor_object_nudge_normal", "Normal",
                PreviewObjectNudgePreset::Normal, 1);
        presetButton("sector_editor_object_nudge_coarse", "Coarse",
                PreviewObjectNudgePreset::Coarse, 2);
        adjustmentY += rowH + 7.0f;
        engine::Text(
                smallConfig,
                assets,
                Rectangle{textX, adjustmentY, adjustmentWidth, rowH},
                smallFont,
                TextFormat("Step %.2f m / %.2f deg",
                        SectorEditorPreviewObjectTranslationStepWorld(
                                adjustment.preset),
                        SectorEditorPreviewObjectYawStepDegrees(
                                adjustment.preset)),
                engine::UITextJustify::Left,
                smallConfig.mutedTextColor);
        adjustmentY += rowH + 2.0f;
        engine::Text(
                smallConfig,
                assets,
                Rectangle{textX, adjustmentY, adjustmentWidth, rowH * 2.0f},
                smallFont,
                "Arrows: world X/Z   PgUp/PgDn: height   Q/E: yaw\nEnter: apply   Esc: cancel   F11: unlock cursor",
                engine::UITextJustify::Left,
                smallConfig.mutedTextColor,
                true);
        adjustmentY += rowH * 2.0f + 7.0f;
        const float actionWidth = (adjustmentWidth - presetGap) * 0.5f;
        if (engine::Button(
                    ui, smallConfig, input, assets,
                    "sector_editor_object_nudge_apply",
                    Rectangle{textX, adjustmentY, actionWidth, rowH},
                    smallFont, "Apply", engine::UITextJustify::Center,
                    adjustmentMouseInteractive)) {
            result.requestApplyAdjustment = true;
        }
        if (engine::Button(
                    ui, smallConfig, input, assets,
                    "sector_editor_object_nudge_cancel",
                    Rectangle{textX + actionWidth + presetGap, adjustmentY,
                            actionWidth, rowH},
                    smallFont, "Cancel", engine::UITextJustify::Center,
                    adjustmentMouseInteractive)) {
            result.requestCancelAdjustment = true;
        }
    } else if (context.structuralPrimitiveEditingState.previewAdjustment.active) {
        PreviewStructuralPrimitiveAdjustmentState& adjustment =
                context.structuralPrimitiveEditingState.previewAdjustment;
        const Rectangle adjustmentPanel = BuildSectorEditorPreviewAdjustmentPanelRect();
        DrawRectangleRec(adjustmentPanel, Color{12, 15, 20, 225});
        DrawRectangleLinesEx(adjustmentPanel, config.borderThickness,
                Color{84, 204, 255, 255});
        const float textX = adjustmentPanel.x + 14.0f;
        float adjustmentY = adjustmentPanel.y + 12.0f;
        const float adjustmentWidth = adjustmentPanel.width - 28.0f;
        const SectorAuthoringStructuralPrimitive* primitive =
                FindSectorAuthoringStructuralPrimitive(
                        adjustment.stagedGraph, adjustment.primitiveId);
        engine::Text(smallConfig, assets,
                Rectangle{textX, adjustmentY, adjustmentWidth, rowH}, smallFont,
                primitive != nullptr
                        ? TextFormat("Adjust %s structure %d",
                                SectorStructuralPrimitiveKindName(primitive->kind),
                                primitive->id)
                        : "Adjustment target missing",
                engine::UITextJustify::Left, Color{84, 204, 255, 255});
        adjustmentY += rowH + 4.0f;
        if (primitive != nullptr) {
            engine::Text(smallConfig, assets,
                    Rectangle{textX, adjustmentY, adjustmentWidth, rowH}, smallFont,
                    TextFormat("X %.2f   Z %.2f   Yaw %.1f deg",
                            SectorCoordToVisibleAuthoring(primitive->x),
                            SectorCoordToVisibleAuthoring(primitive->z),
                            primitive->yawDegrees),
                    engine::UITextJustify::Left);
            adjustmentY += rowH + 2.0f;
            if (primitive->kind != SectorStructuralPrimitiveKind::Ladder) {
                engine::Text(smallConfig, assets,
                        Rectangle{textX, adjustmentY, adjustmentWidth, rowH}, smallFont,
                        TextFormat("Pitch %.1f deg   Roll %.1f deg",
                                primitive->pitchDegrees,
                                primitive->rollDegrees),
                        engine::UITextJustify::Left);
                adjustmentY += rowH + 8.0f;
            } else {
                adjustmentY += 8.0f;
            }
        }
        constexpr float presetGap = 6.0f;
        const float presetWidth = (adjustmentWidth - presetGap * 2.0f) / 3.0f;
        const char* presetLabels[] = {"Fine", "Normal", "Coarse"};
        for (int preset = 0; preset < 3; ++preset) {
            if (engine::Button(ui, smallConfig, input, assets,
                        TextFormat("structure_adjust_preset_%d", preset),
                        Rectangle{textX + preset * (presetWidth + presetGap),
                                adjustmentY, presetWidth, rowH}, smallFont,
                        adjustment.preset == preset
                                ? TextFormat("%s *", presetLabels[preset])
                                : presetLabels[preset],
                        engine::UITextJustify::Center,
                        adjustmentMouseInteractive)) {
                adjustment.preset = preset;
            }
        }
        adjustmentY += rowH + 7.0f;
        const PreviewObjectNudgePreset preset =
                static_cast<PreviewObjectNudgePreset>(adjustment.preset);
        engine::Text(smallConfig, assets,
                Rectangle{textX, adjustmentY, adjustmentWidth, rowH}, smallFont,
                TextFormat("Step %.2f m / %.2f deg",
                        SectorEditorPreviewObjectTranslationStepWorld(preset),
                        SectorEditorPreviewObjectYawStepDegrees(preset)),
                engine::UITextJustify::Left, smallConfig.mutedTextColor);
        adjustmentY += rowH + 2.0f;
        const bool ladder = primitive != nullptr
                && primitive->kind == SectorStructuralPrimitiveKind::Ladder;
        engine::Text(smallConfig, assets,
                Rectangle{textX, adjustmentY, adjustmentWidth, rowH * 3.0f}, smallFont,
                ladder
                        ? "Arrows: world X/Z   PgUp/PgDn: height   Q/E: yaw\nEnter: apply   Esc: cancel   F11: unlock cursor"
                        : "Arrows: world X/Z   PgUp/PgDn: height   Q/E: yaw\nIns/Del: pitch   Home/End: roll\nEnter: apply   Esc: cancel   F11: unlock cursor",
                engine::UITextJustify::Left, smallConfig.mutedTextColor, true);
        adjustmentY += rowH * 3.0f + 7.0f;
        const float actionWidth = (adjustmentWidth - presetGap) * 0.5f;
        if (engine::Button(ui, smallConfig, input, assets,
                    "structure_adjust_apply",
                    Rectangle{textX, adjustmentY, actionWidth, rowH}, smallFont,
                    "Apply", engine::UITextJustify::Center,
                    adjustmentMouseInteractive)) {
            result.requestApplyAdjustment = true;
        }
        if (engine::Button(ui, smallConfig, input, assets,
                    "structure_adjust_cancel",
                    Rectangle{textX + actionWidth + presetGap, adjustmentY,
                            actionWidth, rowH}, smallFont,
                    "Cancel", engine::UITextJustify::Center,
                    adjustmentMouseInteractive)) {
            result.requestCancelAdjustment = true;
        }
    } else if (context.surfaceHeightAdjustmentState.active) {
        PreviewSurfaceHeightAdjustmentState& heightAdjustment =
                context.surfaceHeightAdjustmentState;
        const Rectangle adjustmentPanel =
                BuildSectorEditorPreviewAdjustmentPanelRect();
        DrawRectangleRec(adjustmentPanel, Color{12, 15, 20, 225});
        DrawRectangleLinesEx(
                adjustmentPanel,
                config.borderThickness,
                Color{84, 204, 255, 255});
        const float textX = adjustmentPanel.x + 14.0f;
        float adjustmentY = adjustmentPanel.y + 12.0f;
        const float adjustmentWidth = adjustmentPanel.width - 28.0f;
        const char* targetName =
                heightAdjustment.target == PreviewSurfaceHeightTarget::Floor
                ? "Floor"
                : "Ceiling";
        const SectorAuthoringFaceAnchor* anchor = FindSectorAuthoringFaceAnchor(
                heightAdjustment.stagedGraph,
                heightAdjustment.faceAnchorId);
        const float currentHeight = anchor == nullptr
                ? 0.0f
                : (heightAdjustment.target == PreviewSurfaceHeightTarget::Floor
                        ? anchor->floorZ
                        : anchor->ceilingZ);
        engine::Text(
                smallConfig,
                assets,
                Rectangle{textX, adjustmentY, adjustmentWidth, rowH},
                smallFont,
                TextFormat("Adjust %s | sector %d",
                        targetName,
                        heightAdjustment.topologySectorId),
                engine::UITextJustify::Left,
                Color{84, 204, 255, 255});
        adjustmentY += rowH + 4.0f;
        engine::Text(
                smallConfig,
                assets,
                Rectangle{textX, adjustmentY, adjustmentWidth, rowH},
                smallFont,
                anchor != nullptr
                        ? TextFormat("Height %.2f authored units", currentHeight)
                        : "Adjustment target missing",
                engine::UITextJustify::Left);
        adjustmentY += rowH + 8.0f;

        constexpr float presetGap = 6.0f;
        const float presetWidth =
                (adjustmentWidth - presetGap * 2.0f) / 3.0f;
        const auto presetButton = [&](const char* id, const char* label,
                                      PreviewSurfaceHeightNudgePreset preset,
                                      int index) {
            const Rectangle bounds{
                    textX + static_cast<float>(index)
                                    * (presetWidth + presetGap),
                    adjustmentY,
                    presetWidth,
                    rowH};
            const bool selected = heightAdjustment.preset == preset;
            if (engine::Button(
                        ui, smallConfig, input, assets, id, bounds, smallFont,
                        selected ? TextFormat("%s *", label) : label,
                        engine::UITextJustify::Center,
                        adjustmentMouseInteractive)) {
                heightAdjustment.preset = preset;
            }
        };
        presetButton("sector_editor_height_nudge_fine", "Fine",
                PreviewSurfaceHeightNudgePreset::Fine, 0);
        presetButton("sector_editor_height_nudge_normal", "Normal",
                PreviewSurfaceHeightNudgePreset::Normal, 1);
        presetButton("sector_editor_height_nudge_coarse", "Coarse",
                PreviewSurfaceHeightNudgePreset::Coarse, 2);
        adjustmentY += rowH + 7.0f;
        engine::Text(
                smallConfig,
                assets,
                Rectangle{textX, adjustmentY, adjustmentWidth, rowH},
                smallFont,
                TextFormat("Step %.2f authored units",
                        SectorEditorPreviewSurfaceHeightStepAuthored(
                                heightAdjustment.preset)),
                engine::UITextJustify::Left,
                smallConfig.mutedTextColor);
        adjustmentY += rowH + 2.0f;
        engine::Text(
                smallConfig,
                assets,
                Rectangle{textX, adjustmentY, adjustmentWidth, rowH * 2.0f},
                smallFont,
                "PgUp/PgDn: height\nEnter: apply   Esc: cancel   F11: unlock cursor",
                engine::UITextJustify::Left,
                smallConfig.mutedTextColor,
                true);
        adjustmentY += rowH * 2.0f + 7.0f;
        const float actionWidth = (adjustmentWidth - presetGap) * 0.5f;
        if (engine::Button(
                    ui, smallConfig, input, assets,
                    "sector_editor_height_nudge_apply",
                    Rectangle{textX, adjustmentY, actionWidth, rowH},
                    smallFont, "Apply", engine::UITextJustify::Center,
                    adjustmentMouseInteractive)) {
            result.requestApplyAdjustment = true;
        }
        if (engine::Button(
                    ui, smallConfig, input, assets,
                    "sector_editor_height_nudge_cancel",
                    Rectangle{textX + actionWidth + presetGap, adjustmentY,
                            actionWidth, rowH},
                    smallFont, "Cancel", engine::UITextJustify::Center,
                    adjustmentMouseInteractive)) {
            result.requestCancelAdjustment = true;
        }
    }

    (void)context.font;
    (void)context.statusText;
    return result;
}

} // namespace game
