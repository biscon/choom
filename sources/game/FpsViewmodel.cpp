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
    if (!preview3DActive || inputSuppressed || state.loadState == FpsViewmodelLoadState::Inactive) return false;
    state.holstered = !state.holstered;
    return true;
}

bool IsFpsViewmodelRenderable(const FpsViewmodelRuntimeState& state)
{
    return state.loadState == FpsViewmodelLoadState::Ready
            && !state.holstered
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
    const FpsViewmodelCameraBasis basis = BuildFpsViewmodelCameraBasis(camera);
    Matrix cameraTransform = MatrixIdentity();
    cameraTransform.m0 = basis.right.x; cameraTransform.m1 = basis.right.y; cameraTransform.m2 = basis.right.z;
    cameraTransform.m4 = basis.up.x; cameraTransform.m5 = basis.up.y; cameraTransform.m6 = basis.up.z;
    cameraTransform.m8 = basis.forward.x; cameraTransform.m9 = basis.forward.y; cameraTransform.m10 = basis.forward.z;
    cameraTransform.m12 = camera.position.x; cameraTransform.m13 = camera.position.y; cameraTransform.m14 = camera.position.z;

    const Matrix scale = MatrixScale(presentation.scale, presentation.scale, presentation.scale);
    const Matrix rotation = MatrixRotateXYZ({
            presentation.rotationDegrees.x * DEG2RAD,
            presentation.rotationDegrees.y * DEG2RAD,
            presentation.rotationDegrees.z * DEG2RAD});
    const Matrix translation = MatrixTranslate(
            presentation.position.x, presentation.position.y, presentation.position.z);
    return MatrixMultiply(scale, MatrixMultiply(rotation, MatrixMultiply(translation, cameraTransform)));
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

} // namespace game
