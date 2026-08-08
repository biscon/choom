#pragma once

#include "engine/components/AnimatedModel.h"
#include "game/FpsWeaponRegistry.h"

#include <raylib.h>

#include <string>
#include <string_view>

namespace game {

enum class FpsViewmodelLoadState { Inactive, Pending, Ready, Failed };
enum class FpsViewmodelAttachmentLoadState { Inactive, Pending, Ready, Failed };
enum class FpsViewmodelBonePoseSpace { Unknown, Local, Model };

struct FpsViewmodelAttachmentRuntimeState {
    FpsViewmodelAttachmentLoadState loadState =
            FpsViewmodelAttachmentLoadState::Inactive;
    engine::ModelHandle model = engine::NullModelHandle();
    engine::ModelHandle boneResolvedForModel = engine::NullModelHandle();
    std::string resolvedModelPath;
    std::string configuredBoneName;
    std::string resolvedBoneName;
    int boneIndex = -1;
    FpsViewmodelBonePoseSpace poseSpace = FpsViewmodelBonePoseSpace::Unknown;
    FpsViewmodelGripCorrection gripCorrection;
    FpsViewmodelAttachmentLighting lightingDefaults;
    FpsViewmodelAttachmentLighting lighting;
    float brightnessMultiplier = 1.0f;
    Matrix handModelTransform = {};
    Matrix pistolWorldTransform = {};
    bool handPoseValid = false;
    bool pistolWorldTransformValid = false;
    int meshCount = 0;
    int triangleCount = 0;
    int materialCount = 0;
    std::string error;
};

struct FpsViewmodelRuntimeState {
    FpsViewmodelLoadState loadState = FpsViewmodelLoadState::Inactive;
    std::string activeWeaponId;
    std::string resolvedModelPath;
    std::string animationName;
    engine::AssetScopeHandle assetScope = engine::NullAssetScopeHandle();
    engine::AnimatedModelInstance modelInstance;
    uint32_t animationIndex = engine::InvalidModelAnimationIndex;
    float sourceFrameCursor = 0.0f;
    float raylibFrame = 0.0f;
    bool holstered = false;
    int meshCount = 0;
    int triangleCount = 0;
    int boneCount = 0;
    FpsViewmodelPresentation presentation;
    float brightnessAdjustment = 0.0f;
    float brightnessMultiplier = 1.0f;
    FpsViewmodelMaterialOverride materialOverride;
    float environmentExposure = 0.15f;
    FpsViewmodelAttachmentRuntimeState attachment;
    std::string error;
};

void ResetFpsViewmodelRuntime(FpsViewmodelRuntimeState& state);
bool ToggleFpsViewmodelHolster(
        FpsViewmodelRuntimeState& state,
        bool preview3DActive,
        bool inputSuppressed);
bool IsFpsViewmodelRenderable(const FpsViewmodelRuntimeState& state);
bool IsFpsViewmodelAttachmentRenderable(const FpsViewmodelRuntimeState& state);
float AdvanceFpsViewmodelAnimationCursor(
        float cursor,
        float deltaSeconds,
        float sourceFps,
        float playbackSpeed,
        int firstFrame,
        int lastFrame);
float FpsViewmodelCursorToSeconds(float cursor, float sourceFps);
float FpsViewmodelCursorToRaylibFrame(float cursor, float sourceFps);
float FpsViewmodelBrightnessMultiplier(float brightnessAdjustment);

struct FpsViewmodelCameraBasis {
    Vector3 right;
    Vector3 up;
    Vector3 forward;
};

FpsViewmodelCameraBasis BuildFpsViewmodelCameraBasis(const Camera3D& camera);
Vector3 TransformFpsViewmodelLocalPosition(const Camera3D& camera, Vector3 localPosition);
Matrix BuildFpsViewmodelTransform(
        const Camera3D& camera,
        const FpsViewmodelPresentation& presentation);
Matrix BuildFpsViewmodelGripCorrectionTransform(
        const FpsViewmodelGripCorrection& correction);
int FindFpsViewmodelBoneIndex(
        const BoneInfo* bones,
        int boneCount,
        std::string_view boneName);
bool BuildFpsViewmodelBoneModelTransform(
        const Transform* pose,
        const BoneInfo* bones,
        int boneCount,
        int boneIndex,
        FpsViewmodelBonePoseSpace poseSpace,
        Matrix& outTransform);
Matrix BuildFpsViewmodelAttachmentTransform(
        Matrix viewmodelRoot,
        Matrix handModelTransform,
        const FpsViewmodelGripCorrection& gripCorrection);

} // namespace game
