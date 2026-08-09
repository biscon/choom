#pragma once

#include "engine/components/AnimatedModel.h"
#include "game/FpsWeaponRegistry.h"

#include <raylib.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace game {

enum class FpsViewmodelLoadState { Inactive, Pending, Ready, Failed };
enum class FpsViewmodelAttachmentLoadState { Inactive, Pending, Ready, Failed };
enum class FpsViewmodelBonePoseSpace { Unknown, Local, Model };
enum class FpsViewmodelEquipState {
    Holstered,
    Unholstering,
    Equipped,
    Holstering
};

enum class FpsShotSurfaceKind { None, Floor, Ceiling, Wall, LowerWall, UpperWall };
enum class FpsFireRejectReason {
    None,
    NotInGameplay3D,
    MouseInputInactive,
    UiCaptured,
    NoActiveWeapon,
    WeaponNotReady,
    Cooldown
};

struct FpsShotResult {
    bool accepted = false;
    bool hit = false;
    Vector3 rayOrigin{};
    Vector3 rayDirection{};
    Vector3 position{};
    Vector3 normal{};
    float distance = 0.0f;
    FpsShotSurfaceKind surfaceKind = FpsShotSurfaceKind::None;
    int sectorId = 0;
    int lineDefId = 0;
    int sideDefId = 0;
    int neighborSectorId = 0;
};

struct FpsRecoilRuntimeState {
    Vector3 translation{};
    Vector3 translationVelocity{};
    Vector3 rotationDegrees{};
    Vector3 rotationVelocityDegrees{};
};

struct FpsCameraRecoilRuntimeState {
    Vector3 rotationDegrees{};
    Vector3 rotationVelocityDegrees{};
    Vector3 lastKickDegrees{};
    uint32_t randomState = 0xa511e9b3u;
};

struct FpsMuzzleFlashLobe {
    bool visibilityAnchor = false;
    float azimuthRadians = 0.0f;
    float forwardComponent = 0.0f;
    float lengthScale = 1.0f;
    float widthScale = 1.0f;
};

struct FpsMuzzleFlashShape {
    uint32_t seed = 0;
    int lobeCount = 0;
    float phaseRadians = 0.0f;
    float overallScale = 1.0f;
    float dominantLengthScale = 1.0f;
    float dominantWidthScale = 1.0f;
    std::array<FpsMuzzleFlashLobe, MaxFpsMuzzleFlashLobes> lobes{};
};

struct FpsMuzzleEmissionCapture {
    bool valid = false;
    Matrix cameraLocalTransform = {};
};

struct FpsMuzzleFlashRuntimeState {
    bool active = false;
    float ageSeconds = 0.0f;
    float lifetimeSeconds = 0.0f;
    float sizeWorld = 0.0f;
    Color coreColor{};
    Color hotColor{};
    Color warmColor{};
    Color edgeColor{};
    float edgeSoftness = 0.35f;
    float irregularity = 0.0f;
    float forwardStretch = 1.0f;
    float rearSuppression = 0.0f;
    FpsMuzzleFlashShape shape;
};

struct FpsMuzzleLightRuntimeState {
    bool active = false;
    float ageSeconds = 0.0f;
    float lifetimeSeconds = 0.0f;
    float intensity = 0.0f;
    float radiusWorld = 0.0f;
    float decayExponent = 1.0f;
    Color color{};
};

struct FpsWeaponFiringRuntimeState {
    FpsWeaponFiringDefinition definition;
    float cooldownRemainingSeconds = 0.0f;
    uint32_t randomState = 0x6d2b79f5u;
    uint64_t shotSequence = 0;
    FpsFireRejectReason lastRejectReason = FpsFireRejectReason::None;
    FpsShotResult lastShot;
    bool hasLastShot = false;
    FpsRecoilRuntimeState recoil;
    FpsCameraRecoilRuntimeState cameraRecoil;
    FpsMuzzleFlashRuntimeState flash;
    FpsMuzzleLightRuntimeState light;
    FpsMuzzleEmissionCapture emission;
    Matrix viewmodelRootTransform = {};
    Matrix muzzleWorldTransform = {};
    bool viewmodelRootTransformValid = false;
    bool muzzleWorldTransformValid = false;
};

struct FpsViewmodelHolsterPose {
    float hiddenAmount = 0.0f;
    Vector3 translation{0.0f, 0.0f, 0.0f};
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
};

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
    FpsViewmodelEquipState equipState = FpsViewmodelEquipState::Holstered;
    float equipProgress = 0.0f;
    FpsViewmodelHolsterTransition holsterTransition;
    FpsViewmodelHolsterPose holsterPose;
    int meshCount = 0;
    int triangleCount = 0;
    int boneCount = 0;
    FpsViewmodelPresentation presentation;
    float brightnessAdjustment = 0.0f;
    float brightnessMultiplier = 1.0f;
    FpsViewmodelMaterialOverride materialOverride;
    float environmentExposure = 0.15f;
    FpsViewmodelAttachmentRuntimeState attachment;
    FpsWeaponFiringRuntimeState firing;
    std::string error;
};

void ResetFpsViewmodelRuntime(FpsViewmodelRuntimeState& state);
bool ToggleFpsViewmodelHolster(
        FpsViewmodelRuntimeState& state,
        bool preview3DActive,
        bool inputSuppressed);
void AdvanceFpsViewmodelEquipTransition(
        FpsViewmodelRuntimeState& state,
        float deltaSeconds);
bool IsFpsViewmodelReadyForUse(const FpsViewmodelRuntimeState& state);
bool IsFpsViewmodelPresentationVisible(const FpsViewmodelRuntimeState& state);
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
FpsViewmodelHolsterPose EvaluateFpsViewmodelHolsterPose(
        const FpsViewmodelHolsterTransition& transition,
        float equipProgress);
Matrix BuildFpsViewmodelAnimatedTransform(
        const Camera3D& camera,
        const FpsViewmodelPresentation& presentation,
        const FpsViewmodelHolsterPose& holsterPose);
Matrix BuildFpsViewmodelAnimatedTransform(
        const Camera3D& camera,
        const FpsViewmodelPresentation& presentation,
        const FpsViewmodelHolsterPose& holsterPose,
        const FpsRecoilRuntimeState& recoil);
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
Matrix BuildFpsViewmodelMuzzleTransform(
        Matrix pistolWorldTransform,
        const FpsWeaponMuzzleSocketDefinition& socket);
FpsMuzzleEmissionCapture CaptureFpsMuzzleEmission(
        Matrix muzzleWorldTransform,
        const Camera3D& camera);
Matrix ResolveFpsMuzzleEmissionTransform(
        const FpsMuzzleEmissionCapture& capture,
        const Camera3D& camera);
FpsMuzzleFlashShape GenerateFpsMuzzleFlashShape(
        const FpsWeaponMuzzleFlashDefinition& definition,
        uint32_t seed);
bool CanFireFpsWeapon(
        const FpsViewmodelRuntimeState& state,
        bool preview3DGameplay,
        bool mouseInputActive,
        bool uiCaptured,
        FpsFireRejectReason* outReason = nullptr);
void AdvanceFpsWeaponFiringRuntime(
        FpsWeaponFiringRuntimeState& state,
        float deltaSeconds);
void ApplyFpsWeaponShotEffects(
        FpsWeaponFiringRuntimeState& state,
        const FpsShotResult& shot,
        const FpsMuzzleEmissionCapture& emission);
void ResetFpsCameraRecoil(FpsCameraRecoilRuntimeState& state);
Vector3 SampleFpsCameraRecoilKickDegrees(
        const FpsWeaponCameraRecoilDefinition& definition,
        uint32_t& randomState);
void ApplyFpsCameraRecoilImpulse(
        FpsCameraRecoilRuntimeState& state,
        const FpsWeaponCameraRecoilDefinition& definition);
void AdvanceFpsCameraRecoil(
        FpsCameraRecoilRuntimeState& state,
        const FpsWeaponCameraRecoilDefinition& definition,
        float deltaSeconds);
float FpsMuzzleLightCurrentIntensity(
        const FpsMuzzleLightRuntimeState& state);
float FpsWeaponShotPitch(uint64_t shotSequence, uint32_t randomState);

} // namespace game
