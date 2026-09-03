#include "game/FpsPlayerRuntime.h"

#include "engine/render/ColorTransfer.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorLightmap.h"
#include "game/FpsHudRenderer.h"
#include "game/FpsViewmodelEffectsRenderer.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

void ResetFpsViewmodelRuntimePreservingStringStorage(
        FpsViewmodelRuntimeState& state)
{
    std::string activeWeaponId = std::move(state.activeWeaponId);
    std::string resolvedModelPath = std::move(state.resolvedModelPath);
    std::string animationName = std::move(state.animationName);
    std::string error = std::move(state.error);
    std::string attachmentResolvedModelPath =
            std::move(state.attachment.resolvedModelPath);
    std::string attachmentConfiguredBoneName =
            std::move(state.attachment.configuredBoneName);
    std::string attachmentResolvedBoneName =
            std::move(state.attachment.resolvedBoneName);
    std::string attachmentError = std::move(state.attachment.error);

    ResetFpsViewmodelRuntime(state);
    state.activeWeaponId = std::move(activeWeaponId);
    state.activeWeaponId.clear();
    state.resolvedModelPath = std::move(resolvedModelPath);
    state.resolvedModelPath.clear();
    state.animationName = std::move(animationName);
    state.animationName.clear();
    state.error = std::move(error);
    state.error.clear();
    state.attachment.resolvedModelPath =
            std::move(attachmentResolvedModelPath);
    state.attachment.resolvedModelPath.clear();
    state.attachment.configuredBoneName =
            std::move(attachmentConfiguredBoneName);
    state.attachment.configuredBoneName.clear();
    state.attachment.resolvedBoneName =
            std::move(attachmentResolvedBoneName);
    state.attachment.resolvedBoneName.clear();
    state.attachment.error = std::move(attachmentError);
    state.attachment.error.clear();
}

} // namespace

void FpsPlayerRuntime::Begin(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        const char* scopeName,
        bool activateInitialWeapon)
{
    End(assets, renderer);
    if (!LoadFpsMuzzleFlashRenderResources(muzzleFlashRenderResources)) {
        TraceLog(LOG_WARNING, "MUZZLE FLASH: HDR shader unavailable; visible flash disabled");
    }
    weaponAssetScope = assets.CreateScope(
            scopeName != nullptr ? scopeName : "fps_player_viewmodel");
    if (engine::IsNull(weaponAssetScope)) {
        state.loadState = FpsViewmodelLoadState::Failed;
        state.error = "Could not create the FPS viewmodel asset scope";
        return;
    }
    state.assetScope = weaponAssetScope;
    weaponAssets.reserve(registry.weapons.size());
    for (const FpsWeaponDefinition& weapon : registry.weapons) {
        if (EnsureWeaponAssetEntry(assets, weapon) == InvalidWeaponAssetIndex) {
            TraceLog(
                    LOG_WARNING,
                    "Could not preload FPS weapon models for '%s'",
                    weapon.id.c_str());
        }
        state.activeWeaponId.reserve(std::max(
                state.activeWeaponId.capacity(), weapon.id.size()));
        state.animationName.reserve(std::max(
                state.animationName.capacity(),
                weapon.viewmodel.idleAnimation.size()));
        state.attachment.configuredBoneName.reserve(std::max(
                state.attachment.configuredBoneName.capacity(),
                weapon.viewmodel.attachment.boneName.size()));
        cameraRecoilWeaponId.reserve(std::max(
                cameraRecoilWeaponId.capacity(), weapon.id.size()));
    }
    for (const WeaponAssetEntry& entry : weaponAssets) {
        state.resolvedModelPath.reserve(std::max(
                state.resolvedModelPath.capacity(),
                entry.resolvedModelPath.size()));
        state.attachment.resolvedModelPath.reserve(std::max(
                state.attachment.resolvedModelPath.capacity(),
                entry.resolvedAttachmentModelPath.size()));
    }
    state.attachment.resolvedBoneName.reserve(sizeof(BoneInfo::name));
    state.error.reserve(256);
    state.attachment.error.reserve(256);
    const FpsWeaponDefinition* initial = FindFpsWeaponDefinitionForSlot(
            registry,
            MinFpsWeaponSlot);
    if (activateInitialWeapon && initial != nullptr) {
        ActivateWeapon(
                renderer,
                registry,
                settings,
                initial->id,
                false);
    }
}

size_t FpsPlayerRuntime::FindWeaponAssetEntry(
        const FpsWeaponDefinition& definition) const
{
    for (size_t index = 0; index < weaponAssets.size(); ++index) {
        const WeaponAssetEntry& entry = weaponAssets[index];
        if (entry.weaponId == definition.id
                && entry.modelPath == definition.viewmodel.modelPath
                && entry.attachmentModelPath
                        == definition.viewmodel.attachment.modelPath) {
            return index;
        }
    }
    return InvalidWeaponAssetIndex;
}

size_t FpsPlayerRuntime::EnsureWeaponAssetEntry(
        engine::AssetManager& assets,
        const FpsWeaponDefinition& definition)
{
    const size_t existing = FindWeaponAssetEntry(definition);
    if (existing != InvalidWeaponAssetIndex) {
        return existing;
    }
    if (engine::IsNull(weaponAssetScope)) {
        return InvalidWeaponAssetIndex;
    }

    WeaponAssetEntry entry;
    entry.weaponId = definition.id;
    entry.modelPath = definition.viewmodel.modelPath;
    entry.attachmentModelPath = definition.viewmodel.attachment.modelPath;
    entry.resolvedModelPath = ResolveSectorAssetPath(
            definition.viewmodel.modelPath);
    entry.resolvedAttachmentModelPath = ResolveSectorAssetPath(
            definition.viewmodel.attachment.modelPath);
    entry.model = assets.RequestModel(
            weaponAssetScope,
            ("fps_viewmodel_" + definition.id).c_str(),
            entry.resolvedModelPath.c_str(),
            engine::ModelLoad_Animations);
    entry.attachmentModel = assets.RequestModel(
            weaponAssetScope,
            ("fps_viewmodel_attachment_" + definition.id).c_str(),
            entry.resolvedAttachmentModelPath.c_str(),
            engine::ModelLoad_None);
    if (engine::IsNull(entry.model)
            || engine::IsNull(entry.attachmentModel)) {
        return InvalidWeaponAssetIndex;
    }
    entry.modelInstance.model = entry.model;
    weaponAssets.push_back(std::move(entry));
    return weaponAssets.size() - 1;
}

void FpsPlayerRuntime::PrepareInactiveWeaponInstances(
        engine::AssetManager& assets)
{
    for (size_t index = 0; index < weaponAssets.size(); ++index) {
        if (index == activeWeaponAssetIndex) continue;
        WeaponAssetEntry& entry = weaponAssets[index];
        if (entry.modelInstance.poseReady || entry.modelInstance.poseFailed) {
            continue;
        }
        const engine::ModelAsset* asset = assets.GetModelAsset(entry.model);
        if (asset != nullptr) {
            engine::PrepareAnimatedModelInstance(entry.modelInstance, *asset);
        } else if (assets.HasFailed(entry.model)) {
            entry.modelInstance.poseFailed = true;
        }
    }
}

void FpsPlayerRuntime::ResetActiveWeapon(SectorMeshRenderer& renderer)
{
    if (activeWeaponAssetIndex < weaponAssets.size()) {
        WeaponAssetEntry& entry = weaponAssets[activeWeaponAssetIndex];
        entry.modelInstance = std::move(state.modelInstance);
        entry.modelInstance.model = entry.model;
    }
    renderer.SetRuntimePointLight(nullptr);
    ResetFpsViewmodelRuntimePreservingStringStorage(state);
    state.assetScope = weaponAssetScope;
    activeWeaponAssetIndex = InvalidWeaponAssetIndex;
    cameraRecoilWeaponId.clear();
}

bool FpsPlayerRuntime::ActivateWeapon(
        SectorMeshRenderer& renderer,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        std::string_view weaponId,
        bool allowAssetRequest,
        engine::AssetManager* assets)
{
    const FpsWeaponDefinition* definition = FindFpsWeaponDefinition(
            registry,
            weaponId);
    if (definition == nullptr) {
        state.loadState = FpsViewmodelLoadState::Failed;
        state.error = "Weapon definition '" + std::string(weaponId)
                + "' is unavailable";
        return false;
    }
    size_t assetIndex = FindWeaponAssetEntry(*definition);
    if (assetIndex == InvalidWeaponAssetIndex
            && allowAssetRequest
            && assets != nullptr) {
        assetIndex = EnsureWeaponAssetEntry(*assets, *definition);
    }
    if (assetIndex == InvalidWeaponAssetIndex) {
        state.loadState = FpsViewmodelLoadState::Failed;
        state.error = "Weapon assets for '" + definition->id
                + "' were not preloaded";
        return false;
    }

    ResetActiveWeapon(renderer);
    WeaponAssetEntry& assetEntry = weaponAssets[assetIndex];
    activeWeaponAssetIndex = assetIndex;
    state.activeWeaponId = definition->id;
    cameraRecoilWeaponId = definition->id;
    state.resolvedModelPath = assetEntry.resolvedModelPath;
    state.animationName = definition->viewmodel.idleAnimation;
    state.brightnessAdjustment = definition->viewmodel.brightnessAdjustment;
    state.brightnessMultiplier = FpsViewmodelBrightnessMultiplier(
            state.brightnessAdjustment);
    state.materialOverride = definition->viewmodel.materialOverride;
    state.attachment.resolvedModelPath =
            assetEntry.resolvedAttachmentModelPath;
    state.attachment.configuredBoneName =
            definition->viewmodel.attachment.boneName;
    state.attachment.gripCorrection = ResolveFpsViewmodelGripCorrection(
            definition->viewmodel.attachment.gripCorrection,
            FindFpsViewmodelGripCorrectionOverride(settings, definition->id));
    state.attachment.lightingDefaults =
            definition->viewmodel.attachment.lighting;
    state.attachment.lighting = ResolveFpsViewmodelAttachmentLighting(
            definition->viewmodel.attachment.lighting,
            FindFpsViewmodelAttachmentLightingOverride(
                    settings,
                    definition->id));
    state.attachment.brightnessMultiplier = FpsViewmodelBrightnessMultiplier(
            state.attachment.lighting.brightnessAdjustment);
    state.presentation = ResolveFpsViewmodelPresentation(
            definition->viewmodel.presentation,
            FindFpsViewmodelOverride(settings, definition->id));
    state.holsterTransition = ResolveFpsViewmodelHolsterTransition(
            definition->viewmodel.holsterTransition,
            FindFpsViewmodelHolsterTransitionOverride(
                    settings,
                    definition->id));
    state.holsterPose = EvaluateFpsViewmodelHolsterPose(
            state.holsterTransition,
            state.equipProgress);
    state.firing.definition = ResolveFpsWeaponFiringDefinition(
            definition->firing,
            FindFpsWeaponFiringOverride(settings, definition->id));
    state.reloadDefinition = definition->reload;
    state.modelInstance = std::move(assetEntry.modelInstance);
    state.modelInstance.model = assetEntry.model;
    state.attachment.model = assetEntry.attachmentModel;
    state.attachment.loadState = FpsViewmodelAttachmentLoadState::Pending;
    state.loadState = FpsViewmodelLoadState::Pending;
    return true;
}

bool FpsPlayerRuntime::LoadWeapon(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        std::string_view weaponId,
        const char* scopeName)
{
    (void)scopeName;
    return ActivateWeapon(
            renderer,
            registry,
            settings,
            weaponId,
            true,
            &assets);
}

bool FpsPlayerRuntime::SelectWeapon(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        std::string_view weaponId,
        const char* scopeName)
{
    pendingWeaponSlot = 0;
    pendingUnequip = false;
    return LoadWeapon(
            assets,
            renderer,
            registry,
            settings,
            weaponId,
            scopeName);
}

bool FpsPlayerRuntime::EquipWeapon(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        std::string_view weaponId,
        PlayerWeaponCampaignState* weaponCampaign)
{
    if (!SelectWeapon(
                assets, renderer, registry, settings, weaponId,
                "fps_viewmodel")) {
        return false;
    }
    BeginFpsWeaponSlotTargetUnholster(state);
    if (weaponCampaign != nullptr) {
        weaponCampaign->activeWeaponId = std::string{weaponId};
    }
    return true;
}

bool FpsPlayerRuntime::HolsterForTraversal()
{
    pendingWeaponSlot = 0;
    state.reload = {};
    return RequestFpsViewmodelHolster(state);
}

bool FpsPlayerRuntime::QueueUnequip(
        PlayerWeaponCampaignState* weaponCampaign)
{
    pendingWeaponSlot = 0;
    state.reload = {};
    if (state.activeWeaponId.empty()) {
        if (weaponCampaign != nullptr) weaponCampaign->activeWeaponId.clear();
        return false;
    }
    pendingUnequip = true;
    if (state.equipState != FpsViewmodelEquipState::Holstered
            && state.equipProgress > 0.0f) {
        state.equipState = FpsViewmodelEquipState::Holstering;
    }
    return true;
}

void FpsPlayerRuntime::End(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer)
{
    ResetActiveWeapon(renderer);
    if (!engine::IsNull(weaponAssetScope)) {
        assets.UnloadScope(weaponAssetScope);
    }
    weaponAssetScope = engine::NullAssetScopeHandle();
    weaponAssets.clear();
    activeWeaponAssetIndex = InvalidWeaponAssetIndex;
    pendingWeaponSlot = 0;
    pendingUnequip = false;
    reloadOutOfAmmoRequested = false;
    ResetFpsViewmodelRuntime(state);
    UnloadFpsMuzzleFlashRenderResources(muzzleFlashRenderResources);
}

void FpsPlayerRuntime::AdvanceReload(
        engine::AssetManager& assets,
        engine::AudioSystem* audio,
        float dt)
{
    if (state.reload.phase == FpsWeaponReloadPhase::Completing) {
        state.reload = {};
        return;
    }
    float remaining = std::isfinite(dt) ? std::max(0.0f, dt) : 0.0f;
    const FpsViewmodelHolsterTransition transition =
            ClampFpsViewmodelHolsterTransition(state.holsterTransition);
    while (remaining > 0.0f && IsFpsWeaponReloading(state)) {
        if (state.reload.phase == FpsWeaponReloadPhase::Holstering) {
            state.equipState = FpsViewmodelEquipState::Holstering;
            const float phaseRemaining = state.equipProgress
                    * transition.holsterDurationSeconds;
            const float step = std::min(remaining, phaseRemaining);
            AdvanceFpsViewmodelEquipTransition(state, step);
            state.reload.totalElapsedSeconds += step;
            remaining -= step;
            if (state.equipState != FpsViewmodelEquipState::Holstered) break;
            state.reload.phase = FpsWeaponReloadPhase::Waiting;
            state.reload.waitElapsedSeconds = 0.0f;
            if (audio != nullptr) {
                audio->PlaySound(
                        assets,
                        state.reloadDefinition.reloadSound,
                        engine::SoundPlaybackSettings{});
            }
            continue;
        }
        if (state.reload.phase == FpsWeaponReloadPhase::Waiting) {
            const float phaseRemaining = std::max(
                    0.0f,
                    state.reloadDefinition.durationSeconds
                            - state.reload.waitElapsedSeconds);
            const float step = std::min(remaining, phaseRemaining);
            state.reload.waitElapsedSeconds += step;
            state.reload.totalElapsedSeconds += step;
            remaining -= step;
            if (state.reload.waitElapsedSeconds
                    < state.reloadDefinition.durationSeconds) {
                break;
            }
            state.reload.phase = FpsWeaponReloadPhase::Unholstering;
            state.equipState = FpsViewmodelEquipState::Unholstering;
            continue;
        }
        if (state.reload.phase == FpsWeaponReloadPhase::Unholstering) {
            state.equipState = FpsViewmodelEquipState::Unholstering;
            const float phaseRemaining = (1.0f - state.equipProgress)
                    * transition.unholsterDurationSeconds;
            const float step = std::min(remaining, phaseRemaining);
            AdvanceFpsViewmodelEquipTransition(state, step);
            state.reload.totalElapsedSeconds += step;
            remaining -= step;
            if (state.equipState != FpsViewmodelEquipState::Equipped) break;
            state.reload.totalElapsedSeconds =
                    state.reload.totalDurationSeconds;
            state.reload.phase = FpsWeaponReloadPhase::Completing;
            break;
        }
        break;
    }
}

void FpsPlayerRuntime::Update(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        float dt,
        const FpsPlayerRuntimeTuning* tuning,
        PlayerWeaponCampaignState* weaponCampaign,
        engine::AudioSystem* audio)
{
    PrepareInactiveWeaponInstances(assets);
    const auto commitPendingSwitch = [this, &renderer, &registry, &settings,
                                      weaponCampaign]() {
        if (pendingWeaponSlot == 0) return false;
        const int targetSlot = pendingWeaponSlot;
        pendingWeaponSlot = 0;
        const FpsWeaponDefinition* target = FindFpsWeaponDefinitionForSlot(
                registry,
                targetSlot);
        if (target == nullptr
                || !ActivateWeapon(
                        renderer,
                        registry,
                        settings,
                        target->id,
                        false)) {
            return false;
        }
        BeginFpsWeaponSlotTargetUnholster(state);
        if (weaponCampaign != nullptr) {
            weaponCampaign->activeWeaponId = target->id;
        }
        return true;
    };
    const auto commitPendingUnequip = [this, &renderer, weaponCampaign]() {
        if (!pendingUnequip) return false;
        pendingUnequip = false;
        ResetActiveWeapon(renderer);
        if (weaponCampaign != nullptr) {
            weaponCampaign->activeWeaponId.clear();
        }
        return true;
    };
    if (pendingUnequip
            && (state.activeWeaponId.empty()
                || state.equipState == FpsViewmodelEquipState::Holstered
                || state.equipProgress <= 0.0f)) {
        commitPendingUnequip();
        return;
    }
    if (pendingWeaponSlot != 0
            && (state.activeWeaponId.empty()
                || state.equipState == FpsViewmodelEquipState::Holstered
                || state.equipProgress <= 0.0f)) {
        commitPendingSwitch();
        return;
    }
    if (state.loadState == FpsViewmodelLoadState::Inactive) {
        return;
    }
    const FpsWeaponDefinition* definition = FindFpsWeaponDefinition(
            registry,
            state.activeWeaponId);
    if (definition == nullptr) {
        state.loadState = FpsViewmodelLoadState::Failed;
        state.error = "Active weapon definition disappeared";
        return;
    }
    if (cameraRecoilWeaponId != state.activeWeaponId) {
        ResetFpsCameraRecoil(state.firing.cameraRecoil);
        cameraRecoilWeaponId = state.activeWeaponId;
    }
    state.reloadDefinition = definition->reload;
    state.holsterTransition = ResolveFpsViewmodelHolsterTransition(
            definition->viewmodel.holsterTransition,
            FindFpsViewmodelHolsterTransitionOverride(
                    settings,
                    definition->id));
    if (tuning != nullptr && tuning->holsterTransition != nullptr) {
        state.holsterTransition = *tuning->holsterTransition;
    }
    if (IsFpsWeaponReloading(state)) {
        AdvanceReload(assets, audio, dt);
    } else {
        AdvanceFpsViewmodelEquipTransition(state, dt);
    }
    if (pendingUnequip
            && state.equipState == FpsViewmodelEquipState::Holstered) {
        commitPendingUnequip();
        return;
    }
    if (pendingWeaponSlot != 0
            && state.equipState == FpsViewmodelEquipState::Holstered) {
        commitPendingSwitch();
        return;
    }
    state.firing.definition = ResolveFpsWeaponFiringDefinition(
            definition->firing,
            FindFpsWeaponFiringOverride(settings, definition->id));
    if (tuning != nullptr && tuning->firing != nullptr) {
        state.firing.definition = *tuning->firing;
    }
    state.reloadDefinition = definition->reload;
    if (weaponCampaign != nullptr) {
        PlayerWeaponMagazineState* magazine = FindPlayerWeaponMagazine(
                *weaponCampaign, state.activeWeaponId);
        state.firing.ammunitionEnabled = true;
        if (magazine != nullptr) {
            magazine->loadedRounds = std::clamp(
                    magazine->loadedRounds,
                    0,
                    std::max(1, definition->reload.magazineSize));
            state.firing.loadedRounds = magazine->loadedRounds;
        } else {
            state.firing.loadedRounds = 0;
        }
    } else {
        state.firing.ammunitionEnabled = false;
    }
    AdvanceFpsWeaponFiringRuntime(state.firing, dt);
    if (state.loadState == FpsViewmodelLoadState::Failed) {
        return;
    }
    state.presentation = ResolveFpsViewmodelPresentation(
            definition->viewmodel.presentation,
            FindFpsViewmodelOverride(settings, definition->id));
    if (tuning != nullptr && tuning->presentation != nullptr) {
        state.presentation = *tuning->presentation;
    }
    state.brightnessAdjustment = definition->viewmodel.brightnessAdjustment;
    state.brightnessMultiplier = FpsViewmodelBrightnessMultiplier(
            state.brightnessAdjustment);
    state.materialOverride = definition->viewmodel.materialOverride;
    state.attachment.gripCorrection = ResolveFpsViewmodelGripCorrection(
            definition->viewmodel.attachment.gripCorrection,
            FindFpsViewmodelGripCorrectionOverride(settings, definition->id));
    if (tuning != nullptr && tuning->gripCorrection != nullptr) {
        state.attachment.gripCorrection = *tuning->gripCorrection;
    }
    state.attachment.lighting = ResolveFpsViewmodelAttachmentLighting(
            definition->viewmodel.attachment.lighting,
            FindFpsViewmodelAttachmentLightingOverride(
                    settings,
                    definition->id));
    if (tuning != nullptr && tuning->attachmentLighting != nullptr) {
        state.attachment.lighting = *tuning->attachmentLighting;
    }
    state.attachment.brightnessMultiplier = FpsViewmodelBrightnessMultiplier(
            state.attachment.lighting.brightnessAdjustment);

    const engine::ModelAsset* asset = assets.GetModelAsset(
            state.modelInstance.model);
    if (asset == nullptr) {
        if (assets.HasFailed(state.modelInstance.model)) {
            state.loadState = FpsViewmodelLoadState::Failed;
            state.error = "Failed to load viewmodel model and animations: "
                    + state.resolvedModelPath;
            if (state.attachment.loadState
                    != FpsViewmodelAttachmentLoadState::Failed) {
                state.attachment.loadState =
                        FpsViewmodelAttachmentLoadState::Failed;
                state.attachment.error =
                        "Arms viewmodel failed before the attachment could be evaluated";
            }
        }
        return;
    }
    if (state.loadState == FpsViewmodelLoadState::Pending) {
        state.animationIndex = engine::FindModelAnimationIndex(
                *asset,
                state.animationName.c_str());
        if (state.animationIndex == engine::InvalidModelAnimationIndex) {
            state.loadState = FpsViewmodelLoadState::Failed;
            state.error = "Configured animation '" + state.animationName
                    + "' was not found";
            return;
        }
        const ModelAnimation& animation = asset->animations[state.animationIndex];
        if (!IsModelAnimationValid(asset->model, animation)
                || state.modelInstance.poseFailed
                || (!state.modelInstance.poseReady
                    && !engine::PrepareAnimatedModelInstance(
                            state.modelInstance,
                            *asset))) {
            state.loadState = FpsViewmodelLoadState::Failed;
            state.error = "Viewmodel animation cannot use the model skeleton";
            return;
        }
        state.meshCount = asset->model.meshCount;
        state.boneCount = asset->model.skeleton.boneCount;
        state.triangleCount = 0;
        for (int i = 0; i < asset->model.meshCount; ++i) {
            state.triangleCount += asset->model.meshes[i].triangleCount;
        }
        state.loadState = FpsViewmodelLoadState::Ready;
    }

    if (state.attachment.boneResolvedForModel != state.modelInstance.model) {
        state.attachment.boneResolvedForModel = state.modelInstance.model;
        state.attachment.boneIndex = FindFpsViewmodelBoneIndex(
                asset->model.skeleton.bones,
                asset->model.skeleton.boneCount,
                state.attachment.configuredBoneName);
        state.attachment.resolvedBoneName.clear();
        state.attachment.handPoseValid = false;
        state.attachment.poseSpace = FpsViewmodelBonePoseSpace::Model;
        if (state.attachment.boneIndex < 0) {
            state.attachment.loadState =
                    FpsViewmodelAttachmentLoadState::Failed;
            state.attachment.error = "Configured attachment bone '"
                    + state.attachment.configuredBoneName
                    + "' was not found in the arms skeleton";
        } else {
            state.attachment.resolvedBoneName = asset->model.skeleton.bones[
                    state.attachment.boneIndex].name;
        }
    }

    const engine::ModelAsset* attachmentAsset = assets.GetModelAsset(
            state.attachment.model);
    if (state.attachment.loadState
                    != FpsViewmodelAttachmentLoadState::Failed
            && attachmentAsset == nullptr
            && assets.HasFailed(state.attachment.model)) {
        state.attachment.loadState = FpsViewmodelAttachmentLoadState::Failed;
        state.attachment.error = "Failed to load viewmodel attachment: "
                + state.attachment.resolvedModelPath;
    }
    if (state.attachment.loadState
                    == FpsViewmodelAttachmentLoadState::Pending
            && attachmentAsset != nullptr
            && state.attachment.boneIndex >= 0) {
        state.attachment.meshCount = attachmentAsset->model.meshCount;
        state.attachment.materialCount = attachmentAsset->model.materialCount;
        state.attachment.triangleCount = 0;
        for (int i = 0; i < attachmentAsset->model.meshCount; ++i) {
            state.attachment.triangleCount +=
                    attachmentAsset->model.meshes[i].triangleCount;
        }
        state.attachment.loadState = FpsViewmodelAttachmentLoadState::Ready;
        state.attachment.error.clear();
    }

    state.sourceFrameCursor = AdvanceFpsViewmodelAnimationCursor(
            state.sourceFrameCursor,
            dt,
            definition->viewmodel.sourceFps,
            definition->viewmodel.playbackSpeed,
            definition->viewmodel.firstFrame,
            definition->viewmodel.lastFrame);
    state.raylibFrame = FpsViewmodelCursorToRaylibFrame(
            state.sourceFrameCursor,
            definition->viewmodel.sourceFps);
    Model poseModel = engine::BuildAnimatedModelPoseView(
            *asset,
            state.modelInstance);
    const ModelAnimation& animation = asset->animations[state.animationIndex];
    UpdateModelAnimationEx(
            poseModel,
            animation,
            state.raylibFrame,
            animation,
            state.raylibFrame,
            0.0f);
    state.attachment.handPoseValid = BuildFpsViewmodelBoneModelTransform(
            state.modelInstance.currentPose.data(),
            asset->model.skeleton.bones,
            asset->model.skeleton.boneCount,
            state.attachment.boneIndex,
            state.attachment.poseSpace,
            state.attachment.handModelTransform);
}

bool FpsPlayerRuntime::HandleInput(
        engine::Input& input,
        const FpsWeaponRegistry& registry,
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const SectorCollisionWorld* collisionWorld,
        const SectorMeshRenderer& renderer,
        bool gameplayActive,
        bool mouseLookActive,
        bool uiCaptured,
        const ItemRegistry* itemRegistry,
        ItemCampaignState* itemCampaign)
{
    HandleWeaponSlotInput(
            input,
            registry,
            gameplayActive,
            uiCaptured,
            itemRegistry,
            itemCampaign != nullptr ? &itemCampaign->inventory : nullptr);
    if (itemRegistry != nullptr && itemCampaign != nullptr) {
        HandleReloadInput(
                input,
                assets,
                audio,
                gameplayActive,
                uiCaptured,
                *itemRegistry,
                *itemCampaign);
    }
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, gameplayActive, uiCaptured](engine::InputEvent& event) {
                if (event.key.key != KEY_H) {
                    return;
                }
                if (IsWeaponSwitchInProgress() || IsFpsWeaponReloading(state)) {
                    if (gameplayActive && !uiCaptured) {
                        engine::ConsumeEvent(event);
                    }
                    return;
                }
                if (ToggleFpsViewmodelHolster(
                            state,
                            gameplayActive,
                            uiCaptured)) {
                    engine::ConsumeEvent(event);
                }
            });
    return HandleFireInput(
            input,
            assets,
            audio,
            collisionWorld,
            renderer,
            gameplayActive,
            mouseLookActive,
            uiCaptured,
            itemCampaign != nullptr ? &itemCampaign->weapons : nullptr);
}

bool FpsPlayerRuntime::HandleWeaponSlotInput(
        engine::Input& input,
        const FpsWeaponRegistry& registry,
        bool gameplayActive,
        bool uiCaptured,
        const ItemRegistry* itemRegistry,
        const PlayerInventoryState* inventory)
{
    bool switchRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, &registry, gameplayActive, uiCaptured,
                    itemRegistry, inventory,
                    &switchRequested](engine::InputEvent& event) {
                const int weaponSlot = FpsWeaponSlotFromKey(event.key.key);
                if (weaponSlot == 0 || !gameplayActive || uiCaptured) {
                    return;
                }
                engine::ConsumeEvent(event);
                if (pendingWeaponSlot != 0 || pendingUnequip
                        || IsFpsWeaponReloading(state)) {
                    return;
                }
                const FpsWeaponDefinition* target =
                        FindFpsWeaponDefinitionForSlot(registry, weaponSlot);
                if (target == nullptr || target->id == state.activeWeaponId) {
                    return;
                }
                if (itemRegistry != nullptr && inventory != nullptr
                        && !InventoryOwnsWeapon(
                                *inventory, *itemRegistry, target->id)) {
                    return;
                }
                switchRequested = QueueFpsWeaponSlotSwitch(
                        state,
                        weaponSlot,
                        pendingWeaponSlot);
            });
    return switchRequested;
}

bool FpsPlayerRuntime::HandleReloadInput(
        engine::Input& input,
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        bool gameplayActive,
        bool uiCaptured,
        const ItemRegistry& itemRegistry,
        ItemCampaignState& itemCampaign)
{
    (void)assets;
    (void)audio;
    bool started = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, gameplayActive, uiCaptured, &itemRegistry, &itemCampaign,
                    &started](engine::InputEvent& event) {
                if (event.key.key != KEY_R || !gameplayActive || uiCaptured) {
                    return;
                }
                engine::ConsumeEvent(event);
                if (IsWeaponSwitchInProgress()
                        || IsFpsWeaponReloading(state)
                        || !IsFpsViewmodelReadyForUse(state)
                        || state.activeWeaponId.empty()) {
                    return;
                }
                PlayerWeaponMagazineState* magazine = FindPlayerWeaponMagazine(
                        itemCampaign.weapons, state.activeWeaponId);
                if (magazine == nullptr) return;
                const int magazineSize = std::max(
                        1, state.reloadDefinition.magazineSize);
                magazine->loadedRounds = std::clamp(
                        magazine->loadedRounds, 0, magazineSize);
                const int deficit = magazineSize - magazine->loadedRounds;
                if (deficit <= 0) return;
                const std::uint64_t available = CountInventoryAmmoForWeapon(
                        itemCampaign.inventory,
                        itemRegistry,
                        state.activeWeaponId);
                if (available == 0) {
                    reloadOutOfAmmoRequested = true;
                    return;
                }
                const std::uint64_t requested = std::min<std::uint64_t>(
                        static_cast<std::uint64_t>(deficit), available);
                const std::uint64_t consumed = ConsumeInventoryAmmoForWeapon(
                        itemCampaign.inventory,
                        itemRegistry,
                        state.activeWeaponId,
                        requested);
                if (consumed == 0) {
                    reloadOutOfAmmoRequested = true;
                    return;
                }
                state.reload.loadedRoundsBefore = magazine->loadedRounds;
                state.reload.reservedRounds = static_cast<int>(consumed);
                magazine->loadedRounds += state.reload.reservedRounds;
                state.firing.loadedRounds = magazine->loadedRounds;
                state.firing.ammunitionEnabled = true;
                state.reload.phase = FpsWeaponReloadPhase::Holstering;
                state.reload.waitElapsedSeconds = 0.0f;
                state.reload.totalElapsedSeconds = 0.0f;
                state.reload.totalDurationSeconds =
                        state.holsterTransition.holsterDurationSeconds
                        + state.reloadDefinition.durationSeconds
                        + state.holsterTransition.unholsterDurationSeconds;
                state.equipState = FpsViewmodelEquipState::Holstering;
                started = true;
            });
    return started;
}

bool FpsPlayerRuntime::ConsumeReloadOutOfAmmoRequest()
{
    const bool requested = reloadOutOfAmmoRequested;
    reloadOutOfAmmoRequested = false;
    return requested;
}

bool FpsPlayerRuntime::HandleFireInput(
        engine::Input& input,
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const SectorCollisionWorld* collisionWorld,
        const SectorMeshRenderer& renderer,
        bool gameplayActive,
        bool mouseLookActive,
        bool uiCaptured,
        PlayerWeaponCampaignState* weaponCampaign)
{
    bool acceptedShot = false;
    input.ForEachEvent(
            engine::InputEventType::MouseButtonPressed,
            true,
            [this, &assets, &audio, collisionWorld, &renderer, gameplayActive,
                    mouseLookActive, uiCaptured, weaponCampaign,
                    &acceptedShot](engine::InputEvent& event) {
                if (event.mouseButton.button != MOUSE_BUTTON_LEFT) {
                    return;
                }
                FpsFireRejectReason reason = FpsFireRejectReason::None;
                if (!CanFireFpsWeapon(
                            state,
                            gameplayActive,
                            mouseLookActive,
                            uiCaptured,
                            &reason)) {
                    state.firing.lastRejectReason = reason;
                    if (reason == FpsFireRejectReason::EmptyMagazine) {
                        state.firing.cooldownRemainingSeconds =
                                state.firing.definition.shotIntervalSeconds;
                        audio.PlaySound(
                                assets,
                                state.reloadDefinition.dryFireSound,
                                engine::SoundPlaybackSettings{});
                    }
                    if (gameplayActive && mouseLookActive && !uiCaptured) {
                        engine::ConsumeEvent(event);
                    }
                    return;
                }
                const Camera3D& camera = renderer.RenderCamera();
                const Vector3 direction = Vector3Normalize(
                        Vector3Subtract(camera.target, camera.position));
                FpsShotResult shot;
                shot.accepted = true;
                shot.rayOrigin = camera.position;
                shot.rayDirection = direction;
                if (collisionWorld != nullptr) {
                    const SectorCollisionRayHit hit = collisionWorld->Raycast(
                            camera.position,
                            direction,
                            state.firing.definition.maximumRangeWorld);
                    shot.hit = hit.hit;
                    shot.hitKind = hit.hit
                            ? FpsShotHitKind::SectorSurface
                            : FpsShotHitKind::None;
                    shot.position = hit.position;
                    shot.normal = hit.normal;
                    shot.distance = hit.distance;
                    shot.sectorId = hit.sectorId;
                    shot.lineDefId = hit.lineDefId;
                    shot.sideDefId = hit.sideDefId;
                    shot.neighborSectorId = hit.neighborSectorId;
                    switch (hit.surfaceKind) {
                        case SectorCollisionRaySurfaceKind::Floor:
                            shot.surfaceKind = FpsShotSurfaceKind::Floor; break;
                        case SectorCollisionRaySurfaceKind::Ceiling:
                            shot.surfaceKind = FpsShotSurfaceKind::Ceiling; break;
                        case SectorCollisionRaySurfaceKind::Wall:
                            shot.surfaceKind = FpsShotSurfaceKind::Wall; break;
                        case SectorCollisionRaySurfaceKind::LowerWall:
                            shot.surfaceKind = FpsShotSurfaceKind::LowerWall; break;
                        case SectorCollisionRaySurfaceKind::UpperWall:
                            shot.surfaceKind = FpsShotSurfaceKind::UpperWall; break;
                        case SectorCollisionRaySurfaceKind::None: break;
                    }
                }
                FpsMuzzleEmissionCapture emission;
                if (state.attachment.handPoseValid
                        && state.attachment.loadState
                                == FpsViewmodelAttachmentLoadState::Ready) {
                    const Matrix root = BuildFpsViewmodelAnimatedTransform(
                            camera,
                            state.presentation,
                            state.holsterPose,
                            state.firing.recoil);
                    const Matrix attachment = BuildFpsViewmodelAttachmentTransform(
                            root,
                            state.attachment.handModelTransform,
                            state.attachment.gripCorrection);
                    emission = CaptureFpsMuzzleEmission(
                            BuildFpsViewmodelMuzzleTransform(
                                    attachment,
                                    state.firing.definition.muzzleSocket),
                            camera);
                }
                ApplyFpsWeaponShotEffects(state.firing, shot, emission);
                if (state.firing.ammunitionEnabled) {
                    state.firing.loadedRounds = std::max(
                            0, state.firing.loadedRounds - 1);
                    if (weaponCampaign != nullptr) {
                        PlayerWeaponMagazineState* magazine =
                                FindPlayerWeaponMagazine(
                                        *weaponCampaign,
                                        state.activeWeaponId);
                        if (magazine != nullptr) {
                            magazine->loadedRounds = state.firing.loadedRounds;
                        }
                    }
                }
                acceptedShot = true;
                audio.PlaySound(
                        assets,
                        state.firing.definition.shootSound,
                        engine::SoundPlaybackSettings{
                                1.0f,
                                FpsWeaponShotPitch(
                                        state.firing.shotSequence,
                                        state.firing.randomState),
                                0.0f});
                engine::ConsumeEvent(event);
            });
    return acceptedShot;
}

bool FpsPlayerRuntime::TriggerPreviewShot(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const SectorMeshRenderer& renderer)
{
    FpsFireRejectReason reason = FpsFireRejectReason::None;
    if (!CanFireFpsWeapon(state, true, true, false, &reason)) {
        state.firing.lastRejectReason = reason;
        return false;
    }
    const Camera3D& camera = renderer.RenderCamera();
    const Vector3 direction = Vector3Normalize(
            Vector3Subtract(camera.target, camera.position));
    FpsShotResult shot;
    shot.accepted = true;
    shot.rayOrigin = camera.position;
    shot.rayDirection = direction;
    FpsMuzzleEmissionCapture emission;
    if (state.attachment.handPoseValid
            && state.attachment.loadState
                    == FpsViewmodelAttachmentLoadState::Ready) {
        const Matrix root = BuildFpsViewmodelAnimatedTransform(
                camera,
                state.presentation,
                state.holsterPose,
                state.firing.recoil);
        const Matrix attachment = BuildFpsViewmodelAttachmentTransform(
                root,
                state.attachment.handModelTransform,
                state.attachment.gripCorrection);
        emission = CaptureFpsMuzzleEmission(
                BuildFpsViewmodelMuzzleTransform(
                        attachment,
                        state.firing.definition.muzzleSocket),
                camera);
    }
    ApplyFpsWeaponShotEffects(state.firing, shot, emission);
    audio.PlaySound(
            assets,
            state.firing.definition.shootSound,
            engine::SoundPlaybackSettings{
                    1.0f,
                    FpsWeaponShotPitch(
                            state.firing.shotSequence,
                            state.firing.randomState),
                    0.0f});
    return true;
}

bool FpsPlayerRuntime::TriggerPreviewReload()
{
    if (IsWeaponSwitchInProgress()
            || IsFpsWeaponReloading(state)
            || !IsFpsViewmodelReadyForUse(state)) {
        return false;
    }
    state.reload.loadedRoundsBefore = state.firing.loadedRounds;
    state.reload.reservedRounds = 0;
    state.reload.phase = FpsWeaponReloadPhase::Holstering;
    state.reload.waitElapsedSeconds = 0.0f;
    state.reload.totalElapsedSeconds = 0.0f;
    state.reload.totalDurationSeconds =
            state.holsterTransition.holsterDurationSeconds
            + state.reloadDefinition.durationSeconds
            + state.holsterTransition.unholsterDurationSeconds;
    state.equipState = FpsViewmodelEquipState::Holstering;
    return true;
}

void FpsPlayerRuntime::UpdateTransformsAndLight(
        SectorMeshRenderer& renderer,
        const SectorCollisionWorld* collisionWorld)
{
    state.firing.viewmodelRootTransform = BuildFpsViewmodelAnimatedTransform(
            renderer.RenderCamera(),
            state.presentation,
            state.holsterPose,
            state.firing.recoil);
    state.firing.viewmodelRootTransformValid = !state.activeWeaponId.empty();
    state.attachment.attachmentWorldTransformValid =
            state.firing.viewmodelRootTransformValid
            && state.attachment.handPoseValid
            && state.attachment.loadState
                    == FpsViewmodelAttachmentLoadState::Ready;
    if (state.attachment.attachmentWorldTransformValid) {
        state.attachment.attachmentWorldTransform =
                BuildFpsViewmodelAttachmentTransform(
                        state.firing.viewmodelRootTransform,
                        state.attachment.handModelTransform,
                        state.attachment.gripCorrection);
        state.firing.muzzleWorldTransform = BuildFpsViewmodelMuzzleTransform(
                state.attachment.attachmentWorldTransform,
                state.firing.definition.muzzleSocket);
        state.firing.muzzleWorldTransformValid = true;
    } else {
        state.firing.muzzleWorldTransformValid = false;
    }

    SectorPreviewDynamicPointLightSource source;
    const float intensity = FpsMuzzleLightCurrentIntensity(state.firing.light);
    if (intensity > 0.0f && state.firing.emission.valid) {
        const Matrix lightTransform = ResolveFpsMuzzleEmissionTransform(
                state.firing.emission,
                renderer.RenderCamera());
        const Vector3 position = Vector3Transform(Vector3{}, lightTransform);
        source.lightId = -1;
        source.ownerSectorId = collisionWorld != nullptr
                ? collisionWorld->FindSectorContainingPoint(
                        Vector2{position.x, position.z})
                : 0;
        source.light.lightId = source.lightId;
        source.light.kind = SectorPreviewDynamicLightKind::Point;
        source.light.position = position;
        source.light.color = engine::SrgbColorBytesToLinearSceneRgb(
                state.firing.light.color);
        source.light.radius = state.firing.light.radiusWorld;
        source.light.intensity = intensity;
        renderer.SetRuntimePointLight(&source);
    } else {
        renderer.SetRuntimePointLight(nullptr);
    }
}

void FpsPlayerRuntime::Render(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer,
        const SectorTopologyMap& map,
        const SectorRuntimeObjectState& runtimeObjects,
        int preferredSectorId)
{
    if (!IsFpsViewmodelRenderable(state)
            || !state.firing.viewmodelRootTransformValid) {
        state.attachment.attachmentWorldTransformValid = false;
        return;
    }
    const engine::ModelAsset* asset = assets.GetModelAsset(
            state.modelInstance.model);
    if (asset == nullptr) {
        return;
    }
    const engine::ModelAsset* attachmentAsset = nullptr;
    if (IsFpsViewmodelAttachmentRenderable(state)) {
        attachmentAsset = assets.GetModelAsset(state.attachment.model);
    }
    Camera3D camera = renderer.RenderCamera();
    camera.fovy = state.presentation.verticalFovDegrees;
    const double previousNear = rlGetCullDistanceNear();
    const double previousFar = rlGetCullDistanceFar();
    rlSetClipPlanes(0.01, previousFar);
    state.environmentExposure = ComputeSectorModelEnvironmentExposure(
            map,
            preferredSectorId);
    const BakedObjectLightingVerticalSample ambientLighting =
            SampleBakedObjectLightingVertical(
                    runtimeObjects.objectLightProbes,
                    camera.position,
                    preferredSectorId,
                    &map);
    const SectorViewmodelLightingContext viewmodelLighting{
            state.environmentExposure,
            state.brightnessMultiplier,
            state.materialOverride.enabled,
            state.materialOverride.metallicFactor,
            state.materialOverride.roughnessFactor,
            state.materialOverride.useMetallicRoughnessTexture};
    const SectorViewmodelLightingContext attachmentLighting{
            state.environmentExposure,
            state.attachment.brightnessMultiplier,
            state.attachment.lighting.materialOverride.enabled,
            state.attachment.lighting.materialOverride.metallicFactor,
            state.attachment.lighting.materialOverride.roughnessFactor,
            state.attachment.lighting.materialOverride
                    .useMetallicRoughnessTexture};
    // Draw the additive flash first so the subsequently drawn gun can occlude
    // it with the isolated viewmodel depth buffer. The captured fire-time
    // transform keeps recoil from clipping the flash origin.
    DrawFpsMuzzleFlash(muzzleFlashRenderResources, state.firing, camera);
    renderer.DrawViewmodel(
            assets,
            *asset,
            state.modelInstance,
            camera,
            state.firing.viewmodelRootTransform,
            attachmentAsset,
            state.attachment.attachmentWorldTransform,
            preferredSectorId,
            !runtimeObjects.objectLightProbes.probes.empty(),
            ambientLighting,
            viewmodelLighting,
            attachmentLighting);
    rlSetClipPlanes(previousNear, previousFar);
}

void FpsPlayerRuntime::RenderHud(
        Rectangle playableViewport,
        const FpsWeaponRegistry& registry,
        const engine::FontAsset* font,
        const Health* health,
        const PlayerStamina* stamina,
        std::uint64_t reserveRounds,
        bool showAmmo,
        const PlayerOxygen* oxygen,
        float oxygenAlpha) const
{
    const int displayedLoaded = IsFpsWeaponReloading(state)
            ? state.reload.loadedRoundsBefore
            : state.firing.loadedRounds;
    DrawFpsHud(FpsHudContext{
            true,
            playableViewport,
            registry,
            state,
            font,
            health,
            stamina,
            oxygen,
            oxygenAlpha,
            displayedLoaded,
            reserveRounds,
            showAmmo});
}

} // namespace game
