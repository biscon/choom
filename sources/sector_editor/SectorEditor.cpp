#include "sector_editor/SectorEditor.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorDirtyState.h"
#include "sector_editor/document/SectorEditorDocumentActions.h"
#include "sector_editor/document/SectorEditorDocumentModals.h"
#include "sector_editor/inspector/SectorEditorInspectorPanel.h"
#include "sector_editor/npcs/SectorEditorNpcEditorModal.h"
#include "sector_editor/selection/SectorEditorManipulationService.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorLightInspector.h"
#include "sector_editor/SectorEditorLightmapModal.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorMaterialModals.h"
#include "sector_editor/SectorEditorPreviewActions.h"
#include "sector_editor/SectorEditorPreviewSettingsModal.h"
#include "sector_editor/SectorEditorSetAllModal.h"
#include "sector_editor/preview/SectorEditorPreviewOverlay.h"
#include "sector_editor/preview/SectorEditorPreviewUvPanel.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.h"
#include "sector_editor/services/sounds/SectorEditorSoundService.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"
#include "sector_editor/services/texture_picker/SectorEditorTexturePickerService.h"
#include "sector_editor/tools/doors/SectorEditorDoorModals.h"
#include "sector_editor/tools/materials/SectorEditorMaterialInspector.h"
#include "sector_editor/tools/SectorEditorToolModule.h"
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
#include <optional>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace game {

namespace {

constexpr float SectorEditorPanelScrollPaddingPx = 8.0f;
constexpr float SectorEditorFreeflyPrecisionMoveScale = 0.1f;

SectorEditorSelectionUiDependencies BuildSelectionUiDependencies(
        SectorEditorUiState& uiState,
        RuntimeObjectEditingUiState& runtimeObjectUiState,
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
            runtimeObjectUiState.xInput,
            runtimeObjectUiState.yInput,
            runtimeObjectUiState.zInput,
            runtimeObjectUiState.rotationXInput,
            runtimeObjectUiState.yawInput,
            runtimeObjectUiState.rotationZInput,
            runtimeObjectUiState.heightOffsetInput,
            runtimeObjectUiState.scaleInput,
            runtimeObjectUiState.animationSpeedInput,
            runtimeObjectUiState.widthInput,
            runtimeObjectUiState.heightInput,
            runtimeObjectUiState.thicknessInput,
            runtimeObjectUiState.normalOffsetInput,
            runtimeObjectUiState.openDistanceInput,
            runtimeObjectUiState.speedInput,
            runtimeObjectUiState.initialOpenFractionInput,
            runtimeObjectUiState.autoOpenDistanceInput,
            runtimeObjectUiState.interactionDistanceInput,
            runtimeObjectUiState.originXInput,
            runtimeObjectUiState.originYInput,
            uiState.inspectorScroll,
            inspectorIdUiState};
}

SectorEditorDerivationDocumentAccess MakeLiveDerivationAccess(SectorEditorDerivationState& derivation)
{
    return MakeSectorEditorDerivationDocumentAccess(derivation);
}

SectorEditorConstDerivationDocumentAccess MakeLiveDerivationAccess(const SectorEditorDerivationState& derivation)
{
    return MakeSectorEditorDerivationDocumentAccess(derivation);
}

SectorEditorConstDerivationDocumentAccess MakeLiveConstDerivationAccess(
        const SectorEditorDerivationState& derivation)
{
    return MakeLiveDerivationAccess(derivation);
}

SectorEditorSelectionServiceContext BuildSelectionServiceContextFromState(
        SectorEditorState& state,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorConstDerivationDocumentAccess derivation,
        SectorEditorPreviewSelectionState& previewSelectionState,
        SelectionState& selectionState,
        ManipulationState& manipulationState,
        RuntimeObjectEditingState& runtimeObjectEditingState,
        RuntimeObjectEditingUiState& runtimeObjectEditingUiState,
        SectorEditorUiState& uiState,
        InspectorIdUiState& inspectorIdUiState,
        MaterialEditingUiState& materialUiState,
        std::string* statusText = nullptr,
        void* userData = nullptr,
        void (*requestCancelInteractiveLightEdit)(void*, const char*) = nullptr,
        LightEditingState* lightState = nullptr)
{
    SectorEditorAuthoringDocumentAccess authoring =
            MakeSectorEditorAuthoringDocumentAccess(authoringGraph);
    return SectorEditorSelectionServiceContext{
            topologyMap,
            authoring.graph,
            derivation.authoringDerivation,
            IsSectorEditorAuthoringDerivationCurrent(derivation),
            selectionState,
            previewSelectionState.selectedSurface3D,
            previewSelectionState.selectedTopologySurface3D,
            manipulationState,
            runtimeObjectEditingState.drag,
            BuildSelectionUiDependencies(uiState, runtimeObjectEditingUiState, inspectorIdUiState),
            materialUiState,
            statusText,
            userData,
            requestCancelInteractiveLightEdit,
            lightState};
}

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

} // namespace

bool SectorEditor::Init(engine::EngineContext& context)
{
    engineContext = &context;
    Shutdown(context);
    engineContext = &context;
    weaponRegistryError.clear();
    if (!LoadFpsWeaponRegistry(ASSETS_PATH "config/weapons.json", weaponRegistry, &weaponRegistryError)) {
        TraceLog(LOG_ERROR, "Weapon registry load failed: %s", weaponRegistryError.c_str());
        statusText = "Startup failed: " + weaponRegistryError;
        return false;
    }
    RequestFpsWeaponAudioAssets(context.assets, weaponRegistry);
    applicationSettingsPath = ASSETS_PATH "config/application_settings.json";
    RequestPlayerAudioAssets(
            context.assets,
            applicationSettings.playerSounds,
            playerAudio);
    ResetToBlankMap(context);
    fogVolumeEditingService.emplace(
            SectorEditorAuthoringFogVolumeEditingServiceContext{
                    Lifecycle(),
                    TopologyMap(),
                    AuthoringGraph(),
                    MakeLiveDerivationAccess(documentState.derivation),
                    state.topologyRenderRevision,
                    state.topologyRenderCache,
                    selectionState,
                    manipulationState,
                    statusText});
    levelMarkerEditingService.emplace(
            SectorEditorLevelMarkerEditingServiceContext{
                    Lifecycle(),
                    TopologyMap(),
                    AuthoringGraph(),
                    MakeLiveDerivationAccess(documentState.derivation),
                    state.topologyRenderRevision,
                    state.topologyRenderCache,
                    selectionState,
                    levelMarkerEditingState,
                    statusText});
    triggerEditingService.emplace(
            SectorEditorTriggerEditingServiceContext{
                    Lifecycle(), TopologyMap(), AuthoringGraph(),
                    MakeLiveDerivationAccess(documentState.derivation),
                    state.topologyRenderRevision, state.topologyRenderCache,
                    selectionState, triggerEditingState, statusText});
    authoringFaceMergeService.emplace(
            SectorEditorAuthoringFaceMergeServiceContext{
                    state,
                    Lifecycle(),
                    TopologyMap(),
                    AuthoringGraph(),
                    MakeLiveDerivationAccess(documentState.derivation),
                    selectionState,
                    authoringFaceMergeState,
                    statusText});
    return true;
}

void SectorEditor::Shutdown(engine::EngineContext& context)
{
    engine::AssetManager& assets = context.assets;
    SectorEditorAudioAssetPickerService audioPicker{
            context, audioAssetPickerSessionState};
    audioPicker.Close(npcEditorState.audioPicker.assetPicker);
    BuildNpcEditorService().Shutdown(assets);
    if (state.footstepPicker.open
            || !engine::IsNull(state.footstepPicker.previewScope)) {
        BuildFootstepService().Close();
    }
    lightmapBake.Shutdown();
    EndFpsViewmodel(assets);
    sceneRuntime.Shutdown(context);
    if (engineContext != nullptr) {
        BuildSoundService().Shutdown();
    }
    if (!engine::IsNull(textureCatalogState.editorTextureScope)) {
        assets.UnloadScope(textureCatalogState.editorTextureScope);
    }
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }
    if (!engine::IsNull(runtimeObjectEditingState.spritePicker.previewScope)) {
        assets.UnloadScope(runtimeObjectEditingState.spritePicker.previewScope);
    }
    state = SectorEditorState{};
    uiState = SectorEditorUiState{};
    runtimeObjectEditingState = RuntimeObjectEditingState{};
    runtimeObjectEditingUiState = RuntimeObjectEditingUiState{};
    npcEditorState = SectorEditorNpcEditorState{};
    npcEditorSessionState = SectorEditorNpcEditorSessionState{};
    audioAssetPickerSessionState = SectorEditorAudioAssetPickerSessionState{};
    textureCatalogState = TextureCatalogState{};
    soundCatalogState = SectorEditorSoundCatalogState{};
    lightEditingState = LightEditingState{};
    materialEditingState = MaterialEditingState{};
    materialEditingUiState = MaterialEditingUiState{};
    fogVolumeEditingUiState = FogVolumeEditingUiState{};
    levelMarkerEditingState = LevelMarkerEditingState{};
    levelMarkerEditingUiState = LevelMarkerEditingUiState{};
    triggerEditingState = TriggerEditingState{};
    triggerEditingUiState = TriggerEditingUiState{};
    authoringFaceMergeState = SectorEditorAuthoringFaceMergeState{};
    fogVolumeEditingService.reset();
    authoringFaceMergeService.reset();
    levelMarkerEditingService.reset();
    triggerEditingService.reset();
    playerAudio = PlayerAudioRuntime{};
    canvasRect = {};
    statusText.clear();
    engineContext = nullptr;
    initialized = false;
}

void SectorEditor::BeginFpsViewmodel(engine::AssetManager& assets)
{
    fpsPlayer.Begin(
            assets,
            sceneRuntime.Renderer(),
            weaponRegistry,
            applicationSettings,
            "fps_viewmodel");
}

void SectorEditor::EndFpsViewmodel(engine::AssetManager& assets)
{
    fpsPlayer.End(assets, sceneRuntime.Renderer());
}
void SectorEditor::UpdateFpsViewmodel(
        engine::AssetManager& assets,
        float dt)
{
    if (!state.previewSettingsModal.open) {
        fpsPlayer.Update(
                assets,
                weaponRegistry,
                applicationSettings,
                dt);
        return;
    }

    const FpsViewmodelPresentation presentation =
            ClampFpsViewmodelPresentation(
                    state.previewSettingsModal.draftViewmodel);
    const FpsViewmodelHolsterTransition holsterTransition =
            ClampFpsViewmodelHolsterTransition(
                    state.previewSettingsModal
                            .draftViewmodelHolsterTransition);
    const FpsWeaponFiringDefinition firing =
            ClampFpsWeaponFiringDefinition(
                    state.previewSettingsModal.draftWeaponFiring);
    const FpsViewmodelGripCorrection gripCorrection =
            ClampFpsViewmodelGripCorrection(
                    state.previewSettingsModal.draftViewmodelGrip);
    const FpsViewmodelAttachmentLighting attachmentLighting =
            ClampFpsViewmodelAttachmentLighting(
                    state.previewSettingsModal
                            .draftViewmodelAttachmentLighting);
    const FpsPlayerRuntimeTuning tuning{
            &presentation,
            &holsterTransition,
            &firing,
            &gripCorrection,
            &attachmentLighting};
    fpsPlayer.Update(
            assets,
            weaponRegistry,
            applicationSettings,
            dt,
            &tuning);
}
bool SectorEditor::ProcessFpsWeaponFire(engine::Input& input)
{
    const bool gameplay3D = state.mode == SectorEditorMode::Preview3D
            && previewState.controller.previewControlMode
                    == SectorPreviewControlMode::Gameplay;
    const bool mouseActive = gameplay3D
            && previewState.controller.freeflyController.mouseLookEnabled;
    const bool uiCaptured = uiState.keyboardCaptured
            || state.texturePicker.open
            || state.soundPicker.open
            || state.addMapSound.open
            || state.footstepPicker.open
            || state.decalTintModal.open
            || state.previewSettingsModal.open
            || npcEditorState.open
            || runtimeObjectEditingState.spritePicker.open
            || runtimeObjectEditingState.staticModelPicker.open
            || HasDocumentModalOpen();
    const bool accepted = fpsPlayer.HandleFireInput(
            input,
            engineContext->assets,
            engineContext->audio,
            previewState.collision.sectorCollisionWorldValid
                    ? &previewState.collision.sectorCollisionWorld
                    : nullptr,
            sceneRuntime.Renderer(),
            gameplay3D,
            mouseActive,
            uiCaptured);
    if (!accepted) return false;
    const FpsShotResult request = fpsPlayer.State().firing.lastShot;
    FpsShotResult resolvedShot;
    sceneRuntime.ResolvePlayerWeaponShot(
            *engineContext,
            previewState.collision.sectorCollisionWorldValid
                    ? &previewState.collision.sectorCollisionWorld
                    : nullptr,
            request.rayOrigin,
            request.rayDirection,
            fpsPlayer.State().firing.definition.maximumRangeWorld,
            fpsPlayer.State().firing.definition.impact,
            resolvedShot);
    fpsPlayer.RecordShotResolution(resolvedShot);
    return true;
}
void SectorEditor::UpdateFpsViewmodelTransformsAndLight()
{
    fpsPlayer.UpdateTransformsAndLight(
            sceneRuntime.Renderer(),
            previewState.collision.sectorCollisionWorldValid
                    ? &previewState.collision.sectorCollisionWorld
                    : nullptr);

    if (previewState.controller.previewControlMode
            == SectorPreviewControlMode::Gameplay) {
        const SectorFpsControllerConfig config =
                NormalizeSectorFpsControllerConfig(
                        previewState.controller.fpsControllerConfig);
        sceneRuntime.Renderer().UpdateVisibilityDebug(
                previewState.controller.fpsControllerState.currentSectorId,
                ClampRuntimeVisibilitySeedRadiusWorld(config.playerRadius),
                true,
                &sceneRuntime.RuntimeObjects().dynamicPortalBlockers,
                engineContext != nullptr ? &engineContext->world : nullptr);
    } else {
        sceneRuntime.Renderer().UpdateVisibilityDebug(
                0,
                0.0f,
                false,
                &sceneRuntime.RuntimeObjects().dynamicPortalBlockers,
                engineContext != nullptr ? &engineContext->world : nullptr);
    }
}
void SectorEditor::Update(engine::EngineContext& context, float dt)
{
    engine::Input& input = context.input;
    engine::AssetManager& assets = context.assets;
    if (state.footstepPicker.open) {
        BuildFootstepService().UpdatePreview();
    }
    if (lightmapBake.IsBlocking()) {
        CancelAuthoringVertexDrag(nullptr);
        CancelLightDrag(nullptr);
        CancelPendingAuthoringLine(nullptr);
        CancelPendingAuthoringRectangle(nullptr);
        if (fogVolumeEditingService) {
            fogVolumeEditingService->CancelMove(nullptr);
        }
        if (levelMarkerEditingService) {
            levelMarkerEditingService->CancelMove(nullptr);
        }
        return;
    }

    if (state.mode == SectorEditorMode::Preview3D) {
        const Vector3 playerPosition = previewState.controller.freeflyController.pose.position;
        SectorDoorPlayerObstacle playerObstacle;
        const SectorDoorPlayerObstacle* playerObstaclePtr = nullptr;
        if (previewState.controller.previewControlMode
                == SectorPreviewControlMode::Gameplay) {
            const SectorFpsControllerConfig obstacleConfig =
                    EffectiveSectorFpsControllerConfig(
                            previewState.controller.fpsControllerState,
                            previewState.controller.fpsControllerConfig);
            playerObstacle = SectorDoorPlayerObstacle{
                    previewState.controller.fpsControllerState.feetPosition,
                    obstacleConfig.playerRadius,
                    obstacleConfig.playerHeight};
            playerObstaclePtr = &playerObstacle;
        }
        sceneRuntime.Update(
                context,
                TopologyMap(),
                dt,
                &playerPosition,
                playerObstaclePtr);
        UpdateFpsViewmodel(assets, dt);
        const bool hasBlockingModal = state.texturePicker.open
                || state.soundPicker.open
                || state.addMapSound.open
                || state.footstepPicker.open
                || runtimeObjectEditingState.spritePicker.open
                || runtimeObjectEditingState.staticModelPicker.open
                || HasDocumentModalOpen();
        if (hasBlockingModal) {
            if (previewState.controller.previewControlMode
                    == SectorPreviewControlMode::Gameplay) {
                ApplyGameplayPoseToPreview();
            }
            UpdateFpsViewmodelTransformsAndLight();
            const Camera3D& camera = sceneRuntime.Renderer().RenderCamera();
            context.audio.SetListener(engine::AudioListener{
                    camera.position,
                    Vector3Subtract(camera.target, camera.position),
                    camera.up});
            return;
        }
        const bool canInteractWithDoors = previewState.controller.previewControlMode == SectorPreviewControlMode::Gameplay
                && previewState.controller.freeflyController.mouseLookEnabled
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
                                    previewState.controller.freeflyController.pose.position,
                                    PreviewForwardFromPose(previewState.controller.freeflyController.pose))) {
                            engine::ConsumeEvent(event);
                        }
                    });
        }
        UpdatePreview3D(input, assets, dt);
        if (state.mode == SectorEditorMode::Preview3D) {
            if (ProcessFpsWeaponFire(input)) {
                ApplyGameplayPoseToPreview();
            }
            UpdateFpsViewmodelTransformsAndLight();
            UpdatePreview3DSelection(input);
            const Camera3D& camera = sceneRuntime.Renderer().RenderCamera();
            context.audio.SetListener(engine::AudioListener{
                    camera.position,
                    Vector3Subtract(camera.target, camera.position),
                    camera.up});
        }
        return;
    }

    canvasRect = BuildCanvasRect();
    if (state.texturePicker.open
            || state.soundPicker.open
            || state.footstepPicker.open
            || state.addMapTexture.open
            || state.addMapSound.open
            || npcEditorState.open
            || runtimeObjectEditingState.spritePicker.open
            || runtimeObjectEditingState.staticModelPicker.open
            || HasDocumentModalOpen()) {
        return;
    }
    if (state.currentTool == SectorEditorTool::Select) {
        EnsureTopologyRenderCache();
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
        if (lightmapBake.IsBlocking()) {
            DrawLightmapBakeModal(ui, config, input, assets, font);
            uiState.keyboardCaptured = true;
            engine::EndUI(ui, config, input, assets);
            return;
        }
        if (previewState.overlay.previewUiHidden) {
            ui.hotId = 0;
            ui.activeId = 0;
            ui.focusedId = 0;
            ui.openOptionId = 0;
        } else {
            DrawPreviewOverlay(ui, config, input, assets, font, smallFont);
        }
        if (!previewState.overlay.previewUiHidden
                && !state.texturePicker.open
                && !state.soundPicker.open
                && !state.footstepPicker.open
                && !runtimeObjectEditingState.spritePicker.open
                && !runtimeObjectEditingState.staticModelPicker.open
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
        DrawTexturePickerModal(ui, config, input, assets, font, smallFont);
        DrawSoundPickerModal(ui, config, input, font);
        DrawFootstepPickerModal(ui, config, input, assets, font);
        DrawSpritePickerModal(ui, config, input, assets, font);
        DrawStaticModelPickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = ui.focusedId != 0;
        if (state.texturePicker.open
                || state.soundPicker.open
                || state.footstepPicker.open
                || runtimeObjectEditingState.spritePicker.open
                || runtimeObjectEditingState.staticModelPicker.open
                || state.decalTintModal.open
                || state.doorTextureSettingsModal.open
                || state.previewSettingsModal.open) {
            uiState.keyboardCaptured = true;
        }
        engine::EndUI(ui, config, input, assets);
        return;
    }

    engine::BeginUI(ui, input);
    if (lightmapBake.IsBlocking()) {
        DrawLightmapBakeModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (npcEditorState.open) {
        DrawNpcEditorModal(ui, config, input, assets, font, smallFont);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.setAllModal.open) {
        DrawSetAllModal(ui, config, input, assets, font);
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
        DrawTexturePickerModal(ui, config, input, assets, font, smallFont);
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
    if (state.addMapSound.open) {
        DrawAddMapSoundModal(ui, config, input, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.texturePicker.open) {
        DrawTexturePickerModal(ui, config, input, assets, font, smallFont);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.soundPicker.open) {
        DrawSoundPickerModal(ui, config, input, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.footstepPicker.open) {
        DrawFootstepPickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (runtimeObjectEditingState.spritePicker.open) {
        DrawSpritePickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (runtimeObjectEditingState.staticModelPicker.open) {
        DrawStaticModelPickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }

    DrawToolsPanel(ui, config, input, assets, font);
    DrawSectorsPanel(ui, config, input, assets, font, smallFont);
    DrawStatusPanel(ui, config, assets, smallFont);
    DrawSetAllModal(ui, config, input, assets, font);
    DrawAddMapTextureModal(ui, config, input, assets, font);
    DrawAddMapSoundModal(ui, config, input, font);
    DrawTexturePickerModal(ui, config, input, assets, font, smallFont);
    DrawSoundPickerModal(ui, config, input, font);
    DrawFootstepPickerModal(ui, config, input, assets, font);
    DrawSpritePickerModal(ui, config, input, assets, font);
    DrawStaticModelPickerModal(ui, config, input, assets, font);
    uiState.keyboardCaptured = ui.focusedId != 0;
    if (state.texturePicker.open
            || state.soundPicker.open
            || state.footstepPicker.open
            || state.addMapTexture.open
            || state.addMapSound.open
            || npcEditorState.open
            || runtimeObjectEditingState.spritePicker.open
            || runtimeObjectEditingState.staticModelPicker.open
            || HasDocumentModalOpen()) {
        uiState.keyboardCaptured = true;
    }
    engine::EndUI(ui, config, input, assets);
}

bool SectorEditor::IsPreview3DActive() const
{
    return state.mode == SectorEditorMode::Preview3D;
}

bool SectorEditor::OpenLevel(
        engine::EngineContext& context,
        const std::string& levelName,
        const std::string& jsonAssetPath)
{
    return LoadLevel(context, levelName, jsonAssetPath);
}

void SectorEditor::SuspendRuntime(engine::EngineContext& context)
{
    if (state.mode == SectorEditorMode::Preview3D) {
        LeavePreview3D();
    }
    sceneRuntime.Shutdown(context);
}

void SectorEditor::RestoreRuntimeObjects(engine::EngineContext& context)
{
    sceneRuntime.RefreshMapRuntimeObjects(context, TopologyMap());
}

const SectorTopologyMap& SectorEditor::CurrentTopologyMap() const
{
    return TopologyMap();
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
            state.currentTool,
            state.pendingAuthoringLine,
            state.pendingAuthoringRectangle,
            state.pendingAuthoringInsertVertex,
            AuthoringGraph(),
            selectionState,
            statusText,
            documentState.derivation.authoringDerivationStatus,
            input,
            canvasRect};
    context.fogVolumeEditing = fogVolumeEditingService
            ? &fogVolumeEditingService.value()
            : nullptr;
    context.levelMarkerEditing = levelMarkerEditingService
            ? &levelMarkerEditingService.value()
            : nullptr;
    context.authoringFaceMerge = authoringFaceMergeService
            ? &authoringFaceMergeService.value()
            : nullptr;
    context.triggerEditing = triggerEditingService ? &triggerEditingService.value() : nullptr;
    context.triggerEditingState = &triggerEditingState;
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
        SectorAuthoringGraph& authoringGraph = AuthoringGraph();
        SelectSectorEditorAuthoringLine(authoringGraph, selectionState, lineId);
    };
    context.hoverAuthoringLine = [this](int lineId) {
        SectorAuthoringGraph& authoringGraph = AuthoringGraph();
        SetHoveredSectorEditorAuthoringLine(authoringGraph, selectionState, lineId);
    };
    context.findAuthoringLineNearScreenPoint = [this](Vector2 screenPoint) {
        return FindAuthoringLineNearScreenPoint(screenPoint);
    };
    context.commitAuthoringLinePoint = [this](SectorTopologyCoordPoint point) {
        return ClickSectorEditorAuthoringLineTool(
                state,
                Lifecycle(),
                TopologyMap(),
                AuthoringGraph(),
                MakeLiveDerivationAccess(documentState.derivation),
                selectionState,
                point);
    };
    context.cancelAuthoringLineChain = [this]() {
        CancelSectorEditorAuthoringLineToolChain(state);
    };
    context.commitAuthoringRectangle = [this](
            SectorTopologyCoordPoint firstCorner,
            SectorTopologyCoordPoint oppositeCorner,
            SectorEditorAuthoringRectangleResult* outResult) {
        return AddSectorEditorAuthoringRectangle(
                state,
                Lifecycle(),
                TopologyMap(),
                AuthoringGraph(),
                MakeLiveDerivationAccess(documentState.derivation),
                firstCorner,
                oppositeCorner,
                outResult);
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
        return InsertSectorEditorAuthoringVertexOnLine(
                state,
                Lifecycle(),
                TopologyMap(),
                AuthoringGraph(),
                MakeLiveDerivationAccess(documentState.derivation),
                selectionState,
                lineId,
                point,
                outResult);
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

SectorAuthoringGraph& SectorEditor::AuthoringGraph()
{
    return documentState.authoring.authoringGraph;
}

const SectorAuthoringGraph& SectorEditor::AuthoringGraph() const
{
    return documentState.authoring.authoringGraph;
}

SectorEditorDocumentLifecycleAccess SectorEditor::Lifecycle()
{
    return MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle);
}

SectorEditorConstDocumentLifecycleAccess SectorEditor::Lifecycle() const
{
    return MakeSectorEditorDocumentLifecycleAccess(documentState.lifecycle);
}

SectorTopologyMap& SectorEditor::TopologyMap()
{
    return documentState.map.topologyMap;
}

const SectorTopologyMap& SectorEditor::TopologyMap() const
{
    return documentState.map.topologyMap;
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
        const SectorAuthoringGraph& authoringGraph = AuthoringGraph();
        for (const SectorAuthoringVertex& authoringVertex : authoringGraph.vertices) {
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
        for (const SectorTopologyVertex& topologyVertex : TopologyMap().vertices) {
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
    selectionState.hasHoveredVertex = false;
    selectionState.hoveredTopologyLightId = -1;
    selectionState.hoveredTopologyStaticSpotLightId = -1;
    selectionState.hoveredTopologyDynamicLightId = -1;
    selectionState.hoveredTopologyDynamicSpotLightId = -1;
    selectionState.hoveredTopologyVertexId = -1;
    selectionState.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    ClearSectorEditorAuthoringHover(selectionState);

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
        SectorAuthoringGraph& authoringGraph = AuthoringGraph();
        int authoringVertexId = -1;
        SectorTopologyCoordPoint authoringVertexPoint{};
        if (FindAuthoringVertexNearScreenPoint(
                    input.MousePosition(),
                    authoringVertexId,
                    authoringVertexPoint)) {
            SetHoveredSectorEditorAuthoringVertex(authoringGraph, selectionState, authoringVertexId);
        }
        selectionState.inspectedTopologyVertexId = -1;
        return;
    }

    if (state.currentTool == SectorEditorTool::AuthoringInsertVertex) {
        if (state.pendingAuthoringInsertVertex.lineId >= 0) {
            SectorAuthoringGraph& authoringGraph = AuthoringGraph();
            SetHoveredSectorEditorAuthoringLine(
                    authoringGraph,
                    selectionState,
                    state.pendingAuthoringInsertVertex.lineId);
        }
        selectionState.inspectedTopologyVertexId = -1;
        return;
    }

    if (state.currentTool == SectorEditorTool::StaticLight
            || state.currentTool == SectorEditorTool::Move) {
        const int lightId = FindTopologyLightNearScreenPoint(input.MousePosition());
        if (lightId >= 0) {
            selectionState.hoveredTopologyLightId = lightId;
            selectionState.inspectedTopologyVertexId = -1;
        } else if (state.currentTool == SectorEditorTool::Move
                && !IsSectorEditorGraphAuthoritativeMode()) {
            int vertexId = -1;
            SectorTopologyCoordPoint point;
            if (FindTopologyVertexNearScreenPoint(input.MousePosition(), vertexId, point)) {
                selectionState.hasHoveredVertex = true;
                selectionState.hoveredTopologyVertexId = vertexId;
                selectionState.hoveredTopologyVertexPoint = point;
                selectionState.inspectedTopologyVertexId = vertexId;
            } else {
                selectionState.inspectedTopologyVertexId = -1;
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
            selectionState.hoveredTopologyStaticSpotLightId = lightId;
            selectionState.inspectedTopologyVertexId = -1;
        }
    }

    if (state.currentTool == SectorEditorTool::DynamicLight) {
        const int lightId = FindTopologyDynamicLightNearScreenPoint(input.MousePosition());
        if (lightId >= 0) {
            selectionState.hoveredTopologyDynamicLightId = lightId;
            selectionState.inspectedTopologyVertexId = -1;
        }
    }

    if (state.currentTool == SectorEditorTool::DynamicSpotLight) {
        int lightId = -1;
        SpotLightHandle handle = SpotLightHandle::Origin;
        if (!FindTopologyDynamicSpotLightHandleNearScreenPoint(input.MousePosition(), lightId, handle)) {
            lightId = FindTopologyDynamicSpotLightNearScreenPoint(input.MousePosition());
        }
        if (lightId >= 0) {
            selectionState.hoveredTopologyDynamicSpotLightId = lightId;
            selectionState.inspectedTopologyVertexId = -1;
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
                    } else if (authoringFaceMergeService
                            && authoringFaceMergeService->IsChoosingTarget()) {
                        authoringFaceMergeService->CancelTargetPick(
                                "Merge Selected Into cancelled");
                    } else if (state.pendingAuthoringLine.active) {
                        CancelPendingAuthoringLine("Line chain stopped");
                    } else if (state.pendingAuthoringRectangle.active) {
                        CancelPendingAuthoringRectangle("Rectangle cancelled");
                    } else if (state.pendingAuthoringInsertVertex.active
                            || state.currentTool == SectorEditorTool::AuthoringInsertVertex) {
                        CancelPendingAuthoringInsertVertex("Insert Vertex cancelled");
                    } else if (triggerEditingState.pending.active) {
                        triggerEditingState.pending = PendingTriggerDrawState{};
                        statusText = "Trigger drawing cancelled";
                    } else if (selectionState.selectedTopologyLightId >= 0
                            || selectionState.selectedTopologyStaticSpotLightId >= 0
                            || selectionState.selectedTopologyDynamicLightId >= 0
                            || selectionState.selectedTopologyDynamicSpotLightId >= 0
                            || selectionState.selectedRuntimeObjectId >= 0
                            || selectionState.topologySelectionKind != TopologySelectionKind::None
                            || selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::None) {
                        ClearSelection();
                    } else if (state.currentTool != SectorEditorTool::Select) {
                        state.currentTool = SectorEditorTool::Select;
                    } else {
                        return;
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_DELETE) {
                    if (!selectionState.selectedAuthoringFaceAnchorIds.empty()
                            && authoringFaceMergeService) {
                        authoringFaceMergeService->BeginTargetPick();
                    } else if (IsGraphAuthoringTool(state.currentTool)) {
                        if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line) {
                            DeleteSelectedAuthoringLine();
                        } else if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
                            DeleteSelectedAuthoringVertex();
                        } else {
                            statusText = "Select an authoring line or an isolated/degree-2 authoring vertex to delete.";
                        }
                    } else if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line) {
                        DeleteSelectedAuthoringLine();
                    } else if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
                        DeleteSelectedAuthoringVertex();
                    } else if (selectionState.topologySelectionKind == TopologySelectionKind::StaticLight
                            && selectionState.selectedTopologyLightId >= 0) {
                        OpenDeleteSelectedLightConfirmation();
                    } else if (selectionState.topologySelectionKind == TopologySelectionKind::StaticSpotLight
                            && selectionState.selectedTopologyStaticSpotLightId >= 0) {
                        OpenDeleteSelectedLightConfirmation();
                    } else if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicLight
                            && selectionState.selectedTopologyDynamicLightId >= 0) {
                        OpenDeleteSelectedLightConfirmation();
                    } else if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicSpotLight
                            && selectionState.selectedTopologyDynamicSpotLightId >= 0) {
                        OpenDeleteSelectedLightConfirmation();
                    } else if (selectionState.selectedRuntimeObjectId >= 0) {
                        DeleteSelectedRuntimeObject();
                    } else if (selectionState.topologySelectionKind == TopologySelectionKind::Sector
                            && selectionState.selectedTopologySectorId >= 0) {
                        statusText = LegacyTopologyMutationUnavailableMessage();
                    } else if (selectionState.topologySelectionKind == TopologySelectionKind::Vertex
                            && selectionState.selectedTopologyVertexId >= 0) {
                        statusText = "Standalone vertex deletion is not available; use Dissolve Vertex for simple degree-2 vertices.";
                    } else if (selectionState.topologySelectionKind == TopologySelectionKind::SideDef
                            || selectionState.topologySelectionKind == TopologySelectionKind::LineDef) {
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
                    if (authoringFaceMergeService
                            && authoringFaceMergeService->IsChoosingTarget()) {
                        authoringFaceMergeService->CancelTargetPick(
                                "Merge Selected Into cancelled");
                        engine::ConsumeEvent(event);
                        return;
                    }
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
                    const Vector2 mapPoint = SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition));
                    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
                    lightEditing.AddStaticLight(FindTopologySectorAt(mapPoint), mapPoint);
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::StaticSpotLight) {
                    const Vector2 mapPoint = SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition));
                    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
                    lightEditing.AddStaticSpotLight(FindTopologySectorAt(mapPoint), mapPoint);
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::DynamicLight) {
                    const Vector2 mapPoint = SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition));
                    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
                    lightEditing.AddDynamicLight(FindTopologySectorAt(mapPoint), mapPoint);
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::DynamicSpotLight) {
                    const Vector2 mapPoint = SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition));
                    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
                    lightEditing.AddDynamicSpotLight(FindTopologySectorAt(mapPoint), mapPoint);
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::RuntimeObject) {
                    AddRuntimeObjectAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::StaticModel) {
                    AddStaticModelAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::DynamicModel) {
                    AddDynamicModelAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Npc) {
                    AddNpcAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
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
    if (selectionState.selectedRuntimeObjectId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::RuntimeObject, selectionState.selectedRuntimeObjectId};
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicSpotLight
            && selectionState.selectedTopologyDynamicSpotLightId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::DynamicSpotLight, selectionState.selectedTopologyDynamicSpotLightId};
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicLight
            && selectionState.selectedTopologyDynamicLightId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::DynamicLight, selectionState.selectedTopologyDynamicLightId};
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::StaticSpotLight
            && selectionState.selectedTopologyStaticSpotLightId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::StaticSpotLight, selectionState.selectedTopologyStaticSpotLightId};
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::StaticLight
            && selectionState.selectedTopologyLightId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::StaticLight, selectionState.selectedTopologyLightId};
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex
            && selectionState.selectedAuthoring.vertexId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::AuthoringVertex, selectionState.selectedAuthoring.vertexId};
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line
            && selectionState.selectedAuthoring.lineId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::AuthoringLine, selectionState.selectedAuthoring.lineId};
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor
            && selectionState.selectedAuthoring.faceAnchorId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::AuthoringFaceAnchor, selectionState.selectedAuthoring.faceAnchorId};
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::FogVolume
            && selectionState.selectedAuthoring.fogVolumeId >= 0) {
        return SectorEditorPickTarget{
                SectorEditorPickKind::AuthoringFogVolume,
                selectionState.selectedAuthoring.fogVolumeId};
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::LevelMarker
            && selectionState.selectedAuthoring.levelMarkerId >= 0) {
        return SectorEditorPickTarget{
                SectorEditorPickKind::LevelMarker,
                selectionState.selectedAuthoring.levelMarkerId};
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Trigger
            && selectionState.selectedAuthoring.triggerId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::Trigger,
                selectionState.selectedAuthoring.triggerId};
    }
    return SectorEditorPickTarget{};
}

std::vector<SectorEditorPickCandidate> SectorEditor::BuildSelectPickCandidates(Vector2 screenPoint) const
{
    std::vector<SectorEditorPickCandidate> candidates;
    candidates.reserve(
            TopologyMap().runtimeObjects.size()
            + TopologyMap().dynamicSpotLights.size()
            + TopologyMap().dynamicPointLights.size()
            + TopologyMap().staticSpotLights.size()
            + TopologyMap().staticLights.size()
            + AuthoringGraph().fogVolumes.size()
            + AuthoringGraph().levelMarkers.size()
            + AuthoringGraph().triggers.size()
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

    SectorEditorTopologyDrawContext pickContext;
    pickContext.canvasRect = canvasRect;
    pickContext.viewCenter = state.viewCenter;
    pickContext.viewZoom = state.viewZoom;
    AppendCachedRuntimeObjectPickCandidates(
            state.topologyRenderCache,
            pickContext,
            screenPoint,
            ScreenLightPickPixels,
            candidates);
    AppendCachedLevelMarkerPickCandidates(
            state.topologyRenderCache,
            pickContext,
            screenPoint,
            ScreenLightPickPixels,
            candidates);
    if (triggerEditingService) {
        const Vector2 mapPoint = ScreenToMap(screenPoint);
        const Vector2 tolerancePoint = ScreenToMap(Vector2{screenPoint.x + ScreenLightPickPixels, screenPoint.y});
        const float tolerance = std::fabs(tolerancePoint.x - mapPoint.x);
        for (const SectorAuthoringTrigger& trigger : AuthoringGraph().triggers) {
            if (SectorEditorTriggerHitTest(trigger, mapPoint, tolerance)) {
                candidates.push_back(SectorEditorPickCandidate{
                        SectorEditorPickTarget{SectorEditorPickKind::Trigger, trigger.editorId}, 0.0f});
            }
        }
    }
    for (const SectorTopologyDynamicSpotLight& light : TopologyMap().dynamicSpotLights) {
        addSpotCandidate(
                SectorEditorPickKind::DynamicSpotLight,
                light.id,
                MapToScreen(Vector2{light.position.x, light.position.z}),
                MapToScreen(Vector2{light.target.x, light.target.z}));
    }
    for (const SectorTopologyDynamicPointLight& light : TopologyMap().dynamicPointLights) {
        addPointCandidate(
                SectorEditorPickKind::DynamicLight,
                light.id,
                MapToScreen(Vector2{light.position.x, light.position.z}));
    }
    for (const SectorTopologyStaticSpotLight& light : TopologyMap().staticSpotLights) {
        addSpotCandidate(
                SectorEditorPickKind::StaticSpotLight,
                light.id,
                MapToScreen(Vector2{light.position.x, light.position.z}),
                MapToScreen(Vector2{light.target.x, light.target.z}));
    }
    for (const SectorTopologyStaticPointLight& light : TopologyMap().staticLights) {
        addPointCandidate(
                SectorEditorPickKind::StaticLight,
                light.id,
                MapToScreen(Vector2{light.position.x, light.position.z}));
    }

    if (fogVolumeEditingService) {
        const Vector2 mapPoint = ScreenToMap(screenPoint);
        const Vector2 tolerancePoint = ScreenToMap(Vector2{screenPoint.x + ScreenLightPickPixels, screenPoint.y});
        const float tolerance = std::fabs(tolerancePoint.x - mapPoint.x);
        const int fogVolumeId = fogVolumeEditingService->FindAtMapPoint(mapPoint, tolerance);
        if (fogVolumeId >= 0) {
            candidates.push_back(SectorEditorPickCandidate{
                    SectorEditorPickTarget{SectorEditorPickKind::AuthoringFogVolume, fogVolumeId},
                    0.0f});
        }
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
    SectorAuthoringGraph& authoringGraph = AuthoringGraph();
    if (!IsValidSectorAuthoringId(vertexId)
            || FindSectorAuthoringVertex(authoringGraph, vertexId) == nullptr) {
        return;
    }

    SelectAuthoringVertex(vertexId);
    ClearTopologySelectionOnly();
    manipulationState.authoringVertexDrag.active = true;
    manipulationState.authoringVertexDrag.vertexId = vertexId;
    manipulationState.authoringVertexDrag.originalPoint = point;
    manipulationState.authoringVertexDrag.previewPoint = point;
    manipulationState.authoringVertexDrag.hasPreviewPoint = true;
    manipulationState.authoringVertexDrag.errorMessage.clear();

    size_t connectedCount = 0;
    for (const SectorAuthoringLine& line : authoringGraph.lines) {
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
    if (!manipulationState.authoringVertexDrag.active) {
        return;
    }

    std::string error;
    SectorTopologyCoordPoint snappedPoint;
    if (!SnapAuthoringVertexMoveTarget(ScreenToMap(input.MousePosition()), snappedPoint, error)) {
        manipulationState.authoringVertexDrag.errorMessage = error;
        manipulationState.authoringVertexDrag.hasPreviewPoint = false;
        statusText = TextFormat("Authoring move rejected: %s", error.c_str());
        return;
    }

    manipulationState.authoringVertexDrag.previewPoint = snappedPoint;
    manipulationState.authoringVertexDrag.hasPreviewPoint = true;
    manipulationState.authoringVertexDrag.errorMessage.clear();
    if (SameTopologyPoint(snappedPoint, manipulationState.authoringVertexDrag.originalPoint)) {
        statusText = "Moving authoring vertex: original point";
    } else {
        statusText = TextFormat("Moving authoring vertex %d", manipulationState.authoringVertexDrag.vertexId);
    }
}

void SectorEditor::FinishAuthoringVertexDrag()
{
    if (!manipulationState.authoringVertexDrag.active) {
        return;
    }

    const int vertexId = manipulationState.authoringVertexDrag.vertexId;
    const SectorTopologyCoordPoint original = manipulationState.authoringVertexDrag.originalPoint;
    const SectorTopologyCoordPoint target = manipulationState.authoringVertexDrag.previewPoint;
    if (!manipulationState.authoringVertexDrag.hasPreviewPoint) {
        const std::string error = manipulationState.authoringVertexDrag.errorMessage.empty()
                ? "Move target is outside authoring coordinate range"
                : manipulationState.authoringVertexDrag.errorMessage;
        manipulationState.authoringVertexDrag = AuthoringVertexDragState{};
        statusText = TextFormat("Authoring move rejected: %s", error.c_str());
        return;
    }

    if (SameTopologyPoint(target, original)) {
        manipulationState.authoringVertexDrag = AuthoringVertexDragState{};
        statusText = "Authoring vertex unchanged";
        return;
    }

    if (!MoveSectorEditorAuthoringVertex(
                state,
                Lifecycle(),
                TopologyMap(),
                AuthoringGraph(),
                MakeLiveDerivationAccess(documentState.derivation),
                selectionState,
                vertexId,
                target)) {
        manipulationState.authoringVertexDrag = AuthoringVertexDragState{};
        statusText = "Authoring vertex move rejected";
        return;
    }

    SelectAuthoringVertex(vertexId);
    manipulationState.authoringVertexDrag = AuthoringVertexDragState{};
    const std::string fallbackStatus =
            TextFormat("Moved authoring vertex %d", vertexId);
    statusText = BuildSectorEditorAuthoringDerivationDisplayStatus(
            MakeLiveConstDerivationAccess(documentState.derivation),
            fallbackStatus.c_str());
}

void SectorEditor::CancelAuthoringVertexDrag(const char* message)
{
    manipulationState.authoringVertexDrag = AuthoringVertexDragState{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::StartLightDrag(int topologyLightId, SpotLightHandle spotHandle)
{
    const bool staticSpotSelected = selectionState.topologySelectionKind == TopologySelectionKind::StaticSpotLight
            && selectionState.selectedTopologyStaticSpotLightId == topologyLightId;
    const bool dynamicSpotSelected = selectionState.topologySelectionKind == TopologySelectionKind::DynamicSpotLight
            && selectionState.selectedTopologyDynamicSpotLightId == topologyLightId;
    const bool dynamicLightSelected = selectionState.topologySelectionKind == TopologySelectionKind::DynamicLight
            && selectionState.selectedTopologyDynamicLightId == topologyLightId;

    if (staticSpotSelected || state.currentTool == SectorEditorTool::StaticSpotLight) {
        const SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(
                TopologyMap(),
                topologyLightId);
        if (light == nullptr) {
            return;
        }

        SelectTopologyStaticSpotLight(topologyLightId);
        SectorEditorLightEditingService lightEditing = BuildLightEditingService();
        if (!lightEditing.BeginLightDrag(TopologySelectionKind::StaticSpotLight, topologyLightId, spotHandle)) {
            return;
        }
        lightEditingState.lightDrag.active = true;
        lightEditingState.lightDrag.topologyLightId = topologyLightId;
        lightEditingState.lightDrag.spotHandle = spotHandle;
        lightEditingState.lightDrag.snappedPosition = spotHandle == SpotLightHandle::Target
                ? light->target
                : light->position;
        return;
    }

    if (dynamicSpotSelected || state.currentTool == SectorEditorTool::DynamicSpotLight) {
        const SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(
                TopologyMap(),
                topologyLightId);
        if (light == nullptr) {
            return;
        }

        SelectTopologyDynamicSpotLight(topologyLightId);
        SectorEditorLightEditingService lightEditing = BuildLightEditingService();
        if (!lightEditing.BeginLightDrag(TopologySelectionKind::DynamicSpotLight, topologyLightId, spotHandle)) {
            return;
        }
        lightEditingState.lightDrag.active = true;
        lightEditingState.lightDrag.topologyLightId = topologyLightId;
        lightEditingState.lightDrag.spotHandle = spotHandle;
        lightEditingState.lightDrag.snappedPosition = spotHandle == SpotLightHandle::Target
                ? light->target
                : light->position;
        return;
    }

    if (dynamicLightSelected || state.currentTool == SectorEditorTool::DynamicLight) {
        const SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                TopologyMap(),
                topologyLightId);
        if (light == nullptr) {
            return;
        }

        SelectTopologyDynamicLight(topologyLightId);
        SectorEditorLightEditingService lightEditing = BuildLightEditingService();
        if (!lightEditing.BeginLightDrag(TopologySelectionKind::DynamicLight, topologyLightId, SpotLightHandle::Origin)) {
            return;
        }
        lightEditingState.lightDrag.active = true;
        lightEditingState.lightDrag.topologyLightId = topologyLightId;
        lightEditingState.lightDrag.spotHandle = SpotLightHandle::Origin;
        lightEditingState.lightDrag.snappedPosition = light->position;
        return;
    }

    const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            TopologyMap(),
            topologyLightId);
    if (light == nullptr) {
        return;
    }
    SelectTopologyLight(topologyLightId);
    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    if (!lightEditing.BeginLightDrag(TopologySelectionKind::StaticLight, topologyLightId, SpotLightHandle::Origin)) {
        return;
    }
    lightEditingState.lightDrag.active = true;
    lightEditingState.lightDrag.topologyLightId = topologyLightId;
    lightEditingState.lightDrag.spotHandle = SpotLightHandle::Origin;
    lightEditingState.lightDrag.snappedPosition = light->position;
}

void SectorEditor::UpdateLightDrag(engine::Input& input)
{
    if (!lightEditingState.lightDrag.active) {
        return;
    }

    const Vector2 snapped = SnapMapPoint(ScreenToMap(input.MousePosition()));
    lightEditingState.lightDrag.snappedPosition = Vector3{
            snapped.x,
            0.0f,
            snapped.y};
    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    lightEditing.ApplyLightDragToSnappedPosition(lightEditingState.lightDrag.snappedPosition);
}

void SectorEditor::FinishLightDrag()
{
    if (!lightEditingState.lightDrag.active) {
        return;
    }

    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    const SectorEditorLightMutationResult result = lightEditing.FinishLightDrag();
    lightEditingState.lightDrag = LightDragState{};
    if (result.dynamicLightRendererRefreshNeeded) {
        sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
    }
}

void SectorEditor::CancelLightDrag(const char* message)
{
    if (lightEditingState.lightDrag.active) {
        SectorEditorLightEditingService lightEditing = BuildLightEditingService();
        const SectorEditorLightMutationResult result = lightEditing.CancelLightDragData(message);
        if (result.dynamicLightRendererRefreshNeeded) {
            sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
        }
    }

    lightEditingState.lightDrag = LightDragState{};
}

void SectorEditor::StartRuntimeObjectDrag(int objectId)
{
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    editing.BeginDrag(objectId);
}

void SectorEditor::UpdateRuntimeObjectDrag(engine::Input& input)
{
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    editing.UpdateDrag(SnapMapPoint(ScreenToMap(input.MousePosition())));
}

void SectorEditor::FinishRuntimeObjectDrag()
{
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    editing.FinishDrag();
}

void SectorEditor::CancelRuntimeObjectDrag(const char* message)
{
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    editing.CancelDrag(message);
}

void SectorEditor::UpdatePreview3D(engine::Input& input, engine::AssetManager& assets, float dt)
{
    bool controlModeToggled = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, &assets, &controlModeToggled](engine::InputEvent& event) {
                if (event.key.key == KEY_F1) {
                    previewState.overlay.useBakedAmbientOcclusion = !previewState.overlay.useBakedAmbientOcclusion;
                    statusText = previewState.overlay.useBakedAmbientOcclusion
                            ? "Baked AO enabled"
                            : "Baked AO disabled";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_F2) {
                    if (lightEditingState.proxyPlacement.active) {
                        statusText = "Finish proxy placement before hiding the 3D UI";
                        engine::ConsumeEvent(event);
                        return;
                    }
                    previewState.overlay.previewUiHidden = !previewState.overlay.previewUiHidden;
                    if (previewState.overlay.previewUiHidden) {
                        previewState.selection.hoveredSurface3D = SectorSurfaceHit{};
                    }
                    statusText = previewState.overlay.previewUiHidden
                            ? "3D UI hidden"
                            : "3D UI shown";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_F3) {
                    if (lightEditingState.lightPilot.active
                            || lightEditingState.proxyPlacement.active) {
                        statusText = "Finish light editing before changing 3D control mode";
                        engine::ConsumeEvent(event);
                        return;
                    }
                    TogglePreviewControlMode();
                    controlModeToggled = true;
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_F4) {
                    sceneRuntime.Renderer().ToggleDynamicLightingEnabled();
                    statusText = sceneRuntime.Renderer().DynamicLightingEnabled()
                            ? "Dynamic lighting enabled"
                            : "Dynamic lighting disabled";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_H) {
                    if (ToggleFpsViewmodelHolster(fpsPlayer.State(), true, uiState.keyboardCaptured)) {
                        statusText = fpsPlayer.State().equipState
                                        == FpsViewmodelEquipState::Holstering
                                ? "Viewmodel holstering"
                                : "Viewmodel unholstering";
                        engine::ConsumeEvent(event);
                    }
                    return;
                }

                if (event.key.key == KEY_TAB || event.key.key == KEY_ESCAPE) {
                    CancelLightProxyPlacement(nullptr);
                    CancelLightPilotWithPreviewRestore(nullptr);
                    LeavePreview3D();
                    engine::ConsumeEvent(event);
                }
            }
    );

    if (controlModeToggled) {
        return;
    }

    if (state.mode == SectorEditorMode::Preview3D) {
        if (previewState.controller.previewControlMode == SectorPreviewControlMode::FreeFly) {
            const bool precisionMove = input.IsKeyDown(KEY_LEFT_SHIFT)
                    || input.IsKeyDown(KEY_RIGHT_SHIFT);
            UpdateSectorFreeflyController(
                    previewState.controller.freeflyController,
                    input,
                    dt,
                    precisionMove ? SectorEditorFreeflyPrecisionMoveScale : 1.0f);
            sceneRuntime.Renderer().ApplyRendererPose(
                    previewState.controller.freeflyController.pose,
                    false);
        } else {
            const float previousVisualEyeY = sceneRuntime.Renderer().RendererPose().position.y;
            input.ForEachEvent(
                    engine::InputEventType::KeyPressed,
                    true,
                    [this](engine::InputEvent& event) {
                        if (event.key.key != KEY_F11) {
                            return;
                        }

                        SetSectorFreeflyMouseLookEnabled(
                                previewState.controller.freeflyController,
                                !previewState.controller.freeflyController.mouseLookEnabled);
                        engine::ConsumeEvent(event);
                    }
            );

            SectorFpsControllerInput controllerInput;
            controllerInput.moveForward = input.IsKeyDown(KEY_W);
            controllerInput.moveBackward = input.IsKeyDown(KEY_S);
            controllerInput.strafeLeft = input.IsKeyDown(KEY_A);
            controllerInput.strafeRight = input.IsKeyDown(KEY_D);
            controllerInput.run = input.IsKeyDown(KEY_LEFT_SHIFT) || input.IsKeyDown(KEY_RIGHT_SHIFT);
            controllerInput.mouseLookEnabled = previewState.controller.freeflyController.mouseLookEnabled;
            controllerInput.mouseDelta = input.MouseDelta();
            const bool canConsumeGameplayActions =
                    state.mode == SectorEditorMode::Preview3D
                    && previewState.controller.previewControlMode == SectorPreviewControlMode::Gameplay
                    && !uiState.keyboardCaptured
                    && !state.texturePicker.open
                    && !state.soundPicker.open
                    && !state.decalTintModal.open
                    && !state.previewSettingsModal.open;
            if (canConsumeGameplayActions) {
                input.ForEachEvent(
                        engine::InputEventType::KeyPressed,
                        true,
                        [&controllerInput](engine::InputEvent& event) {
                            if (event.key.key == KEY_SPACE) {
                                controllerInput.jumpPressed = true;
                            } else if (event.key.key == KEY_LEFT_CONTROL
                                    || event.key.key == KEY_RIGHT_CONTROL) {
                                controllerInput.crouchTogglePressed = true;
                            } else {
                                return;
                            }
                            engine::ConsumeEvent(event);
                        }
                );
            }
            UpdateSectorEditorGameplayPreview(
                    sceneRuntime.RuntimeObjects().dynamicDoorColliders,
                    sceneRuntime.RuntimeObjects().staticModelColliders,
                    previewState.collision,
                    previewState.controller,
                    state.previewSettingsModal.open,
                    controllerInput,
                    previousVisualEyeY,
                    dt,
                    &sceneRuntime.NpcNavigation().collisionCylinders);
            if (previewState.controller.frameEvents.footstep
                    && engineContext != nullptr) {
                sceneRuntime.PlayFootstepForSector(
                        *engineContext,
                        previewState.controller.fpsControllerState.currentSectorId,
                        applicationSettings.footsteps.volume);
            }
            if (engineContext != nullptr) {
                if (previewState.controller.frameEvents.jumped) {
                    PlayPlayerSound(
                            engineContext->assets,
                            engineContext->audio,
                            playerAudio,
                            "jump");
                }
                if (previewState.controller.frameEvents.landed) {
                    PlayPlayerSound(
                            engineContext->assets,
                            engineContext->audio,
                            playerAudio,
                            "land");
                    sceneRuntime.PlayFootstepForSector(
                            *engineContext,
                            previewState.controller.fpsControllerState
                                    .currentSectorId,
                            std::clamp(
                                    applicationSettings.footsteps.volume
                                            * applicationSettings.footsteps
                                                    .landingImpactVolumeMultiplier,
                                    0.0f,
                                    1.0f));
                }
            }
            ApplyGameplayPoseToPreview();
            previewState.controller.freeflyController.pose =
                    ActiveSectorEditorPreviewPose(
                            previewState.controller,
                            sceneRuntime.Renderer());
        }
    }
}

void SectorEditor::UpdatePreview3DSelection(engine::Input& input)
{
    if (!initialized
            || !sceneRuntime.Renderer().IsRendererReady()
            || previewState.controller.freeflyController.mouseLookEnabled
            || previewState.overlay.previewUiHidden
            || state.texturePicker.open
            || state.soundPicker.open
            || lightEditingState.lightPilot.active
            || lightEditingState.proxyPlacement.active) {
        previewState.selection.hoveredSurface3D = SectorSurfaceHit{};
        return;
    }

    const Rectangle viewport{0.0f, 0.0f, EditorWidth, EditorHeight};
    const Vector2 mouse = input.MousePosition();
    const bool overPanel = IsValidTopologySurfaceEditTarget(previewState.selection.selectedTopologySurface3D)
            && Contains(BuildPreviewUvPanelRect(), mouse);
    const bool overPreviewOverlay = IsPreviewOverlayMouseInteractive()
            && Contains(BuildPreviewOverlayInteractionRect(), mouse);
    previewState.selection.hoveredSurface3D = overPanel
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
                if (previewState.selection.hoveredSurface3D.hit) {
                    SelectSurface3D(previewState.selection.hoveredSurface3D.surface);
                    statusText = TextFormat("Selected 3D %s", SurfaceKindName(previewState.selection.hoveredSurface3D.surface.kind));
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
    SectorAuthoringGraph& authoringGraph = AuthoringGraph();
    if (FindSectorAuthoringLine(authoringGraph, lineId) == nullptr) {
        statusText = "Insert Vertex: select or click an authoring line";
        return;
    }

    if (state.pendingAuthoringLine.active) {
        CancelPendingAuthoringLine("Cancelled authoring line");
    }
    if (state.pendingAuthoringRectangle.active) {
        CancelPendingAuthoringRectangle("Rectangle cancelled");
    }
    if (manipulationState.authoringVertexDrag.active) {
        CancelAuthoringVertexDrag("Cancelled authoring vertex move");
    }
    if (lightEditingState.lightDrag.active) {
        CancelLightDrag("Cancelled light move");
    }

    ClearTopologySelectionOnly();
    SelectSectorEditorAuthoringLine(authoringGraph, selectionState, lineId);
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

    const SectorAuthoringGraph& authoringGraph = AuthoringGraph();
    const SectorAuthoringLine* line = FindSectorAuthoringLine(authoringGraph, lineId);
    if (line == nullptr) {
        error = "Insert Vertex: select or click an authoring line";
        return false;
    }

    const SectorAuthoringVertex* start =
            FindSectorAuthoringVertex(authoringGraph, line->startVertexId);
    const SectorAuthoringVertex* end =
            FindSectorAuthoringVertex(authoringGraph, line->endVertexId);
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
            state.currentTool,
            TopologyMap(),
            AuthoringGraph(),
            manipulationState,
            lightEditingState,
            runtimeObjectEditingState.drag,
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
    context.placedObjectMoveProvider = nullptr;
    context.fogVolumeEditing = fogVolumeEditingService
            ? &fogVolumeEditingService.value()
            : nullptr;
    context.levelMarkerEditing = levelMarkerEditingService
            ? &levelMarkerEditingService.value()
            : nullptr;
    context.triggerEditing = triggerEditingService ? &triggerEditingService.value() : nullptr;
    context.screenToMap = [this](Vector2 screenPoint) {
        return ScreenToMap(screenPoint);
    };
    context.snapMapPoint = [this](Vector2 mapPoint) {
        return SnapMapPoint(mapPoint);
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
    return BuildSelectionServiceContextFromState(
            state,
            TopologyMap(),
            AuthoringGraph(),
            MakeLiveConstDerivationAccess(documentState.derivation),
            previewState.selection,
            selectionState,
            manipulationState,
            runtimeObjectEditingState,
            runtimeObjectEditingUiState,
            uiState,
            inspectorIdUiState,
            materialEditingUiState,
            &statusText,
            this,
            [](void* userData, const char* message) {
                SectorEditor* editor = static_cast<SectorEditor*>(userData);
                editor->CancelLightProxyPlacement(message);
                editor->CancelLightPilotWithPreviewRestore(message);
            },
            &lightEditingState);
}

SectorTopologySector* SectorEditor::SelectedTopologySector()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologySector(context);
}

const SectorTopologySector* SectorEditor::SelectedTopologySector() const
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContextFromState(
            const_cast<SectorEditorState&>(state),
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            MakeLiveConstDerivationAccess(documentState.derivation),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            const_cast<RuntimeObjectEditingState&>(runtimeObjectEditingState),
            const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
            const_cast<SectorEditorUiState&>(uiState),
            const_cast<InspectorIdUiState&>(inspectorIdUiState),
            const_cast<MaterialEditingUiState&>(materialEditingUiState));
    return SelectedSectorEditorTopologySector(context);
}

SectorTopologyVertex* SectorEditor::SelectedTopologyVertex()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyVertex(context);
}

const SectorTopologyVertex* SectorEditor::SelectedTopologyVertex() const
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContextFromState(
            const_cast<SectorEditorState&>(state),
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            MakeLiveConstDerivationAccess(documentState.derivation),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            const_cast<RuntimeObjectEditingState&>(runtimeObjectEditingState),
            const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
            const_cast<SectorEditorUiState&>(uiState),
            const_cast<InspectorIdUiState&>(inspectorIdUiState),
            const_cast<MaterialEditingUiState&>(materialEditingUiState));
    return SelectedSectorEditorTopologyVertex(context);
}

SectorTopologySideDef* SectorEditor::SelectedTopologySideDef()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologySideDef(context);
}

const SectorTopologySideDef* SectorEditor::SelectedTopologySideDef() const
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContextFromState(
            const_cast<SectorEditorState&>(state),
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            MakeLiveConstDerivationAccess(documentState.derivation),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            const_cast<RuntimeObjectEditingState&>(runtimeObjectEditingState),
            const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
            const_cast<SectorEditorUiState&>(uiState),
            const_cast<InspectorIdUiState&>(inspectorIdUiState),
            const_cast<MaterialEditingUiState&>(materialEditingUiState));
    return SelectedSectorEditorTopologySideDef(context);
}

SectorTopologyLineDef* SectorEditor::SelectedTopologyLineDef()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyLineDef(context);
}

const SectorTopologyLineDef* SectorEditor::SelectedTopologyLineDef() const
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContextFromState(
            const_cast<SectorEditorState&>(state),
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            MakeLiveConstDerivationAccess(documentState.derivation),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            const_cast<RuntimeObjectEditingState&>(runtimeObjectEditingState),
            const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
            const_cast<SectorEditorUiState&>(uiState),
            const_cast<InspectorIdUiState&>(inspectorIdUiState),
            const_cast<MaterialEditingUiState&>(materialEditingUiState));
    return SelectedSectorEditorTopologyLineDef(context);
}

SectorTopologyStaticPointLight* SectorEditor::SelectedTopologyLight()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyLight(context);
}

const SectorTopologyStaticPointLight* SectorEditor::SelectedTopologyLight() const
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContextFromState(
            const_cast<SectorEditorState&>(state),
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            MakeLiveConstDerivationAccess(documentState.derivation),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            const_cast<RuntimeObjectEditingState&>(runtimeObjectEditingState),
            const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
            const_cast<SectorEditorUiState&>(uiState),
            const_cast<InspectorIdUiState&>(inspectorIdUiState),
            const_cast<MaterialEditingUiState&>(materialEditingUiState));
    return SelectedSectorEditorTopologyLight(context);
}

SectorTopologyStaticSpotLight* SectorEditor::SelectedTopologyStaticSpotLight()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyStaticSpotLight(context);
}

const SectorTopologyStaticSpotLight* SectorEditor::SelectedTopologyStaticSpotLight() const
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContextFromState(
            const_cast<SectorEditorState&>(state),
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            MakeLiveConstDerivationAccess(documentState.derivation),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            const_cast<RuntimeObjectEditingState&>(runtimeObjectEditingState),
            const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
            const_cast<SectorEditorUiState&>(uiState),
            const_cast<InspectorIdUiState&>(inspectorIdUiState),
            const_cast<MaterialEditingUiState&>(materialEditingUiState));
    return SelectedSectorEditorTopologyStaticSpotLight(context);
}

SectorTopologyDynamicPointLight* SectorEditor::SelectedTopologyDynamicLight()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyDynamicLight(context);
}

const SectorTopologyDynamicPointLight* SectorEditor::SelectedTopologyDynamicLight() const
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContextFromState(
            const_cast<SectorEditorState&>(state),
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            MakeLiveConstDerivationAccess(documentState.derivation),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            const_cast<RuntimeObjectEditingState&>(runtimeObjectEditingState),
            const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
            const_cast<SectorEditorUiState&>(uiState),
            const_cast<InspectorIdUiState&>(inspectorIdUiState),
            const_cast<MaterialEditingUiState&>(materialEditingUiState));
    return SelectedSectorEditorTopologyDynamicLight(context);
}

SectorTopologyDynamicSpotLight* SectorEditor::SelectedTopologyDynamicSpotLight()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorTopologyDynamicSpotLight(context);
}

const SectorTopologyDynamicSpotLight* SectorEditor::SelectedTopologyDynamicSpotLight() const
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContextFromState(
            const_cast<SectorEditorState&>(state),
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            MakeLiveConstDerivationAccess(documentState.derivation),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            const_cast<RuntimeObjectEditingState&>(runtimeObjectEditingState),
            const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
            const_cast<SectorEditorUiState&>(uiState),
            const_cast<InspectorIdUiState&>(inspectorIdUiState),
            const_cast<MaterialEditingUiState&>(materialEditingUiState));
    return SelectedSectorEditorTopologyDynamicSpotLight(context);
}

SectorPlacedRuntimeObject* SectorEditor::SelectedRuntimeObject()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    return SelectedSectorEditorRuntimeObject(context);
}

const SectorPlacedRuntimeObject* SectorEditor::SelectedRuntimeObject() const
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContextFromState(
            const_cast<SectorEditorState&>(state),
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            MakeLiveConstDerivationAccess(documentState.derivation),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            const_cast<RuntimeObjectEditingState&>(runtimeObjectEditingState),
            const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
            const_cast<SectorEditorUiState&>(uiState),
            const_cast<InspectorIdUiState&>(inspectorIdUiState),
            const_cast<MaterialEditingUiState&>(materialEditingUiState));
    return SelectedSectorEditorRuntimeObject(context);
}

void SectorEditor::ClearStaleTopologySelection()
{
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContext();
    ClearStaleSectorEditorTopologySelection(context);
}

void SectorEditor::MarkTopologyDocumentEdited(const char* status)
{
    MarkSectorEditorTopologyDocumentEdited(
            Lifecycle(),
            state.topologyRenderRevision,
            state.topologyRenderCache,
            statusText,
            status);
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
            Lifecycle(),
            TopologyMap(),
            AuthoringGraph(),
            MakeLiveDerivationAccess(documentState.derivation),
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
    previewState.selection.hoveredSurface3D = SectorSurfaceHit{};
    previewState.selection.selectedSurface3D = SectorSurfaceRef{};
    previewState.selection.selectedTopologySurface3D = TopologySurfaceEditTarget{};
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
        inspectorIdUiState.idEditError = "No topology sector selected";
        statusText = inspectorIdUiState.idEditError;
        return false;
    }

    const std::string newName = inspectorIdUiState.selectedSectorIdBuffer;
    if (newName == sector->name) {
        inspectorIdUiState.idEditError.clear();
        return true;
    }

    const bool hasAuthoringGraph = HasAuthoringGraphData();
    if (!hasAuthoringGraph) {
        inspectorIdUiState.idEditError = "Cannot edit sector property: authoring data is required.";
        statusText = inspectorIdUiState.idEditError;
        return true;
    }
    const SectorEditorConstDerivationDocumentAccess derivation =
            MakeLiveConstDerivationAccess(documentState.derivation);
    if (hasAuthoringGraph && !IsSectorEditorAuthoringDerivationCurrent(derivation)) {
        inspectorIdUiState.idEditError = "Sector name edit unavailable: derived topology is not current";
        statusText = inspectorIdUiState.idEditError;
        return true;
    }

    const bool hasFaceAnchorMapping =
            FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                    AuthoringGraph(),
                    derivation.authoringDerivation,
                    sector->id) >= 0;
    if (hasFaceAnchorMapping) {
        MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                state,
                Lifecycle(),
                TopologyMap(),
                AuthoringGraph(),
                MakeLiveDerivationAccess(documentState.derivation),
                sector->id,
                TextFormat("Renamed authoring face anchor %d", sector->id),
                [&newName](SectorAuthoringFaceAnchor& anchor) {
                    if (anchor.name == newName) {
                        return false;
                    }
                    anchor.name = newName;
                    return true;
                });
        inspectorIdUiState.idEditError.clear();
        return true;
    }
    inspectorIdUiState.idEditError = "Sector name edit unavailable: selected sector has no face anchor mapping";
    statusText = inspectorIdUiState.idEditError;
    return true;
}

bool SectorEditor::OpenDeleteSelectedLightConfirmation()
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
            [this]() {
                SectorEditorLightEditingService lightEditing = BuildLightEditingService();
                const SectorEditorLightMutationResult result = lightEditing.DeleteSelectedLightConfirmed();
                if (result.previewPoseRestoreNeeded && state.mode == SectorEditorMode::Preview3D) {
                    ResetSectorFreeflyController(
                            previewState.controller.freeflyController,
                            previewState.controller.lightPilotPreviewRestore.originalPreviewPose);
                    SetSectorFreeflyMouseLookEnabled(
                            previewState.controller.freeflyController,
                            previewState.controller.lightPilotPreviewRestore.originalMouseLookEnabled);
                    previewState.controller.lightPilotPreviewRestore = LightPilotPreviewRestoreState{};
                    sceneRuntime.Renderer().ApplyRendererPose(previewState.controller.freeflyController.pose);
                }
                if (result.dynamicLightRendererRefreshNeeded) {
                    sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
                }
            });
    return true;
}

SectorEditorRuntimeObjectEditingService
SectorEditor::BuildRuntimeObjectEditingService(
        SectorEditorSelectionServiceContext* selectionService)
{
    return SectorEditorRuntimeObjectEditingService{
        SectorEditorRuntimeObjectEditingServiceContext{
            TopologyMap(),
            sceneRuntime.RuntimeObjects(),
            runtimeObjectEditingState,
            runtimeObjectEditingUiState,
            selectionState,
            selectionService,
            Lifecycle(),
            state.topologyRenderRevision,
            state.topologyRenderCache,
            statusText,
            engineContext,
            IsSectorEditorAuthoringDerivationCurrent(
                    MakeLiveConstDerivationAccess(documentState.derivation))}};
}

SectorEditorSoundService SectorEditor::BuildSoundService(
        SectorEditorRuntimeObjectEditingService* runtimeObjectEditing)
{
    return SectorEditorSoundService{
            SectorEditorSoundServiceContext{
                    state,
                    Lifecycle(),
                    TopologyMap(),
                    soundCatalogState,
                    audioAssetPickerSessionState,
                    statusText,
                    *engineContext,
                    runtimeObjectEditing}};
}

void SectorEditor::AddRuntimeObjectAt(Vector2 mapPoint)
{
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    editing.AddBillboard(FindTopologySectorAt(mapPoint), mapPoint);
}

void SectorEditor::AddStaticModelAt(Vector2 mapPoint)
{
    EnsureTopologyRenderCache();
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    editing.AddStaticModel(mapPoint);
}

void SectorEditor::AddDynamicModelAt(Vector2 mapPoint)
{
    EnsureTopologyRenderCache();
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    editing.AddDynamicModel(mapPoint);
}

void SectorEditor::AddNpcAt(Vector2 mapPoint)
{
    EnsureTopologyRenderCache();
    SectorRuntimeObjectState& runtimeObjects = sceneRuntime.RuntimeObjects();
    RefreshSectorEditorNpcPlacementOptions(
            runtimeObjectEditingState.npcPlacement,
            runtimeObjects.npcDefinitionCatalog,
            runtimeObjects.npcDefinitionCatalogRevision);
    const std::string definitionId = ResolveSectorEditorNpcPlacementDefault(
            runtimeObjectEditingState.npcPlacement);
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    editing.AddNpc(mapPoint, definitionId);
}

void SectorEditor::AddDoorAtPortal(Vector2 screenPoint)
{
    const Vector2 mapPoint = ScreenToMap(screenPoint);
    int lineDefId = -1;
    int sideDefId = -1;
    SectorTopologySideKind side = SectorTopologySideKind::Front;
    bool preferredMissing = false;
    if (!FindTopologyLineNearScreenPoint(
                screenPoint,
                mapPoint,
                lineDefId,
                sideDefId,
                side,
                preferredMissing)) {
        statusText = "Door placement failed: click a two-sided portal";
        return;
    }
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    editing.AddDoor(lineDefId);
}

bool SectorEditor::DeleteSelectedRuntimeObject()
{
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    const SectorEditorRuntimeObjectDeleteRequest confirmation =
            editing.RequestDeleteSelected();
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
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    return editing.DeleteById(objectId);
}

bool SectorEditor::MutateSelectedRuntimeObject(
        const char* status,
        const std::function<bool(SectorPlacedRuntimeObject&)>& mutate)
{
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    return editing.MutateSelected(status, mutate);
}

void SectorEditor::RefreshRuntimeObjectsAfterAuthoringEdit()
{
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService();
    editing.RefreshPreviewObjects();
}

bool SectorEditor::StartLightmapBake()
{
    if (!lightmapBake.CanStart()) {
        statusText = "Lightmap bake already running";
        return false;
    }

    if (!Lifecycle().hasCurrentLevelPath) {
        statusText = "Save the level before baking lightmaps";
        return false;
    }

    std::string gateMessage;
    if (!CanUseCurrentAuthoringDerivedTopologyForLightmapBake(
                MakeLiveConstDerivationAccess(documentState.derivation),
                &gateMessage)) {
        statusText = gateMessage.empty() ? "Bake failed: derived topology is not current" : gateMessage;
        return false;
    }

    if (TopologyMap().sectors.empty()) {
        statusText = "Bake failed: no sectors";
        return false;
    }

    LevelPaths levelPaths;
    std::string pathError;
    if (!BuildLevelPaths(Lifecycle().currentLevelName, levelPaths, pathError)) {
        statusText = TextFormat("Bake failed: %s", pathError.c_str());
        return false;
    }
    const std::string finalOutputPath = levelPaths.lightmapFilePath.string();
    const std::string temporaryOutputPath = MakeTemporaryLightmapPath(finalOutputPath);

    if (engineContext == nullptr) {
        statusText = "Bake failed: asset manager is unavailable";
        return false;
    }
    SectorStaticModelLightmapData preparedStaticModels;
    std::string staticModelError;
    if (!PrepareSectorStaticModelsForLightmapBake(
                TopologyMap(),
                engineContext->assets,
                {},
                preparedStaticModels,
                staticModelError)) {
        statusText = staticModelError.empty()
                ? "Bake failed: could not prepare static models"
                : staticModelError;
        return false;
    }

    SectorEditorLightmapBakeRequest request;
    request.mapSnapshot = TopologyMap();
    request.staticModels = std::move(preparedStaticModels);
    request.expectedSourceHash = ComputeSectorLightmapSourceHash(TopologyMap());
    request.finalOutputPath = finalOutputPath;
    request.temporaryOutputPath = temporaryOutputPath;

    std::string startStatus;
    if (!lightmapBake.StartBake(std::move(request), startStatus)) {
        statusText = startStatus.empty() ? "Lightmap bake already running" : startStatus;
        return false;
    }

    statusText = startStatus.empty() ? "Baking lightmap..." : startStatus;
    return true;
}

void SectorEditor::PollLightmapBakeResult(engine::AssetManager& assets)
{
    SectorEditorLightmapBakePollResult pollResult = lightmapBake.Poll();
    if (pollResult.status == SectorEditorLightmapBakePollStatus::None) {
        return;
    }

    if (pollResult.status == SectorEditorLightmapBakePollStatus::Cancelled
            || pollResult.status == SectorEditorLightmapBakePollStatus::Failed) {
        statusText = pollResult.message;
        return;
    }

    if (pollResult.status == SectorEditorLightmapBakePollStatus::Completed
            && pollResult.completedResult.has_value()) {
        const bool installed = InstallLightmapBakeResult(*pollResult.completedResult, assets);
        lightmapBake.CompleteInstall(installed);
    }
}

bool SectorEditor::InstallLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets)
{
    (void)assets;

    SectorEditorLightmapBakeInstallPayload installPayload;
    if (!lightmapBake.InstallCompletedResultFiles(
                result,
                ComputeSectorLightmapSourceHash(TopologyMap()),
                installPayload)) {
        statusText = installPayload.status;
        return false;
    }

    LevelPaths levelPaths;
    std::string pathError;
    if (!BuildLevelPaths(Lifecycle().currentLevelName, levelPaths, pathError)) {
        statusText = TextFormat("Bake failed: %s", pathError.c_str());
        return false;
    }
    SectorLightmapMetadata installedMetadata;
    installedMetadata.path = levelPaths.lightmapAssetPath;
    installedMetadata.width = installPayload.bakeResult.width;
    installedMetadata.height = installPayload.bakeResult.height;
    installedMetadata.version = installPayload.bakeResult.artifactVersion;
    installedMetadata.format = installPayload.bakeResult.artifactFormat;
    installedMetadata.sourceHash = installPayload.bakeResult.sourceHash;
    installedMetadata.storedStatistics =
            installPayload.bakeResult.storedAtlasStatistics;
    if (installPayload.bakeResult.atlases.size() > 1) {
        installedMetadata.additionalAtlases.assign(
                installPayload.bakeResult.atlases.begin() + 1,
                installPayload.bakeResult.atlases.end());
    }
    installedMetadata.objectProbes = installPayload.bakeResult.objectProbes;
    installedMetadata.staticModels =
            installPayload.bakeResult.staticModels;
    // Data files are installed and validated before this single metadata publish.
    TopologyMap().bakedLightmap = std::move(installedMetadata);
    Lifecycle().hasUnsavedChanges = true;
    Lifecycle().topologyDocumentDirty = true;

    std::istringstream report(result.bakeReportText);
    std::string line;
    while (std::getline(report, line)) {
        TraceLog(LOG_INFO, "%s", line.c_str());
    }
    TraceLog(LOG_INFO, "INFO: Lightmap bake completed asynchronously in %.2fs", result.bakeResult.totalBakeSeconds);

    if (state.mode == SectorEditorMode::Preview3D && sceneRuntime.Renderer().IsRendererReady()) {
        if (engineContext != nullptr) {
            RebuildPreviewMeshesPreservingView(*engineContext);
        }
    }

    const size_t atlasCount = std::max<size_t>(
            1,
            installPayload.bakeResult.atlases.size());
    statusText = atlasCount == 1
            ? TextFormat(
                    "Baked lightmap in %.1fs",
                    result.bakeResult.totalBakeSeconds)
            : TextFormat(
                    "Baked %zu lightmap atlases in %.1fs",
                    atlasCount,
                    result.bakeResult.totalBakeSeconds);
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

    for (const SectorTopologyVertex& vertex : TopologyMap().vertices) {
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
    (void)assets;
    if (engineContext != nullptr) {
        sceneRuntime.RenderShadowMaps(*engineContext);
    }
}

void SectorEditor::RenderPreview3DScene(engine::EngineContext& context)
{
    sceneRuntime.Renderer().SetPbrDiagnosticSelectedObjectId(
            selectionState.selectedRuntimeObjectId);
    sceneRuntime.RenderScene(
            context,
            TopologyMap(),
            previewState.overlay.useBakedAmbientOcclusion);
}

void SectorEditor::RenderPreview3DViewmodel(
        engine::AssetManager& assets)
{
    if (state.mode != SectorEditorMode::Preview3D) {
        return;
    }
    const int preferredSectorId =
            previewState.controller.previewControlMode
                            == SectorPreviewControlMode::Gameplay
                    ? previewState.controller.fpsControllerState
                            .currentSectorId
                    : 0;
    fpsPlayer.Render(
            assets,
            sceneRuntime.Renderer(),
            TopologyMap(),
            sceneRuntime.RuntimeObjects(),
            preferredSectorId);
}
void SectorEditor::ApplyPreview3DWorldAtmosphere(
        engine::RenderTarget& sceneTarget,
        bool collectGpuDiagnostics)
{
    if (state.mode != SectorEditorMode::Preview3D) {
        return;
    }
    sceneRuntime.ApplyWorldAtmosphere(
            sceneTarget,
            TopologyMap(),
            collectGpuDiagnostics);
}

void SectorEditor::ApplyPreview3DHdrBloom(engine::RenderTarget& sceneTarget)
{
    if (state.mode != SectorEditorMode::Preview3D) {
        return;
    }
    sceneRuntime.ApplyHdrBloom(sceneTarget, applicationSettings.hdrBloom);
}

bool SectorEditor::CompositePreview3DViewmodel(
        engine::RenderTarget& sceneTarget,
        const engine::RenderTarget& viewmodelTarget)
{
    return sceneRuntime.CompositeViewmodel(sceneTarget, viewmodelTarget);
}

void SectorEditor::RenderPreview3DOverlays()
{
    if (!previewState.overlay.previewUiHidden) {
        DrawPreviewSurfaceHighlights();
        DrawPreviewSpotLightOverlay();
        DrawPreviewObjectProbeOverlay();
        DrawSectorEditorPreviewNavigationOverlay(
                previewState.overlay,
                sceneRuntime.Navigation(),
                sceneRuntime.NpcNavigation(),
                selectionState.selectedRuntimeObjectId,
                sceneRuntime.Renderer());
    }
}

void SectorEditor::RenderPreview3DHud(Rectangle playableViewport) const
{
    if (state.mode == SectorEditorMode::Preview3D) {
        fpsPlayer.RenderHud(playableViewport, weaponRegistry);
    }
}

SectorSurfaceHit SectorEditor::PickSectorSurface3D(Vector2 mousePosition, Rectangle viewportRect) const
{
    SectorSurfaceHit best;
    if (!sceneRuntime.Renderer().IsRendererReady()) {
        return best;
    }

    const Vector2 localMouse{
            mousePosition.x - viewportRect.x,
            mousePosition.y - viewportRect.y
    };
    const Ray ray = GetScreenToWorldRayEx(
            localMouse,
            sceneRuntime.Renderer().RenderCamera(),
            static_cast<int>(std::round(viewportRect.width)),
            static_cast<int>(std::round(viewportRect.height))
    );

    const SectorGeneratedSurfaceHit hit = PickSectorGeneratedGeometry(
            sceneRuntime.Renderer().RenderedGeometry(),
            ray,
            sceneRuntime.Renderer().VisibilityResult(),
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
    const SectorEditorConstDerivationDocumentAccess derivation =
            MakeLiveConstDerivationAccess(documentState.derivation);
    DrawSectorEditorPreviewSurfaceHighlights(
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            derivation.authoringDerivation,
            IsSectorEditorAuthoringDerivationCurrent(derivation),
            const_cast<RuntimeObjectDragState&>(runtimeObjectEditingState.drag),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            previewState.controller,
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            BuildSelectionUiDependencies(
                    const_cast<SectorEditorUiState&>(uiState),
                    const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
                    const_cast<InspectorIdUiState&>(inspectorIdUiState)),
            const_cast<MaterialEditingUiState&>(materialEditingUiState),
            sceneRuntime.Renderer());
}

void SectorEditor::DrawPreviewSpotLightOverlay() const
{
    DrawSectorEditorPreviewSpotLightOverlay(
            TopologyMap(),
            previewState.controller,
            selectionState,
            sceneRuntime.Renderer());
}

void SectorEditor::DrawPreviewObjectProbeOverlay() const
{
    DrawSectorEditorPreviewObjectProbeOverlay(
            TopologyMap(),
            previewState,
            sceneRuntime.RuntimeObjects(),
            sceneRuntime.Renderer());
}

void SectorEditor::RefreshPreviewObjectProbeDebugData()
{
    RefreshSectorRuntimeObjectMapData(sceneRuntime.RuntimeObjects(), TopologyMap());
}

bool SectorEditor::IsPreviewOverlayMouseInteractive() const
{
    return !previewState.controller.freeflyController.mouseLookEnabled;
}

Rectangle SectorEditor::BuildPreviewOverlayInteractionRect() const
{
    return BuildSectorEditorPreviewOverlayInteractionRect(previewState.overlay.activePreviewDebugOverlayTab);
}

void SectorEditor::DrawPreviewOverlay(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    const SectorEditorConstDerivationDocumentAccess derivation =
            MakeLiveConstDerivationAccess(documentState.derivation);
    SectorEditorPreviewOverlayContext overlayContext{
            ui,
            config,
            input,
            assets,
            font,
            smallFont,
            TopologyMap(),
            AuthoringGraph(),
            derivation.authoringDerivation,
            IsSectorEditorAuthoringDerivationCurrent(derivation),
            Lifecycle().topologyDocumentDirty,
            runtimeObjectEditingState.drag,
            previewState,
            sceneRuntime.RuntimeObjects(),
            sceneRuntime.Navigation(),
            sceneRuntime.NpcNavigation(),
            fpsPlayer.State(),
            selectionState,
            manipulationState,
            BuildSelectionUiDependencies(
                    uiState,
                    runtimeObjectEditingUiState,
                    inspectorIdUiState),
            uiState.objectProbeDebugDrawMaxDistanceInput,
            materialEditingUiState,
            lightEditingState,
            statusText,
            sceneRuntime.Renderer()};
    const SectorEditorPreviewOverlayResult result = DrawSectorEditorPreviewOverlay(overlayContext);

    if (result.requestCancelLightPilot) {
        CancelLightPilotWithPreviewRestore("Light pilot cancelled");
    }
    if (result.requestApplyLightPilot) {
        ApplyLightPilotFromPreviewPose();
    }
    if (result.requestStartLightPilot) {
        StartLightPilot();
    }
    if (result.requestCancelProxyPlacement) {
        CancelLightProxyPlacement(
                lightEditingState.proxyPlacement.proxyKind == LightProxyPlacementKind::Shaft
                        ? "Shaft placement cancelled"
                        : "Halo placement cancelled");
    }
    if (result.requestApplyProxyPlacement) {
        ApplyLightProxyPlacement();
    }
    if (result.requestStartProxyPlacement != LightProxyPlacementKind::None) {
        StartLightProxyPlacement(result.requestStartProxyPlacement);
    }
    if (result.previewProxyOffsetChanged) {
        PreviewLightProxyPlacementOffset(result.previewProxyOffsetWorld);
    }
    if (result.openPreviewSettings) {
        OpenPreviewSettingsModal();
    }
    if (result.requestNavigationRebuild) {
        statusText = engineContext != nullptr
                && sceneRuntime.RebuildNavigationForMap(
                        *engineContext, TopologyMap())
                ? "Navigation rebuild queued"
                : "Navigation rebuild failed to initialize";
    }
    if (result.markTopologyDocumentEdited) {
        MarkTopologyDocumentEdited(result.topologyDocumentEditStatus);
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
    if (previewState.controller.freeflyController.mouseLookEnabled) {
        return;
    }

    if (!IsValidSurfaceRef(previewState.selection.selectedSurface3D)
            || !IsValidTopologySurfaceEditTarget(previewState.selection.selectedTopologySurface3D)) {
        previewState.selection.selectedSurface3D = SectorSurfaceRef{};
        previewState.selection.selectedTopologySurface3D = TopologySurfaceEditTarget{};
        return;
    }
    if (!EnsureSelectedSurface3DAuthoringMappingCurrent()) {
        return;
    }

    SectorEditorMaterialEditingService materialEditing = BuildMaterialEditingService();
    SectorEditorTextureCatalogService textureCatalog = MakeTextureCatalogService();
    const SectorEditorConstDerivationDocumentAccess derivation =
            MakeLiveConstDerivationAccess(documentState.derivation);
    SectorEditorPreviewUvPanelContext panelContext{
            ui,
            config,
            input,
            assets,
            font,
            engine::FontHandle{},
            BuildPreviewUvPanelRect(),
            TopologyMap(),
            AuthoringGraph(),
            derivation.authoringDerivation,
            IsSectorEditorAuthoringDerivationCurrent(derivation),
            state.texturePicker,
            previewState.selection,
            selectionState,
            materialEditingUiState,
            statusText,
            materialEditing,
            textureCatalog,
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
    const SectorRuntimeObjectState& runtimeObjects = sceneRuntime.RuntimeObjects();
    if (!IsSectorEditorTopologyRenderCacheCurrent(
                state.topologyRenderCache,
                state.topologyRenderRevision,
                runtimeObjects.swingDoorCatalogRevision)) {
        const SectorEditorConstDerivationDocumentAccess derivation =
                MakeLiveConstDerivationAccess(documentState.derivation);
        state.topologyRenderCache = BuildSectorEditorTopologyRenderCache(
                TopologyMap(),
                AuthoringGraph(),
                derivation.authoringDerivation,
                state.topologyRenderRevision,
                runtimeObjects.swingDoorCatalogLoaded
                        ? &runtimeObjects.swingDoorCatalog
                        : nullptr,
                runtimeObjects.swingDoorCatalogRevision);
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
            ShouldDrawLegacyTopologySelectionHighlight(hasAuthoringGraph, selectionState.topologySelectionKind);
    const SectorEditorConstDerivationDocumentAccess derivation =
            MakeLiveConstDerivationAccess(documentState.derivation);
    const SectorEditorTopologyDrawContext drawContext{
            canvasRect,
            state.viewCenter,
            state.viewZoom,
            state.showSectorIds,
            derivation.authoringDerivedTopologyStale,
            state.currentTool,
            drawLegacyTopologySelection ? selectionState.topologySelectionKind : TopologySelectionKind::None,
            drawLegacyTopologySelection ? selectionState.selectedTopologySectorId : -1,
            drawLegacyTopologySelection ? selectionState.selectedTopologyVertexId : -1,
            drawLegacyTopologySelection ? selectionState.selectedTopologyLightId : -1,
            drawLegacyTopologySelection ? selectionState.selectedTopologyStaticSpotLightId : -1,
            drawLegacyTopologySelection ? selectionState.selectedTopologyDynamicLightId : -1,
            drawLegacyTopologySelection ? selectionState.selectedTopologyDynamicSpotLightId : -1,
            selectionState.selectedRuntimeObjectId,
            selectionState.hasHoveredVertex,
            selectionState.hoveredTopologyVertexId,
            selectionState.hoveredTopologyLightId,
            selectionState.hoveredTopologyStaticSpotLightId,
            selectionState.hoveredTopologyDynamicLightId,
            selectionState.hoveredTopologyDynamicSpotLightId,
            selectionState.selectedAuthoring,
            selectionState.hoveredAuthoring,
            &selectionState.selectedAuthoringFaceAnchorIds,
            authoringFaceMergeState.hoveredTargetFaceAnchorId
    };
    DrawCachedTopologySectors(state.topologyRenderCache, drawContext);
    DrawCachedTriggers(state.topologyRenderCache, drawContext,
            triggerEditingState.drag.active ? &triggerEditingState.drag : nullptr);
    DrawAuthoringFogVolumes();

    if (drawLegacyTopologySelection) {
        DrawTopologySelectedLineHighlight();
    }
    DrawCachedTopologyLineDefs(state.topologyRenderCache, drawContext);
    DrawCachedTopologyVertices(state.topologyRenderCache, drawContext);
    DrawCachedAuthoringGraphOverlay(state.topologyRenderCache, drawContext);
    DrawCachedAuthoringDiagnostics(state.topologyRenderCache, drawContext);
    DrawAuthoringVertexMoveOverlay();
    DrawAuthoringFogVolumeMoveOverlay();
    DrawCachedTopologyStaticLights(state.topologyRenderCache, drawContext);
    DrawCachedTopologyStaticSpotLights(state.topologyRenderCache, drawContext);
    DrawCachedTopologyDynamicLights(state.topologyRenderCache, drawContext);
    DrawCachedTopologyDynamicSpotLights(state.topologyRenderCache, drawContext);
    DrawCachedRuntimeObjects(state.topologyRenderCache, drawContext);
    DrawCachedLevelMarkers(
            state.topologyRenderCache,
            drawContext,
            levelMarkerEditingService ? &levelMarkerEditingService->Drag() : nullptr);
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
    drawToolOverlay(SectorEditorTool::AuthoringFogVolume);
    drawToolOverlay(SectorEditorTool::Trigger);
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
    if (derivation.authoringDerivedTopologyStale) {
        const std::string staleStatus =
                BuildSectorEditorAuthoringDerivationDisplayStatus(
                        derivation,
                        "Authoring graph changed; derived sector fills are stale");
        DrawText(
                staleStatus.c_str(),
                static_cast<int>(canvasRect.x + 16.0f),
                static_cast<int>(canvasRect.y + 64.0f),
                18,
                Color{236, 196, 92, 255}
        );
    }
}

void SectorEditor::DrawAuthoringFogVolumes() const
{
    const auto drawVolume = [this](
            const SectorAuthoringFogVolume& volume,
            SectorTopologyCoordPoint point,
            Color outline,
            bool resolved,
            bool drawLabel) {
        const Vector2 mapCenter{
                SectorCoordToVisibleAuthoring(point.x),
                SectorCoordToVisibleAuthoring(point.y)};
        const Vector2 center = MapToScreen(mapCenter);
        const Vector2 edgeX = MapToScreen(Vector2{
                mapCenter.x + SectorWorldToAuthoringDistance(volume.radiusXWorld),
                mapCenter.y});
        const Vector2 edgeZ = MapToScreen(Vector2{
                mapCenter.x,
                mapCenter.y + SectorWorldToAuthoringDistance(volume.radiusZWorld)});
        const float radiusX = std::max(2.0f, std::fabs(edgeX.x - center.x));
        const float radiusY = std::max(2.0f, std::fabs(edgeZ.y - center.y));
        Color fill = volume.enabled ? volume.color : Color{112, 118, 122, 255};
        fill.a = 46;
        const bool drawBox = volume.shape == SectorLocalFogShape::Box;
        float innerScaleX = std::clamp(1.0f - volume.edgeSoftness, 0.05f, 1.0f);
        float innerScaleZ = innerScaleX;
        const bool roomStyle = volume.analyticStyle == SectorAnalyticFogStyle::Room;
        const float minimumFraction = roomStyle ? 0.005f : 0.01f;
        const float maximumFraction = roomStyle ? 0.20f : 0.45f;
        const float minimumHalfExtent = std::min(
                {volume.radiusXWorld, volume.heightWorld * 0.5f, volume.radiusZWorld});
        const float normalizedSoftness = std::clamp(
                volume.edgeSoftness, 0.0f, 1.0f);
        const float edgeWidth = minimumHalfExtent
                * (minimumFraction
                        + (maximumFraction - minimumFraction) * normalizedSoftness);
        innerScaleX = std::clamp(
                1.0f - edgeWidth / std::max(volume.radiusXWorld, 0.0001f),
                0.05f,
                1.0f);
        innerScaleZ = std::clamp(
                1.0f - edgeWidth / std::max(volume.radiusZWorld, 0.0001f),
                0.05f,
                1.0f);
        Color inner = outline;
        inner.a = 130;
        if (drawBox) {
            const float cosine = std::cos(volume.yawDegrees * DEG2RAD);
            const float sine = std::sin(volume.yawDegrees * DEG2RAD);
            const auto boxCorners = [&](float scaleX, float scaleZ) {
                std::array<Vector2, 4> corners{};
                constexpr std::array<Vector2, 4> signs = {
                        Vector2{-1.0f, -1.0f},
                        Vector2{1.0f, -1.0f},
                        Vector2{1.0f, 1.0f},
                        Vector2{-1.0f, 1.0f}};
                for (std::size_t index = 0; index < signs.size(); ++index) {
                    const float localX = signs[index].x * volume.radiusXWorld * scaleX;
                    const float localZ = signs[index].y * volume.radiusZWorld * scaleZ;
                    const Vector2 mapCorner{
                            mapCenter.x + SectorWorldToAuthoringDistance(
                                    cosine * localX + sine * localZ),
                            mapCenter.y + SectorWorldToAuthoringDistance(
                                    -sine * localX + cosine * localZ)};
                    corners[index] = MapToScreen(mapCorner);
                }
                return corners;
            };
            const auto outerCorners = boxCorners(1.0f, 1.0f);
            DrawTriangle(outerCorners[0], outerCorners[1], outerCorners[2], fill);
            DrawTriangle(outerCorners[0], outerCorners[2], outerCorners[3], fill);
            for (std::size_t index = 0; index < outerCorners.size(); ++index) {
                DrawLineV(
                        outerCorners[index],
                        outerCorners[(index + 1) % outerCorners.size()],
                        outline);
            }
            const auto innerCorners = boxCorners(innerScaleX, innerScaleZ);
            for (std::size_t index = 0; index < innerCorners.size(); ++index) {
                DrawLineV(
                        innerCorners[index],
                        innerCorners[(index + 1) % innerCorners.size()],
                        inner);
            }
        } else {
            DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y), radiusX, radiusY, fill);
            DrawEllipseLines(static_cast<int>(center.x), static_cast<int>(center.y), radiusX, radiusY, outline);
            DrawEllipseLines(
                    static_cast<int>(center.x),
                    static_cast<int>(center.y),
                    radiusX * innerScaleX,
                    radiusY * innerScaleZ,
                    inner);
        }
        if (!resolved) {
            DrawLineEx(
                    Vector2{center.x - 8.0f, center.y - 8.0f},
                    Vector2{center.x + 8.0f, center.y + 8.0f},
                    2.0f,
                    Color{240, 82, 82, 255});
            DrawLineEx(
                    Vector2{center.x + 8.0f, center.y - 8.0f},
                    Vector2{center.x - 8.0f, center.y + 8.0f},
                    2.0f,
                    Color{240, 82, 82, 255});
        }
        if (drawLabel) {
            DrawText("FG", static_cast<int>(center.x + 7.0f), static_cast<int>(center.y - 18.0f), 14, outline);
        }
    };

    for (const SectorAuthoringFogVolume& volume : AuthoringGraph().fogVolumes) {
        bool resolved = false;
        for (const SectorAuthoringDerivedFogVolumeMapping& mapping
                : documentState.derivation.authoringDerivation.mapping.fogVolumes) {
            if (mapping.authoringFogVolumeId == volume.id) {
                resolved = mapping.resolved;
                break;
            }
        }
        const bool selected = selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::FogVolume
                && selectionState.selectedAuthoring.fogVolumeId == volume.id;
        const bool hovered = selectionState.hoveredAuthoring.kind == SectorAuthoringSelectionKind::FogVolume
                && selectionState.hoveredAuthoring.fogVolumeId == volume.id;
        Color outline = !resolved
                ? Color{240, 82, 82, 255}
                : !volume.enabled
                        ? Color{145, 150, 155, 235}
                        : selected
                                ? Color{72, 220, 245, 255}
                                : hovered
                                        ? Color{244, 192, 70, 255}
                                        : Color{116, 205, 164, 230};
        drawVolume(
                volume,
                SectorTopologyCoordPoint{volume.x, volume.y},
                outline,
                resolved,
                true);
    }
}

void SectorEditor::DrawAuthoringFogVolumeMoveOverlay() const
{
    const AuthoringFogVolumeDragState& drag = manipulationState.authoringFogVolumeDrag;
    if (!drag.active || !drag.hasPreviewPoint) {
        return;
    }
    const SectorAuthoringFogVolume* volume = FindSectorAuthoringFogVolume(AuthoringGraph(), drag.fogVolumeId);
    if (volume == nullptr) {
        return;
    }
    const Vector2 originalMap{
            SectorCoordToVisibleAuthoring(drag.originalPoint.x),
            SectorCoordToVisibleAuthoring(drag.originalPoint.y)};
    const Vector2 previewMap{
            SectorCoordToVisibleAuthoring(drag.previewPoint.x),
            SectorCoordToVisibleAuthoring(drag.previewPoint.y)};
    const Vector2 original = MapToScreen(originalMap);
    const Vector2 previewPoint = MapToScreen(previewMap);
    const Vector2 radiusXPoint = MapToScreen(Vector2{
            previewMap.x + SectorWorldToAuthoringDistance(volume->radiusXWorld),
            previewMap.y});
    const Vector2 radiusZPoint = MapToScreen(Vector2{
            previewMap.x,
            previewMap.y + SectorWorldToAuthoringDistance(volume->radiusZWorld)});
    const float radiusX = std::max(2.0f, std::fabs(radiusXPoint.x - previewPoint.x));
    const float radiusY = std::max(2.0f, std::fabs(radiusZPoint.y - previewPoint.y));
    const Color previewColor = drag.previewResolved
            ? Color{72, 220, 245, 255}
            : Color{240, 82, 82, 255};
    DrawLineEx(original, previewPoint, 2.0f, Color{150, 200, 220, 180});
    DrawEllipseLines(
            static_cast<int>(original.x),
            static_cast<int>(original.y),
            radiusX,
            radiusY,
            Color{150, 200, 220, 120});
    DrawEllipseLines(
            static_cast<int>(previewPoint.x),
            static_cast<int>(previewPoint.y),
            radiusX,
            radiusY,
            previewColor);
}

void SectorEditor::DrawTopologySelectedLineHighlight() const
{
    if (selectionState.topologySelectionKind != TopologySelectionKind::SideDef
            && selectionState.topologySelectionKind != TopologySelectionKind::LineDef) {
        return;
    }

    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        return;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(TopologyMap(), *lineDef, start, end)) {
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
    if (selectionState.topologySelectionKind == TopologySelectionKind::LineDef) {
        normal = Vector2{0.0f, 0.0f};
        color = Color{210, 214, 224, 125};
    } else if (selectionState.selectedTopologySideKind == SectorTopologySideKind::Back) {
        normal.x = -normal.x;
        normal.y = -normal.y;
        color = Color{94, 238, 186, 132};
    }

    const float offset = selectionState.topologySelectionKind == TopologySelectionKind::LineDef ? 0.0f : 7.0f;
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
            && !manipulationState.authoringVertexDrag.active) {
        return;
    }

    if (!manipulationState.authoringVertexDrag.active
            && selectionState.hoveredAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
        const SectorAuthoringGraph& authoringGraph = AuthoringGraph();
        const SectorAuthoringVertex* vertex =
                FindSectorAuthoringVertex(authoringGraph, selectionState.hoveredAuthoring.vertexId);
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

    if (!manipulationState.authoringVertexDrag.active) {
        return;
    }

    const bool invalid = !manipulationState.authoringVertexDrag.errorMessage.empty()
            || !manipulationState.authoringVertexDrag.hasPreviewPoint;
    const Color targetColor = invalid ? Color{230, 82, 82, 255} : Color{120, 230, 154, 255};
    const Color previewColor = invalid ? Color{230, 82, 82, 205} : Color{122, 220, 244, 220};
    const Color originalColor = Color{245, 226, 154, 230};
    const Vector2 original = MapToScreen(Vector2{
            SectorCoordToVisibleAuthoring(manipulationState.authoringVertexDrag.originalPoint.x),
            SectorCoordToVisibleAuthoring(manipulationState.authoringVertexDrag.originalPoint.y)});

    if (!manipulationState.authoringVertexDrag.hasPreviewPoint) {
        DrawCircleLines(
                static_cast<int>(std::round(original.x)),
                static_cast<int>(std::round(original.y)),
                10.0f,
                originalColor);
        return;
    }

    const int draggedVertexId = manipulationState.authoringVertexDrag.vertexId;
    const Vector2 previewMap{
            SectorCoordToVisibleAuthoring(manipulationState.authoringVertexDrag.previewPoint.x),
            SectorCoordToVisibleAuthoring(manipulationState.authoringVertexDrag.previewPoint.y)};
    const SectorAuthoringGraph& authoringGraph = AuthoringGraph();
    for (const SectorAuthoringLine& line : authoringGraph.lines) {
        if (line.startVertexId != draggedVertexId && line.endVertexId != draggedVertexId) {
            continue;
        }

        const int otherVertexId = line.startVertexId == draggedVertexId
                ? line.endVertexId
                : line.startVertexId;
        const SectorAuthoringVertex* otherVertex =
                FindSectorAuthoringVertex(authoringGraph, otherVertexId);
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

    if (lightEditingState.lightDrag.active) {
        if (selectionState.topologySelectionKind == TopologySelectionKind::StaticSpotLight) {
            const SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(
                    TopologyMap(),
                    lightEditingState.lightDrag.topologyLightId);
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
                    lightEditingState.lightDrag.spotHandle == SpotLightHandle::Origin ? 15.0f : 11.0f,
                    Color{120, 230, 154, 255});
            DrawCircleV(origin, 6.5f, Color{120, 230, 154, 255});
            DrawCircleLines(
                    static_cast<int>(std::round(target.x)),
                    static_cast<int>(std::round(target.y)),
                    lightEditingState.lightDrag.spotHandle == SpotLightHandle::Target ? 15.0f : 10.0f,
                    Color{122, 220, 244, 255});
            DrawCircleV(target, 5.0f, Color{122, 220, 244, 255});
            return;
        }

        if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicSpotLight) {
            const SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(
                    TopologyMap(),
                    lightEditingState.lightDrag.topologyLightId);
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
                    lightEditingState.lightDrag.spotHandle == SpotLightHandle::Origin ? 15.0f : 11.0f,
                    Color{120, 230, 154, 255});
            DrawCircleV(origin, 6.5f, Color{120, 230, 154, 255});
            DrawCircleLines(
                    static_cast<int>(std::round(target.x)),
                    static_cast<int>(std::round(target.y)),
                    lightEditingState.lightDrag.spotHandle == SpotLightHandle::Target ? 15.0f : 10.0f,
                    Color{122, 220, 244, 255});
            DrawCircleV(target, 5.0f, Color{122, 220, 244, 255});
            return;
        }

        if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicLight) {
            const SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                    TopologyMap(),
                    lightEditingState.lightDrag.topologyLightId);
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
                TopologyMap(),
                lightEditingState.lightDrag.topologyLightId);
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

    if (selectionState.hoveredTopologyStaticSpotLightId >= 0) {
        const SectorTopologyStaticSpotLight* light = FindSectorTopologyStaticSpotLight(
                TopologyMap(),
                selectionState.hoveredTopologyStaticSpotLightId);
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

    if (selectionState.hoveredTopologyDynamicSpotLightId >= 0) {
        const SectorTopologyDynamicSpotLight* light = FindSectorTopologyDynamicSpotLight(
                TopologyMap(),
                selectionState.hoveredTopologyDynamicSpotLightId);
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

    if (selectionState.hoveredTopologyDynamicLightId >= 0) {
        const SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                TopologyMap(),
                selectionState.hoveredTopologyDynamicLightId);
        if (light == nullptr) {
            return;
        }

        const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
        DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 13.0f, Color{245, 226, 154, 255});
        return;
    }

    const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            TopologyMap(),
            selectionState.hoveredTopologyLightId);
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
            sectionLabelH + rowsHeight(5)
            + separatorH + sectionLabelH + rowsHeight(13)
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
        manipulationState.selectDragArm = SelectDragArmState{};
        if (authoringFaceMergeService
                && authoringFaceMergeService->IsChoosingTarget()) {
            authoringFaceMergeService->CancelTargetPick(
                    "Merge Selected Into cancelled");
        }
        if (state.pendingAuthoringLine.active && tool != SectorEditorTool::AuthoringLine) {
            CancelPendingAuthoringLine("Cancelled authoring line");
        }
        if (state.pendingAuthoringRectangle.active && tool != SectorEditorTool::AuthoringRectangle) {
            CancelPendingAuthoringRectangle("Rectangle cancelled");
        }
        if (state.pendingAuthoringInsertVertex.active && tool != SectorEditorTool::AuthoringInsertVertex) {
            CancelPendingAuthoringInsertVertex("Insert Vertex cancelled");
        }
        if (triggerEditingState.pending.active && tool != SectorEditorTool::Trigger) {
            triggerEditingState.pending = PendingTriggerDrawState{};
            statusText = "Trigger drawing cancelled";
        }
        if (manipulationState.authoringVertexDrag.active && tool != SectorEditorTool::AuthoringMove) {
            CancelAuthoringVertexDrag("Cancelled authoring vertex move");
        }
        if (manipulationState.authoringFogVolumeDrag.active
                && tool != SectorEditorTool::Select
                && fogVolumeEditingService) {
            fogVolumeEditingService->CancelMove("Cancelled fog volume move");
        }
        if (levelMarkerEditingService
                && levelMarkerEditingService->Drag().active
                && tool != SectorEditorTool::Select) {
            levelMarkerEditingService->CancelMove("Cancelled Level Marker move");
        }
        if (triggerEditingService
                && triggerEditingService->IsMoving()
                && tool != SectorEditorTool::Select) {
            triggerEditingService->CancelMove("Cancelled trigger move");
        }
        if (lightEditingState.lightDrag.active
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
            const SectorAuthoringGraph& authoringGraph = AuthoringGraph();
            state.pendingAuthoringInsertVertex = PendingAuthoringInsertVertex{};
            if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line
                    && FindSectorAuthoringLine(authoringGraph, selectionState.selectedAuthoring.lineId) != nullptr) {
                state.pendingAuthoringInsertVertex.active = true;
                state.pendingAuthoringInsertVertex.lineId = selectionState.selectedAuthoring.lineId;
                statusText = "Insert Vertex: click point on selected line, Esc/right click cancels";
            } else {
                statusText = "Insert Vertex: select or click an authoring line";
            }
        } else if (tool == SectorEditorTool::RuntimeObject) {
            statusText = "Billboard: click inside a sector to place a billboard";
        } else if (tool == SectorEditorTool::StaticModel) {
            statusText = "3D Prop: click inside a derived sector to place a static model";
        } else if (tool == SectorEditorTool::DynamicModel) {
            statusText = "Dynamic Prop: click inside a derived sector to place an animated model";
        } else if (tool == SectorEditorTool::Npc) {
            SectorRuntimeObjectState& runtimeObjects = sceneRuntime.RuntimeObjects();
            RefreshSectorEditorNpcPlacementOptions(
                    runtimeObjectEditingState.npcPlacement,
                    runtimeObjects.npcDefinitionCatalog,
                    runtimeObjects.npcDefinitionCatalogRevision);
            statusText = runtimeObjectEditingState.npcPlacement.definitionIds.empty()
                    ? "NPC: create an NPC definition in the NPC Editor first"
                    : "NPC: click inside a derived sector to place a character";
        } else if (tool == SectorEditorTool::Door) {
            statusText = "Door: click a two-sided portal line";
        } else if (tool == SectorEditorTool::AuthoringFogVolume) {
            statusText = "Fog Volume: click strictly inside a sector";
        } else if (tool == SectorEditorTool::LevelMarker) {
            statusText = "Level Marker: click strictly inside a sector";
        } else if (tool == SectorEditorTool::Trigger) {
            statusText = triggerEditingState.drawMode == TriggerDrawMode::Rectangle
                    ? "Trigger Rectangle: click first corner"
                    : "Trigger Polygon: click first point";
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
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_set_all",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Set All")) {
        OpenSectorEditorSetAllModal(
                state.setAllModal,
                AuthoringGraph(),
                selectionState.selectedAuthoring);
    }
    y += rowH + gap;

    separator();
    sectionLabel("Map objects");
    const SectorEditorTool mapTools[] = {
            SectorEditorTool::RuntimeObject,
            SectorEditorTool::StaticModel,
            SectorEditorTool::DynamicModel,
            SectorEditorTool::Npc,
            SectorEditorTool::Door,
            SectorEditorTool::Trigger,
            SectorEditorTool::LevelMarker,
            SectorEditorTool::AuthoringFogVolume,
            SectorEditorTool::StaticLight,
            SectorEditorTool::StaticSpotLight,
            SectorEditorTool::DynamicLight,
            SectorEditorTool::DynamicSpotLight
    };
    for (SectorEditorTool tool : mapTools) {
        if (drawToolButton(tool)) {
            selectTool(tool);
        }
        if (tool == SectorEditorTool::Trigger && state.currentTool == SectorEditorTool::Trigger) {
            const float half = (contentW - gap) * 0.5f;
            const bool rectangle = engine::ToolButton(
                    ui, config, input, assets, "sector_editor_trigger_rectangle",
                    Rectangle{0.0f, y, half, rowH}, font, "Rectangle",
                    triggerEditingState.drawMode == TriggerDrawMode::Rectangle);
            const bool polygon = engine::ToolButton(
                    ui, config, input, assets, "sector_editor_trigger_polygon",
                    Rectangle{half + gap, y, half, rowH}, font, "Polygon",
                    triggerEditingState.drawMode == TriggerDrawMode::Polygon);
            y += rowH + gap;
            if (rectangle || polygon) {
                triggerEditingState.pending = PendingTriggerDrawState{};
                triggerEditingState.drawMode = rectangle
                        ? TriggerDrawMode::Rectangle : TriggerDrawMode::Polygon;
                statusText = rectangle ? "Trigger Rectangle: click first corner"
                                       : "Trigger Polygon: click first point";
            }
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
    if (engine::Button(ui, config, input, assets, "sector_editor_add_map_sound", Rectangle{0.0f, y, contentW, rowH}, font, "Add Map Sound")) {
        BuildSoundService().OpenAddModal();
    }
    y += rowH + gap;
    if (engine::Button(ui, config, input, assets, "sector_editor_preview_settings_2d", Rectangle{0.0f, y, contentW, rowH}, font, "Settings")) {
        OpenPreviewSettingsModal();
    }
    y += rowH + gap;
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_editor",
                Rectangle{0.0f, y, contentW, rowH},
                font, "NPC Editor")) {
        BuildNpcEditorService().Open();
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
            Lifecycle().hasUnsavedChanges = true;
            Lifecycle().topologyDocumentDirty = true;
            statusText = status;
        }
        y += rowH + gap;
    };

    TopologyMap().lightmapSettings.ambientOcclusionRadius = ClampAmbientOcclusionRadius(TopologyMap().lightmapSettings.ambientOcclusionRadius);
    TopologyMap().lightmapSettings.ambientOcclusionStrength = ClampAmbientOcclusionStrength(TopologyMap().lightmapSettings.ambientOcclusionStrength);
    TopologyMap().lightmapSettings.indirectBounceRadius = ClampIndirectBounceRadius(TopologyMap().lightmapSettings.indirectBounceRadius);
    TopologyMap().lightmapSettings.indirectBounceStrength = ClampIndirectBounceStrength(TopologyMap().lightmapSettings.indirectBounceStrength);
    drawLightmapSetting(
            "sector_editor_ao_radius",
            "AO radius",
            TopologyMap().lightmapSettings.ambientOcclusionRadius,
            uiState.ambientOcclusionRadiusInput,
            SectorWorldToAuthoringDistance(0.05f),
            SectorWorldToAuthoringDistance(16.0f),
            2,
            "Updated AO radius"
    );
    drawLightmapSetting(
            "sector_editor_ao_strength",
            "AO strength",
            TopologyMap().lightmapSettings.ambientOcclusionStrength,
            uiState.ambientOcclusionStrengthInput,
            0.0f,
            1.0f,
            3,
            "Updated AO strength"
    );
    drawLightmapSetting(
            "sector_editor_bounce_radius",
            "Bounce radius",
            TopologyMap().lightmapSettings.indirectBounceRadius,
            uiState.indirectBounceRadiusInput,
            SectorWorldToAuthoringDistance(0.05f),
            SectorWorldToAuthoringDistance(16.0f),
            2,
            "Updated bounce radius"
    );
    drawLightmapSetting(
            "sector_editor_bounce_strength",
            "Bounce strength",
            TopologyMap().lightmapSettings.indirectBounceStrength,
            uiState.indirectBounceStrengthInput,
            0.0f,
            1.0f,
            3,
            "Updated bounce strength"
    );

    if (engine::Button(ui, config, input, assets, "sector_editor_bake_lightmaps", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        StartLightmapBake();
    }
    y += rowH + gap;

    separator();

    const float gridLabelW = 64.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, gridLabelW, rowH}, font, "Grid", engine::UITextJustify::Left, config.mutedTextColor);
    const engine::UINumericInputResult gridInputResult = engine::IntInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_grid",
            Rectangle{gridLabelW + gap, y, std::max(0.0f, contentW - gridLabelW - gap), rowH},
            font,
            state.gridSize,
            uiState.gridSizeInput,
            SectorAuthoringEditorGridSizeMin,
            SectorAuthoringEditorGridSizeMax,
            1
    );
    if (gridInputResult.changed) {
        Lifecycle().hasUnsavedChanges = true;
        Lifecycle().topologyDocumentDirty = true;
        statusText = "Updated grid size";
    }
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
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService runtimeObjectEditing =
            BuildRuntimeObjectEditingService(&selection);
    SectorEditorStaticModelPickerService staticModelPicker{
            runtimeObjectEditingState.staticModelPicker,
            statusText};
    SectorEditorMaterialEditingService materialEditing = BuildMaterialEditingService();
    SectorEditorFootstepService footsteps = BuildFootstepService();
    SectorEditorTextureCatalogService textureCatalog = MakeTextureCatalogService();
    SectorEditorSoundService sounds = BuildSoundService(&runtimeObjectEditing);
    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    SectorEditorInspectorPanelContext context{
            ui,
            config,
            input,
            assets,
            font,
            smallFont,
            BuildRightPanelRect(),
            state,
            Lifecycle(),
            TopologyMap(),
            AuthoringGraph(),
            MakeLiveDerivationAccess(documentState.derivation),
            selectionState,
            uiState,
            runtimeObjectEditingState,
            runtimeObjectEditingUiState,
            sceneRuntime.RuntimeObjects(),
            inspectorIdUiState,
            materialEditingUiState,
            fogVolumeEditingUiState,
            levelMarkerEditingUiState,
            triggerEditingUiState,
            statusText,
            selection,
            runtimeObjectEditing,
            staticModelPicker,
            materialEditing,
            footsteps,
            textureCatalog,
            sounds,
            lightEditing,
            fogVolumeEditingService.value(),
            levelMarkerEditingService.value(),
            triggerEditingService.value(),
            authoringFaceMergeService.value(),
            engineContext};
    const SectorEditorInspectorPanelResult result = DrawSectorEditorInspectorPanel(context);
    for (int i = 0; i < result.requestCount; ++i) {
        const SectorEditorInspectorPanelRequest& request = result.requests[static_cast<size_t>(i)];
        switch (request.kind) {
        case SectorEditorInspectorPanelRequestKind::RebuildSectorCollisionWorld:
            RebuildSectorCollisionWorld();
            break;
        case SectorEditorInspectorPanelRequestKind::BeginAuthoringInsertVertex:
            BeginPendingAuthoringInsertVertex(request.lineId);
            break;
        case SectorEditorInspectorPanelRequestKind::DeleteSelectedAuthoringVertex:
            DeleteSelectedAuthoringVertex();
            break;
        case SectorEditorInspectorPanelRequestKind::DeleteSelectedRuntimeObject:
            DeleteSelectedRuntimeObject();
            break;
        case SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedLightConfirmation:
            OpenDeleteSelectedLightConfirmation();
            break;
        case SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedFogVolumeConfirmation:
            OpenConfirmation(
                    "Delete Fog Volume",
                    "Delete the selected authoring fog volume?",
                    [this]() {
                        if (fogVolumeEditingService) {
                            fogVolumeEditingService->DeleteSelected();
                        }
                    });
            break;
        case SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedLevelMarkerConfirmation:
            OpenConfirmation(
                    "Delete Level Marker",
                    "Delete the selected Level Marker?",
                    [this]() {
                        if (levelMarkerEditingService) {
                            levelMarkerEditingService->DeleteSelected();
                        }
                    });
            break;
        case SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedTriggerConfirmation:
            OpenConfirmation(
                    "Delete Trigger",
                    "Delete the selected trigger?",
                    [this]() {
                        if (triggerEditingService) {
                            triggerEditingService->DeleteSelected();
                        }
                    });
            break;
        case SectorEditorInspectorPanelRequestKind::BakeLightmaps:
            StartLightmapBake();
            break;
        case SectorEditorInspectorPanelRequestKind::RefreshPreviewLightSources:
            if (state.mode == SectorEditorMode::Preview3D && sceneRuntime.Renderer().IsRendererReady()) {
                sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
            }
            break;
        }
    }
}

void SectorEditor::DrawAddMapTextureModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    SectorEditorTextureCatalogService textureCatalog = MakeTextureCatalogService();
    const SectorEditorAddTextureModalCallbacks callbacks{
            [this, &assets]() { CloseAddMapTextureModal(assets); },
            [this, &assets]() { return AddSelectedMapTexture(assets); },
            [&textureCatalog, this](int pathIndex) {
                textureCatalog.SelectAddMapTexturePath(state.addMapTexture, pathIndex);
            },
            [this, &assets]() { game::RefreshAddMapTexturePreview(state.addMapTexture, assets); },
            [&textureCatalog, this](std::string& error) {
                return textureCatalog.ValidateAddMapTextureId(state.addMapTexture, error);
            }
    };
    game::DrawAddMapTextureModal(ui, config, input, assets, font, state.addMapTexture, callbacks);
}

void SectorEditor::DrawAddMapSoundModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::FontHandle font)
{
    BuildSoundService().DrawAddModal(ui, config, input, font);
}

void SectorEditor::DrawSoundPickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::FontHandle font)
{
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService runtimeObjectEditing =
            BuildRuntimeObjectEditingService(&selection);
    BuildSoundService(&runtimeObjectEditing).DrawPickerModal(ui, config, input, font);
}

void SectorEditor::DrawTexturePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    SectorEditorTextureCatalogService textureCatalog = MakeTextureCatalogService();
    const SectorEditorTexturePickerServiceCallbacks callbacks{
            [this, &assets]() { ApplyTexturePickerSelection(assets); },
            [this]() { return CurrentTextureForPickerTarget(); }
    };
    DrawSectorEditorTexturePickerModal(
            ui,
            config,
            input,
            assets,
            font,
            smallFont,
            state.texturePicker,
            textureCatalog,
            callbacks);
}

void SectorEditor::DrawSetAllModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorSetAllModalCallbacks callbacks{
            [this]() {
                state.setAllModal = SectorEditorSetAllModalState{};
            },
            [this](
                    SectorEditorSectorLightingScope scope,
                    float ambientIntensity,
                    Color ambientColor) {
                std::string status;
                SetSectorEditorSectorLighting(
                        state,
                        Lifecycle(),
                        TopologyMap(),
                        AuthoringGraph(),
                        MakeLiveDerivationAccess(documentState.derivation),
                        selectionState,
                        scope,
                        ambientIntensity,
                        ambientColor,
                        &status);
                if (!status.empty()) {
                    statusText = std::move(status);
                }
                state.setAllModal = SectorEditorSetAllModalState{};
            }};
    std::size_t selectedSectorCount = 0;
    for (const SectorAuthoringFaceAnchor& anchor : AuthoringGraph().faceAnchors) {
        if (!anchor.isVoid
                && IsSectorEditorAuthoringFaceSelected(selectionState, anchor.id)) {
            ++selectedSectorCount;
        }
    }
    DrawSectorEditorSetAllModal(
            ui,
            config,
            input,
            assets,
            font,
            state.setAllModal,
            selectedSectorCount,
            callbacks);
}

void SectorEditor::DrawFootstepPickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    if (engineContext == nullptr) return;
    BuildFootstepService().DrawModal(ui, config, input, assets, font);
}

void SectorEditor::DrawSpritePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorSpritePickerCallbacks callbacks{
            [this, &assets]() { CloseSpritePicker(runtimeObjectEditingState.spritePicker, assets); },
            [this, &assets]() {
                ApplySelectedBillboardSpritePickerSelection();
                CloseSpritePicker(runtimeObjectEditingState.spritePicker, assets);
            },
            [this]() {
                RefreshSpritePickerScan(runtimeObjectEditingState.spritePicker);
                runtimeObjectEditingState.spriteMetadataCatalog.scanned = true;
                runtimeObjectEditingState.spriteMetadataCatalog.scanMessage = runtimeObjectEditingState.spritePicker.scanMessage;
                runtimeObjectEditingState.spriteMetadataCatalog.sprites = runtimeObjectEditingState.spritePicker.sprites;
            },
            [this](int spriteIndex) { SelectSpritePickerSprite(runtimeObjectEditingState.spritePicker, spriteIndex); },
            [this, &assets]() { RefreshSpritePickerPreview(runtimeObjectEditingState.spritePicker, assets); }
    };
    game::DrawSpritePickerModal(ui, config, input, assets, font, runtimeObjectEditingState.spritePicker, callbacks);
}

void SectorEditor::DrawStaticModelPickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    SectorEditorStaticModelPickerService picker(
            runtimeObjectEditingState.staticModelPicker,
            statusText);
    StaticModelPickerState& pickerState = picker.State();
    if (!pickerState.open) {
        return;
    }
    if (!pickerState.scanned) {
        picker.Refresh();
    }

    bool cancelRequested = false;
    bool selectRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested, &selectRequested](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    cancelRequested = true;
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER
                        || event.key.key == KEY_KP_ENTER) {
                    selectRequested = true;
                    engine::ConsumeEvent(event);
                }
            });

    DrawRectangle(
            0,
            0,
            static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight),
            Color{0, 0, 0, 135});
    const Rectangle modal{
            (EditorWidth - 820.0f) * 0.5f,
            (EditorHeight - 650.0f) * 0.5f,
            820.0f,
            650.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(
            config,
            assets,
            Rectangle{modal.x + 22.0f, modal.y + 18.0f, modal.width - 44.0f, 36.0f},
            font,
            pickerState.target == ModelPickerTarget::DynamicModel
                    ? "Choose Dynamic Prop Model"
                    : "Choose 3D Prop Model");

    const Rectangle listBounds{
            modal.x + 22.0f,
            modal.y + 68.0f,
            modal.width - 44.0f,
            450.0f};
    const float listContentW =
            ScrollAreaContentWidthForVerticalScrollbar(
                    listBounds.width,
                    config,
                    0.0f,
                    true);
    const Vector2 contentSize{
            listContentW,
            std::max(
                    listBounds.height,
                    config.listItemHeight
                            * static_cast<float>(pickerState.optionLabels.size()))};
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_static_model_picker_scroll",
            listBounds,
            contentSize,
            pickerState.scroll);
    if (!pickerState.optionLabels.empty()) {
        const int previous = pickerState.selectedModelIndex;
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_static_model_picker_list",
                Rectangle{0.0f, 0.0f, scroll.viewport.width, contentSize.y},
                font,
                pickerState.optionLabels.data(),
                pickerState.optionLabels.size(),
                pickerState.selectedModelIndex);
        if (pickerState.selectedModelIndex != previous) {
            picker.SelectIndex(pickerState.selectedModelIndex);
        }
    }
    engine::EndScrollArea(ui, config, input, scroll, pickerState.scroll);

    engine::Text(
            config,
            assets,
            Rectangle{
                    listBounds.x,
                    listBounds.y + listBounds.height + 8.0f,
                    listBounds.width,
                    32.0f},
            font,
            pickerState.scanMessage.c_str(),
            engine::UITextJustify::Left,
            pickerState.modelPaths.empty()
                    ? config.invalidColor
                    : config.mutedTextColor);

    const float buttonY = modal.y + modal.height - 64.0f;
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_static_model_picker_refresh",
                Rectangle{modal.x + 22.0f, buttonY, 130.0f, 44.0f},
                font,
                "Refresh")) {
        picker.Refresh();
    }
    selectRequested = selectRequested || engine::Button(
            ui,
            config,
            input,
            assets,
            "sector_editor_static_model_picker_select",
            Rectangle{modal.x + modal.width - 322.0f, buttonY, 140.0f, 44.0f},
            font,
            "Select");
    cancelRequested = cancelRequested || engine::Button(
            ui,
            config,
            input,
            assets,
            "sector_editor_static_model_picker_cancel",
            Rectangle{modal.x + modal.width - 162.0f, buttonY, 140.0f, 44.0f},
            font,
            "Cancel");

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) {
                engine::ConsumeEvent(event);
            });
    if (cancelRequested) {
        picker.Close();
    } else if (selectRequested) {
        if (!picker.HasSelection()) {
            statusText = "Select a model first";
        } else {
            SectorEditorSelectionServiceContext selection =
                    BuildSelectionServiceContext();
            SectorEditorRuntimeObjectEditingService editing =
                    BuildRuntimeObjectEditingService(&selection);
            if (pickerState.target == ModelPickerTarget::DynamicModel) {
                editing.AssignSelectedDynamicModel(picker.SelectedModelPath());
            } else if (pickerState.target == ModelPickerTarget::StaticModel) {
                editing.AssignSelectedStaticModel(picker.SelectedModelPath());
            } else {
                statusText = "NPC model selection is only available in the NPC Editor";
            }
            pickerState.open = false;
        }
    }
}

void SectorEditor::DrawNpcEditorModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    if (engineContext == nullptr) {
        statusText = "NPC Editor audio picker requires an engine context";
        return;
    }
    SectorEditorNpcEditorService editor = BuildNpcEditorService();
    SectorEditorStaticModelPickerService modelPicker{
            runtimeObjectEditingState.staticModelPicker,
            statusText};
    SectorEditorAudioAssetPickerService audioPicker{
            *engineContext, audioAssetPickerSessionState};
    const SectorEditorNpcEditorModalResult result =
            game::DrawSectorEditorNpcEditorModal(
            ui,
            config,
            input,
            assets,
            font,
            smallFont,
            editor,
            modelPicker,
            audioPicker);
    if (result == SectorEditorNpcEditorModalResult::Saved
            && engineContext != nullptr) {
        sceneRuntime.RefreshMapRuntimeObjects(*engineContext, TopologyMap());
    }
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
        if (Lifecycle().topologyDocumentDirty) {
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
            TopologyMap(),
            selectionState.selectedRuntimeObjectId,
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
            [this]() {
                ResetSectorPreviewSettingsModalPreservingView(
                        state.previewSettingsModal);
            },
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
            [this]() {
                if (lightmapBake.RequestCancel()) {
                    statusText = "Cancelling bake...";
                }
            },
            [this]() { lightmapBake.AcknowledgeTerminalState(); }
    };
    game::DrawLightmapBakeModal(ui, config, input, assets, font, lightmapBake.BuildModalView(), callbacks);
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
    } else if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line) {
        selectedLabel = TextFormat("authoring line %d", selectionState.selectedAuthoring.lineId);
    } else if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
        selectedLabel = TextFormat("authoring vertex %d", selectionState.selectedAuthoring.vertexId);
    } else if (selectionState.selectedAuthoringFaceAnchorIds.size() > 1) {
        selectedLabel = TextFormat(
                "%d authoring faces",
                static_cast<int>(
                        selectionState.selectedAuthoringFaceAnchorIds.size()));
    } else if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor) {
        selectedLabel = TextFormat("authoring face anchor %d", selectionState.selectedAuthoring.faceAnchorId);
    }

    std::string pendingText;
    if (state.pendingAuthoringLine.active) {
        pendingText = " | authoring line";
    } else if (state.pendingAuthoringRectangle.active) {
        pendingText = " | rectangle";
    } else if (state.pendingAuthoringInsertVertex.active) {
        pendingText = " | insert vertex";
    } else if (manipulationState.authoringVertexDrag.active) {
        pendingText = " | authoring vertex move";
    } else if (authoringFaceMergeState.choosingTarget) {
        pendingText = " | merge target";
    }
    const std::string shortMapPath = Lifecycle().hasCurrentLevelPath
            ? Lifecycle().currentLevelPath
            : std::string{"<untitled>"};
    if (state.lightmapSourceHashRevision != state.topologyRenderRevision) {
        state.lightmapSourceHash = ComputeSectorLightmapSourceHash(TopologyMap());
        state.lightmapSourceHashRevision = state.topologyRenderRevision;
    }
    const char* lightmapText = SectorLightmapStatusText(GetSectorLightmapStatus(
            TopologyMap(),
            state.lightmapSourceHash));
    std::string status = statusText.empty() ? "Ready" : statusText;
    if (!state.topologyRenderWarning.empty()) {
        status += " | ";
        status += state.topologyRenderWarning;
    }
    const char* text = TextFormat(
            "%s%s | %s%s | map %s%s | grid %d | %s | selected %s",
            status.c_str(),
            Lifecycle().topologyDocumentDirty ? " *modified" : "",
            ToolHelpText(state.currentTool),
            pendingText.c_str(),
            shortMapPath.c_str(),
            Lifecycle().topologyDocumentDirty ? "*" : "",
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
    lightmapBake.Shutdown();
    sceneRuntime.Shutdown(context);
    BuildSoundService().Shutdown();
    if (!engine::IsNull(textureCatalogState.editorTextureScope)) {
        assets.UnloadScope(textureCatalogState.editorTextureScope);
    }
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }
    if (!engine::IsNull(runtimeObjectEditingState.spritePicker.previewScope)) {
        assets.UnloadScope(runtimeObjectEditingState.spritePicker.previewScope);
    }

    state = SectorEditorState{};
    manipulationState = ManipulationState{};
    uiState = SectorEditorUiState{};
    runtimeObjectEditingState = RuntimeObjectEditingState{};
    runtimeObjectEditingUiState = RuntimeObjectEditingUiState{};
    textureCatalogState = TextureCatalogState{};
    soundCatalogState = SectorEditorSoundCatalogState{};
    lightEditingState = LightEditingState{};
    materialEditingState = MaterialEditingState{};
    materialEditingUiState = MaterialEditingUiState{};
    fogVolumeEditingUiState = FogVolumeEditingUiState{};
    levelMarkerEditingState = LevelMarkerEditingState{};
    levelMarkerEditingUiState = LevelMarkerEditingUiState{};
    triggerEditingState = TriggerEditingState{};
    triggerEditingUiState = TriggerEditingUiState{};
    previewState.controller = SectorEditorPreviewControllerState{};
    ResetEditorTopologyDocumentState(
            Lifecycle(),
            TopologyMap(),
            AuthoringGraph(),
            MakeLiveDerivationAccess(documentState.derivation),
            previewState.controller);
    state.viewCenter = Vector2{9.0f, 6.0f};
    state.viewZoom = 48.0f;
    state.gridSize = SectorAuthoringEditorGridSizeDefault;
    SectorEditorTextureCatalogService textureCatalog = MakeTextureCatalogService();
    textureCatalog.RefreshDefaultTextureIds();
    textureCatalog.RefreshTextureHandles(assets);
    BuildSoundService().RefreshCatalogHandles();
    ReloadSectorSwingDoorCatalog(sceneRuntime.RuntimeObjects());
    initialized = true;
    authoringFaceMergeState = SectorEditorAuthoringFaceMergeState{};
    ClearSectorEditorAuthoringSelection(selectionState);
    ReserveSectorEditorAuthoringFaceSelection(
            selectionState,
            AuthoringGraph().faceAnchors.size());
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

    sceneRuntime.Shutdown(context);
    BuildSoundService().Shutdown();
    if (!engine::IsNull(runtimeObjectEditingState.spritePicker.previewScope)) {
        assets.UnloadScope(runtimeObjectEditingState.spritePicker.previewScope);
    }
    CancelAuthoringVertexDrag(nullptr);
    if (fogVolumeEditingService) {
        fogVolumeEditingService->CancelMove(nullptr);
    }
    if (levelMarkerEditingService) {
        levelMarkerEditingService->CancelMove(nullptr);
    }
    CancelLightDrag(nullptr);
    bool loadedAuthoringGraph = false;
    bool authoringDerivationCurrent = false;
    SectorTopologyMap& topologyMap = TopologyMap();
    if (loaded.format == SectorEditorDocumentFormat::AuthoringGraph) {
        loadedAuthoringGraph = true;
        topologyMap = std::move(loaded.mapData);
        const SectorLightmapMetadata loadedBakedLightmap = topologyMap.bakedLightmap;
        AuthoringGraph() = std::move(loaded.authoringGraph);
        SectorEditorDerivationDocumentAccess derivation =
                MakeLiveDerivationAccess(documentState.derivation);
        derivation.authoringDerivation = SectorAuthoringDerivationResult{};
        derivation.lastValidAuthoringDerivedTopology.reset();
        derivation.lastValidFaceAnchorBindings.clear();
        derivation.authoringDerivationState = SectorEditorAuthoringDerivationState::InvalidNoDerived;
        derivation.authoringDerivedTopologyStale = true;
        const std::string successStatus = TextFormat(
                "Authoring graph: loaded %s; derived topology current",
                jsonAssetPath.c_str());
        const std::string failureStatus = TextFormat(
                "Authoring graph: loaded %s; derivation failed",
                jsonAssetPath.c_str());
        authoringDerivationCurrent = RefreshSectorEditorAuthoringDerivation(
                state,
                Lifecycle(),
                topologyMap,
                AuthoringGraph(),
                derivation,
                successStatus.c_str(),
                failureStatus.c_str());
        if (authoringDerivationCurrent) {
            topologyMap.bakedLightmap = loadedBakedLightmap;
            derivation.authoringDerivation.topology.bakedLightmap = loadedBakedLightmap;
            if (derivation.lastValidAuthoringDerivedTopology.has_value()) {
                derivation.lastValidAuthoringDerivedTopology->bakedLightmap = loadedBakedLightmap;
            }
        }
    } else {
        topologyMap = std::move(loaded.mapData);
        InitializeSectorEditorAuthoringStateFromTopology(
                AuthoringGraph(),
                MakeLiveDerivationAccess(documentState.derivation),
                topologyMap);
    }
    std::string fingerprintError;
    if (!RefreshSectorStaticModelGeometryFingerprints(
                topologyMap,
                fingerprintError)
            && !fingerprintError.empty()) {
        TraceLog(LOG_WARNING, "%s", fingerprintError.c_str());
    }
    InvalidateTopologyRenderCache();
    previewState.controller.fpsControllerConfig = SectorFpsControllerConfigFromPreviewSettings(
            topologyMap.previewSettings);
    Lifecycle().topologyDocumentInitialized = true;
    Lifecycle().topologyDocumentDirty = false;
    if (!loadedAuthoringGraph) {
        Lifecycle().topologyDocumentStatus = TextFormat(
                "Topology document: imported legacy topology %s",
                jsonAssetPath.c_str());
    }
    Lifecycle().currentLevelName = levelName;
    Lifecycle().currentLevelPath = jsonAssetPath;
    Lifecycle().hasCurrentLevelPath = true;
    Lifecycle().hasUnsavedChanges = false;
    state.mode = SectorEditorMode::Edit2D;
    state.gridSize = loaded.editorSettings.gridSize;
    uiState.gridSizeInput = engine::UIIntInputState{};
    previewState.controller.hasPreviewPose = false;
    previewState.selection.hoveredSurface3D = SectorSurfaceHit{};
    state.texturePicker = TexturePickerState{};
    state.soundPicker = SoundPickerState{};
    state.addMapSound = AddMapSoundState{};
    state.loadLevelModal = LoadLevelModalState{};
    state.saveLevelModal = SaveLevelModalState{};
    state.confirmationModal = ConfirmationModalState{};
    state.setAllModal = SectorEditorSetAllModalState{};
    state.decalTintModal = DecalTintModalState{};
    state.doorTextureSettingsModal = DoorTextureSettingsModalState{};
    authoringFaceMergeState = SectorEditorAuthoringFaceMergeState{};
    ClearSelection();
    ReserveSectorEditorAuthoringFaceSelection(
            selectionState,
            AuthoringGraph().faceAnchors.size());
    selectionState.hoveredTopologyLightId = -1;
    selectionState.hoveredTopologyStaticSpotLightId = -1;
    selectionState.hoveredTopologyDynamicLightId = -1;
    selectionState.hoveredTopologyDynamicSpotLightId = -1;
    selectionState.hasHoveredVertex = false;
    selectionState.hoveredTopologyVertexId = -1;
    selectionState.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    manipulationState = ManipulationState{};
    levelMarkerEditingState = LevelMarkerEditingState{};
    levelMarkerEditingUiState = LevelMarkerEditingUiState{};
    triggerEditingState = TriggerEditingState{};
    triggerEditingUiState = TriggerEditingUiState{};
    runtimeObjectEditingState = RuntimeObjectEditingState{};
    runtimeObjectEditingUiState = RuntimeObjectEditingUiState{};
    lightEditingState = LightEditingState{};
    SectorEditorTextureCatalogService textureCatalog = MakeTextureCatalogService();
    textureCatalog.RefreshDefaultTextureIds();
    textureCatalog.RefreshTextureHandles(assets);
    BuildSoundService().RefreshCatalogHandles();
    sceneRuntime.RefreshMapRuntimeObjects(context, topologyMap);
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
    if (!Lifecycle().hasCurrentLevelPath) {
        statusText = "No saved level to reload.";
        return;
    }
    const std::string name = Lifecycle().currentLevelName;
    const std::string path = Lifecycle().currentLevelPath;
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
            Lifecycle().hasCurrentLevelPath,
            Lifecycle().currentLevelName);
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
                Lifecycle().hasCurrentLevelPath,
                Lifecycle().currentLevelPath,
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

    if (!SaveSectorEditorAuthoringDocument(
                savePlan.paths,
                documentState.authoring,
                documentState.map,
                documentState.derivation,
                SectorAuthoringEditorSettings{state.gridSize},
                modal.errorMessage)) {
        statusText = TextFormat("Save failed: %s", savePlan.paths.jsonAssetPath.c_str());
        return false;
    }

    Lifecycle().currentLevelName = name;
    Lifecycle().currentLevelPath = savePlan.paths.jsonAssetPath;
    Lifecycle().hasCurrentLevelPath = true;
    Lifecycle().hasUnsavedChanges = false;
    Lifecycle().topologyDocumentInitialized = true;
    Lifecycle().topologyDocumentDirty = false;
    Lifecycle().topologyDocumentStatus = TextFormat("Authoring graph: saved %s", savePlan.paths.jsonAssetPath.c_str());
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
            || state.setAllModal.open
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
    if (fogVolumeEditingService) {
        fogVolumeEditingService->CancelMove(nullptr);
    }
    if (levelMarkerEditingService) {
        levelMarkerEditingService->CancelMove(nullptr);
    }
    ui.hotId = 0;
    ui.activeId = 0;
    ui.openOptionId = 0;
    ui.focusedId = 0;
    uiState.keyboardCaptured = false;

    std::string gateMessage;
    if (!CanUseCurrentAuthoringDerivedTopologyForPreview(
                MakeLiveConstDerivationAccess(documentState.derivation),
                &gateMessage)) {
        statusText = gateMessage.empty() ? "3D mode failed: derived topology is not current" : gateMessage;
        return false;
    }

    std::string fingerprintError;
    if (!RefreshSectorStaticModelGeometryFingerprints(
                TopologyMap(),
                fingerprintError)
            && !fingerprintError.empty()) {
        TraceLog(LOG_WARNING, "%s", fingerprintError.c_str());
    }
    std::string error;
    if (!sceneRuntime.Rebuild(
                context,
                TopologyMap(),
                "sector_editor_preview",
                applicationSettings.footsteps.defaultSet,
                applicationSettings.footsteps.volume,
                error)) {
        sceneRuntime.RuntimeObjects().objectLightProbes = SectorBakedObjectLightProbeRuntimeData{};
        sceneRuntime.RuntimeObjects().objectProbeStatus.clear();
        sceneRuntime.RuntimeObjects().objectSectorLookupWorld = SectorCollisionWorld{};
        sceneRuntime.RuntimeObjects().objectSectorLookupWorldValid = false;
        sceneRuntime.RuntimeObjects().objectSectorLookupWarning.clear();
        previewState.collision.sectorCollisionWorldValid = false;
        previewState.collision.sectorCollisionWorldWarning.clear();
        previewState.collision.previewCollisionSectorId = 0;
        previewState.controller.fpsControllerState.currentSectorId = 0;
        previewState.collision.previewVerticalResult = SectorFpsVerticalResult{};
        previewState.collision.previewMoveResult = SectorCollisionMoveResult{};
        previewState.collision.previewCollisionNoclipFallback = false;
        previewState.controller.visualStepOffsetY = 0.0f;
        ClearSectorFpsHeadBob(previewState.controller.headBobState);
        ClearSectorFpsFootstepCadence(previewState.controller.footstepCadenceState);
        ClearSectorFpsLandingDip(previewState.controller.landingDipState);
        state.mode = SectorEditorMode::Edit2D;
        if (StartsWith(error, "Preview failed:")) {
            statusText = std::string{"3D mode failed:"} + error.substr(std::strlen("Preview failed:"));
        } else {
            statusText = error.empty() ? "3D mode failed" : error;
        }
        return false;
    }
    RefreshPreviewObjectProbeDebugData();
    BeginFpsViewmodel(assets);

    if (previewState.controller.hasPreviewPose) {
        sceneRuntime.Renderer().ApplyRendererPose(previewState.controller.lastPreviewPose);
    }

    previewState.controller.previewControlMode = SectorPreviewControlMode::FreeFly;
    ResetSectorFreeflyController(previewState.controller.freeflyController, sceneRuntime.Renderer().RendererPose());
    EnterSectorFreeflyController(previewState.controller.freeflyController);
    sceneRuntime.Renderer().ApplyRendererPose(previewState.controller.freeflyController.pose);
    previewState.controller.visualStepOffsetY = 0.0f;
    ResetSectorFpsCrouch(previewState.controller.fpsControllerState);
    ClearSectorFpsHeadBob(previewState.controller.headBobState);
    ClearSectorFpsFootstepCadence(previewState.controller.footstepCadenceState);
    previewState.controller.frameEvents = SectorFpsFrameEvents{};
    ClearSectorFpsLandingDip(previewState.controller.landingDipState);
    previewState.controller.fpsControllerConfig = NormalizeSectorFpsControllerConfig(previewState.controller.fpsControllerConfig);
    state.mode = SectorEditorMode::Preview3D;
    previewState.overlay.previewUiHidden = false;
    previewState.selection.hoveredSurface3D = SectorSurfaceHit{};
    previewState.selection.selectedSurface3D = SectorSurfaceRef{};
    previewState.selection.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    RebuildSectorCollisionWorld();
    statusText = TextFormat(
            "3D mode rebuilt: %zu batches, %d triangles",
            sceneRuntime.Renderer().BatchCount(),
            sceneRuntime.Renderer().TriangleCount()
    );
    return true;
}

void SectorEditor::LeavePreview3D()
{
    CancelLightProxyPlacement(nullptr);
    CancelLightPilotWithPreviewRestore(nullptr);
    if (previewState.controller.previewControlMode == SectorPreviewControlMode::Gameplay) {
        ResetFpsCameraRecoil(fpsPlayer.State().firing.cameraRecoil);
        ClearSectorFpsLandingDip(previewState.controller.landingDipState);
        ApplyGameplayPoseToPreview();
    }
    previewState.controller.lastPreviewPose = ActivePreviewPose();
    previewState.controller.hasPreviewPose = true;
    previewState.controller.visualStepOffsetY = 0.0f;
    ResetSectorFpsCrouch(previewState.controller.fpsControllerState);
    ClearSectorFpsHeadBob(previewState.controller.headBobState);
    ClearSectorFpsFootstepCadence(previewState.controller.footstepCadenceState);
    ClearSectorFpsLandingDip(previewState.controller.landingDipState);
    previewState.controller.previewControlMode = SectorPreviewControlMode::FreeFly;
    state.mode = SectorEditorMode::Edit2D;
    if (engineContext != nullptr) {
        engineContext->audio.StopAll(engineContext->assets);
        sceneRuntime.StopLevelAudio(*engineContext);
        EndFpsViewmodel(engineContext->assets);
    }
    previewState.selection.hoveredSurface3D = SectorSurfaceHit{};
    ResetSectorPreviewSettingsModalPreservingView(
            state.previewSettingsModal);
    LeaveSectorFreeflyController();
    statusText = "Returned to 2D editor";
}

SectorViewPose SectorEditor::ActivePreviewPose() const
{
    return ActiveSectorEditorPreviewPose(
            previewState.controller,
            sceneRuntime.Renderer());
}

void SectorEditor::ApplyGameplayPoseToPreview()
{
    const SectorViewPose basePose = SectorFpsControllerVisualPose(
            previewState.controller.fpsControllerState,
            previewState.controller.fpsControllerConfig,
            previewState.controller.visualStepOffsetY,
            previewState.controller.headBobState.offset,
            previewState.controller.landingDipState.offsetY);
    sceneRuntime.Renderer().ApplyRendererPose(
            ApplySectorFpsViewRotationOffset(
                    basePose,
                    fpsPlayer.State().firing.cameraRecoil.rotationDegrees),
            false);
}

void SectorEditor::TogglePreviewControlMode()
{
    ResetFpsCameraRecoil(fpsPlayer.State().firing.cameraRecoil);
    if (!ToggleSectorEditorPreviewControlMode(
                state.mode == SectorEditorMode::Preview3D,
                previewState.collision,
                previewState.controller,
                sceneRuntime.RuntimeObjects().staticModelColliders,
                sceneRuntime.Renderer())) {
        return;
    }

    statusText = TextFormat("3D control mode: %s", PreviewControlModeName(previewState.controller.previewControlMode));
}

bool SectorEditor::StartLightPilot()
{
    if (state.mode != SectorEditorMode::Preview3D || previewState.controller.previewControlMode != SectorPreviewControlMode::FreeFly) {
        statusText = "Light pilot requires 3D FreeFly mode";
        return false;
    }
    if (lightEditingState.proxyPlacement.active) {
        statusText = "Finish proxy placement before piloting the light";
        return false;
    }

    int lightId = -1;
    LightPilotKind pilotKind = LightPilotKind::None;
    Vector3 lightPosition = {};
    Vector3 lightTarget = {};
    float lightRange = 0.0f;
    const char* lightName = nullptr;
    if (const SectorTopologyStaticPointLight* light = SelectedTopologyLight()) {
        lightId = light->id;
        pilotKind = LightPilotKind::StaticPoint;
        lightPosition = light->position;
        lightName = "static light";
    } else if (const SectorTopologyStaticSpotLight* light = SelectedTopologyStaticSpotLight()) {
        lightId = light->id;
        pilotKind = LightPilotKind::StaticSpot;
        lightPosition = light->position;
        lightTarget = light->target;
        lightRange = light->range;
        lightName = "static spot";
    } else if (const SectorTopologyDynamicPointLight* light = SelectedTopologyDynamicLight()) {
        lightId = light->id;
        pilotKind = LightPilotKind::DynamicPoint;
        lightPosition = light->position;
        lightName = "dynamic light";
    } else if (const SectorTopologyDynamicSpotLight* light = SelectedTopologyDynamicSpotLight()) {
        lightId = light->id;
        pilotKind = LightPilotKind::DynamicSpot;
        lightPosition = light->position;
        lightTarget = light->target;
        lightRange = light->range;
        lightName = "dynamic spot";
    } else {
        statusText = "Select a light to pilot";
        return false;
    }

    const bool isSpot = pilotKind == LightPilotKind::StaticSpot
            || pilotKind == LightPilotKind::DynamicSpot;
    const Vector3 originWorld = SectorAuthoringToWorldPosition(lightPosition);
    Vector3 targetWorld = {};
    float targetDistanceWorld = 4.0f;
    if (isSpot) {
        targetWorld = SectorAuthoringToWorldPosition(lightTarget);
        targetDistanceWorld = Vector3Distance(originWorld, targetWorld);
        if (!std::isfinite(targetDistanceWorld) || targetDistanceWorld <= 0.01f) {
            targetDistanceWorld = std::max(SectorAuthoringToWorldDistance(lightRange) * 0.5f, 4.0f);
        }
    }

    const SectorViewPose originalPreviewPose = ActivePreviewPose();
    lightEditingState.lightPilot.active = true;
    lightEditingState.lightPilot.kind = pilotKind;
    lightEditingState.lightPilot.lightId = lightId;
    lightEditingState.lightPilot.originalPosition = lightPosition;
    lightEditingState.lightPilot.originalTarget = lightTarget;
    previewState.controller.lightPilotPreviewRestore.originalPreviewPose = originalPreviewPose;
    previewState.controller.lightPilotPreviewRestore.originalMouseLookEnabled = previewState.controller.freeflyController.mouseLookEnabled;
    lightEditingState.lightPilot.targetDistanceWorld = targetDistanceWorld;

    SectorViewPose pilotPose = originalPreviewPose;
    pilotPose.position = originWorld;
    if (isSpot) {
        pilotPose = PreviewPoseLookingAt(originWorld, targetWorld);
    }
    ResetSectorFreeflyController(previewState.controller.freeflyController, pilotPose);
    EnterSectorFreeflyController(previewState.controller.freeflyController);
    sceneRuntime.Renderer().ApplyRendererPose(previewState.controller.freeflyController.pose);
    previewState.selection.hoveredSurface3D = SectorSurfaceHit{};
    statusText = TextFormat("Piloting %s %d", lightName, lightId);
    return true;
}

bool SectorEditor::StartLightProxyPlacement(LightProxyPlacementKind proxyKind)
{
    const bool shaft = proxyKind == LightProxyPlacementKind::Shaft;
    const char* proxyName = shaft ? "shaft" : "haze";
    if (proxyKind == LightProxyPlacementKind::None) return false;
    if (state.mode != SectorEditorMode::Preview3D
            || previewState.controller.previewControlMode != SectorPreviewControlMode::FreeFly) {
        statusText = TextFormat("%s placement requires 3D FreeFly mode", proxyName);
        return false;
    }
    if (previewState.controller.freeflyController.mouseLookEnabled) {
        statusText = TextFormat("Unlock the cursor with F11 before placing a %s", proxyName);
        return false;
    }
    if (lightEditingState.lightPilot.active) {
        statusText = TextFormat("Finish light pilot before placing a %s", proxyName);
        return false;
    }

    LightPilotKind kind = LightPilotKind::None;
    int lightId = -1;
    bool proxyEnabled = false;
    if (const SectorTopologyStaticPointLight* light = SelectedTopologyLight()) {
        kind = LightPilotKind::StaticPoint;
        lightId = light->id;
        proxyEnabled = !shaft && light->atmosphere.proxy.halo.enabled;
    } else if (const SectorTopologyStaticSpotLight* light = SelectedTopologyStaticSpotLight()) {
        kind = LightPilotKind::StaticSpot;
        lightId = light->id;
        proxyEnabled = shaft
                ? light->atmosphere.proxy.shaft.enabled
                : light->atmosphere.proxy.halo.enabled;
    } else if (const SectorTopologyDynamicPointLight* light = SelectedTopologyDynamicLight()) {
        kind = LightPilotKind::DynamicPoint;
        lightId = light->id;
        proxyEnabled = !shaft && light->atmosphere.proxy.halo.enabled;
    } else if (const SectorTopologyDynamicSpotLight* light = SelectedTopologyDynamicSpotLight()) {
        kind = LightPilotKind::DynamicSpot;
        lightId = light->id;
        proxyEnabled = shaft
                ? light->atmosphere.proxy.shaft.enabled
                : light->atmosphere.proxy.halo.enabled;
    }
    if (lightId < 0 || !proxyEnabled) {
        statusText = TextFormat(
                "Select a %s with enabled %s",
                shaft ? "spotlight" : "light",
                proxyName);
        return false;
    }

    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    return lightEditing.BeginProxyPlacement(proxyKind, kind, lightId);
}

bool SectorEditor::PreviewLightProxyPlacementOffset(Vector3 offsetWorld)
{
    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    const SectorEditorLightMutationResult result =
            lightEditing.PreviewProxyPlacement(offsetWorld);
    if (result.dynamicLightRendererRefreshNeeded
            && state.mode == SectorEditorMode::Preview3D
            && sceneRuntime.Renderer().IsRendererReady()) {
        sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
    }
    return result.changed;
}

bool SectorEditor::ApplyLightProxyPlacement()
{
    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    const SectorEditorLightMutationResult result = lightEditing.ApplyProxyPlacement();
    if (result.dynamicLightRendererRefreshNeeded
            && state.mode == SectorEditorMode::Preview3D
            && sceneRuntime.Renderer().IsRendererReady()) {
        sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
    }
    return result.changed;
}

void SectorEditor::CancelLightProxyPlacement(const char* message)
{
    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    const SectorEditorLightMutationResult result =
            lightEditing.CancelProxyPlacementData(message);
    if (result.dynamicLightRendererRefreshNeeded
            && state.mode == SectorEditorMode::Preview3D
            && sceneRuntime.Renderer().IsRendererReady()) {
        sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
    }
}

bool SectorEditor::ApplyLightPilotFromPreviewPose()
{
    if (!lightEditingState.lightPilot.active) {
        return false;
    }

    const LightPilotLightState pilot = lightEditingState.lightPilot;
    const SectorViewPose pose = ActivePreviewPose();
    const Vector3 forward = PreviewForwardFromPose(pose);
    const Vector3 targetWorld = Vector3Add(
            pose.position,
            Vector3Scale(forward, pilot.targetDistanceWorld));

    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    const SectorEditorLightMutationResult result = lightEditing.ApplyLightPilot(
            SectorWorldToAuthoringPosition(pose.position),
            SectorWorldToAuthoringPosition(targetWorld));
    if (result.previewPoseRestoreNeeded && state.mode == SectorEditorMode::Preview3D) {
        ResetSectorFreeflyController(
                previewState.controller.freeflyController,
                previewState.controller.lightPilotPreviewRestore.originalPreviewPose);
        SetSectorFreeflyMouseLookEnabled(
                previewState.controller.freeflyController,
                previewState.controller.lightPilotPreviewRestore.originalMouseLookEnabled);
        previewState.controller.lightPilotPreviewRestore = LightPilotPreviewRestoreState{};
        sceneRuntime.Renderer().ApplyRendererPose(previewState.controller.freeflyController.pose);
    }
    if (result.dynamicLightRendererRefreshNeeded) {
        sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
    }
    if (result.changed) {
        previewState.controller.lightPilotPreviewRestore = LightPilotPreviewRestoreState{};
    }
    return result.changed;
}

void SectorEditor::CancelLightPilotWithPreviewRestore(const char* message)
{
    if (!lightEditingState.lightPilot.active) {
        return;
    }

    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    const SectorEditorLightMutationResult result = lightEditing.CancelLightPilotData(message);
    if (result.previewPoseRestoreNeeded && state.mode == SectorEditorMode::Preview3D) {
        ResetSectorFreeflyController(
                previewState.controller.freeflyController,
                previewState.controller.lightPilotPreviewRestore.originalPreviewPose);
        SetSectorFreeflyMouseLookEnabled(
                previewState.controller.freeflyController,
                previewState.controller.lightPilotPreviewRestore.originalMouseLookEnabled);
        previewState.controller.lightPilotPreviewRestore = LightPilotPreviewRestoreState{};
        sceneRuntime.Renderer().ApplyRendererPose(previewState.controller.freeflyController.pose);
    }
    if (result.dynamicLightRendererRefreshNeeded) {
        sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
    }
}

bool SectorEditor::RebuildSectorCollisionWorld()
{
    return RebuildSectorEditorCollisionWorld(
            TopologyMap(),
            previewState.collision,
            previewState.controller,
            sceneRuntime.RuntimeObjects().staticModelColliders);
}

SectorFpsVerticalContext SectorEditor::BuildGameplayVerticalContext()
{
    return BuildSectorEditorGameplayVerticalContext(
            previewState.collision,
            previewState.controller,
            sceneRuntime.RuntimeObjects().staticModelColliders);
}

void SectorEditor::RefreshGameplaySectorAndVerticalContext()
{
    RefreshSectorEditorGameplaySectorAndVerticalContext(previewState.collision, previewState.controller);
}

void SectorEditor::InitializeGameplayVerticalState()
{
    InitializeSectorEditorGameplayVerticalState(
            previewState.collision,
            previewState.controller,
            sceneRuntime.RuntimeObjects().staticModelColliders);
}

void SectorEditor::OpenPreviewSettingsModal()
{
    ResetSectorPreviewSettingsModalPreservingView(
            state.previewSettingsModal);
    state.previewSettingsModal.open = true;
    state.previewSettingsModal.draftConfig = NormalizeSectorFpsControllerConfig(previewState.controller.fpsControllerConfig);
    state.previewSettingsModal.draftSkySettings = NormalizeSectorTopologySkySettings(TopologyMap().skySettings);
    state.previewSettingsModal.draftDirectionalLight =
            NormalizeSectorTopologyDirectionalLightSettings(TopologyMap().directionalLight);
    state.previewSettingsModal.draftFogSettings =
            NormalizeSectorTopologyFogSettings(TopologyMap().fogSettings);
    state.previewSettingsModal.draftLightmapSettings =
            NormalizeSectorPreviewObjectProbeSettings(TopologyMap().lightmapSettings);
    state.previewSettingsModal.draftHdrBloom =
            engine::NormalizeHdrBloomSettings(applicationSettings.hdrBloom);
    state.previewSettingsModal.weaponId = fpsPlayer.State().activeWeaponId.empty()
            ? weaponRegistry.initialWeaponId
            : fpsPlayer.State().activeWeaponId;
    const FpsWeaponDefinition* weapon = FindFpsWeaponDefinition(
            weaponRegistry, state.previewSettingsModal.weaponId);
    if (weapon != nullptr) {
        state.previewSettingsModal.viewmodelDefaults = weapon->viewmodel.presentation;
        state.previewSettingsModal.draftViewmodel = ResolveFpsViewmodelPresentation(
                weapon->viewmodel.presentation,
                FindFpsViewmodelOverride(applicationSettings, weapon->id));
        state.previewSettingsModal.viewmodelHolsterTransitionDefaults =
                weapon->viewmodel.holsterTransition;
        state.previewSettingsModal.draftViewmodelHolsterTransition =
                ResolveFpsViewmodelHolsterTransition(
                        weapon->viewmodel.holsterTransition,
                        FindFpsViewmodelHolsterTransitionOverride(
                                applicationSettings,
                                weapon->id));
        state.previewSettingsModal.viewmodelGripDefaults =
                weapon->viewmodel.attachment.gripCorrection;
        state.previewSettingsModal.draftViewmodelGrip =
                ResolveFpsViewmodelGripCorrection(
                        weapon->viewmodel.attachment.gripCorrection,
                        FindFpsViewmodelGripCorrectionOverride(
                                applicationSettings, weapon->id));
        state.previewSettingsModal.viewmodelAttachmentLightingDefaults =
                weapon->viewmodel.attachment.lighting;
        state.previewSettingsModal.draftViewmodelAttachmentLighting =
                ResolveFpsViewmodelAttachmentLighting(
                        weapon->viewmodel.attachment.lighting,
                        FindFpsViewmodelAttachmentLightingOverride(
                                applicationSettings, weapon->id));
        state.previewSettingsModal.weaponFiringDefaults = weapon->firing;
        state.previewSettingsModal.draftWeaponFiring =
                ResolveFpsWeaponFiringDefinition(
                        weapon->firing,
                        FindFpsWeaponFiringOverride(
                                applicationSettings, weapon->id));
    }
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
                    TopologyMap().previewSettings).objectProbeDebugDrawMaxDistanceWorld;
    const SectorTopologySkySettings draftSkySettings = NormalizeSectorTopologySkySettings(
            state.previewSettingsModal.draftSkySettings);
    const SectorTopologyDirectionalLightSettings draftDirectionalLight =
            NormalizeSectorTopologyDirectionalLightSettings(
                    state.previewSettingsModal.draftDirectionalLight);
    const SectorTopologyFogSettings draftFogSettings =
            NormalizeSectorTopologyFogSettings(state.previewSettingsModal.draftFogSettings);
    const SectorLightmapBakeSettings draftLightmapSettings =
            NormalizeSectorPreviewObjectProbeSettings(state.previewSettingsModal.draftLightmapSettings);
    const bool previewChanged = !SamePreviewSettings(
            TopologyMap().previewSettings,
            draftPreviewSettings);
    const bool navigationClimbChanged =
            NormalizeSectorPreviewSettings(TopologyMap().previewSettings).stepHeight
                    != draftPreviewSettings.stepHeight;
    const bool skyChanged = !SameSkySettings(TopologyMap().skySettings, draftSkySettings);
    const bool directionalChanged = !SameDirectionalLightSettings(
            TopologyMap().directionalLight,
            draftDirectionalLight);
    const bool fogChanged = !SameFogSettings(TopologyMap().fogSettings, draftFogSettings);
    const SectorLightmapBakeSettings currentLightmapSettings =
            NormalizeSectorPreviewObjectProbeSettings(TopologyMap().lightmapSettings);
    const bool objectProbeSettingsChanged =
            currentLightmapSettings.objectProbeSpacingWorld != draftLightmapSettings.objectProbeSpacingWorld
            || currentLightmapSettings.objectProbeLowerHeightWorld
                    != draftLightmapSettings.objectProbeLowerHeightWorld
            || currentLightmapSettings.objectProbeUpperHeightWorld
                    != draftLightmapSettings.objectProbeUpperHeightWorld;
    const engine::HdrBloomSettings draftHdrBloom =
            engine::NormalizeHdrBloomSettings(state.previewSettingsModal.draftHdrBloom);
    const engine::HdrBloomSettings currentHdrBloom =
            engine::NormalizeHdrBloomSettings(applicationSettings.hdrBloom);
    const bool bloomChanged = draftHdrBloom.enabled != currentHdrBloom.enabled
            || draftHdrBloom.threshold != currentHdrBloom.threshold
            || draftHdrBloom.softKnee != currentHdrBloom.softKnee
            || draftHdrBloom.intensity != currentHdrBloom.intensity
            || draftHdrBloom.radius != currentHdrBloom.radius;
    const FpsWeaponDefinition* weapon = FindFpsWeaponDefinition(
            weaponRegistry,
            state.previewSettingsModal.weaponId.empty()
                    ? weaponRegistry.initialWeaponId
                    : state.previewSettingsModal.weaponId);
    const FpsViewmodelPresentation draftViewmodel = ClampFpsViewmodelPresentation(
            state.previewSettingsModal.draftViewmodel);
    const FpsViewmodelPresentation currentViewmodel = weapon != nullptr
            ? ResolveFpsViewmodelPresentation(weapon->viewmodel.presentation,
                    FindFpsViewmodelOverride(applicationSettings, weapon->id))
            : FpsViewmodelPresentation{};
    const FpsViewmodelPresentationOverride viewmodelOverride = weapon != nullptr
            ? BuildFpsViewmodelOverride(weapon->viewmodel.presentation, draftViewmodel)
            : FpsViewmodelPresentationOverride{};
    const bool viewmodelChanged = weapon != nullptr
            && !FpsViewmodelOverrideEmpty(
                    BuildFpsViewmodelOverride(currentViewmodel, draftViewmodel));
    const FpsViewmodelHolsterTransition draftHolsterTransition =
            ClampFpsViewmodelHolsterTransition(
                    state.previewSettingsModal
                            .draftViewmodelHolsterTransition);
    const FpsViewmodelHolsterTransition currentHolsterTransition =
            weapon != nullptr
            ? ResolveFpsViewmodelHolsterTransition(
                    weapon->viewmodel.holsterTransition,
                    FindFpsViewmodelHolsterTransitionOverride(
                            applicationSettings,
                            weapon->id))
            : FpsViewmodelHolsterTransition{};
    const FpsViewmodelHolsterTransitionOverride holsterTransitionOverride =
            weapon != nullptr
            ? BuildFpsViewmodelHolsterTransitionOverride(
                    weapon->viewmodel.holsterTransition,
                    draftHolsterTransition)
            : FpsViewmodelHolsterTransitionOverride{};
    const bool holsterTransitionChanged = weapon != nullptr
            && !FpsViewmodelHolsterTransitionOverrideEmpty(
                    BuildFpsViewmodelHolsterTransitionOverride(
                            currentHolsterTransition,
                            draftHolsterTransition));
    const FpsViewmodelGripCorrection draftGrip =
            ClampFpsViewmodelGripCorrection(
                    state.previewSettingsModal.draftViewmodelGrip);
    const FpsViewmodelGripCorrection currentGrip = weapon != nullptr
            ? ResolveFpsViewmodelGripCorrection(
                    weapon->viewmodel.attachment.gripCorrection,
                    FindFpsViewmodelGripCorrectionOverride(
                            applicationSettings, weapon->id))
            : FpsViewmodelGripCorrection{};
    const FpsViewmodelGripCorrectionOverride gripOverride = weapon != nullptr
            ? BuildFpsViewmodelGripCorrectionOverride(
                    weapon->viewmodel.attachment.gripCorrection,
                    draftGrip)
            : FpsViewmodelGripCorrectionOverride{};
    const bool gripChanged = weapon != nullptr
            && !FpsViewmodelGripCorrectionOverrideEmpty(
                    BuildFpsViewmodelGripCorrectionOverride(
                            currentGrip, draftGrip));
    const FpsViewmodelAttachmentLighting draftAttachmentLighting =
            ClampFpsViewmodelAttachmentLighting(
                    state.previewSettingsModal
                            .draftViewmodelAttachmentLighting);
    const FpsViewmodelAttachmentLighting currentAttachmentLighting =
            weapon != nullptr
            ? ResolveFpsViewmodelAttachmentLighting(
                    weapon->viewmodel.attachment.lighting,
                    FindFpsViewmodelAttachmentLightingOverride(
                            applicationSettings, weapon->id))
            : FpsViewmodelAttachmentLighting{};
    const FpsViewmodelAttachmentLightingOverride attachmentLightingOverride =
            weapon != nullptr
            ? BuildFpsViewmodelAttachmentLightingOverride(
                    weapon->viewmodel.attachment.lighting,
                    draftAttachmentLighting)
            : FpsViewmodelAttachmentLightingOverride{};
    const bool attachmentLightingChanged = weapon != nullptr
            && !FpsViewmodelAttachmentLightingOverrideEmpty(
                    BuildFpsViewmodelAttachmentLightingOverride(
                            currentAttachmentLighting,
                            draftAttachmentLighting));
    const FpsWeaponFiringDefinition draftFiring =
            ClampFpsWeaponFiringDefinition(
                    state.previewSettingsModal.draftWeaponFiring);
    const FpsWeaponFiringDefinition currentFiring = weapon != nullptr
            ? ResolveFpsWeaponFiringDefinition(
                    weapon->firing,
                    FindFpsWeaponFiringOverride(applicationSettings, weapon->id))
            : FpsWeaponFiringDefinition{};
    const FpsWeaponFiringOverride firingOverride = weapon != nullptr
            ? BuildFpsWeaponFiringOverride(weapon->firing, draftFiring)
            : FpsWeaponFiringOverride{};
    const bool firingChanged = weapon != nullptr
            && !FpsWeaponFiringOverrideEmpty(
                    BuildFpsWeaponFiringOverride(currentFiring, draftFiring));
    if (!previewChanged && !skyChanged && !directionalChanged && !fogChanged
            && !objectProbeSettingsChanged && !viewmodelChanged && !gripChanged
            && !holsterTransitionChanged && !attachmentLightingChanged
            && !firingChanged && !bloomChanged) {
        ResetSectorPreviewSettingsModalPreservingView(
                state.previewSettingsModal);
        statusText = "Preview settings unchanged";
        return;
    }

    const float previousGravity = NormalizeSectorFpsControllerConfig(previewState.controller.fpsControllerConfig).gravity;
    previewState.controller.fpsControllerConfig = draftConfig;
    if (previousGravity > 0.0f && previewState.controller.fpsControllerConfig.gravity == 0.0f) {
        previewState.controller.fpsControllerState.verticalVelocity = 0.0f;
    }
    TopologyMap().previewSettings = draftPreviewSettings;
    TopologyMap().skySettings = draftSkySettings;
    TopologyMap().directionalLight = draftDirectionalLight;
    ApplySectorPreviewFogSettings(TopologyMap(), draftFogSettings);
    ApplySectorPreviewObjectProbeSettings(TopologyMap(), draftLightmapSettings);
    applicationSettings.hdrBloom = draftHdrBloom;
    const bool weaponApplicationSettingsChanged = viewmodelChanged
            || holsterTransitionChanged || gripChanged
            || attachmentLightingChanged || firingChanged;
    if (weaponApplicationSettingsChanged && weapon != nullptr) {
        if (viewmodelChanged) {
            if (FpsViewmodelOverrideEmpty(viewmodelOverride)) {
                ClearFpsViewmodelOverride(applicationSettings, weapon->id);
            } else {
                SetFpsViewmodelOverride(
                        applicationSettings, weapon->id, viewmodelOverride);
            }
        }
        if (holsterTransitionChanged) {
            if (FpsViewmodelHolsterTransitionOverrideEmpty(
                        holsterTransitionOverride)) {
                ClearFpsViewmodelHolsterTransitionOverride(
                        applicationSettings,
                        weapon->id);
            } else {
                SetFpsViewmodelHolsterTransitionOverride(
                        applicationSettings,
                        weapon->id,
                        holsterTransitionOverride);
            }
        }
        if (gripChanged) {
            if (FpsViewmodelGripCorrectionOverrideEmpty(gripOverride)) {
                ClearFpsViewmodelGripCorrectionOverride(
                        applicationSettings, weapon->id);
            } else {
                SetFpsViewmodelGripCorrectionOverride(
                        applicationSettings, weapon->id, gripOverride);
            }
        }
        if (attachmentLightingChanged) {
            if (FpsViewmodelAttachmentLightingOverrideEmpty(
                        attachmentLightingOverride)) {
                ClearFpsViewmodelAttachmentLightingOverride(
                        applicationSettings, weapon->id);
            } else {
                SetFpsViewmodelAttachmentLightingOverride(
                        applicationSettings,
                        weapon->id,
                        attachmentLightingOverride);
            }
        }
        if (firingChanged) {
            if (FpsWeaponFiringOverrideEmpty(firingOverride)) {
                ClearFpsWeaponFiringOverride(applicationSettings, weapon->id);
            } else {
                SetFpsWeaponFiringOverride(
                        applicationSettings, weapon->id, firingOverride);
            }
        }
    }
    if (weaponApplicationSettingsChanged || bloomChanged) {
        std::string saveError;
        if (!SaveFpsApplicationSettings(applicationSettingsPath, applicationSettings, &saveError)) {
            TraceLog(LOG_WARNING, "Could not persist application settings: %s", saveError.c_str());
        }
    }
    if (previewChanged || skyChanged || directionalChanged || fogChanged || objectProbeSettingsChanged) {
        MarkTopologyDocumentEdited("Preview settings updated");
    }
    ResetSectorPreviewSettingsModalPreservingView(
            state.previewSettingsModal);
    if (skyChanged && state.mode == SectorEditorMode::Preview3D && sceneRuntime.Renderer().IsRendererReady()) {
        if (engineContext != nullptr) {
            RebuildPreviewMeshesPreservingView(*engineContext);
        }
    } else if (navigationClimbChanged
            && state.mode == SectorEditorMode::Preview3D
            && sceneRuntime.Renderer().IsRendererReady()
            && engineContext != nullptr) {
        if (!sceneRuntime.RebuildNavigationForMap(*engineContext, TopologyMap())) {
            TraceLog(LOG_WARNING, "Could not rebuild navigation after step-height change");
        }
    }
    if (state.mode == SectorEditorMode::Preview3D
            && previewState.controller.previewControlMode == SectorPreviewControlMode::Gameplay
            && sceneRuntime.Renderer().IsRendererReady()) {
        previewState.collision.previewVerticalResult = UpdateSectorFpsVerticalPhysics(
                previewState.controller.fpsControllerState,
                previewState.controller.fpsControllerConfig,
                BuildGameplayVerticalContext(),
                0.0f);
        previewState.controller.visualStepOffsetY = 0.0f;
        ClearSectorFpsHeadBob(previewState.controller.headBobState);
        ClearSectorFpsFootstepCadence(previewState.controller.footstepCadenceState);
        ClearSectorFpsLandingDip(previewState.controller.landingDipState);
        ApplyGameplayPoseToPreview();
    }
    statusText = TextFormat(
            "Preview settings updated: walk %.1f run %.1f eye %.1f gravity %.1f radius %.2f height %.2f step %.2f jump %.2f bob %.3f freq %.1f",
            previewState.controller.fpsControllerConfig.walkSpeed,
            previewState.controller.fpsControllerConfig.runSpeed,
            previewState.controller.fpsControllerConfig.eyeHeight,
            previewState.controller.fpsControllerConfig.gravity,
            previewState.controller.fpsControllerConfig.playerRadius,
            previewState.controller.fpsControllerConfig.playerHeight,
            previewState.controller.fpsControllerConfig.stepHeight,
            previewState.controller.fpsControllerConfig.jumpHeight,
            previewState.controller.fpsControllerConfig.headBobStrength,
            previewState.controller.fpsControllerConfig.headBobFrequency);
}

SectorEditorTextureCatalogService SectorEditor::MakeTextureCatalogService()
{
    return SectorEditorTextureCatalogService{
            SectorEditorTextureCatalogServiceContext{
                    TopologyMap(),
                    textureCatalogState,
                    state.defaultFloorTextureId,
                    state.defaultCeilingTextureId,
                    state.defaultWallTextureId,
                    state.defaultLowerWallTextureId,
                    state.defaultUpperWallTextureId}};
}

SectorEditorNpcEditorService SectorEditor::BuildNpcEditorService()
{
    return SectorEditorNpcEditorService{
            npcEditorState,
            npcEditorSessionState,
            statusText,
            std::filesystem::path(ASSETS_PATH) / "npcs"};
}

void SectorEditor::OpenAddMapTextureModal(engine::AssetManager& assets)
{
    CloseAddMapTextureModal(assets);
    state.addMapTexture.open = true;
    game::RefreshAddMapTextureScan(state.addMapTexture);
    SectorEditorTextureCatalogService textureCatalog = MakeTextureCatalogService();
    textureCatalog.SelectAddMapTexturePath(
            state.addMapTexture,
            state.addMapTexture.selectedPathIndex);
    statusText = "Add topology texture";
}

void SectorEditor::CloseAddMapTextureModal(engine::AssetManager& assets)
{
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }
    state.addMapTexture = AddMapTextureState{};
}

bool SectorEditor::AddSelectedMapTexture(engine::AssetManager& assets)
{
    SectorEditorTextureCatalogService textureCatalog = MakeTextureCatalogService();
    const SectorEditorAddTextureResult result =
            textureCatalog.RegisterSelectedMapTexture(state.addMapTexture);
    if (!result.success) {
        return false;
    }

    textureCatalog.RefreshTextureHandles(assets);
    Lifecycle().hasUnsavedChanges = true;
    Lifecycle().topologyDocumentDirty = true;
    statusText = TextFormat("%s texture %s", result.replacing ? "Updated" : "Added", result.textureId.c_str());
    CloseAddMapTextureModal(assets);
    return true;
}

bool SectorEditor::PointInTopologyLoop(Vector2 mapPoint, const SectorTopologyLoop& loop) const
{
    std::vector<SectorPoint> points;
    points.reserve(loop.vertexIds.size());
    for (int vertexId : loop.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(TopologyMap(), vertexId);
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
    if (!ExtractSectorTopologyLoops(TopologyMap(), sector.id, loops, &loopIssues)) {
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
    for (const SectorTopologySector& sector : TopologyMap().sectors) {
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
    for (const SectorTopologyStaticPointLight& light : TopologyMap().staticLights) {
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
    for (const SectorTopologyStaticSpotLight& light : TopologyMap().staticSpotLights) {
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
    for (const SectorTopologyDynamicPointLight& light : TopologyMap().dynamicPointLights) {
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
    for (const SectorTopologyDynamicSpotLight& light : TopologyMap().dynamicSpotLights) {
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

    for (const SectorTopologyStaticSpotLight& light : TopologyMap().staticSpotLights) {
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

    for (const SectorTopologyDynamicSpotLight& light : TopologyMap().dynamicSpotLights) {
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
    for (const SectorPlacedRuntimeObject& object : TopologyMap().runtimeObjects) {
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

    for (const SectorTopologyLineDef& lineDef : TopologyMap().lineDefs) {
        const SectorTopologyVertex* start = nullptr;
        const SectorTopologyVertex* end = nullptr;
        if (!GetSectorTopologyLineVertices(TopologyMap(), lineDef, start, end)) {
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
            && FindSectorTopologySideDef(TopologyMap(), bestLine->frontSideDefId) != nullptr;
    const bool hasBack = bestLine->backSideDefId >= 0
            && FindSectorTopologySideDef(TopologyMap(), bestLine->backSideDefId) != nullptr;

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
    const SectorAuthoringGraph& authoringGraph = AuthoringGraph();
    if (!FindSectorEditorAuthoringLineNearMapPoint(
                authoringGraph,
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
    const SectorAuthoringGraph& authoringGraph = AuthoringGraph();
    for (const SectorAuthoringVertex& vertex : authoringGraph.vertices) {
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
    const SectorEditorConstDerivationDocumentAccess derivation =
            MakeLiveConstDerivationAccess(documentState.derivation);
    const bool authoringDerivationCurrent =
            IsSectorEditorAuthoringDerivationCurrent(derivation);
    return FindSectorEditorAuthoringSelectionAtMapPoint(
            AuthoringGraph(),
            derivation.authoringDerivation,
            authoringDerivationCurrent,
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
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContextFromState(
            const_cast<SectorEditorState&>(state),
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            MakeLiveConstDerivationAccess(documentState.derivation),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            const_cast<RuntimeObjectEditingState&>(runtimeObjectEditingState),
            const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
            const_cast<SectorEditorUiState&>(uiState),
            const_cast<InspectorIdUiState&>(inspectorIdUiState),
            const_cast<MaterialEditingUiState&>(materialEditingUiState));
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
    SectorEditorSelectionServiceContext context = BuildSelectionServiceContextFromState(
            const_cast<SectorEditorState&>(state),
            const_cast<SectorTopologyMap&>(TopologyMap()),
            const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
            MakeLiveConstDerivationAccess(documentState.derivation),
            const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
            const_cast<SelectionState&>(selectionState),
            const_cast<ManipulationState&>(manipulationState),
            const_cast<RuntimeObjectEditingState&>(runtimeObjectEditingState),
            const_cast<RuntimeObjectEditingUiState&>(runtimeObjectEditingUiState),
            const_cast<SectorEditorUiState&>(uiState),
            const_cast<InspectorIdUiState&>(inspectorIdUiState),
            const_cast<MaterialEditingUiState&>(materialEditingUiState));
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
    const int lineId = selectionState.selectedAuthoring.lineId;
    const bool hadValidSelection =
            selectionState.selectedAuthoring.kind
                    == SectorAuthoringSelectionKind::Line
            && FindSectorAuthoringLine(AuthoringGraph(), lineId) != nullptr;
    if (!DeleteSectorEditorSelectedAuthoringLine(
                state,
                Lifecycle(),
                TopologyMap(),
                AuthoringGraph(),
                MakeLiveDerivationAccess(documentState.derivation),
                selectionState)) {
        statusText = hadValidSelection
                ? documentState.derivation.authoringDerivationStatus
                : "Select an authoring line to delete.";
        return false;
    }

    statusText = documentState.derivation.authoringDerivationStatus.empty()
            ? TextFormat("Deleted authoring line %d", lineId)
            : documentState.derivation.authoringDerivationStatus;
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
    const int vertexId = selectionState.selectedAuthoring.vertexId;
    if (!DeleteSectorEditorSelectedAuthoringVertex(
                state,
                Lifecycle(),
                TopologyMap(),
                AuthoringGraph(),
                MakeLiveDerivationAccess(documentState.derivation),
                selectionState)) {
        const SectorEditorConstDerivationDocumentAccess derivation =
                MakeLiveConstDerivationAccess(documentState.derivation);
        statusText = derivation.authoringDerivationStatus.empty()
                ? "Select an isolated or degree-2 authoring vertex to delete."
                : derivation.authoringDerivationStatus;
        return false;
    }

    statusText = documentState.derivation.authoringDerivationStatus.empty()
            ? TextFormat("Deleted authoring vertex %d", vertexId)
            : documentState.derivation.authoringDerivationStatus;
    return true;
}

bool SectorEditor::HasAuthoringGraphData() const
{
    const SectorAuthoringGraph& authoringGraph = AuthoringGraph();
    return !authoringGraph.vertices.empty()
            || !authoringGraph.lines.empty()
            || !authoringGraph.lineSides.empty()
            || !authoringGraph.faceAnchors.empty();
}

bool SectorEditor::EnsureSelectedSurface3DAuthoringMappingCurrent()
{
    std::string unavailableStatus;
    const SectorEditorConstDerivationDocumentAccess derivation =
            MakeLiveConstDerivationAccess(documentState.derivation);
    const bool authoringDerivationCurrent =
            IsSectorEditorAuthoringDerivationCurrent(derivation);
    if (ClearSelectedSectorEditorSurface3DIfAuthoringMappingUnavailable(
                TopologyMap(),
                AuthoringGraph(),
                derivation.authoringDerivation,
                authoringDerivationCurrent,
                previewState.selection,
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
                    Lifecycle(),
                    TopologyMap(),
                    AuthoringGraph(),
                    MakeLiveDerivationAccess(documentState.derivation),
                    state.topologyRenderWarning,
                    state.topologyRenderRevision,
                    state.topologyRenderCache,
                    previewState.selection,
                    selectionState,
                    materialEditingState,
                    materialEditingUiState,
                    state.texturePicker,
                    state.decalTintModal,
                    statusText,
                    [this](engine::AssetManager*) {
                        if (state.mode == SectorEditorMode::Preview3D
                                && sceneRuntime.Renderer().IsRendererReady()
                                && engineContext != nullptr) {
                            return RebuildPreviewMeshesPreservingView(*engineContext);
                        }
                        return true;
                    }}};
}

SectorEditorFootstepService SectorEditor::BuildFootstepService()
{
    return SectorEditorFootstepService{
            SectorEditorFootstepServiceContext{
                    state,
                    Lifecycle(),
                    TopologyMap(),
                    AuthoringGraph(),
                    MakeLiveDerivationAccess(documentState.derivation),
                    applicationSettings,
                    statusText,
                    *engineContext}};
}

SectorEditorLightEditingService SectorEditor::BuildLightEditingService()
{
    return SectorEditorLightEditingService{
            SectorEditorLightEditingServiceContext{
                    TopologyMap(),
                    lightEditingState,
                    Lifecycle(),
                    state.topologyRenderRevision,
                    state.topologyRenderCache,
                    {
                            manipulationState,
                            runtimeObjectEditingState.drag,
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
                            previewState.selection.selectedSurface3D,
                            previewState.selection.selectedTopologySurface3D,
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
                            {
                                    &uiState.lightDustAmountInput,
                                    &uiState.lightDustExtentScaleInput,
                                    &uiState.lightDustMinimumSizeInput,
                                    &uiState.lightDustMaximumSizeInput,
                                    &uiState.lightDustOpacityInput,
                                    &uiState.lightDustDriftSpeedInput,
                                    &uiState.lightDustTurbulenceInput,
                                    &uiState.lightDustRedInput,
                                    &uiState.lightDustGreenInput,
                                    &uiState.lightDustBlueInput,
                                    &uiState.lightProxyHaloRadiusInput,
                                    &uiState.lightProxyHaloOffsetXInput,
                                    &uiState.lightProxyHaloOffsetYInput,
                                    &uiState.lightProxyHaloOffsetZInput,
                                    &uiState.lightProxyHaloBrightnessInput,
                                    &uiState.lightProxyHaloMaxExtinctionInput,
                                    &uiState.lightProxyHaloSoftnessInput,
                                    &uiState.lightProxyHaloRedInput,
                                    &uiState.lightProxyHaloGreenInput,
                                    &uiState.lightProxyHaloBlueInput,
                                    &uiState.lightProxyShaftOffsetXInput,
                                    &uiState.lightProxyShaftOffsetYInput,
                                    &uiState.lightProxyShaftOffsetZInput,
                                    &uiState.lightProxyShaftLengthInput,
                                    &uiState.lightProxyShaftWidthInput,
                                    &uiState.lightProxyShaftBrightnessInput,
                                    &uiState.lightProxyShaftMaxExtinctionInput,
                                    &uiState.lightProxyShaftSoftnessInput,
                                    &uiState.lightProxyShaftRedInput,
                                    &uiState.lightProxyShaftGreenInput,
                                    &uiState.lightProxyShaftBlueInput,
                            },
                    },
                    statusText}};
}

bool SectorEditor::RebuildPreviewMeshesPreservingView(engine::EngineContext& context)
{
    engine::AssetManager& assets = context.assets;
    if (!sceneRuntime.Renderer().IsRendererReady()) {
        return false;
    }

    std::string gateMessage;
    if (!CanUseCurrentAuthoringDerivedTopologyForPreview(
                MakeLiveConstDerivationAccess(documentState.derivation),
                &gateMessage)) {
        statusText = gateMessage.empty() ? "3D mode rebuild failed: derived topology is not current" : gateMessage;
        return false;
    }

    if (previewState.controller.previewControlMode == SectorPreviewControlMode::Gameplay) {
        ResetFpsCameraRecoil(fpsPlayer.State().firing.cameraRecoil);
        ClearSectorFpsLandingDip(previewState.controller.landingDipState);
        ApplyGameplayPoseToPreview();
    }
    const SectorViewPose pose = sceneRuntime.Renderer().RendererPose();
    const bool mouseLook = previewState.controller.freeflyController.mouseLookEnabled;
    const SectorSurfaceRef selected = previewState.selection.selectedSurface3D;
    const TopologySurfaceEditTarget selectedTarget = previewState.selection.selectedTopologySurface3D;

    std::string fingerprintError;
    if (!RefreshSectorStaticModelGeometryFingerprints(
                TopologyMap(),
                fingerprintError)
            && !fingerprintError.empty()) {
        TraceLog(LOG_WARNING, "%s", fingerprintError.c_str());
    }
    std::string error;
    if (!sceneRuntime.Rebuild(
                context,
                TopologyMap(),
                "sector_editor_preview",
                applicationSettings.footsteps.defaultSet,
                applicationSettings.footsteps.volume,
                error)) {
        sceneRuntime.RuntimeObjects().objectLightProbes = SectorBakedObjectLightProbeRuntimeData{};
        sceneRuntime.RuntimeObjects().objectProbeStatus.clear();
        sceneRuntime.RuntimeObjects().objectSectorLookupWorld = SectorCollisionWorld{};
        sceneRuntime.RuntimeObjects().objectSectorLookupWorldValid = false;
        sceneRuntime.RuntimeObjects().objectSectorLookupWarning.clear();
        previewState.collision.sectorCollisionWorldValid = false;
        previewState.collision.sectorCollisionWorldWarning.clear();
        previewState.collision.previewCollisionSectorId = 0;
        previewState.controller.fpsControllerState.currentSectorId = 0;
        previewState.collision.previewVerticalResult = SectorFpsVerticalResult{};
        previewState.collision.previewMoveResult = SectorCollisionMoveResult{};
        previewState.collision.previewCollisionNoclipFallback = false;
        ClearSectorFpsLandingDip(previewState.controller.landingDipState);
        if (StartsWith(error, "Preview failed:")) {
            statusText = std::string{"3D mode failed:"} + error.substr(std::strlen("Preview failed:"));
        } else {
            statusText = error.empty() ? "3D mode rebuild failed" : error;
        }
        state.mode = SectorEditorMode::Edit2D;
        context.audio.StopAll(context.assets);
        EndFpsViewmodel(context.assets);
        LeaveSectorFreeflyController();
        return false;
    }
    RefreshPreviewObjectProbeDebugData();

    sceneRuntime.Renderer().ApplyRendererPose(pose);
    ResetSectorFreeflyController(previewState.controller.freeflyController, pose);
    SetSectorFreeflyMouseLookEnabled(previewState.controller.freeflyController, mouseLook);
    const bool selectedStillValid = IsValidSurfaceRef(selected);
    previewState.selection.selectedSurface3D = selectedStillValid ? selected : SectorSurfaceRef{};
    previewState.selection.selectedTopologySurface3D = selectedStillValid && IsValidTopologySurfaceEditTarget(selectedTarget)
            ? selectedTarget
            : TopologySurfaceEditTarget{};
    RebuildSectorCollisionWorld();
    return true;
}

std::string SectorEditor::CurrentTextureForPickerTarget() const
{
    if (IsSectorEditorMaterialTexturePickerTarget(state.texturePicker.topologyTargetKind)) {
        SectorEditorMaterialEditingServiceContext serviceContext{
                MakeSectorEditorDocumentLifecycleAccess(
                        const_cast<SectorEditorDocumentLifecycleState&>(documentState.lifecycle)),
                const_cast<SectorTopologyMap&>(TopologyMap()),
                const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
                MakeLiveDerivationAccess(const_cast<SectorEditorDerivationState&>(documentState.derivation)),
                const_cast<std::string&>(state.topologyRenderWarning),
                const_cast<uint64_t&>(state.topologyRenderRevision),
                const_cast<SectorEditorTopologyRenderCache&>(state.topologyRenderCache),
                const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
                const_cast<SelectionState&>(selectionState),
                const_cast<MaterialEditingState&>(materialEditingState),
                const_cast<MaterialEditingUiState&>(materialEditingUiState),
                const_cast<TexturePickerState&>(state.texturePicker),
                const_cast<DecalTintModalState&>(state.decalTintModal),
                const_cast<std::string&>(statusText),
                nullptr};
        SectorEditorMaterialEditingService materialEditing{serviceContext};
        return materialEditing.CurrentTextureForPickerTarget();
    }
    return game::CurrentTextureForPickerTarget(
            state,
            TopologyMap(),
            AuthoringGraph(),
            MakeLiveConstDerivationAccess(documentState.derivation));
}

void SectorEditor::OpenSelectedBillboardSpritePicker()
{
    const SectorPlacedRuntimeObject* object = SelectedRuntimeObject();
    if (object == nullptr || object->kind != "billboard") {
        statusText = "Select a billboard first.";
        return;
    }

    OpenBillboardSpritePicker(
            runtimeObjectEditingState.spritePicker,
            object->billboard.spriteAnimationPath);
}

void SectorEditor::ApplySelectedBillboardSpritePickerSelection()
{
    const SectorEditorSpritePickerResult selected = SelectedSpritePickerResult(runtimeObjectEditingState.spritePicker);
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
    SectorEditorTextureCatalogService textureCatalog = MakeTextureCatalogService();
    if (!game::OpenMapSkyTexturePicker(state, TopologyMap(), AuthoringGraph(), textureCatalog)) {
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

    SectorEditorTextureCatalogService textureCatalog = MakeTextureCatalogService();
    if (!game::OpenRuntimeDoorTexturePicker(state, TopologyMap(), AuthoringGraph(), textureCatalog, object->id)) {
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

        if (selectionState.selectedRuntimeObjectId != targetObjectId) {
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

    game::ApplyTexturePickerSelection(
            state,
            Lifecycle(),
            TopologyMap(),
            AuthoringGraph(),
            MakeLiveDerivationAccess(documentState.derivation));
}

} // namespace game
