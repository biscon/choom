#include "game/FpsPlayerRuntime.h"

#include "engine/render/ColorTransfer.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorLightmap.h"
#include "game/FpsHudRenderer.h"
#include "game/FpsViewmodelEffectsRenderer.h"

#include <raymath.h>
#include <rlgl.h>

namespace game {

void FpsPlayerRuntime::Begin(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        const char* scopeName)
{
    End(assets, renderer);
    if (!LoadFpsMuzzleFlashRenderResources(muzzleFlashRenderResources)) {
        TraceLog(LOG_WARNING, "MUZZLE FLASH: HDR shader unavailable; visible flash disabled");
    }
    LoadWeapon(
            assets,
            renderer,
            registry,
            settings,
            registry.initialWeaponId,
            scopeName);
}

void FpsPlayerRuntime::UnloadActiveWeapon(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer)
{
    if (!engine::IsNull(state.assetScope)) {
        assets.UnloadScope(state.assetScope);
    }
    renderer.SetRuntimePointLight(nullptr);
    ResetFpsViewmodelRuntime(state);
    cameraRecoilWeaponId.clear();
}

bool FpsPlayerRuntime::LoadWeapon(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        std::string_view weaponId,
        const char* scopeName)
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
    const auto failAttachment = [this]() {
        if (state.attachment.loadState
                != FpsViewmodelAttachmentLoadState::Failed) {
            state.attachment.loadState =
                    FpsViewmodelAttachmentLoadState::Failed;
            state.attachment.error =
                    "Arms viewmodel failed before the attachment could be evaluated";
        }
    };
    state.activeWeaponId = definition->id;
    cameraRecoilWeaponId = definition->id;
    state.resolvedModelPath = ResolveSectorAssetPath(
            definition->viewmodel.modelPath);
    state.animationName = definition->viewmodel.idleAnimation;
    state.brightnessAdjustment = definition->viewmodel.brightnessAdjustment;
    state.brightnessMultiplier = FpsViewmodelBrightnessMultiplier(
            state.brightnessAdjustment);
    state.materialOverride = definition->viewmodel.materialOverride;
    state.attachment.resolvedModelPath = ResolveSectorAssetPath(
            definition->viewmodel.attachment.modelPath);
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
    state.assetScope = assets.CreateScope(
            scopeName != nullptr ? scopeName : "fps_player_viewmodel");
    if (engine::IsNull(state.assetScope)) {
        state.loadState = FpsViewmodelLoadState::Failed;
        state.error = "Could not create the FPS viewmodel asset scope";
        failAttachment();
        return false;
    }
    state.modelInstance.model = assets.RequestModel(
            state.assetScope,
            ("fps_viewmodel_" + definition->id).c_str(),
            state.resolvedModelPath.c_str(),
            engine::ModelLoad_Animations);
    if (engine::IsNull(state.modelInstance.model)) {
        state.loadState = FpsViewmodelLoadState::Failed;
        state.error = "Could not request FPS viewmodel asset: "
                + state.resolvedModelPath;
        failAttachment();
        return false;
    }
    state.attachment.model = assets.RequestModel(
            state.assetScope,
            ("fps_viewmodel_attachment_" + definition->id).c_str(),
            state.attachment.resolvedModelPath.c_str(),
            engine::ModelLoad_None);
    if (engine::IsNull(state.attachment.model)) {
        state.attachment.loadState =
                FpsViewmodelAttachmentLoadState::Failed;
        state.attachment.error = "Could not request FPS viewmodel attachment: "
                + state.attachment.resolvedModelPath;
    } else {
        state.attachment.loadState =
                FpsViewmodelAttachmentLoadState::Pending;
    }
    state.loadState = FpsViewmodelLoadState::Pending;
    return true;
}

bool FpsPlayerRuntime::SelectWeapon(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        std::string_view weaponId,
        const char* scopeName)
{
    UnloadActiveWeapon(assets, renderer);
    return LoadWeapon(
            assets,
            renderer,
            registry,
            settings,
            weaponId,
            scopeName);
}

void FpsPlayerRuntime::End(
        engine::AssetManager& assets,
        SectorMeshRenderer& renderer)
{
    UnloadActiveWeapon(assets, renderer);
    UnloadFpsMuzzleFlashRenderResources(muzzleFlashRenderResources);
}

void FpsPlayerRuntime::Update(
        engine::AssetManager& assets,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        float dt,
        const FpsPlayerRuntimeTuning* tuning)
{
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
    state.holsterTransition = ResolveFpsViewmodelHolsterTransition(
            definition->viewmodel.holsterTransition,
            FindFpsViewmodelHolsterTransitionOverride(
                    settings,
                    definition->id));
    if (tuning != nullptr && tuning->holsterTransition != nullptr) {
        state.holsterTransition = *tuning->holsterTransition;
    }
    AdvanceFpsViewmodelEquipTransition(state, dt);
    state.firing.definition = ResolveFpsWeaponFiringDefinition(
            definition->firing,
            FindFpsWeaponFiringOverride(settings, definition->id));
    if (tuning != nullptr && tuning->firing != nullptr) {
        state.firing.definition = *tuning->firing;
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
                || !engine::PrepareAnimatedModelInstance(
                        state.modelInstance,
                        *asset)) {
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
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const SectorCollisionWorld* collisionWorld,
        const SectorMeshRenderer& renderer,
        bool gameplayActive,
        bool mouseLookActive,
        bool uiCaptured)
{
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, gameplayActive, uiCaptured](engine::InputEvent& event) {
                if (event.key.key != KEY_H) {
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
            uiCaptured);
}

bool FpsPlayerRuntime::HandleFireInput(
        engine::Input& input,
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const SectorCollisionWorld* collisionWorld,
        const SectorMeshRenderer& renderer,
        bool gameplayActive,
        bool mouseLookActive,
        bool uiCaptured)
{
    bool acceptedShot = false;
    input.ForEachEvent(
            engine::InputEventType::MouseButtonPressed,
            true,
            [this, &assets, &audio, collisionWorld, &renderer, gameplayActive,
                    mouseLookActive, uiCaptured, &acceptedShot](engine::InputEvent& event) {
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
        const PlayerStamina* stamina) const
{
    DrawFpsHud(FpsHudContext{
            true,
            playableViewport,
            registry,
            state,
            font,
            health,
            stamina});
}

} // namespace game
