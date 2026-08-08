#include "game/FpsViewmodel.h"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace game {

void ResetFpsViewmodelRuntime(FpsViewmodelRuntimeState& state)
{
    state = {};
}

bool ToggleFpsViewmodelHolster(FpsViewmodelRuntimeState& state, bool preview3DActive, bool inputSuppressed)
{
    if (!preview3DActive || inputSuppressed || state.activeWeaponId.empty()) {
        return false;
    }
    switch (state.equipState) {
        case FpsViewmodelEquipState::Holstered:
        case FpsViewmodelEquipState::Holstering:
            state.equipState = FpsViewmodelEquipState::Unholstering;
            break;
        case FpsViewmodelEquipState::Unholstering:
        case FpsViewmodelEquipState::Equipped:
            state.equipState = FpsViewmodelEquipState::Holstering;
            break;
    }
    return true;
}

void AdvanceFpsViewmodelEquipTransition(
        FpsViewmodelRuntimeState& state,
        float deltaSeconds)
{
    const FpsViewmodelHolsterTransition transition =
            ClampFpsViewmodelHolsterTransition(state.holsterTransition);
    state.holsterTransition = transition;
    state.equipProgress = std::isfinite(state.equipProgress)
            ? std::clamp(state.equipProgress, 0.0f, 1.0f)
            : 1.0f;
    const float dt = std::isfinite(deltaSeconds)
            ? std::max(0.0f, deltaSeconds)
            : 0.0f;

    switch (state.equipState) {
        case FpsViewmodelEquipState::Holstered:
            state.equipProgress = 0.0f;
            break;
        case FpsViewmodelEquipState::Unholstering:
            state.equipProgress = std::min(
                    1.0f,
                    state.equipProgress
                            + dt / transition.unholsterDurationSeconds);
            if (state.equipProgress >= 1.0f) {
                state.equipProgress = 1.0f;
                state.equipState = FpsViewmodelEquipState::Equipped;
            }
            break;
        case FpsViewmodelEquipState::Equipped:
            state.equipProgress = 1.0f;
            break;
        case FpsViewmodelEquipState::Holstering:
            state.equipProgress = std::max(
                    0.0f,
                    state.equipProgress
                            - dt / transition.holsterDurationSeconds);
            if (state.equipProgress <= 0.0f) {
                state.equipProgress = 0.0f;
                state.equipState = FpsViewmodelEquipState::Holstered;
            }
            break;
    }
    state.holsterPose = EvaluateFpsViewmodelHolsterPose(
            transition,
            state.equipProgress);
}

bool IsFpsViewmodelReadyForUse(const FpsViewmodelRuntimeState& state)
{
    return !state.activeWeaponId.empty()
            && state.equipState == FpsViewmodelEquipState::Equipped
            && state.equipProgress >= 1.0f;
}

bool IsFpsViewmodelPresentationVisible(const FpsViewmodelRuntimeState& state)
{
    return state.equipState != FpsViewmodelEquipState::Holstered;
}

bool IsFpsViewmodelRenderable(const FpsViewmodelRuntimeState& state)
{
    return state.loadState == FpsViewmodelLoadState::Ready
            && IsFpsViewmodelPresentationVisible(state)
            && state.modelInstance.poseReady
            && !state.modelInstance.poseFailed
            && state.animationIndex != engine::InvalidModelAnimationIndex;
}

bool IsFpsViewmodelAttachmentRenderable(const FpsViewmodelRuntimeState& state)
{
    return IsFpsViewmodelRenderable(state)
            && state.attachment.loadState
                    == FpsViewmodelAttachmentLoadState::Ready
            && state.attachment.boneIndex >= 0
            && state.attachment.handPoseValid;
}

float AdvanceFpsViewmodelAnimationCursor(float cursor, float dt, float fps, float speed, int first, int last)
{
    const float loopLength = static_cast<float>(last - first);
    if (loopLength <= 0.0f || fps <= 0.0f || speed <= 0.0f) return 0.0f;
    cursor += std::max(0.0f, dt) * fps * speed;
    cursor = std::fmod(cursor, loopLength);
    return cursor < 0.0f ? cursor + loopLength : cursor;
}

float FpsViewmodelCursorToSeconds(float cursor, float fps) { return fps > 0.0f ? cursor / fps : 0.0f; }

float FpsViewmodelCursorToRaylibFrame(float cursor, float fps)
{
    return FpsViewmodelCursorToSeconds(cursor, fps) * engine::GltfAnimationFramesPerSecond;
}

float FpsViewmodelBrightnessMultiplier(float adjustment)
{
    if (!std::isfinite(adjustment)) return 1.0f;
    return 1.0f + std::clamp(adjustment, -1.0f, 1.0f);
}

FpsViewmodelCameraBasis BuildFpsViewmodelCameraBasis(const Camera3D& camera)
{
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    if (Vector3LengthSqr(forward) <= 0.000001f) forward = {0.0f, 0.0f, 1.0f};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    if (Vector3LengthSqr(right) <= 0.000001f) right = {1.0f, 0.0f, 0.0f};
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
    return {right, up, forward};
}

namespace {

float SmootherStep01(float value)
{
    const float t = std::isfinite(value)
            ? std::clamp(value, 0.0f, 1.0f)
            : 0.0f;
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

Matrix BuildFpsViewmodelCameraTransform(const Camera3D& camera)
{
    const FpsViewmodelCameraBasis basis = BuildFpsViewmodelCameraBasis(camera);
    Matrix result = MatrixIdentity();
    result.m0 = basis.right.x; result.m1 = basis.right.y; result.m2 = basis.right.z;
    result.m4 = basis.up.x; result.m5 = basis.up.y; result.m6 = basis.up.z;
    result.m8 = basis.forward.x; result.m9 = basis.forward.y; result.m10 = basis.forward.z;
    result.m12 = camera.position.x; result.m13 = camera.position.y; result.m14 = camera.position.z;
    return result;
}

} // namespace

Vector3 TransformFpsViewmodelLocalPosition(const Camera3D& camera, Vector3 local)
{
    const FpsViewmodelCameraBasis basis = BuildFpsViewmodelCameraBasis(camera);
    Vector3 result = camera.position;
    result = Vector3Add(result, Vector3Scale(basis.right, local.x));
    result = Vector3Add(result, Vector3Scale(basis.up, local.y));
    result = Vector3Add(result, Vector3Scale(basis.forward, local.z));
    return result;
}

Matrix BuildFpsViewmodelTransform(const Camera3D& camera, const FpsViewmodelPresentation& presentation)
{
    const Matrix scale = MatrixScale(presentation.scale, presentation.scale, presentation.scale);
    const Matrix rotation = MatrixRotateXYZ({
            presentation.rotationDegrees.x * DEG2RAD,
            presentation.rotationDegrees.y * DEG2RAD,
            presentation.rotationDegrees.z * DEG2RAD});
    const Matrix translation = MatrixTranslate(
            presentation.position.x, presentation.position.y, presentation.position.z);
    return MatrixMultiply(scale, MatrixMultiply(rotation, MatrixMultiply(
            translation,
            BuildFpsViewmodelCameraTransform(camera))));
}

FpsViewmodelHolsterPose EvaluateFpsViewmodelHolsterPose(
        const FpsViewmodelHolsterTransition& transition,
        float equipProgress)
{
    const float progress = std::isfinite(equipProgress)
            ? std::clamp(equipProgress, 0.0f, 1.0f)
            : 1.0f;
    FpsViewmodelHolsterPose result;
    result.hiddenAmount = SmootherStep01(1.0f - progress);
    result.translation = Vector3Scale(
            transition.hiddenTranslation,
            result.hiddenAmount);
    if (result.hiddenAmount <= 0.0f) {
        result.rotation = QuaternionIdentity();
    } else {
        const Matrix hiddenRotation = MatrixRotateXYZ({
                transition.hiddenRotationDegrees.x * DEG2RAD,
                transition.hiddenRotationDegrees.y * DEG2RAD,
                transition.hiddenRotationDegrees.z * DEG2RAD});
        const Quaternion target = QuaternionNormalize(
                QuaternionFromMatrix(hiddenRotation));
        result.rotation = result.hiddenAmount >= 1.0f
                ? target
                : QuaternionNormalize(QuaternionSlerp(
                        QuaternionIdentity(),
                        target,
                        result.hiddenAmount));
    }
    return result;
}

Matrix BuildFpsViewmodelAnimatedTransform(
        const Camera3D& camera,
        const FpsViewmodelPresentation& presentation,
        const FpsViewmodelHolsterPose& holsterPose)
{
    return BuildFpsViewmodelAnimatedTransform(
            camera,
            presentation,
            holsterPose,
            FpsRecoilRuntimeState{});
}

Matrix BuildFpsViewmodelAnimatedTransform(
        const Camera3D& camera,
        const FpsViewmodelPresentation& presentation,
        const FpsViewmodelHolsterPose& holsterPose,
        const FpsRecoilRuntimeState& recoil)
{
    if (holsterPose.hiddenAmount <= 0.0f
            && Vector3LengthSqr(recoil.translation) <= 0.0f
            && Vector3LengthSqr(recoil.rotationDegrees) <= 0.0f) {
        return BuildFpsViewmodelTransform(camera, presentation);
    }
    const Matrix scale = MatrixScale(
            presentation.scale,
            presentation.scale,
            presentation.scale);
    const Matrix readyRotation = MatrixRotateXYZ({
            presentation.rotationDegrees.x * DEG2RAD,
            presentation.rotationDegrees.y * DEG2RAD,
            presentation.rotationDegrees.z * DEG2RAD});
    const Matrix proceduralRotation = QuaternionToMatrix(
            QuaternionNormalize(holsterPose.rotation));
    const Matrix recoilRotation = MatrixRotateXYZ({
            recoil.rotationDegrees.x * DEG2RAD,
            recoil.rotationDegrees.y * DEG2RAD,
            recoil.rotationDegrees.z * DEG2RAD});
    const Matrix readyTranslation = MatrixTranslate(
            presentation.position.x,
            presentation.position.y,
            presentation.position.z);
    const Matrix proceduralTranslation = MatrixTranslate(
            holsterPose.translation.x,
            holsterPose.translation.y,
            holsterPose.translation.z);
    const Matrix recoilTranslation = MatrixTranslate(
            recoil.translation.x,
            recoil.translation.y,
            recoil.translation.z);
    return MatrixMultiply(
            scale,
            MatrixMultiply(
                    readyRotation,
                    MatrixMultiply(
                            proceduralRotation,
                            MatrixMultiply(
                                    recoilRotation,
                                    MatrixMultiply(
                                            readyTranslation,
                                            MatrixMultiply(
                                                    proceduralTranslation,
                                                    MatrixMultiply(
                                                            recoilTranslation,
                                                            BuildFpsViewmodelCameraTransform(
                                                                    camera))))))));
}

Matrix BuildFpsViewmodelGripCorrectionTransform(
        const FpsViewmodelGripCorrection& correction)
{
    const Matrix scale = MatrixScale(
            correction.scale, correction.scale, correction.scale);
    const Matrix rotation = MatrixRotateXYZ({
            correction.rotationDegrees.x * DEG2RAD,
            correction.rotationDegrees.y * DEG2RAD,
            correction.rotationDegrees.z * DEG2RAD});
    const Matrix translation = MatrixTranslate(
            correction.translation.x,
            correction.translation.y,
            correction.translation.z);
    return MatrixMultiply(scale, MatrixMultiply(rotation, translation));
}

int FindFpsViewmodelBoneIndex(
        const BoneInfo* bones,
        int boneCount,
        std::string_view boneName)
{
    if (bones == nullptr || boneCount <= 0 || boneName.empty()) return -1;
    for (int index = 0; index < boneCount; ++index) {
        size_t length = 0;
        while (length < sizeof(bones[index].name)
                && bones[index].name[length] != '\0') {
            ++length;
        }
        if (boneName == std::string_view(bones[index].name, length)) return index;
    }
    return -1;
}

namespace {

Matrix FpsViewmodelPoseTransformToMatrix(const Transform& transform)
{
    const Matrix scale = MatrixScale(
            transform.scale.x, transform.scale.y, transform.scale.z);
    const Matrix rotation = QuaternionToMatrix(transform.rotation);
    const Matrix translation = MatrixTranslate(
            transform.translation.x,
            transform.translation.y,
            transform.translation.z);
    return MatrixMultiply(scale, MatrixMultiply(rotation, translation));
}

} // namespace

bool BuildFpsViewmodelBoneModelTransform(
        const Transform* pose,
        const BoneInfo* bones,
        int boneCount,
        int boneIndex,
        FpsViewmodelBonePoseSpace poseSpace,
        Matrix& outTransform)
{
    outTransform = MatrixIdentity();
    if (pose == nullptr || bones == nullptr || boneCount <= 0
            || boneCount > engine::MaxAnimatedModelBones
            || boneIndex < 0 || boneIndex >= boneCount) {
        return false;
    }

    if (poseSpace == FpsViewmodelBonePoseSpace::Model) {
        outTransform = FpsViewmodelPoseTransformToMatrix(pose[boneIndex]);
        return true;
    }
    if (poseSpace != FpsViewmodelBonePoseSpace::Local) return false;

    std::array<int, engine::MaxAnimatedModelBones> chain{};
    int chainLength = 0;
    int current = boneIndex;
    while (current >= 0) {
        if (current >= boneCount || chainLength >= boneCount) return false;
        chain[static_cast<size_t>(chainLength++)] = current;
        current = bones[current].parent;
    }

    Matrix accumulated = MatrixIdentity();
    for (int chainIndex = chainLength - 1; chainIndex >= 0; --chainIndex) {
        const Matrix local = FpsViewmodelPoseTransformToMatrix(
                pose[chain[static_cast<size_t>(chainIndex)]]);
        accumulated = MatrixMultiply(local, accumulated);
    }
    outTransform = accumulated;
    return true;
}

Matrix BuildFpsViewmodelAttachmentTransform(
        Matrix viewmodelRoot,
        Matrix handModelTransform,
        const FpsViewmodelGripCorrection& gripCorrection)
{
    return MatrixMultiply(
            BuildFpsViewmodelGripCorrectionTransform(gripCorrection),
            MatrixMultiply(handModelTransform, viewmodelRoot));
}

Matrix BuildFpsViewmodelMuzzleTransform(
        Matrix pistolWorldTransform,
        const FpsWeaponMuzzleSocketDefinition& socket)
{
    const Matrix rotation = MatrixRotateXYZ({
            socket.rotationDegrees.x * DEG2RAD,
            socket.rotationDegrees.y * DEG2RAD,
            socket.rotationDegrees.z * DEG2RAD});
    const Matrix translation = MatrixTranslate(
            socket.position.x,
            socket.position.y,
            socket.position.z);
    return MatrixMultiply(
            MatrixMultiply(rotation, translation),
            pistolWorldTransform);
}

namespace {

bool Finite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

float NextSignedUnit(uint32_t& state)
{
    if (state == 0) state = 0x6d2b79f5u;
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return static_cast<float>(state & 0x00ffffffu)
                    / static_cast<float>(0x00ffffffu)
            * 2.0f - 1.0f;
}

void AdvanceSpringAxis(
        float& offset,
        float& velocity,
        float frequencyHz,
        float dampingRatio,
        float deltaSeconds)
{
    const float angularFrequency = 2.0f * PI * frequencyHz;
    float remaining = deltaSeconds;
    while (remaining > 0.0f) {
        const float step = std::min(remaining, 1.0f / 240.0f);
        const float acceleration = -angularFrequency * angularFrequency * offset
                - 2.0f * dampingRatio * angularFrequency * velocity;
        velocity += acceleration * step;
        offset += velocity * step;
        remaining -= step;
    }
    if (std::fabs(offset) < 0.000001f && std::fabs(velocity) < 0.00001f) {
        offset = 0.0f;
        velocity = 0.0f;
    }
}

void AdvanceSpringVector(
        Vector3& offset,
        Vector3& velocity,
        float frequencyHz,
        float dampingRatio,
        float deltaSeconds)
{
    AdvanceSpringAxis(offset.x, velocity.x, frequencyHz, dampingRatio, deltaSeconds);
    AdvanceSpringAxis(offset.y, velocity.y, frequencyHz, dampingRatio, deltaSeconds);
    AdvanceSpringAxis(offset.z, velocity.z, frequencyHz, dampingRatio, deltaSeconds);
}

Vector3 ClampMagnitudePerAxis(Vector3 value, Vector3 limits)
{
    value.x = std::clamp(value.x, -limits.x, limits.x);
    value.y = std::clamp(value.y, -limits.y, limits.y);
    value.z = std::clamp(value.z, -limits.z, limits.z);
    return value;
}

} // namespace

bool CanFireFpsWeapon(
        const FpsViewmodelRuntimeState& state,
        bool preview3DGameplay,
        bool mouseInputActive,
        bool uiCaptured,
        FpsFireRejectReason* outReason)
{
    FpsFireRejectReason reason = FpsFireRejectReason::None;
    if (!preview3DGameplay) reason = FpsFireRejectReason::NotInGameplay3D;
    else if (!mouseInputActive) reason = FpsFireRejectReason::MouseInputInactive;
    else if (uiCaptured) reason = FpsFireRejectReason::UiCaptured;
    else if (state.activeWeaponId.empty()) reason = FpsFireRejectReason::NoActiveWeapon;
    else if (!IsFpsViewmodelReadyForUse(state)) reason = FpsFireRejectReason::WeaponNotReady;
    else if (state.firing.cooldownRemainingSeconds > 0.0f) reason = FpsFireRejectReason::Cooldown;
    if (outReason != nullptr) *outReason = reason;
    return reason == FpsFireRejectReason::None;
}

void AdvanceFpsWeaponFiringRuntime(
        FpsWeaponFiringRuntimeState& state,
        float deltaSeconds)
{
    const float fullDt = std::isfinite(deltaSeconds)
            ? std::max(0.0f, deltaSeconds)
            : 0.0f;
    state.definition = ClampFpsWeaponFiringDefinition(state.definition);
    state.cooldownRemainingSeconds = std::max(
            0.0f,
            std::isfinite(state.cooldownRemainingSeconds)
                    ? state.cooldownRemainingSeconds - fullDt
                    : 0.0f);
    if (!Finite(state.recoil.translation)
            || !Finite(state.recoil.translationVelocity)
            || !Finite(state.recoil.rotationDegrees)
            || !Finite(state.recoil.rotationVelocityDegrees)) {
        state.recoil = {};
    }
    const float springDt = std::min(fullDt, 0.25f);
    AdvanceSpringVector(
            state.recoil.translation,
            state.recoil.translationVelocity,
            state.definition.recoil.springFrequencyHz,
            state.definition.recoil.dampingRatio,
            springDt);
    AdvanceSpringVector(
            state.recoil.rotationDegrees,
            state.recoil.rotationVelocityDegrees,
            state.definition.recoil.springFrequencyHz,
            state.definition.recoil.dampingRatio,
            springDt);

    const auto advanceLifetime = [fullDt](bool& active, float& age, float lifetime) {
        if (!active) return;
        age = std::max(0.0f, age) + fullDt;
        if (!(lifetime > 0.0f) || age >= lifetime) {
            active = false;
            age = std::max(0.0f, lifetime);
        }
    };
    advanceLifetime(state.flash.active, state.flash.ageSeconds, state.flash.lifetimeSeconds);
    advanceLifetime(state.light.active, state.light.ageSeconds, state.light.lifetimeSeconds);
}

void ApplyFpsWeaponShotEffects(
        FpsWeaponFiringRuntimeState& state,
        const FpsShotResult& shot)
{
    state.definition = ClampFpsWeaponFiringDefinition(state.definition);
    state.cooldownRemainingSeconds = state.definition.shotIntervalSeconds;
    state.lastShot = shot;
    state.lastShot.accepted = true;
    state.hasLastShot = true;
    ++state.shotSequence;
    state.lastRejectReason = FpsFireRejectReason::None;

    const float roll = NextSignedUnit(state.randomState)
            * state.definition.recoil.rollVariationDegrees;
    state.recoil.translation = ClampMagnitudePerAxis(
            Vector3Add(
                    state.recoil.translation,
                    state.definition.recoil.translationImpulse),
            state.definition.recoil.maximumTranslation);
    Vector3 rotationImpulse = state.definition.recoil.rotationImpulseDegrees;
    rotationImpulse.z += roll;
    state.recoil.rotationDegrees = ClampMagnitudePerAxis(
            Vector3Add(state.recoil.rotationDegrees, rotationImpulse),
            state.definition.recoil.maximumRotationDegrees);

    if (state.definition.muzzleFlash.enabled) {
        state.flash.active = true;
        state.flash.ageSeconds = 0.0f;
        state.flash.lifetimeSeconds = state.definition.muzzleFlash.lifetimeSeconds;
        const float sizeRandom = NextSignedUnit(state.randomState);
        state.flash.sizeWorld = state.definition.muzzleFlash.sizeWorld
                * (1.0f + sizeRandom * state.definition.muzzleFlash.sizeVariation);
        state.flash.rotationDegrees = NextSignedUnit(state.randomState) * 180.0f;
        state.flash.coreColor = state.definition.muzzleFlash.coreColor;
        state.flash.hotColor = state.definition.muzzleFlash.hotColor;
        state.flash.warmColor = state.definition.muzzleFlash.warmColor;
        state.flash.edgeColor = state.definition.muzzleFlash.edgeColor;
        state.flash.edgeSoftness = state.definition.muzzleFlash.edgeSoftness;
    }
    if (state.definition.muzzleLight.enabled) {
        state.light.active = true;
        state.light.ageSeconds = 0.0f;
        state.light.lifetimeSeconds = state.definition.muzzleLight.lifetimeSeconds;
        state.light.intensity = state.definition.muzzleLight.intensity;
        state.light.radiusWorld = state.definition.muzzleLight.radiusWorld;
        state.light.decayExponent = state.definition.muzzleLight.decayExponent;
        state.light.color = state.definition.muzzleLight.color;
    }
}

float FpsMuzzleLightCurrentIntensity(
        const FpsMuzzleLightRuntimeState& state)
{
    if (!state.active || !(state.lifetimeSeconds > 0.0f)) return 0.0f;
    const float remaining = std::clamp(
            1.0f - state.ageSeconds / state.lifetimeSeconds,
            0.0f,
            1.0f);
    return state.intensity * std::pow(remaining, state.decayExponent);
}

} // namespace game
