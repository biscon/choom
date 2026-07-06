#include "sector_editor/SectorEditor.h"

#include "engine/assets/TextureLoadFlags.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentActions.h"
#include "sector_editor/document/SectorEditorDocumentModals.h"
#include "sector_editor/selection/SectorEditorManipulationService.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorLightInspector.h"
#include "sector_editor/SectorEditorLightmapModal.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorMaterialModals.h"
#include "sector_editor/SectorEditorPreviewActions.h"
#include "sector_editor/SectorEditorPreviewSettingsModal.h"
#include "sector_editor/preview/SectorEditorPreviewUvPanel.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.h"
#include "sector_editor/services/texture_picker/SectorEditorTexturePickerService.h"
#include "sector_editor/tools/billboards/SectorEditorBillboardActions.h"
#include "sector_editor/tools/doors/SectorEditorDoorActions.h"
#include "sector_editor/tools/doors/SectorEditorDoorModals.h"
#include "sector_editor/tools/materials/SectorEditorMaterialInspector.h"
#include "sector_editor/tools/SectorEditorToolModule.h"
#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectActions.h"
#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectDrag.h"
#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectMoveProvider.h"
#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"
#include "sector_editor/SectorEditorSectorInspector.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/SectorEditorTopologyActions.h"
#include "sector_editor/SectorEditorTopologyRenderCache.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/SectorEditorVertexInspector.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorFreeflyController.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyGeometry.h"
#include "sector_demo/SectorTopologySerialization.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace game {

namespace {

constexpr float SectorEditorPanelScrollPaddingPx = 8.0f;

float ScrollAreaContentWidthForVerticalScrollbar(
        float boundsWidth,
        const engine::UIConfig& config,
        float paddingPx,
        bool drawFrame)
{
    const float clientWidth = std::max(
            0.0f,
            boundsWidth - (drawFrame ? config.borderThickness * 2.0f : 0.0f));
    return std::max(0.0f, clientWidth - config.scrollbarSize - paddingPx * 2.0f);
}

bool IsFlatTopologySurfaceTarget(TopologySurfaceEditTarget target)
{
    return target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling;
}

int64_t PositiveModulo(int64_t value, int64_t divisor)
{
    if (divisor <= 0) {
        return 0;
    }
    const int64_t result = value % divisor;
    return result < 0 ? result + divisor : result;
}

bool CoordAlignedToStep(SectorCoord value, int64_t step)
{
    return step <= 1 || PositiveModulo(value, step) == 0;
}

Vector3 PreviewForwardFromPose(const SectorViewPose& pose)
{
    const float cosPitch = std::cos(pose.pitchRadians);
    return Vector3Normalize(Vector3{
            std::cos(pose.yawRadians) * cosPitch,
            std::sin(pose.pitchRadians),
            std::sin(pose.yawRadians) * cosPitch});
}

SectorViewPose PreviewPoseLookingAt(Vector3 position, Vector3 target)
{
    Vector3 direction = Vector3Subtract(target, position);
    if (Vector3LengthSqr(direction) <= 0.000001f) {
        direction = Vector3{1.0f, 0.0f, 0.0f};
    } else {
        direction = Vector3Normalize(direction);
    }

    return SectorViewPose{
            position,
            std::atan2(direction.z, direction.x),
            std::asin(Clamp(direction.y, -1.0f, 1.0f))};
}

int64_t LcmClamped(int64_t a, int64_t b)
{
    if (a <= 0 || b <= 0) {
        return 1;
    }
    const int64_t divisor = std::gcd(a, b);
    if (divisor <= 0) {
        return 1;
    }
    constexpr int64_t maxReasonablePeriod = 1 << 20;
    const int64_t divided = a / divisor;
    if (divided > maxReasonablePeriod / b) {
        return maxReasonablePeriod;
    }
    return std::max<int64_t>(1, divided * b);
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

void UpdateCachedRuntimeObjectDraw(
        SectorEditorTopologyRenderCache& cache,
        const SectorPlacedRuntimeObject& object)
{
    for (CachedRuntimeObjectDraw& cached : cache.runtimeObjects) {
        if (cached.objectId != object.id) {
            continue;
        }

        cached.definitionId = !object.kind.empty() ? object.kind : object.definitionId;
        cached.map = Vector2{object.position.x, object.position.z};
        cached.yawRadians = object.yawRadians;
        cached.definitionKnown = object.kind == "billboard";
        cached.isDoor = false;
        cached.doorFootprintValid = false;
        return;
    }
}

int64_t CoordinateSequencePeriod(SectorCoord stepDelta, int64_t snapStep)
{
    if (snapStep <= 1) {
        return 1;
    }
    const int64_t divisor = std::gcd<int64_t>(
            std::llabs(static_cast<int64_t>(stepDelta)),
            snapStep);
    return std::max<int64_t>(1, snapStep / std::max<int64_t>(1, divisor));
}

float AuthoringInspectorTextureRowTotalHeight(float gap)
{
    return SectorEditorInspectorTextureRowHeight() + gap;
}

float AuthoringInspectorAssignedDecalControlsHeight(bool emissive, float rowH, float gap, bool includeTintAndFit)
{
    float height = 0.0f;
    height += rowH + gap;
    height += 36.0f + gap;
    if (emissive) {
        height += rowH + gap;
    }
    if (includeTintAndFit) {
        height += rowH + gap;
        height += 36.0f + gap;
    }
    return height;
}

float AuthoringInspectorDecalBlockHeight(
        const SectorTopologyDecalLayer& decal,
        float rowH,
        float gap,
        bool includeTintAndFit)
{
    float height = AuthoringInspectorTextureRowTotalHeight(gap);
    if (!decal.textureId.empty()) {
        height += AuthoringInspectorAssignedDecalControlsHeight(
                decal.emissive,
                rowH,
                gap,
                includeTintAndFit);
    }
    return height;
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

    Vector3 previous = Vector3Add(
            center,
            Vector3Scale(right, radius));
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

float AuthoringLineInspectorContentHeight(
        const SectorAuthoringLine& line,
        const SectorAuthoringGraph& graph,
        float rowH,
        float gap,
        float endpointSummaryHeight)
{
    float height = 0.0f;
    height += 38.0f;
    height += endpointSummaryHeight;
    height += 36.0f + gap;

    const auto addSideSection = [&](SectorTopologySideKind sideKind) {
        height += 18.0f;
        height += 30.0f;
        height += AuthoringInspectorTextureRowTotalHeight(gap) * 4.0f;

        const SectorAuthoringLineSide* side =
                FindSectorAuthoringLineSide(graph, SectorAuthoringSideId{line.id, sideKind});
        const SectorTopologyDecalLayer emptyDecal;
        const SectorTopologyDecalLayer& wallDecal =
                side != nullptr ? side->wall.decal : emptyDecal;
        const SectorTopologyDecalLayer& lowerDecal =
                side != nullptr ? side->lower.decal : emptyDecal;
        const SectorTopologyDecalLayer& upperDecal =
                side != nullptr ? side->upper.decal : emptyDecal;
        height += AuthoringInspectorDecalBlockHeight(wallDecal, rowH, gap, true);
        height += AuthoringInspectorDecalBlockHeight(lowerDecal, rowH, gap, true);
        height += AuthoringInspectorDecalBlockHeight(upperDecal, rowH, gap, true);
    };

    addSideSection(SectorTopologySideKind::Front);
    addSideSection(SectorTopologySideKind::Back);
    height += rowH + gap;
    height += rowH + gap;
    return height;
}

float AuthoringFaceInspectorContentHeight(
        const SectorAuthoringFaceAnchor& anchor,
        float rowH,
        float gap,
        float anchorSummaryHeight)
{
    float height = 0.0f;
    height += 38.0f;
    height += anchorSummaryHeight;
    height += (rowH + gap) * 2.0f;
    height += rowH + gap;

    height += 18.0f;
    height += 30.0f;
    height += rowH + gap;
    height += (rowH + gap) * 3.0f;

    height += 18.0f;
    height += 30.0f;
    height += AuthoringInspectorTextureRowTotalHeight(gap) * 5.0f;
    height += AuthoringInspectorDecalBlockHeight(anchor.floorDecal, rowH, gap, true);
    height += AuthoringInspectorDecalBlockHeight(anchor.ceilingDecal, rowH, gap, true);
    height += AuthoringInspectorDecalBlockHeight(anchor.defaultWall.decal, rowH, gap, false);
    height += AuthoringInspectorDecalBlockHeight(anchor.defaultLower.decal, rowH, gap, false);
    height += AuthoringInspectorDecalBlockHeight(anchor.defaultUpper.decal, rowH, gap, false);
    height += rowH + gap;
    return height;
}

} // namespace

bool SectorEditor::Init(engine::EngineContext& context)
{
    engineContext = &context;
    Shutdown(context);
    engineContext = &context;
    ResetToBlankMap(context);
    return true;
}

void SectorEditor::Shutdown(engine::EngineContext& context)
{
    engine::AssetManager& assets = context.assets;
    ShutdownLightmapBake();
    if (initialized
            || state.runtimeObjects.worldReserved
            || !engine::IsNull(state.runtimeObjects.runtimeObjectAssetScope)) {
        ClearSectorRuntimeObjects(context.world, assets, state.runtimeObjects);
    }
    preview.ShutdownRendererResources(assets);
    if (!engine::IsNull(state.editorTextureScope)) {
        assets.UnloadScope(state.editorTextureScope);
    }
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }
    if (!engine::IsNull(state.spritePicker.previewScope)) {
        assets.UnloadScope(state.spritePicker.previewScope);
    }
    state = SectorEditorState{};
    uiState = SectorEditorUiState{};
    canvasRect = {};
    statusText.clear();
    engineContext = nullptr;
    initialized = false;
}

void SectorEditor::Update(engine::EngineContext& context, float dt)
{
    engine::Input& input = context.input;
    engine::AssetManager& assets = context.assets;
    if (IsLightmapBakeBlocking()) {
        CancelAuthoringVertexDrag(nullptr);
        CancelLightDrag(nullptr);
        CancelPendingAuthoringLine(nullptr);
        CancelPendingAuthoringRectangle(nullptr);
        return;
    }

    if (state.mode == SectorEditorMode::Preview3D) {
        const Vector3 playerPosition = state.freeflyController.pose.position;
        UpdateSectorRuntimeObjects(context.world, assets, state.runtimeObjects, state.topologyMap, dt, &playerPosition);
        state.runtimeObjects.dynamicDoorColliders.clear();
        CollectSectorDoorDynamicColliders(context.world, state.runtimeObjects.dynamicDoorColliders);
        state.runtimeObjects.dynamicPortalBlockers.clear();
        CollectSectorDoorDynamicPortalBlockers(context.world, state.runtimeObjects.dynamicPortalBlockers);
        preview.AdvanceRuntime(dt);
        const bool hasBlockingModal = state.texturePicker.open
                || state.spritePicker.open
                || HasDocumentModalOpen();
        if (hasBlockingModal) {
            return;
        }
        const bool canInteractWithDoors = state.previewControlMode == SectorPreviewControlMode::Gameplay
                && state.freeflyController.mouseLookEnabled
                && !uiState.keyboardCaptured;
        if (canInteractWithDoors) {
            input.ForEachEvent(
                    engine::InputEventType::KeyPressed,
                    true,
                    [this, &context](engine::InputEvent& event) {
                        if (event.key.key != KEY_F) {
                            return;
                        }
                        if (ToggleTargetedSectorDoorInteractionSystem(
                                    context.world,
                                    state.freeflyController.pose.position,
                                    PreviewForwardFromPose(state.freeflyController.pose))) {
                            engine::ConsumeEvent(event);
                        }
                    });
        }
        UpdatePreview3D(input, assets, dt);
        return;
    }

    canvasRect = BuildCanvasRect();
    if (state.texturePicker.open || state.addMapTexture.open || state.spritePicker.open || HasDocumentModalOpen()) {
        return;
    }
    UpdateHoverAndMouse(input);
    HandleCanvasInput(input, dt);
}

void SectorEditor::Render(engine::AssetManager& assets)
{
    if (state.mode == SectorEditorMode::Preview3D) {
        RenderPreview3D(assets);
        return;
    }

    canvasRect = BuildCanvasRect();
    DrawRectangleRec(canvasRect, Color{12, 15, 20, 255});

    BeginScissorMode(
            static_cast<int>(std::round(canvasRect.x)),
            static_cast<int>(std::round(canvasRect.y)),
            static_cast<int>(std::round(canvasRect.width)),
            static_cast<int>(std::round(canvasRect.height))
    );

    if (state.showGrid) {
        DrawGrid();
    }
    DrawTopologyDocument();
    EndScissorMode();

    DrawRectangleLinesEx(canvasRect, 2.0f, Color{67, 76, 93, 255});
}

void SectorEditor::RenderUI(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    PollLightmapBakeResult(assets);

    if (state.mode == SectorEditorMode::Preview3D) {
        engine::BeginUI(ui, input);
        if (IsLightmapBakeBlocking()) {
            DrawLightmapBakeModal(ui, config, input, assets, font);
            uiState.keyboardCaptured = true;
            engine::EndUI(ui, config, input, assets);
            return;
        }
        if (state.previewUiHidden) {
            ui.hotId = 0;
            ui.activeId = 0;
            ui.focusedId = 0;
            ui.openOptionId = 0;
        } else {
            DrawPreviewOverlay(ui, config, input, assets, font, smallFont);
        }
        if (!state.previewUiHidden
                && !state.texturePicker.open
                && !state.spritePicker.open
                && !state.decalTintModal.open
                && !state.doorTextureSettingsModal.open
                && !state.previewSettingsModal.open) {
            DrawPreviewUvPanel(ui, config, input, assets, font);
        }
        if (state.decalTintModal.open) {
            DrawDecalTintModal(ui, config, input, assets, font);
        }
        if (state.doorTextureSettingsModal.open) {
            DrawDoorTextureSettingsModal(ui, config, input, assets, font, smallFont);
        }
        if (state.previewSettingsModal.open) {
            DrawPreviewSettingsModal(ui, config, input, assets, font);
        }
        DrawTexturePickerModal(ui, config, input, assets, font);
        DrawSpritePickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = ui.focusedId != 0;
        if (state.texturePicker.open
                || state.spritePicker.open
                || state.decalTintModal.open
                || state.doorTextureSettingsModal.open
                || state.previewSettingsModal.open) {
            uiState.keyboardCaptured = true;
        }
        engine::EndUI(ui, config, input, assets);
        return;
    }

    engine::BeginUI(ui, input);
    if (IsLightmapBakeBlocking()) {
        DrawLightmapBakeModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.confirmationModal.open) {
        DrawConfirmationModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.saveLevelModal.open) {
        DrawSaveLevelModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.loadLevelModal.open) {
        DrawLoadLevelModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.decalTintModal.open) {
        DrawDecalTintModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.doorTextureSettingsModal.open) {
        DrawDoorTextureSettingsModal(ui, config, input, assets, font, smallFont);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.previewSettingsModal.open) {
        DrawPreviewSettingsModal(ui, config, input, assets, font);
        DrawTexturePickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.addMapTexture.open) {
        DrawAddMapTextureModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.texturePicker.open) {
        DrawTexturePickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.spritePicker.open) {
        DrawSpritePickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }

    DrawToolsPanel(ui, config, input, assets, font);
    DrawSectorsPanel(ui, config, input, assets, font, smallFont);
    DrawStatusPanel(ui, config, assets, smallFont);
    DrawAddMapTextureModal(ui, config, input, assets, font);
    DrawTexturePickerModal(ui, config, input, assets, font);
    DrawSpritePickerModal(ui, config, input, assets, font);
    uiState.keyboardCaptured = ui.focusedId != 0;
    if (state.texturePicker.open || state.addMapTexture.open || state.spritePicker.open || HasDocumentModalOpen()) {
        uiState.keyboardCaptured = true;
    }
    engine::EndUI(ui, config, input, assets);
}

bool SectorEditor::IsPreview3DActive() const
{
    return state.mode == SectorEditorMode::Preview3D;
}

Vector2 SectorEditor::MapToScreen(Vector2 map) const
{
    return CanvasWorldToScreen(SectorAuthoringToWorldPosition(map));
}

Vector2 SectorEditor::ScreenToMap(Vector2 screen) const
{
    return SectorWorldToAuthoringPosition(ScreenToCanvasWorld(screen));
}

SectorEditorToolContext SectorEditor::BuildToolContext(engine::Input* input)
{
    SectorEditorToolContext context{
            state,
            statusText,
            input,
            canvasRect};
    context.currentSnappedSectorPoint = [this]() {
        return CurrentSnappedSectorPoint();
    };
    context.toTopologyCoordPoint = [this](
            SectorPoint point,
            SectorTopologyCoordPoint& outPoint,
            std::string& error) {
        return ToTopologyCoordPoint(point, outPoint, error);
    };
    context.mapToScreen = [this](Vector2 map) {
        return MapToScreen(map);
    };
    context.screenToMap = [this](Vector2 screen) {
        return ScreenToMap(screen);
    };
    context.clearTopologySelectionOnly = [this]() {
        ClearTopologySelectionOnly();
    };
    context.clearSelection = [this]() {
        ClearSelection();
    };
    context.selectAuthoringLine = [this](int lineId) {
        SelectSectorEditorAuthoringLine(state, lineId);
    };
    context.hoverAuthoringLine = [this](int lineId) {
        SetHoveredSectorEditorAuthoringLine(state, lineId);
    };
    context.findAuthoringLineNearScreenPoint = [this](Vector2 screenPoint) {
        return FindAuthoringLineNearScreenPoint(screenPoint);
    };
    context.commitAuthoringLinePoint = [this](SectorTopologyCoordPoint point) {
        return ClickSectorEditorAuthoringLineTool(state, point);
    };
    context.cancelAuthoringLineChain = [this]() {
        CancelSectorEditorAuthoringLineToolChain(state);
    };
    context.commitAuthoringRectangle = [this](
            SectorTopologyCoordPoint firstCorner,
            SectorTopologyCoordPoint oppositeCorner,
            SectorEditorAuthoringRectangleResult* outResult) {
        return AddSectorEditorAuthoringRectangle(state, firstCorner, oppositeCorner, outResult);
    };
    context.resolveAuthoringInsertVertexPoint = [this](
            int lineId,
            Vector2 mapPoint,
            SectorTopologyCoordPoint& outPoint,
            std::string& error) {
        return TryResolveAuthoringInsertVertexPoint(lineId, mapPoint, outPoint, error);
    };
    context.commitAuthoringInsertVertex = [this](
            int lineId,
            SectorTopologyCoordPoint point,
            SectorAuthoringInsertVertexResult* outResult) {
        return InsertSectorEditorAuthoringVertexOnLine(state, lineId, point, outResult);
    };
    context.buildSelectPickCandidates = [this](Vector2 screenPoint) {
        return BuildSelectPickCandidates(screenPoint);
    };
    context.currentPickSelectionTarget = [this]() {
        return CurrentPickSelectionTarget();
    };
    context.buildSelectionServiceContext = [this]() {
        return BuildSelectionServiceContext();
    };
    context.buildManipulationServiceContext = [this]() {
        return BuildManipulationServiceContext();
    };
    return context;
}

Vector2 SectorEditor::CanvasWorldToScreen(Vector2 canvasWorld) const
{
    return Vector2{
            canvasRect.x + canvasRect.width * 0.5f + (canvasWorld.x - state.viewCenter.x) * state.viewZoom,
            canvasRect.y + canvasRect.height * 0.5f + (canvasWorld.y - state.viewCenter.y) * state.viewZoom
    };
}

Vector2 SectorEditor::ScreenToCanvasWorld(Vector2 screen) const
{
    return Vector2{
            state.viewCenter.x + (screen.x - (canvasRect.x + canvasRect.width * 0.5f)) / state.viewZoom,
            state.viewCenter.y + (screen.y - (canvasRect.y + canvasRect.height * 0.5f)) / state.viewZoom
    };
}

Vector2 SectorEditor::SnapMapPoint(Vector2 map) const
{
    const float grid = static_cast<float>(std::max(1, state.gridSize));
    Vector2 snapped{
            std::round(map.x / grid) * grid,
            std::round(map.y / grid) * grid
    };

    if (state.currentTool != SectorEditorTool::AuthoringLine
            && state.currentTool != SectorEditorTool::AuthoringRectangle) {
        return snapped;
    }

    const float threshold = std::max(
            SectorWorldToAuthoringDistance(ScreenVertexSnapPixels / std::max(1.0f, state.viewZoom)),
            grid * 0.20f
    );
    float bestDistance2 = threshold * threshold;
    bool found = false;
    Vector2 best = snapped;
    if (state.currentTool == SectorEditorTool::AuthoringLine
            || state.currentTool == SectorEditorTool::AuthoringRectangle) {
        for (const SectorAuthoringVertex& authoringVertex : state.authoringGraph.vertices) {
            const Vector2 vertex{
                    SectorCoordToVisibleAuthoring(authoringVertex.x),
                    SectorCoordToVisibleAuthoring(authoringVertex.y)};
            const float dx = vertex.x - map.x;
            const float dy = vertex.y - map.y;
            const float distance2 = dx * dx + dy * dy;
            if (distance2 <= bestDistance2) {
                bestDistance2 = distance2;
                best = vertex;
                found = true;
            }
        }
    } else {
        for (const SectorTopologyVertex& topologyVertex : state.topologyMap.vertices) {
            const Vector2 vertex = SectorTopologyVertexToMap(topologyVertex);
            const float dx = vertex.x - map.x;
            const float dy = vertex.y - map.y;
            const float distance2 = dx * dx + dy * dy;
            if (distance2 <= bestDistance2) {
                bestDistance2 = distance2;
                best = vertex;
                found = true;
            }
        }
    }

    return found ? best : snapped;
}

Rectangle SectorEditor::BuildLeftPanelRect() const
{
    return Rectangle{0.0f, 0.0f, LeftPanelWidth, EditorHeight - BottomPanelHeight};
}

Rectangle SectorEditor::BuildRightPanelRect() const
{
    return Rectangle{EditorWidth - RightPanelWidth, 0.0f, RightPanelWidth, EditorHeight - BottomPanelHeight};
}

Rectangle SectorEditor::BuildBottomPanelRect() const
{
    return Rectangle{0.0f, EditorHeight - BottomPanelHeight, EditorWidth, BottomPanelHeight};
}

Rectangle SectorEditor::BuildCanvasRect() const
{
    return Rectangle{
            LeftPanelWidth + PanelGap,
            PanelGap,
            EditorWidth - LeftPanelWidth - RightPanelWidth - PanelGap * 2.0f,
            EditorHeight - BottomPanelHeight - PanelGap * 2.0f
    };
}

bool SectorEditor::IsMouseOverCanvas(const engine::Input& input) const
{
    return Contains(canvasRect, input.MousePosition());
}

void SectorEditor::UpdateHoverAndMouse(engine::Input& input)
{
    state.rawMouseMap = ScreenToMap(input.MousePosition());
    state.snappedMouseMap = SnapMapPoint(state.rawMouseMap);
    if (state.pendingAuthoringRectangle.active) {
        std::string error;
        SectorTopologyCoordPoint currentCorner;
        if (ToTopologyCoordPoint(CurrentSnappedSectorPoint(), currentCorner, error)) {
            state.pendingAuthoringRectangle.currentCorner = currentCorner;
        }
    }
    if (state.currentTool == SectorEditorTool::AuthoringInsertVertex
            || state.pendingAuthoringInsertVertex.active) {
        UpdatePendingAuthoringInsertVertex(state.rawMouseMap);
    }
    state.hasHoveredVertex = false;
    state.hoveredTopologyLightId = -1;
    state.hoveredTopologyStaticSpotLightId = -1;
    state.hoveredTopologyDynamicLightId = -1;
    state.hoveredTopologyDynamicSpotLightId = -1;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    ClearSectorEditorAuthoringHover(state);

    if (!initialized || !IsMouseOverCanvas(input)) {
        return;
    }

    if (state.currentTool == SectorEditorTool::Select) {
        if (const SectorEditorToolModule* module = FindSectorEditorToolModule(state.currentTool)) {
            if (module->updateHover != nullptr) {
                SectorEditorToolContext toolContext = BuildToolContext(&input);
                module->updateHover(toolContext, state.rawMouseMap);
            }
        }
        return;
    }

    if (state.currentTool == SectorEditorTool::AuthoringMove) {
        int authoringVertexId = -1;
        SectorTopologyCoordPoint authoringVertexPoint{};
        if (FindAuthoringVertexNearScreenPoint(
                    input.MousePosition(),
                    authoringVertexId,
                    authoringVertexPoint)) {
            SetHoveredSectorEditorAuthoringVertex(state, authoringVertexId);
        }
        state.inspectedTopologyVertexId = -1;
        return;
    }

    if (state.currentTool == SectorEditorTool::AuthoringInsertVertex) {
        if (state.pendingAuthoringInsertVertex.lineId >= 0) {
            SetHoveredSectorEditorAuthoringLine(state, state.pendingAuthoringInsertVertex.lineId);
        }
        state.inspectedTopologyVertexId = -1;
        return;
    }

    if (state.currentTool == SectorEditorTool::StaticLight
            || state.currentTool == SectorEditorTool::Move) {
        const int lightId = FindTopologyLightNearScreenPoint(input.MousePosition());
        if (lightId >= 0) {
            state.hoveredTopologyLightId = lightId;
            state.inspectedTopologyVertexId = -1;
        } else if (state.currentTool == SectorEditorTool::Move
                && !IsSectorEditorGraphAuthoritativeMode()) {
            int vertexId = -1;
            SectorTopologyCoordPoint point;
            if (FindTopologyVertexNearScreenPoint(input.MousePosition(), vertexId, point)) {
                state.hasHoveredVertex = true;
                state.hoveredTopologyVertexId = vertexId;
                state.hoveredTopologyVertexPoint = point;
                state.inspectedTopologyVertexId = vertexId;
            } else {
                state.inspectedTopologyVertexId = -1;
            }
        }
    }

    if (state.currentTool == SectorEditorTool::StaticSpotLight) {
        int lightId = -1;
        SpotLightHandle handle = SpotLightHandle::Origin;
        if (!FindTopologyStaticSpotLightHandleNearScreenPoint(input.MousePosition(), lightId, handle)) {
            lightId = FindTopologyStaticSpotLightNearScreenPoint(input.MousePosition());
        }
        if (lightId >= 0) {
            state.hoveredTopologyStaticSpotLightId = lightId;
            state.inspectedTopologyVertexId = -1;
        }
    }

    if (state.currentTool == SectorEditorTool::DynamicLight) {
        const int lightId = FindTopologyDynamicLightNearScreenPoint(input.MousePosition());
        if (lightId >= 0) {
            state.hoveredTopologyDynamicLightId = lightId;
            state.inspectedTopologyVertexId = -1;
        }
    }

    if (state.currentTool == SectorEditorTool::DynamicSpotLight) {
        int lightId = -1;
        SpotLightHandle handle = SpotLightHandle::Origin;
        if (!FindTopologyDynamicSpotLightHandleNearScreenPoint(input.MousePosition(), lightId, handle)) {
            lightId = FindTopologyDynamicSpotLightNearScreenPoint(input.MousePosition());
        }
        if (lightId >= 0) {
            state.hoveredTopologyDynamicSpotLightId = lightId;
            state.inspectedTopologyVertexId = -1;
        }
    }
}

void SectorEditor::HandleCanvasInput(engine::Input& input, float dt)
{
    if (!initialized) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    SectorEditorManipulationServiceContext manipulationContext =
                            BuildManipulationServiceContext();
                    if (CancelFirstActiveSectorEditorManipulation(
                                manipulationContext,
                                "Cancelled authoring vertex move",
                                "Cancelled light move",
                                "Cancelled object move")) {
                    } else if (state.pendingAuthoringLine.active) {
                        CancelPendingAuthoringLine("Line chain stopped");
                    } else if (state.pendingAuthoringRectangle.active) {
                        CancelPendingAuthoringRectangle("Rectangle cancelled");
                    } else if (state.pendingAuthoringInsertVertex.active
                            || state.currentTool == SectorEditorTool::AuthoringInsertVertex) {
                        CancelPendingAuthoringInsertVertex("Insert Vertex cancelled");
                    } else if (state.selectedTopologyLightId >= 0
                            || state.selectedTopologyStaticSpotLightId >= 0
                            || state.selectedTopologyDynamicLightId >= 0
                            || state.selectedTopologyDynamicSpotLightId >= 0
                            || state.selectedRuntimeObjectId >= 0
                            || state.topologySelectionKind != TopologySelectionKind::None
                            || state.selectedAuthoring.kind != SectorAuthoringSelectionKind::None) {
                        ClearSelection();
                    } else {
                        state.currentTool = SectorEditorTool::Select;
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_DELETE) {
                    if (IsGraphAuthoringTool(state.currentTool)) {
                        if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line) {
                            DeleteSelectedAuthoringLine();
                        } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
                            DeleteSelectedAuthoringVertex();
                        } else {
                            statusText = "Select an authoring line or isolated authoring vertex to delete.";
                        }
                    } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line) {
                        DeleteSelectedAuthoringLine();
                    } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
                        DeleteSelectedAuthoringVertex();
                    } else if (state.topologySelectionKind == TopologySelectionKind::StaticLight
                            && state.selectedTopologyLightId >= 0) {
                        DeleteSelectedLight();
                    } else if (state.topologySelectionKind == TopologySelectionKind::StaticSpotLight
                            && state.selectedTopologyStaticSpotLightId >= 0) {
                        DeleteSelectedLight();
                    } else if (state.topologySelectionKind == TopologySelectionKind::DynamicLight
                            && state.selectedTopologyDynamicLightId >= 0) {
                        DeleteSelectedLight();
                    } else if (state.topologySelectionKind == TopologySelectionKind::DynamicSpotLight
                            && state.selectedTopologyDynamicSpotLightId >= 0) {
                        DeleteSelectedLight();
                    } else if (state.selectedRuntimeObjectId >= 0) {
                        DeleteSelectedRuntimeObject();
                    } else if (state.topologySelectionKind == TopologySelectionKind::Sector
                            && state.selectedTopologySectorId >= 0) {
                        statusText = LegacyTopologyMutationUnavailableMessage();
                    } else if (state.topologySelectionKind == TopologySelectionKind::Vertex
                            && state.selectedTopologyVertexId >= 0) {
                        statusText = "Standalone vertex deletion is not available; use Dissolve Vertex for simple degree-2 vertices.";
                    } else if (state.topologySelectionKind == TopologySelectionKind::SideDef
                            || state.topologySelectionKind == TopologySelectionKind::LineDef) {
                        statusText = "Direct linedef/sidedef deletion is not available yet.";
                    } else {
                        statusText = "Select a topology sector to delete.";
                    }
                    engine::ConsumeEvent(event);
                    return;
                }
            }
    );

    SectorEditorManipulationServiceContext manipulationContext =
            BuildManipulationServiceContext();
    if (IsAnySectorEditorManipulationActive(manipulationContext)) {
        UpdateActiveSectorEditorManipulation(manipulationContext, input);

        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this, &manipulationContext](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_RIGHT_BUTTON) {
                        CancelActiveSectorEditorManipulation(
                                manipulationContext,
                                "Cancelled authoring vertex move",
                                "Cancelled light move",
                                "Cancelled object move");
                        engine::ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                engine::InputEventType::MouseButtonReleased,
                true,
                [&manipulationContext](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_LEFT_BUTTON) {
                        FinishActiveSectorEditorManipulation(manipulationContext);
                        engine::ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                engine::InputEventType::MouseWheel,
                true,
                [](engine::InputEvent& event) {
                    engine::ConsumeEvent(event);
                }
        );
        return;
    }

    if (const SectorEditorToolModule* module = FindSectorEditorToolModule(state.currentTool)) {
        if (module->updateEarly != nullptr) {
            SectorEditorToolContext toolContext = BuildToolContext(&input);
            module->updateEarly(toolContext);
        }
    }
    if (IsAnySectorEditorManipulationActive(manipulationContext)) {
        return;
    }

    if (!uiState.keyboardCaptured) {
        Vector2 pan{};
        if (input.IsKeyDown(KEY_A)) {
            pan.x -= 1.0f;
        }
        if (input.IsKeyDown(KEY_D)) {
            pan.x += 1.0f;
        }
        if (input.IsKeyDown(KEY_W)) {
            pan.y -= 1.0f;
        }
        if (input.IsKeyDown(KEY_S)) {
            pan.y += 1.0f;
        }

        if (pan.x != 0.0f || pan.y != 0.0f) {
            const float length = std::sqrt(pan.x * pan.x + pan.y * pan.y);
            pan.x /= length;
            pan.y /= length;
            const float mapUnits = (PanPixelsPerSecond * dt) / std::max(1.0f, state.viewZoom);
            state.viewCenter.x += pan.x * mapUnits;
            state.viewCenter.y += pan.y * mapUnits;
        }
    }

    if (!IsMouseOverCanvas(input)) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::MouseButtonPressed,
            true,
            [this, &input](engine::InputEvent& event) {
                if (event.mouseButton.button != MOUSE_LEFT_BUTTON
                        || !Contains(canvasRect, event.mouseButton.position)) {
                    return;
                }

                if (const SectorEditorToolModule* module = FindSectorEditorToolModule(state.currentTool)) {
                    if (module->handleMousePress != nullptr) {
                        SectorEditorToolContext toolContext = BuildToolContext(&input);
                        if (module->handleMousePress(toolContext, event)) {
                            engine::ConsumeEvent(event);
                        }
                    }
                }
            }
    );

    input.ForEachEvent(
            engine::InputEventType::MouseWheel,
            true,
            [this, &input](engine::InputEvent& event) {
                const Vector2 mouseBefore = ScreenToCanvasWorld(input.MousePosition());
                const float zoomFactor = event.wheel.value > 0.0f ? 1.12f : 1.0f / 1.12f;
                state.viewZoom = std::clamp(state.viewZoom * zoomFactor, MinZoom, MaxZoom);
                const Vector2 mouseAfter = ScreenToCanvasWorld(input.MousePosition());
                state.viewCenter.x += mouseBefore.x - mouseAfter.x;
                state.viewCenter.y += mouseBefore.y - mouseAfter.y;
                engine::ConsumeEvent(event);
            }
    );

    if (const SectorEditorToolModule* module = FindSectorEditorToolModule(state.currentTool)) {
        if (module->update != nullptr) {
            SectorEditorToolContext toolContext = BuildToolContext(&input);
            module->update(toolContext);
        }
    }

    input.ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [this](engine::InputEvent& event) {
                if (!Contains(canvasRect, event.mouseClick.releasePosition)) {
                    return;
                }

                if (event.mouseClick.button == MOUSE_RIGHT_BUTTON) {
                    if (state.pendingAuthoringLine.active) {
                        CancelPendingAuthoringLine("Line chain stopped");
                        engine::ConsumeEvent(event);
                        return;
                    }
                    if (state.pendingAuthoringRectangle.active) {
                        CancelPendingAuthoringRectangle("Rectangle cancelled");
                        engine::ConsumeEvent(event);
                        return;
                    }
                    if (state.pendingAuthoringInsertVertex.active
                            || state.currentTool == SectorEditorTool::AuthoringInsertVertex) {
                        CancelPendingAuthoringInsertVertex("Insert Vertex cancelled");
                        engine::ConsumeEvent(event);
                        return;
                    }
                }

                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }

                if (state.currentTool == SectorEditorTool::StaticLight) {
                    AddStaticLightAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::StaticSpotLight) {
                    AddStaticSpotLightAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::DynamicLight) {
                    AddDynamicLightAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::DynamicSpotLight) {
                    AddDynamicSpotLightAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::RuntimeObject) {
                    AddRuntimeObjectAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Door) {
                    AddDoorAtPortal(event.mouseClick.releasePosition);
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Move) {
                    statusText = "Move: click a topology light";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::AuthoringMove) {
                    statusText = "Move Vertex: click an authoring vertex";
                    engine::ConsumeEvent(event);
                }
            }
    );
}

SectorEditorPickTarget SectorEditor::CurrentPickSelectionTarget() const
{
    if (state.selectedRuntimeObjectId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::RuntimeObject, state.selectedRuntimeObjectId};
    }
    if (state.topologySelectionKind == TopologySelectionKind::DynamicSpotLight
            && state.selectedTopologyDynamicSpotLightId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::DynamicSpotLight, state.selectedTopologyDynamicSpotLightId};
    }
    if (state.topologySelectionKind == TopologySelectionKind::DynamicLight
            && state.selectedTopologyDynamicLightId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::DynamicLight, state.selectedTopologyDynamicLightId};
    }
    if (state.topologySelectionKind == TopologySelectionKind::StaticSpotLight
            && state.selectedTopologyStaticSpotLightId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::StaticSpotLight, state.selectedTopologyStaticSpotLightId};
    }
    if (state.topologySelectionKind == TopologySelectionKind::StaticLight
            && state.selectedTopologyLightId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::StaticLight, state.selectedTopologyLightId};
    }
    if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex
            && state.selectedAuthoring.vertexId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::AuthoringVertex, state.selectedAuthoring.vertexId};
    }
    if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line
            && state.selectedAuthoring.lineId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::AuthoringLine, state.selectedAuthoring.lineId};
    }
    if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor
            && state.selectedAuthoring.faceAnchorId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::AuthoringFaceAnchor, state.selectedAuthoring.faceAnchorId};
    }
    return SectorEditorPickTarget{};
}

std::vector<SectorEditorPickCandidate> SectorEditor::BuildSelectPickCandidates(Vector2 screenPoint) const
{
    std::vector<SectorEditorPickCandidate> candidates;
    candidates.reserve(
            state.topologyMap.runtimeObjects.size()
            + state.topologyMap.dynamicSpotLights.size()
            + state.topologyMap.dynamicPointLights.size()
            + state.topologyMap.staticSpotLights.size()
            + state.topologyMap.staticLights.size()
            + 3);

    const auto addPointCandidate = [&](SectorEditorPickKind kind, int id, Vector2 center) {
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > ScreenLightPickPixels * ScreenLightPickPixels) {
            return;
        }
        candidates.push_back(SectorEditorPickCandidate{
                SectorEditorPickTarget{kind, id},
                distance2});
    };
    const auto addSpotCandidate = [&](SectorEditorPickKind kind, int id, Vector2 origin, Vector2 target) {
        const float originDx = origin.x - screenPoint.x;
        const float originDy = origin.y - screenPoint.y;
        const float targetDx = target.x - screenPoint.x;
        const float targetDy = target.y - screenPoint.y;
        const float originDistance2 = originDx * originDx + originDy * originDy;
        const float targetDistance2 = targetDx * targetDx + targetDy * targetDy;
        const float distance2 = std::min(originDistance2, targetDistance2);
        if (distance2 > ScreenLightPickPixels * ScreenLightPickPixels) {
            return;
        }
        candidates.push_back(SectorEditorPickCandidate{
                SectorEditorPickTarget{kind, id},
                distance2});
    };

    for (const SectorPlacedRuntimeObject& object : state.topologyMap.runtimeObjects) {
        addPointCandidate(
                SectorEditorPickKind::RuntimeObject,
                object.id,
                MapToScreen(Vector2{object.position.x, object.position.z}));
    }
    for (const SectorTopologyDynamicSpotLight& light : state.topologyMap.dynamicSpotLights) {
        addSpotCandidate(
                SectorEditorPickKind::DynamicSpotLight,
                light.id,
                MapToScreen(Vector2{light.position.x, light.position.z}),
                MapToScreen(Vector2{light.target.x, light.target.z}));
    }
    for (const SectorTopologyDynamicPointLight& light : state.topologyMap.dynamicPointLights) {
        addPointCandidate(
                SectorEditorPickKind::DynamicLight,
                light.id,
                MapToScreen(Vector2{light.position.x, light.position.z}));
    }
    for (const SectorTopologyStaticSpotLight& light : state.topologyMap.staticSpotLights) {
        addSpotCandidate(
                SectorEditorPickKind::StaticSpotLight,
                light.id,
                MapToScreen(Vector2{light.position.x, light.position.z}),
                MapToScreen(Vector2{light.target.x, light.target.z}));
    }
    for (const SectorTopologyStaticPointLight& light : state.topologyMap.staticLights) {
        addPointCandidate(
                SectorEditorPickKind::StaticLight,
                light.id,
                MapToScreen(Vector2{light.position.x, light.position.z}));
    }

    SectorAuthoringSelectionTarget authoringTarget;
    SectorTopologyCoordPoint authoringVertexPoint{};
    if (FindAuthoringSelectionNearScreenPoint(screenPoint, authoringTarget, authoringVertexPoint)) {
        if (authoringTarget.kind == SectorAuthoringSelectionKind::Vertex) {
            candidates.push_back(SectorEditorPickCandidate{
                    SectorEditorPickTarget{SectorEditorPickKind::AuthoringVertex, authoringTarget.vertexId},
                    0.0f});
        } else if (authoringTarget.kind == SectorAuthoringSelectionKind::Line) {
            candidates.push_back(SectorEditorPickCandidate{
                    SectorEditorPickTarget{SectorEditorPickKind::AuthoringLine, authoringTarget.lineId},
                    0.0f});
        } else if (authoringTarget.kind == SectorAuthoringSelectionKind::FaceAnchor) {
            candidates.push_back(SectorEditorPickCandidate{
                    SectorEditorPickTarget{SectorEditorPickKind::AuthoringFaceAnchor, authoringTarget.faceAnchorId},
                    0.0f});
        }
    }

    return SortSectorEditorPickCandidates(std::move(candidates));
}

void SectorEditor::StartAuthoringVertexDrag(int vertexId, SectorTopologyCoordPoint point)
{
    if (!IsValidSectorAuthoringId(vertexId)
            || FindSectorAuthoringVertex(state.authoringGraph, vertexId) == nullptr) {
        return;
    }

    SelectAuthoringVertex(vertexId);
    ClearTopologySelectionOnly();
    state.authoringVertexDrag.active = true;
    state.authoringVertexDrag.vertexId = vertexId;
    state.authoringVertexDrag.originalPoint = point;
    state.authoringVertexDrag.previewPoint = point;
    state.authoringVertexDrag.hasPreviewPoint = true;
    state.authoringVertexDrag.errorMessage.clear();

    size_t connectedCount = 0;
    for (const SectorAuthoringLine& line : state.authoringGraph.lines) {
        if (line.startVertexId == vertexId || line.endVertexId == vertexId) {
            ++connectedCount;
        }
    }
    statusText = connectedCount > 0
            ? TextFormat("Moving authoring vertex %d (%zu connected lines)", vertexId, connectedCount)
            : TextFormat("Moving authoring vertex %d", vertexId);
}

void SectorEditor::UpdateAuthoringVertexDrag(engine::Input& input)
{
    if (!state.authoringVertexDrag.active) {
        return;
    }

    std::string error;
    SectorTopologyCoordPoint snappedPoint;
    if (!SnapAuthoringVertexMoveTarget(ScreenToMap(input.MousePosition()), snappedPoint, error)) {
        state.authoringVertexDrag.errorMessage = error;
        state.authoringVertexDrag.hasPreviewPoint = false;
        statusText = TextFormat("Authoring move rejected: %s", error.c_str());
        return;
    }

    state.authoringVertexDrag.previewPoint = snappedPoint;
    state.authoringVertexDrag.hasPreviewPoint = true;
    state.authoringVertexDrag.errorMessage.clear();
    if (SameTopologyPoint(snappedPoint, state.authoringVertexDrag.originalPoint)) {
        statusText = "Moving authoring vertex: original point";
    } else {
        statusText = TextFormat("Moving authoring vertex %d", state.authoringVertexDrag.vertexId);
    }
}

void SectorEditor::FinishAuthoringVertexDrag()
{
    if (!state.authoringVertexDrag.active) {
        return;
    }

    const int vertexId = state.authoringVertexDrag.vertexId;
    const SectorTopologyCoordPoint original = state.authoringVertexDrag.originalPoint;
    const SectorTopologyCoordPoint target = state.authoringVertexDrag.previewPoint;
    if (!state.authoringVertexDrag.hasPreviewPoint) {
        const std::string error = state.authoringVertexDrag.errorMessage.empty()
                ? "Move target is outside authoring coordinate range"
                : state.authoringVertexDrag.errorMessage;
        state.authoringVertexDrag = AuthoringVertexDragState{};
        statusText = TextFormat("Authoring move rejected: %s", error.c_str());
        return;
    }

    if (SameTopologyPoint(target, original)) {
        state.authoringVertexDrag = AuthoringVertexDragState{};
        statusText = "Authoring vertex unchanged";
        return;
    }

    if (!MoveSectorEditorAuthoringVertex(state, vertexId, target)) {
        state.authoringVertexDrag = AuthoringVertexDragState{};
        statusText = "Authoring vertex move rejected";
        return;
    }

    SelectAuthoringVertex(vertexId);
    state.authoringVertexDrag = AuthoringVertexDragState{};
    statusText = TextFormat("Moved authoring vertex %d", vertexId);
}

void SectorEditor::CancelAuthoringVertexDrag(const char* message)
{
    state.authoringVertexDrag = AuthoringVertexDragState{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::StartLightDrag(int topologyLightId, SpotLightHandle spotHandle)
{
    const bool staticSpotSelected = state.topologySelectionKind == TopologySelectionKind::StaticSpotLight
            && state.selectedTopologyStaticSpotLightId == topologyLightId;
    const bool dynamicSpotSelected = state.topologySelectionKind == TopologySelectionKind::DynamicSpotLight
            && state.selectedTopologyDynamicSpotLightId == topologyLightId;
    const bool dynamicLightSelected = state.topologySelectionKind == TopologySelectionKind::DynamicLight
            && state.selectedTopologyDynamicLightId == topologyLightId;

    if (staticSpotSelected || state.currentTool == SectorEditorTool::StaticSpotLight) {
        const SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(
                state.topologyMap,
                topologyLightId);
        if (light == nullptr) {
            return;
        }

        SelectTopologyStaticSpotLight(topologyLightId);
        state.lightDrag.active = true;
        state.lightDrag.topologyLightId = topologyLightId;
        state.lightDrag.spotHandle = spotHandle;
        state.lightDrag.originalPosition = light->position;
        state.lightDrag.originalTarget = light->target;
        state.lightDrag.snappedPosition = spotHandle == SpotLightHandle::Target
                ? light->target
                : light->position;
        statusText = spotHandle == SpotLightHandle::Target
                ? TextFormat("Aiming static spot %d", light->id)
                : TextFormat("Moving static spot %d", light->id);
        return;
    }

    if (dynamicSpotSelected || state.currentTool == SectorEditorTool::DynamicSpotLight) {
        const SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(
                state.topologyMap,
                topologyLightId);
        if (light == nullptr) {
            return;
        }

        SelectTopologyDynamicSpotLight(topologyLightId);
        state.lightDrag.active = true;
        state.lightDrag.topologyLightId = topologyLightId;
        state.lightDrag.spotHandle = spotHandle;
        state.lightDrag.originalPosition = light->position;
        state.lightDrag.originalTarget = light->target;
        state.lightDrag.snappedPosition = spotHandle == SpotLightHandle::Target
                ? light->target
                : light->position;
        statusText = spotHandle == SpotLightHandle::Target
                ? TextFormat("Aiming dynamic spot %d", light->id)
                : TextFormat("Moving dynamic spot %d", light->id);
        return;
    }

    if (dynamicLightSelected || state.currentTool == SectorEditorTool::DynamicLight) {
        const SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                state.topologyMap,
                topologyLightId);
        if (light == nullptr) {
            return;
        }

        SelectTopologyDynamicLight(topologyLightId);
        state.lightDrag.active = true;
        state.lightDrag.topologyLightId = topologyLightId;
        state.lightDrag.originalPosition = light->position;
        state.lightDrag.snappedPosition = light->position;
        statusText = TextFormat("Moving dynamic light %d", light->id);
        return;
    }

    const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            state.topologyMap,
            topologyLightId);
    if (light == nullptr) {
        return;
    }
    SelectTopologyLight(topologyLightId);
    state.lightDrag.active = true;
    state.lightDrag.topologyLightId = topologyLightId;
    state.lightDrag.originalPosition = light->position;
    state.lightDrag.snappedPosition = light->position;
    statusText = TextFormat("Moving static light %d", light->id);
}

void SectorEditor::UpdateLightDrag(engine::Input& input)
{
    if (!state.lightDrag.active) {
        return;
    }

    const Vector2 snapped = SnapMapPoint(ScreenToMap(input.MousePosition()));
    state.lightDrag.snappedPosition = Vector3{
            snapped.x,
            state.lightDrag.originalPosition.y,
            snapped.y};

    if (state.topologySelectionKind == TopologySelectionKind::DynamicLight) {
        SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                state.topologyMap,
                state.lightDrag.topologyLightId);
        if (light == nullptr) {
            return;
        }
        light->position.x = state.lightDrag.snappedPosition.x;
        light->position.y = state.lightDrag.originalPosition.y;
        light->position.z = state.lightDrag.snappedPosition.z;
        statusText = TextFormat("Moving dynamic light %d", light->id);
        return;
    }

    if (state.topologySelectionKind == TopologySelectionKind::DynamicSpotLight) {
        SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(
                state.topologyMap,
                state.lightDrag.topologyLightId);
        if (light == nullptr) {
            return;
        }

        if (state.lightDrag.spotHandle == SpotLightHandle::Target) {
            light->target.x = state.lightDrag.snappedPosition.x;
            light->target.y = state.lightDrag.originalTarget.y;
            light->target.z = state.lightDrag.snappedPosition.z;
            statusText = TextFormat("Aiming dynamic spot %d", light->id);
            return;
        }

        const float dx = state.lightDrag.snappedPosition.x - state.lightDrag.originalPosition.x;
        const float dz = state.lightDrag.snappedPosition.z - state.lightDrag.originalPosition.z;
        light->position.x = state.lightDrag.snappedPosition.x;
        light->position.y = state.lightDrag.originalPosition.y;
        light->position.z = state.lightDrag.snappedPosition.z;
        light->target.x = state.lightDrag.originalTarget.x + dx;
        light->target.y = state.lightDrag.originalTarget.y;
        light->target.z = state.lightDrag.originalTarget.z + dz;
        statusText = TextFormat("Moving dynamic spot %d", light->id);
        return;
    }

    if (state.topologySelectionKind == TopologySelectionKind::StaticSpotLight) {
        SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(
                state.topologyMap,
                state.lightDrag.topologyLightId);
        if (light == nullptr) {
            return;
        }

        if (state.lightDrag.spotHandle == SpotLightHandle::Target) {
            light->target.x = state.lightDrag.snappedPosition.x;
            light->target.y = state.lightDrag.originalTarget.y;
            light->target.z = state.lightDrag.snappedPosition.z;
            statusText = TextFormat("Aiming static spot %d", light->id);
            return;
        }

        const float dx = state.lightDrag.snappedPosition.x - state.lightDrag.originalPosition.x;
        const float dz = state.lightDrag.snappedPosition.z - state.lightDrag.originalPosition.z;
        light->position.x = state.lightDrag.snappedPosition.x;
        light->position.y = state.lightDrag.originalPosition.y;
        light->position.z = state.lightDrag.snappedPosition.z;
        light->target.x = state.lightDrag.originalTarget.x + dx;
        light->target.y = state.lightDrag.originalTarget.y;
        light->target.z = state.lightDrag.originalTarget.z + dz;
        statusText = TextFormat("Moving static spot %d", light->id);
        return;
    }

    SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            state.topologyMap,
            state.lightDrag.topologyLightId);
    if (light == nullptr) {
        return;
    }
    light->position.x = state.lightDrag.snappedPosition.x;
    light->position.y = state.lightDrag.originalPosition.y;
    light->position.z = state.lightDrag.snappedPosition.z;
    statusText = TextFormat("Moving static light %d", light->id);
}

void SectorEditor::FinishLightDrag()
{
    if (!state.lightDrag.active) {
        return;
    }

    const int lightId = state.lightDrag.topologyLightId;
    const Vector3 original = state.lightDrag.originalPosition;
    const Vector3 originalTarget = state.lightDrag.originalTarget;
    const SpotLightHandle spotHandle = state.lightDrag.spotHandle;
    const TopologySelectionKind selectionKind = state.topologySelectionKind;
    state.lightDrag = LightDragState{};

    if (selectionKind == TopologySelectionKind::DynamicLight) {
        SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(state.topologyMap, lightId);
        if (light == nullptr) {
            return;
        }

        SelectTopologyDynamicLight(lightId);
        FinishTopologyActionResult(FinishMoveDynamicLight(state.topologyMap, lightId, original));
        return;
    }

    if (selectionKind == TopologySelectionKind::DynamicSpotLight) {
        SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(state.topologyMap, lightId);
        if (light == nullptr) {
            return;
        }

        SelectTopologyDynamicSpotLight(lightId);
        const bool movedOrigin = std::fabs(light->position.x - original.x) > GeometryEpsilon
                || std::fabs(light->position.z - original.z) > GeometryEpsilon;
        const bool movedTarget = std::fabs(light->target.x - originalTarget.x) > GeometryEpsilon
                || std::fabs(light->target.z - originalTarget.z) > GeometryEpsilon;
        SectorEditorTopologyActionResult result;
        result.changed = spotHandle == SpotLightHandle::Target
                ? movedTarget
                : (movedOrigin || movedTarget);
        result.status = result.changed
                ? TextFormat("Moved dynamic spot %d", lightId)
                : TextFormat("Dynamic spot %d unchanged", lightId);
        FinishTopologyActionResult(result);
        return;
    }

    if (selectionKind == TopologySelectionKind::StaticSpotLight) {
        SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(state.topologyMap, lightId);
        if (light == nullptr) {
            return;
        }

        SelectTopologyStaticSpotLight(lightId);
        const bool movedOrigin = std::fabs(light->position.x - original.x) > GeometryEpsilon
                || std::fabs(light->position.z - original.z) > GeometryEpsilon;
        const bool movedTarget = std::fabs(light->target.x - originalTarget.x) > GeometryEpsilon
                || std::fabs(light->target.z - originalTarget.z) > GeometryEpsilon;
        SectorEditorTopologyActionResult result;
        result.changed = spotHandle == SpotLightHandle::Target
                ? movedTarget
                : (movedOrigin || movedTarget);
        result.status = result.changed
                ? TextFormat("Moved static spot %d", lightId)
                : TextFormat("Static spot %d unchanged", lightId);
        FinishTopologyActionResult(result);
        return;
    }

    SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(state.topologyMap, lightId);
    if (light == nullptr) {
        return;
    }
    SelectTopologyLight(lightId);
    FinishTopologyActionResult(FinishMoveStaticLight(state.topologyMap, lightId, original));
}

void SectorEditor::CancelLightDrag(const char* message)
{
    if (state.lightDrag.active) {
        if (state.topologySelectionKind == TopologySelectionKind::DynamicLight) {
            SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                    state.topologyMap,
                    state.lightDrag.topologyLightId);
            if (light != nullptr) {
                light->position = state.lightDrag.originalPosition;
                SelectTopologyDynamicLight(light->id);
            }
        } else if (state.topologySelectionKind == TopologySelectionKind::DynamicSpotLight) {
            SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(
                    state.topologyMap,
                    state.lightDrag.topologyLightId);
            if (light != nullptr) {
                light->position = state.lightDrag.originalPosition;
                light->target = state.lightDrag.originalTarget;
                SelectTopologyDynamicSpotLight(light->id);
            }
        } else if (state.topologySelectionKind == TopologySelectionKind::StaticSpotLight) {
            SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(
                    state.topologyMap,
                    state.lightDrag.topologyLightId);
            if (light != nullptr) {
                light->position = state.lightDrag.originalPosition;
                light->target = state.lightDrag.originalTarget;
                SelectTopologyStaticSpotLight(light->id);
            }
        } else {
            SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
                    state.topologyMap,
                    state.lightDrag.topologyLightId);
            if (light != nullptr) {
                light->position = state.lightDrag.originalPosition;
                SelectTopologyLight(light->id);
            }
        }
    }

    state.lightDrag = LightDragState{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::StartRuntimeObjectDrag(int objectId)
{
    SectorEditorPlacedObjectDragContext dragContext = BuildRuntimeObjectDragContext();
    StartSectorEditorPlacedObjectDrag(dragContext, objectId);
}

void SectorEditor::UpdateRuntimeObjectDrag(engine::Input& input)
{
    SectorEditorPlacedObjectDragContext dragContext = BuildRuntimeObjectDragContext();
    UpdateSectorEditorPlacedObjectDrag(dragContext, input.MousePosition());
}

void SectorEditor::FinishRuntimeObjectDrag()
{
    SectorEditorPlacedObjectDragContext dragContext = BuildRuntimeObjectDragContext();
    FinishSectorEditorPlacedObjectDrag(dragContext);
}

void SectorEditor::CancelRuntimeObjectDrag(const char* message)
{
    SectorEditorPlacedObjectDragContext dragContext = BuildRuntimeObjectDragContext();
    CancelSectorEditorPlacedObjectDrag(dragContext, message);
}

void SectorEditor::UpdatePreview3D(engine::Input& input, engine::AssetManager& assets, float dt)
{
    bool controlModeToggled = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, &assets, &controlModeToggled](engine::InputEvent& event) {
                if (event.key.key == KEY_F1) {
                    state.useBakedAmbientOcclusion = !state.useBakedAmbientOcclusion;
                    statusText = state.useBakedAmbientOcclusion
                            ? "Baked AO enabled"
                            : "Baked AO disabled";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_F2) {
                    state.previewUiHidden = !state.previewUiHidden;
                    if (state.previewUiHidden) {
                        state.hoveredSurface3D = SectorSurfaceHit{};
                    }
                    statusText = state.previewUiHidden
                            ? "3D UI hidden"
                            : "3D UI shown";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_F3) {
                    if (state.spotLightPilot.active) {
                        statusText = "Finish spotlight pilot before changing 3D control mode";
                        engine::ConsumeEvent(event);
                        return;
                    }
                    TogglePreviewControlMode();
                    controlModeToggled = true;
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_F4) {
                    preview.ToggleDynamicLightingEnabled();
                    statusText = preview.DynamicLightingEnabled()
                            ? "Dynamic lighting enabled"
                            : "Dynamic lighting disabled";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_TAB || event.key.key == KEY_ESCAPE) {
                    CancelSpotLightPilot(nullptr);
                    LeavePreview3D();
                    engine::ConsumeEvent(event);
                }
            }
    );

    if (controlModeToggled) {
        UpdatePreview3DSelection(input);
        return;
    }

    if (state.mode == SectorEditorMode::Preview3D) {
        if (state.previewControlMode == SectorPreviewControlMode::FreeFly) {
            UpdateSectorFreeflyController(state.freeflyController, input, dt);
            preview.ApplyRendererPose(state.freeflyController.pose);
            preview.UpdateVisibilityDebug(
                    0,
                    0.0f,
                    false,
                    &state.runtimeObjects.dynamicPortalBlockers,
                    &engineContext->world);
        } else {
            const float previousVisualEyeY = preview.RendererPose().position.y;
            input.ForEachEvent(
                    engine::InputEventType::KeyPressed,
                    true,
                    [this](engine::InputEvent& event) {
                        if (event.key.key != KEY_F11) {
                            return;
                        }

                        SetSectorFreeflyMouseLookEnabled(
                                state.freeflyController,
                                !state.freeflyController.mouseLookEnabled);
                        engine::ConsumeEvent(event);
                    }
            );

            SectorFpsControllerInput controllerInput;
            controllerInput.moveForward = input.IsKeyDown(KEY_W);
            controllerInput.moveBackward = input.IsKeyDown(KEY_S);
            controllerInput.strafeLeft = input.IsKeyDown(KEY_A);
            controllerInput.strafeRight = input.IsKeyDown(KEY_D);
            controllerInput.run = input.IsKeyDown(KEY_LEFT_SHIFT) || input.IsKeyDown(KEY_RIGHT_SHIFT);
            controllerInput.mouseLookEnabled = state.freeflyController.mouseLookEnabled;
            controllerInput.mouseDelta = input.MouseDelta();
            const bool canConsumeGameplayJump =
                    state.mode == SectorEditorMode::Preview3D
                    && state.previewControlMode == SectorPreviewControlMode::Gameplay
                    && !uiState.keyboardCaptured
                    && !state.texturePicker.open
                    && !state.decalTintModal.open
                    && !state.previewSettingsModal.open;
            if (canConsumeGameplayJump) {
                input.ForEachEvent(
                        engine::InputEventType::KeyPressed,
                        true,
                        [&controllerInput](engine::InputEvent& event) {
                            if (event.key.key != KEY_SPACE) {
                                return;
                            }
                            controllerInput.jumpPressed = true;
                            engine::ConsumeEvent(event);
                        }
                );
            }
            UpdateSectorEditorGameplayPreview(state, controllerInput, previousVisualEyeY, dt);
            ApplyGameplayPoseToPreview();
            const SectorFpsControllerConfig normalizedVisibilityConfig =
                    NormalizeSectorFpsControllerConfig(state.fpsControllerConfig);
            preview.UpdateVisibilityDebug(
                    state.fpsControllerState.currentSectorId,
                    ClampRuntimeVisibilitySeedRadiusWorld(normalizedVisibilityConfig.playerRadius),
                    true,
                    &state.runtimeObjects.dynamicPortalBlockers,
                    &engineContext->world);
            state.freeflyController.pose = preview.RendererPose();
        }
        UpdatePreview3DSelection(input);
    }
}

void SectorEditor::UpdatePreview3DSelection(engine::Input& input)
{
    if (!initialized
            || !preview.IsRendererReady()
            || state.freeflyController.mouseLookEnabled
            || state.previewUiHidden
            || state.texturePicker.open
            || state.spotLightPilot.active) {
        state.hoveredSurface3D = SectorSurfaceHit{};
        return;
    }

    const Rectangle viewport{0.0f, 0.0f, EditorWidth, EditorHeight};
    const Vector2 mouse = input.MousePosition();
    const bool overPanel = IsValidTopologySurfaceEditTarget(state.selectedTopologySurface3D)
            && Contains(BuildPreviewUvPanelRect(), mouse);
    const bool overPreviewOverlay = IsPreviewOverlayMouseInteractive()
            && Contains(BuildPreviewOverlayInteractionRect(), mouse);
    state.hoveredSurface3D = overPanel
            || overPreviewOverlay
            ? SectorSurfaceHit{}
            : PickSectorSurface3D(mouse, viewport);

    input.ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [this, overPanel, overPreviewOverlay](engine::InputEvent& event) {
                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }
                if (overPanel) {
                    engine::ConsumeEvent(event);
                    return;
                }
                if (overPreviewOverlay) {
                    return;
                }
                if (state.hoveredSurface3D.hit) {
                    SelectSurface3D(state.hoveredSurface3D.surface);
                    statusText = TextFormat("Selected 3D %s", SurfaceKindName(state.hoveredSurface3D.surface.kind));
                    engine::ConsumeEvent(event);
                }
            }
    );
}

void SectorEditor::CancelPendingAuthoringLine(const char* message)
{
    if (const SectorEditorToolModule* module =
                FindSectorEditorToolModule(SectorEditorTool::AuthoringLine)) {
        if (module->cancel != nullptr) {
            SectorEditorToolContext toolContext = BuildToolContext(nullptr);
            module->cancel(toolContext, message);
            return;
        }
    }
    CancelSectorEditorAuthoringLineToolChain(state);
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::CancelPendingAuthoringRectangle(const char* message)
{
    if (const SectorEditorToolModule* module =
                FindSectorEditorToolModule(SectorEditorTool::AuthoringRectangle)) {
        if (module->cancel != nullptr) {
            SectorEditorToolContext toolContext = BuildToolContext(nullptr);
            module->cancel(toolContext, message);
            return;
        }
    }
    state.pendingAuthoringRectangle = PendingAuthoringRectangleDraw{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::CancelPendingAuthoringInsertVertex(const char* message)
{
    if (const SectorEditorToolModule* module =
                FindSectorEditorToolModule(SectorEditorTool::AuthoringInsertVertex)) {
        if (module->cancel != nullptr) {
            SectorEditorToolContext toolContext = BuildToolContext(nullptr);
            module->cancel(toolContext, message);
            return;
        }
    }
    state.pendingAuthoringInsertVertex = PendingAuthoringInsertVertex{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::BeginPendingAuthoringInsertVertex(int lineId)
{
    if (FindSectorAuthoringLine(state.authoringGraph, lineId) == nullptr) {
        statusText = "Insert Vertex: select or click an authoring line";
        return;
    }

    if (state.pendingAuthoringLine.active) {
        CancelPendingAuthoringLine("Cancelled authoring line");
    }
    if (state.pendingAuthoringRectangle.active) {
        CancelPendingAuthoringRectangle("Rectangle cancelled");
    }
    if (state.authoringVertexDrag.active) {
        CancelAuthoringVertexDrag("Cancelled authoring vertex move");
    }
    if (state.lightDrag.active) {
        CancelLightDrag("Cancelled light move");
    }

    ClearTopologySelectionOnly();
    SelectSectorEditorAuthoringLine(state, lineId);
    state.currentTool = SectorEditorTool::AuthoringInsertVertex;
    state.pendingAuthoringInsertVertex = PendingAuthoringInsertVertex{};
    state.pendingAuthoringInsertVertex.active = true;
    state.pendingAuthoringInsertVertex.lineId = lineId;
    statusText = "Insert Vertex: click point on selected line, Esc/right click cancels";
}

bool SectorEditor::TryResolveAuthoringInsertVertexPoint(
        int lineId,
        Vector2 mapPoint,
        SectorTopologyCoordPoint& outPoint,
        std::string& error) const
{
    outPoint = SectorTopologyCoordPoint{};
    error.clear();

    const SectorAuthoringLine* line = FindSectorAuthoringLine(state.authoringGraph, lineId);
    if (line == nullptr) {
        error = "Insert Vertex: select or click an authoring line";
        return false;
    }

    const SectorAuthoringVertex* start =
            FindSectorAuthoringVertex(state.authoringGraph, line->startVertexId);
    const SectorAuthoringVertex* end =
            FindSectorAuthoringVertex(state.authoringGraph, line->endVertexId);
    if (start == nullptr || end == nullptr
            || (start->x == end->x && start->y == end->y)) {
        error = "Insert Vertex unavailable: selected authoring line is invalid";
        return false;
    }

    const int64_t dx = static_cast<int64_t>(end->x) - start->x;
    const int64_t dy = static_cast<int64_t>(end->y) - start->y;
    const int64_t latticeCount = std::gcd(std::llabs(dx), std::llabs(dy));
    if (latticeCount <= 1) {
        error = "Insert point is too close to an endpoint";
        return false;
    }

    const double mouseX = static_cast<double>(mapPoint.x)
            * static_cast<double>(SectorCoordSubdivisions);
    const double mouseY = static_cast<double>(mapPoint.y)
            * static_cast<double>(SectorCoordSubdivisions);
    const double apX = mouseX - static_cast<double>(start->x);
    const double apY = mouseY - static_cast<double>(start->y);
    const double lengthSquared = static_cast<double>(dx) * static_cast<double>(dx)
            + static_cast<double>(dy) * static_cast<double>(dy);
    if (lengthSquared <= 0.0) {
        error = "Insert Vertex unavailable: selected authoring line is invalid";
        return false;
    }
    const double t = std::clamp(
            (apX * static_cast<double>(dx) + apY * static_cast<double>(dy)) / lengthSquared,
            0.0,
            1.0);
    const int64_t nearestLattice = std::clamp<int64_t>(
            static_cast<int64_t>(std::llround(t * static_cast<double>(latticeCount))),
            1,
            latticeCount - 1);

    const SectorCoord stepX = static_cast<SectorCoord>(dx / latticeCount);
    const SectorCoord stepY = static_cast<SectorCoord>(dy / latticeCount);
    const auto pointAt = [&](int64_t index) {
        return SectorTopologyCoordPoint{
                static_cast<SectorCoord>(static_cast<int64_t>(start->x) + static_cast<int64_t>(stepX) * index),
                static_cast<SectorCoord>(static_cast<int64_t>(start->y) + static_cast<int64_t>(stepY) * index)};
    };

    const int64_t snapStep = std::max<int64_t>(
            1,
            static_cast<int64_t>(std::max(1, state.gridSize)) * SectorCoordSubdivisions);
    const auto alignedToSnapStep = [&](int64_t index) {
        const SectorTopologyCoordPoint point = pointAt(index);
        return CoordAlignedToStep(point.x, snapStep)
                && CoordAlignedToStep(point.y, snapStep);
    };

    const int64_t xPeriod = CoordinateSequencePeriod(stepX, snapStep);
    const int64_t yPeriod = CoordinateSequencePeriod(stepY, snapStep);
    const int64_t searchPeriod = LcmClamped(xPeriod, yPeriod);
    bool foundSnapCandidate = false;
    int64_t bestSnapIndex = nearestLattice;
    for (int64_t offset = 0; offset <= searchPeriod; ++offset) {
        const int64_t candidates[2] = {
                nearestLattice - offset,
                nearestLattice + offset};
        for (int64_t candidate : candidates) {
            if (candidate <= 0 || candidate >= latticeCount) {
                continue;
            }
            if (!alignedToSnapStep(candidate)) {
                continue;
            }
            bestSnapIndex = candidate;
            foundSnapCandidate = true;
            break;
        }
        if (foundSnapCandidate) {
            break;
        }
    }

    const int64_t chosenIndex = foundSnapCandidate ? bestSnapIndex : nearestLattice;
    if (chosenIndex <= 0 || chosenIndex >= latticeCount) {
        error = "Insert point is too close to an endpoint";
        return false;
    }

    outPoint = pointAt(chosenIndex);
    return true;
}

void SectorEditor::UpdatePendingAuthoringInsertVertex(Vector2 mapPoint)
{
    if (const SectorEditorToolModule* module =
                FindSectorEditorToolModule(SectorEditorTool::AuthoringInsertVertex)) {
        if (module->updateHover != nullptr) {
            SectorEditorToolContext toolContext = BuildToolContext(nullptr);
            module->updateHover(toolContext, mapPoint);
            return;
        }
    }
}

SectorPoint SectorEditor::CurrentSnappedSectorPoint() const
{
    const SectorPoint point = Vector2ToSectorPoint(state.snappedMouseMap);
    SectorPoint canonical;
    std::string error;
    return ToCanonicalSectorPoint(point, canonical, error) ? canonical : point;
}

bool SectorEditor::ToTopologyCoordPoint(
        SectorPoint point,
        SectorTopologyCoordPoint& outPoint,
        std::string& error) const
{
    SectorCoord x = 0;
    SectorCoord y = 0;
    if (!VisibleAuthoringToSectorCoord(point.x, x)
            || !VisibleAuthoringToSectorCoord(point.y, y)) {
        error = "Point is outside topology coordinate range";
        return false;
    }
    outPoint = SectorTopologyCoordPoint{x, y};
    error.clear();
    return true;
}

bool SectorEditor::ToCanonicalSectorPoint(
        SectorPoint point,
        SectorPoint& outPoint,
        std::string& error) const
{
    SectorTopologyCoordPoint topologyPoint;
    if (!ToTopologyCoordPoint(point, topologyPoint, error)) {
        return false;
    }
    outPoint = SectorTopologyCoordPointToSectorPoint(topologyPoint);
    return true;
}

SectorEditorManipulationServiceContext SectorEditor::BuildManipulationServiceContext()
{
    SectorEditorManipulationServiceContext context{
            state,
            uiState,
            statusText};
    context.userData = this;
    context.currentPickSelectionTarget = [](void* userData) {
        return static_cast<SectorEditor*>(userData)->CurrentPickSelectionTarget();
    };
    context.buildSelectPickCandidates = [](void* userData, Vector2 point) {
        return static_cast<SectorEditor*>(userData)->BuildSelectPickCandidates(point);
    };
    context.findStaticSpotLightHandle = [](
            void* userData,
            Vector2 point,
            int& outLightId,
            SpotLightHandle& outHandle) {
        return static_cast<SectorEditor*>(userData)->FindTopologyStaticSpotLightHandleNearScreenPoint(
                point,
                outLightId,
                outHandle);
    };
    context.findDynamicSpotLightHandle = [](
            void* userData,
            Vector2 point,
            int& outLightId,
            SpotLightHandle& outHandle) {
        return static_cast<SectorEditor*>(userData)->FindTopologyDynamicSpotLightHandleNearScreenPoint(
                point,
                outLightId,
                outHandle);
    };
    context.placedObjectMoveProvider = &SectorEditorPlacedObjectMoveProvider();
    context.screenToMap = [this](Vector2 screenPoint) {
        return ScreenToMap(screenPoint);
    };
    context.snapMapPoint = [this](Vector2 mapPoint) {
        return SnapMapPoint(mapPoint);
    };
    context.selectRuntimeObject = [this](int objectId) {
        SelectRuntimeObject(objectId);
    };
    context.updateCachedRuntimeObjectDraw = [this](const SectorPlacedRuntimeObject& object) {
        UpdateCachedRuntimeObjectDraw(state.topologyRenderCache, object);
    };
    context.markTopologyDocumentEdited = [this](const char* status) {
        MarkTopologyDocumentEdited(status);
    };
    context.refreshRuntimeObjectsAfterAuthoringEdit = [this]() {
        RefreshRuntimeObjectsAfterAuthoringEdit();
    };
    context.startAuthoringVertexDrag = [](void* userData, int vertexId, SectorTopologyCoordPoint point) {
        static_cast<SectorEditor*>(userData)->StartAuthoringVertexDrag(vertexId, point);
    };
    context.startRuntimeObjectDrag = [](void* userData, int objectId) {
        static_cast<SectorEditor*>(userData)->StartRuntimeObjectDrag(objectId);
    };
    context.startLightDrag = [](void* userData, int topologyLightId, SpotLightHandle spotHandle) {
        static_cast<SectorEditor*>(userData)->StartLightDrag(topologyLightId, spotHandle);
    };
    context.updateAuthoringVertexDrag = [](void* userData, engine::Input& input) {
        static_cast<SectorEditor*>(userData)->UpdateAuthoringVertexDrag(input);
    };
    context.finishAuthoringVertexDrag = [](void* userData) {
        static_cast<SectorEditor*>(userData)->FinishAuthoringVertexDrag();
    };
    context.cancelAuthoringVertexDrag = [](void* userData, const char* message) {
        static_cast<SectorEditor*>(userData)->CancelAuthoringVertexDrag(message);
    };
    context.updateRuntimeObjectDrag = [](void* userData, engine::Input& input) {
        static_cast<SectorEditor*>(userData)->UpdateRuntimeObjectDrag(input);
    };
    context.finishRuntimeObjectDrag = [](void* userData) {
        static_cast<SectorEditor*>(userData)->FinishRuntimeObjectDrag();
    };
    context.cancelRuntimeObjectDrag = [](void* userData, const char* message) {
        static_cast<SectorEditor*>(userData)->CancelRuntimeObjectDrag(message);
    };
    context.updateLightDrag = [](void* userData, engine::Input& input) {
        static_cast<SectorEditor*>(userData)->UpdateLightDrag(input);
    };
    context.finishLightDrag = [](void* userData) {
        static_cast<SectorEditor*>(userData)->FinishLightDrag();
    };
    context.cancelLightDrag = [](void* userData, const char* message) {
        static_cast<SectorEditor*>(userData)->CancelLightDrag(message);
    };
    return context;
}

SectorEditorSelectionServiceContext SectorEditor::BuildSelectionServiceContext()
{
    SectorEditorSelectionServiceContext context{
            state,
            uiState,
            &statusText,
            this,
            [](void* userData, const char* message) {
                static_cast<SectorEditor*>(userData)->CancelSpotLightPilot(message);
            }};
    return context;
}

SectorTopologySector* SectorEditor::SelectedTopologySector()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologySector(context);
}

const SectorTopologySector* SectorEditor::SelectedTopologySector() const
{
    SectorEditorSelectionServiceContext context{
            const_cast<SectorEditorState&>(state),
            const_cast<SectorEditorUiState&>(uiState),
            nullptr,
            nullptr,
            nullptr};
    return SelectedSectorEditorTopologySector(context);
}

SectorTopologyVertex* SectorEditor::SelectedTopologyVertex()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyVertex(context);
}

const SectorTopologyVertex* SectorEditor::SelectedTopologyVertex() const
{
    SectorEditorSelectionServiceContext context{
            const_cast<SectorEditorState&>(state),
            const_cast<SectorEditorUiState&>(uiState),
            nullptr,
            nullptr,
            nullptr};
    return SelectedSectorEditorTopologyVertex(context);
}

SectorTopologySideDef* SectorEditor::SelectedTopologySideDef()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologySideDef(context);
}

const SectorTopologySideDef* SectorEditor::SelectedTopologySideDef() const
{
    SectorEditorSelectionServiceContext context{
            const_cast<SectorEditorState&>(state),
            const_cast<SectorEditorUiState&>(uiState),
            nullptr,
            nullptr,
            nullptr};
    return SelectedSectorEditorTopologySideDef(context);
}

SectorTopologyLineDef* SectorEditor::SelectedTopologyLineDef()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyLineDef(context);
}

const SectorTopologyLineDef* SectorEditor::SelectedTopologyLineDef() const
{
    SectorEditorSelectionServiceContext context{
            const_cast<SectorEditorState&>(state),
            const_cast<SectorEditorUiState&>(uiState),
            nullptr,
            nullptr,
            nullptr};
    return SelectedSectorEditorTopologyLineDef(context);
}

SectorTopologyStaticPointLight* SectorEditor::SelectedTopologyLight()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyLight(context);
}

const SectorTopologyStaticPointLight* SectorEditor::SelectedTopologyLight() const
{
    SectorEditorSelectionServiceContext context{
            const_cast<SectorEditorState&>(state),
            const_cast<SectorEditorUiState&>(uiState),
            nullptr,
            nullptr,
            nullptr};
    return SelectedSectorEditorTopologyLight(context);
}

SectorTopologyStaticSpotLight* SectorEditor::SelectedTopologyStaticSpotLight()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyStaticSpotLight(context);
}

const SectorTopologyStaticSpotLight* SectorEditor::SelectedTopologyStaticSpotLight() const
{
    SectorEditorSelectionServiceContext context{
            const_cast<SectorEditorState&>(state),
            const_cast<SectorEditorUiState&>(uiState),
            nullptr,
            nullptr,
            nullptr};
    return SelectedSectorEditorTopologyStaticSpotLight(context);
}

SectorTopologyDynamicPointLight* SectorEditor::SelectedTopologyDynamicLight()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyDynamicLight(context);
}

const SectorTopologyDynamicPointLight* SectorEditor::SelectedTopologyDynamicLight() const
{
    SectorEditorSelectionServiceContext context{
            const_cast<SectorEditorState&>(state),
            const_cast<SectorEditorUiState&>(uiState),
            nullptr,
            nullptr,
            nullptr};
    return SelectedSectorEditorTopologyDynamicLight(context);
}

SectorTopologyDynamicSpotLight* SectorEditor::SelectedTopologyDynamicSpotLight()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyDynamicSpotLight(context);
}

const SectorTopologyDynamicSpotLight* SectorEditor::SelectedTopologyDynamicSpotLight() const
{
    SectorEditorSelectionServiceContext context{
            const_cast<SectorEditorState&>(state),
            const_cast<SectorEditorUiState&>(uiState),
            nullptr,
            nullptr,
            nullptr};
    return SelectedSectorEditorTopologyDynamicSpotLight(context);
}

SectorPlacedRuntimeObject* SectorEditor::SelectedRuntimeObject()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorRuntimeObject(context);
}

const SectorPlacedRuntimeObject* SectorEditor::SelectedRuntimeObject() const
{
    SectorEditorSelectionServiceContext context{
            const_cast<SectorEditorState&>(state),
            const_cast<SectorEditorUiState&>(uiState),
            nullptr,
            nullptr,
            nullptr};
    return SelectedSectorEditorRuntimeObject(context);
}

void SectorEditor::ClearStaleTopologySelection()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    ClearStaleSectorEditorTopologySelection(context);
}

void SectorEditor::MarkTopologyDocumentEdited(const char* status)
{
    state.topologyDocumentDirty = true;
    state.hasUnsavedChanges = true;
    InvalidateTopologyRenderCache();
    if (status != nullptr && status[0] != '\0') {
        statusText = status;
    }
}

bool SectorEditor::FinishTopologyActionResult(const SectorEditorTopologyActionResult& result)
{
    if (!result.changed) {
        if (!result.status.empty()) {
            statusText = result.status;
        }
        return false;
    }

    MarkTopologyDocumentEdited(result.status.c_str());
    return true;
}

bool SectorEditor::SetAuthoringLineDefBlocksPlayer(int lineDefId, bool blocksPlayer)
{
    std::string status;
    const bool changed = SetSectorEditorAuthoringLineDefBlocksPlayer(
            state,
            lineDefId,
            blocksPlayer,
            &status);
    if (!status.empty()) {
        statusText = status;
    }
    if (changed && status.empty()) {
        return true;
    }
    if (changed) {
        RebuildSectorCollisionWorld();
    }
    return changed;
}

void SectorEditor::ClearTransientTopologyEditStateAfterGeometryChange()
{
    ClearStaleTopologySelection();
    state.topologyRenderWarning.clear();
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
}

void SectorEditor::SyncSelectedSectorIdBuffer()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SyncSectorEditorSelectedSectorIdBuffer(context);
}

void SectorEditor::SyncSelectedLightIdBuffer()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SyncSectorEditorSelectedLightIdBuffer(context);
}

bool SectorEditor::TryRenameSelectedDerivedSectorAuthoringName()
{
    SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        uiState.idEditError = "No topology sector selected";
        statusText = uiState.idEditError;
        return false;
    }

    const std::string newName = uiState.selectedSectorIdBuffer;
    if (newName == sector->name) {
        uiState.idEditError.clear();
        return true;
    }

    const bool hasAuthoringGraph = HasAuthoringGraphData();
    if (!hasAuthoringGraph) {
        uiState.idEditError = "Cannot edit sector property: authoring data is required.";
        statusText = uiState.idEditError;
        return true;
    }
    if (hasAuthoringGraph
            && (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                    || state.authoringDerivedTopologyStale
                    || !state.authoringDerivation.success)) {
        uiState.idEditError = "Sector name edit unavailable: derived topology is not current";
        statusText = uiState.idEditError;
        return true;
    }

    const bool hasFaceAnchorMapping =
            FindSectorEditorAuthoringFaceAnchorIdForTopologySector(state, sector->id) >= 0;
    if (hasFaceAnchorMapping) {
        MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                state,
                sector->id,
                TextFormat("Renamed authoring face anchor %d", sector->id),
                [&newName](SectorAuthoringFaceAnchor& anchor) {
                    if (anchor.name == newName) {
                        return false;
                    }
                    anchor.name = newName;
                    return true;
                });
        uiState.idEditError.clear();
        return true;
    }
    uiState.idEditError = "Sector name edit unavailable: selected sector has no face anchor mapping";
    statusText = uiState.idEditError;
    return true;
}

bool SectorEditor::TryRenameSelectedLight()
{
    if (SelectedTopologyLight() == nullptr
            && SelectedTopologyStaticSpotLight() == nullptr
            && SelectedTopologyDynamicLight() == nullptr
            && SelectedTopologyDynamicSpotLight() == nullptr) {
        uiState.idEditError = "No light selected";
        statusText = uiState.idEditError;
        return false;
    }

    uiState.idEditError = "Topology light IDs are stable";
    statusText = uiState.idEditError;
    return false;
}

bool SectorEditor::DeleteSelectedLight()
{
    const SectorTopologyStaticPointLight* light = SelectedTopologyLight();
    const SectorTopologyStaticSpotLight* staticSpotLight = SelectedTopologyStaticSpotLight();
    const SectorTopologyDynamicPointLight* dynamicLight = SelectedTopologyDynamicLight();
    const SectorTopologyDynamicSpotLight* dynamicSpotLight = SelectedTopologyDynamicSpotLight();
    if (light == nullptr && staticSpotLight == nullptr && dynamicLight == nullptr && dynamicSpotLight == nullptr) {
        return false;
    }

    const int lightId = light != nullptr
            ? light->id
            : (staticSpotLight != nullptr
                    ? staticSpotLight->id
                    : (dynamicLight != nullptr ? dynamicLight->id : dynamicSpotLight->id));
    OpenConfirmation(
            "Delete Light",
            light != nullptr
                    ? TextFormat("Delete static light %d?", lightId)
                    : (staticSpotLight != nullptr
                            ? TextFormat("Delete static spot %d?", lightId)
                            : (dynamicLight != nullptr
                                    ? TextFormat("Delete dynamic light %d?", lightId)
                                    : TextFormat("Delete dynamic spot %d?", lightId))),
            [this,
             lightId,
             isStaticSpot = staticSpotLight != nullptr,
             isDynamic = dynamicLight != nullptr,
             isDynamicSpot = dynamicSpotLight != nullptr]() {
                if (isDynamicSpot) {
                    DeleteDynamicSpotLightById(lightId);
                } else if (isStaticSpot) {
                    DeleteStaticSpotLightById(lightId);
                } else if (isDynamic) {
                    DeleteDynamicLightById(lightId);
                } else {
                    DeleteLightById(lightId);
                }
            });
    return true;
}

bool SectorEditor::DeleteLightById(int topologyLightId)
{
    const bool hadLight = FindSectorTopologyStaticLight(state.topologyMap, topologyLightId) != nullptr;
    const SectorEditorTopologyActionResult result = DeleteStaticLight(state.topologyMap, topologyLightId);
    if (!result.changed) {
        if (!hadLight) {
            ClearStaleTopologySelection();
        }
        FinishTopologyActionResult(result);
        return false;
    }

    if (!hadLight) {
        ClearStaleTopologySelection();
        return false;
    }

    if (state.selectedTopologyLightId == topologyLightId) {
        ClearSelection();
    }
    if (state.hoveredTopologyLightId == topologyLightId) {
        state.hoveredTopologyLightId = -1;
    }
    if (state.lightDrag.topologyLightId == topologyLightId) {
        state.lightDrag = LightDragState{};
    }
    return FinishTopologyActionResult(result);
}

void SectorEditor::AddStaticLightAt(Vector2 mapPoint)
{
    const int sectorId = FindTopologySectorAt(mapPoint);
    const SectorEditorAddStaticLightResult result = AddStaticLightToSector(
            state.topologyMap,
            sectorId,
            mapPoint);
    if (!result.changed) {
        if (!result.status.empty()) {
            statusText = result.status;
        }
        return;
    }

    SelectTopologyLight(result.lightId);
    SectorEditorTopologyActionResult finish;
    finish.changed = true;
    finish.status = result.status;
    FinishTopologyActionResult(finish);
}

bool SectorEditor::DeleteStaticSpotLightById(int topologyLightId)
{
    const bool hadLight = FindSectorTopologyStaticSpotLight(state.topologyMap, topologyLightId) != nullptr;
    const SectorEditorTopologyActionResult result = DeleteStaticSpotLight(state.topologyMap, topologyLightId);
    if (!result.changed) {
        if (!hadLight) {
            ClearStaleTopologySelection();
        }
        FinishTopologyActionResult(result);
        return false;
    }

    if (!hadLight) {
        ClearStaleTopologySelection();
        return false;
    }

    if (state.selectedTopologyStaticSpotLightId == topologyLightId) {
        CancelSpotLightPilot(nullptr);
        ClearSelection();
    }
    if (state.hoveredTopologyStaticSpotLightId == topologyLightId) {
        state.hoveredTopologyStaticSpotLightId = -1;
    }
    if (state.lightDrag.topologyLightId == topologyLightId) {
        state.lightDrag = LightDragState{};
    }
    return FinishTopologyActionResult(result);
}

void SectorEditor::AddStaticSpotLightAt(Vector2 mapPoint)
{
    const int sectorId = FindTopologySectorAt(mapPoint);
    const SectorEditorAddStaticSpotLightResult result = AddStaticSpotLightToSector(
            state.topologyMap,
            sectorId,
            mapPoint);
    if (!result.changed) {
        if (!result.status.empty()) {
            statusText = result.status;
        }
        return;
    }

    SelectTopologyStaticSpotLight(result.lightId);
    SectorEditorTopologyActionResult finish;
    finish.changed = true;
    finish.status = result.status;
    FinishTopologyActionResult(finish);
}

bool SectorEditor::DeleteDynamicLightById(int topologyLightId)
{
    const bool hadLight = FindSectorTopologyDynamicLight(state.topologyMap, topologyLightId) != nullptr;
    const SectorEditorTopologyActionResult result = DeleteDynamicLight(state.topologyMap, topologyLightId);
    if (!result.changed) {
        if (!hadLight) {
            ClearStaleTopologySelection();
        }
        FinishTopologyActionResult(result);
        return false;
    }

    if (!hadLight) {
        ClearStaleTopologySelection();
        return false;
    }

    if (state.selectedTopologyDynamicLightId == topologyLightId) {
        ClearSelection();
    }
    if (state.hoveredTopologyDynamicLightId == topologyLightId) {
        state.hoveredTopologyDynamicLightId = -1;
    }
    if (state.lightDrag.topologyLightId == topologyLightId) {
        state.lightDrag = LightDragState{};
    }
    return FinishTopologyActionResult(result);
}

void SectorEditor::AddDynamicLightAt(Vector2 mapPoint)
{
    const int sectorId = FindTopologySectorAt(mapPoint);
    const SectorEditorAddDynamicLightResult result = AddDynamicLightToSector(
            state.topologyMap,
            sectorId,
            mapPoint);
    if (!result.changed) {
        if (!result.status.empty()) {
            statusText = result.status;
        }
        return;
    }

    SelectTopologyDynamicLight(result.lightId);
    SectorEditorTopologyActionResult finish;
    finish.changed = true;
    finish.status = result.status;
    FinishTopologyActionResult(finish);
}

bool SectorEditor::DeleteDynamicSpotLightById(int topologyLightId)
{
    const bool hadLight = FindSectorTopologyDynamicSpotLight(state.topologyMap, topologyLightId) != nullptr;
    const SectorEditorTopologyActionResult result = DeleteDynamicSpotLight(state.topologyMap, topologyLightId);
    if (!result.changed) {
        if (!hadLight) {
            ClearStaleTopologySelection();
        }
        FinishTopologyActionResult(result);
        return false;
    }

    if (!hadLight) {
        ClearStaleTopologySelection();
        return false;
    }

    if (state.selectedTopologyDynamicSpotLightId == topologyLightId) {
        CancelSpotLightPilot(nullptr);
        ClearSelection();
    }
    if (state.hoveredTopologyDynamicSpotLightId == topologyLightId) {
        state.hoveredTopologyDynamicSpotLightId = -1;
    }
    if (state.lightDrag.topologyLightId == topologyLightId) {
        state.lightDrag = LightDragState{};
    }
    return FinishTopologyActionResult(result);
}

void SectorEditor::AddDynamicSpotLightAt(Vector2 mapPoint)
{
    const int sectorId = FindTopologySectorAt(mapPoint);
    const SectorEditorAddDynamicSpotLightResult result = AddDynamicSpotLightToSector(
            state.topologyMap,
            sectorId,
            mapPoint);
    if (!result.changed) {
        if (!result.status.empty()) {
            statusText = result.status;
        }
        return;
    }

    SelectTopologyDynamicSpotLight(result.lightId);
    SectorEditorTopologyActionResult finish;
    finish.changed = true;
    finish.status = result.status;
    FinishTopologyActionResult(finish);
}

SectorEditorPlacedObjectDragContext SectorEditor::BuildRuntimeObjectDragContext()
{
    return SectorEditorPlacedObjectDragContext{
            state,
            statusText,
            [this](Vector2 screenPoint) {
                return ScreenToMap(screenPoint);
            },
            [this](Vector2 mapPoint) {
                return SnapMapPoint(mapPoint);
            },
            [this](int objectId) {
                SelectRuntimeObject(objectId);
            },
            [this](const SectorPlacedRuntimeObject& object) {
                UpdateCachedRuntimeObjectDraw(state.topologyRenderCache, object);
            },
            [this](const char* status) {
                MarkTopologyDocumentEdited(status);
            },
            [this]() {
                RefreshRuntimeObjectsAfterAuthoringEdit();
            }};
}

SectorEditorPlacedObjectActionContext SectorEditor::BuildRuntimeObjectActionContext()
{
    return SectorEditorPlacedObjectActionContext{
            state,
            statusText,
            engineContext,
            [this](Vector2 mapPoint) {
                return FindTopologySectorAt(mapPoint);
            },
            [this](
                    Vector2 screenPoint,
                    Vector2 mapPoint,
                    int& outLineDefId,
                    int& outSideDefId,
                    SectorTopologySideKind& outSide,
                    bool& outPreferredMissing) {
                return FindTopologyLineNearScreenPoint(
                        screenPoint,
                        mapPoint,
                        outLineDefId,
                        outSideDefId,
                        outSide,
                        outPreferredMissing);
            },
            [this](Vector2 screenPoint) {
                return ScreenToMap(screenPoint);
            },
            [this](int objectId) {
                SelectRuntimeObject(objectId);
            },
            [this]() {
                ClearSelection();
            },
            [this]() {
                ClearStaleTopologySelection();
            },
            [this](const char* status) {
                MarkTopologyDocumentEdited(status);
            }};
}

void SectorEditor::AddRuntimeObjectAt(Vector2 mapPoint)
{
    SectorEditorPlacedObjectActionContext actionContext = BuildRuntimeObjectActionContext();
    AddSectorEditorBillboard(actionContext, mapPoint);
}

void SectorEditor::AddDoorAtPortal(Vector2 screenPoint)
{
    SectorEditorPlacedObjectActionContext actionContext = BuildRuntimeObjectActionContext();
    AddSectorEditorDoor(actionContext, screenPoint);
}

bool SectorEditor::DeleteSelectedRuntimeObject()
{
    SectorEditorPlacedObjectActionContext actionContext = BuildRuntimeObjectActionContext();
    const SectorEditorPlacedObjectDeleteConfirmation confirmation =
            RequestDeleteSelectedSectorEditorPlacedObject(actionContext);
    if (!confirmation.requested) {
        return false;
    }

    const int objectId = confirmation.objectId;
    OpenConfirmation(
            confirmation.title.c_str(),
            confirmation.message.c_str(),
            [this, objectId]() {
                DeleteRuntimeObjectById(objectId);
            });
    return true;
}

bool SectorEditor::DeleteRuntimeObjectById(int objectId)
{
    SectorEditorPlacedObjectActionContext actionContext = BuildRuntimeObjectActionContext();
    return DeleteSectorEditorPlacedObjectById(actionContext, objectId);
}

bool SectorEditor::MutateSelectedRuntimeObject(
        const char* status,
        const std::function<bool(SectorPlacedRuntimeObject&)>& mutate)
{
    SectorEditorPlacedObjectActionContext actionContext = BuildRuntimeObjectActionContext();
    return MutateSelectedSectorEditorPlacedObject(actionContext, status, mutate);
}

void SectorEditor::RefreshRuntimeObjectsAfterAuthoringEdit()
{
    SectorEditorPlacedObjectActionContext actionContext = BuildRuntimeObjectActionContext();
    RefreshSectorEditorPlacedObjectsAfterAuthoringEdit(actionContext);
}

bool SectorEditor::BakeLightmaps()
{
    return StartLightmapBake();
}

bool SectorEditor::StartLightmapBake()
{
    if (lightmapBake.progress.running.load() || lightmapBake.worker.joinable() || lightmapBake.modalOpen) {
        statusText = "Lightmap bake already running";
        return false;
    }

    if (!state.hasCurrentLevelPath) {
        statusText = "Save the level before baking lightmaps";
        return false;
    }

    std::string gateMessage;
    if (!CanUseCurrentAuthoringDerivedTopologyForLightmapBake(state, &gateMessage)) {
        statusText = gateMessage.empty() ? "Bake failed: derived topology is not current" : gateMessage;
        return false;
    }

    if (state.topologyMap.sectors.empty()) {
        statusText = "Bake failed: no sectors";
        return false;
    }

    LevelPaths levelPaths;
    std::string pathError;
    if (!BuildLevelPaths(state.currentLevelName, levelPaths, pathError)) {
        statusText = TextFormat("Bake failed: %s", pathError.c_str());
        return false;
    }
    const std::string finalOutputPath = levelPaths.lightmapFilePath.string();
    const std::string temporaryOutputPath = MakeTemporaryLightmapPath(finalOutputPath);

    SectorTopologyLightmapBakeInput input;
    input.mapSnapshot = state.topologyMap;
    input.expectedSourceHash = ComputeSectorLightmapSourceHash(state.topologyMap);
    input.finalOutputPath = finalOutputPath;
    input.temporaryOutputPath = temporaryOutputPath;

    DeleteFileIfExists(temporaryOutputPath);
    DeleteFileIfExists(MakeSectorObjectProbeSidecarPathForLightmapPath(temporaryOutputPath));

    lightmapBake.progress.phase.store(SectorLightmapBakePhase::Preparing);
    lightmapBake.progress.completedWork.store(0);
    lightmapBake.progress.totalWork.store(1);
    lightmapBake.progress.cancelRequested.store(false);
    lightmapBake.progress.running.store(true);
    lightmapBake.modalOpen = true;
    lightmapBake.awaitingAcknowledgement = false;
    lightmapBake.cancelButtonPressed = false;
    lightmapBake.terminalMessage.clear();
    lightmapBake.terminalSuccess = false;
    lightmapBake.terminalCancelled = false;
    lightmapBake.temporaryOutputPath = temporaryOutputPath;
    lightmapBake.startTimeSeconds = GetTime();
    lightmapBake.completedTimeSeconds = 0.0;
    {
        std::lock_guard<std::mutex> lock(lightmapBake.resultMutex);
        lightmapBake.pendingResult.reset();
    }

    LightmapBakeProgress* progress = &lightmapBake.progress;
    std::mutex* resultMutex = &lightmapBake.resultMutex;
    std::optional<SectorLightmapBakeAsyncResult>* pendingResult = &lightmapBake.pendingResult;

    lightmapBake.worker = std::thread([input = std::move(input), progress, resultMutex, pendingResult]() mutable {
        SectorLightmapBakeAsyncResult asyncResult;
        asyncResult.expectedSourceHash = input.expectedSourceHash;
        asyncResult.sourceMapRevision = input.editorMapRevision;
        asyncResult.finalOutputPath = input.finalOutputPath;
        asyncResult.temporaryOutputPath = input.temporaryOutputPath;

        SectorLightmapBakeCallbacks callbacks;
        callbacks.onProgress = [progress](SectorLightmapBakePhase phase, uint32_t completedWork, uint32_t totalWork) {
            progress->phase.store(phase);
            progress->completedWork.store(completedWork);
            progress->totalWork.store(totalWork);
        };
        callbacks.isCancellationRequested = [progress]() {
            return progress->cancelRequested.load();
        };

        std::string error;
        const bool succeeded = BakeSectorLightmap(input, callbacks, asyncResult.bakeResult, error);
        asyncResult.cancelled = !succeeded && progress->cancelRequested.load();
        asyncResult.succeeded = succeeded && !asyncResult.cancelled;
        asyncResult.errorMessage = error.empty()
                ? (asyncResult.cancelled ? "Bake cancelled" : "Bake failed")
                : error;
        if (asyncResult.succeeded) {
            asyncResult.bakeReportText = FormatSectorLightmapBakeReport(asyncResult.bakeResult);
        }

        {
            std::lock_guard<std::mutex> lock(*resultMutex);
            *pendingResult = std::move(asyncResult);
        }

        if (progress->cancelRequested.load()) {
            progress->phase.store(SectorLightmapBakePhase::Cancelled);
        } else if (succeeded) {
            progress->phase.store(SectorLightmapBakePhase::Completed);
        } else {
            progress->phase.store(SectorLightmapBakePhase::Failed);
        }
        progress->completedWork.store(1);
        progress->totalWork.store(1);
        progress->running.store(false);
    });

    statusText = "Baking lightmap...";
    return true;
}

void SectorEditor::PollLightmapBakeResult(engine::AssetManager& assets)
{
    std::optional<SectorLightmapBakeAsyncResult> pending;
    {
        std::lock_guard<std::mutex> lock(lightmapBake.resultMutex);
        if (lightmapBake.pendingResult.has_value()) {
            pending = std::move(lightmapBake.pendingResult);
            lightmapBake.pendingResult.reset();
        }
    }

    if (!pending.has_value()) {
        return;
    }

    JoinLightmapBakeWorker();
    lightmapBake.completedTimeSeconds = GetTime();
    ConsumeLightmapBakeResult(*pending, assets);
}

void SectorEditor::RequestLightmapBakeCancel()
{
    if (!lightmapBake.progress.running.load()) {
        return;
    }
    lightmapBake.progress.cancelRequested.store(true);
    lightmapBake.cancelButtonPressed = true;
    statusText = "Cancelling bake...";
}

void SectorEditor::JoinLightmapBakeWorker()
{
    if (lightmapBake.worker.joinable()) {
        lightmapBake.worker.join();
    }
}

void SectorEditor::ShutdownLightmapBake()
{
    if (lightmapBake.progress.running.load()) {
        lightmapBake.progress.cancelRequested.store(true);
    }
    JoinLightmapBakeWorker();
    DeleteFileIfExists(lightmapBake.temporaryOutputPath);
    DeleteFileIfExists(MakeSectorObjectProbeSidecarPathForLightmapPath(lightmapBake.temporaryOutputPath));
    lightmapBake.temporaryOutputPath.clear();
    {
        std::lock_guard<std::mutex> lock(lightmapBake.resultMutex);
        if (lightmapBake.pendingResult.has_value()) {
            DeleteFileIfExists(lightmapBake.pendingResult->temporaryOutputPath);
            DeleteFileIfExists(MakeSectorObjectProbeSidecarPathForLightmapPath(
                    lightmapBake.pendingResult->temporaryOutputPath));
            lightmapBake.pendingResult.reset();
        }
    }
    lightmapBake.modalOpen = false;
    lightmapBake.awaitingAcknowledgement = false;
    lightmapBake.progress.running.store(false);
    lightmapBake.progress.cancelRequested.store(false);
    lightmapBake.progress.phase.store(SectorLightmapBakePhase::Idle);
}

bool SectorEditor::IsLightmapBakeBlocking() const
{
    return lightmapBake.modalOpen || lightmapBake.progress.running.load();
}

bool SectorEditor::ConsumeLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets)
{
    lightmapBake.progress.phase.store(result.cancelled
            ? SectorLightmapBakePhase::Cancelled
            : (result.succeeded ? SectorLightmapBakePhase::InstallingResult : SectorLightmapBakePhase::Failed));

    if (result.cancelled) {
        DeleteFileIfExists(result.temporaryOutputPath);
        DeleteFileIfExists(result.bakeResult.objectProbes.path);
        lightmapBake.terminalMessage = "Lightmap bake cancelled";
        lightmapBake.terminalCancelled = true;
        lightmapBake.awaitingAcknowledgement = true;
        statusText = lightmapBake.terminalMessage;
        return false;
    }

    if (!result.succeeded) {
        DeleteFileIfExists(result.temporaryOutputPath);
        DeleteFileIfExists(result.bakeResult.objectProbes.path);
        lightmapBake.terminalMessage = result.errorMessage.empty() ? "Bake failed" : result.errorMessage;
        lightmapBake.terminalSuccess = false;
        lightmapBake.awaitingAcknowledgement = true;
        statusText = lightmapBake.terminalMessage;
        TraceLog(LOG_WARNING, "%s", lightmapBake.terminalMessage.c_str());
        return false;
    }

    const bool installed = InstallLightmapBakeResult(result, assets);
    lightmapBake.modalOpen = false;
    lightmapBake.awaitingAcknowledgement = false;
    lightmapBake.cancelButtonPressed = false;
    lightmapBake.terminalSuccess = installed;
    lightmapBake.terminalCancelled = false;
    lightmapBake.temporaryOutputPath.clear();
    lightmapBake.progress.phase.store(installed ? SectorLightmapBakePhase::Completed : SectorLightmapBakePhase::Failed);
    return installed;
}

bool SectorEditor::InstallLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets)
{
    const std::string temporaryObjectProbePath = result.bakeResult.objectProbes.path.empty()
            ? MakeSectorObjectProbeSidecarPathForLightmapPath(result.temporaryOutputPath)
            : result.bakeResult.objectProbes.path;

    if (ComputeSectorLightmapSourceHash(state.topologyMap) != result.expectedSourceHash) {
        DeleteFileIfExists(result.temporaryOutputPath);
        DeleteFileIfExists(temporaryObjectProbePath);
        statusText = "Bake discarded: document changed during bake";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(result.temporaryOutputPath, ec) || ec) {
        DeleteFileIfExists(result.temporaryOutputPath);
        DeleteFileIfExists(temporaryObjectProbePath);
        statusText = "Bake failed: temporary lightmap output missing";
        return false;
    }
    if (!std::filesystem::exists(temporaryObjectProbePath, ec) || ec) {
        DeleteFileIfExists(result.temporaryOutputPath);
        DeleteFileIfExists(temporaryObjectProbePath);
        statusText = "Bake failed: temporary object probe output missing";
        return false;
    }

    const std::filesystem::path finalPath(result.finalOutputPath);
    if (!finalPath.parent_path().empty()) {
        std::filesystem::create_directories(finalPath.parent_path(), ec);
        if (ec) {
            DeleteFileIfExists(result.temporaryOutputPath);
            DeleteFileIfExists(temporaryObjectProbePath);
            statusText = TextFormat("Bake failed: could not create output directory: %s", ec.message().c_str());
            return false;
        }
    }

    const std::string finalObjectProbePath = MakeSectorObjectProbeSidecarPathForLightmapPath(result.finalOutputPath);
    std::filesystem::copy_file(
            temporaryObjectProbePath,
            finalObjectProbePath,
            std::filesystem::copy_options::overwrite_existing,
            ec
    );
    if (ec) {
        DeleteFileIfExists(result.temporaryOutputPath);
        DeleteFileIfExists(temporaryObjectProbePath);
        statusText = TextFormat("Bake failed: could not install object probe sidecar: %s", ec.message().c_str());
        return false;
    }
    std::filesystem::copy_file(
            result.temporaryOutputPath,
            result.finalOutputPath,
            std::filesystem::copy_options::overwrite_existing,
            ec
    );
    if (ec) {
        DeleteFileIfExists(result.temporaryOutputPath);
        DeleteFileIfExists(temporaryObjectProbePath);
        DeleteFileIfExists(finalObjectProbePath);
        statusText = TextFormat("Bake failed: could not install lightmap: %s", ec.message().c_str());
        return false;
    }
    DeleteFileIfExists(result.temporaryOutputPath);
    DeleteFileIfExists(temporaryObjectProbePath);

    LevelPaths levelPaths;
    std::string pathError;
    if (!BuildLevelPaths(state.currentLevelName, levelPaths, pathError)) {
        DeleteFileIfExists(result.temporaryOutputPath);
        DeleteFileIfExists(temporaryObjectProbePath);
        statusText = TextFormat("Bake failed: %s", pathError.c_str());
        return false;
    }
    state.topologyMap.bakedLightmap.path = levelPaths.lightmapAssetPath;
    state.topologyMap.bakedLightmap.width = result.bakeResult.width;
    state.topologyMap.bakedLightmap.height = result.bakeResult.height;
    state.topologyMap.bakedLightmap.sourceHash = result.bakeResult.sourceHash;
    state.topologyMap.bakedLightmap.objectProbes = result.bakeResult.objectProbes;
    state.topologyMap.bakedLightmap.objectProbes.path =
            MakeSectorAssetRelativePath(finalObjectProbePath);
    state.topologyMap.bakedLightmap.objectProbes.sourceHash = result.bakeResult.sourceHash;
    state.hasUnsavedChanges = true;
    state.topologyDocumentDirty = true;

    std::istringstream report(result.bakeReportText);
    std::string line;
    while (std::getline(report, line)) {
        TraceLog(LOG_INFO, "%s", line.c_str());
    }
    TraceLog(LOG_INFO, "INFO: Lightmap bake completed asynchronously in %.2fs", result.bakeResult.totalBakeSeconds);

    if (state.mode == SectorEditorMode::Preview3D && preview.IsRendererReady()) {
        if (engineContext != nullptr) {
            RebuildPreviewMeshesPreservingView(*engineContext);
        }
    }

    statusText = TextFormat("Baked lightmap in %.1fs", result.bakeResult.totalBakeSeconds);
    return true;
}

bool SectorEditor::FindTopologyVertexNearScreenPoint(
        Vector2 screenPoint,
        int& outVertexId,
        SectorTopologyCoordPoint& outPoint) const
{
    float bestDistance2 = ScreenVertexSnapPixels * ScreenVertexSnapPixels;
    int bestVertexId = -1;
    SectorTopologyCoordPoint bestPoint{};

    for (const SectorTopologyVertex& vertex : state.topologyMap.vertices) {
        const Vector2 screenVertex = MapToScreen(SectorTopologyVertexToMap(vertex));
        const float dx = screenVertex.x - screenPoint.x;
        const float dy = screenVertex.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > bestDistance2) {
            continue;
        }
        if (bestVertexId >= 0
                && std::fabs(distance2 - bestDistance2) <= 0.001f
                && vertex.id >= bestVertexId) {
            continue;
        }
        bestDistance2 = distance2;
        bestVertexId = vertex.id;
        bestPoint = SectorTopologyCoordPoint{vertex.x, vertex.y};
    }

    if (bestVertexId < 0) {
        outVertexId = -1;
        outPoint = SectorTopologyCoordPoint{};
        return false;
    }

    outVertexId = bestVertexId;
    outPoint = bestPoint;
    return true;
}

bool SectorEditor::SnapAuthoringVertexMoveTarget(
        Vector2 mapPoint,
        SectorTopologyCoordPoint& outPoint,
        std::string& error) const
{
    const float grid = static_cast<float>(std::max(1, state.gridSize));
    const Vector2 snapped{
            std::round(mapPoint.x / grid) * grid,
            std::round(mapPoint.y / grid) * grid
    };

    SectorCoord x = 0;
    SectorCoord y = 0;
    if (!VisibleAuthoringToSectorCoord(snapped.x, x)
            || !VisibleAuthoringToSectorCoord(snapped.y, y)) {
        error = "Move target is outside authoring coordinate range";
        outPoint = SectorTopologyCoordPoint{};
        return false;
    }

    outPoint = SectorTopologyCoordPoint{x, y};
    error.clear();
    return true;
}

void SectorEditor::RenderPreview3D(engine::AssetManager& assets)
{
    RenderPreview3DShadowMaps(assets);
    if (engineContext != nullptr) {
        RenderPreview3DScene(*engineContext);
    }
    RenderPreview3DOverlays();
}

void SectorEditor::RenderPreview3DShadowMaps(engine::AssetManager& assets)
{
    if (state.mode != SectorEditorMode::Preview3D) {
        return;
    }
    preview.RenderDynamicSpotLightShadowMaps(
            assets,
            engineContext != nullptr ? &engineContext->world : nullptr);
}

void SectorEditor::RenderPreview3DScene(engine::EngineContext& context)
{
    preview.DrawScene(
            context.assets,
            state.useBakedAmbientOcclusion,
            &context.world,
            SectorRuntimeDoorLightingContext{&state.runtimeObjects.objectLightProbes, &state.topologyMap});
}

void SectorEditor::ApplyPreview3DBloom(engine::AssetManager& assets, RenderTexture2D& sceneTarget)
{
    if (state.mode != SectorEditorMode::Preview3D) {
        return;
    }
    preview.ApplyEmissiveDecalBloomToScene(assets, sceneTarget);
}

void SectorEditor::RenderPreview3DOverlays()
{
    if (!state.previewUiHidden) {
        DrawPreviewSurfaceHighlights();
        DrawPreviewSpotLightOverlay();
        DrawPreviewObjectProbeOverlay();
    }
}

SectorSurfaceHit SectorEditor::PickSectorSurface3D(Vector2 mousePosition, Rectangle viewportRect) const
{
    SectorSurfaceHit best;
    if (!preview.IsRendererReady()) {
        return best;
    }

    const Vector2 localMouse{
            mousePosition.x - viewportRect.x,
            mousePosition.y - viewportRect.y
    };
    const Ray ray = GetScreenToWorldRayEx(
            localMouse,
            preview.RenderCamera(),
            static_cast<int>(std::round(viewportRect.width)),
            static_cast<int>(std::round(viewportRect.height))
    );

    const SectorGeneratedSurfaceHit hit = PickSectorGeneratedGeometry(
            preview.RenderedGeometry(),
            ray,
            preview.VisibilityResult(),
            GeometryEpsilon);
    if (!hit.hit) {
        return best;
    }

    best.hit = true;
    best.surface = ToEditorSurfaceRef(hit.ref);
    best.worldPosition = hit.worldPosition;
    best.distance = hit.distance;
    return best;
}

void SectorEditor::DrawPreviewSurfaceHighlights() const
{
    if (!preview.IsRendererReady() || state.freeflyController.mouseLookEnabled) {
        return;
    }

    auto drawSurface = [this](SectorSurfaceRef surface, Color color, float thickness) {
        if (!IsValidSurfaceRef(surface)) {
            return;
        }
        const float lift = IsWallSurface(surface.kind) ? PreviewHighlightLift : PreviewHighlightLift * 2.0f;
        for (const SectorGeneratedSurface& generated : preview.RenderedGeometry().surfaces) {
            if (!ShouldIncludeSectorGeneratedSurfaceForVisibility(generated, preview.VisibilityResult())) {
                continue;
            }
            const SectorSurfaceRef generatedRef = ToEditorSurfaceRef(generated.ref);
            if (!SameSurfaceRef(surface, generatedRef)) {
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
            && !SameSurfaceRef(state.hoveredSurface3D.surface, state.selectedSurface3D)) {
        drawSurface(state.hoveredSurface3D.surface, Color{248, 238, 124, 235}, 2.0f);
    }
    if (state.selectedSurface3D.kind != SectorSurfaceKind::None) {
        drawSurface(state.selectedSurface3D, Color{84, 204, 255, 255}, 3.0f);
    }
    EndMode3D();
}

void SectorEditor::DrawPreviewSpotLightOverlay() const
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
    if (const SectorTopologyStaticSpotLight* light = SelectedTopologyStaticSpotLight()) {
        lightPosition = light->position;
        lightTarget = light->target;
        lightRange = light->range;
        innerConeDegrees = light->innerConeDegrees;
        outerConeDegrees = light->outerConeDegrees;
        selectedStaticSpotLight = true;
    } else if (const SectorTopologyDynamicSpotLight* light = SelectedTopologyDynamicSpotLight()) {
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
    DrawSpotLightConeRing(
            origin,
            forward,
            right,
            up,
            range,
            outerConeDegrees,
            outerConeColor,
            true);
    DrawSpotLightConeRing(
            origin,
            forward,
            right,
            up,
            range,
            innerConeDegrees,
            innerConeColor,
            false);
    EndMode3D();
}

void SectorEditor::DrawPreviewObjectProbeOverlay() const
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

void SectorEditor::RefreshPreviewObjectProbeDebugData()
{
    RefreshSectorRuntimeObjectMapData(state.runtimeObjects, state.topologyMap);
}

bool SectorEditor::IsPreviewOverlayMouseInteractive() const
{
    return !state.freeflyController.mouseLookEnabled;
}

Rectangle SectorEditor::BuildPreviewOverlayInteractionRect() const
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
            state.activePreviewDebugOverlayTab == PreviewDebugOverlayTab::None
                    ? collapsedHeight
                    : expandedHeight};
}

void SectorEditor::DrawPreviewOverlay(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    struct OverlayLine {
        std::string text;
        Color color;
        bool wrap;
    };

    const bool mouseInteractive = IsPreviewOverlayMouseInteractive();
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

    const SectorViewPose pose = ActivePreviewPose();
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
                if (state.spotLightPilot.active) {
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

    const bool hasSelectedSpotLight = SelectedTopologyStaticSpotLight() != nullptr
            || SelectedTopologyDynamicSpotLight() != nullptr;
    engine::Text(
            smallConfig,
            assets,
            Rectangle{
                    panel.x + padding,
                    panel.y + padding,
                    mouseInteractive && (state.spotLightPilot.active
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
        if (state.spotLightPilot.active) {
            if (engine::Button(
                        ui,
                        smallConfig,
                        input,
                        assets,
                        "sector_editor_preview_spotlight_pilot_cancel",
                        Rectangle{actionsRight - 72.0f, actionY, 72.0f, 28.0f},
                        smallFont,
                        "Cancel")) {
                CancelSpotLightPilot("Spotlight pilot cancelled");
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
                ApplySpotLightPilot();
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
                StartSpotLightPilot();
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
                MarkTopologyDocumentEdited("Object probe debug draw distance updated");
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
                OpenPreviewSettingsModal();
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
}

Rectangle SectorEditor::BuildPreviewUvPanelRect() const
{
    return BuildSectorEditorPreviewUvPanelRect();
}

void SectorEditor::DrawPreviewUvPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    if (state.freeflyController.mouseLookEnabled) {
        return;
    }

    if (!IsValidSurfaceRef(state.selectedSurface3D)
            || !IsValidTopologySurfaceEditTarget(state.selectedTopologySurface3D)) {
        state.selectedSurface3D = SectorSurfaceRef{};
        state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
        return;
    }
    if (!EnsureSelectedSurface3DAuthoringMappingCurrent()) {
        return;
    }

    SectorEditorMaterialEditingService materialEditing = BuildMaterialEditingService();
    SectorEditorPreviewUvPanelContext panelContext{
            ui,
            config,
            input,
            assets,
            font,
            engine::FontHandle{},
            BuildPreviewUvPanelRect(),
            state,
            uiState,
            statusText,
            materialEditing,
            [this](int lineDefId, bool blocksPlayer) {
                return SetAuthoringLineDefBlocksPlayer(lineDefId, blocksPlayer);
            }};
    DrawSectorEditorPreviewUvPanel(panelContext);
}

void SectorEditor::DrawGrid() const
{
    const int grid = std::max(1, state.gridSize);
    const Vector2 minMap = ScreenToMap(Vector2{canvasRect.x, canvasRect.y});
    const Vector2 maxMap = ScreenToMap(Vector2{canvasRect.x + canvasRect.width, canvasRect.y + canvasRect.height});
    const int startX = static_cast<int>(std::floor(minMap.x / static_cast<float>(grid))) * grid;
    const int endX = static_cast<int>(std::ceil(maxMap.x / static_cast<float>(grid))) * grid;
    const int startY = static_cast<int>(std::floor(minMap.y / static_cast<float>(grid))) * grid;
    const int endY = static_cast<int>(std::ceil(maxMap.y / static_cast<float>(grid))) * grid;

    const Color gridColor{44, 50, 62, 155};
    const Color majorColor{62, 70, 86, 185};
    const Color axisColor{112, 148, 148, 230};

    for (int x = startX; x <= endX; x += grid) {
        const Vector2 a = MapToScreen(Vector2{static_cast<float>(x), minMap.y});
        const Vector2 b = MapToScreen(Vector2{static_cast<float>(x), maxMap.y});
        const bool axis = x == 0;
        const bool major = grid < 8 && x % 8 == 0;
        DrawLineEx(a, b, axis && state.showAxes ? 2.0f : 1.0f, axis && state.showAxes ? axisColor : (major ? majorColor : gridColor));
    }

    for (int y = startY; y <= endY; y += grid) {
        const Vector2 a = MapToScreen(Vector2{minMap.x, static_cast<float>(y)});
        const Vector2 b = MapToScreen(Vector2{maxMap.x, static_cast<float>(y)});
        const bool axis = y == 0;
        const bool major = grid < 8 && y % 8 == 0;
        DrawLineEx(a, b, axis && state.showAxes ? 2.0f : 1.0f, axis && state.showAxes ? axisColor : (major ? majorColor : gridColor));
    }

    if (state.showAxes) {
        const Vector2 origin = MapToScreen(Vector2{0.0f, 0.0f});
        DrawCircleV(origin, 5.0f, Color{180, 210, 190, 255});
    }
}

void SectorEditor::InvalidateTopologyRenderCache()
{
    ++state.topologyRenderRevision;
    state.topologyRenderCache.valid = false;
}

void SectorEditor::EnsureTopologyRenderCache()
{
    if (!state.topologyRenderCache.valid
            || state.topologyRenderCache.revision != state.topologyRenderRevision) {
        state.topologyRenderCache = BuildSectorEditorTopologyRenderCache(
                state.topologyMap,
                state.authoringGraph,
                state.authoringDerivation,
                state.topologyRenderRevision);
        state.topologyRenderWarning = state.topologyRenderCache.warning;
    }
}

void SectorEditor::DrawTopologyDocument()
{
    if (!initialized) {
        DrawText("Topology map failed to load", static_cast<int>(canvasRect.x + 24.0f), static_cast<int>(canvasRect.y + 24.0f), 28, RED);
        return;
    }

    EnsureTopologyRenderCache();

    ClearStaleTopologySelection();
    const bool hasAuthoringGraph = HasAuthoringGraphData();
    const bool drawLegacyTopologySelection =
            ShouldDrawLegacyTopologySelectionHighlight(hasAuthoringGraph, state.topologySelectionKind);
    const SectorEditorTopologyDrawContext drawContext{
            canvasRect,
            state.viewCenter,
            state.viewZoom,
            state.showSectorIds,
            state.authoringDerivedTopologyStale,
            state.currentTool,
            drawLegacyTopologySelection ? state.topologySelectionKind : TopologySelectionKind::None,
            drawLegacyTopologySelection ? state.selectedTopologySectorId : -1,
            drawLegacyTopologySelection ? state.selectedTopologyVertexId : -1,
            drawLegacyTopologySelection ? state.selectedTopologyLightId : -1,
            drawLegacyTopologySelection ? state.selectedTopologyStaticSpotLightId : -1,
            drawLegacyTopologySelection ? state.selectedTopologyDynamicLightId : -1,
            drawLegacyTopologySelection ? state.selectedTopologyDynamicSpotLightId : -1,
            state.selectedRuntimeObjectId,
            state.hasHoveredVertex,
            state.hoveredTopologyVertexId,
            state.hoveredTopologyLightId,
            state.hoveredTopologyStaticSpotLightId,
            state.hoveredTopologyDynamicLightId,
            state.hoveredTopologyDynamicSpotLightId,
            state.selectedAuthoring,
            state.hoveredAuthoring
    };
    DrawCachedTopologySectors(state.topologyRenderCache, drawContext);

    if (drawLegacyTopologySelection) {
        DrawTopologySelectedLineHighlight();
    }
    DrawCachedTopologyLineDefs(state.topologyRenderCache, drawContext);
    DrawCachedTopologyVertices(state.topologyRenderCache, drawContext);
    DrawCachedAuthoringGraphOverlay(state.topologyRenderCache, drawContext);
    DrawCachedAuthoringDiagnostics(state.topologyRenderCache, drawContext);
    DrawAuthoringVertexMoveOverlay();
    DrawCachedTopologyStaticLights(state.topologyRenderCache, drawContext);
    DrawCachedTopologyStaticSpotLights(state.topologyRenderCache, drawContext);
    DrawCachedTopologyDynamicLights(state.topologyRenderCache, drawContext);
    DrawCachedTopologyDynamicSpotLights(state.topologyRenderCache, drawContext);
    DrawCachedRuntimeObjects(state.topologyRenderCache, drawContext);
    DrawLightMoveOverlay();
    const auto drawToolOverlay = [this](SectorEditorTool tool) {
        if (const SectorEditorToolModule* lineModule = FindSectorEditorToolModule(tool)) {
            if (lineModule->drawCanvasOverlay == nullptr) {
                return;
            }
            SectorEditorToolContext toolContext = BuildToolContext(nullptr);
            lineModule->drawCanvasOverlay(toolContext);
        }
    };
    drawToolOverlay(SectorEditorTool::AuthoringLine);
    drawToolOverlay(SectorEditorTool::AuthoringRectangle);
    drawToolOverlay(SectorEditorTool::AuthoringInsertVertex);
    DrawTopologySnapCrosshair();

    if (!state.topologyRenderWarning.empty()) {
        DrawText(
                state.topologyRenderWarning.c_str(),
                static_cast<int>(canvasRect.x + 16.0f),
                static_cast<int>(canvasRect.y + 14.0f),
                18,
                Color{236, 196, 92, 255}
        );
    }
    if (state.authoringDerivedTopologyStale) {
        DrawText(
                "Authoring graph changed; derived sector fills are stale",
                static_cast<int>(canvasRect.x + 16.0f),
                static_cast<int>(canvasRect.y + 64.0f),
                18,
                Color{236, 196, 92, 255}
        );
    }
}

void SectorEditor::DrawTopologySelectedLineHighlight() const
{
    if (state.topologySelectionKind != TopologySelectionKind::SideDef
            && state.topologySelectionKind != TopologySelectionKind::LineDef) {
        return;
    }

    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        return;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)) {
        return;
    }

    Vector2 a = MapToScreen(SectorTopologyVertexToMap(*start));
    Vector2 b = MapToScreen(SectorTopologyVertexToMap(*end));
    Vector2 dir{b.x - a.x, b.y - a.y};
    const float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (length <= GeometryEpsilon) {
        return;
    }

    dir.x /= length;
    dir.y /= length;
    Vector2 normal{-dir.y, dir.x};
    Color color{72, 210, 246, 138};
    if (state.topologySelectionKind == TopologySelectionKind::LineDef) {
        normal = Vector2{0.0f, 0.0f};
        color = Color{210, 214, 224, 125};
    } else if (state.selectedTopologySideKind == SectorTopologySideKind::Back) {
        normal.x = -normal.x;
        normal.y = -normal.y;
        color = Color{94, 238, 186, 132};
    }

    const float offset = state.topologySelectionKind == TopologySelectionKind::LineDef ? 0.0f : 7.0f;
    a.x += normal.x * offset;
    a.y += normal.y * offset;
    b.x += normal.x * offset;
    b.y += normal.y * offset;
    DrawLineEx(a, b, 10.0f, color);
}

void SectorEditor::DrawTopologySnapCrosshair() const
{
    if (!Contains(canvasRect, GetMousePosition())) {
        return;
    }

    if ((state.currentTool == SectorEditorTool::AuthoringInsertVertex
                || state.pendingAuthoringInsertVertex.active)
            && state.pendingAuthoringInsertVertex.hasPreviewPoint) {
        return;
    }

    const bool useCanonicalSectorPoint = state.currentTool == SectorEditorTool::AuthoringLine
            || state.currentTool == SectorEditorTool::AuthoringRectangle
            || state.pendingAuthoringLine.active
            || state.pendingAuthoringRectangle.active;
    const Vector2 snap = useCanonicalSectorPoint
            ? MapToScreen(SectorPointToVector2(CurrentSnappedSectorPoint()))
            : MapToScreen(state.snappedMouseMap);
    DrawLineEx(Vector2{snap.x - 9.0f, snap.y}, Vector2{snap.x + 9.0f, snap.y}, 2.0f, Color{235, 224, 130, 255});
    DrawLineEx(Vector2{snap.x, snap.y - 9.0f}, Vector2{snap.x, snap.y + 9.0f}, 2.0f, Color{235, 224, 130, 255});
}

void SectorEditor::DrawAuthoringVertexMoveOverlay() const
{
    if (state.currentTool != SectorEditorTool::AuthoringMove
            && !state.authoringVertexDrag.active) {
        return;
    }

    if (!state.authoringVertexDrag.active
            && state.hoveredAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
        const SectorAuthoringVertex* vertex =
                FindSectorAuthoringVertex(state.authoringGraph, state.hoveredAuthoring.vertexId);
        if (vertex == nullptr) {
            return;
        }

        const Vector2 point = MapToScreen(Vector2{
                SectorCoordToVisibleAuthoring(vertex->x),
                SectorCoordToVisibleAuthoring(vertex->y)});
        DrawCircleLines(
                static_cast<int>(std::round(point.x)),
                static_cast<int>(std::round(point.y)),
                12.0f,
                Color{122, 220, 244, 255});
        DrawCircleV(point, 4.5f, Color{122, 220, 244, 255});
        return;
    }

    if (!state.authoringVertexDrag.active) {
        return;
    }

    const bool invalid = !state.authoringVertexDrag.errorMessage.empty()
            || !state.authoringVertexDrag.hasPreviewPoint;
    const Color targetColor = invalid ? Color{230, 82, 82, 255} : Color{120, 230, 154, 255};
    const Color previewColor = invalid ? Color{230, 82, 82, 205} : Color{122, 220, 244, 220};
    const Color originalColor = Color{245, 226, 154, 230};
    const Vector2 original = MapToScreen(Vector2{
            SectorCoordToVisibleAuthoring(state.authoringVertexDrag.originalPoint.x),
            SectorCoordToVisibleAuthoring(state.authoringVertexDrag.originalPoint.y)});

    if (!state.authoringVertexDrag.hasPreviewPoint) {
        DrawCircleLines(
                static_cast<int>(std::round(original.x)),
                static_cast<int>(std::round(original.y)),
                10.0f,
                originalColor);
        return;
    }

    const int draggedVertexId = state.authoringVertexDrag.vertexId;
    const Vector2 previewMap{
            SectorCoordToVisibleAuthoring(state.authoringVertexDrag.previewPoint.x),
            SectorCoordToVisibleAuthoring(state.authoringVertexDrag.previewPoint.y)};
    for (const SectorAuthoringLine& line : state.authoringGraph.lines) {
        if (line.startVertexId != draggedVertexId && line.endVertexId != draggedVertexId) {
            continue;
        }

        const int otherVertexId = line.startVertexId == draggedVertexId
                ? line.endVertexId
                : line.startVertexId;
        const SectorAuthoringVertex* otherVertex =
                FindSectorAuthoringVertex(state.authoringGraph, otherVertexId);
        if (otherVertex == nullptr) {
            continue;
        }

        DrawLineEx(
                MapToScreen(previewMap),
                MapToScreen(Vector2{
                        SectorCoordToVisibleAuthoring(otherVertex->x),
                        SectorCoordToVisibleAuthoring(otherVertex->y)}),
                4.0f,
                previewColor);
    }

    const Vector2 target = MapToScreen(previewMap);
    DrawLineEx(original, target, 2.0f, WithAlpha(targetColor, 180));
    DrawCircleLines(
            static_cast<int>(std::round(original.x)),
            static_cast<int>(std::round(original.y)),
            10.0f,
            originalColor);
    DrawCircleLines(
            static_cast<int>(std::round(target.x)),
            static_cast<int>(std::round(target.y)),
            13.0f,
            targetColor);
    DrawCircleV(target, 5.0f, targetColor);
}

void SectorEditor::DrawLightMoveOverlay() const
{
    if (state.currentTool != SectorEditorTool::Select
            && state.currentTool != SectorEditorTool::StaticLight
            && state.currentTool != SectorEditorTool::StaticSpotLight
            && state.currentTool != SectorEditorTool::DynamicLight
            && state.currentTool != SectorEditorTool::DynamicSpotLight
            && state.currentTool != SectorEditorTool::Move) {
        return;
    }

    if (state.lightDrag.active) {
        if (state.topologySelectionKind == TopologySelectionKind::StaticSpotLight) {
            const SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(
                    state.topologyMap,
                    state.lightDrag.topologyLightId);
            if (light == nullptr) {
                return;
            }

            const Vector2 origin = MapToScreen(Vector2{light->position.x, light->position.z});
            const Vector2 target = MapToScreen(Vector2{light->target.x, light->target.z});
            Color color = light->color;
            color.a = 245;
            DrawLineEx(origin, target, 3.0f, WithAlpha(color, 220));
            DrawCircleLines(
                    static_cast<int>(std::round(origin.x)),
                    static_cast<int>(std::round(origin.y)),
                    state.lightDrag.spotHandle == SpotLightHandle::Origin ? 15.0f : 11.0f,
                    Color{120, 230, 154, 255});
            DrawCircleV(origin, 6.5f, Color{120, 230, 154, 255});
            DrawCircleLines(
                    static_cast<int>(std::round(target.x)),
                    static_cast<int>(std::round(target.y)),
                    state.lightDrag.spotHandle == SpotLightHandle::Target ? 15.0f : 10.0f,
                    Color{122, 220, 244, 255});
            DrawCircleV(target, 5.0f, Color{122, 220, 244, 255});
            return;
        }

        if (state.topologySelectionKind == TopologySelectionKind::DynamicSpotLight) {
            const SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(
                    state.topologyMap,
                    state.lightDrag.topologyLightId);
            if (light == nullptr) {
                return;
            }

            const Vector2 origin = MapToScreen(Vector2{light->position.x, light->position.z});
            const Vector2 target = MapToScreen(Vector2{light->target.x, light->target.z});
            Color color = light->enabled ? light->color : Color{120, 128, 140, 255};
            color.a = 245;
            DrawLineEx(origin, target, 3.0f, WithAlpha(color, 220));
            DrawCircleLines(
                    static_cast<int>(std::round(origin.x)),
                    static_cast<int>(std::round(origin.y)),
                    state.lightDrag.spotHandle == SpotLightHandle::Origin ? 15.0f : 11.0f,
                    Color{120, 230, 154, 255});
            DrawCircleV(origin, 6.5f, Color{120, 230, 154, 255});
            DrawCircleLines(
                    static_cast<int>(std::round(target.x)),
                    static_cast<int>(std::round(target.y)),
                    state.lightDrag.spotHandle == SpotLightHandle::Target ? 15.0f : 10.0f,
                    Color{122, 220, 244, 255});
            DrawCircleV(target, 5.0f, Color{122, 220, 244, 255});
            return;
        }

        if (state.topologySelectionKind == TopologySelectionKind::DynamicLight) {
            const SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                    state.topologyMap,
                    state.lightDrag.topologyLightId);
            if (light == nullptr) {
                return;
            }

            const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
            const float radiusPixels = SectorAuthoringToWorldDistance(light->radius) * state.viewZoom;
            Color color = light->color;
            color.a = 245;
            DrawCircleLines(
                    static_cast<int>(std::round(center.x)),
                    static_cast<int>(std::round(center.y)),
                    radiusPixels,
                    WithAlpha(color, 165)
            );
            DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 15.0f, Color{120, 230, 154, 255});
            DrawCircleV(center, 6.5f, Color{120, 230, 154, 255});
            return;
        }

        const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
                state.topologyMap,
                state.lightDrag.topologyLightId);
        if (light == nullptr) {
            return;
        }

        const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
        const float radiusPixels = SectorAuthoringToWorldDistance(light->radius) * state.viewZoom;
        Color color = light->color;
        color.a = 245;
        DrawCircleLines(
                static_cast<int>(std::round(center.x)),
                static_cast<int>(std::round(center.y)),
                radiusPixels,
                WithAlpha(color, 165)
        );
        DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 15.0f, Color{120, 230, 154, 255});
        DrawCircleV(center, 6.5f, Color{120, 230, 154, 255});
        return;
    }

    if (state.hoveredTopologyStaticSpotLightId >= 0) {
        const SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(
                state.topologyMap,
                state.hoveredTopologyStaticSpotLightId);
        if (light == nullptr) {
            return;
        }

        const Vector2 origin = MapToScreen(Vector2{light->position.x, light->position.z});
        const Vector2 target = MapToScreen(Vector2{light->target.x, light->target.z});
        DrawCircleLines(
                static_cast<int>(std::round(origin.x)),
                static_cast<int>(std::round(origin.y)),
                13.0f,
                Color{245, 226, 154, 255});
        DrawCircleLines(
                static_cast<int>(std::round(target.x)),
                static_cast<int>(std::round(target.y)),
                11.0f,
                Color{122, 220, 244, 255});
        return;
    }

    if (state.hoveredTopologyDynamicSpotLightId >= 0) {
        const SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(
                state.topologyMap,
                state.hoveredTopologyDynamicSpotLightId);
        if (light == nullptr) {
            return;
        }

        const Vector2 origin = MapToScreen(Vector2{light->position.x, light->position.z});
        const Vector2 target = MapToScreen(Vector2{light->target.x, light->target.z});
        DrawCircleLines(
                static_cast<int>(std::round(origin.x)),
                static_cast<int>(std::round(origin.y)),
                13.0f,
                Color{245, 226, 154, 255});
        DrawCircleLines(
                static_cast<int>(std::round(target.x)),
                static_cast<int>(std::round(target.y)),
                11.0f,
                Color{122, 220, 244, 255});
        return;
    }

    if (state.hoveredTopologyDynamicLightId >= 0) {
        const SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                state.topologyMap,
                state.hoveredTopologyDynamicLightId);
        if (light == nullptr) {
            return;
        }

        const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
        DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 13.0f, Color{245, 226, 154, 255});
        return;
    }

    const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            state.topologyMap,
            state.hoveredTopologyLightId);
    if (light == nullptr) {
        return;
    }

    const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
    DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 13.0f, Color{245, 226, 154, 255});
}

void SectorEditor::DrawToolsPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    if (!IsToolAvailableInGraphAuthoritativeMode(state.currentTool)) {
        state.currentTool = SectorEditorTool::Select;
        statusText = LegacyTopologyMutationUnavailableMessage();
    }

    const engine::UIPanelResult panel = engine::BeginPanel(
            ui,
            config,
            assets,
            "sector_editor_tools",
            BuildLeftPanelRect(),
            font,
            "Tools"
    );

    const float rowH = 46.0f;
    const float gap = config.rowSpacing;
    const float separatorH = 22.0f;
    const float sectionLabelH = 26.0f;
    const float lightmapLabelH = 32.0f;
    const float bottomPadding = (rowH + gap * 2.0f) + 100;
    const auto rowsHeight = [rowH, gap](int count) {
        return static_cast<float>(count) * (rowH + gap);
    };
    const float toolsContentH =
            sectionLabelH + rowsHeight(4)
            + separatorH + sectionLabelH + rowsHeight(2)
            + separatorH + rowsHeight(6)
            + lightmapLabelH + rowsHeight(5)
            + separatorH + rowsHeight(4)
            + separatorH + rowsHeight(1)
            + bottomPadding;
    const float scrollContentW = ScrollAreaContentWidthForVerticalScrollbar(
            panel.contentRect.width,
            config,
            SectorEditorPanelScrollPaddingPx,
            false);
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_tools_scroll",
            panel.contentRect,
            Vector2{scrollContentW, toolsContentH},
            uiState.toolsScroll,
            false,
            SectorEditorPanelScrollPaddingPx
    );

    const float contentW = scroll.viewport.width;
    float y = 0.0f;
    const auto separator = [&]() {
        engine::Separator(
                config,
                Rectangle{
                        scroll.viewport.x,
                        scroll.viewport.y - uiState.toolsScroll.offset.y + y,
                        contentW,
                        12.0f
                }
        );
        y += 22.0f;
    };
    const auto sectionLabel = [&](const char* label) {
        engine::Text(
                config,
                assets,
                Rectangle{
                        scroll.viewport.x,
                        scroll.viewport.y - uiState.toolsScroll.offset.y + y,
                        contentW,
                        20.0f
                },
                font,
                label,
                engine::UITextJustify::Left
        );
        y += 26.0f;
    };

    const auto drawToolButton = [&](SectorEditorTool tool) {
        const bool clicked = engine::ToolButton(
                ui,
                config,
                input,
                assets,
                TextFormat("sector_editor_tool_%s", ToolName(tool)),
                Rectangle{0.0f, y, contentW, rowH},
                font,
                ToolName(tool),
                state.currentTool == tool);
        y += rowH + gap;
        return clicked;
    };

    const auto selectTool = [&](SectorEditorTool tool) {
        if (!IsToolAvailableInGraphAuthoritativeMode(tool)) {
            statusText = LegacyTopologyMutationUnavailableMessage();
            return;
        }
        state.selectDragArm = SelectDragArmState{};
        if (state.pendingAuthoringLine.active && tool != SectorEditorTool::AuthoringLine) {
            CancelPendingAuthoringLine("Cancelled authoring line");
        }
        if (state.pendingAuthoringRectangle.active && tool != SectorEditorTool::AuthoringRectangle) {
            CancelPendingAuthoringRectangle("Rectangle cancelled");
        }
        if (state.pendingAuthoringInsertVertex.active && tool != SectorEditorTool::AuthoringInsertVertex) {
            CancelPendingAuthoringInsertVertex("Insert Vertex cancelled");
        }
        if (state.authoringVertexDrag.active && tool != SectorEditorTool::AuthoringMove) {
            CancelAuthoringVertexDrag("Cancelled authoring vertex move");
        }
        if (state.lightDrag.active
                && tool != SectorEditorTool::Move
                && tool != SectorEditorTool::Select
                && tool != SectorEditorTool::StaticLight
                && tool != SectorEditorTool::StaticSpotLight
                && tool != SectorEditorTool::DynamicLight
                && tool != SectorEditorTool::DynamicSpotLight) {
            CancelLightDrag("Cancelled light move");
        }
        if (IsGraphAuthoringTool(tool)) {
            ClearTopologySelectionOnly();
        }
        state.currentTool = tool;
        if (tool == SectorEditorTool::AuthoringLine) {
            statusText = "Line: click start point";
        } else if (tool == SectorEditorTool::AuthoringInsertVertex) {
            state.pendingAuthoringInsertVertex = PendingAuthoringInsertVertex{};
            if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line
                    && FindSectorAuthoringLine(state.authoringGraph, state.selectedAuthoring.lineId) != nullptr) {
                state.pendingAuthoringInsertVertex.active = true;
                state.pendingAuthoringInsertVertex.lineId = state.selectedAuthoring.lineId;
                statusText = "Insert Vertex: click point on selected line, Esc/right click cancels";
            } else {
                statusText = "Insert Vertex: select or click an authoring line";
            }
        } else if (tool == SectorEditorTool::RuntimeObject) {
            statusText = "Billboard: click inside a sector to place a billboard";
        } else if (tool == SectorEditorTool::Door) {
            statusText = "Door: click a two-sided portal line";
        }
    };

    sectionLabel("Graph authoring");
    const SectorEditorTool graphTools[] = {
            SectorEditorTool::Select,
            SectorEditorTool::AuthoringLine,
            SectorEditorTool::AuthoringRectangle,
            SectorEditorTool::AuthoringInsertVertex
    };
    for (SectorEditorTool tool : graphTools) {
        if (drawToolButton(tool)) {
            selectTool(tool);
        }
    }

    separator();
    sectionLabel("Map objects");
    const SectorEditorTool mapTools[] = {
            SectorEditorTool::RuntimeObject,
            SectorEditorTool::Door,
            SectorEditorTool::StaticLight,
            SectorEditorTool::StaticSpotLight,
            SectorEditorTool::DynamicLight,
            SectorEditorTool::DynamicSpotLight
    };
    for (SectorEditorTool tool : mapTools) {
        if (drawToolButton(tool)) {
            selectTool(tool);
        }
    }

    separator();

    const float documentButtonW = (contentW - gap) * 0.5f;
    if (engine::Button(ui, config, input, assets, "sector_editor_new", Rectangle{0.0f, y, documentButtonW, rowH}, font, "New")) {
        OpenNewConfirmation(assets);
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_load", Rectangle{documentButtonW + gap, y, documentButtonW, rowH}, font, "Load")) {
        OpenLoadLevelModal();
    }
    y += rowH + gap;
    if (engine::Button(ui, config, input, assets, "sector_editor_save", Rectangle{0.0f, y, documentButtonW, rowH}, font, "Save")) {
        OpenSaveLevelModal();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_reload", Rectangle{documentButtonW + gap, y, documentButtonW, rowH}, font, "Reload")) {
        OpenReloadConfirmation(assets);
    }
    y += rowH + gap;

    if (engine::Button(ui, config, input, assets, "sector_editor_add_map_texture", Rectangle{0.0f, y, contentW, rowH}, font, "Add Map Texture")) {
        OpenAddMapTextureModal(assets);
    }
    y += rowH + gap;
    if (engine::Button(ui, config, input, assets, "sector_editor_preview_settings_2d", Rectangle{0.0f, y, contentW, rowH}, font, "Settings")) {
        OpenPreviewSettingsModal();
    }
    y += rowH + gap;

    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 28.0f}, font, "Lightmap Settings", engine::UITextJustify::Left, config.mutedTextColor);
    y += 32.0f;

    const float lightmapLabelW = 180.0f;
    const auto drawLightmapSetting = [&](const char* id, const char* label, float& value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals, const char* status) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, lightmapLabelW, rowH},
                Rectangle{lightmapLabelW + gap, y, std::max(0.0f, contentW - lightmapLabelW - gap), rowH},
                engine::UITextJustify::Left,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        if (result.changed && result.value != value) {
            value = result.value;
            state.hasUnsavedChanges = true;
            state.topologyDocumentDirty = true;
            statusText = status;
        }
        y += rowH + gap;
    };

    state.topologyMap.lightmapSettings.ambientOcclusionRadius = ClampAmbientOcclusionRadius(state.topologyMap.lightmapSettings.ambientOcclusionRadius);
    state.topologyMap.lightmapSettings.ambientOcclusionStrength = ClampAmbientOcclusionStrength(state.topologyMap.lightmapSettings.ambientOcclusionStrength);
    state.topologyMap.lightmapSettings.indirectBounceRadius = ClampIndirectBounceRadius(state.topologyMap.lightmapSettings.indirectBounceRadius);
    state.topologyMap.lightmapSettings.indirectBounceStrength = ClampIndirectBounceStrength(state.topologyMap.lightmapSettings.indirectBounceStrength);
    drawLightmapSetting(
            "sector_editor_ao_radius",
            "AO radius",
            state.topologyMap.lightmapSettings.ambientOcclusionRadius,
            uiState.ambientOcclusionRadiusInput,
            SectorWorldToAuthoringDistance(0.05f),
            SectorWorldToAuthoringDistance(16.0f),
            2,
            "Updated AO radius"
    );
    drawLightmapSetting(
            "sector_editor_ao_strength",
            "AO strength",
            state.topologyMap.lightmapSettings.ambientOcclusionStrength,
            uiState.ambientOcclusionStrengthInput,
            0.0f,
            1.0f,
            3,
            "Updated AO strength"
    );
    drawLightmapSetting(
            "sector_editor_bounce_radius",
            "Bounce radius",
            state.topologyMap.lightmapSettings.indirectBounceRadius,
            uiState.indirectBounceRadiusInput,
            SectorWorldToAuthoringDistance(0.05f),
            SectorWorldToAuthoringDistance(16.0f),
            2,
            "Updated bounce radius"
    );
    drawLightmapSetting(
            "sector_editor_bounce_strength",
            "Bounce strength",
            state.topologyMap.lightmapSettings.indirectBounceStrength,
            uiState.indirectBounceStrengthInput,
            0.0f,
            1.0f,
            3,
            "Updated bounce strength"
    );

    if (engine::Button(ui, config, input, assets, "sector_editor_bake_lightmaps", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        BakeLightmaps();
    }
    y += rowH + gap;

    separator();

    const float gridLabelW = 64.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, gridLabelW, rowH}, font, "Grid", engine::UITextJustify::Left, config.mutedTextColor);
    engine::IntInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_grid",
            Rectangle{gridLabelW + gap, y, std::max(0.0f, contentW - gridLabelW - gap), rowH},
            font,
            state.gridSize,
            uiState.gridSizeInput,
            1,
            64,
            1
    );
    y += rowH + gap;

    engine::Checkbox(ui, config, input, assets, "sector_editor_show_grid", Rectangle{0.0f, y, contentW, rowH}, font, "Show grid", state.showGrid);
    y += rowH + gap;
    engine::Checkbox(ui, config, input, assets, "sector_editor_show_axes", Rectangle{0.0f, y, contentW, rowH}, font, "Show axes", state.showAxes);
    y += rowH + gap;
    engine::Checkbox(ui, config, input, assets, "sector_editor_show_ids", Rectangle{0.0f, y, contentW, rowH}, font, "Show ids", state.showSectorIds);
    y += rowH + gap;

    separator();

    if (engine::Button(ui, config, input, assets, "sector_editor_preview_3d", Rectangle{0.0f, y, contentW, rowH}, font, "3D Mode")) {
        if (engineContext != nullptr) {
            TryEnterPreview3D(*engineContext, ui);
        }
    }

    engine::EndScrollArea(ui, config, input, scroll, uiState.toolsScroll);
    engine::EndPanel(ui, config, panel);
}

void SectorEditor::DrawSectorsPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    const engine::UIPanelResult panel = engine::BeginPanel(
            ui,
            config,
            assets,
            "sector_editor_sectors",
            BuildRightPanelRect(),
            font,
            "Inspector"
    );

    ClearStaleTopologySelection();
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();

    const bool hasSelectedTopologySector = SelectedTopologySector() != nullptr;
    const bool hasSelectedTopologyVertex = SelectedTopologyVertex() != nullptr;
    const bool hasSelectedTopologySideDef = SelectedTopologySideDef() != nullptr;
    const bool hasSelectedTopologyLineDef = state.topologySelectionKind == TopologySelectionKind::LineDef
            && SelectedTopologyLineDef() != nullptr;
    const bool hasSelectedLight = SelectedTopologyLight() != nullptr;
    const bool hasSelectedStaticSpotLight = SelectedTopologyStaticSpotLight() != nullptr;
    const bool hasSelectedDynamicLight = SelectedTopologyDynamicLight() != nullptr;
    const bool hasSelectedDynamicSpotLight = SelectedTopologyDynamicSpotLight() != nullptr;
    const bool hasSelectedRuntimeObject = SelectedRuntimeObject() != nullptr;
    const SectorEditorInspectorTarget inspectorTarget = ResolveSectorEditorInspectorTarget(state);
    const bool allowLegacyTopologyInspector =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::LegacyTopology
            || inspectorTarget.kind == SectorEditorInspectorTargetKind::None;
    const SectorAuthoringLine* selectedAuthoringLine =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringLine
            ? FindSectorAuthoringLine(state.authoringGraph, inspectorTarget.lineId)
            : nullptr;
    const SectorAuthoringFaceAnchor* selectedAuthoringFaceAnchor =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringFaceAnchor
            ? FindSectorAuthoringFaceAnchor(state.authoringGraph, inspectorTarget.faceAnchorId)
            : nullptr;
    const SectorAuthoringVertex* selectedAuthoringVertex =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringVertex
            ? FindSectorAuthoringVertex(state.authoringGraph, inspectorTarget.vertexId)
            : nullptr;
    const SectorTopologyVertex* inspectedVertex = FindSectorTopologyVertex(
            state.topologyMap,
            state.inspectedTopologyVertexId);
    SectorEditorMaterialEditingService materialEditing = BuildMaterialEditingService();
    const bool hasInspectedVertex = !hasSelectedTopologySector
            && !hasSelectedTopologyVertex
            && !hasSelectedTopologySideDef
            && !hasSelectedTopologyLineDef
            && !hasSelectedLight
            && !hasSelectedStaticSpotLight
            && !hasSelectedDynamicLight
            && !hasSelectedDynamicSpotLight
            && state.currentTool == SectorEditorTool::Move
            && inspectedVertex != nullptr;

    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float scrollContentW = ScrollAreaContentWidthForVerticalScrollbar(
            panel.contentRect.width,
            config,
            SectorEditorPanelScrollPaddingPx,
            false);
    const engine::UIConfig smallConfig = SectorEditorSmallFontConfig(config, assets, smallFont);
    const SectorEditorPlacedObjectInspectorCallbacks runtimeObjectInspectorCallbacks{
            [this]() { return SelectedRuntimeObject(); },
            [this](
                    const char* status,
                    const std::function<bool(SectorPlacedRuntimeObject&)>& mutate) {
                return MutateSelectedRuntimeObject(status, mutate);
            },
            [this]() { OpenSelectedBillboardSpritePicker(); },
            [this]() { OpenSelectedDoorTexturePicker(); },
            [this]() { OpenDoorTextureSettingsModal(); },
            [this]() { return DeleteSelectedRuntimeObject(); },
            [this](bool& outOpen) {
                const SectorPlacedRuntimeObject* selectedObject = SelectedRuntimeObject();
                if (selectedObject == nullptr || engineContext == nullptr) {
                    return false;
                }
                for (const SectorPlacedRuntimeObjectEntity& entry : state.runtimeObjects.placedObjectEntities) {
                    if (entry.placedObjectId != selectedObject->id
                            || !engineContext->world.IsAlive(entry.entity)
                            || !engineContext->world.Has<SectorDoorMotion>(entry.entity)) {
                        continue;
                    }
                    const SectorDoorMotion& motion = engineContext->world.Get<SectorDoorMotion>(entry.entity);
                    outOpen = std::isfinite(motion.targetOpenFraction)
                            && motion.targetOpenFraction > 0.5f;
                    return true;
                }
                return false;
            },
            [this](bool open) {
                const SectorPlacedRuntimeObject* selectedObject = SelectedRuntimeObject();
                if (selectedObject == nullptr || engineContext == nullptr) {
                    return;
                }
                for (const SectorPlacedRuntimeObjectEntity& entry : state.runtimeObjects.placedObjectEntities) {
                    if (entry.placedObjectId != selectedObject->id
                            || !engineContext->world.IsAlive(entry.entity)
                            || !engineContext->world.Has<SectorDoorMotion>(entry.entity)) {
                        continue;
                    }
                    SectorDoorMotion& motion = engineContext->world.Get<SectorDoorMotion>(entry.entity);
                    motion.targetOpenFraction = open ? 1.0f : 0.0f;
                    statusText = open
                            ? "Door debug runtime target: open"
                            : "Door debug runtime target: close";
                    return;
                }
            }
    };
    const auto inspectorContentHeight = [&]() {
        if (inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringUnavailable) {
            return 120.0f;
        }
        if (hasSelectedRuntimeObject) {
            const SectorEditorPlacedObjectInspectorMeasureContext runtimeObjectMeasureContext{
                    assets,
                    smallFont,
                    smallConfig,
                    state,
                    engineContext,
                    runtimeObjectInspectorCallbacks,
                    scrollContentW,
                    rowH,
                    gap
            };
            return MeasureSectorEditorPlacedObjectInspectorContentHeight(runtimeObjectMeasureContext);
        }
        if (hasSelectedLight) {
            return StaticLightInspectorContentHeight(rowH, gap, !uiState.idEditError.empty());
        }
        if (hasSelectedStaticSpotLight) {
            return StaticSpotLightInspectorContentHeight(rowH, gap, !uiState.idEditError.empty());
        }
        if (hasSelectedDynamicLight) {
            return DynamicLightInspectorContentHeight(rowH, gap, !uiState.idEditError.empty());
        }
        if (hasSelectedDynamicSpotLight) {
            const float shadowNoteHeight = MeasureSectorEditorWrappedTextHeight(
                    smallConfig,
                    assets,
                    smallFont,
                    TextFormat(
                            "Requests one of %zu shadow slots. Priority decides budget; over-budget spots still light.",
                            MaxDynamicSpotLightShadowCasters),
                    scrollContentW,
                    2);
            return DynamicSpotLightInspectorContentHeight(rowH, gap, !uiState.idEditError.empty(), shadowNoteHeight);
        }
        if (hasSelectedTopologySector && allowLegacyTopologyInspector) {
            return SectorInspectorContentHeight(rowH, gap, !uiState.idEditError.empty());
        }
        if (hasSelectedTopologyVertex && allowLegacyTopologyInspector) {
            return SelectedVertexInspectorContentHeight();
        }
        if (hasSelectedTopologySideDef && allowLegacyTopologyInspector) {
            return 1240.0f;
        }
        if (hasSelectedTopologyLineDef && allowLegacyTopologyInspector) {
            return 218.0f;
        }
        if (hasInspectedVertex) {
            return InspectedVertexInspectorContentHeight();
        }
        if (selectedAuthoringLine != nullptr) {
            const SectorAuthoringVertex* start =
                    FindSectorAuthoringVertex(state.authoringGraph, selectedAuthoringLine->startVertexId);
            const SectorAuthoringVertex* end =
                    FindSectorAuthoringVertex(state.authoringGraph, selectedAuthoringLine->endVertexId);
            const float endpointSummaryHeight = MeasureSectorEditorWrappedTextHeight(
                    smallConfig,
                    assets,
                    smallFont,
                    start != nullptr && end != nullptr
                            ? TextFormat(
                                    "From %.2f, %.2f  To %.2f, %.2f",
                                    SectorCoordToVisibleAuthoring(start->x),
                                    SectorCoordToVisibleAuthoring(start->y),
                                    SectorCoordToVisibleAuthoring(end->x),
                                    SectorCoordToVisibleAuthoring(end->y))
                            : "Line endpoints are invalid",
                    scrollContentW);
            return AuthoringLineInspectorContentHeight(
                    *selectedAuthoringLine,
                    state.authoringGraph,
                    rowH,
                    gap,
                    endpointSummaryHeight);
        }
        if (selectedAuthoringFaceAnchor != nullptr) {
            const float anchorSummaryHeight = MeasureSectorEditorWrappedTextHeight(
                    smallConfig,
                    assets,
                    smallFont,
                    TextFormat(
                            "Anchor %.2f, %.2f",
                            SectorCoordToVisibleAuthoring(selectedAuthoringFaceAnchor->x),
                            SectorCoordToVisibleAuthoring(selectedAuthoringFaceAnchor->y)),
                    scrollContentW);
            return AuthoringFaceInspectorContentHeight(*selectedAuthoringFaceAnchor, rowH, gap, anchorSummaryHeight);
        }
        if (selectedAuthoringVertex != nullptr) {
            return 120.0f;
        }
        return 42.0f;
    };
    const float contentH = inspectorContentHeight();
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_inspector_scroll",
            panel.contentRect,
            Vector2{scrollContentW, contentH},
            uiState.inspectorScroll,
            false,
            SectorEditorPanelScrollPaddingPx
    );

    const float contentW = scroll.viewport.width;
    float y = 0.0f;

    if (inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringUnavailable) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                "Authoring Inspector",
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 64.0f},
                font,
                inspectorTarget.status.empty()
                        ? "Mapped authoring target is unavailable."
                        : inspectorTarget.status.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (hasSelectedRuntimeObject) {
        SectorEditorPlacedObjectInspectorContext runtimeObjectInspectorContext{
                ui,
                config,
                input,
                assets,
                font,
                smallFont,
                scroll,
                state,
                uiState,
                engineContext,
                runtimeObjectInspectorCallbacks,
                contentW,
                rowH,
                gap
        };
        DrawSectorEditorPlacedObjectInspector(runtimeObjectInspectorContext);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (hasSelectedTopologySector && allowLegacyTopologyInspector) {
        const auto hasAuthoringGraph = [this]() {
            return !state.authoringGraph.vertices.empty()
                    || !state.authoringGraph.lines.empty()
                    || !state.authoringGraph.lineSides.empty()
                    || !state.authoringGraph.faceAnchors.empty();
        };
        const auto selectedAuthoringFaceAnchorUnavailable = [this, hasAuthoringGraph]() {
            const SectorTopologySector* selectedSector = SelectedTopologySector();
            if (selectedSector == nullptr) {
                return false;
            }
            if (!hasAuthoringGraph()) {
                return true;
            }
            if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                    || state.authoringDerivedTopologyStale
                    || !state.authoringDerivation.success) {
                return true;
            }
            return FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                           state,
                           selectedSector->id) < 0;
        };
        const auto reportAuthoringFaceAnchorUnavailable = [this, hasAuthoringGraph]() {
            const char* message = !hasAuthoringGraph()
                    ? "Cannot edit sector property: authoring data is required."
                    : "Sector property edit unavailable: selected sector has no current face anchor mapping";
            statusText = message;
            return true;
        };
        const auto mutateSelectedAuthoringFaceAnchor =
                [this](const char* status, const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate) {
                    const SectorTopologySector* selectedSector = SelectedTopologySector();
                    if (selectedSector == nullptr) {
                        return false;
                    }
                    if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                            || state.authoringDerivedTopologyStale
                            || !state.authoringDerivation.success) {
                        return false;
                    }
                    if (FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                                state,
                                selectedSector->id) < 0) {
                        return false;
                    }
                    MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                            state,
                            selectedSector->id,
                            status,
                            mutate);
                    return true;
                };
        SectorEditorMaterialEditingService materialEditing = BuildMaterialEditingService();
        const SectorEditorSectorInspectorCallbacks callbacks{
                [this]() { return TryRenameSelectedDerivedSectorAuthoringName(); },
                [this](const char* status) { statusText = status != nullptr ? status : ""; },
                [this](const char* status) { MarkTopologyDocumentEdited(status); },
                [mutateSelectedAuthoringFaceAnchor,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](
                        float floorZ,
                        float ceilingZ) {
                    const char* status = "Updated sector height";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [floorZ, ceilingZ](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.floorZ == floorZ && anchor.ceilingZ == ceilingZ) {
                                        return false;
                                    }
                                    anchor.floorZ = floorZ;
                                    anchor.ceilingZ = ceilingZ;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return reportAuthoringFaceAnchorUnavailable();
                },
                [mutateSelectedAuthoringFaceAnchor,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](bool ceilingSky) {
                    const char* status = "Updated sector ceiling sky";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [ceilingSky](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.ceilingSky == ceilingSky) {
                                        return false;
                                    }
                                    anchor.ceilingSky = ceilingSky;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return reportAuthoringFaceAnchorUnavailable();
                },
                [mutateSelectedAuthoringFaceAnchor,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](float intensity) {
                    const char* status = "Updated sector ambient intensity";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [intensity](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.ambientIntensity == intensity) {
                                        return false;
                                    }
                                    anchor.ambientIntensity = intensity;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return reportAuthoringFaceAnchorUnavailable();
                },
                [mutateSelectedAuthoringFaceAnchor,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](Color color) {
                    const char* status = "Updated sector ambient color";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [color](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.ambientColor.r == color.r
                                            && anchor.ambientColor.g == color.g
                                            && anchor.ambientColor.b == color.b
                                            && anchor.ambientColor.a == color.a) {
                                        return false;
                                    }
                                    anchor.ambientColor = color;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return reportAuthoringFaceAnchorUnavailable();
                },
                [mutateSelectedAuthoringFaceAnchor,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](
                        TopologySectorTextureField field,
                        const SectorTopologyUvSettings& uv) {
                    const char* status = "Updated sector UV";
                    const auto applyAnchorUv =
                            [field, uv](SectorAuthoringFaceAnchor& anchor) {
                                SectorTopologyUvSettings* target = nullptr;
                                switch (field) {
                                case TopologySectorTextureField::Floor:
                                    target = &anchor.floorUv;
                                    break;
                                case TopologySectorTextureField::Ceiling:
                                    target = &anchor.ceilingUv;
                                    break;
                                case TopologySectorTextureField::DefaultWall:
                                    target = &anchor.defaultWall.uv;
                                    break;
                                case TopologySectorTextureField::DefaultLower:
                                    target = &anchor.defaultLower.uv;
                                    break;
                                case TopologySectorTextureField::DefaultUpper:
                                    target = &anchor.defaultUpper.uv;
                                    break;
                                case TopologySectorTextureField::None:
                                    break;
                                }
                                if (target == nullptr) {
                                    return false;
                                }
                                *target = uv;
                                return true;
                };
                    if (mutateSelectedAuthoringFaceAnchor(status, applyAnchorUv)) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return reportAuthoringFaceAnchorUnavailable();
                }
        };
        if (game::DrawTopologySectorInspector(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    smallFont,
                    scroll,
                    contentW,
                    rowH,
                    gap,
                    *SelectedTopologySector(),
                    state,
                    uiState,
                    materialEditing,
                    callbacks)) {
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return;
        }
    }

    if (allowLegacyTopologyInspector && (hasSelectedTopologySideDef || hasSelectedTopologyLineDef)) {
        if (DrawTopologySideDefInspector(ui, config, input, assets, font, smallFont, scroll, contentW, rowH, gap)) {
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return;
        }
    }

    if (hasSelectedLight) {
        const SectorEditorLightInspectorCallbacks callbacks{
                [this](const char* status) {
                    SectorEditorTopologyActionResult result;
                    result.changed = true;
                    result.status = status == nullptr ? "" : status;
                    FinishTopologyActionResult(result);
                },
                [this]() { return DeleteSelectedLight(); },
                [this]() { return BakeLightmaps(); }
        };
        DrawSelectedStaticLightInspector(
                ui,
                config,
                input,
                assets,
                font,
                scroll,
                contentW,
                rowH,
                gap,
                *SelectedTopologyLight(),
                uiState,
                callbacks);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (hasSelectedStaticSpotLight) {
        const SectorEditorLightInspectorCallbacks callbacks{
                [this](const char* status) {
                    SectorEditorTopologyActionResult result;
                    result.changed = true;
                    result.status = status == nullptr ? "" : status;
                    FinishTopologyActionResult(result);
                },
                [this]() { return DeleteSelectedLight(); },
                [this]() { return BakeLightmaps(); }
        };
        DrawSelectedStaticSpotLightInspector(
                ui,
                config,
                input,
                assets,
                font,
                scroll,
                contentW,
                rowH,
                gap,
                *SelectedTopologyStaticSpotLight(),
                uiState,
                callbacks);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (hasSelectedDynamicLight) {
        const SectorEditorLightInspectorCallbacks callbacks{
                [this](const char* status) {
                    SectorEditorTopologyActionResult result;
                    result.changed = true;
                    result.status = status == nullptr ? "" : status;
                    FinishTopologyActionResult(result);
                },
                [this]() { return DeleteSelectedLight(); },
                []() { return false; }
        };
        DrawSelectedDynamicLightInspector(
                ui,
                config,
                input,
                assets,
                font,
                scroll,
                contentW,
                rowH,
                gap,
                *SelectedTopologyDynamicLight(),
                uiState,
                callbacks);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (hasSelectedDynamicSpotLight) {
        const SectorEditorLightInspectorCallbacks callbacks{
                [this](const char* status) {
                    SectorEditorTopologyActionResult result;
                    result.changed = true;
                    result.status = status == nullptr ? "" : status;
                    FinishTopologyActionResult(result);
                },
                [this]() { return DeleteSelectedLight(); },
                []() { return false; }
        };
        DrawSelectedDynamicSpotLightInspector(
                ui,
                config,
                input,
                assets,
                font,
                smallFont,
                scroll,
                contentW,
                rowH,
                gap,
                *SelectedTopologyDynamicSpotLight(),
                uiState,
                callbacks);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if ((allowLegacyTopologyInspector && hasSelectedTopologyVertex)
            || hasInspectedVertex) {
        const SectorEditorVertexInspectorCallbacks callbacks{
                [this]() { ClearStaleTopologySelection(); }
        };
        DrawTopologyVertexInspector(
                ui,
                config,
                input,
                assets,
                font,
                contentW,
                rowH,
                gap,
                inspectedVertex,
                hasSelectedTopologyVertex,
                state,
                callbacks);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (selectedAuthoringLine != nullptr) {
        const SectorAuthoringVertex* start =
                FindSectorAuthoringVertex(state.authoringGraph, selectedAuthoringLine->startVertexId);
        const SectorAuthoringVertex* end =
                FindSectorAuthoringVertex(state.authoringGraph, selectedAuthoringLine->endVertexId);
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Authoring Line: %d", selectedAuthoringLine->id),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;

        if (start != nullptr && end != nullptr) {
            const char* endpointText = TextFormat(
                    "From %.2f, %.2f  To %.2f, %.2f",
                    SectorCoordToVisibleAuthoring(start->x),
                    SectorCoordToVisibleAuthoring(start->y),
                    SectorCoordToVisibleAuthoring(end->x),
                    SectorCoordToVisibleAuthoring(end->y));
            const float endpointHeight = MeasureSectorEditorWrappedTextHeight(
                    smallConfig,
                    assets,
                    smallFont,
                    endpointText,
                    contentW);
            engine::Text(
                    ui,
                    smallConfig,
                    assets,
                    Rectangle{0.0f, y, contentW, endpointHeight},
                    smallFont,
                    endpointText,
                    engine::UITextJustify::Left,
                    smallConfig.mutedTextColor,
                    true);
            y += endpointHeight;
        } else {
            const float endpointHeight = MeasureSectorEditorWrappedTextHeight(
                    smallConfig,
                    assets,
                    smallFont,
                    "Line endpoints are invalid",
                    contentW);
            engine::Text(ui, smallConfig, assets, Rectangle{0.0f, y, contentW, endpointHeight}, smallFont, "Line endpoints are invalid", engine::UITextJustify::Left, config.invalidColor, true);
            y += endpointHeight;
        }

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_line_insert_vertex",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Insert Vertex")) {
            BeginPendingAuthoringInsertVertex(selectedAuthoringLine->id);
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return;
        }
        y += rowH + gap;

        bool blocksPlayer = selectedAuthoringLine->flags.blocksPlayer;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_line_blocks_player",
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Blocks Player",
                    blocksPlayer)) {
            const int lineId = selectedAuthoringLine->id;
            MutateSectorEditorAuthoringLineById(
                    state,
                    lineId,
                    "Updated authoring line flags",
                    [blocksPlayer](SectorAuthoringLine& line) {
                        if (line.flags.blocksPlayer == blocksPlayer) {
                            return false;
                        }
                        line.flags.blocksPlayer = blocksPlayer;
                        return true;
                    });
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return;
        }
        y += 36.0f + gap;

        const auto drawAuthoringSideSection =
                [&](SectorTopologySideKind sideKind, const char* title, const char* idPrefix) {
                    engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
                    y += 18.0f;
                    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, title, engine::UITextJustify::Left, config.textColor);
                    y += 30.0f;

                    const SectorAuthoringSideId sideId{selectedAuthoringLine->id, sideKind};
                    const SectorAuthoringLineSide* authoringSide =
                            FindSectorAuthoringLineSide(state.authoringGraph, sideId);
                    const auto textureForPart = [authoringSide](TopologyWallPart part) -> std::string {
                        if (authoringSide == nullptr) {
                            return std::string{};
                        }
                        return TopologyWallPartSettingsFor(*authoringSide, part).textureId;
                    };
                    const auto decalForPart = [authoringSide](TopologyWallPart part) -> SectorTopologyDecalLayer {
                        if (authoringSide == nullptr) {
                            return SectorTopologyDecalLayer{};
                        }
                        return TopologyWallPartSettingsFor(*authoringSide, part).decal;
                    };
                    const auto mappedTargetForPart = [this, sideId](TopologyWallPart part, TopologySurfaceEditTarget& outTarget) {
                        if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                                || state.authoringDerivedTopologyStale
                                || !state.authoringDerivation.success) {
                            return false;
                        }
                        for (const SectorAuthoringDerivedSideMapping& mapping : state.authoringDerivation.mapping.sides) {
                            if (mapping.authoringLineId != sideId.lineId || mapping.authoringSide != sideId.side) {
                                continue;
                            }
                            const SectorTopologySideDef* sideDef =
                                    FindSectorTopologySideDef(state.topologyMap, mapping.topologySideDefId);
                            if (sideDef == nullptr) {
                                continue;
                            }
                            outTarget.kind = TopologyWallPartEditTargetKind(part);
                            outTarget.sectorId = sideDef->sectorId;
                            outTarget.lineDefId = sideDef->lineDefId;
                            outTarget.sideDefId = sideDef->id;
                            outTarget.side = sideDef->side;
                            return true;
                        }
                        return false;
                    };
                    const auto mutateSide = [this, sideId](const char* status, const std::function<bool(SectorAuthoringLineSide&)>& mutate) {
                        return MutateSectorEditorAuthoringSideById(state, sideId, status, mutate);
                    };
                    const auto drawTextureRow =
                            [&](const char* suffix, const char* label, TopologyWallPart part) {
                                const float buttonW = 38.0f;
                                const bool canClear = part == TopologyWallPart::Middle;
                                const float clearW = canClear ? 58.0f : 0.0f;
                                const std::string textureId = textureForPart(part);
                                const SectorEditorInspectorTextureRowLayout row =
                                        BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                                const bool missing = !textureId.empty()
                                        && FindSectorTopologyTexture(state.topologyMap, textureId) == nullptr;
                                engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
                                engine::Text(
                                        ui,
                                        smallConfig,
                                        assets,
                                        row.valueRect,
                                        smallFont,
                                        textureId.empty() ? "<none>" : textureId.c_str(),
                                        engine::UITextJustify::Left,
                                        missing ? config.invalidColor : config.mutedTextColor);
                                if (canClear
                                        && engine::Button(
                                                ui,
                                                config,
                                                input,
                                                assets,
                                                TextFormat("%s_%s_clear", idPrefix, suffix),
                                                row.clearButtonRect,
                                                font,
                                                "Clear")) {
                                    mutateSide(
                                            "Cleared authoring middle texture",
                                            [part](SectorAuthoringLineSide& side) {
                                                SectorTopologyWallPartSettings& settings =
                                                        TopologyWallPartSettingsFor(side, part);
                                                if (IsDefaultWallPartSettings(settings)) {
                                                    return false;
                                                }
                                                settings = SectorTopologyWallPartSettings{};
                                                return true;
                                            });
                                }
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s", idPrefix, suffix),
                                            row.pickerButtonRect,
                                            font,
                                            ">")) {
                                    if (!materialEditing.OpenMaterialPickerForAuthoringSide(
                                                sideId,
                                                part,
                                                TopologyMaterialLayer::Base)) {
                                        statusText = "Authoring side texture picker unavailable: derived mapping is not current";
                                    }
                                }
                                y += row.height + gap;
                            };
                    const auto drawDecalControls =
                            [&](const char* suffix, const char* title, TopologyWallPart part) {
                                const SectorTopologyDecalLayer decal = decalForPart(part);
                                const float buttonW = 38.0f;
                                const float clearW = 92.0f;
                                const SectorEditorInspectorTextureRowLayout row =
                                        BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                                const bool missing = !decal.textureId.empty()
                                        && FindSectorTopologyTexture(state.topologyMap, decal.textureId) == nullptr;
                                engine::Text(ui, config, assets, row.labelRect, font, title, engine::UITextJustify::Left, config.mutedTextColor);
                                engine::Text(
                                        ui,
                                        smallConfig,
                                        assets,
                                        row.valueRect,
                                        smallFont,
                                        decal.textureId.empty() ? "<none>" : decal.textureId.c_str(),
                                        engine::UITextJustify::Left,
                                        missing ? config.invalidColor : config.mutedTextColor);
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_clear_decal", idPrefix, suffix),
                                            row.clearButtonRect,
                                            font,
                                            "Clear")) {
                                    mutateSide(
                                            "Cleared authoring side decal",
                                            [part](SectorAuthoringLineSide& side) {
                                                SectorTopologyDecalLayer& target =
                                                        TopologyWallPartSettingsFor(side, part).decal;
                                                if (IsDefaultDecalLayer(target)) {
                                                    return false;
                                                }
                                                ResetDecalLayer(target);
                                                return true;
                                            });
                                }
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_pick_decal", idPrefix, suffix),
                                            row.pickerButtonRect,
                                            font,
                                            ">")) {
                                    if (!materialEditing.OpenMaterialPickerForAuthoringSide(
                                                sideId,
                                                part,
                                                TopologyMaterialLayer::Decal)) {
                                        statusText = "Authoring side decal picker unavailable: derived mapping is not current";
                                    }
                                }
                                y += row.height + gap;

                                if (decal.textureId.empty()) {
                                    return;
                                }

                                const SectorEditorInspectorNumericRowLayout opacityLayout =
                                        BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                                const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                                        ui,
                                        config,
                                        input,
                                        assets,
                                        font,
                                        TextFormat("%s_%s_decal_opacity", idPrefix, suffix),
                                        "Opacity:",
                                        opacityLayout.labelRect,
                                        opacityLayout.inputRect,
                                        engine::UITextJustify::Left,
                                        decal.opacity,
                                        uiState.topologySideDefDecalOpacityInput,
                                        0.0f,
                                        1.0f,
                                        3);
                                if (opacityResult.changed && opacityResult.value != decal.opacity && opacityResult.finite) {
                                    mutateSide(
                                            "Updated authoring side decal opacity",
                                            [part, value = opacityResult.value](SectorAuthoringLineSide& side) {
                                                SectorTopologyDecalLayer& target =
                                                        TopologyWallPartSettingsFor(side, part).decal;
                                                if (target.textureId.empty() || target.opacity == value) {
                                                    return false;
                                                }
                                                target.opacity = value;
                                                return true;
                                            });
                                }
                                y += rowH + gap;

                                bool emissive = decal.emissive;
                                if (engine::Checkbox(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_decal_emissive", idPrefix, suffix),
                                            Rectangle{0.0f, y, contentW, 36.0f},
                                            font,
                                            "Emissive",
                                            emissive)) {
                                    mutateSide(
                                            "Updated authoring side decal emissive",
                                            [part, emissive](SectorAuthoringLineSide& side) {
                                                SectorTopologyDecalLayer& target =
                                                        TopologyWallPartSettingsFor(side, part).decal;
                                                if (target.textureId.empty() || target.emissive == emissive) {
                                                    return false;
                                                }
                                                target.emissive = emissive;
                                                return true;
                                            });
                                }
                                y += 36.0f + gap;

                                if (decal.emissive) {
                                    const SectorEditorInspectorNumericRowLayout bloomLayout =
                                            BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                                    const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            font,
                                            TextFormat("%s_%s_decal_bloom", idPrefix, suffix),
                                            "Bloom:",
                                            bloomLayout.labelRect,
                                            bloomLayout.inputRect,
                                            engine::UITextJustify::Left,
                                            decal.bloomIntensity,
                                            uiState.topologySideDefDecalBloomIntensityInput,
                                            0.0f,
                                            10.0f,
                                            3);
                                    if (bloomResult.changed && bloomResult.value != decal.bloomIntensity && bloomResult.finite) {
                                        mutateSide(
                                                "Updated authoring side decal bloom intensity",
                                                [part, value = bloomResult.value](SectorAuthoringLineSide& side) {
                                                    SectorTopologyDecalLayer& target =
                                                            TopologyWallPartSettingsFor(side, part).decal;
                                                    if (target.textureId.empty() || target.bloomIntensity == value) {
                                                        return false;
                                                    }
                                                    target.bloomIntensity = value;
                                                    return true;
                                                });
                                    }
                                    y += rowH + gap;
                                }

                                engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
                                const Rectangle swatchLocal{82.0f, y + 3.0f, 56.0f, rowH - 6.0f};
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_decal_tint", idPrefix, suffix),
                                            swatchLocal,
                                            font,
                                            "")) {
                                    TopologySurfaceEditTarget target;
                                    if (mappedTargetForPart(part, target)) {
                                        materialEditing.OpenDecalTintModal(target);
                                    } else {
                                        statusText = "Authoring side decal tint unavailable: derived mapping is not current";
                                    }
                                }
                                const Rectangle swatchScreen{
                                        scroll.viewport.x + swatchLocal.x,
                                        scroll.viewport.y - uiState.inspectorScroll.offset.y + swatchLocal.y,
                                        swatchLocal.width,
                                        swatchLocal.height};
                                DrawColorSwatch(config, swatchScreen, DecalTintPreviewColor(decal.tint), config.borderThickness);
                                y += rowH + gap;

                                TopologySurfaceEditTarget fitTarget;
                                if (mappedTargetForPart(part, fitTarget)
                                        && engine::Button(
                                                ui,
                                                config,
                                                input,
                                                assets,
                                                TextFormat("%s_%s_fit_decal", idPrefix, suffix),
                                                Rectangle{0.0f, y, contentW, 36.0f},
                                                font,
                                                "Fit Decal")) {
                                    materialEditing.FitSelectedDecal(fitTarget, &assets);
                                }
                                y += 36.0f + gap;
                            };
                    drawTextureRow("wall", "Wall:", TopologyWallPart::Wall);
                    drawTextureRow("lower", "Lower:", TopologyWallPart::Lower);
                    drawTextureRow("upper", "Upper:", TopologyWallPart::Upper);
                    drawTextureRow("middle", "Middle:", TopologyWallPart::Middle);
                    drawDecalControls("wall", "Wall Decal:", TopologyWallPart::Wall);
                    drawDecalControls("lower", "Lower Decal:", TopologyWallPart::Lower);
                    drawDecalControls("upper", "Upper Decal:", TopologyWallPart::Upper);
                };

        drawAuthoringSideSection(SectorTopologySideKind::Front, "Front Side", "sector_editor_authoring_front_side");
        drawAuthoringSideSection(SectorTopologySideKind::Back, "Back Side", "sector_editor_authoring_back_side");
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (selectedAuthoringFaceAnchor != nullptr) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Authoring Face: %d", selectedAuthoringFaceAnchor->id),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;
        const char* anchorText = TextFormat(
                "Anchor %.2f, %.2f",
                SectorCoordToVisibleAuthoring(selectedAuthoringFaceAnchor->x),
                SectorCoordToVisibleAuthoring(selectedAuthoringFaceAnchor->y));
        const float anchorHeight = MeasureSectorEditorWrappedTextHeight(
                smallConfig,
                assets,
                smallFont,
                anchorText,
                contentW);
        engine::Text(
                ui,
                smallConfig,
                assets,
                Rectangle{0.0f, y, contentW, anchorHeight},
                smallFont,
                anchorText,
                engine::UITextJustify::Left,
                smallConfig.mutedTextColor,
                true);
        y += anchorHeight;

        const int faceAnchorId = selectedAuthoringFaceAnchor->id;
        const auto mutateFaceAnchor =
                [this, faceAnchorId](const char* status, const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate) {
                    return MutateSectorEditorAuthoringFaceAnchorById(state, faceAnchorId, status, mutate);
                };

        bool isVoidFace = selectedAuthoringFaceAnchor->isVoid;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_face_void",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Void Face",
                    isVoidFace)) {
            mutateFaceAnchor(
                    "Updated authoring face void state",
                    [isVoidFace](SectorAuthoringFaceAnchor& anchor) {
                        if (anchor.isVoid == isVoidFace) {
                            return false;
                        }
                        anchor.isVoid = isVoidFace;
                        return true;
                    });
        }
        y += rowH + gap;

        auto drawHeight = [&](const char* id, const char* label, float current, engine::UIFloatInputState& inputState, bool floorField) {
            const SectorEditorInspectorNumericRowLayout layout =
                    BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
            const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    id,
                    label,
                    layout.labelRect,
                    layout.inputRect,
                    engine::UITextJustify::Right,
                    current,
                    inputState,
                    -512.0f,
                    512.0f,
                    2);
            if (result.changed && result.value != current) {
                const float nextFloor = floorField ? result.value : selectedAuthoringFaceAnchor->floorZ;
                const float nextCeiling = floorField ? selectedAuthoringFaceAnchor->ceilingZ : result.value;
                if (!std::isfinite(nextFloor) || !std::isfinite(nextCeiling) || nextCeiling <= nextFloor) {
                    statusText = "Invalid authoring face heights: ceiling must be greater than floor";
                } else {
                    mutateFaceAnchor(
                            "Updated authoring face height",
                            [nextFloor, nextCeiling](SectorAuthoringFaceAnchor& anchor) {
                                if (anchor.floorZ == nextFloor && anchor.ceilingZ == nextCeiling) {
                                    return false;
                                }
                                anchor.floorZ = nextFloor;
                                anchor.ceilingZ = nextCeiling;
                                return true;
                            });
                }
            }
            y += rowH + gap;
        };
        drawHeight("sector_editor_authoring_face_floor", "Floor:", selectedAuthoringFaceAnchor->floorZ, uiState.floorInput, true);
        drawHeight("sector_editor_authoring_face_ceiling", "Ceiling:", selectedAuthoringFaceAnchor->ceilingZ, uiState.ceilingInput, false);

        bool ceilingSky = selectedAuthoringFaceAnchor->ceilingSky;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_face_ceiling_sky",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Ceiling Sky",
                    ceilingSky)) {
            mutateFaceAnchor(
                    "Updated authoring face ceiling sky",
                    [ceilingSky](SectorAuthoringFaceAnchor& anchor) {
                        if (anchor.ceilingSky == ceilingSky) {
                            return false;
                        }
                        anchor.ceilingSky = ceilingSky;
                        return true;
                    });
        }
        y += rowH + gap;

        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Lighting", engine::UITextJustify::Left, config.textColor);
        y += 30.0f;

        const float ambientIntensity = std::clamp(selectedAuthoringFaceAnchor->ambientIntensity, 0.0f, 1.0f);
        const SectorEditorInspectorNumericRowLayout ambientLayout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult ambientResult = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_authoring_face_ambient_intensity",
                "Intensity:",
                ambientLayout.labelRect,
                ambientLayout.inputRect,
                engine::UITextJustify::Right,
                ambientIntensity,
                uiState.ambientIntensityInput,
                0.0f,
                1.0f,
                3);
        if (ambientResult.changed && ambientResult.value != selectedAuthoringFaceAnchor->ambientIntensity) {
            mutateFaceAnchor(
                    "Updated authoring face ambient intensity",
                    [value = ambientResult.value](SectorAuthoringFaceAnchor& anchor) {
                        if (anchor.ambientIntensity == value) {
                            return false;
                        }
                        anchor.ambientIntensity = value;
                        return true;
                    });
        }
        y += rowH + gap;

        auto drawAmbientChannel = [&](const char* id, const char* label, unsigned char current, engine::UIIntInputState& inputState, int channel) {
            const float colorLabelW = 92.0f;
            const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    id,
                    label,
                    Rectangle{0.0f, y, colorLabelW, rowH},
                    Rectangle{colorLabelW, y, contentW - colorLabelW, rowH},
                    engine::UITextJustify::Right,
                    current,
                    inputState);
            if (result.changed && result.channel != current) {
                mutateFaceAnchor(
                        "Updated authoring face ambient color",
                        [channel, value = result.channel](SectorAuthoringFaceAnchor& anchor) {
                            Color next = anchor.ambientColor;
                            if (channel == 0) {
                                next.r = value;
                            } else if (channel == 1) {
                                next.g = value;
                            } else {
                                next.b = value;
                            }
                            next.a = 255;
                            if (anchor.ambientColor.r == next.r
                                    && anchor.ambientColor.g == next.g
                                    && anchor.ambientColor.b == next.b
                                    && anchor.ambientColor.a == next.a) {
                                return false;
                            }
                            anchor.ambientColor = next;
                            return true;
                        });
            }
            y += rowH + gap;
        };
        drawAmbientChannel("sector_editor_authoring_face_ambient_r", "R:", selectedAuthoringFaceAnchor->ambientColor.r, uiState.ambientRedInput, 0);
        drawAmbientChannel("sector_editor_authoring_face_ambient_g", "G:", selectedAuthoringFaceAnchor->ambientColor.g, uiState.ambientGreenInput, 1);
        drawAmbientChannel("sector_editor_authoring_face_ambient_b", "B:", selectedAuthoringFaceAnchor->ambientColor.b, uiState.ambientBlueInput, 2);

        const auto drawTextureRow = [&](const char* id, const char* label, const std::string& textureId, TopologySectorTextureField field) {
            const float buttonW = 38.0f;
            const SectorEditorInspectorTextureRowLayout row =
                    BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, 0.0f);
            const bool missing = !textureId.empty() && FindSectorTopologyTexture(state.topologyMap, textureId) == nullptr;
            engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
            engine::Text(
                    ui,
                    smallConfig,
                    assets,
                    row.valueRect,
                    smallFont,
                    textureId.empty() ? "<none>" : textureId.c_str(),
                    engine::UITextJustify::Left,
                    missing ? config.invalidColor : config.mutedTextColor);
            if (engine::Button(ui, config, input, assets, id, row.pickerButtonRect, font, ">")) {
                if (!materialEditing.OpenMaterialPickerForAuthoringFaceAnchor(
                            faceAnchorId,
                            field,
                            TopologyMaterialLayer::Base)) {
                    statusText = "Authoring face texture picker unavailable: derived mapping is not current";
                }
            }
            y += row.height + gap;
        };
        const auto mappedFlatTargetForField = [this, faceAnchorId](TopologySectorTextureField field, TopologySurfaceEditTarget& outTarget) {
            if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                    || state.authoringDerivedTopologyStale
                    || !state.authoringDerivation.success) {
                return false;
            }
            for (const SectorAuthoringDerivedSectorMapping& mapping : state.authoringDerivation.mapping.sectors) {
                if (mapping.faceAnchorId != faceAnchorId) {
                    continue;
                }
                if (FindSectorTopologySector(state.topologyMap, mapping.topologySectorId) == nullptr) {
                    continue;
                }
                if (field == TopologySectorTextureField::Floor) {
                    outTarget.kind = TopologySurfaceEditTargetKind::SectorFloor;
                } else if (field == TopologySectorTextureField::Ceiling) {
                    outTarget.kind = TopologySurfaceEditTargetKind::SectorCeiling;
                } else {
                    return false;
                }
                outTarget.sectorId = mapping.topologySectorId;
                return true;
            }
            return false;
        };
        const auto drawFlatDecalControls =
                [&](const char* idPrefix, const char* label, const SectorTopologyDecalLayer& decal, TopologySectorTextureField field, int inputIndex) {
                    const float buttonW = 38.0f;
                    const float clearW = 92.0f;
                    const SectorEditorInspectorTextureRowLayout row =
                            BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                    const bool missing = !decal.textureId.empty()
                            && FindSectorTopologyTexture(state.topologyMap, decal.textureId) == nullptr;
                    engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
                    engine::Text(
                            ui,
                            smallConfig,
                            assets,
                            row.valueRect,
                            smallFont,
                            decal.textureId.empty() ? "<none>" : decal.textureId.c_str(),
                            engine::UITextJustify::Left,
                            missing ? config.invalidColor : config.mutedTextColor);
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_clear", idPrefix),
                                row.clearButtonRect,
                                font,
                                "Clear")) {
                        mutateFaceAnchor(
                                "Cleared authoring face decal",
                                [field](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = nullptr;
                                    if (field == TopologySectorTextureField::Floor) {
                                        target = &anchor.floorDecal;
                                    } else if (field == TopologySectorTextureField::Ceiling) {
                                        target = &anchor.ceilingDecal;
                                    }
                                    if (target == nullptr || IsDefaultDecalLayer(*target)) {
                                        return false;
                                    }
                                    ResetDecalLayer(*target);
                                    return true;
                                });
                    }
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_pick", idPrefix),
                                row.pickerButtonRect,
                                font,
                                ">")) {
                        if (!materialEditing.OpenMaterialPickerForAuthoringFaceAnchor(
                                    faceAnchorId,
                                    field,
                                    TopologyMaterialLayer::Decal)) {
                            statusText = "Authoring face decal picker unavailable: derived mapping is not current";
                        }
                    }
                    y += row.height + gap;

                    if (decal.textureId.empty()) {
                        return;
                    }

                    const SectorEditorInspectorNumericRowLayout opacityLayout =
                            BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                    const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                            ui,
                            config,
                            input,
                            assets,
                            font,
                            TextFormat("%s_opacity", idPrefix),
                            "Opacity:",
                            opacityLayout.labelRect,
                            opacityLayout.inputRect,
                            engine::UITextJustify::Left,
                            decal.opacity,
                            uiState.topologySectorDecalOpacityInputs[inputIndex],
                            0.0f,
                            1.0f,
                            3);
                    if (opacityResult.changed && opacityResult.value != decal.opacity && opacityResult.finite) {
                        mutateFaceAnchor(
                                "Updated authoring face decal opacity",
                                [field, value = opacityResult.value](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = field == TopologySectorTextureField::Floor
                                            ? &anchor.floorDecal
                                            : &anchor.ceilingDecal;
                                    if (target->textureId.empty() || target->opacity == value) {
                                        return false;
                                    }
                                    target->opacity = value;
                                    return true;
                                });
                    }
                    y += rowH + gap;

                    bool emissive = decal.emissive;
                    if (engine::Checkbox(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_emissive", idPrefix),
                                Rectangle{0.0f, y, contentW, 36.0f},
                                font,
                                "Emissive",
                                emissive)) {
                        mutateFaceAnchor(
                                "Updated authoring face decal emissive",
                                [field, emissive](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = field == TopologySectorTextureField::Floor
                                            ? &anchor.floorDecal
                                            : &anchor.ceilingDecal;
                                    if (target->textureId.empty() || target->emissive == emissive) {
                                        return false;
                                    }
                                    target->emissive = emissive;
                                    return true;
                                });
                    }
                    y += 36.0f + gap;

                    if (decal.emissive) {
                        const SectorEditorInspectorNumericRowLayout bloomLayout =
                                BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                        const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                                ui,
                                config,
                                input,
                                assets,
                                font,
                                TextFormat("%s_bloom", idPrefix),
                                "Bloom:",
                                bloomLayout.labelRect,
                                bloomLayout.inputRect,
                                engine::UITextJustify::Left,
                                decal.bloomIntensity,
                                uiState.topologySectorDecalBloomIntensityInputs[inputIndex],
                                0.0f,
                                10.0f,
                                3);
                        if (bloomResult.changed && bloomResult.value != decal.bloomIntensity && bloomResult.finite) {
                            mutateFaceAnchor(
                                    "Updated authoring face decal bloom intensity",
                                    [field, value = bloomResult.value](SectorAuthoringFaceAnchor& anchor) {
                                        SectorTopologyDecalLayer* target = field == TopologySectorTextureField::Floor
                                                ? &anchor.floorDecal
                                                : &anchor.ceilingDecal;
                                        if (target->textureId.empty() || target->bloomIntensity == value) {
                                            return false;
                                        }
                                        target->bloomIntensity = value;
                                        return true;
                                    });
                        }
                        y += rowH + gap;
                    }

                    engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
                    const Rectangle swatchLocal{82.0f, y + 3.0f, 56.0f, rowH - 6.0f};
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_tint", idPrefix),
                                swatchLocal,
                                font,
                                "")) {
                        TopologySurfaceEditTarget target;
                        if (mappedFlatTargetForField(field, target)) {
                            materialEditing.OpenDecalTintModal(target);
                        } else {
                            statusText = "Authoring face decal tint unavailable: derived mapping is not current";
                        }
                    }
                    const Rectangle swatchScreen{
                            scroll.viewport.x + swatchLocal.x,
                            scroll.viewport.y - uiState.inspectorScroll.offset.y + swatchLocal.y,
                            swatchLocal.width,
                            swatchLocal.height};
                    DrawColorSwatch(config, swatchScreen, DecalTintPreviewColor(decal.tint), config.borderThickness);
                    y += rowH + gap;

                    TopologySurfaceEditTarget fitTarget;
                    if (mappedFlatTargetForField(field, fitTarget)
                            && engine::Button(
                                    ui,
                                    config,
                                    input,
                                    assets,
                                    TextFormat("%s_fit", idPrefix),
                                    Rectangle{0.0f, y, contentW, 36.0f},
                                    font,
                                    "Fit Decal")) {
                        materialEditing.FitSelectedDecal(fitTarget, &assets);
                    }
                    y += 36.0f + gap;
                };
        const auto drawDefaultDecalControls =
                [&](const char* idPrefix, const char* label, const SectorTopologyDecalLayer& decal, TopologySectorTextureField field, int inputIndex) {
                    const float buttonW = 38.0f;
                    const float clearW = 92.0f;
                    const SectorEditorInspectorTextureRowLayout row =
                            BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                    const bool missing = !decal.textureId.empty()
                            && FindSectorTopologyTexture(state.topologyMap, decal.textureId) == nullptr;
                    engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
                    engine::Text(
                            ui,
                            smallConfig,
                            assets,
                            row.valueRect,
                            smallFont,
                            decal.textureId.empty() ? "<none>" : decal.textureId.c_str(),
                            engine::UITextJustify::Left,
                            missing ? config.invalidColor : config.mutedTextColor);
                    auto defaultDecalForField = [field](SectorAuthoringFaceAnchor& anchor) -> SectorTopologyDecalLayer* {
                        if (field == TopologySectorTextureField::DefaultWall) {
                            return &anchor.defaultWall.decal;
                        }
                        if (field == TopologySectorTextureField::DefaultLower) {
                            return &anchor.defaultLower.decal;
                        }
                        if (field == TopologySectorTextureField::DefaultUpper) {
                            return &anchor.defaultUpper.decal;
                        }
                        return nullptr;
                    };
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_clear", idPrefix),
                                row.clearButtonRect,
                                font,
                                "Clear")) {
                        mutateFaceAnchor(
                                "Cleared authoring default decal",
                                [defaultDecalForField](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                    if (target == nullptr || IsDefaultDecalLayer(*target)) {
                                        return false;
                                    }
                                    ResetDecalLayer(*target);
                                    return true;
                                });
                    }
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_pick", idPrefix),
                                row.pickerButtonRect,
                                font,
                                ">")) {
                        if (!materialEditing.OpenMaterialPickerForAuthoringFaceAnchor(
                                    faceAnchorId,
                                    field,
                                    TopologyMaterialLayer::Decal)) {
                            statusText = "Authoring default decal picker unavailable: derived mapping is not current";
                        }
                    }
                    y += row.height + gap;

                    if (decal.textureId.empty()) {
                        return;
                    }

                    const SectorEditorInspectorNumericRowLayout opacityLayout =
                            BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                    const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                            ui,
                            config,
                            input,
                            assets,
                            font,
                            TextFormat("%s_opacity", idPrefix),
                            "Opacity:",
                            opacityLayout.labelRect,
                            opacityLayout.inputRect,
                            engine::UITextJustify::Left,
                            decal.opacity,
                            uiState.topologySectorDecalOpacityInputs[inputIndex],
                            0.0f,
                            1.0f,
                            3);
                    if (opacityResult.changed && opacityResult.value != decal.opacity && opacityResult.finite) {
                        mutateFaceAnchor(
                                "Updated authoring default decal opacity",
                                [defaultDecalForField, value = opacityResult.value](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                    if (target == nullptr || target->textureId.empty() || target->opacity == value) {
                                        return false;
                                    }
                                    target->opacity = value;
                                    return true;
                                });
                    }
                    y += rowH + gap;

                    bool emissive = decal.emissive;
                    if (engine::Checkbox(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_emissive", idPrefix),
                                Rectangle{0.0f, y, contentW, 36.0f},
                                font,
                                "Emissive",
                                emissive)) {
                        mutateFaceAnchor(
                                "Updated authoring default decal emissive",
                                [defaultDecalForField, emissive](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                    if (target == nullptr || target->textureId.empty() || target->emissive == emissive) {
                                        return false;
                                    }
                                    target->emissive = emissive;
                                    return true;
                                });
                    }
                    y += 36.0f + gap;

                    if (decal.emissive) {
                        const SectorEditorInspectorNumericRowLayout bloomLayout =
                                BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                        const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                                ui,
                                config,
                                input,
                                assets,
                                font,
                                TextFormat("%s_bloom", idPrefix),
                                "Bloom:",
                                bloomLayout.labelRect,
                                bloomLayout.inputRect,
                                engine::UITextJustify::Left,
                                decal.bloomIntensity,
                                uiState.topologySectorDecalBloomIntensityInputs[inputIndex],
                                0.0f,
                                10.0f,
                                3);
                        if (bloomResult.changed && bloomResult.value != decal.bloomIntensity && bloomResult.finite) {
                            mutateFaceAnchor(
                                    "Updated authoring default decal bloom intensity",
                                    [defaultDecalForField, value = bloomResult.value](SectorAuthoringFaceAnchor& anchor) {
                                        SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                        if (target == nullptr || target->textureId.empty() || target->bloomIntensity == value) {
                                            return false;
                                        }
                                        target->bloomIntensity = value;
                                        return true;
                                    });
                        }
                        y += rowH + gap;
                    }
                };

        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Materials", engine::UITextJustify::Left, config.textColor);
        y += 30.0f;
        drawTextureRow("sector_editor_authoring_face_pick_floor", "Floor:", selectedAuthoringFaceAnchor->floorTextureId, TopologySectorTextureField::Floor);
        drawTextureRow("sector_editor_authoring_face_pick_ceiling", "Ceiling:", selectedAuthoringFaceAnchor->ceilingTextureId, TopologySectorTextureField::Ceiling);
        drawTextureRow("sector_editor_authoring_face_pick_default_wall", "Wall:", selectedAuthoringFaceAnchor->defaultWall.textureId, TopologySectorTextureField::DefaultWall);
        drawTextureRow("sector_editor_authoring_face_pick_default_lower", "Lower:", selectedAuthoringFaceAnchor->defaultLower.textureId, TopologySectorTextureField::DefaultLower);
        drawTextureRow("sector_editor_authoring_face_pick_default_upper", "Upper:", selectedAuthoringFaceAnchor->defaultUpper.textureId, TopologySectorTextureField::DefaultUpper);
        drawFlatDecalControls("sector_editor_authoring_face_floor_decal", "Floor Decal:", selectedAuthoringFaceAnchor->floorDecal, TopologySectorTextureField::Floor, 0);
        drawFlatDecalControls("sector_editor_authoring_face_ceiling_decal", "Ceiling Decal:", selectedAuthoringFaceAnchor->ceilingDecal, TopologySectorTextureField::Ceiling, 1);
        drawDefaultDecalControls("sector_editor_authoring_face_default_wall_decal", "Wall Decal:", selectedAuthoringFaceAnchor->defaultWall.decal, TopologySectorTextureField::DefaultWall, 0);
        drawDefaultDecalControls("sector_editor_authoring_face_default_lower_decal", "Lower Decal:", selectedAuthoringFaceAnchor->defaultLower.decal, TopologySectorTextureField::DefaultLower, 1);
        drawDefaultDecalControls("sector_editor_authoring_face_default_upper_decal", "Upper Decal:", selectedAuthoringFaceAnchor->defaultUpper.decal, TopologySectorTextureField::DefaultUpper, 0);

        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (selectedAuthoringVertex != nullptr) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Authoring Vertex: %d", selectedAuthoringVertex->id),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                TextFormat(
                        "%.2f, %.2f",
                        SectorCoordToVisibleAuthoring(selectedAuthoringVertex->x),
                        SectorCoordToVisibleAuthoring(selectedAuthoringVertex->y)),
                engine::UITextJustify::Left,
                config.mutedTextColor);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 42.0f}, font, "Selected: none", engine::UITextJustify::Left, config.mutedTextColor);

    // TODO: Add undo/redo.
    // TODO: Add validation issue highlighting.

    engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
    engine::EndPanel(ui, config, panel);
}

bool SectorEditor::DrawTopologySideDefInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap)
{
    const SectorEditorMaterialInspectorCallbacks callbacks{
            [this](int sideDefId, TopologyWallPart wallPart) {
                SelectTopologySideDef(sideDefId, wallPart);
            },
            [this](int lineDefId, bool blocksPlayer) {
                return SetAuthoringLineDefBlocksPlayer(lineDefId, blocksPlayer);
            }};
    SectorEditorMaterialEditingService materialEditing = BuildMaterialEditingService();
    SectorEditorMaterialInspectorContext context{
            ui,
            config,
            input,
            assets,
            font,
            smallFont,
            scroll,
            contentW,
            rowH,
            gap,
            state,
            uiState,
            statusText,
            callbacks,
            materialEditing};
    return DrawTopologySideDefMaterialInspector(context);
}

void SectorEditor::DrawAddMapTextureModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorAddTextureModalCallbacks callbacks{
            [this, &assets]() { CloseAddMapTextureModal(assets); },
            [this, &assets]() { return AddSelectedMapTexture(assets); },
            [this](int pathIndex) { SelectAddMapTexturePath(pathIndex); },
            [this, &assets]() { RefreshAddMapTexturePreview(assets); },
            [this](std::string& error) { return ValidateAddMapTextureId(error); }
    };
    game::DrawAddMapTextureModal(ui, config, input, assets, font, state.addMapTexture, callbacks);
}

void SectorEditor::DrawTexturePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorTexturePickerServiceCallbacks callbacks{
            [this, &assets]() { ApplyTexturePickerSelection(assets); },
            [this]() { return CurrentTextureForPickerTarget(); },
            [this](const std::string& textureId) { return EditorTextureHandleForId(textureId); }
    };
    DrawSectorEditorTexturePickerModal(
            ui,
            config,
            input,
            assets,
            font,
            state.texturePicker,
            state.topologyMap,
            callbacks);
}

void SectorEditor::DrawSpritePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorSpritePickerCallbacks callbacks{
            [this, &assets]() { CloseSpritePicker(state.spritePicker, assets); },
            [this, &assets]() {
                ApplySelectedBillboardSpritePickerSelection();
                CloseSpritePicker(state.spritePicker, assets);
            },
            [this]() {
                RefreshSpritePickerScan(state.spritePicker);
                state.spriteMetadataCatalog.scanned = true;
                state.spriteMetadataCatalog.scanMessage = state.spritePicker.scanMessage;
                state.spriteMetadataCatalog.sprites = state.spritePicker.sprites;
            },
            [this](int spriteIndex) { SelectSpritePickerSprite(state.spritePicker, spriteIndex); },
            [this, &assets]() { RefreshSpritePickerPreview(state.spritePicker, assets); }
    };
    game::DrawSpritePickerModal(ui, config, input, assets, font, state.spritePicker, callbacks);
}

void SectorEditor::DrawSaveLevelModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorSaveLevelModalCallbacks callbacks{
            [this]() { state.saveLevelModal = SaveLevelModalState{}; },
            [this]() { SaveLevelFromModal(); }
    };
    DrawSectorEditorSaveLevelModal(ui, config, input, assets, font, state.saveLevelModal, callbacks);
}

void SectorEditor::DrawLoadLevelModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const auto requestLoad = [this]() {
        LoadLevelModalState& loadState = state.loadLevelModal;
        if (loadState.selectedIndex < 0
                || loadState.selectedIndex >= static_cast<int>(loadState.levels.size())) {
            loadState.errorMessage = "Select a level to load";
            return;
        }
        const LevelListEntry selected = loadState.levels[static_cast<size_t>(loadState.selectedIndex)];
        if (state.topologyDocumentDirty) {
            OpenConfirmation(
                    "Load Level",
                    "Discard unsaved changes and load selected level?",
                    [this, selected]() {
                        if (engineContext != nullptr) {
                            LoadLevel(*engineContext, selected.name, selected.jsonAssetPath);
                        }
                    }
            );
        } else {
            if (engineContext != nullptr) {
                LoadLevel(*engineContext, selected.name, selected.jsonAssetPath);
            }
        }
    };
    const SectorEditorLoadLevelModalCallbacks callbacks{
            [this]() { state.loadLevelModal = LoadLevelModalState{}; },
            requestLoad
    };
    DrawSectorEditorLoadLevelModal(ui, config, input, assets, font, state.loadLevelModal, callbacks);
}

void SectorEditor::DrawConfirmationModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorConfirmationModalCallbacks callbacks{
            [this]() { state.confirmationModal = ConfirmationModalState{}; },
            [this]() {
                std::function<void()> action = std::move(state.confirmationModal.onOkay);
                state.confirmationModal = ConfirmationModalState{};
                if (action) {
                    action();
                }
            }
    };
    DrawSectorEditorConfirmationModal(ui, config, input, assets, font, state.confirmationModal, callbacks);
}

void SectorEditor::DrawDecalTintModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    SectorEditorMaterialEditingService materialEditing = BuildMaterialEditingService();
    const SectorEditorDecalTintModalCallbacks callbacks{
            [this]() { state.decalTintModal = DecalTintModalState{}; },
            [&materialEditing](TopologySurfaceEditTarget target) {
                return materialEditing.IsValidSurfaceTarget(target);
            },
            [&materialEditing](TopologySurfaceEditTarget target) {
                return materialEditing.DecalForSurface(target);
            },
            [&materialEditing, &assets](TopologySurfaceEditTarget target, Vector3 tint) {
                return materialEditing.ApplyDecalTint(target, tint, &assets);
            }
    };
    SectorEditorDecalTintModalContext context{
            ui,
            config,
            input,
            assets,
            font,
            state.decalTintModal,
            statusText,
            callbacks
    };
    DrawSectorEditorDecalTintModal(context);
}

void SectorEditor::DrawDoorTextureSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    const SectorEditorDoorTextureSettingsModalCallbacks callbacks{
            [this]() { return SelectedRuntimeObject(); },
            [this](
                    const char* status,
                    const std::function<bool(SectorPlacedRuntimeObject&)>& mutate) {
                return MutateSelectedRuntimeObject(status, mutate);
            }};
    SectorEditorDoorTextureSettingsModalContext context{
            ui,
            config,
            input,
            assets,
            font,
            smallFont,
            state.doorTextureSettingsModal,
            state.topologyMap,
            state.selectedRuntimeObjectId,
            statusText,
            callbacks};
    DrawSectorEditorDoorTextureSettingsModal(context);
}

void SectorEditor::DrawPreviewSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorPreviewSettingsModalCallbacks callbacks{
            [this]() { state.previewSettingsModal = SectorPreviewSettingsModalState{}; },
            [this, &assets]() { ApplyPreviewSettingsModal(assets); },
            [this]() { OpenMapSkyTexturePicker(); }
    };
    game::DrawPreviewSettingsModal(
            ui,
            config,
            input,
            assets,
            font,
            state.previewSettingsModal,
            state.texturePicker.open,
            callbacks);
}

void SectorEditor::DrawLightmapBakeModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorLightmapBakeModalCallbacks callbacks{
            [this]() { return IsLightmapBakeBlocking(); },
            [this]() { RequestLightmapBakeCancel(); },
            [this]() {
                lightmapBake.modalOpen = false;
                lightmapBake.awaitingAcknowledgement = false;
                lightmapBake.cancelButtonPressed = false;
                lightmapBake.terminalMessage.clear();
                lightmapBake.temporaryOutputPath.clear();
                lightmapBake.progress.phase.store(SectorLightmapBakePhase::Idle);
            }
    };
    game::DrawLightmapBakeModal(ui, config, input, assets, font, lightmapBake, callbacks);
}

void SectorEditor::DrawStatusPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle smallFont)
{
    (void)ui;
    const Rectangle panel = BuildBottomPanelRect();
    DrawRectangleRec(panel, config.panelColor);
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    std::string selectedLabel = "none";
    if (const SectorTopologyStaticPointLight* light = SelectedTopologyLight()) {
        selectedLabel = TextFormat("static light %d", light->id);
    } else if (const SectorTopologyStaticSpotLight* light = SelectedTopologyStaticSpotLight()) {
        selectedLabel = TextFormat("static spot %d", light->id);
    } else if (const SectorTopologyDynamicPointLight* light = SelectedTopologyDynamicLight()) {
        selectedLabel = TextFormat("dynamic light %d", light->id);
    } else if (const SectorTopologyDynamicSpotLight* light = SelectedTopologyDynamicSpotLight()) {
        selectedLabel = TextFormat("dynamic spot %d", light->id);
    } else if (const SectorTopologySector* topologySector = SelectedTopologySector()) {
        selectedLabel = topologySector->name.empty()
                ? TextFormat("topology sector %d", topologySector->id)
                : TextFormat("%s (%d)", topologySector->name.c_str(), topologySector->id);
    } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line) {
        selectedLabel = TextFormat("authoring line %d", state.selectedAuthoring.lineId);
    } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
        selectedLabel = TextFormat("authoring vertex %d", state.selectedAuthoring.vertexId);
    } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor) {
        selectedLabel = TextFormat("authoring face anchor %d", state.selectedAuthoring.faceAnchorId);
    }

    std::string pendingText;
    if (state.pendingAuthoringLine.active) {
        pendingText = " | authoring line";
    } else if (state.pendingAuthoringRectangle.active) {
        pendingText = " | rectangle";
    } else if (state.pendingAuthoringInsertVertex.active) {
        pendingText = " | insert vertex";
    } else if (state.authoringVertexDrag.active) {
        pendingText = " | authoring vertex move";
    }
    const std::string shortMapPath = state.hasCurrentLevelPath
            ? state.currentLevelPath
            : std::string{"<untitled>"};
    const char* lightmapText = SectorLightmapStatusText(GetSectorLightmapStatus(state.topologyMap));
    std::string status = statusText.empty() ? "Ready" : statusText;
    if (!state.topologyRenderWarning.empty()) {
        status += " | ";
        status += state.topologyRenderWarning;
    }
    const char* text = TextFormat(
            "%s%s | %s%s | map %s%s | grid %d | %s | selected %s",
            status.c_str(),
            state.topologyDocumentDirty ? " *modified" : "",
            ToolHelpText(state.currentTool),
            pendingText.c_str(),
            shortMapPath.c_str(),
            state.topologyDocumentDirty ? "*" : "",
            state.gridSize,
            lightmapText,
            selectedLabel.c_str()
    );

    engine::UIConfig statusConfig = SectorEditorSmallFontConfig(config, assets, smallFont);

    engine::Text(
            statusConfig,
            assets,
            Rectangle{panel.x + 18.0f, panel.y, panel.width - 36.0f, panel.height},
            smallFont,
            text,
            engine::UITextJustify::Left,
            statusConfig.textColor,
            true
    );
}

void SectorEditor::ResetToBlankMap(engine::EngineContext& context)
{
    engine::AssetManager& assets = context.assets;
    ShutdownLightmapBake();
    ClearSectorRuntimeObjects(context.world, assets, state.runtimeObjects);
    preview.ShutdownRendererResources(assets);
    if (!engine::IsNull(state.editorTextureScope)) {
        assets.UnloadScope(state.editorTextureScope);
    }
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }

    state = SectorEditorState{};
    uiState = SectorEditorUiState{};
    ResetEditorTopologyDocumentState(state);
    state.viewCenter = Vector2{9.0f, 6.0f};
    state.viewZoom = 48.0f;
    state.gridSize = 8;
    RefreshDefaultTextures();
    RefreshEditorTextureAssets(assets);
    initialized = true;
    statusText = "New blank level";
}

bool SectorEditor::LoadLevel(
        engine::EngineContext& context,
        const std::string& levelName,
        const std::string& jsonAssetPath)
{
    engine::AssetManager& assets = context.assets;
    SectorEditorLoadedDocument loaded;
    if (!LoadSectorEditorDocumentFromAsset(jsonAssetPath, loaded, state.loadLevelModal.errorMessage)) {
        statusText = state.loadLevelModal.errorMessage;
        return false;
    }

    ClearSectorRuntimeObjects(context.world, assets, state.runtimeObjects);
    preview.ShutdownRendererResources(assets);
    CancelAuthoringVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    bool loadedAuthoringGraph = false;
    bool authoringDerivationCurrent = false;
    if (loaded.format == SectorEditorDocumentFormat::AuthoringGraph) {
        loadedAuthoringGraph = true;
        state.topologyMap = std::move(loaded.mapData);
        const SectorLightmapMetadata loadedBakedLightmap = state.topologyMap.bakedLightmap;
        state.authoringGraph = std::move(loaded.authoringGraph);
        state.authoringDerivation = SectorAuthoringDerivationResult{};
        state.lastValidAuthoringDerivedTopology.reset();
        state.authoringDerivationState = SectorEditorAuthoringDerivationState::InvalidNoDerived;
        state.authoringDerivedTopologyStale = true;
        const std::string successStatus = TextFormat(
                "Authoring graph: loaded %s; derived topology current",
                jsonAssetPath.c_str());
        const std::string failureStatus = TextFormat(
                "Authoring graph: loaded %s; derivation failed",
                jsonAssetPath.c_str());
        authoringDerivationCurrent = RefreshSectorEditorAuthoringDerivation(
                state,
                successStatus.c_str(),
                failureStatus.c_str());
        if (authoringDerivationCurrent) {
            state.topologyMap.bakedLightmap = loadedBakedLightmap;
            state.authoringDerivation.topology.bakedLightmap = loadedBakedLightmap;
            if (state.lastValidAuthoringDerivedTopology.has_value()) {
                state.lastValidAuthoringDerivedTopology->bakedLightmap = loadedBakedLightmap;
            }
        }
    } else {
        state.topologyMap = std::move(loaded.mapData);
        InitializeSectorEditorAuthoringStateFromTopology(state, state.topologyMap);
    }
    InvalidateTopologyRenderCache();
    state.fpsControllerConfig = SectorFpsControllerConfigFromPreviewSettings(
            state.topologyMap.previewSettings);
    state.topologyDocumentInitialized = true;
    state.topologyDocumentDirty = false;
    if (!loadedAuthoringGraph) {
        state.topologyDocumentStatus = TextFormat(
                "Topology document: imported legacy topology %s",
                jsonAssetPath.c_str());
    }
    state.currentLevelName = levelName;
    state.currentLevelPath = jsonAssetPath;
    state.hasCurrentLevelPath = true;
    state.hasUnsavedChanges = false;
    state.mode = SectorEditorMode::Edit2D;
    state.hasPreviewPose = false;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.texturePicker = TexturePickerState{};
    state.loadLevelModal = LoadLevelModalState{};
    state.saveLevelModal = SaveLevelModalState{};
    state.confirmationModal = ConfirmationModalState{};
    state.decalTintModal = DecalTintModalState{};
    state.doorTextureSettingsModal = DoorTextureSettingsModalState{};
    ClearSelection();
    state.hoveredTopologyLightId = -1;
    state.hoveredTopologyStaticSpotLightId = -1;
    state.hoveredTopologyDynamicLightId = -1;
    state.hoveredTopologyDynamicSpotLightId = -1;
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.authoringVertexDrag = AuthoringVertexDragState{};
    state.lightDrag = LightDragState{};
    RefreshDefaultTextures();
    RefreshEditorTextureAssets(assets);
    ResetSectorRuntimeObjectsForMap(context.world, assets, state.runtimeObjects, state.topologyMap);
    if (loadedAuthoringGraph) {
        const char* loadedText = authoringDerivationCurrent
                ? "Loaded authoring graph"
                : "Loaded authoring graph with derivation diagnostics";
        statusText = TextFormat("%s %s", loadedText, jsonAssetPath.c_str());
    } else {
        statusText = TextFormat("Imported legacy topology %s", jsonAssetPath.c_str());
    }
    return true;
}

void SectorEditor::OpenConfirmation(const char* title, const char* message, std::function<void()> onOkay)
{
    OpenConfirmationModal(state.confirmationModal, title, message, std::move(onOkay));
}

void SectorEditor::OpenNewConfirmation(engine::AssetManager& assets)
{
    (void)assets;
    OpenConfirmation(
            "New Level",
            "Discard current level and create a new blank map?",
            [this]() {
                if (engineContext != nullptr) {
                    ResetToBlankMap(*engineContext);
                }
            }
    );
}

void SectorEditor::OpenReloadConfirmation(engine::AssetManager& assets)
{
    (void)assets;
    if (!state.hasCurrentLevelPath) {
        statusText = "No saved level to reload.";
        return;
    }
    const std::string name = state.currentLevelName;
    const std::string path = state.currentLevelPath;
    OpenConfirmation(
            "Reload Level",
            "Reload current level from disk and discard unsaved changes?",
            [this, name, path]() {
                if (engineContext != nullptr) {
                    LoadLevel(*engineContext, name, path);
                }
            }
    );
}

void SectorEditor::OpenSaveLevelModal()
{
    OpenSaveLevelModalState(
            state.saveLevelModal,
            state.hasCurrentLevelPath,
            state.currentLevelName);
}

void SectorEditor::RefreshLevelList()
{
    RefreshLoadLevelModalState(state.loadLevelModal);
}

void SectorEditor::OpenLoadLevelModal()
{
    OpenLoadLevelModalState(state.loadLevelModal);
}

bool SectorEditor::SaveLevelFromModal(bool overwriteConfirmed)
{
    SaveLevelModalState& modal = state.saveLevelModal;
    const std::string name = modal.nameBuffer;
    SectorEditorSaveLevelPlan savePlan;
    if (!PrepareSaveLevelPlan(
                name,
                state.hasCurrentLevelPath,
                state.currentLevelPath,
                overwriteConfirmed,
                savePlan,
                modal.errorMessage)) {
        return false;
    }

    if (savePlan.needsOverwriteConfirmation) {
        OpenConfirmation(
                "Overwrite Level",
                "Level already exists. Overwrite it?",
                [this]() { SaveLevelFromModal(true); }
        );
        return false;
    }

    if (!EnsureSaveLevelDirectory(savePlan.paths, modal.errorMessage)) {
        return false;
    }

    if (!SaveSectorEditorAuthoringDocument(savePlan.paths, state, modal.errorMessage)) {
        statusText = TextFormat("Save failed: %s", savePlan.paths.jsonAssetPath.c_str());
        return false;
    }

    state.currentLevelName = name;
    state.currentLevelPath = savePlan.paths.jsonAssetPath;
    state.hasCurrentLevelPath = true;
    state.hasUnsavedChanges = false;
    state.topologyDocumentInitialized = true;
    state.topologyDocumentDirty = false;
    state.topologyDocumentStatus = TextFormat("Authoring graph: saved %s", savePlan.paths.jsonAssetPath.c_str());
    state.saveLevelModal = SaveLevelModalState{};
    state.confirmationModal = ConfirmationModalState{};
    state.decalTintModal = DecalTintModalState{};
    state.doorTextureSettingsModal = DoorTextureSettingsModalState{};
    statusText = TextFormat("Saved authoring graph %s", savePlan.paths.jsonAssetPath.c_str());
    return true;
}

bool SectorEditor::HasDocumentModalOpen() const
{
    return state.saveLevelModal.open
            || state.loadLevelModal.open
            || state.confirmationModal.open
            || state.decalTintModal.open
            || state.doorTextureSettingsModal.open
            || state.previewSettingsModal.open;
}

bool SectorEditor::TryEnterPreview3D(engine::EngineContext& context, engine::UIContext& ui)
{
    engine::AssetManager& assets = context.assets;
    if (!initialized) {
        statusText = "3D mode failed: no map loaded";
        return false;
    }

    CancelAuthoringVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    ui.hotId = 0;
    ui.activeId = 0;
    ui.openOptionId = 0;
    ui.focusedId = 0;
    uiState.keyboardCaptured = false;

    std::string gateMessage;
    if (!CanUseCurrentAuthoringDerivedTopologyForPreview(state, &gateMessage)) {
        statusText = gateMessage.empty() ? "3D mode failed: derived topology is not current" : gateMessage;
        return false;
    }

    std::string error;
    if (!preview.RebuildRendererResources(assets, state.topologyMap, "sector_editor_preview", error)) {
        state.runtimeObjects.objectLightProbes = SectorBakedObjectLightProbeRuntimeData{};
        state.runtimeObjects.objectProbeStatus.clear();
        state.runtimeObjects.objectSectorLookupWorld = SectorCollisionWorld{};
        state.runtimeObjects.objectSectorLookupWorldValid = false;
        state.runtimeObjects.objectSectorLookupWarning.clear();
        state.sectorCollisionWorldValid = false;
        state.sectorCollisionWorldWarning.clear();
        state.previewCollisionSectorId = 0;
        state.fpsControllerState.currentSectorId = 0;
        state.previewVerticalResult = SectorFpsVerticalResult{};
        state.previewMoveResult = SectorCollisionMoveResult{};
        state.previewCollisionNoclipFallback = false;
        state.visualStepOffsetY = 0.0f;
        ClearSectorFpsHeadBob(state.headBobState);
        ClearSectorFpsLandingDip(state.landingDipState);
        state.mode = SectorEditorMode::Edit2D;
        if (StartsWith(error, "Preview failed:")) {
            statusText = std::string{"3D mode failed:"} + error.substr(std::strlen("Preview failed:"));
        } else {
            statusText = error.empty() ? "3D mode failed" : error;
        }
        return false;
    }
    RefreshPreviewObjectProbeDebugData();
    EnsureSectorRuntimeObjectWorldReserved(context.world, state.runtimeObjects);
    SpawnPlacedRuntimeObjects(context.world, assets, state.runtimeObjects, state.topologyMap);

    if (state.hasPreviewPose) {
        preview.ApplyRendererPose(state.lastPreviewPose);
    }

    state.previewControlMode = SectorPreviewControlMode::FreeFly;
    ResetSectorFreeflyController(state.freeflyController, preview.RendererPose());
    EnterSectorFreeflyController(state.freeflyController);
    preview.ApplyRendererPose(state.freeflyController.pose);
    state.visualStepOffsetY = 0.0f;
    ClearSectorFpsHeadBob(state.headBobState);
    ClearSectorFpsLandingDip(state.landingDipState);
    state.fpsControllerConfig = NormalizeSectorFpsControllerConfig(state.fpsControllerConfig);
    state.mode = SectorEditorMode::Preview3D;
    state.previewUiHidden = false;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    RebuildSectorCollisionWorld();
    statusText = TextFormat(
            "3D mode rebuilt: %zu batches, %d triangles",
            preview.BatchCount(),
            preview.TriangleCount()
    );
    return true;
}

void SectorEditor::LeavePreview3D()
{
    CancelSpotLightPilot(nullptr);
    if (state.previewControlMode == SectorPreviewControlMode::Gameplay) {
        ClearSectorFpsLandingDip(state.landingDipState);
        ApplyGameplayPoseToPreview();
    }
    state.lastPreviewPose = ActivePreviewPose();
    state.hasPreviewPose = true;
    state.visualStepOffsetY = 0.0f;
    ClearSectorFpsHeadBob(state.headBobState);
    ClearSectorFpsLandingDip(state.landingDipState);
    state.previewControlMode = SectorPreviewControlMode::FreeFly;
    state.mode = SectorEditorMode::Edit2D;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.previewSettingsModal = SectorPreviewSettingsModalState{};
    LeaveSectorFreeflyController();
    statusText = "Returned to 2D editor";
}

SectorViewPose SectorEditor::ActivePreviewPose() const
{
    return ActiveSectorEditorPreviewPose(state, preview);
}

void SectorEditor::ApplyGameplayPoseToPreview()
{
    ApplySectorEditorGameplayPoseToPreview(state, preview);
}

void SectorEditor::TogglePreviewControlMode()
{
    if (!ToggleSectorEditorPreviewControlMode(state, preview)) {
        return;
    }

    statusText = TextFormat("3D control mode: %s", PreviewControlModeName(state.previewControlMode));
}

bool SectorEditor::StartSpotLightPilot()
{
    if (state.mode != SectorEditorMode::Preview3D || state.previewControlMode != SectorPreviewControlMode::FreeFly) {
        statusText = "Spotlight pilot requires 3D FreeFly mode";
        return false;
    }

    int lightId = -1;
    SpotLightPilotKind pilotKind = SpotLightPilotKind::None;
    Vector3 lightPosition = {};
    Vector3 lightTarget = {};
    float lightRange = 0.0f;
    if (const SectorTopologyStaticSpotLight* light = SelectedTopologyStaticSpotLight()) {
        lightId = light->id;
        pilotKind = SpotLightPilotKind::Static;
        lightPosition = light->position;
        lightTarget = light->target;
        lightRange = light->range;
    } else if (const SectorTopologyDynamicSpotLight* light = SelectedTopologyDynamicSpotLight()) {
        lightId = light->id;
        pilotKind = SpotLightPilotKind::Dynamic;
        lightPosition = light->position;
        lightTarget = light->target;
        lightRange = light->range;
    } else {
        statusText = "Select a spotlight to pilot";
        return false;
    }

    const Vector3 originWorld = SectorAuthoringToWorldPosition(lightPosition);
    const Vector3 targetWorld = SectorAuthoringToWorldPosition(lightTarget);
    float targetDistanceWorld = Vector3Distance(originWorld, targetWorld);
    if (!std::isfinite(targetDistanceWorld) || targetDistanceWorld <= 0.01f) {
        targetDistanceWorld = std::max(SectorAuthoringToWorldDistance(lightRange) * 0.5f, 4.0f);
    }

    state.spotLightPilot.active = true;
    state.spotLightPilot.kind = pilotKind;
    state.spotLightPilot.lightId = lightId;
    state.spotLightPilot.originalPosition = lightPosition;
    state.spotLightPilot.originalTarget = lightTarget;
    state.spotLightPilot.originalPreviewPose = ActivePreviewPose();
    state.spotLightPilot.originalMouseLookEnabled = state.freeflyController.mouseLookEnabled;
    state.spotLightPilot.targetDistanceWorld = targetDistanceWorld;

    const SectorViewPose pilotPose = PreviewPoseLookingAt(originWorld, targetWorld);
    ResetSectorFreeflyController(state.freeflyController, pilotPose);
    EnterSectorFreeflyController(state.freeflyController);
    preview.ApplyRendererPose(state.freeflyController.pose);
    state.hoveredSurface3D = SectorSurfaceHit{};
    statusText = pilotKind == SpotLightPilotKind::Static
            ? TextFormat("Piloting static spot %d", lightId)
            : TextFormat("Piloting dynamic spot %d", lightId);
    return true;
}

bool SectorEditor::ApplySpotLightPilot()
{
    if (!state.spotLightPilot.active) {
        return false;
    }

    const SpotLightPilotState pilot = state.spotLightPilot;
    SectorTopologyStaticSpotLight* staticLight = pilot.kind == SpotLightPilotKind::Static
            ? FindSectorTopologyStaticSpotLight(state.topologyMap, pilot.lightId)
            : nullptr;
    SectorTopologyDynamicSpotLight* dynamicLight = pilot.kind == SpotLightPilotKind::Dynamic
            ? FindSectorTopologyDynamicSpotLight(state.topologyMap, pilot.lightId)
            : nullptr;
    if (staticLight == nullptr && dynamicLight == nullptr) {
        CancelSpotLightPilot("Spotlight pilot cancelled: light missing");
        return false;
    }

    const int lightId = staticLight != nullptr ? staticLight->id : dynamicLight->id;
    const SectorViewPose pose = ActivePreviewPose();
    const Vector3 forward = PreviewForwardFromPose(pose);
    const Vector3 targetWorld = Vector3Add(
            pose.position,
            Vector3Scale(forward, pilot.targetDistanceWorld));
    if (staticLight != nullptr) {
        staticLight->position = SectorWorldToAuthoringPosition(pose.position);
        staticLight->target = SectorWorldToAuthoringPosition(targetWorld);
    } else {
        dynamicLight->position = SectorWorldToAuthoringPosition(pose.position);
        dynamicLight->target = SectorWorldToAuthoringPosition(targetWorld);
    }
    state.spotLightPilot = SpotLightPilotState{};
    MarkTopologyDocumentEdited(staticLight != nullptr
            ? TextFormat("Piloted static spot %d", lightId)
            : TextFormat("Piloted dynamic spot %d", lightId));
    if (dynamicLight != nullptr) {
        preview.RefreshDynamicLightSources(state.topologyMap);
    }
    statusText = staticLight != nullptr
            ? TextFormat("Applied static spot %d pilot pose", lightId)
            : TextFormat("Applied dynamic spot %d pilot pose", lightId);
    return true;
}

void SectorEditor::CancelSpotLightPilot(const char* message)
{
    if (!state.spotLightPilot.active) {
        return;
    }

    const SpotLightPilotState pilot = state.spotLightPilot;
    state.spotLightPilot = SpotLightPilotState{};
    if (pilot.kind == SpotLightPilotKind::Static) {
        if (SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(state.topologyMap, pilot.lightId)) {
            light->position = pilot.originalPosition;
            light->target = pilot.originalTarget;
        }
    } else if (SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(state.topologyMap, pilot.lightId)) {
        light->position = pilot.originalPosition;
        light->target = pilot.originalTarget;
    }
    if (state.mode == SectorEditorMode::Preview3D) {
        ResetSectorFreeflyController(state.freeflyController, pilot.originalPreviewPose);
        SetSectorFreeflyMouseLookEnabled(state.freeflyController, pilot.originalMouseLookEnabled);
        preview.ApplyRendererPose(state.freeflyController.pose);
    }
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

bool SectorEditor::RebuildSectorCollisionWorld()
{
    return RebuildSectorEditorCollisionWorld(state);
}

SectorFpsVerticalContext SectorEditor::BuildGameplayVerticalContext()
{
    return BuildSectorEditorGameplayVerticalContext(state);
}

void SectorEditor::RefreshGameplaySectorAndVerticalContext()
{
    RefreshSectorEditorGameplaySectorAndVerticalContext(state);
}

void SectorEditor::InitializeGameplayVerticalState()
{
    InitializeSectorEditorGameplayVerticalState(state);
}

void SectorEditor::OpenPreviewSettingsModal()
{
    state.previewSettingsModal = SectorPreviewSettingsModalState{};
    state.previewSettingsModal.open = true;
    state.previewSettingsModal.draftConfig = NormalizeSectorFpsControllerConfig(state.fpsControllerConfig);
    state.previewSettingsModal.draftSkySettings = NormalizeSectorTopologySkySettings(state.topologyMap.skySettings);
    state.previewSettingsModal.draftDirectionalLight =
            NormalizeSectorTopologyDirectionalLightSettings(state.topologyMap.directionalLight);
    state.previewSettingsModal.draftLightmapSettings =
            NormalizeSectorPreviewObjectProbeSettings(state.topologyMap.lightmapSettings);
}

void SectorEditor::ApplyPreviewSettingsModal(engine::AssetManager& assets)
{
    if (!state.previewSettingsModal.open) {
        return;
    }

    const SectorFpsControllerConfig draftConfig = NormalizeSectorFpsControllerConfig(
            state.previewSettingsModal.draftConfig);
    SectorPreviewSettings draftPreviewSettings = SectorPreviewSettingsFromFpsControllerConfig(draftConfig);
    draftPreviewSettings.objectProbeDebugDrawMaxDistanceWorld =
            NormalizeSectorPreviewSettings(
                    state.topologyMap.previewSettings).objectProbeDebugDrawMaxDistanceWorld;
    const SectorTopologySkySettings draftSkySettings = NormalizeSectorTopologySkySettings(
            state.previewSettingsModal.draftSkySettings);
    const SectorTopologyDirectionalLightSettings draftDirectionalLight =
            NormalizeSectorTopologyDirectionalLightSettings(
                    state.previewSettingsModal.draftDirectionalLight);
    const SectorLightmapBakeSettings draftLightmapSettings =
            NormalizeSectorPreviewObjectProbeSettings(state.previewSettingsModal.draftLightmapSettings);
    const bool previewChanged = !SamePreviewSettings(
            state.topologyMap.previewSettings,
            draftPreviewSettings);
    const bool skyChanged = !SameSkySettings(state.topologyMap.skySettings, draftSkySettings);
    const bool directionalChanged = !SameDirectionalLightSettings(
            state.topologyMap.directionalLight,
            draftDirectionalLight);
    const SectorLightmapBakeSettings currentLightmapSettings =
            NormalizeSectorPreviewObjectProbeSettings(state.topologyMap.lightmapSettings);
    const bool objectProbeSettingsChanged =
            currentLightmapSettings.objectProbeSpacingWorld != draftLightmapSettings.objectProbeSpacingWorld
            || currentLightmapSettings.objectProbeHeightWorld != draftLightmapSettings.objectProbeHeightWorld;
    if (!previewChanged && !skyChanged && !directionalChanged && !objectProbeSettingsChanged) {
        state.previewSettingsModal = SectorPreviewSettingsModalState{};
        statusText = "Preview settings unchanged";
        return;
    }

    const float previousGravity = NormalizeSectorFpsControllerConfig(state.fpsControllerConfig).gravity;
    state.fpsControllerConfig = draftConfig;
    if (previousGravity > 0.0f && state.fpsControllerConfig.gravity == 0.0f) {
        state.fpsControllerState.verticalVelocity = 0.0f;
    }
    state.topologyMap.previewSettings = draftPreviewSettings;
    state.topologyMap.skySettings = draftSkySettings;
    state.topologyMap.directionalLight = draftDirectionalLight;
    ApplySectorPreviewObjectProbeSettings(state.topologyMap, draftLightmapSettings);
    MarkTopologyDocumentEdited("Preview settings updated");
    state.previewSettingsModal = SectorPreviewSettingsModalState{};
    if (skyChanged && state.mode == SectorEditorMode::Preview3D && preview.IsRendererReady()) {
        if (engineContext != nullptr) {
            RebuildPreviewMeshesPreservingView(*engineContext);
        }
    }
    if (state.mode == SectorEditorMode::Preview3D
            && state.previewControlMode == SectorPreviewControlMode::Gameplay
            && preview.IsRendererReady()) {
        state.previewVerticalResult = UpdateSectorFpsVerticalPhysics(
                state.fpsControllerState,
                state.fpsControllerConfig,
                BuildGameplayVerticalContext(),
                0.0f);
        state.visualStepOffsetY = 0.0f;
        ClearSectorFpsHeadBob(state.headBobState);
        ClearSectorFpsLandingDip(state.landingDipState);
        ApplyGameplayPoseToPreview();
    }
    statusText = TextFormat(
            "Preview settings updated: walk %.1f run %.1f eye %.1f gravity %.1f radius %.2f height %.2f step %.2f jump %.2f bob %.3f freq %.1f",
            state.fpsControllerConfig.walkSpeed,
            state.fpsControllerConfig.runSpeed,
            state.fpsControllerConfig.eyeHeight,
            state.fpsControllerConfig.gravity,
            state.fpsControllerConfig.playerRadius,
            state.fpsControllerConfig.playerHeight,
            state.fpsControllerConfig.stepHeight,
            state.fpsControllerConfig.jumpHeight,
            state.fpsControllerConfig.headBobStrength,
            state.fpsControllerConfig.headBobFrequency);
}

void SectorEditor::RefreshDefaultTextures()
{
    auto findTexture = [this](const char* preferred, const std::string& fallback = std::string{}) {
        const auto preferredIt = state.topologyMap.texturesById.find(preferred);
        if (preferredIt != state.topologyMap.texturesById.end()) {
            return preferredIt->first;
        }
        if (!fallback.empty()) {
            return fallback;
        }
        const std::vector<std::string> textureIds = SortedSectorTopologyTextureIds(state.topologyMap);
        return textureIds.empty() ? std::string{} : textureIds.front();
    };

    state.defaultWallTextureId = findTexture("wall");
    state.defaultFloorTextureId = findTexture("floor");
    state.defaultCeilingTextureId = findTexture("ceiling");
    state.defaultLowerWallTextureId = findTexture("step_wall", state.defaultWallTextureId);
    state.defaultUpperWallTextureId = findTexture("upper_wall", state.defaultWallTextureId);
}

void SectorEditor::RefreshEditorTextureAssets(engine::AssetManager& assets)
{
    if (!engine::IsNull(state.editorTextureScope)) {
        assets.UnloadScope(state.editorTextureScope);
        state.editorTextureScope = engine::NullAssetScopeHandle();
    }
    state.editorTextureHandlesById.clear();

    if (state.topologyMap.texturesById.empty()) {
        return;
    }

    state.editorTextureScope = assets.CreateScope("sector_editor_textures");
    if (engine::IsNull(state.editorTextureScope)) {
        return;
    }

    for (const std::string& textureId : SortedSectorTopologyTextureIds(state.topologyMap)) {
        const SectorTextureDefinition* texture = FindSectorTopologyTexture(state.topologyMap, textureId);
        if (texture == nullptr) {
            continue;
        }

        const std::string resolvedPath = ResolveEditorAssetPath(texture->path);
        state.editorTextureHandlesById.emplace(
                texture->id,
                assets.RequestTexture(
                        state.editorTextureScope,
                        texture->id.c_str(),
                        resolvedPath.c_str(),
                        SectorTextureLoadFlags(texture->filter)
                )
        );
    }
}

engine::TextureHandle SectorEditor::EditorTextureHandleForId(const std::string& textureId) const
{
    const auto it = state.editorTextureHandlesById.find(textureId);
    return it == state.editorTextureHandlesById.end() ? engine::NullTextureHandle() : it->second;
}

void SectorEditor::OpenAddMapTextureModal(engine::AssetManager& assets)
{
    CloseAddMapTextureModal(assets);
    state.addMapTexture.open = true;
    RefreshAddMapTextureScan();
    SelectAddMapTexturePath(state.addMapTexture.selectedPathIndex);
    statusText = "Add topology texture";
}

void SectorEditor::CloseAddMapTextureModal(engine::AssetManager& assets)
{
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }
    state.addMapTexture = AddMapTextureState{};
}

void SectorEditor::RefreshAddMapTextureScan()
{
    game::RefreshAddMapTextureScan(state.addMapTexture);
}

void SectorEditor::SelectAddMapTexturePath(int pathIndex)
{
    game::SelectAddMapTexturePath(state.addMapTexture, state.topologyMap, pathIndex);
}

void SectorEditor::RefreshAddMapTexturePreview(engine::AssetManager& assets)
{
    game::RefreshAddMapTexturePreview(state.addMapTexture, assets);
}

bool SectorEditor::ValidateAddMapTextureId(std::string& error) const
{
    return game::ValidateAddMapTextureId(state.addMapTexture, error);
}

bool SectorEditor::AddSelectedMapTexture(engine::AssetManager& assets)
{
    const SectorEditorAddTextureResult result = game::AddSelectedMapTexture(state);
    if (!result.success) {
        return false;
    }

    RefreshEditorTextureAssets(assets);
    state.hasUnsavedChanges = true;
    state.topologyDocumentDirty = true;
    statusText = TextFormat("%s texture %s", result.replacing ? "Updated" : "Added", result.textureId.c_str());
    CloseAddMapTextureModal(assets);
    return true;
}

bool SectorEditor::PointInTopologyLoop(Vector2 mapPoint, const SectorTopologyLoop& loop) const
{
    std::vector<SectorPoint> points;
    points.reserve(loop.vertexIds.size());
    for (int vertexId : loop.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
        if (vertex == nullptr) {
            return false;
        }
        points.push_back(Vector2ToSectorPoint(SectorTopologyVertexToMap(*vertex)));
    }
    const SectorPoint point = Vector2ToSectorPoint(mapPoint);
    return StrictPointInPolygon(point, points) || PointOnPolygonBoundary(point, points);
}

bool SectorEditor::PointInTopologySector(Vector2 mapPoint, const SectorTopologySector& sector) const
{
    SectorTopologyLoopSet loops;
    std::vector<SectorTopologyValidationIssue> loopIssues;
    if (!ExtractSectorTopologyLoops(state.topologyMap, sector.id, loops, &loopIssues)) {
        return false;
    }
    if (!PointInTopologyLoop(mapPoint, loops.outer)) {
        return false;
    }
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (PointInTopologyLoop(mapPoint, hole)) {
            return false;
        }
    }
    return true;
}

int SectorEditor::FindTopologySectorAt(Vector2 mapPoint, bool* outMultipleMatches) const
{
    if (outMultipleMatches != nullptr) {
        *outMultipleMatches = false;
    }

    int selectedId = -1;
    int matchCount = 0;
    for (const SectorTopologySector& sector : state.topologyMap.sectors) {
        if (!PointInTopologySector(mapPoint, sector)) {
            continue;
        }
        ++matchCount;
        if (selectedId < 0 || sector.id < selectedId) {
            selectedId = sector.id;
        }
    }

    if (outMultipleMatches != nullptr) {
        *outMultipleMatches = matchCount > 1;
    }
    return selectedId;
}

int SectorEditor::FindTopologyLightNearScreenPoint(Vector2 screenPoint) const
{
    float bestDistance2 = ScreenLightPickPixels * ScreenLightPickPixels;
    int bestId = -1;
    for (const SectorTopologyStaticPointLight& light : state.topologyMap.staticLights) {
        const Vector2 center = MapToScreen(Vector2{light.position.x, light.position.z});
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 < bestDistance2 - 0.001f
                || (std::fabs(distance2 - bestDistance2) <= 0.001f
                        && (bestId < 0 || light.id < bestId))) {
            bestDistance2 = distance2;
            bestId = light.id;
        }
    }
    return bestId;
}

int SectorEditor::FindTopologyStaticSpotLightNearScreenPoint(Vector2 screenPoint) const
{
    float bestDistance2 = ScreenLightPickPixels * ScreenLightPickPixels;
    int bestId = -1;
    for (const SectorTopologyStaticSpotLight& light : state.topologyMap.staticSpotLights) {
        const Vector2 center = MapToScreen(Vector2{light.position.x, light.position.z});
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 < bestDistance2 - 0.001f
                || (std::fabs(distance2 - bestDistance2) <= 0.001f
                        && (bestId < 0 || light.id < bestId))) {
            bestDistance2 = distance2;
            bestId = light.id;
        }
    }
    return bestId;
}

int SectorEditor::FindTopologyDynamicLightNearScreenPoint(Vector2 screenPoint) const
{
    float bestDistance2 = ScreenLightPickPixels * ScreenLightPickPixels;
    int bestId = -1;
    for (const SectorTopologyDynamicPointLight& light : state.topologyMap.dynamicPointLights) {
        const Vector2 center = MapToScreen(Vector2{light.position.x, light.position.z});
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 < bestDistance2 - 0.001f
                || (std::fabs(distance2 - bestDistance2) <= 0.001f
                        && (bestId < 0 || light.id < bestId))) {
            bestDistance2 = distance2;
            bestId = light.id;
        }
    }
    return bestId;
}

int SectorEditor::FindTopologyDynamicSpotLightNearScreenPoint(Vector2 screenPoint) const
{
    float bestDistance2 = ScreenLightPickPixels * ScreenLightPickPixels;
    int bestId = -1;
    for (const SectorTopologyDynamicSpotLight& light : state.topologyMap.dynamicSpotLights) {
        const Vector2 center = MapToScreen(Vector2{light.position.x, light.position.z});
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 < bestDistance2 - 0.001f
                || (std::fabs(distance2 - bestDistance2) <= 0.001f
                        && (bestId < 0 || light.id < bestId))) {
            bestDistance2 = distance2;
            bestId = light.id;
        }
    }
    return bestId;
}

bool SectorEditor::FindTopologyStaticSpotLightHandleNearScreenPoint(
        Vector2 screenPoint,
        int& outLightId,
        SpotLightHandle& outHandle) const
{
    float bestDistance2 = ScreenLightPickPixels * ScreenLightPickPixels;
    outLightId = -1;
    outHandle = SpotLightHandle::Origin;

    auto considerHandle = [&](int lightId, SpotLightHandle handle, Vector2 center) {
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 < bestDistance2 - 0.001f
                || (std::fabs(distance2 - bestDistance2) <= 0.001f
                        && (outLightId < 0
                                || handle == SpotLightHandle::Target
                                || lightId < outLightId))) {
            bestDistance2 = distance2;
            outLightId = lightId;
            outHandle = handle;
        }
    };

    for (const SectorTopologyStaticSpotLight& light : state.topologyMap.staticSpotLights) {
        considerHandle(
                light.id,
                SpotLightHandle::Target,
                MapToScreen(Vector2{light.target.x, light.target.z}));
        considerHandle(
                light.id,
                SpotLightHandle::Origin,
                MapToScreen(Vector2{light.position.x, light.position.z}));
    }

    return outLightId >= 0;
}

bool SectorEditor::FindTopologyDynamicSpotLightHandleNearScreenPoint(
        Vector2 screenPoint,
        int& outLightId,
        SpotLightHandle& outHandle) const
{
    float bestDistance2 = ScreenLightPickPixels * ScreenLightPickPixels;
    outLightId = -1;
    outHandle = SpotLightHandle::Origin;

    auto considerHandle = [&](int lightId, SpotLightHandle handle, Vector2 center) {
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 < bestDistance2 - 0.001f
                || (std::fabs(distance2 - bestDistance2) <= 0.001f
                        && (outLightId < 0
                                || handle == SpotLightHandle::Target
                                || lightId < outLightId))) {
            bestDistance2 = distance2;
            outLightId = lightId;
            outHandle = handle;
        }
    };

    for (const SectorTopologyDynamicSpotLight& light : state.topologyMap.dynamicSpotLights) {
        considerHandle(
                light.id,
                SpotLightHandle::Target,
                MapToScreen(Vector2{light.target.x, light.target.z}));
        considerHandle(
                light.id,
                SpotLightHandle::Origin,
                MapToScreen(Vector2{light.position.x, light.position.z}));
    }

    return outLightId >= 0;
}

int SectorEditor::FindRuntimeObjectNearScreenPoint(Vector2 screenPoint) const
{
    float bestDistance2 = ScreenLightPickPixels * ScreenLightPickPixels;
    int bestId = -1;
    for (const SectorPlacedRuntimeObject& object : state.topologyMap.runtimeObjects) {
        const Vector2 center = MapToScreen(Vector2{object.position.x, object.position.z});
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 < bestDistance2 - 0.001f
                || (std::fabs(distance2 - bestDistance2) <= 0.001f
                        && (bestId < 0 || object.id < bestId))) {
            bestDistance2 = distance2;
            bestId = object.id;
        }
    }
    return bestId;
}

bool SectorEditor::FindTopologyLineNearScreenPoint(
        Vector2 screenPoint,
        Vector2 mapPoint,
        int& outLineDefId,
        int& outSideDefId,
        SectorTopologySideKind& outSide,
        bool& outPreferredMissing) const
{
    outLineDefId = -1;
    outSideDefId = -1;
    outSide = SectorTopologySideKind::Front;
    outPreferredMissing = false;

    float bestDistance = ScreenEdgePickPixels;
    const SectorTopologyLineDef* bestLine = nullptr;
    Vector2 bestStart{};
    Vector2 bestEnd{};

    for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
        const SectorTopologyVertex* start = nullptr;
        const SectorTopologyVertex* end = nullptr;
        if (!GetSectorTopologyLineVertices(state.topologyMap, lineDef, start, end)) {
            continue;
        }

        const Vector2 screenStart = MapToScreen(SectorTopologyVertexToMap(*start));
        const Vector2 screenEnd = MapToScreen(SectorTopologyVertexToMap(*end));
        const float distance = DistancePointToSegment(screenPoint, screenStart, screenEnd);
        if (distance > ScreenEdgePickPixels) {
            continue;
        }
        if (bestLine != nullptr
                && (distance > bestDistance + 0.001f
                        || (std::fabs(distance - bestDistance) <= 0.001f
                                && lineDef.id >= bestLine->id))) {
            continue;
        }

        bestDistance = distance;
        bestLine = &lineDef;
        bestStart = SectorTopologyVertexToMap(*start);
        bestEnd = SectorTopologyVertexToMap(*end);
    }

    if (bestLine == nullptr) {
        return false;
    }

    const bool hasFront = bestLine->frontSideDefId >= 0
            && FindSectorTopologySideDef(state.topologyMap, bestLine->frontSideDefId) != nullptr;
    const bool hasBack = bestLine->backSideDefId >= 0
            && FindSectorTopologySideDef(state.topologyMap, bestLine->backSideDefId) != nullptr;

    SectorTopologySideKind preferredSide = SectorTopologySideKind::Front;
    const float sideCross = Cross(bestStart, bestEnd, mapPoint);
    if (sideCross > GeometryEpsilon) {
        preferredSide = SectorTopologySideKind::Front;
    } else if (sideCross < -GeometryEpsilon) {
        preferredSide = SectorTopologySideKind::Back;
    } else if (!hasFront && hasBack) {
        preferredSide = SectorTopologySideKind::Back;
    }

    const int preferredSideDefId = preferredSide == SectorTopologySideKind::Front
            ? bestLine->frontSideDefId
            : bestLine->backSideDefId;
    const int oppositeSideDefId = preferredSide == SectorTopologySideKind::Front
            ? bestLine->backSideDefId
            : bestLine->frontSideDefId;
    const bool preferredExists = preferredSide == SectorTopologySideKind::Front ? hasFront : hasBack;
    const bool oppositeExists = preferredSide == SectorTopologySideKind::Front ? hasBack : hasFront;

    outLineDefId = bestLine->id;
    if (preferredExists) {
        outSide = preferredSide;
        outSideDefId = preferredSideDefId;
    } else if (oppositeExists) {
        outSide = OppositeSectorTopologySideKind(preferredSide);
        outSideDefId = oppositeSideDefId;
        outPreferredMissing = true;
    } else {
        outSide = preferredSide;
        outSideDefId = -1;
    }
    return true;
}

int SectorEditor::FindAuthoringLineNearScreenPoint(Vector2 screenPoint) const
{
    int lineId = -1;
    const float maxDistance = SectorWorldToAuthoringDistance(
            ScreenEdgePickPixels / std::max(1.0f, state.viewZoom));
    if (!FindSectorEditorAuthoringLineNearMapPoint(
                state.authoringGraph,
                ScreenToMap(screenPoint),
                maxDistance,
                &lineId)) {
        return -1;
    }
    return lineId;
}

bool SectorEditor::FindAuthoringVertexNearScreenPoint(
        Vector2 screenPoint,
        int& outVertexId,
        SectorTopologyCoordPoint& outPoint) const
{
    outVertexId = -1;
    outPoint = SectorTopologyCoordPoint{};

    float bestDistance2 = ScreenVertexSnapPixels * ScreenVertexSnapPixels;
    int bestVertexId = -1;
    SectorTopologyCoordPoint bestPoint{};
    for (const SectorAuthoringVertex& vertex : state.authoringGraph.vertices) {
        const Vector2 screenVertex = MapToScreen(Vector2{
                SectorCoordToVisibleAuthoring(vertex.x),
                SectorCoordToVisibleAuthoring(vertex.y)});
        const float dx = screenVertex.x - screenPoint.x;
        const float dy = screenVertex.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > bestDistance2) {
            continue;
        }
        if (bestVertexId >= 0
                && std::fabs(distance2 - bestDistance2) <= 0.001f
                && vertex.id >= bestVertexId) {
            continue;
        }
        bestDistance2 = distance2;
        bestVertexId = vertex.id;
        bestPoint = SectorTopologyCoordPoint{vertex.x, vertex.y};
    }

    if (bestVertexId < 0) {
        return false;
    }
    outVertexId = bestVertexId;
    outPoint = bestPoint;
    return true;
}

bool SectorEditor::FindAuthoringSelectionNearScreenPoint(
        Vector2 screenPoint,
        SectorAuthoringSelectionTarget& outTarget,
        SectorTopologyCoordPoint& outVertexPoint) const
{
    const float vertexMaxDistance = SectorWorldToAuthoringDistance(
            ScreenVertexSnapPixels / std::max(1.0f, state.viewZoom));
    const float lineMaxDistance = SectorWorldToAuthoringDistance(
            ScreenEdgePickPixels / std::max(1.0f, state.viewZoom));
    return FindSectorEditorAuthoringSelectionAtMapPoint(
            state,
            ScreenToMap(screenPoint),
            vertexMaxDistance,
            lineMaxDistance,
            &outTarget,
            &outVertexPoint);
}

void SectorEditor::SelectTopologySector(int sectorId)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorTopologySector(context, sectorId);
}

void SectorEditor::SelectTopologyVertex(int vertexId)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorTopologyVertex(context, vertexId);
}

void SectorEditor::SelectTopologySideDef(int sideDefId, TopologyWallPart wallPart)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorTopologySideDef(context, sideDefId, wallPart);
}

void SectorEditor::SelectTopologyLineDef(
        int lineDefId,
        SectorTopologySideKind side,
        TopologyWallPart wallPart)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorTopologyLineDef(context, lineDefId, side, wallPart);
}

void SectorEditor::SelectTopologyLight(int topologyLightId)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorTopologyLight(context, topologyLightId);
}

void SectorEditor::SelectTopologyStaticSpotLight(int topologyLightId)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorTopologyStaticSpotLight(context, topologyLightId);
}

void SectorEditor::SelectTopologyDynamicLight(int topologyLightId)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorTopologyDynamicLight(context, topologyLightId);
}

void SectorEditor::SelectTopologyDynamicSpotLight(int topologyLightId)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorTopologyDynamicSpotLight(context, topologyLightId);
}

void SectorEditor::SelectRuntimeObject(int objectId)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorRuntimeObject(context, objectId);
}

void SectorEditor::SelectSurface3D(SectorSurfaceRef surface)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorSurface3D(context, surface);
}

bool SectorEditor::IsValidSurfaceRef(SectorSurfaceRef surface) const
{
    SectorEditorSelectionServiceContext context{
            const_cast<SectorEditorState&>(state),
            const_cast<SectorEditorUiState&>(uiState),
            nullptr,
            nullptr,
            nullptr};
    return IsValidSectorEditorSurfaceRef(context, surface);
}

bool SectorEditor::SameSurfaceRef(SectorSurfaceRef a, SectorSurfaceRef b) const
{
    return SameSectorEditorSurfaceRef(a, b);
}

TopologySurfaceEditTarget SectorEditor::TopologyEditTargetForSurface(SectorSurfaceRef surface) const
{
    return SectorEditorTopologyEditTargetForSurface(surface);
}

bool SectorEditor::IsValidTopologySurfaceEditTarget(TopologySurfaceEditTarget target) const
{
    SectorEditorSelectionServiceContext context{
            const_cast<SectorEditorState&>(state),
            const_cast<SectorEditorUiState&>(uiState),
            nullptr,
            nullptr,
            nullptr};
    return IsValidSectorEditorTopologySurfaceEditTarget(context, target);
}

void SectorEditor::ResetSurface3DUiState()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    ResetSectorEditorSurface3DUiState(context);
}

void SectorEditor::ClearTopologySelectionOnly()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    ClearSectorEditorTopologySelectionOnly(context);
}

void SectorEditor::ClearSelection()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    ClearSectorEditorSelection(context);
}

void SectorEditor::SelectAuthoringLine(int lineId)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorAuthoringLineTarget(context, lineId);
}

bool SectorEditor::DeleteSelectedAuthoringLine()
{
    const int lineId = state.selectedAuthoring.lineId;
    if (!DeleteSectorEditorSelectedAuthoringLine(state)) {
        statusText = "Select an authoring line to delete.";
        return false;
    }

    statusText = TextFormat("Deleted authoring line %d", lineId);
    return true;
}

void SectorEditor::SelectAuthoringVertex(int vertexId)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorAuthoringVertexTarget(context, vertexId);
}

void SectorEditor::SelectAuthoringFaceAnchor(int faceAnchorId)
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    SelectSectorEditorAuthoringFaceAnchorTarget(context, faceAnchorId);
}

bool SectorEditor::DeleteSelectedAuthoringVertex()
{
    const int vertexId = state.selectedAuthoring.vertexId;
    if (!DeleteSectorEditorSelectedAuthoringVertex(state)) {
        statusText = state.authoringDerivationStatus.empty()
                ? "Select an isolated authoring vertex to delete."
                : state.authoringDerivationStatus;
        return false;
    }

    statusText = TextFormat("Deleted authoring vertex %d", vertexId);
    return true;
}

bool SectorEditor::HasAuthoringGraphData() const
{
    return !state.authoringGraph.vertices.empty()
            || !state.authoringGraph.lines.empty()
            || !state.authoringGraph.lineSides.empty()
            || !state.authoringGraph.faceAnchors.empty();
}

bool SectorEditor::EnsureSelectedSurface3DAuthoringMappingCurrent()
{
    std::string unavailableStatus;
    if (ClearSelectedSectorEditorSurface3DIfAuthoringMappingUnavailable(
                state,
                &unavailableStatus)) {
        return true;
    }
    ResetSurface3DUiState();
    statusText = unavailableStatus;
    return false;
}

SectorEditorMaterialEditingService SectorEditor::BuildMaterialEditingService()
{
    return SectorEditorMaterialEditingService{
            SectorEditorMaterialEditingServiceContext{
                    state,
                    uiState,
                    state.texturePicker,
                    statusText,
                    [this](engine::AssetManager*) {
                        if (state.mode == SectorEditorMode::Preview3D
                                && preview.IsRendererReady()
                                && engineContext != nullptr) {
                            return RebuildPreviewMeshesPreservingView(*engineContext);
                        }
                        return true;
                    }}};
}

bool SectorEditor::RebuildPreviewMeshesPreservingView(engine::EngineContext& context)
{
    engine::AssetManager& assets = context.assets;
    if (!preview.IsRendererReady()) {
        return false;
    }

    std::string gateMessage;
    if (!CanUseCurrentAuthoringDerivedTopologyForPreview(state, &gateMessage)) {
        statusText = gateMessage.empty() ? "3D mode rebuild failed: derived topology is not current" : gateMessage;
        return false;
    }

    if (state.previewControlMode == SectorPreviewControlMode::Gameplay) {
        ClearSectorFpsLandingDip(state.landingDipState);
        ApplyGameplayPoseToPreview();
    }
    const SectorViewPose pose = preview.RendererPose();
    const bool mouseLook = state.freeflyController.mouseLookEnabled;
    const SectorSurfaceRef selected = state.selectedSurface3D;
    const TopologySurfaceEditTarget selectedTarget = state.selectedTopologySurface3D;

    std::string error;
    if (!preview.RebuildRendererResources(assets, state.topologyMap, "sector_editor_preview", error)) {
        state.runtimeObjects.objectLightProbes = SectorBakedObjectLightProbeRuntimeData{};
        state.runtimeObjects.objectProbeStatus.clear();
        state.runtimeObjects.objectSectorLookupWorld = SectorCollisionWorld{};
        state.runtimeObjects.objectSectorLookupWorldValid = false;
        state.runtimeObjects.objectSectorLookupWarning.clear();
        state.sectorCollisionWorldValid = false;
        state.sectorCollisionWorldWarning.clear();
        state.previewCollisionSectorId = 0;
        state.fpsControllerState.currentSectorId = 0;
        state.previewVerticalResult = SectorFpsVerticalResult{};
        state.previewMoveResult = SectorCollisionMoveResult{};
        state.previewCollisionNoclipFallback = false;
        ClearSectorFpsLandingDip(state.landingDipState);
        if (StartsWith(error, "Preview failed:")) {
            statusText = std::string{"3D mode failed:"} + error.substr(std::strlen("Preview failed:"));
        } else {
            statusText = error.empty() ? "3D mode rebuild failed" : error;
        }
        state.mode = SectorEditorMode::Edit2D;
        LeaveSectorFreeflyController();
        return false;
    }
    RefreshPreviewObjectProbeDebugData();
    EnsureSectorRuntimeObjectWorldReserved(context.world, state.runtimeObjects);
    SpawnPlacedRuntimeObjects(context.world, assets, state.runtimeObjects, state.topologyMap);

    preview.ApplyRendererPose(pose);
    ResetSectorFreeflyController(state.freeflyController, pose);
    SetSectorFreeflyMouseLookEnabled(state.freeflyController, mouseLook);
    const bool selectedStillValid = IsValidSurfaceRef(selected);
    state.selectedSurface3D = selectedStillValid ? selected : SectorSurfaceRef{};
    state.selectedTopologySurface3D = selectedStillValid && IsValidTopologySurfaceEditTarget(selectedTarget)
            ? selectedTarget
            : TopologySurfaceEditTarget{};
    RebuildSectorCollisionWorld();
    return true;
}

std::string SectorEditor::CurrentTextureForPickerTarget() const
{
    if (IsSectorEditorMaterialTexturePickerTarget(state.texturePicker.topologyTargetKind)) {
        SectorEditorMaterialEditingServiceContext serviceContext{
                const_cast<SectorEditorState&>(state),
                const_cast<SectorEditorUiState&>(uiState),
                const_cast<TexturePickerState&>(state.texturePicker),
                const_cast<std::string&>(statusText),
                nullptr};
        SectorEditorMaterialEditingService materialEditing{serviceContext};
        return materialEditing.CurrentTextureForPickerTarget();
    }
    return game::CurrentTextureForPickerTarget(state);
}

void SectorEditor::OpenSelectedBillboardSpritePicker()
{
    const SectorPlacedRuntimeObject* object = SelectedRuntimeObject();
    if (object == nullptr || object->kind != "billboard") {
        statusText = "Select a billboard first.";
        return;
    }

    OpenBillboardSpritePicker(
            state.spritePicker,
            object->billboard.spriteAnimationPath);
}

void SectorEditor::ApplySelectedBillboardSpritePickerSelection()
{
    const SectorEditorSpritePickerResult selected = SelectedSpritePickerResult(state.spritePicker);
    if (!selected.valid) {
        statusText = "Select a sprite";
        return;
    }

    const bool changed = MutateSelectedRuntimeObject(
            "Updated billboard sprite",
            [&selected](SectorPlacedRuntimeObject& object) {
                if (object.kind != "billboard") {
                    return false;
                }
                return ApplySpritePickerResultToBillboard(object.billboard, selected);
            });
    statusText = changed
            ? TextFormat("Selected sprite %s", selected.spriteAnimationPath.c_str())
            : "Billboard sprite unchanged";
}

void SectorEditor::OpenMapSkyTexturePicker()
{
    if (!game::OpenMapSkyTexturePicker(state)) {
        statusText = "No sky texture target";
    }
}

void SectorEditor::OpenSelectedDoorTexturePicker()
{
    const SectorPlacedRuntimeObject* object = SelectedRuntimeObject();
    if (object == nullptr || object->kind != "door") {
        statusText = "Select a door first.";
        return;
    }

    if (!game::OpenRuntimeDoorTexturePicker(state, object->id)) {
        statusText = "No door texture target";
    }
}

void SectorEditor::OpenDoorTextureSettingsModal()
{
    OpenSectorEditorDoorTextureSettingsModal(
            state.doorTextureSettingsModal,
            SelectedRuntimeObject(),
            statusText);
}

void SectorEditor::ApplyTexturePickerSelection(engine::AssetManager& assets)
{
    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::RuntimeDoor) {
        TexturePickerState& picker = state.texturePicker;
        const SectorEditorSelectedTexture selected = CurrentSectorEditorTexturePickerSelection(picker);
        if (!selected.valid) {
            statusText = "Select a texture";
            CloseSectorEditorTexturePicker(picker);
            return;
        }

        const int targetObjectId = picker.runtimeObjectId;
        const std::string selectedTexture = selected.textureId;
        CloseSectorEditorTexturePicker(picker);

        if (state.selectedRuntimeObjectId != targetObjectId) {
            statusText = "Door texture target unavailable";
            return;
        }

        const bool changed = MutateSelectedRuntimeObject(
                "Updated door texture",
                [&selectedTexture](SectorPlacedRuntimeObject& object) {
                    if (object.kind != "door" || object.door.textureId == selectedTexture) {
                        return false;
                    }
                    object.door.textureId = selectedTexture;
                    return true;
                });
        statusText = changed
                ? TextFormat("Selected door texture %s", selectedTexture.c_str())
                : "Door texture unchanged";
        return;
    }

    if (IsSectorEditorMaterialTexturePickerTarget(state.texturePicker.topologyTargetKind)) {
        SectorEditorMaterialEditingService materialEditing = BuildMaterialEditingService();
        materialEditing.ApplyTexturePickerSelection(&assets);
        return;
    }

    game::ApplyTexturePickerSelection(state);
}

} // namespace game
