#include "sector_editor/preview/SectorEditorPreviewOverlay.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorPreviewActions.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
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

bool IsPreviewOverlayMouseInteractive(const SectorEditorState& state)
{
    return !state.freeflyController.mouseLookEnabled;
}

SectorEditorSelectionUiDependencies BuildSelectionUiDependencies(
        SectorEditorUiState& uiState,
        InspectorIdUiState& inspectorIdUiState)
{
    return SectorEditorSelectionUiDependencies{
            uiState.floorInput,
            uiState.ceilingInput,
            uiState.ambientIntensityInput,
            uiState.ambientRedInput,
            uiState.ambientGreenInput,
            uiState.ambientBlueInput,
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
            uiState.lightRedInput,
            uiState.lightGreenInput,
            uiState.lightBlueInput,
            uiState.runtimeObjectXInput,
            uiState.runtimeObjectYInput,
            uiState.runtimeObjectZInput,
            uiState.runtimeObjectYawInput,
            uiState.runtimeObjectWidthInput,
            uiState.runtimeObjectHeightInput,
            uiState.runtimeObjectThicknessInput,
            uiState.runtimeObjectNormalOffsetInput,
            uiState.runtimeObjectOpenDistanceInput,
            uiState.runtimeObjectSpeedInput,
            uiState.runtimeObjectInitialOpenFractionInput,
            uiState.runtimeObjectAutoOpenDistanceInput,
            uiState.runtimeObjectInteractionDistanceInput,
            uiState.runtimeObjectOriginXInput,
            uiState.runtimeObjectOriginYInput,
            uiState.inspectorScroll,
            inspectorIdUiState};
}

bool IsValidPreviewSurfaceRef(
        SectorEditorState& state,
        SelectionState& selectionState,
        ManipulationState& manipulationState,
        SectorEditorUiState& uiState,
        InspectorIdUiState& inspectorIdUiState,
        MaterialEditingUiState& materialUiState,
        SectorSurfaceRef surface)
{
    const bool authoringDerivationCurrent =
            state.authoringDerivationState == SectorEditorAuthoringDerivationState::ValidCurrent
            && !state.authoringDerivedTopologyStale
            && state.authoringDerivation.success;
    SectorEditorSelectionServiceContext context{
            state.topologyMap,
            state.authoringGraph,
            state.authoringDerivation,
            authoringDerivationCurrent,
            selectionState,
            state.selectedSurface3D,
            state.selectedTopologySurface3D,
            manipulationState,
            state.runtimeObjectDrag,
            BuildSelectionUiDependencies(uiState, inspectorIdUiState),
            materialUiState,
            nullptr,
            nullptr,
            nullptr};
    return IsValidSectorEditorSurfaceRef(context, surface);
}

const SectorTopologyStaticSpotLight* SelectedTopologyStaticSpotLight(
        const SectorEditorState& state,
        const SelectionState& selectionState)
{
    return selectionState.topologySelectionKind == TopologySelectionKind::StaticSpotLight
            ? FindSectorTopologyStaticSpotLight(state.topologyMap, selectionState.selectedTopologyStaticSpotLightId)
            : nullptr;
}

const SectorTopologyDynamicSpotLight* SelectedTopologyDynamicSpotLight(
        const SectorEditorState& state,
        const SelectionState& selectionState)
{
    return selectionState.topologySelectionKind == TopologySelectionKind::DynamicSpotLight
            ? FindSectorTopologyDynamicSpotLight(state.topologyMap, selectionState.selectedTopologyDynamicSpotLightId)
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
    constexpr float width = 620.0f;
    constexpr float collapsedHeight = 78.0f;
    constexpr float expandedHeight = 390.0f;
    return Rectangle{
            x,
            y,
            width,
            activeTab == PreviewDebugOverlayTab::None ? collapsedHeight : expandedHeight};
}

void DrawSectorEditorPreviewSurfaceHighlights(
        SectorEditorState& state,
        SelectionState& selectionState,
        ManipulationState& manipulationState,
        SectorEditorUiState& uiState,
        InspectorIdUiState& inspectorIdUiState,
        MaterialEditingUiState& materialUiState,
        const SectorMeshRenderer& preview)
{
    if (!preview.IsRendererReady() || state.freeflyController.mouseLookEnabled) {
        return;
    }

    auto drawSurface = [&state, &selectionState, &manipulationState, &uiState, &inspectorIdUiState, &materialUiState, &preview](
                               SectorSurfaceRef surface,
                               Color color,
                               float thickness) {
        if (!IsValidPreviewSurfaceRef(
                    state,
                    selectionState,
                    manipulationState,
                    uiState,
                    inspectorIdUiState,
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
    if (state.hoveredSurface3D.hit
            && !SameSectorEditorSurfaceRef(state.hoveredSurface3D.surface, state.selectedSurface3D)) {
        drawSurface(state.hoveredSurface3D.surface, Color{248, 238, 124, 235}, 2.0f);
    }
    if (state.selectedSurface3D.kind != SectorSurfaceKind::None) {
        drawSurface(state.selectedSurface3D, Color{84, 204, 255, 255}, 3.0f);
    }
    EndMode3D();
}

void DrawSectorEditorPreviewSpotLightOverlay(
        const SectorEditorState& state,
        const SelectionState& selectionState,
        const SectorMeshRenderer& preview)
{
    if (!preview.IsRendererReady() || state.freeflyController.mouseLookEnabled) {
        return;
    }

    Vector3 lightPosition = {};
    Vector3 lightTarget = {};
    float lightRange = 0.0f;
    float innerConeDegrees = 0.0f;
    float outerConeDegrees = 0.0f;
    bool selectedStaticSpotLight = false;
    if (const SectorTopologyStaticSpotLight* light = SelectedTopologyStaticSpotLight(state, selectionState)) {
        lightPosition = light->position;
        lightTarget = light->target;
        lightRange = light->range;
        innerConeDegrees = light->innerConeDegrees;
        outerConeDegrees = light->outerConeDegrees;
        selectedStaticSpotLight = true;
    } else if (const SectorTopologyDynamicSpotLight* light = SelectedTopologyDynamicSpotLight(state, selectionState)) {
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
        const SectorEditorState& state,
        const SectorMeshRenderer& preview)
{
    if (!preview.IsRendererReady()
            || !state.showObjectProbeDebugOverlay
            || state.runtimeObjects.objectLightProbes.probes.empty()) {
        return;
    }

    constexpr float MarkerRadius = 0.08f;
    const Vector3 referencePosition = preview.RendererPose().position;
    const float maxDistanceWorld = NormalizeSectorPreviewSettings(
            state.topologyMap.previewSettings).objectProbeDebugDrawMaxDistanceWorld;
    BeginMode3D(preview.RenderCamera());
    for (const SectorBakedObjectLightProbe& probe : state.runtimeObjects.objectLightProbes.probes) {
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
    SectorEditorState& state = context.state;
    SelectionState& selectionState = context.selectionState;
    SectorEditorUiState& uiState = context.uiState;
    MaterialEditingUiState& materialUiState = context.materialUiState;
    SectorMeshRenderer& preview = context.preview;
    SectorEditorPreviewOverlayResult result;

    const bool mouseInteractive = IsPreviewOverlayMouseInteractive(state);
    const bool drawExpanded = state.activePreviewDebugOverlayTab != PreviewDebugOverlayTab::None;
    const float panelW = 620.0f;
    const float padding = 10.0f;
    const float gap = 6.0f;
    const float stripH = 26.0f;
    const float tabH = 30.0f;
    const float rowH = 24.0f;
    const Rectangle basePanel{32.0f, 32.0f, panelW, 0.0f};
    const float contentW = panelW - padding * 2.0f;
    engine::UIConfig smallConfig = SectorEditorSmallFontConfig(config, assets, smallFont);

    const SectorViewPose pose = ActiveSectorEditorPreviewPose(state, preview);
    const Vector3 position = pose.position;
    const RuntimePortalVisibilityResult& visibility = preview.VisibilityResult();
    const int compactSectorId =
            state.previewControlMode == SectorPreviewControlMode::Gameplay
                    ? state.fpsControllerState.currentSectorId
                    : (visibility.validStartSector ? visibility.startSectorId : 0);
    std::string compactSector = compactSectorId > 0
            ? TextFormat("sector %d", compactSectorId)
            : "sector none";
    const char* dirtyText = state.topologyDocumentDirty ? "unsaved" : "saved";
    const std::string compactStatus = TextFormat(
            "3D %s | %s | assets %.0f%% | lightmap %s | %s",
            PreviewControlModeName(state.previewControlMode),
            compactSector.c_str(),
            preview.RendererAssetProgress(assets) * 100.0f,
            preview.RendererLightmapStatusText(),
            dirtyText);

    std::string collisionStatus;
    if (state.previewControlMode == SectorPreviewControlMode::Gameplay) {
        if (state.sectorCollisionWorldValid) {
            if (state.previewCollisionNoclipFallback) {
                collisionStatus = "mode: gameplay collision | status: no sector / noclip";
            } else if (state.fpsControllerState.currentSectorId == 0
                    || !state.previewVerticalResult.hasSector) {
                collisionStatus = "mode: gameplay collision | status: no sector";
            } else {
                std::string blockText;
                if (state.previewMoveResult.hitWall) {
                    blockText += "wall ";
                }
                if (state.previewMoveResult.blockedByStep) {
                    blockText += "step ";
                }
                if (state.previewMoveResult.blockedByCeiling) {
                    blockText += "ceiling ";
                }
                if (blockText.empty()) {
                    blockText = "clear";
                }
                const char* verticalState = state.previewVerticalResult.cannotFit
                        ? "cannot fit"
                        : (state.fpsControllerState.grounded
                                ? "grounded"
                                : (state.fpsControllerState.verticalVelocity > 0.0f ? "jumping" : "falling"));
                collisionStatus = TextFormat(
                        "mode: gameplay collision | sector: %d | vertical: %s / %s | block: %s | radius: %.2f | step: %.2f | jump: %.2f",
                        state.fpsControllerState.currentSectorId,
                        verticalState,
                        VerticalTransitionName(state.previewVerticalResult.transition),
                        blockText.c_str(),
                        state.fpsControllerConfig.playerRadius,
                        state.fpsControllerConfig.stepHeight,
                        state.fpsControllerConfig.jumpHeight);
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
        switch (state.activePreviewDebugOverlayTab) {
            case PreviewDebugOverlayTab::View:
                addKeyValue("mode", PreviewControlModeName(state.previewControlMode));
                addKeyValue("position", TextFormat("%.2f, %.2f, %.2f", position.x, position.y, position.z));
                addKeyValue("sector", compactSector);
                addWrappedLine(collisionStatus);
                if (!state.sectorCollisionWorldWarning.empty()) {
                    addKeyValueStyled("warning", state.sectorCollisionWorldWarning, Color{236, 92, 92, 245}, true);
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
                addKeyValue("AO", state.useBakedAmbientOcclusion ? "on" : "off");
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
                        state.runtimeObjects.doorObjectCount,
                        state.runtimeObjects.validDoorAnchorCount));
                break;
            case PreviewDebugOverlayTab::Objects: {
                const SectorRuntimeObjectState& objects = state.runtimeObjects;
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
                const char* objectProbeStatus = state.runtimeObjects.objectProbeStatus.empty()
                        ? "none"
                        : state.runtimeObjects.objectProbeStatus.c_str();
                addKeyValueStyled("status", objectProbeStatus, smallConfig.mutedTextColor, true);
                const size_t totalProbeCount = state.runtimeObjects.objectLightProbes.probes.size();
                addKeyValue("probe count", TextFormat("%zu", totalProbeCount));
                if (state.showObjectProbeDebugOverlay) {
                    const float maxDistanceWorld = NormalizeSectorPreviewSettings(
                            state.topologyMap.previewSettings).objectProbeDebugDrawMaxDistanceWorld;
                    const size_t visibleProbeCount = CountVisibleObjectProbeDebugMarkers(
                            state.runtimeObjects.objectLightProbes,
                            preview.RendererPose().position,
                            maxDistanceWorld);
                    addKeyValue("drawn", TextFormat("%zu / %zu", visibleProbeCount, totalProbeCount));
                }
                if (!state.runtimeObjects.objectSectorLookupWarning.empty()) {
                    addKeyValueStyled("lookup warning", state.runtimeObjects.objectSectorLookupWarning, Color{236, 92, 92, 245}, true);
                }
                break;
            }
            case PreviewDebugOverlayTab::Controls:
                if (context.lightState.spotLightPilot.active) {
                    addWrappedLine("pilot light: WASD move, mouse look, Space/Ctrl up/down. Unlock cursor with F11 to click Apply or Cancel.");
                } else if (state.previewControlMode == SectorPreviewControlMode::Gameplay) {
                    addWrappedLine("movement: WASD move, Space jump, Shift run, mouse look. F11 unlocks cursor for UI tabs.");
                } else {
                    addWrappedLine("movement: WASD move, mouse look, Space/Ctrl up/down. F11 unlocks cursor for UI tabs.");
                }
                addWrappedLine("hotkeys: F1 AO, F2 hide/show 3D UI, F3 control mode, F4 dynamic lights, Tab/Esc return to 2D.");
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
    if (drawExpanded && state.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Probes) {
        contentH += (rowH + 6.0f) * 2.0f;
    }
    if (drawExpanded && state.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Lighting) {
        contentH += rowH + 6.0f;
    }
    if (drawExpanded && state.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Controls) {
        contentH += rowH + 6.0f;
    }
    const Rectangle panel{
            basePanel.x,
            basePanel.y,
            basePanel.width,
            padding + stripH + gap + tabH + contentH + padding};
    DrawRectangleRec(panel, Color{12, 15, 20, 205});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    const bool hasSelectedSpotLight = SelectedTopologyStaticSpotLight(state, selectionState) != nullptr
            || SelectedTopologyDynamicSpotLight(state, selectionState) != nullptr;
    engine::Text(
            smallConfig,
            assets,
            Rectangle{
                    panel.x + padding,
                    panel.y + padding,
                    mouseInteractive && (context.lightState.spotLightPilot.active
                            || (hasSelectedSpotLight && state.previewControlMode == SectorPreviewControlMode::FreeFly))
                            ? contentW - 170.0f
                            : contentW,
                    stripH},
            smallFont,
            compactStatus.c_str(),
            engine::UITextJustify::Left,
            state.topologyDocumentDirty ? Color{236, 196, 92, 255} : smallConfig.textColor,
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
        } else if (hasSelectedSpotLight && state.previewControlMode == SectorPreviewControlMode::FreeFly) {
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
            {PreviewDebugOverlayTab::Controls, "sector_editor_preview_tab_controls", "Controls"},
    };

    const float tabY = panel.y + padding + stripH + gap;
    const float tabGap = 6.0f;
    const float tabW = (contentW - tabGap * 6.0f) / 7.0f;
    for (int i = 0; i < 7; ++i) {
        const Rectangle tabRect{
                panel.x + padding + static_cast<float>(i) * (tabW + tabGap),
                tabY,
                tabW,
                tabH};
        const bool selected = state.activePreviewDebugOverlayTab == tabs[i].tab;
        if (mouseInteractive) {
            if (engine::ToolButton(ui, smallConfig, input, assets, tabs[i].id, tabRect, smallFont, tabs[i].label, selected)) {
                state.activePreviewDebugOverlayTab = selected
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
    if (drawExpanded && state.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Lighting) {
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
    if (drawExpanded && state.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Probes) {
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
                    state.showObjectProbeDebugOverlay);
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
            if (state.showObjectProbeDebugOverlay) {
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
                NormalizeSectorPreviewSettings(state.topologyMap.previewSettings);
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
                    uiState.objectProbeDebugDrawMaxDistanceInput,
                    0.0f,
                    512.0f,
                    1);
            if (distanceResult.changed) {
                SectorPreviewSettings editedPreviewSettings = state.topologyMap.previewSettings;
                editedPreviewSettings.objectProbeDebugDrawMaxDistanceWorld =
                        distanceResult.finite
                        ? distanceResult.value
                        : normalizedPreviewSettings.objectProbeDebugDrawMaxDistanceWorld;
                state.topologyMap.previewSettings =
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
    if (drawExpanded && state.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::Controls) {
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
