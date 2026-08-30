#include "sector_editor/SectorEditor.h"

#include "engine/input/InputEvents.h"
#include "engine/render/ColorTransfer.h"
#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorColorSettingsModal.h"
#include "sector_editor/SectorEditorDirtyState.h"
#include "sector_editor/document/SectorEditorDocumentActions.h"
#include "sector_editor/document/SectorEditorDocumentModals.h"
#include "sector_editor/inspector/SectorEditorInspectorPanel.h"
#include "sector_editor/npcs/SectorEditorNpcEditorModal.h"
#include "sector_editor/patrols/SectorEditorPatrolEditorPanel.h"
#include "sector_editor/items/SectorEditorItemEditorPanel.h"
#include "sector_editor/materials/SectorEditorMaterialRegistryEditorPanel.h"
#include "sector_editor/sounds/SectorEditorSoundEditorPanel.h"
#include "sector_editor/weapons/SectorEditorWeaponEditorPanel.h"
#include "sector_editor/player/SectorEditorPlayerSettingsModal.h"
#include "sector_editor/selection/SectorEditorManipulationService.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorLightInspector.h"
#include "sector_editor/SectorEditorLightmapModal.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorMaterialModals.h"
#include "sector_editor/SectorEditorMainMenu.h"
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
#include "sector_demo/SectorRectLight.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorReflectionProbes.h"
#include "sector_demo/SectorStaticModelTransform.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyGeometry.h"
#include "sector_demo/SectorTopologySerialization.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cstdio>
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

void SetFpsViewmodelEquipPose(
        FpsViewmodelRuntimeState& state,
        FpsViewmodelEquipState equipState,
        float equipProgress)
{
    state.equipState = equipState;
    state.equipProgress = std::clamp(equipProgress, 0.0f, 1.0f);
    state.holsterPose = EvaluateFpsViewmodelHolsterPose(
            state.holsterTransition,
            state.equipProgress);
}

void EquipFpsViewmodelForWeaponEditing(FpsViewmodelRuntimeState& state)
{
    SetFpsViewmodelEquipPose(
            state,
            FpsViewmodelEquipState::Equipped,
            1.0f);
}

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
            uiState.lightStartFeatherInput,
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
    weaponRegistryPath = ASSETS_PATH "config/weapons.json";
    if (!LoadFpsWeaponRegistry(weaponRegistryPath, weaponRegistry, &weaponRegistryError)) {
        TraceLog(LOG_ERROR, "Weapon registry load failed: %s", weaponRegistryError.c_str());
        statusText = "Startup failed: " + weaponRegistryError;
        return false;
    }
    RequestFpsWeaponAudioAssets(context.assets, weaponRegistry);
    itemRegistryPath = ASSETS_PATH "config/items.json";
    applicationSettingsPath = ASSETS_PATH "config/application_settings.json";
    RequestPlayerAudioAssets(
            context.assets,
            applicationSettings.playerSounds,
            playerAudio);
    sceneRuntime.SetItemRuntimeAssets(&itemRegistry, &itemModelAssets);
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
    reflectionProbeEditingService.emplace(
            SectorEditorReflectionProbeEditingServiceContext{
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
    soundEmitterEditingService.emplace(
            SectorEditorSoundEmitterEditingServiceContext{
                    Lifecycle(), TopologyMap(), AuthoringGraph(),
                    MakeLiveDerivationAccess(documentState.derivation),
                    state.topologyRenderRevision, state.topologyRenderCache,
                    selectionState, soundEmitterEditingState, statusText});
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
    audioPicker.Close(soundEditorState.assetPicker);
    BuildNpcEditorService().Shutdown(assets);
    audioPicker.Close(weaponEditorState.audioPicker);
    BuildWeaponEditorService().Shutdown();
    BuildItemEditorService().Shutdown();
    BuildPlayerSettingsService().Shutdown(context);
    BuildMaterialRegistryEditorService().Shutdown(assets);
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
    if (!engine::IsNull(runtimeObjectEditingState.spritePicker.previewScope)) {
        assets.UnloadScope(runtimeObjectEditingState.spritePicker.previewScope);
    }
    state = SectorEditorState{};
    uiState = SectorEditorUiState{};
    runtimeObjectEditingState = RuntimeObjectEditingState{};
    runtimeObjectEditingUiState = RuntimeObjectEditingUiState{};
    surfaceHeightAdjustmentState = PreviewSurfaceHeightAdjustmentState{};
    npcEditorState = SectorEditorNpcEditorState{};
    npcEditorSessionState = SectorEditorNpcEditorSessionState{};
    weaponEditorState = SectorEditorWeaponEditorState{};
    weaponEditorSessionState = SectorEditorWeaponEditorSessionState{};
    itemEditorState = SectorEditorItemEditorState{};
    itemEditorSessionState = SectorEditorItemEditorSessionState{};
    playerSettingsState = SectorEditorPlayerSettingsState{};
    materialRegistryEditorState = SectorEditorMaterialRegistryEditorState{};
    soundEditorState = SectorEditorSoundEditorState{};
    patrolEditorState = SectorEditorPatrolEditorState{};
    audioAssetPickerSessionState = SectorEditorAudioAssetPickerSessionState{};
    textureCatalogState = TextureCatalogState{};
    soundCatalogState = SectorEditorSoundCatalogState{};
    lightEditingState = LightEditingState{};
    materialEditingUiState = MaterialEditingUiState{};
    fogVolumeEditingUiState = FogVolumeEditingUiState{};
    reflectionProbeEditingUiState = ReflectionProbeEditingUiState{};
    levelMarkerEditingState = LevelMarkerEditingState{};
    levelMarkerEditingUiState = LevelMarkerEditingUiState{};
    soundEmitterEditingState = SoundEmitterEditingState{};
    soundEmitterEditingUiState = SoundEmitterEditingUiState{};
    triggerEditingState = TriggerEditingState{};
    triggerEditingUiState = TriggerEditingUiState{};
    authoringFaceMergeState = SectorEditorAuthoringFaceMergeState{};
    fogVolumeEditingService.reset();
    reflectionProbeEditingService.reset();
    authoringFaceMergeService.reset();
    levelMarkerEditingService.reset();
    soundEmitterEditingService.reset();
    triggerEditingService.reset();
    playerAudio = PlayerAudioRuntime{};
    canvasRect = {};
    statusText.clear();
    gameSessionExists = false;
    clearGameSessionRequested = false;
    playerAudioSettingsChanged = false;
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
    if (weaponEditorState.open) {
        SectorEditorWeaponEditorService editor = BuildWeaponEditorService();
        if (editor.ConsumePreviewReloadRequest()) {
            const std::string selectedId = editor.SelectedWeaponId();
            if (!selectedId.empty()) {
                const bool selected = fpsPlayer.SelectWeapon(
                        assets,
                        sceneRuntime.Renderer(),
                        editor.PreviewRegistry(),
                        editor.PreviewApplicationSettings(),
                        selectedId,
                        "fps_weapon_editor_viewmodel");
                if (selected) {
                    EquipFpsViewmodelForWeaponEditing(fpsPlayer.State());
                }
            }
        }
        fpsPlayer.Update(
                assets,
                sceneRuntime.Renderer(),
                editor.PreviewRegistry(),
                editor.PreviewApplicationSettings(),
                dt,
                nullptr,
                nullptr,
                engineContext != nullptr ? &engineContext->audio : nullptr);
        return;
    }
    fpsPlayer.Update(
            assets,
            sceneRuntime.Renderer(),
            weaponRegistry,
            applicationSettings,
            dt,
            nullptr,
            nullptr,
            engineContext != nullptr ? &engineContext->audio : nullptr);
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
            || soundEditorState.open
            || patrolEditorState.open
            || state.footstepPicker.open
            || state.decalTintModal.open
            || state.previewSettingsModal.open
            || npcEditorState.open
            || itemEditorState.open
            || weaponEditorState.open
            || materialRegistryEditorState.open
            || uiState.mainMenu.openRootIndex >= 0
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
            fpsPlayer.State().firing.shotSequence,
            fpsPlayer.State().firing.definition,
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
    ProcessPendingReflectionProbeBake(context);
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
        if (reflectionProbeEditingService) {
            reflectionProbeEditingService->CancelMove(nullptr);
        }
        if (levelMarkerEditingService) {
            levelMarkerEditingService->CancelMove(nullptr);
        }
        if (soundEmitterEditingService) {
            soundEmitterEditingService->CancelMove(nullptr);
        }
        return;
    }

    if (state.mode == SectorEditorMode::Preview3D
            && uiState.mainMenu.openRootIndex >= 0) {
        input.ForEachEvent(
                engine::InputEventType::KeyPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.key.key != KEY_F2) return;
                    if (lightEditingState.proxyPlacement.active
                            || PreviewAdjustmentActive()) {
                        statusText = PreviewAdjustmentActive()
                                ? "Apply or cancel the 3D adjustment before hiding the 3D UI"
                                : "Finish proxy placement before hiding the 3D UI";
                    } else {
                        previewState.overlay.previewUiHidden = true;
                        previewState.selection.hoveredSurface3D = SectorSurfaceHit{};
                        engine::CloseMainMenu(uiState.mainMenu);
                        statusText = "3D UI hidden";
                    }
                    engine::ConsumeEvent(event);
                });
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
                previewState.controller.fpsControllerState.currentSectorId,
                playerObstaclePtr);
        UpdateFpsViewmodel(assets, dt);
        const bool hasBlockingModal = state.texturePicker.open
                || state.soundPicker.open
                || soundEditorState.open
                || patrolEditorState.open
                || npcEditorState.open
                || materialRegistryEditorState.open
                || state.footstepPicker.open
                || runtimeObjectEditingState.spritePicker.open
                || runtimeObjectEditingState.staticModelPicker.open
                || itemEditorState.open
                || weaponEditorState.open
                || uiState.mainMenu.openRootIndex >= 0
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
        UpdatePreview3D(input, assets, dt);
        previewUseTarget = {};
        previewUsePromptTitle = {};
        if (canInteractWithDoors) {
            const SectorViewPose pose = SectorFpsControllerPose(
                    previewState.controller.fpsControllerState,
                    previewState.controller.fpsControllerConfig);
            previewUseTarget = FindSectorUseTarget(
                    context.world,
                    &assets,
                    pose.position,
                    PreviewForwardFromPose(pose),
                    previewState.collision.sectorCollisionWorldValid
                            ? &previewState.collision.sectorCollisionWorld : nullptr,
                    false);
            const std::string_view title = SectorUseTargetTitle(
                    context.world, previewUseTarget);
            if (!title.empty()) {
                std::snprintf(
                        previewUsePromptTitle.data(),
                        previewUsePromptTitle.size(),
                        "%.*s",
                        static_cast<int>(title.size()),
                        title.data());
            }
            input.ForEachEvent(
                    engine::InputEventType::KeyPressed,
                    true,
                    [this, &context](engine::InputEvent& event) {
                        if (event.key.key != KEY_E
                                || previewUseTarget.kind != SectorUseTargetKind::Door
                                || !context.world.IsAlive(previewUseTarget.entity)
                                || !context.world.Has<SectorDoorMotion>(previewUseTarget.entity)) {
                            return;
                        }
                        SectorDoorMotion& motion = context.world.Get<SectorDoorMotion>(
                                previewUseTarget.entity);
                        motion.targetOpenFraction =
                                motion.targetOpenFraction > 0.5f
                                        || motion.openFraction > 0.5f
                                ? 0.0f : 1.0f;
                        engine::ConsumeEvent(event);
                    });
        }
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
            || soundEditorState.open
            || patrolEditorState.open
            || npcEditorState.open
            || itemEditorState.open
            || weaponEditorState.open
            || uiState.mainMenu.openRootIndex >= 0
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

    engine::BeginUI(ui, input);
    const bool mainMenuVisible = state.mode != SectorEditorMode::Preview3D
            || !previewState.overlay.previewUiHidden;
    const SectorEditorConfigTarget configTarget =
            ResolveSectorEditorConfigTarget(
                    state.mode,
                    TopologyMap(),
                    AuthoringGraph(),
                    MakeLiveConstDerivationAccess(documentState.derivation),
                    selectionState,
                    previewState.selection);
    const SectorEditorMainMenuCommand command = DrawSectorEditorMainMenu(
            ui,
            config,
            input,
            assets,
            font,
            smallFont,
            state,
            uiState.mainMenu,
            gameSessionExists,
            configTarget.kind != SectorEditorConfigKind::None,
            configTarget.kind != SectorEditorConfigKind::None
                    && configTarget.kind == configClipboardState.kind,
            [&]() {
                const SectorPlacedRuntimeObject* object =
                        FindSectorPlacedRuntimeObject(
                                TopologyMap(),
                                selectionState.selectedRuntimeObjectId);
                if (object != nullptr
                        && IsSectorEditorPreviewAdjustableObject(*object)) {
                    return true;
                }
                return IsSectorEditorPreviewSurfaceHeightAdjustable(
                        TopologyMap(),
                        AuthoringGraph(),
                        documentState.derivation.authoringDerivation,
                        IsSectorEditorAuthoringDerivationCurrent(
                                MakeLiveConstDerivationAccess(
                                        documentState.derivation)),
                        previewState.selection.selectedSurface3D);
            }(),
            PreviewAdjustmentActive(),
            mainMenuVisible,
            IsMainMenuInteractionEnabled());
    HandleMainMenuCommand(command, ui, assets);

    if (state.mode == SectorEditorMode::Preview3D) {
        if (lightmapBake.IsBlocking()) {
            DrawLightmapBakeModal(ui, config, input, assets, font);
            uiState.keyboardCaptured = true;
            engine::EndUI(ui, config, input, assets);
            return;
        }
        if (state.lightmapBakeSetupModal.open) {
            DrawLightmapBakeSetupModal(ui, config, input, assets, font);
            uiState.keyboardCaptured = true;
            engine::EndUI(ui, config, input, assets);
            return;
        }
        if (state.colorSettingsModal.open) {
            DrawColorSettingsModal(
                    ui, config, input, assets, font, smallFont);
            uiState.keyboardCaptured = true;
            engine::EndUI(ui, config, input, assets);
            return;
        }
        if (playerSettingsState.open) {
            DrawPlayerSettingsModal(
                    ui, config, input, assets, font, smallFont);
            uiState.keyboardCaptured = true;
            engine::EndUI(ui, config, input, assets);
            return;
        }
        if (itemEditorState.open) {
            DrawItemEditor(ui, config, input, assets, font, smallFont);
            uiState.keyboardCaptured = true;
            engine::EndUI(ui, config, input, assets);
            return;
        }
        if (weaponEditorState.open) {
            DrawWeaponEditor(ui, config, input, assets, font, smallFont);
            uiState.keyboardCaptured = true;
            engine::EndUI(ui, config, input, assets);
            return;
        }
        if (materialRegistryEditorState.open) {
            DrawMaterialRegistryEditor(ui, config, input, assets, font, smallFont);
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
        if (patrolEditorState.open) {
            DrawPatrolEditor(ui, config, input, assets, font, smallFont);
            uiState.keyboardCaptured = true;
            engine::EndUI(ui, config, input, assets);
            return;
        }
        if (soundEditorState.open) {
            DrawSoundEditor(ui, config, input, assets, font, smallFont);
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
        uiState.keyboardCaptured = ui.focusedId != 0
                || uiState.mainMenu.openRootIndex >= 0;
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

    if (lightmapBake.IsBlocking()) {
        DrawLightmapBakeModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.lightmapBakeSetupModal.open) {
        DrawLightmapBakeSetupModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.colorSettingsModal.open) {
        DrawColorSettingsModal(
                ui, config, input, assets, font, smallFont);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (playerSettingsState.open) {
        DrawPlayerSettingsModal(
                ui, config, input, assets, font, smallFont);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (itemEditorState.open) {
        DrawItemEditor(ui, config, input, assets, font, smallFont);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (weaponEditorState.open) {
        DrawWeaponEditor(ui, config, input, assets, font, smallFont);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (materialRegistryEditorState.open) {
        DrawMaterialRegistryEditor(ui, config, input, assets, font, smallFont);
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
    if (patrolEditorState.open) {
        DrawPatrolEditor(ui, config, input, assets, font, smallFont);
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
    if (soundEditorState.open) {
        DrawSoundEditor(ui, config, input, assets, font, smallFont);
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
    DrawSoundEditor(ui, config, input, assets, font, smallFont);
    DrawTexturePickerModal(ui, config, input, assets, font, smallFont);
    DrawSoundPickerModal(ui, config, input, font);
    DrawFootstepPickerModal(ui, config, input, assets, font);
    DrawSpritePickerModal(ui, config, input, assets, font);
    DrawStaticModelPickerModal(ui, config, input, assets, font);
    uiState.keyboardCaptured = ui.focusedId != 0
            || uiState.mainMenu.openRootIndex >= 0;
    if (state.texturePicker.open
            || state.soundPicker.open
            || state.footstepPicker.open
            || soundEditorState.open
            || patrolEditorState.open
            || npcEditorState.open
            || itemEditorState.open
            || weaponEditorState.open
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

float SectorEditor::VisibleMainMenuHeight() const
{
    return state.mode == SectorEditorMode::Preview3D
                    && previewState.overlay.previewUiHidden
            ? 0.0f
            : EditorMainMenuHeight;
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
    context.reflectionProbeEditing = reflectionProbeEditingService
            ? &reflectionProbeEditingService.value()
            : nullptr;
    context.levelMarkerEditing = levelMarkerEditingService
            ? &levelMarkerEditingService.value()
            : nullptr;
    context.soundEmitterEditing = soundEmitterEditingService
            ? &soundEmitterEditingService.value()
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
    return BuildSectorEditorWorkspaceLayout().leftPanel;
}

Rectangle SectorEditor::BuildRightPanelRect() const
{
    return BuildSectorEditorWorkspaceLayout().rightPanel;
}

Rectangle SectorEditor::BuildBottomPanelRect() const
{
    return BuildSectorEditorWorkspaceLayout().bottomPanel;
}

Rectangle SectorEditor::BuildCanvasRect() const
{
    return BuildSectorEditorWorkspaceLayout().canvas;
}

bool SectorEditor::IsMainMenuInteractionEnabled() const
{
    return !lightmapBake.IsBlocking()
            && !materialRegistryEditorState.open
            && !soundEditorState.open
            && !patrolEditorState.open
            && !npcEditorState.open
            && !itemEditorState.open
            && !weaponEditorState.open
            && !state.texturePicker.open
            && !state.soundPicker.open
            && !state.footstepPicker.open
            && !runtimeObjectEditingState.spritePicker.open
            && !runtimeObjectEditingState.staticModelPicker.open
            && !HasDocumentModalOpen();
}

void SectorEditor::HandleMainMenuCommand(
        SectorEditorMainMenuCommand command,
        engine::UIContext& ui,
        engine::AssetManager& assets)
{
    if (PreviewAdjustmentActive()
            && command != SectorEditorMainMenuCommand::ApplyPreviewAdjustment
            && command != SectorEditorMainMenuCommand::CancelPreviewAdjustment
            && command != SectorEditorMainMenuCommand::None) {
        statusText = "Apply or cancel the active 3D adjustment first";
        return;
    }
    switch (command) {
        case SectorEditorMainMenuCommand::NewLevel:
            OpenNewConfirmation(assets);
            break;
        case SectorEditorMainMenuCommand::LoadLevel:
            OpenLoadLevelModal();
            break;
        case SectorEditorMainMenuCommand::SaveLevel:
            SaveCurrentLevel();
            break;
        case SectorEditorMainMenuCommand::SaveLevelAs:
            OpenSaveLevelModal();
            break;
        case SectorEditorMainMenuCommand::ReloadLevel:
            OpenReloadConfirmation(assets);
            break;
        case SectorEditorMainMenuCommand::ClearGameSession:
            if (!gameSessionExists) {
                statusText = "No game session is running.";
                break;
            }
            OpenConfirmation(
                    "Clear Game Session",
                    "Discard the current game session and all unsaved game progress?",
                    [this]() { clearGameSessionRequested = true; });
            break;
        case SectorEditorMainMenuCommand::CopyConfig:
            CopySelectedConfig(assets);
            break;
        case SectorEditorMainMenuCommand::PasteConfig:
            PasteSelectedConfig(assets);
            break;
        case SectorEditorMainMenuCommand::BeginPreviewAdjustment:
            BeginPreviewAdjustment();
            break;
        case SectorEditorMainMenuCommand::ApplyPreviewAdjustment:
            ApplyPreviewAdjustment();
            break;
        case SectorEditorMainMenuCommand::CancelPreviewAdjustment:
            CancelPreviewAdjustment("3D adjustment cancelled");
            break;
        case SectorEditorMainMenuCommand::Toggle3DMode:
            if (state.mode == SectorEditorMode::Preview3D) {
                LeavePreview3D();
            } else if (engineContext != nullptr) {
                TryEnterPreview3D(*engineContext, ui);
            }
            break;
        case SectorEditorMainMenuCommand::OpenMaterialEditor:
            BuildMaterialRegistryEditorService().Open();
            break;
        case SectorEditorMainMenuCommand::OpenSoundEditor:
            BuildSoundEditorService().Open();
            break;
        case SectorEditorMainMenuCommand::OpenPatrolEditor:
            BuildPatrolEditorService().Open();
            break;
        case SectorEditorMainMenuCommand::OpenNpcEditor:
            BuildNpcEditorService().Open();
            break;
        case SectorEditorMainMenuCommand::OpenWeaponEditor:
            OpenWeaponEditor(state.mode == SectorEditorMode::Preview3D);
            break;
        case SectorEditorMainMenuCommand::OpenItemEditor:
            BuildItemEditorService().Open();
            break;
        case SectorEditorMainMenuCommand::ToggleShowGrid:
            state.showGrid = !state.showGrid;
            break;
        case SectorEditorMainMenuCommand::ToggleShowAxes:
            state.showAxes = !state.showAxes;
            break;
        case SectorEditorMainMenuCommand::ToggleShowIds:
            state.showSectorIds = !state.showSectorIds;
            break;
        case SectorEditorMainMenuCommand::OpenLevelSettings:
            OpenPreviewSettingsModal();
            break;
        case SectorEditorMainMenuCommand::OpenColorSettings:
            OpenColorSettingsModal();
            break;
        case SectorEditorMainMenuCommand::OpenPlayerSettings:
            if (engineContext != nullptr) {
                BuildPlayerSettingsService().Open(*engineContext);
            }
            break;
        case SectorEditorMainMenuCommand::OpenSneakSettings:
            if (engineContext != nullptr) {
                BuildPlayerSettingsService().Open(
                        *engineContext,
                        SectorEditorPlayerSettingsTab::Sneaking);
            }
            break;
        case SectorEditorMainMenuCommand::None:
            break;
    }
}

bool SectorEditor::ConsumeClearGameSessionRequest()
{
    const bool requested = clearGameSessionRequested;
    clearGameSessionRequested = false;
    return requested;
}

bool SectorEditor::ConsumePlayerAudioSettingsChanged()
{
    const bool changed = playerAudioSettingsChanged;
    playerAudioSettingsChanged = false;
    return changed;
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
                    } else if (selectionState.topologySelectionKind == TopologySelectionKind::StaticRectLight
                            && selectionState.selectedTopologyStaticSpotLightId >= 0) {
                        OpenDeleteSelectedLightConfirmation();
                    } else if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicRectLight
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

    if (ShouldApplySectorEditorKeyboardPan(
                uiState.keyboardCaptured,
                input.IsKeyDown(KEY_LEFT_CONTROL),
                input.IsKeyDown(KEY_RIGHT_CONTROL))) {
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

                if (state.currentTool == SectorEditorTool::StaticRectLight) {
                    const Vector2 mapPoint = SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition));
                    BuildLightEditingService().AddStaticRectLight(FindTopologySectorAt(mapPoint), mapPoint);
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


                if (state.currentTool == SectorEditorTool::DynamicRectLight) {
                    const Vector2 mapPoint = SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition));
                    BuildLightEditingService().AddDynamicRectLight(FindTopologySectorAt(mapPoint), mapPoint);
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

                if (state.currentTool == SectorEditorTool::Item) {
                    AddItemAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
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

                if (state.currentTool == SectorEditorTool::Window) {
                    AddWindowAtPortal(event.mouseClick.releasePosition);
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
    if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicRectLight
            && selectionState.selectedTopologyDynamicSpotLightId >= 0) {
        return {SectorEditorPickKind::DynamicRectLight, selectionState.selectedTopologyDynamicSpotLightId};
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicLight
            && selectionState.selectedTopologyDynamicLightId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::DynamicLight, selectionState.selectedTopologyDynamicLightId};
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::StaticSpotLight
            && selectionState.selectedTopologyStaticSpotLightId >= 0) {
        return SectorEditorPickTarget{SectorEditorPickKind::StaticSpotLight, selectionState.selectedTopologyStaticSpotLightId};
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::StaticRectLight
            && selectionState.selectedTopologyStaticSpotLightId >= 0) {
        return {SectorEditorPickKind::StaticRectLight, selectionState.selectedTopologyStaticSpotLightId};
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
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::ReflectionProbe
            && selectionState.selectedAuthoring.reflectionProbeId >= 0) {
        return {SectorEditorPickKind::AuthoringReflectionProbe,
                selectionState.selectedAuthoring.reflectionProbeId};
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::LevelMarker
            && selectionState.selectedAuthoring.levelMarkerId >= 0) {
        return SectorEditorPickTarget{
                SectorEditorPickKind::LevelMarker,
                selectionState.selectedAuthoring.levelMarkerId};
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::SoundEmitter
            && selectionState.selectedAuthoring.soundEmitterId >= 0) {
        return {SectorEditorPickKind::SoundEmitter,
                selectionState.selectedAuthoring.soundEmitterId};
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
            + TopologyMap().dynamicRectLights.size()
            + TopologyMap().dynamicPointLights.size()
            + TopologyMap().staticSpotLights.size()
            + TopologyMap().staticRectLights.size()
            + TopologyMap().staticLights.size()
            + AuthoringGraph().fogVolumes.size()
            + AuthoringGraph().reflectionProbes.size()
            + AuthoringGraph().levelMarkers.size()
            + AuthoringGraph().soundEmitters.size()
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
    AppendCachedSoundEmitterPickCandidates(
            state.topologyRenderCache, pickContext, screenPoint,
            ScreenLightPickPixels, candidates);
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
    for (const SectorTopologyDynamicRectLight& light : TopologyMap().dynamicRectLights) {
        addSpotCandidate(SectorEditorPickKind::DynamicRectLight, light.id,
                MapToScreen({light.position.x, light.position.z}),
                MapToScreen({light.target.x, light.target.z}));
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
    for (const SectorTopologyStaticRectLight& light : TopologyMap().staticRectLights) {
        addSpotCandidate(SectorEditorPickKind::StaticRectLight, light.id,
                MapToScreen({light.position.x, light.position.z}),
                MapToScreen({light.target.x, light.target.z}));
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
    if (reflectionProbeEditingService) {
        const Vector2 mapPoint = ScreenToMap(screenPoint);
        const Vector2 tolerancePoint = ScreenToMap(
                Vector2{screenPoint.x + ScreenLightPickPixels, screenPoint.y});
        const int probeId = reflectionProbeEditingService->FindAtMapPoint(
                mapPoint, std::fabs(tolerancePoint.x - mapPoint.x));
        if (probeId >= 0) {
            candidates.push_back({
                    {SectorEditorPickKind::AuthoringReflectionProbe, probeId}, 0.0f});
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
    const bool staticRectSelected = selectionState.topologySelectionKind == TopologySelectionKind::StaticRectLight
            && selectionState.selectedTopologyStaticSpotLightId == topologyLightId;
    const bool dynamicRectSelected = selectionState.topologySelectionKind == TopologySelectionKind::DynamicRectLight
            && selectionState.selectedTopologyDynamicSpotLightId == topologyLightId;

    if (staticRectSelected || state.currentTool == SectorEditorTool::StaticRectLight) {
        const auto* light = FindSectorTopologyStaticRectLight(TopologyMap(), topologyLightId);
        if (light == nullptr) return;
        selectionState.topologySelectionKind = TopologySelectionKind::StaticRectLight;
        selectionState.selectedTopologyStaticSpotLightId = topologyLightId;
        auto service = BuildLightEditingService();
        if (!service.BeginLightDrag(TopologySelectionKind::StaticRectLight, topologyLightId, spotHandle)) return;
        lightEditingState.lightDrag = {true, topologyLightId, spotHandle,
                spotHandle == SpotLightHandle::Target ? light->target : light->position};
        return;
    }
    if (dynamicRectSelected || state.currentTool == SectorEditorTool::DynamicRectLight) {
        const auto* light = FindSectorTopologyDynamicRectLight(TopologyMap(), topologyLightId);
        if (light == nullptr) return;
        selectionState.topologySelectionKind = TopologySelectionKind::DynamicRectLight;
        selectionState.selectedTopologyDynamicSpotLightId = topologyLightId;
        auto service = BuildLightEditingService();
        if (!service.BeginLightDrag(TopologySelectionKind::DynamicRectLight, topologyLightId, spotHandle)) return;
        lightEditingState.lightDrag = {true, topologyLightId, spotHandle,
                spotHandle == SpotLightHandle::Target ? light->target : light->position};
        return;
    }

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

void SectorEditor::UpdatePreviewAdjustmentInput(engine::Input& input)
{
    if (!PreviewAdjustmentActive()) return;

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_ENTER
                        || event.key.key == KEY_KP_ENTER) {
                    ApplyPreviewAdjustment();
                } else if (event.key.key == KEY_ESCAPE) {
                    CancelPreviewAdjustment("3D adjustment cancelled");
                } else if (event.key.key == KEY_TAB) {
                    CancelPreviewAdjustment(
                            "3D adjustment cancelled on return to 2D");
                    LeavePreview3D();
                } else {
                    return;
                }
                engine::ConsumeEvent(event);
            });

    const auto handleNudge = [this](engine::InputEvent& event) {
        if (runtimeObjectEditingState.previewAdjustment.active) {
            const PreviewObjectNudgePreset preset =
                    runtimeObjectEditingState.previewAdjustment.preset;
            const float moveStep =
                    SectorEditorPreviewObjectTranslationStepWorld(preset);
            const float yawStep =
                    SectorEditorPreviewObjectYawStepDegrees(preset);
            float dx = 0.0f;
            float dz = 0.0f;
            float dh = 0.0f;
            float dyaw = 0.0f;
            if (event.key.key == KEY_LEFT) dx = -moveStep;
            else if (event.key.key == KEY_RIGHT) dx = moveStep;
            else if (event.key.key == KEY_UP) dz = moveStep;
            else if (event.key.key == KEY_DOWN) dz = -moveStep;
            else if (event.key.key == KEY_PAGE_UP) dh = moveStep;
            else if (event.key.key == KEY_PAGE_DOWN) dh = -moveStep;
            else if (event.key.key == KEY_Q) dyaw = -yawStep;
            else if (event.key.key == KEY_E) dyaw = yawStep;
            else return;

            SectorEditorRuntimeObjectEditingService editing =
                    BuildRuntimeObjectEditingService();
            FinishPreviewObjectAdjustmentResult(
                    editing.PreviewNudge(dx, dz, dh, dyaw));
            engine::ConsumeEvent(event);
            return;
        }

        if (!surfaceHeightAdjustmentState.active
                || (event.key.key != KEY_PAGE_UP
                    && event.key.key != KEY_PAGE_DOWN)) {
            return;
        }
        const float step = SectorEditorPreviewSurfaceHeightStepAuthored(
                surfaceHeightAdjustmentState.preset);
        const float delta = event.key.key == KEY_PAGE_UP ? step : -step;
        SectorEditorSurfaceHeightEditingService editing =
                BuildSurfaceHeightEditingService();
        PreviewSurfaceHeightNudgeCandidate candidate;
        if (editing.BuildPreviewNudge(delta, candidate)) {
            std::string refreshError;
            if (RefreshPreviewSurfaceGeometry(
                        candidate.derivation.topology, &refreshError)) {
                editing.AcceptPreviewNudge(std::move(candidate));
            } else {
                statusText = refreshError.empty()
                        ? "Height nudge failed: preview geometry refresh failed"
                        : refreshError;
            }
        }
        engine::ConsumeEvent(event);
    };
    input.ForEachEvent(
            engine::InputEventType::KeyPressed, true, handleNudge);
    input.ForEachEvent(
            engine::InputEventType::KeyRepeated, true, handleNudge);
}

void SectorEditor::UpdatePreview3D(engine::Input& input, engine::AssetManager& assets, float dt)
{
    UpdatePreviewAdjustmentInput(input);
    bool controlModeToggled = false;
    const bool gameplayWeaponInput = state.mode == SectorEditorMode::Preview3D
            && previewState.controller.previewControlMode
                    == SectorPreviewControlMode::Gameplay;
    const bool weaponInputCaptured = uiState.keyboardCaptured
            || state.texturePicker.open
            || state.soundPicker.open
            || state.decalTintModal.open
            || state.previewSettingsModal.open
            || itemEditorState.open
            || patrolEditorState.open
            || weaponEditorState.open;
    fpsPlayer.HandleWeaponSlotInput(
            input,
            weaponRegistry,
            gameplayWeaponInput,
            weaponInputCaptured);
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
                    if (lightEditingState.proxyPlacement.active
                            || PreviewAdjustmentActive()) {
                        statusText = PreviewAdjustmentActive()
                                ? "Apply or cancel the 3D adjustment before hiding the 3D UI"
                                : "Finish proxy placement before hiding the 3D UI";
                        engine::ConsumeEvent(event);
                        return;
                    }
                    previewState.overlay.previewUiHidden = !previewState.overlay.previewUiHidden;
                    if (previewState.overlay.previewUiHidden) {
                        previewState.selection.hoveredSurface3D = SectorSurfaceHit{};
                        engine::CloseMainMenu(uiState.mainMenu);
                    }
                    statusText = previewState.overlay.previewUiHidden
                            ? "3D UI hidden"
                            : "3D UI shown";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_F3) {
                    if (lightEditingState.lightPilot.active
                            || lightEditingState.proxyPlacement.active
                            || PreviewAdjustmentActive()) {
                        statusText = PreviewAdjustmentActive()
                                ? "Apply or cancel the 3D adjustment before changing 3D control mode"
                                : "Finish light editing before changing 3D control mode";
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
                    if (fpsPlayer.IsWeaponSwitchInProgress()) {
                        if (!uiState.keyboardCaptured) {
                            engine::ConsumeEvent(event);
                        }
                        return;
                    }
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
            if (lightEditingState.lightPilot.active
                    && (lightEditingState.lightPilot.kind == LightPilotKind::StaticRect
                        || lightEditingState.lightPilot.kind == LightPilotKind::DynamicRect)) {
                constexpr float RollSpeedRadians = 90.0f * DEG2RAD;
                if (input.IsKeyDown(KEY_Q)) {
                    previewState.controller.freeflyController.pose.rollRadians -= RollSpeedRadians * dt;
                }
                if (input.IsKeyDown(KEY_E)) {
                    previewState.controller.freeflyController.pose.rollRadians += RollSpeedRadians * dt;
                }
            }
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
                    sceneRuntime.RuntimeObjects().physicalModelColliders,
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
            || lightEditingState.proxyPlacement.active
            || PreviewAdjustmentActive()) {
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
                const auto tryResizeRectLight = [this, &event]() {
                    Vector3 authoredPosition{};
                    Vector3 authoredTarget{};
                    float rollDegrees = 0.0f;
                    float widthAuthored = 0.0f;
                    float heightAuthored = 0.0f;
                    SectorTopologyStaticRectLight* staticLight = nullptr;
                    SectorTopologyDynamicRectLight* dynamicLight = nullptr;
                    if (selectionState.topologySelectionKind == TopologySelectionKind::StaticRectLight) {
                        staticLight = FindSectorTopologyStaticRectLight(
                                TopologyMap(), selectionState.selectedTopologyStaticSpotLightId);
                        if (staticLight != nullptr) {
                            authoredPosition = staticLight->position; authoredTarget = staticLight->target;
                            rollDegrees = staticLight->rollDegrees;
                            widthAuthored = staticLight->width; heightAuthored = staticLight->height;
                        }
                    } else if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicRectLight) {
                        dynamicLight = FindSectorTopologyDynamicRectLight(
                                TopologyMap(), selectionState.selectedTopologyDynamicSpotLightId);
                        if (dynamicLight != nullptr) {
                            authoredPosition = dynamicLight->position; authoredTarget = dynamicLight->target;
                            rollDegrees = dynamicLight->rollDegrees;
                            widthAuthored = dynamicLight->width; heightAuthored = dynamicLight->height;
                        }
                    }
                    if (staticLight == nullptr && dynamicLight == nullptr) return false;
                    const Vector3 origin = SectorAuthoringToWorldPosition(authoredPosition);
                    const SectorRectLightBasis basis = BuildSectorRectLightBasis(
                            origin, SectorAuthoringToWorldPosition(authoredTarget), rollDegrees);
                    const float halfWidth = SectorAuthoringToWorldDistance(widthAuthored) * 0.5f;
                    const float halfHeight = SectorAuthoringToWorldDistance(heightAuthored) * 0.5f;
                    const Camera3D& camera = sceneRuntime.Renderer().RenderCamera();
                    const Vector3 handleWorld[4] = {
                            Vector3Add(origin, Vector3Scale(basis.right, halfWidth)),
                            Vector3Subtract(origin, Vector3Scale(basis.right, halfWidth)),
                            Vector3Add(origin, Vector3Scale(basis.up, halfHeight)),
                            Vector3Subtract(origin, Vector3Scale(basis.up, halfHeight))};
                    int handle = -1;
                    float best = 12.0f * 12.0f;
                    for (int index = 0; index < 4; ++index) {
                        const Vector2 screen = GetWorldToScreenEx(
                                handleWorld[index], camera, EditorWidth, EditorHeight);
                        const float distance = Vector2DistanceSqr(screen, event.mouseClick.pressPosition);
                        if (distance <= best) { best = distance; handle = index; }
                    }
                    if (handle < 0) return false;
                    const Ray ray = GetScreenToWorldRayEx(
                            event.mouseClick.releasePosition, camera, EditorWidth, EditorHeight);
                    const Vector3 axis = handle < 2 ? basis.right : basis.up;
                    const Vector3 offset = Vector3Subtract(ray.position, origin);
                    const float b = Vector3DotProduct(ray.direction, axis);
                    const float d = Vector3DotProduct(ray.direction, offset);
                    const float e = Vector3DotProduct(axis, offset);
                    const float denominator = 1.0f - b * b;
                    if (std::fabs(denominator) <= 0.00001f) return true;
                    const float alongAxis = (e - b * d) / denominator;
                    const float sizeWorld = std::clamp(std::fabs(alongAxis) * 2.0f, 0.05f, 64.0f);
                    const float sizeAuthored = SectorWorldToAuthoringDistance(sizeWorld);
                    auto editing = BuildLightEditingService();
                    const bool changed = handle < 2
                            ? (staticLight != nullptr
                                    ? editing.SetStaticRectLightWidth(*staticLight, sizeAuthored)
                                    : editing.SetDynamicRectLightWidth(*dynamicLight, sizeAuthored))
                            : (staticLight != nullptr
                                    ? editing.SetStaticRectLightHeight(*staticLight, sizeAuthored)
                                    : editing.SetDynamicRectLightHeight(*dynamicLight, sizeAuthored));
                    if (changed && sceneRuntime.Renderer().IsRendererReady()) {
                        sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
                    }
                    statusText = TextFormat("Resized %s rect light %d",
                            staticLight != nullptr ? "static" : "dynamic",
                            staticLight != nullptr ? staticLight->id : dynamicLight->id);
                    return true;
                };
                if (tryResizeRectLight()) {
                    engine::ConsumeEvent(event);
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
    context.reflectionProbeEditing = reflectionProbeEditingService
            ? &reflectionProbeEditingService.value()
            : nullptr;
    context.levelMarkerEditing = levelMarkerEditingService
            ? &levelMarkerEditingService.value()
            : nullptr;
    context.soundEmitterEditing = soundEmitterEditingService
            ? &soundEmitterEditingService.value()
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

void SectorEditor::RefreshResolvedMaterials()
{
    const std::vector<std::string> missing =
            ResolveSectorMaterialsForMap(TopologyMap(), materialRegistry);
    for (const std::string& id : missing) {
        TraceLog(LOG_WARNING, "Editor map references missing global material: %s", id.c_str());
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
    const auto* staticRectLight = selectionState.topologySelectionKind == TopologySelectionKind::StaticRectLight
            ? FindSectorTopologyStaticRectLight(TopologyMap(), selectionState.selectedTopologyStaticSpotLightId) : nullptr;
    const auto* dynamicRectLight = selectionState.topologySelectionKind == TopologySelectionKind::DynamicRectLight
            ? FindSectorTopologyDynamicRectLight(TopologyMap(), selectionState.selectedTopologyDynamicSpotLightId) : nullptr;
    if (light == nullptr && staticSpotLight == nullptr && dynamicLight == nullptr
            && dynamicSpotLight == nullptr && staticRectLight == nullptr && dynamicRectLight == nullptr) {
        return false;
    }

    const int lightId = light != nullptr
            ? light->id
            : (staticSpotLight != nullptr
                    ? staticSpotLight->id
                    : (dynamicLight != nullptr ? dynamicLight->id
                            : (dynamicSpotLight != nullptr ? dynamicSpotLight->id
                                    : (staticRectLight != nullptr ? staticRectLight->id : dynamicRectLight->id))));
    OpenConfirmation(
            "Delete Light",
            light != nullptr
                    ? TextFormat("Delete static light %d?", lightId)
                    : (staticSpotLight != nullptr
                            ? TextFormat("Delete static spot %d?", lightId)
                            : (dynamicLight != nullptr
                                    ? TextFormat("Delete dynamic light %d?", lightId)
                                    : (dynamicSpotLight != nullptr
                                            ? TextFormat("Delete dynamic spot %d?", lightId)
                                            : (staticRectLight != nullptr
                                                    ? TextFormat("Delete static rect light %d?", lightId)
                                                    : TextFormat("Delete dynamic rect light %d?", lightId))))),
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

bool SectorEditor::ConvertSelectedLight()
{
    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    const SectorEditorLightMutationResult result = lightEditing.ConvertSelectedLight();
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
    return result.changed;
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
                    MakeLiveConstDerivationAccess(documentState.derivation)),
            &itemRegistry}};
}

SectorEditorSurfaceHeightEditingService
SectorEditor::BuildSurfaceHeightEditingService()
{
    return SectorEditorSurfaceHeightEditingService{
        SectorEditorSurfaceHeightEditingServiceContext{
            state,
            Lifecycle(),
            TopologyMap(),
            AuthoringGraph(),
            MakeLiveDerivationAccess(documentState.derivation),
            selectionState,
            previewState.selection,
            surfaceHeightAdjustmentState,
            statusText}};
}

bool SectorEditor::PreviewAdjustmentActive() const
{
    return runtimeObjectEditingState.previewAdjustment.active
            || surfaceHeightAdjustmentState.active;
}

bool SectorEditor::BeginPreviewAdjustment()
{
    if (state.mode != SectorEditorMode::Preview3D
            || !sceneRuntime.Renderer().IsRendererReady()) {
        statusText = "Enter 3D mode before adjusting the selection";
        return false;
    }
    if (lightEditingState.lightPilot.active
            || lightEditingState.proxyPlacement.active) {
        statusText = "Finish light editing before adjusting the selection";
        return false;
    }
    if (PreviewAdjustmentActive()) {
        statusText = "Apply or cancel the active 3D adjustment first";
        return false;
    }
    previewState.selection.hoveredSurface3D = SectorSurfaceHit{};
    const SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(
            TopologyMap(), selectionState.selectedRuntimeObjectId);
    if (object != nullptr && IsSectorEditorPreviewAdjustableObject(*object)) {
        return BuildRuntimeObjectEditingService().BeginPreviewAdjustment();
    }
    return BuildSurfaceHeightEditingService().BeginPreviewAdjustment();
}

void SectorEditor::FinishPreviewObjectAdjustmentResult(
        const SectorEditorPreviewObjectAdjustmentResult& result)
{
    if (result.bakedStatusRefreshNeeded
            && sceneRuntime.Renderer().IsRendererReady()) {
        if (result.restoreBakedStatus) {
            sceneRuntime.Renderer().FinishStaticObjectAdjustmentBakedData(true);
        } else {
            sceneRuntime.Renderer().BeginStaticObjectAdjustmentBakedDataStale();
        }
    }
    if (result.commitBakedStatus
            && sceneRuntime.Renderer().IsRendererReady()) {
        sceneRuntime.Renderer().FinishStaticObjectAdjustmentBakedData(false);
    }
    if (result.staticNavigationRebuildNeeded) {
        sceneRuntime.Navigation().RequestRebuild();
    }
}

bool SectorEditor::ApplyPreviewAdjustment()
{
    if (runtimeObjectEditingState.previewAdjustment.active) {
        SectorEditorRuntimeObjectEditingService editing =
                BuildRuntimeObjectEditingService();
        const SectorEditorPreviewObjectAdjustmentResult result =
                editing.ApplyPreviewAdjustment();
        FinishPreviewObjectAdjustmentResult(result);
        return result.changed;
    }
    if (!surfaceHeightAdjustmentState.active) return false;

    const PreviewSurfaceHeightAdjustmentResult result =
            BuildSurfaceHeightEditingService().ApplyPreviewAdjustment();
    if (!result.committed) return result.changed;

    if (engineContext != nullptr) {
        std::string refreshError;
        if (!sceneRuntime.RefreshTopologyRuntimeData(
                    *engineContext, TopologyMap(), refreshError)) {
            TraceLog(
                    LOG_WARNING,
                    "3D height adjustment runtime refresh warning: %s",
                    refreshError.empty() ? "unknown error" : refreshError.c_str());
        }
        RebuildSectorCollisionWorld();
        RefreshPreviewObjectProbeDebugData();
    }
    return result.changed;
}

bool SectorEditor::CancelPreviewAdjustment(const char* message)
{
    if (runtimeObjectEditingState.previewAdjustment.active) {
        SectorEditorRuntimeObjectEditingService editing =
                BuildRuntimeObjectEditingService();
        const SectorEditorPreviewObjectAdjustmentResult result =
                editing.CancelPreviewAdjustment(message);
        FinishPreviewObjectAdjustmentResult(result);
        return result.changed;
    }
    if (!surfaceHeightAdjustmentState.active) return false;

    const bool changed = surfaceHeightAdjustmentState.changed;
    std::string refreshError;
    const bool restored = !changed
            || RefreshPreviewSurfaceGeometry(TopologyMap(), &refreshError);
    const PreviewSurfaceHeightAdjustmentResult result =
            BuildSurfaceHeightEditingService().CancelPreviewAdjustment(message);
    if (!restored && engineContext != nullptr) {
        TraceLog(
                LOG_WARNING,
                "Incremental height-adjustment restore failed; rebuilding preview: %s",
                refreshError.empty() ? "unknown error" : refreshError.c_str());
        RebuildPreviewMeshesPreservingView(*engineContext);
    }
    return result.changed;
}

SectorEditorSoundService SectorEditor::BuildSoundService(
        SectorEditorRuntimeObjectEditingService* runtimeObjectEditing,
        SectorEditorSoundEmitterEditingService* soundEmitterEditing)
{
    return SectorEditorSoundService{
            SectorEditorSoundServiceContext{
                    state,
                    AuthoringGraph(),
                    TopologyMap(),
                    soundCatalogState,
                    audioAssetPickerSessionState,
                    statusText,
                    *engineContext,
                    runtimeObjectEditing,
                    soundEmitterEditing}};
}

SectorEditorSoundEditorService SectorEditor::BuildSoundEditorService()
{
    return SectorEditorSoundEditorService{
            soundEditorState,
            AuthoringGraph(),
            TopologyMap(),
            MakeLiveDerivationAccess(documentState.derivation),
            Lifecycle(),
            statusText};
}

SectorEditorPatrolEditorService SectorEditor::BuildPatrolEditorService()
{
    return SectorEditorPatrolEditorService{
            patrolEditorState,
            AuthoringGraph(),
            TopologyMap(),
            MakeLiveDerivationAccess(documentState.derivation),
            Lifecycle(),
            state.topologyRenderRevision,
            state.topologyRenderCache,
            statusText};
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

void SectorEditor::AddItemAt(Vector2 mapPoint)
{
    EnsureTopologyRenderCache();
    const std::string definitionId =
            runtimeObjectEditingState.itemPlacement.lastDefinitionId;
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    editing.AddItem(mapPoint, definitionId);
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

void SectorEditor::AddWindowAtPortal(Vector2 screenPoint)
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
        statusText = "Window placement failed: click a two-sided portal";
        return;
    }
    SectorEditorSelectionServiceContext selection = BuildSelectionServiceContext();
    SectorEditorRuntimeObjectEditingService editing =
            BuildRuntimeObjectEditingService(&selection);
    editing.AddWindow(lineDefId);
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

bool SectorEditor::OpenLightmapBakeSetup()
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
        statusText = gateMessage.empty()
                ? "Bake failed: derived topology is not current"
                : gateMessage;
        return false;
    }
    if (TopologyMap().sectors.empty()) {
        statusText = "Bake failed: no sectors";
        return false;
    }
    if (engineContext == nullptr) {
        statusText = "Bake failed: asset manager is unavailable";
        return false;
    }

    OpenSectorEditorLightmapBakeSetupModal(
            state.lightmapBakeSetupModal,
            TopologyMap().lightmapSettings.qualityPreset);
    return true;
}

bool SectorEditor::StartLightmapBake(
        SectorLightmapBakeQualityPreset qualityPreset)
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

    RefreshResolvedMaterials();

    if (engineContext == nullptr) {
        statusText = "Bake failed: asset manager is unavailable";
        return false;
    }

    const SectorLightmapBakeQualityPreset previousQuality =
            NormalizeSectorLightmapBakeQualityPreset(
                    TopologyMap().lightmapSettings.qualityPreset);
    const SectorLightmapBakeQualityPreset selectedQuality =
            NormalizeSectorLightmapBakeQualityPreset(qualityPreset);
    TopologyMap().lightmapSettings.qualityPreset = selectedQuality;
    SectorStaticModelLightmapData preparedStaticModels;
    std::string staticModelError;
    if (!PrepareSectorStaticModelsForLightmapBake(
                TopologyMap(),
                engineContext->assets,
                {},
                preparedStaticModels,
                staticModelError)) {
        TopologyMap().lightmapSettings.qualityPreset = previousQuality;
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
        TopologyMap().lightmapSettings.qualityPreset = previousQuality;
        statusText = startStatus.empty() ? "Lightmap bake already running" : startStatus;
        return false;
    }

    if (selectedQuality != previousQuality) {
        Lifecycle().hasUnsavedChanges = true;
        Lifecycle().topologyDocumentDirty = true;
        state.lightmapSourceHashRevision = 0;
        if (state.mode == SectorEditorMode::Preview3D
                && sceneRuntime.Renderer().IsRendererReady()) {
            RebuildPreviewMeshesPreservingView(*engineContext);
        }
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
    // Reflection captures contain the baked lighting and must be explicitly
    // regenerated after any lightmap install.
    TopologyMap().bakedReflectionProbes = {};
    documentState.derivation.authoringDerivation.topology.bakedReflectionProbes = {};
    if (documentState.derivation.lastValidAuthoringDerivedTopology.has_value()) {
        documentState.derivation.lastValidAuthoringDerivedTopology
                ->bakedReflectionProbes = {};
    }
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

void SectorEditor::ProcessPendingReflectionProbeBake(engine::EngineContext& context)
{
    if (!reflectionProbeBakePending) return;
    const int selectedProbeId = reflectionProbeBakeSelectedId;
    reflectionProbeBakePending = false;
    reflectionProbeBakeSelectedId = -1;
    BakeReflectionProbes(context, selectedProbeId);
}

bool SectorEditor::BakeReflectionProbes(
        engine::EngineContext& context,
        int selectedProbeId)
{
    if (state.mode != SectorEditorMode::Preview3D
            || !sceneRuntime.Renderer().IsRendererReady()) {
        statusText = "Enter 3D preview and use the Probes tab to bake reflection probes";
        return false;
    }
    if (Lifecycle().currentLevelName.empty()) {
        statusText = "Save the level before baking reflection probes";
        return false;
    }
    if (GetSectorLightmapStatus(TopologyMap()) != SectorLightmapStatus::Valid) {
        statusText = "Bake current static lightmaps before reflection probes";
        return false;
    }
    LevelPaths paths;
    std::string error;
    if (!BuildLevelPaths(Lifecycle().currentLevelName, paths, error)
            || !EnsureSaveLevelDirectory(paths, error)) {
        statusText = "Reflection bake failed: " + error;
        return false;
    }

    SectorBakedReflectionProbeArtifact artifact;
    artifact.version = SectorReflectionProbeBakeVersion;
    if (selectedProbeId > 0
            && std::filesystem::exists(paths.reflectionProbeFilePath)) {
        std::string ignored;
        ReadSectorReflectionProbeArtifact(
                paths.reflectionProbeFilePath, artifact, ignored);
        artifact.version = SectorReflectionProbeBakeVersion;
    }
    if (selectedProbeId <= 0) artifact.probes.clear();

    std::vector<const SectorCompiledReflectionProbe*> targets;
    for (const SectorCompiledReflectionProbe& probe : TopologyMap().compiledReflectionProbes) {
        if (!probe.enabled) continue;
        if (selectedProbeId <= 0 || probe.sourceAuthoringProbeId == selectedProbeId) {
            targets.push_back(&probe);
        }
    }
    if (targets.empty()) {
        statusText = selectedProbeId > 0
                ? "Selected reflection probe is disabled or unresolved"
                : "No enabled reflection probes to bake";
        return false;
    }

    const SectorRuntimeDoorLightingContext doorLighting{
            &sceneRuntime.RuntimeObjects().objectLightProbes,
            &TopologyMap(),
            sceneRuntime.RuntimeObjects().staticLightingRevision};
    const auto started = std::chrono::steady_clock::now();
    int bakedCount = 0;
    for (const SectorCompiledReflectionProbe* probe : targets) {
        statusText = TextFormat("Baking reflection probe %d...",
                probe->sourceAuthoringProbeId);
        std::vector<Vector4> capturedFaces;
        if (!sceneRuntime.Renderer().CaptureReflectionProbe(
                    context.assets,
                    probe->capturePositionWorld,
                    probe->resolution,
                    &context.world,
                    doorLighting,
                    capturedFaces,
                    error)) {
            statusText = "Reflection bake failed: " + error;
            return false;
        }
        SectorBakedReflectionProbeRecord record;
        if (!BuildSectorReflectionProbeRecord(
                    probe->sourceAuthoringProbeId,
                    probe->resolution,
                    ComputeSectorReflectionProbeSourceHash(TopologyMap(), *probe),
                    capturedFaces,
                    record,
                    error)) {
            statusText = "Reflection prefilter failed: " + error;
            return false;
        }
        auto existing = std::find_if(artifact.probes.begin(), artifact.probes.end(),
                [probe](const SectorBakedReflectionProbeRecord& value) {
                    return value.probeId == probe->sourceAuthoringProbeId;
                });
        if (existing == artifact.probes.end()) artifact.probes.push_back(std::move(record));
        else *existing = std::move(record);
        ++bakedCount;
    }
    artifact.probes.erase(
            std::remove_if(artifact.probes.begin(), artifact.probes.end(),
                    [this](const SectorBakedReflectionProbeRecord& record) {
                        return std::none_of(
                                TopologyMap().compiledReflectionProbes.begin(),
                                TopologyMap().compiledReflectionProbes.end(),
                                [&record](const SectorCompiledReflectionProbe& probe) {
                                    return probe.sourceAuthoringProbeId == record.probeId;
                                });
                    }),
            artifact.probes.end());
    std::sort(artifact.probes.begin(), artifact.probes.end(),
            [](const auto& a, const auto& b) { return a.probeId < b.probeId; });
    if (!WriteSectorReflectionProbeArtifact(
                paths.reflectionProbeFilePath, artifact, error)) {
        statusText = "Reflection bake failed: " + error;
        return false;
    }

    const SectorBakedReflectionProbeMetadata metadata{
            paths.reflectionProbeAssetPath,
            SectorReflectionProbeBakeVersion,
            static_cast<int>(artifact.probes.size()),
            "rgba16f-cubemap-mips"};
    TopologyMap().bakedReflectionProbes = metadata;
    documentState.derivation.authoringDerivation.topology.bakedReflectionProbes = metadata;
    if (documentState.derivation.lastValidAuthoringDerivedTopology.has_value()) {
        documentState.derivation.lastValidAuthoringDerivedTopology
                ->bakedReflectionProbes = metadata;
    }
    Lifecycle().hasUnsavedChanges = true;
    Lifecycle().topologyDocumentDirty = true;
    const float seconds = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - started).count();
    statusText = TextFormat("Baked %d reflection probe%s in %.1fs",
            bakedCount, bakedCount == 1 ? "" : "s", seconds);
    RebuildPreviewMeshesPreservingView(context);
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
        DrawPreviewObjectAdjustmentGizmo();
        DrawPreviewSpotLightOverlay();
        DrawPreviewObjectProbeOverlay();
        DrawPreviewReflectionProbeOverlay();
        DrawSectorEditorPreviewNavigationOverlay(
                previewState.overlay,
                sceneRuntime.Navigation(),
                sceneRuntime.NpcNavigation(),
                selectionState.selectedRuntimeObjectId,
                sceneRuntime.Renderer());
    }
}

void SectorEditor::RenderPreview3DHud(
        engine::AssetManager& assets,
        engine::FontHandle usePromptFont,
        Rectangle playableViewport) const
{
    if (state.mode == SectorEditorMode::Preview3D) {
        fpsPlayer.RenderHud(
                playableViewport,
                weaponEditorState.open
                        ? weaponEditorState.draftRegistry
                        : weaponRegistry,
                assets.GetFont(usePromptFont));
        DrawSectorUsePrompt(
                playableViewport,
                assets.GetFont(usePromptFont),
                previewUsePromptTitle.data());
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

void SectorEditor::DrawPreviewObjectAdjustmentGizmo() const
{
    const PreviewObjectAdjustmentState& adjustment =
            runtimeObjectEditingState.previewAdjustment;
    if (!adjustment.active || engineContext == nullptr
            || !sceneRuntime.Renderer().IsRendererReady()) {
        return;
    }
    const auto entry = std::find_if(
            sceneRuntime.RuntimeObjects().placedObjectEntities.begin(),
            sceneRuntime.RuntimeObjects().placedObjectEntities.end(),
            [&adjustment](const SectorPlacedRuntimeObjectEntity& value) {
                return value.placedObjectId == adjustment.objectId;
            });
    if (entry == sceneRuntime.RuntimeObjects().placedObjectEntities.end()
            || !engineContext->world.IsAlive(entry->entity)
            || !engineContext->world.Has<SectorObjectTransform>(entry->entity)) {
        return;
    }

    const SectorObjectTransform& transform =
            engineContext->world.Get<SectorObjectTransform>(entry->entity);
    const engine::ModelAsset* asset = nullptr;
    float scale = 1.0f;
    if (engineContext->world.Has<SectorStaticModel>(entry->entity)) {
        const SectorStaticModel& model =
                engineContext->world.Get<SectorStaticModel>(entry->entity);
        asset = engineContext->assets.GetModelAsset(model.model);
        scale = model.scale;
    } else if (engineContext->world.Has<SectorDynamicModel>(entry->entity)
            && engineContext->world.Has<engine::AnimatedModelInstance>(
                    entry->entity)) {
        const SectorDynamicModel& model =
                engineContext->world.Get<SectorDynamicModel>(entry->entity);
        const engine::AnimatedModelInstance& instance =
                engineContext->world.Get<engine::AnimatedModelInstance>(
                        entry->entity);
        asset = engineContext->assets.GetModelAsset(instance.model);
        scale = model.scale;
    } else if (engineContext->world.Has<SectorItem>(entry->entity)) {
        const SectorItem& item =
                engineContext->world.Get<SectorItem>(entry->entity);
        asset = engineContext->assets.GetModelAsset(item.model);
        scale = item.scale * item.presentation.scaleMultiplier;
    }

    const Color xColor = engine::SrgbColorBytesToLinearSceneUnorm(
            Color{246, 92, 92, 255});
    const Color yColor = engine::SrgbColorBytesToLinearSceneUnorm(
            Color{108, 232, 126, 255});
    const Color zColor = engine::SrgbColorBytesToLinearSceneUnorm(
            Color{84, 170, 255, 255});
    const Color boundsColor = engine::SrgbColorBytesToLinearSceneUnorm(
            Color{255, 229, 112, 230});
    float axisLength = 0.5f;

    BeginMode3D(sceneRuntime.Renderer().RenderCamera());
    if (asset != nullptr && asset->hasLocalBounds) {
        const BoundingBox bounds = asset->hasAnimatedLocalBounds
                ? asset->animatedLocalBounds
                : asset->localBounds;
        const Matrix authored = BuildSectorStaticModelAuthoredTransform(
                transform.position,
                transform.rotationXRadians,
                transform.yawRadians,
                transform.rotationZRadians,
                scale);
        Vector3 corners[8];
        for (int index = 0; index < 8; ++index) {
            const Vector3 local{
                    (index & 1) != 0 ? bounds.max.x : bounds.min.x,
                    (index & 2) != 0 ? bounds.max.y : bounds.min.y,
                    (index & 4) != 0 ? bounds.max.z : bounds.min.z};
            corners[index] = Vector3Transform(local, authored);
        }
        for (int index = 0; index < 8; ++index) {
            for (int bit : {1, 2, 4}) {
                const int neighbor = index ^ bit;
                if (index < neighbor) {
                    DrawLine3D(corners[index], corners[neighbor], boundsColor);
                }
            }
        }
        axisLength = std::clamp(
                Vector3Distance(bounds.min, bounds.max) * scale * 0.35f,
                0.3f,
                1.5f);
    }

    const Vector3 origin = transform.position;
    DrawLine3D(origin, Vector3Add(origin, Vector3{axisLength, 0.0f, 0.0f}), xColor);
    DrawLine3D(origin, Vector3Add(origin, Vector3{0.0f, axisLength, 0.0f}), yColor);
    DrawLine3D(origin, Vector3Add(origin, Vector3{0.0f, 0.0f, axisLength}), zColor);
    const Vector3 yawDirection{
            std::cos(transform.yawRadians), 0.0f,
            std::sin(transform.yawRadians)};
    DrawLine3D(
            origin,
            Vector3Add(origin, Vector3Scale(yawDirection, axisLength * 1.4f)),
            boundsColor);
    Vector3 previous = Vector3Add(origin, Vector3{axisLength * 0.7f, 0.0f, 0.0f});
    for (int segment = 1; segment <= 24; ++segment) {
        const float angle = static_cast<float>(segment) * 2.0f * PI / 24.0f;
        const Vector3 current = Vector3Add(
                origin,
                Vector3{std::cos(angle) * axisLength * 0.7f,
                        0.0f,
                        std::sin(angle) * axisLength * 0.7f});
        DrawLine3D(previous, current, boundsColor);
        previous = current;
    }
    EndMode3D();
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

void SectorEditor::DrawPreviewReflectionProbeOverlay() const
{
    DrawSectorEditorPreviewReflectionProbeOverlay(
            TopologyMap(),
            previewState,
            selectionState,
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
            runtimeObjectEditingState,
            surfaceHeightAdjustmentState,
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
    if (result.openWeaponEditor) {
        OpenWeaponEditor(true);
    }
    if (result.requestNavigationRebuild) {
        statusText = engineContext != nullptr
                && sceneRuntime.RebuildNavigationForMap(
                        *engineContext, TopologyMap())
                ? "Navigation rebuild queued"
                : "Navigation rebuild failed to initialize";
    }
    if (result.requestBakeSelectedReflectionProbe) {
        reflectionProbeBakeSelectedId =
                selectionState.selectedAuthoring.kind
                                == SectorAuthoringSelectionKind::ReflectionProbe
                        ? selectionState.selectedAuthoring.reflectionProbeId
                        : -1;
        reflectionProbeBakePending = reflectionProbeBakeSelectedId > 0;
    }
    if (result.requestBakeAllReflectionProbes) {
        reflectionProbeBakeSelectedId = -1;
        reflectionProbeBakePending = true;
    }
    if (result.requestApplyAdjustment) {
        ApplyPreviewAdjustment();
    }
    if (result.requestCancelAdjustment) {
        CancelPreviewAdjustment("3D adjustment cancelled");
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
                runtimeObjects.swingDoorCatalogRevision,
                itemRegistry.revision)) {
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
                runtimeObjects.swingDoorCatalogRevision,
                &itemRegistry,
                itemRegistry.revision);
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
    DrawAuthoringReflectionProbes();

    if (drawLegacyTopologySelection) {
        DrawTopologySelectedLineHighlight();
    }
    DrawCachedTopologyLineDefs(state.topologyRenderCache, drawContext);
    DrawCachedTopologyVertices(state.topologyRenderCache, drawContext);
    DrawCachedAuthoringGraphOverlay(state.topologyRenderCache, drawContext);
    DrawCachedAuthoringDiagnostics(state.topologyRenderCache, drawContext);
    DrawAuthoringVertexMoveOverlay();
    DrawAuthoringFogVolumeMoveOverlay();
    DrawAuthoringReflectionProbeMoveOverlay();
    DrawCachedTopologyStaticLights(state.topologyRenderCache, drawContext);
    DrawCachedTopologyStaticSpotLights(state.topologyRenderCache, drawContext);
    DrawCachedTopologyDynamicLights(state.topologyRenderCache, drawContext);
    DrawCachedTopologyDynamicSpotLights(state.topologyRenderCache, drawContext);
    const auto drawRectLight = [this](
            Vector3 position,
            Vector3 target,
            Color color,
            bool selected,
            const char* label) {
        const Vector2 origin = MapToScreen({position.x, position.z});
        const Vector2 aim = MapToScreen({target.x, target.z});
        const Color drawColor = selected ? YELLOW : color;
        DrawLineEx(origin, aim, selected ? 2.0f : 1.0f, Fade(drawColor, 0.8f));
        DrawRectangleLinesEx(Rectangle{origin.x - 7.0f, origin.y - 4.0f, 14.0f, 8.0f},
                selected ? 2.0f : 1.0f, drawColor);
        DrawCircleV(aim, 3.0f, drawColor);
        DrawText(
                label,
                static_cast<int>(origin.x + 12.0f),
                static_cast<int>(origin.y - 22.0f),
                18,
                Color{92, 255, 176, 255});
    };
    for (const auto& light : TopologyMap().staticRectLights) {
        drawRectLight(light.position, light.target, SKYBLUE,
                selectionState.topologySelectionKind == TopologySelectionKind::StaticRectLight
                        && selectionState.selectedTopologyStaticSpotLightId == light.id,
                "SR");
    }
    for (const auto& light : TopologyMap().dynamicRectLights) {
        drawRectLight(light.position, light.target, ORANGE,
                selectionState.topologySelectionKind == TopologySelectionKind::DynamicRectLight
                        && selectionState.selectedTopologyDynamicSpotLightId == light.id,
                "DR");
    }
    DrawCachedRuntimeObjects(state.topologyRenderCache, drawContext);
    DrawCachedLevelMarkers(
            state.topologyRenderCache,
            drawContext,
            levelMarkerEditingService ? &levelMarkerEditingService->Drag() : nullptr);
    DrawCachedSoundEmitters(
            state.topologyRenderCache,
            drawContext,
            soundEmitterEditingService ? &soundEmitterEditingService->Drag() : nullptr);
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
    drawToolOverlay(SectorEditorTool::ReflectionProbe);
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

void SectorEditor::DrawAuthoringReflectionProbes() const
{
    const auto drawProbe = [&](const SectorAuthoringReflectionProbe& source,
                               SectorTopologyCoordPoint capturePoint,
                               bool selected,
                               bool hovered,
                               bool resolved) {
        const SectorAuthoringReflectionProbe probe =
                NormalizeSectorAuthoringReflectionProbe(source);
        const Vector2 capture = MapToScreen({
                SectorCoordToVisibleAuthoring(capturePoint.x),
                SectorCoordToVisibleAuthoring(capturePoint.y)});
        const Vector2 influenceMap{
                SectorCoordToVisibleAuthoring(capturePoint.x)
                        + SectorWorldToAuthoringDistance(probe.influenceOffsetWorld.x),
                SectorCoordToVisibleAuthoring(capturePoint.y)
                        + SectorWorldToAuthoringDistance(probe.influenceOffsetWorld.z)};
        const float hx = SectorWorldToAuthoringDistance(probe.halfExtentsWorld.x);
        const float hz = SectorWorldToAuthoringDistance(probe.halfExtentsWorld.z);
        const float cosine = std::cos(probe.yawDegrees * DEG2RAD);
        const float sine = std::sin(probe.yawDegrees * DEG2RAD);
        std::array<Vector2, 4> corners{};
        const std::array<Vector2, 4> local{{{-hx, -hz}, {hx, -hz}, {hx, hz}, {-hx, hz}}};
        for (std::size_t i = 0; i < corners.size(); ++i) {
            corners[i] = MapToScreen({
                    influenceMap.x + cosine * local[i].x - sine * local[i].y,
                    influenceMap.y + sine * local[i].x + cosine * local[i].y});
        }
        const Color color = !resolved ? Color{240, 82, 82, 245}
                : selected ? Color{220, 145, 255, 255}
                : hovered ? Color{244, 192, 70, 255}
                : probe.enabled ? Color{173, 112, 230, 230}
                                : Color{130, 130, 140, 210};
        for (std::size_t i = 0; i < corners.size(); ++i) {
            DrawLineEx(corners[i], corners[(i + 1) % corners.size()],
                    selected ? 2.5f : 1.5f, color);
        }
        DrawLineEx(capture, MapToScreen(influenceMap), 1.0f,
                Color{color.r, color.g, color.b, 150});
        DrawCircleV(capture, selected ? 7.0f : 5.0f, color);
        DrawText("RP", static_cast<int>(capture.x + 7.0f),
                static_cast<int>(capture.y - 18.0f), 14, color);
    };

    for (const SectorAuthoringReflectionProbe& probe : AuthoringGraph().reflectionProbes) {
        bool resolved = false;
        for (const SectorAuthoringDerivedReflectionProbeMapping& mapping
                : documentState.derivation.authoringDerivation.mapping.reflectionProbes) {
            if (mapping.authoringReflectionProbeId == probe.id) {
                resolved = mapping.resolved;
                break;
            }
        }
        drawProbe(
                probe,
                {probe.x, probe.z},
                selectionState.selectedAuthoring.kind
                                == SectorAuthoringSelectionKind::ReflectionProbe
                        && selectionState.selectedAuthoring.reflectionProbeId == probe.id,
                selectionState.hoveredAuthoring.kind
                                == SectorAuthoringSelectionKind::ReflectionProbe
                        && selectionState.hoveredAuthoring.reflectionProbeId == probe.id,
                resolved);
    }
}

void SectorEditor::DrawAuthoringReflectionProbeMoveOverlay() const
{
    const AuthoringReflectionProbeDragState& drag =
            manipulationState.authoringReflectionProbeDrag;
    if (!drag.active || !drag.hasPreviewPoint) return;
    const Vector2 center = MapToScreen({
            SectorCoordToVisibleAuthoring(drag.previewPoint.x),
            SectorCoordToVisibleAuthoring(drag.previewPoint.y)});
    const Color color = drag.previewResolved
            ? Color{220, 145, 255, 230} : Color{240, 82, 82, 245};
    DrawCircleV(center, 8.0f, color);
    DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
            11.0f, color);
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
            && state.currentTool != SectorEditorTool::StaticRectLight
            && state.currentTool != SectorEditorTool::DynamicLight
            && state.currentTool != SectorEditorTool::DynamicSpotLight
            && state.currentTool != SectorEditorTool::DynamicRectLight
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
    auto& itemPlacement = runtimeObjectEditingState.itemPlacement;
    if (itemPlacement.registryRevision != itemRegistry.revision) {
        itemPlacement.registryRevision = itemRegistry.revision;
        itemPlacement.definitionIds.clear();
        itemPlacement.labelStorage.clear();
        itemPlacement.labels.clear();
        itemPlacement.definitionIds.reserve(itemRegistry.items.size());
        itemPlacement.labelStorage.reserve(itemRegistry.items.size());
        for (const ItemDefinition& definition : itemRegistry.items) {
            itemPlacement.definitionIds.push_back(definition.id);
            itemPlacement.labelStorage.push_back(
                    definition.title + " (" + definition.id + ")");
        }
        itemPlacement.labels.reserve(itemPlacement.labelStorage.size());
        for (const std::string& label : itemPlacement.labelStorage) {
            itemPlacement.labels.push_back(label.c_str());
        }
        if (FindItemDefinition(
                    itemRegistry, itemPlacement.lastDefinitionId) == nullptr) {
            itemPlacement.lastDefinitionId = itemRegistry.items.empty()
                    ? std::string{} : itemRegistry.items.front().id;
        }
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
    const float toolsContentH = MeasureSectorEditorToolsContentHeight(
            rowH,
            gap,
            state.currentTool == SectorEditorTool::Trigger,
            state.currentTool == SectorEditorTool::Item);
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
        if (soundEmitterEditingService
                && soundEmitterEditingService->Drag().active
                && tool != SectorEditorTool::Select) {
            soundEmitterEditingService->CancelMove("Cancelled Sound Emitter move");
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
        } else if (tool == SectorEditorTool::Item) {
            statusText = itemRegistry.items.empty()
                    ? "Item: create an item definition in the Item Editor first"
                    : "Item: click inside a derived sector to place an item";
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
        } else if (tool == SectorEditorTool::Window) {
            statusText = "Window: click a two-sided portal line";
        } else if (tool == SectorEditorTool::AuthoringFogVolume) {
            statusText = "Fog Volume: click strictly inside a sector";
        } else if (tool == SectorEditorTool::ReflectionProbe) {
            statusText = "Reflection Probe: click inside a sector";
        } else if (tool == SectorEditorTool::LevelMarker) {
            statusText = "Level Marker: click strictly inside a sector";
        } else if (tool == SectorEditorTool::SoundEmitter) {
            statusText = "Sound Emitter: click inside a sector";
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
            SectorEditorTool::Item,
            SectorEditorTool::Npc,
            SectorEditorTool::Door,
            SectorEditorTool::Window,
            SectorEditorTool::Trigger,
            SectorEditorTool::LevelMarker,
            SectorEditorTool::SoundEmitter,
            SectorEditorTool::AuthoringFogVolume,
            SectorEditorTool::ReflectionProbe,
            SectorEditorTool::StaticLight,
            SectorEditorTool::StaticSpotLight,
            SectorEditorTool::StaticRectLight,
            SectorEditorTool::DynamicLight,
            SectorEditorTool::DynamicSpotLight,
            SectorEditorTool::DynamicRectLight
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
        if (tool == SectorEditorTool::Item
                && state.currentTool == SectorEditorTool::Item) {
            int selectedIndex = -1;
            for (std::size_t index = 0;
                    index < itemPlacement.definitionIds.size(); ++index) {
                if (itemPlacement.definitionIds[index]
                        == itemPlacement.lastDefinitionId) {
                    selectedIndex = static_cast<int>(index);
                    break;
                }
            }
            if (!itemPlacement.labels.empty()) {
                const int previous = selectedIndex;
                engine::Option(
                        ui, config, input, assets,
                        "sector_editor_item_placement_definition",
                        Rectangle{0.0f, y, contentW, rowH},
                        font,
                        itemPlacement.labels.data(),
                        itemPlacement.labels.size(),
                        selectedIndex);
                if (selectedIndex != previous && selectedIndex >= 0) {
                    itemPlacement.lastDefinitionId =
                            itemPlacement.definitionIds[
                                    static_cast<std::size_t>(selectedIndex)];
                }
            } else {
                engine::Text(
                        config,
                        assets,
                        Rectangle{0.0f, y, contentW, rowH},
                        font,
                        "No item definitions",
                        engine::UITextJustify::Left,
                        config.invalidColor);
            }
            y += rowH + gap;
        }
    }

    separator();
    const float roomtoneFadeLabelW = 150.0f;
    const SectorEditorIntInputResult roomtoneFadeResult = DrawLabeledIntInput(
            ui, config, input, assets, font,
            "sector_editor_roomtone_map_fade", "Roomtone Fade ms",
            {0.0f, y, roomtoneFadeLabelW, rowH},
            {roomtoneFadeLabelW + gap, y,
                    std::max(0.0f, contentW - roomtoneFadeLabelW - gap), rowH},
            engine::UITextJustify::Left,
            AuthoringGraph().audioSettings.roomtoneFadeMilliseconds,
            uiState.roomtoneMapFadeInput, 0, 60000, 50);
    if (roomtoneFadeResult.changed) {
        BuildSoundEditorService().SetRoomtoneFadeMilliseconds(
                roomtoneFadeResult.value);
    }
    y += rowH + gap;

    if (engine::Button(ui, config, input, assets, "sector_editor_bake_lightmaps", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        OpenLightmapBakeSetup();
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
    SectorEditorSoundService sounds = BuildSoundService(
            &runtimeObjectEditing, &soundEmitterEditingService.value());
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
            reflectionProbeEditingUiState,
            levelMarkerEditingUiState,
            soundEmitterEditingUiState,
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
            reflectionProbeEditingService.value(),
            levelMarkerEditingService.value(),
            soundEmitterEditingService.value(),
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
        case SectorEditorInspectorPanelRequestKind::ConvertSelectedLight:
            ConvertSelectedLight();
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
        case SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedReflectionProbeConfirmation:
            OpenConfirmation(
                    "Delete Reflection Probe",
                    "Delete the selected reflection probe?",
                    [this]() {
                        if (reflectionProbeEditingService) {
                            reflectionProbeEditingService->DeleteSelected();
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
        case SectorEditorInspectorPanelRequestKind::OpenDeleteSelectedSoundEmitterConfirmation:
            OpenConfirmation(
                    "Delete Sound Emitter",
                    "Delete the selected Sound Emitter?",
                    [this]() {
                        if (soundEmitterEditingService) soundEmitterEditingService->DeleteSelected();
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
            OpenLightmapBakeSetup();
            break;
        case SectorEditorInspectorPanelRequestKind::RefreshPreviewLightSources:
            if (state.mode == SectorEditorMode::Preview3D && sceneRuntime.Renderer().IsRendererReady()) {
                sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
            }
            break;
        }
    }
}

void SectorEditor::DrawSoundEditor(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    SectorEditorSoundEditorService editor = BuildSoundEditorService();
    SectorEditorAudioAssetPickerService audioPicker{
            *engineContext, audioAssetPickerSessionState};
    if (DrawSectorEditorSoundEditorPanel(
                ui, config, input, assets, font, smallFont,
                editor, audioPicker)
            == SectorEditorSoundEditorPanelResult::Saved) {
        BuildSoundService().RefreshCatalogHandles();
    }
}

void SectorEditor::DrawPatrolEditor(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    SectorEditorPatrolEditorService editor = BuildPatrolEditorService();
    DrawSectorEditorPatrolEditorPanel(
            ui, config, input, assets, font, smallFont, editor);
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
    BuildSoundService(&runtimeObjectEditing, &soundEmitterEditingService.value())
            .DrawPickerModal(ui, config, input, font);
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

void SectorEditor::DrawWeaponEditor(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    if (engineContext == nullptr) {
        statusText = "Weapon Editor requires an engine context";
        return;
    }
    const bool openedFromPreview3D = weaponEditorState.openedFromPreview3D;
    const std::string originalActiveWeaponId =
            weaponEditorState.originalActiveWeaponId;
    const FpsViewmodelEquipState originalEquipState =
            weaponEditorState.originalEquipState;
    const float originalEquipProgress =
            weaponEditorState.originalEquipProgress;
    SectorEditorWeaponEditorService editor = BuildWeaponEditorService();
    SectorEditorStaticModelPickerService modelPicker{
            runtimeObjectEditingState.staticModelPicker,
            statusText};
    SectorEditorAudioAssetPickerService audioPicker{
            *engineContext, audioAssetPickerSessionState};
    const SectorEditorWeaponEditorPanelResult result =
            DrawSectorEditorWeaponEditorPanel(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    smallFont,
                    openedFromPreview3D ? &fpsPlayer.State() : nullptr,
                    editor,
                    modelPicker,
                    audioPicker);
    if (result.previewFireRequested) {
        statusText = fpsPlayer.TriggerPreviewShot(
                assets, engineContext->audio, sceneRuntime.Renderer())
                ? "Preview weapon fired"
                : "Preview weapon is not ready to fire";
    }
    if (result.previewReloadRequested) {
        statusText = fpsPlayer.TriggerPreviewReload()
                ? "Preview weapon reloading"
                : "Preview weapon is not ready to reload";
    }
    if (result.holsterToggleRequested) {
        if (!IsFpsWeaponReloading(fpsPlayer.State())
                && ToggleFpsViewmodelHolster(
                        fpsPlayer.State(), true, false)) {
            statusText = fpsPlayer.State().equipState
                            == FpsViewmodelEquipState::Holstering
                    ? "Preview weapon holstering"
                    : "Preview weapon unholstering";
        }
    }
    if (openedFromPreview3D && (result.saved || result.cancelled)) {
        const std::string weaponId = result.saved
                ? weaponEditorSessionState.selectedWeaponId
                : originalActiveWeaponId;
        if (result.cancelled && weaponId.empty()) {
            fpsPlayer.Begin(
                    assets,
                    sceneRuntime.Renderer(),
                    weaponRegistry,
                    applicationSettings,
                    "fps_viewmodel");
            return;
        }
        const FpsWeaponDefinition* slotOneWeapon =
                FindFpsWeaponDefinitionForSlot(
                        weaponRegistry,
                        MinFpsWeaponSlot);
        const std::string fallbackId = FindFpsWeaponDefinition(
                weaponRegistry, weaponId) != nullptr
                ? weaponId
                : (slotOneWeapon != nullptr
                        ? slotOneWeapon->id
                        : (weaponRegistry.weapons.empty()
                                ? std::string{}
                                : weaponRegistry.weapons.front().id));
        const bool selected = !fallbackId.empty() && fpsPlayer.SelectWeapon(
                assets,
                sceneRuntime.Renderer(),
                weaponRegistry,
                applicationSettings,
                fallbackId,
                "fps_viewmodel");
        if (selected) {
            SetFpsViewmodelEquipPose(
                    fpsPlayer.State(),
                    originalEquipState,
                    originalEquipProgress);
        }
    }
}

void SectorEditor::DrawItemEditor(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    SectorEditorItemEditorService editor = BuildItemEditorService();
    SectorEditorStaticModelPickerService modelPicker{
            runtimeObjectEditingState.staticModelPicker,
            statusText};
    const SectorEditorItemEditorPanelResult result =
            DrawSectorEditorItemEditorPanel(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    smallFont,
                    editor,
                    modelPicker);
    if (result.saved) {
        RebuildItemModelAssets(assets, itemRegistry, itemModelAssets);
        InvalidateTopologyRenderCache();
    }
}

void SectorEditor::DrawMaterialRegistryEditor(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    SectorEditorMaterialRegistryEditorService editor =
            BuildMaterialRegistryEditorService();
    const SectorEditorMaterialRegistryEditorResult result =
            game::DrawSectorEditorMaterialRegistryEditor(
                    ui, config, input, assets, font, smallFont, editor);
    if (result == SectorEditorMaterialRegistryEditorResult::Saved) {
        SectorEditorTextureCatalogService catalog = MakeTextureCatalogService();
        catalog.RefreshTextureHandles(assets);
        catalog.RefreshDefaultTextureIds();
        RefreshResolvedMaterials();
        state.lightmapSourceHashRevision = 0;
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

void SectorEditor::DrawColorSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    const SectorEditorColorSettingsModalAction action =
            DrawSectorEditorColorSettingsModal(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    smallFont,
                    state.colorSettingsModal);
    switch (action) {
        case SectorEditorColorSettingsModalAction::Cancel:
            state.colorSettingsModal = {};
            break;
        case SectorEditorColorSettingsModalAction::Apply:
            ApplyColorSettingsModal();
            break;
        case SectorEditorColorSettingsModalAction::None:
            break;
    }
}

void SectorEditor::DrawPlayerSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    if (engineContext == nullptr) return;
    SectorEditorPlayerSettingsService service = BuildPlayerSettingsService();
    const SectorEditorPlayerSettingsSaveResult result =
            DrawSectorEditorPlayerSettingsModal(
            ui,
            config,
            input,
            assets,
            font,
            smallFont,
            *engineContext,
            service);
    if (!result.saved) return;
    if (result.playerAudioChanged) {
        RequestPlayerAudioAssets(
                engineContext->assets,
                applicationSettings.playerSounds,
                playerAudio);
        playerAudioSettingsChanged = true;
    }
    if (result.footstepsChanged
            && state.mode == SectorEditorMode::Preview3D
            && sceneRuntime.Renderer().IsRendererReady()) {
        RebuildPreviewMeshesPreservingView(*engineContext);
    }
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

void SectorEditor::DrawLightmapBakeSetupModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorLightmapBakeSetupModalResult result =
            game::DrawLightmapBakeSetupModal(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    state.lightmapBakeSetupModal);
    if (result != SectorEditorLightmapBakeSetupModalResult::BakeRequested) {
        return;
    }

    const SectorLightmapBakeQualityPreset selectedQuality =
            state.lightmapBakeSetupModal.selectedQuality;
    if (StartLightmapBake(selectedQuality)) {
        CloseSectorEditorLightmapBakeSetupModal(
                state.lightmapBakeSetupModal);
        return;
    }
    state.lightmapBakeSetupModal.errorMessage = statusText;
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
    if (!engine::IsNull(runtimeObjectEditingState.spritePicker.previewScope)) {
        assets.UnloadScope(runtimeObjectEditingState.spritePicker.previewScope);
    }

    state = SectorEditorState{};
    manipulationState = ManipulationState{};
    uiState = SectorEditorUiState{};
    runtimeObjectEditingState = RuntimeObjectEditingState{};
    runtimeObjectEditingUiState = RuntimeObjectEditingUiState{};
    surfaceHeightAdjustmentState = PreviewSurfaceHeightAdjustmentState{};
    textureCatalogState = TextureCatalogState{};
    soundCatalogState = SectorEditorSoundCatalogState{};
    lightEditingState = LightEditingState{};
    materialEditingUiState = MaterialEditingUiState{};
    fogVolumeEditingUiState = FogVolumeEditingUiState{};
    levelMarkerEditingState = LevelMarkerEditingState{};
    levelMarkerEditingUiState = LevelMarkerEditingUiState{};
    soundEmitterEditingState = SoundEmitterEditingState{};
    soundEmitterEditingUiState = SoundEmitterEditingUiState{};
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
    if (soundEmitterEditingService) {
        soundEmitterEditingService->CancelMove(nullptr);
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
    RefreshResolvedMaterials();
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
    soundEditorState = SectorEditorSoundEditorState{};
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
    soundEmitterEditingState = SoundEmitterEditingState{};
    soundEmitterEditingUiState = SoundEmitterEditingUiState{};
    triggerEditingState = TriggerEditingState{};
    triggerEditingUiState = TriggerEditingUiState{};
    runtimeObjectEditingState = RuntimeObjectEditingState{};
    runtimeObjectEditingUiState = RuntimeObjectEditingUiState{};
    surfaceHeightAdjustmentState = PreviewSurfaceHeightAdjustmentState{};
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

    return SaveLevelWithPlan(name, savePlan, modal.errorMessage);
}

bool SectorEditor::SaveCurrentLevel()
{
    if (!Lifecycle().hasCurrentLevelPath) {
        OpenSaveLevelModal();
        return false;
    }

    SectorEditorSaveLevelPlan savePlan;
    std::string errorMessage;
    if (!PrepareSaveLevelPlan(
                Lifecycle().currentLevelName,
                true,
                Lifecycle().currentLevelPath,
                false,
                savePlan,
                errorMessage)) {
        statusText = errorMessage.empty()
                ? "Save failed"
                : "Save failed: " + errorMessage;
        return false;
    }
    return SaveLevelWithPlan(
            Lifecycle().currentLevelName,
            savePlan,
            errorMessage);
}

bool SectorEditor::SaveLevelWithPlan(
        const std::string& name,
        const SectorEditorSaveLevelPlan& savePlan,
        std::string& errorMessage)
{
    if (!EnsureSaveLevelDirectory(savePlan.paths, errorMessage)) {
        statusText = errorMessage.empty()
                ? "Save failed"
                : "Save failed: " + errorMessage;
        return false;
    }

    if (!SaveSectorEditorAuthoringDocument(
                savePlan.paths,
                documentState.authoring,
                documentState.map,
                documentState.derivation,
                SectorAuthoringEditorSettings{state.gridSize},
                errorMessage)) {
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
            || state.lightmapBakeSetupModal.open
            || state.previewSettingsModal.open
            || state.colorSettingsModal.open
            || playerSettingsState.open;
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
    if (soundEmitterEditingService) {
        soundEmitterEditingService->CancelMove(nullptr);
    }
    ui.hotId = 0;
    ui.activeId = 0;
    ui.openOptionId = 0;
    ui.focusedId = 0;
    engine::CloseMainMenu(uiState.mainMenu);
    uiState.keyboardCaptured = false;

    std::string gateMessage;
    if (!CanUseCurrentAuthoringDerivedTopologyForPreview(
                MakeLiveConstDerivationAccess(documentState.derivation),
                &gateMessage)) {
        statusText = gateMessage.empty() ? "3D mode failed: derived topology is not current" : gateMessage;
        return false;
    }

    RefreshResolvedMaterials();
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
    if (PreviewAdjustmentActive()) {
        CancelPreviewAdjustment(
                "3D adjustment cancelled on return to 2D");
    }
    engine::CloseMainMenu(uiState.mainMenu);
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
                sceneRuntime.RuntimeObjects().physicalModelColliders,
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
    float lightRollDegrees = 0.0f;
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
    } else if (selectionState.topologySelectionKind == TopologySelectionKind::StaticRectLight) {
        const auto* light = FindSectorTopologyStaticRectLight(
                TopologyMap(), selectionState.selectedTopologyStaticSpotLightId);
        if (light == nullptr) return false;
        lightId = light->id; pilotKind = LightPilotKind::StaticRect;
        lightPosition = light->position; lightTarget = light->target;
        lightRange = light->range; lightRollDegrees = light->rollDegrees;
        lightName = "static rect light";
    } else if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicRectLight) {
        const auto* light = FindSectorTopologyDynamicRectLight(
                TopologyMap(), selectionState.selectedTopologyDynamicSpotLightId);
        if (light == nullptr) return false;
        lightId = light->id; pilotKind = LightPilotKind::DynamicRect;
        lightPosition = light->position; lightTarget = light->target;
        lightRange = light->range; lightRollDegrees = light->rollDegrees;
        lightName = "dynamic rect light";
    } else {
        statusText = "Select a light to pilot";
        return false;
    }

    const bool isSpot = pilotKind == LightPilotKind::StaticSpot
            || pilotKind == LightPilotKind::DynamicSpot
            || pilotKind == LightPilotKind::StaticRect
            || pilotKind == LightPilotKind::DynamicRect;
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
    lightEditingState.lightPilot.originalRollDegrees = lightRollDegrees;
    previewState.controller.lightPilotPreviewRestore.originalPreviewPose = originalPreviewPose;
    previewState.controller.lightPilotPreviewRestore.originalMouseLookEnabled = previewState.controller.freeflyController.mouseLookEnabled;
    lightEditingState.lightPilot.targetDistanceWorld = targetDistanceWorld;

    SectorViewPose pilotPose = originalPreviewPose;
    pilotPose.position = originWorld;
    if (isSpot) {
        pilotPose = PreviewPoseLookingAt(originWorld, targetWorld);
        // Rect pilot roll is relative to the authored roll. Starting upright
        // keeps fixture-alignment roll from tilting the editing camera.
        pilotPose.rollRadians = 0.0f;
    }
    ResetSectorFreeflyController(previewState.controller.freeflyController, pilotPose);
    EnterSectorFreeflyController(previewState.controller.freeflyController);
    sceneRuntime.Renderer().ApplyRendererPose(previewState.controller.freeflyController.pose);
    previewState.selection.hoveredSurface3D = SectorSurfaceHit{};
    statusText = TextFormat(
            (pilotKind == LightPilotKind::StaticRect || pilotKind == LightPilotKind::DynamicRect)
                    ? "Piloting %s %d (Q/E roll, Enter apply, Escape cancel)"
                    : "Piloting %s %d (Enter apply, Escape cancel)",
            lightName,
            lightId);
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
    } else if (selectionState.topologySelectionKind == TopologySelectionKind::StaticRectLight) {
        if (const auto* light = FindSectorTopologyStaticRectLight(
                    TopologyMap(), selectionState.selectedTopologyStaticSpotLightId)) {
            kind = LightPilotKind::StaticRect; lightId = light->id;
            proxyEnabled = shaft ? light->atmosphere.proxy.shaft.enabled
                    : light->atmosphere.proxy.halo.enabled;
        }
    } else if (selectionState.topologySelectionKind == TopologySelectionKind::DynamicRectLight) {
        if (const auto* light = FindSectorTopologyDynamicRectLight(
                    TopologyMap(), selectionState.selectedTopologyDynamicSpotLightId)) {
            kind = LightPilotKind::DynamicRect; lightId = light->id;
            proxyEnabled = shaft ? light->atmosphere.proxy.shaft.enabled
                    : light->atmosphere.proxy.halo.enabled;
        }
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
            SectorWorldToAuthoringPosition(targetWorld),
            pose.rollRadians * RAD2DEG);
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
            sceneRuntime.RuntimeObjects().physicalModelColliders);
}

SectorFpsVerticalContext SectorEditor::BuildGameplayVerticalContext()
{
    return BuildSectorEditorGameplayVerticalContext(
            previewState.collision,
            previewState.controller,
            sceneRuntime.RuntimeObjects().physicalModelColliders);
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
            sceneRuntime.RuntimeObjects().physicalModelColliders);
}

void SectorEditor::OpenPreviewSettingsModal()
{
    ResetSectorPreviewSettingsModalPreservingView(
            state.previewSettingsModal);
    state.previewSettingsModal.open = true;
    state.previewSettingsModal.draftConfig = NormalizeSectorFpsControllerConfig(previewState.controller.fpsControllerConfig);
    state.previewSettingsModal.draftNpcToNpcCollisionEnabled =
            NormalizeSectorPreviewSettings(
                    TopologyMap().previewSettings).npcToNpcCollisionEnabled;
    state.previewSettingsModal.draftSkySettings = NormalizeSectorTopologySkySettings(TopologyMap().skySettings);
    state.previewSettingsModal.draftDirectionalLight =
            NormalizeSectorTopologyDirectionalLightSettings(TopologyMap().directionalLight);
    state.previewSettingsModal.draftFogSettings =
            NormalizeSectorTopologyFogSettings(TopologyMap().fogSettings);
    state.previewSettingsModal.draftLightmapSettings =
            NormalizeSectorLevelLightmapSettings(TopologyMap().lightmapSettings);
    state.previewSettingsModal.draftHdrBloom =
            engine::NormalizeHdrBloomSettings(applicationSettings.hdrBloom);
}

void SectorEditor::OpenColorSettingsModal()
{
    state.colorSettingsModal = {};
    state.colorSettingsModal.open = true;
    state.colorSettingsModal.draft = engine::NormalizeToneMappingSettings(
            applicationSettings.toneMapping);
}

void SectorEditor::ApplyColorSettingsModal()
{
    if (!state.colorSettingsModal.open) return;

    FpsApplicationSettings candidate = applicationSettings;
    candidate.toneMapping = engine::NormalizeToneMappingSettings(
            state.colorSettingsModal.draft);
    std::string saveError;
    if (!SaveFpsApplicationSettings(
                applicationSettingsPath,
                candidate,
                &saveError)) {
        state.colorSettingsModal.errorMessage = saveError.empty()
                ? "Could not save color settings"
                : saveError;
        return;
    }

    applicationSettings = std::move(candidate);
    statusText = "Color settings saved";
    state.colorSettingsModal = {};
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
    draftPreviewSettings.npcToNpcCollisionEnabled =
            state.previewSettingsModal.draftNpcToNpcCollisionEnabled;
    const SectorTopologySkySettings draftSkySettings = NormalizeSectorTopologySkySettings(
            state.previewSettingsModal.draftSkySettings);
    const SectorTopologyDirectionalLightSettings draftDirectionalLight =
            NormalizeSectorTopologyDirectionalLightSettings(
                    state.previewSettingsModal.draftDirectionalLight);
    const SectorTopologyFogSettings draftFogSettings =
            NormalizeSectorTopologyFogSettings(state.previewSettingsModal.draftFogSettings);
    const SectorLightmapBakeSettings draftLightmapSettings =
            NormalizeSectorLevelLightmapSettings(state.previewSettingsModal.draftLightmapSettings);
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
            NormalizeSectorLevelLightmapSettings(TopologyMap().lightmapSettings);
    const bool lightmapSettingsChanged = !SameSectorLevelLightmapSettings(
            currentLightmapSettings,
            draftLightmapSettings);
    const engine::HdrBloomSettings draftHdrBloom =
            engine::NormalizeHdrBloomSettings(state.previewSettingsModal.draftHdrBloom);
    const engine::HdrBloomSettings currentHdrBloom =
            engine::NormalizeHdrBloomSettings(applicationSettings.hdrBloom);
    const bool bloomChanged = draftHdrBloom.enabled != currentHdrBloom.enabled
            || draftHdrBloom.threshold != currentHdrBloom.threshold
            || draftHdrBloom.softKnee != currentHdrBloom.softKnee
            || draftHdrBloom.intensity != currentHdrBloom.intensity
            || draftHdrBloom.radius != currentHdrBloom.radius;
    if (!previewChanged && !skyChanged && !directionalChanged && !fogChanged
            && !lightmapSettingsChanged && !bloomChanged) {
        ResetSectorPreviewSettingsModalPreservingView(
                state.previewSettingsModal);
        statusText = "Level settings unchanged";
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
    ApplySectorLevelLightmapSettings(TopologyMap(), draftLightmapSettings);
    applicationSettings.hdrBloom = draftHdrBloom;
    if (bloomChanged) {
        std::string saveError;
        if (!SaveFpsApplicationSettings(applicationSettingsPath, applicationSettings, &saveError)) {
            TraceLog(LOG_WARNING, "Could not persist application settings: %s", saveError.c_str());
        }
    }
    if (previewChanged || skyChanged || directionalChanged || fogChanged || lightmapSettingsChanged) {
        MarkTopologyDocumentEdited("Level settings updated");
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
            "Level settings updated: walk %.1f run %.1f eye %.1f gravity %.1f radius %.2f height %.2f step %.2f jump %.2f bob %.3f freq %.1f",
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
                    materialRegistry,
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

SectorEditorWeaponEditorService SectorEditor::BuildWeaponEditorService()
{
    return SectorEditorWeaponEditorService{
            weaponEditorState,
            weaponEditorSessionState,
            weaponRegistry,
            itemRegistry,
            applicationSettings,
            statusText,
            weaponRegistryPath,
            applicationSettingsPath};
}

SectorEditorItemEditorService SectorEditor::BuildItemEditorService()
{
    return SectorEditorItemEditorService{
            itemEditorState,
            itemEditorSessionState,
            itemRegistry,
            weaponRegistry,
            gameSessionExists,
            statusText,
            itemRegistryPath,
            std::filesystem::path(ASSETS_PATH) / "levels"};
}

SectorEditorPlayerSettingsService SectorEditor::BuildPlayerSettingsService()
{
    return SectorEditorPlayerSettingsService{
            playerSettingsState,
            applicationSettings,
            statusText,
            applicationSettingsPath};
}

SectorEditorMaterialRegistryEditorService SectorEditor::BuildMaterialRegistryEditorService()
{
    return SectorEditorMaterialRegistryEditorService{
            materialRegistryEditorState,
            materialRegistry,
            AuthoringGraph(),
            TopologyMap(),
            MakeLiveDerivationAccess(documentState.derivation),
            Lifecycle(),
            statusText,
            std::filesystem::path(ASSETS_PATH) / "materials" / "materials.json",
            std::filesystem::path(ASSETS_PATH) / "levels"};
}

void SectorEditor::OpenWeaponEditor(bool fromPreview3D)
{
    const FpsWeaponDefinition* slotOneWeapon =
            FindFpsWeaponDefinitionForSlot(
                    weaponRegistry,
                    MinFpsWeaponSlot);
    const std::string activeWeaponId = fromPreview3D
            ? fpsPlayer.State().activeWeaponId
            : (slotOneWeapon != nullptr
                    ? slotOneWeapon->id
                    : std::string{});
    const FpsViewmodelEquipState equipState = fpsPlayer.State().equipState;
    const float equipProgress = fpsPlayer.State().equipProgress;
    if (BuildWeaponEditorService().Open(activeWeaponId, fromPreview3D)
            && fromPreview3D) {
        weaponEditorState.originalEquipState = equipState;
        weaponEditorState.originalEquipProgress = equipProgress;
        EquipFpsViewmodelForWeaponEditing(fpsPlayer.State());
    }
}

bool SectorEditor::PointInTopologyLoop(
        Vector2 mapPoint,
        const SectorTopologyLoop& loop) const
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
                    materialRegistry,
                    TopologyMap(),
                    AuthoringGraph(),
                    MakeLiveDerivationAccess(documentState.derivation),
                    state.topologyRenderWarning,
                    state.topologyRenderRevision,
                    state.topologyRenderCache,
                    previewState.selection,
                    selectionState,
                    materialEditingUiState,
                    state.texturePicker,
                    state.decalTintModal,
                    statusText,
                    [this](engine::AssetManager*) {
                        if (state.mode == SectorEditorMode::Preview3D
                                && sceneRuntime.Renderer().IsRendererReady()
                                && engineContext != nullptr) {
                            return RefreshPreviewSurfaceMaterials(*engineContext);
                        }
                        return true;
                    }}};
}

bool SectorEditor::CopySelectedConfig(engine::AssetManager& assets)
{
    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    SectorEditorRuntimeObjectEditingService runtimeObjectEditing =
            BuildRuntimeObjectEditingService();
    SectorEditorMaterialEditingService materialEditing =
            BuildMaterialEditingService();
    SectorEditorConfigClipboardService service{
            SectorEditorConfigClipboardServiceContext{
                    state,
                    Lifecycle(),
                    TopologyMap(),
                    AuthoringGraph(),
                    MakeLiveDerivationAccess(documentState.derivation),
                    selectionState,
                    previewState.selection,
                    uiState,
                    configClipboardState,
                    lightEditing,
                    runtimeObjectEditing,
                    materialEditing,
                    assets,
                    statusText}};
    return service.Copy();
}

bool SectorEditor::PasteSelectedConfig(engine::AssetManager& assets)
{
    const SectorEditorConfigTarget target = ResolveSectorEditorConfigTarget(
            state.mode,
            TopologyMap(),
            AuthoringGraph(),
            MakeLiveConstDerivationAccess(documentState.derivation),
            selectionState,
            previewState.selection);
    SectorEditorLightEditingService lightEditing = BuildLightEditingService();
    SectorEditorRuntimeObjectEditingService runtimeObjectEditing =
            BuildRuntimeObjectEditingService();
    SectorEditorMaterialEditingService materialEditing =
            BuildMaterialEditingService();
    SectorEditorConfigClipboardService service{
            SectorEditorConfigClipboardServiceContext{
                    state,
                    Lifecycle(),
                    TopologyMap(),
                    AuthoringGraph(),
                    MakeLiveDerivationAccess(documentState.derivation),
                    selectionState,
                    previewState.selection,
                    uiState,
                    configClipboardState,
                    lightEditing,
                    runtimeObjectEditing,
                    materialEditing,
                    assets,
                    statusText}};
    const bool changed = service.Paste();
    if (!changed) return false;

    const bool lightChanged = target.kind == SectorEditorConfigKind::StaticPointLight
            || target.kind == SectorEditorConfigKind::StaticSpotLight
            || target.kind == SectorEditorConfigKind::StaticRectLight
            || target.kind == SectorEditorConfigKind::DynamicPointLight
            || target.kind == SectorEditorConfigKind::DynamicSpotLight
            || target.kind == SectorEditorConfigKind::DynamicRectLight;
    if (lightChanged
            && state.mode == SectorEditorMode::Preview3D
            && sceneRuntime.Renderer().IsRendererReady()) {
        sceneRuntime.Renderer().RefreshDynamicLightSources(TopologyMap());
    }
    if (target.kind == SectorEditorConfigKind::Sector
            && state.mode == SectorEditorMode::Preview3D
            && engineContext != nullptr) {
        RebuildPreviewMeshesPreservingView(*engineContext);
    }
    return true;
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
                            uiState.lightStartFeatherInput,
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

bool SectorEditor::RefreshPreviewSurfaceMaterials(engine::EngineContext& context)
{
    if (!sceneRuntime.Renderer().IsRendererReady()) {
        return false;
    }

    std::string gateMessage;
    if (!CanUseCurrentAuthoringDerivedTopologyForPreview(
                MakeLiveConstDerivationAccess(documentState.derivation),
                &gateMessage)) {
        statusText = gateMessage.empty()
                ? "3D material refresh failed: derived topology is not current"
                : gateMessage;
        return false;
    }

    RefreshResolvedMaterials();
    std::string error;
    if (sceneRuntime.Renderer().RefreshSurfaceMaterials(
                context.assets, TopologyMap(), error)) {
        return true;
    }

    TraceLog(
            LOG_WARNING,
            "Incremental 3D material refresh failed; falling back to full preview rebuild: %s",
            error.empty() ? "unknown error" : error.c_str());
    return RebuildPreviewMeshesPreservingView(context);
}

bool SectorEditor::RefreshPreviewSurfaceGeometry(
        const SectorTopologyMap& topologyMap,
        std::string* outError)
{
    if (outError != nullptr) outError->clear();
    if (engineContext == nullptr
            || !sceneRuntime.Renderer().IsRendererReady()) {
        if (outError != nullptr) {
            *outError = "3D surface geometry refresh requires an active preview";
        }
        return false;
    }

    std::string error;
    const bool refreshed = sceneRuntime.Renderer().RefreshSurfaceGeometry(
            engineContext->assets, topologyMap, error);
    if (!refreshed && outError != nullptr) {
        *outError = error.empty()
                ? "3D surface geometry refresh failed"
                : error;
    }
    return refreshed;
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

    RefreshResolvedMaterials();
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
                materialRegistry,
                const_cast<SectorTopologyMap&>(TopologyMap()),
                const_cast<SectorAuthoringGraph&>(AuthoringGraph()),
                MakeLiveDerivationAccess(const_cast<SectorEditorDerivationState&>(documentState.derivation)),
                const_cast<std::string&>(state.topologyRenderWarning),
                const_cast<uint64_t&>(state.topologyRenderRevision),
                const_cast<SectorEditorTopologyRenderCache&>(state.topologyRenderCache),
                const_cast<SectorEditorPreviewSelectionState&>(previewState.selection),
                const_cast<SelectionState&>(selectionState),
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
        const std::string selectedTexture = selected.materialId;
        CloseSectorEditorTexturePicker(picker);

        if (selectionState.selectedRuntimeObjectId != targetObjectId) {
            statusText = "Door texture target unavailable";
            return;
        }

        const bool changed = MutateSelectedRuntimeObject(
                "Updated door texture",
                [&selectedTexture](SectorPlacedRuntimeObject& object) {
                    if (object.kind != "door" || object.door.materialId == selectedTexture) {
                        return false;
                    }
                    object.door.materialId = selectedTexture;
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
