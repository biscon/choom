#include "engine/systems/AnimatedModelRaycast.h"

#include "engine/assets/ModelAssets.h"
#include "engine/components/AnimatedModel.h"
#include "engine/systems/AnimatedModelSystem.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace engine {
namespace {

constexpr float RaycastEpsilon = 0.000001f;

bool Finite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

bool HasUsableTriangles(const Model& model)
{
    if (model.meshes == nullptr || model.meshCount <= 0) return false;
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const Mesh& mesh = model.meshes[meshIndex];
        if (mesh.vertices != nullptr
                && mesh.vertexCount >= 3
                && mesh.triangleCount > 0) {
            return true;
        }
    }
    return false;
}

BoundingBox TransformBounds(BoundingBox bounds, Matrix transform)
{
    BoundingBox result{
            Vector3{
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max()},
            Vector3{
                    -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max()}};
    for (float x : {bounds.min.x, bounds.max.x}) {
        for (float y : {bounds.min.y, bounds.max.y}) {
            for (float z : {bounds.min.z, bounds.max.z}) {
                const Vector3 point = Vector3Transform(Vector3{x, y, z}, transform);
                result.min.x = std::min(result.min.x, point.x);
                result.min.y = std::min(result.min.y, point.y);
                result.min.z = std::min(result.min.z, point.z);
                result.max.x = std::max(result.max.x, point.x);
                result.max.y = std::max(result.max.y, point.y);
                result.max.z = std::max(result.max.z, point.z);
            }
        }
    }
    return result;
}

bool RayIntersectsBounds(Ray ray, BoundingBox bounds, float maximumDistance)
{
    float nearDistance = 0.0f;
    float farDistance = maximumDistance;
    const float origins[3] = {ray.position.x, ray.position.y, ray.position.z};
    const float directions[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float minimums[3] = {bounds.min.x, bounds.min.y, bounds.min.z};
    const float maximums[3] = {bounds.max.x, bounds.max.y, bounds.max.z};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(directions[axis]) <= RaycastEpsilon) {
            if (origins[axis] < minimums[axis]
                    || origins[axis] > maximums[axis]) {
                return false;
            }
            continue;
        }
        float first = (minimums[axis] - origins[axis]) / directions[axis];
        float second = (maximums[axis] - origins[axis]) / directions[axis];
        if (first > second) std::swap(first, second);
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance) return false;
    }
    return farDistance >= 0.0f && nearDistance <= maximumDistance;
}

bool MeshTriangleVertexIndices(
        const Mesh& mesh,
        int triangleIndex,
        std::array<uint32_t, 3>& outIndices)
{
    if (triangleIndex < 0 || triangleIndex >= mesh.triangleCount) return false;
    for (int corner = 0; corner < 3; ++corner) {
        const int sourceIndex = triangleIndex * 3 + corner;
        const uint32_t vertexIndex = mesh.indices != nullptr
                ? mesh.indices[sourceIndex]
                : static_cast<uint32_t>(sourceIndex);
        if (vertexIndex >= static_cast<uint32_t>(mesh.vertexCount)) return false;
        outIndices[static_cast<size_t>(corner)] = vertexIndex;
    }
    return true;
}

bool SkinnedVertex(
        const ModelAsset& asset,
        const AnimatedModelInstance& instance,
        const Mesh& mesh,
        int meshIndex,
        uint32_t vertexIndex,
        Matrix modelTransform,
        Vector3& outVertex)
{
    if (mesh.vertices == nullptr
            || vertexIndex >= static_cast<uint32_t>(mesh.vertexCount)) {
        return false;
    }
    const int positionOffset = static_cast<int>(vertexIndex) * 3;
    const Vector3 source{
            mesh.vertices[positionOffset],
            mesh.vertices[positionOffset + 1],
            mesh.vertices[positionOffset + 2]};
    Vector3 posed = source;
    int meshBoneCount = 0;
    const Matrix* meshBoneMatrices = AnimatedModelMeshBoneMatrices(
            asset,
            instance,
            meshIndex,
            meshBoneCount);
    const bool canSkin = mesh.boneIndices != nullptr
            && mesh.boneWeights != nullptr
            && meshBoneMatrices != nullptr
            && meshBoneCount > 0;
    if (canSkin) {
        posed = {};
        bool usedInfluence = false;
        for (int influence = 0; influence < 4; ++influence) {
            const int influenceOffset = static_cast<int>(vertexIndex) * 4 + influence;
            const float weight = mesh.boneWeights[influenceOffset];
            const uint32_t boneIndex = mesh.boneIndices[influenceOffset];
            if (!std::isfinite(weight) || weight <= 0.0f) continue;
            if (boneIndex >= static_cast<uint32_t>(meshBoneCount)) return false;
            posed = Vector3Add(
                    posed,
                    Vector3Scale(
                            Vector3Transform(
                                    source,
                                    meshBoneMatrices[boneIndex]),
                            weight));
            usedInfluence = true;
        }
        if (!usedInfluence) posed = source;
    }
    outVertex = Vector3Transform(posed, modelTransform);
    return Finite(outVertex);
}

bool RayTriangle(
        Ray ray,
        Vector3 first,
        Vector3 second,
        Vector3 third,
        float maximumDistance,
        float& outDistance,
        Vector3& outNormal,
        Vector3& outBarycentric)
{
    const Vector3 edge1 = Vector3Subtract(second, first);
    const Vector3 edge2 = Vector3Subtract(third, first);
    const Vector3 p = Vector3CrossProduct(ray.direction, edge2);
    const float determinant = Vector3DotProduct(edge1, p);
    if (std::fabs(determinant) <= RaycastEpsilon) return false;
    const float inverseDeterminant = 1.0f / determinant;
    const Vector3 offset = Vector3Subtract(ray.position, first);
    const float u = Vector3DotProduct(offset, p) * inverseDeterminant;
    if (u < 0.0f || u > 1.0f) return false;
    const Vector3 q = Vector3CrossProduct(offset, edge1);
    const float v = Vector3DotProduct(ray.direction, q) * inverseDeterminant;
    if (v < 0.0f || u + v > 1.0f) return false;
    const float distance = Vector3DotProduct(edge2, q) * inverseDeterminant;
    if (distance <= RaycastEpsilon || distance > maximumDistance) return false;
    Vector3 normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
    if (Vector3DotProduct(normal, ray.direction) > 0.0f) {
        normal = Vector3Negate(normal);
    }
    outDistance = distance;
    outNormal = normal;
    outBarycentric = {1.0f - u - v, u, v};
    return true;
}

} // namespace

AnimatedModelRaycastStatus RaycastAnimatedModel(
        const ModelAsset& asset,
        const AnimatedModelInstance& instance,
        Matrix authoredTransform,
        Ray ray,
        float maximumDistance,
        AnimatedModelRaycastResult& outResult)
{
    outResult = {};
    if (!instance.poseReady
            || instance.poseFailed
            || !std::isfinite(maximumDistance)
            || maximumDistance <= 0.0f
            || !Finite(ray.position)
            || !Finite(ray.direction)
            || !HasUsableTriangles(asset.model)) {
        return AnimatedModelRaycastStatus::Unavailable;
    }
    const BoundingBox* localBounds = asset.hasAnimatedLocalBounds
            ? &asset.animatedLocalBounds
            : (asset.hasLocalBounds ? &asset.localBounds : nullptr);
    if (localBounds == nullptr) return AnimatedModelRaycastStatus::Unavailable;
    if (!RayIntersectsBounds(
                ray,
                TransformBounds(*localBounds, authoredTransform),
                maximumDistance)) {
        return AnimatedModelRaycastStatus::Miss;
    }

    bool hit = false;
    float closestDistance = maximumDistance;
    for (int meshIndex = 0; meshIndex < asset.model.meshCount; ++meshIndex) {
        const Mesh& mesh = asset.model.meshes[meshIndex];
        if (mesh.vertices == nullptr || mesh.triangleCount <= 0) continue;
        const Matrix modelTransform = AnimatedModelMeshTransform(
                asset,
                instance.meshNodeMatrices,
                meshIndex,
                authoredTransform);
        for (int triangleIndex = 0;
                triangleIndex < mesh.triangleCount;
                ++triangleIndex) {
            std::array<uint32_t, 3> indices{};
            if (!MeshTriangleVertexIndices(mesh, triangleIndex, indices)) continue;
            std::array<Vector3, 3> vertices{};
            if (!SkinnedVertex(
                        asset, instance, mesh, meshIndex, indices[0],
                        modelTransform, vertices[0])
                    || !SkinnedVertex(
                        asset, instance, mesh, meshIndex, indices[1],
                        modelTransform, vertices[1])
                    || !SkinnedVertex(
                        asset, instance, mesh, meshIndex, indices[2],
                        modelTransform, vertices[2])) {
                continue;
            }
            float distance = 0.0f;
            Vector3 normal{};
            Vector3 barycentric{};
            if (!RayTriangle(
                        ray,
                        vertices[0], vertices[1], vertices[2],
                        closestDistance,
                        distance, normal, barycentric)) {
                continue;
            }
            hit = true;
            closestDistance = distance;
            outResult.distance = distance;
            outResult.position = Vector3Add(
                    ray.position, Vector3Scale(ray.direction, distance));
            outResult.normal = normal;
            outResult.anchor.model = instance.model;
            outResult.anchor.vertexIndices = indices;
            outResult.anchor.barycentric = barycentric;
            outResult.anchor.meshIndex = static_cast<uint32_t>(meshIndex);
            outResult.anchor.valid = true;
        }
    }
    return hit
            ? AnimatedModelRaycastStatus::Hit
            : AnimatedModelRaycastStatus::Miss;
}

bool ResolveAnimatedModelSurfaceAnchor(
        const ModelAsset& asset,
        const AnimatedModelInstance& instance,
        const AnimatedModelSurfaceAnchor& anchor,
        Matrix authoredTransform,
        Vector3& outPosition)
{
    if (!anchor.valid
            || anchor.model != instance.model
            || anchor.meshIndex >= static_cast<uint32_t>(asset.model.meshCount)
            || asset.model.meshes == nullptr) {
        return false;
    }
    const Mesh& mesh = asset.model.meshes[anchor.meshIndex];
    std::array<Vector3, 3> vertices{};
    const Matrix modelTransform = AnimatedModelMeshTransform(
            asset,
            instance.meshNodeMatrices,
            static_cast<int>(anchor.meshIndex),
            authoredTransform);
    for (size_t corner = 0; corner < vertices.size(); ++corner) {
        if (!SkinnedVertex(
                    asset,
                    instance,
                    mesh,
                    static_cast<int>(anchor.meshIndex),
                    anchor.vertexIndices[corner],
                    modelTransform,
                    vertices[corner])) {
            return false;
        }
    }
    outPosition = {};
    const float barycentric[3] = {
            anchor.barycentric.x,
            anchor.barycentric.y,
            anchor.barycentric.z};
    for (size_t corner = 0; corner < vertices.size(); ++corner) {
        outPosition = Vector3Add(
                outPosition,
                Vector3Scale(
                        vertices[corner],
                        barycentric[corner]));
    }
    return Finite(outPosition);
}

} // namespace engine
