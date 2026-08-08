#include "sector_editor/preview/SectorEditorPreviewOverlay.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorPreviewActions.h"
#include "sector_editor/SectorEditorUiHelpers.h"
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

const SectorTopologyDynamicSpotLight* SelectedTopologyDynamicSpotLight(
        const SectorTopologyMap& topologyMap,
        const SelectionState& selectionState)
{
    return selectionState.topologySelectionKind == TopologySelectionKind::DynamicSpotLight
            ? FindSectorTopologyDynamicSpotLight(topologyMap, selectionState.selectedTopologyDynamicSpotLightId)
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
    constexpr float expandedHeight = 390.0f;
    return Rectangle{
            x,
            y,
            width,
            activeTab == PreviewDebugOverlayTab::None ? collapsedHeight : expandedHeight};
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
                DrawLine3D(a, b, color);
                DrawLine3D(b, c, color);
                DrawLine3D(c, a, color);
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
    DrawSphereWires(origin, OriginMarkerRadius, 8, 12, originColor);
    DrawSphereWires(target, TargetMarkerRadius, 8, 12, targetColor);
    DrawLine3D(origin, target, directionColor);
    DrawLine3D(origin, rangeEnd, rangeColor);
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
        DrawSphereWires(probe.position, MarkerRadius * 1.65f, 8, 8, Color{255, 255, 255, 155});
    }
    EndMode3D();
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

    const bool mouseInteractive = IsPreviewOverlayMouseInteractive(controllerState);
    const bool drawExpanded = overlayState.activePreviewDebugOverlayTab != PreviewDebugOverlayTab::None;
    const float panelW = 700.0f;
    const float padding = 10.0f;
    const float gap = 6.0f;
    const float stripH = 26.0f;
    const float tabH = 30.0f;
    const float rowH = 24.0f;
    const Rectangle basePanel{32.0f, 32.0f, panelW, 0.0f};
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
                const SectorFpsControllerConfig effectiveConfig =
                        EffectiveSectorFpsControllerConfig(
                                controllerState.fpsControllerState,
                                controllerState.fpsControllerConfig);
                collisionStatus = TextFormat(
                        "mode: gameplay collision | sector: %d | vertical: %s / %s | stance: %s %.2fm | block: %s | radius: %.2f | step: %.2f | jump: %.2f",
                        controllerState.fpsControllerState.currentSectorId,
                        verticalState,
                        VerticalTransitionName(collisionState.previewVerticalResult.transition),
                        stance,
                        effectiveConfig.playerHeight,
                        blockText.c_str(),
                        controllerState.fpsControllerConfig.playerRadius,
                        controllerState.fpsControllerConfig.stepHeight,
                        controllerState.fpsControllerConfig.jumpHeight);
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
                addKeyValue("door mode", SectorDoorLightingDebugModeName(preview.DoorLightingDebugMode()));
                addKeyValue("selected dynamic", TextFormat(
                        "%zu / %zu / %zu",
                        preview.SelectedDynamicLights().size(),
                        preview.DynamicLightCandidateCount(),
                        preview.DynamicLightSourceCount()));
                if (!preview.SelectedDynamicLightIds().empty()) {
                    std::ostringstream ids;
                    for (size_t i = 0; i < preview.SelectedDynamicLightIds().size(); ++i) {
                        if (i > 0) {
                            ids << ",";
                        }
                        ids << preview.SelectedDynamicLightIds()[i];
                    }
                    addKeyValue("selected ids", ids.str());
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
                addKeyValueStyled("status", objectProbeStatus, smallConfig.mutedTextColor, true);
                const size_t totalProbeCount = context.runtimeObjects.objectLightProbes.probes.size();
                addKeyValue("probe count", TextFormat("%zu", totalProbeCount));
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
                        "pistol model",
                        attachment.resolvedModelPath.empty()
                                ? "none"
                                : attachment.resolvedModelPath,
                        smallConfig.mutedTextColor,
                        true);
                addKeyValue("pistol load", attachmentLoadState);
                addKeyValue("pistol geometry", TextFormat(
                        "meshes %d | triangles %d | materials %d",
                        attachment.meshCount,
                        attachment.triangleCount,
                        attachment.materialCount));
                addKeyValue("pistol lighting default", TextFormat(
                        "brightness %+.3f | metallic %.3f | roughness %.3f",
                        attachment.lightingDefaults.brightnessAdjustment,
                        attachment.lightingDefaults.materialOverride
                                .metallicFactor,
                        attachment.lightingDefaults.materialOverride
                                .roughnessFactor));
                addKeyValue("pistol lighting effective", TextFormat(
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
                if (attachment.pistolWorldTransformValid) {
                    addKeyValueStyled(
                            "pistol transform",
                            FormatViewmodelTransform(
                                    attachment.pistolWorldTransform),
                            smallConfig.mutedTextColor,
                            true);
                }
                if (!IsFpsViewmodelAttachmentRenderable(vm)) {
                    const char* reason = vm.equipState
                                            == FpsViewmodelEquipState::Holstered
                            ? "viewmodel is holstered"
                            : attachment.loadState
                                            == FpsViewmodelAttachmentLoadState::Pending
                                    ? "pistol resource or attachment bone is pending"
                                    : !attachment.error.empty()
                                            ? attachment.error.c_str()
                                            : !attachment.handPoseValid
                                                    ? "current hand pose is unavailable"
                                                    : "attachment is not ready";
                    addKeyValueStyled(
                            "pistol hidden",
                            reason,
                            Color{236, 92, 92, 245},
                            true);
                }
                if (!attachment.error.empty()) {
                    addKeyValueStyled(
                            "pistol error",
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
                addKeyValue("muzzle effects", TextFormat(
                        "socket %s | flash %s %.3f/%.3f (%.2f) soft %.2f | light %s %.3f",
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
            case PreviewDebugOverlayTab::Controls:
                if (context.lightState.spotLightPilot.active) {
                    addWrappedLine("pilot light: WASD move, mouse look, Space/Ctrl up/down. Unlock cursor with F11 to click Apply or Cancel.");
                } else if (controllerState.previewControlMode == SectorPreviewControlMode::Gameplay) {
                    addWrappedLine("movement: WASD move, Space jump, Shift run, Ctrl toggle crouch, mouse look. F11 unlocks cursor for UI tabs.");
                } else {
                    addWrappedLine("movement: WASD move, mouse look, Space/Ctrl up/down. F11 unlocks cursor for UI tabs.");
                }
                addWrappedLine("hotkeys: left mouse fire, H holster/equip viewmodel, F1 AO, F2 hide/show 3D UI, F3 control mode, F4 dynamic lights, F10 borderless window, Tab/Esc return to 2D.");
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
    if (drawExpanded && overlayState.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Controls) {
        contentH += rowH + 6.0f;
    }
    const Rectangle panel{
            basePanel.x,
            basePanel.y,
            basePanel.width,
            padding + stripH + gap + tabH + contentH + padding};
    DrawRectangleRec(panel, Color{12, 15, 20, 205});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    const bool hasSelectedSpotLight = SelectedTopologyStaticSpotLight(topologyMap, selectionState) != nullptr
            || SelectedTopologyDynamicSpotLight(topologyMap, selectionState) != nullptr;
    engine::Text(
            smallConfig,
            assets,
            Rectangle{
                    panel.x + padding,
                    panel.y + padding,
                    mouseInteractive && (context.lightState.spotLightPilot.active
                            || (hasSelectedSpotLight && controllerState.previewControlMode == SectorPreviewControlMode::FreeFly))
                            ? contentW - 170.0f
                            : contentW,
                    stripH},
            smallFont,
            compactStatus.c_str(),
            engine::UITextJustify::Left,
            context.topologyDocumentDirty ? Color{236, 196, 92, 255} : smallConfig.textColor,
            true);

    float actionsRight = panel.x + panel.width - padding;
    const float actionY = panel.y + padding - 2.0f;
    if (mouseInteractive) {
        if (context.lightState.spotLightPilot.active) {
            if (engine::Button(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        "sector_editor_preview_spotlight_pilot_cancel",
                        Rectangle{actionsRight - 72.0f, actionY, 72.0f, 28.0f},
                        smallFont,
                        "Cancel")) {
                result.requestCancelSpotLightPilot = true;
            }
            actionsRight -= 82.0f;
            if (engine::Button(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        "sector_editor_preview_spotlight_pilot_apply",
                        Rectangle{actionsRight - 66.0f, actionY, 66.0f, 28.0f},
                        smallFont,
                        "Apply")) {
                result.requestApplySpotLightPilot = true;
            }
        } else if (hasSelectedSpotLight && controllerState.previewControlMode == SectorPreviewControlMode::FreeFly) {
            if (engine::Button(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        "sector_editor_preview_spotlight_pilot_start",
                        Rectangle{actionsRight - 92.0f, actionY, 92.0f, 28.0f},
                        smallFont,
                        "Pilot")) {
                result.requestStartSpotLightPilot = true;
            }
        }
    }

    const struct {
        PreviewDebugOverlayTab tab;
        const char* id;
        const char* label;
    } tabs[] = {
            {PreviewDebugOverlayTab::View, "sector_editor_preview_tab_view", "View"},
            {PreviewDebugOverlayTab::Render, "sector_editor_preview_tab_render", "Render"},
            {PreviewDebugOverlayTab::Visibility, "sector_editor_preview_tab_visibility", "Visibility"},
            {PreviewDebugOverlayTab::Lighting, "sector_editor_preview_tab_lighting", "Lighting"},
            {PreviewDebugOverlayTab::Objects, "sector_editor_preview_tab_objects", "Objects"},
            {PreviewDebugOverlayTab::Probes, "sector_editor_preview_tab_probes", "Probes"},
            {PreviewDebugOverlayTab::Viewmodel, "sector_editor_preview_tab_viewmodel", "Arms"},
            {PreviewDebugOverlayTab::Controls, "sector_editor_preview_tab_controls", "Controls"},
    };

    const float tabY = panel.y + padding + stripH + gap;
    const float tabGap = 6.0f;
    const float tabW = (contentW - tabGap * 7.0f) / 8.0f;
    for (int i = 0; i < 8; ++i) {
        const Rectangle tabRect{
                panel.x + padding + static_cast<float>(i) * (tabW + tabGap),
                tabY,
                tabW,
                tabH};
        const bool selected = overlayState.activePreviewDebugOverlayTab == tabs[i].tab;
        if (mouseInteractive) {
            if (engine::ToolButton(ui, smallConfig, input, assets, tabs[i].id, tabRect, smallFont, tabs[i].label, selected)) {
                overlayState.activePreviewDebugOverlayTab = selected
                        ? PreviewDebugOverlayTab::None
                        : tabs[i].tab;
            }
        } else {
            DrawRectangleRec(tabRect, selected ? Color{48, 68, 86, 210} : Color{24, 30, 38, 185});
            DrawRectangleLinesEx(tabRect, config.borderThickness, config.borderColor);
            engine::Text(smallConfig, assets, tabRect, smallFont, tabs[i].label, engine::UITextJustify::Center, smallConfig.mutedTextColor);
        }
    }

    float y = tabY + tabH + gap;
    if (drawExpanded && overlayState.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Lighting) {
        const char* doorModeOptions[] = {
                "Normal",
                "AlbedoOnly",
                "BakedOnly",
                "DynamicOnly",
                "NormalVisualize",
                "FlatColorNoTexture"};
        const float labelW = 86.0f;
        const Rectangle labelRect{panel.x + padding, y, labelW, rowH};
        const Rectangle modeRect{panel.x + padding + labelW + gap, y, 220.0f, rowH};
        int selectedMode = static_cast<int>(preview.DoorLightingDebugMode());
        engine::Text(
                smallConfig,
                assets,
                labelRect,
                smallFont,
                "Door Debug",
                engine::UITextJustify::Left,
                smallConfig.textColor);
        if (mouseInteractive) {
            if (engine::Option(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        "sector_editor_preview_door_lighting_debug_mode",
                        modeRect,
                        smallFont,
                        doorModeOptions,
                        sizeof(doorModeOptions) / sizeof(doorModeOptions[0]),
                        selectedMode)) {
                preview.SetDoorLightingDebugMode(static_cast<SectorDoorLightingDebugMode>(selectedMode));
            }
        } else {
            DrawRectangleRec(modeRect, Color{24, 30, 38, 155});
            DrawRectangleLinesEx(modeRect, config.borderThickness, config.borderColor);
            engine::Text(
                    smallConfig,
                    assets,
                    modeRect,
                    smallFont,
                    SectorDoorLightingDebugModeName(preview.DoorLightingDebugMode()),
                    engine::UITextJustify::Center,
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

    (void)context.font;
    (void)context.statusText;
    return result;
}

} // namespace game
