#include "sector_demo/renderer/SectorAtmosphereCulling.h"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace game {
namespace {

bool IsFiniteVector(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

bool ValidBounds(Vector3 minimum, Vector3 maximum)
{
    return IsFiniteVector(minimum)
            && IsFiniteVector(maximum)
            && minimum.x <= maximum.x
            && minimum.y <= maximum.y
            && minimum.z <= maximum.z;
}

SectorAtmosphereScissorRect FullRect(int width, int height)
{
    return SectorAtmosphereScissorRect{
            0, 0, std::max(width, 0), std::max(height, 0)};
}

Vector3 Corner(Vector3 minimum, Vector3 maximum, int index)
{
    return Vector3{
            (index & 1) != 0 ? maximum.x : minimum.x,
            (index & 2) != 0 ? maximum.y : minimum.y,
            (index & 4) != 0 ? maximum.z : minimum.z};
}

} // namespace

SectorAtmosphereScissorRect ProjectSectorAtmosphereBoundsToScissor(
        const Camera3D& camera,
        float aspectRatio,
        float nearPlane,
        Vector3 boundsMin,
        Vector3 boundsMax,
        int targetWidth,
        int targetHeight)
{
    if (targetWidth <= 0 || targetHeight <= 0) return {};
    if (!ValidBounds(boundsMin, boundsMax)
            || !IsFiniteVector(camera.position)
            || !IsFiniteVector(camera.target)
            || !IsFiniteVector(camera.up)
            || !std::isfinite(camera.fovy)
            || !std::isfinite(aspectRatio)
            || !std::isfinite(nearPlane)
            || camera.fovy <= 0.0f
            || camera.fovy >= 179.0f
            || aspectRatio <= 0.0f
            || nearPlane <= 0.0f) {
        return FullRect(targetWidth, targetHeight);
    }

    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    if (Vector3LengthSqr(forward) <= 0.00000001f) {
        return FullRect(targetWidth, targetHeight);
    }
    forward = Vector3Normalize(forward);
    Vector3 right = Vector3CrossProduct(forward, camera.up);
    if (Vector3LengthSqr(right) <= 0.00000001f) {
        return FullRect(targetWidth, targetHeight);
    }
    right = Vector3Normalize(right);
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
    const float tanHalfFov = std::tan(camera.fovy * DEG2RAD * 0.5f);
    if (!std::isfinite(tanHalfFov) || tanHalfFov <= 0.0f) {
        return FullRect(targetWidth, targetHeight);
    }

    float minimumDepth = std::numeric_limits<float>::max();
    float maximumDepth = -std::numeric_limits<float>::max();
    std::array<Vector3, 8> relativeCorners{};
    for (int index = 0; index < 8; ++index) {
        relativeCorners[static_cast<std::size_t>(index)] = Vector3Subtract(
                Corner(boundsMin, boundsMax, index), camera.position);
        const float depth = Vector3DotProduct(
                relativeCorners[static_cast<std::size_t>(index)], forward);
        minimumDepth = std::min(minimumDepth, depth);
        maximumDepth = std::max(maximumDepth, depth);
    }
    if (maximumDepth < nearPlane) return {};
    // Projection becomes unbounded when a conservative box crosses the near
    // plane. Fullscreen is the safe fallback in that case.
    if (minimumDepth <= nearPlane) return FullRect(targetWidth, targetHeight);

    float minimumNdcX = std::numeric_limits<float>::max();
    float minimumNdcY = std::numeric_limits<float>::max();
    float maximumNdcX = -std::numeric_limits<float>::max();
    float maximumNdcY = -std::numeric_limits<float>::max();
    for (const Vector3 relative : relativeCorners) {
        const float depth = Vector3DotProduct(relative, forward);
        const float ndcX = Vector3DotProduct(relative, right)
                / (depth * tanHalfFov * aspectRatio);
        const float ndcY = Vector3DotProduct(relative, up)
                / (depth * tanHalfFov);
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) {
            return FullRect(targetWidth, targetHeight);
        }
        minimumNdcX = std::min(minimumNdcX, ndcX);
        minimumNdcY = std::min(minimumNdcY, ndcY);
        maximumNdcX = std::max(maximumNdcX, ndcX);
        maximumNdcY = std::max(maximumNdcY, ndcY);
    }
    if (maximumNdcX < -1.0f || minimumNdcX > 1.0f
            || maximumNdcY < -1.0f || minimumNdcY > 1.0f) {
        return {};
    }

    minimumNdcX = std::clamp(minimumNdcX, -1.0f, 1.0f);
    maximumNdcX = std::clamp(maximumNdcX, -1.0f, 1.0f);
    minimumNdcY = std::clamp(minimumNdcY, -1.0f, 1.0f);
    maximumNdcY = std::clamp(maximumNdcY, -1.0f, 1.0f);
    const int minimumX = std::max(0, static_cast<int>(std::floor(
            (minimumNdcX * 0.5f + 0.5f) * targetWidth)) - 1);
    const int maximumX = std::min(targetWidth, static_cast<int>(std::ceil(
            (maximumNdcX * 0.5f + 0.5f) * targetWidth)) + 1);
    const int minimumY = std::max(0, static_cast<int>(std::floor(
            (minimumNdcY * 0.5f + 0.5f) * targetHeight)) - 1);
    const int maximumY = std::min(targetHeight, static_cast<int>(std::ceil(
            (maximumNdcY * 0.5f + 0.5f) * targetHeight)) + 1);
    return SectorAtmosphereScissorRect{
            minimumX,
            minimumY,
            std::max(maximumX - minimumX, 0),
            std::max(maximumY - minimumY, 0)};
}

SectorAtmosphereScissorRect UnionSectorAtmosphereScissors(
        SectorAtmosphereScissorRect left,
        SectorAtmosphereScissorRect right,
        int targetWidth,
        int targetHeight)
{
    if (left.Empty()) return right;
    if (right.Empty()) return left;
    const int minimumX = std::clamp(std::min(left.x, right.x), 0, targetWidth);
    const int minimumY = std::clamp(std::min(left.y, right.y), 0, targetHeight);
    const int maximumX = std::clamp(
            std::max(left.x + left.width, right.x + right.width),
            0,
            targetWidth);
    const int maximumY = std::clamp(
            std::max(left.y + left.height, right.y + right.height),
            0,
            targetHeight);
    return SectorAtmosphereScissorRect{
            minimumX,
            minimumY,
            std::max(maximumX - minimumX, 0),
            std::max(maximumY - minimumY, 0)};
}

float SectorAtmosphereScissorCoverage(
        SectorAtmosphereScissorRect rect,
        int targetWidth,
        int targetHeight)
{
    if (rect.Empty() || targetWidth <= 0 || targetHeight <= 0) return 0.0f;
    const double covered = static_cast<double>(rect.width)
            * static_cast<double>(rect.height);
    const double total = static_cast<double>(targetWidth)
            * static_cast<double>(targetHeight);
    return static_cast<float>(std::clamp(covered / total, 0.0, 1.0));
}

bool SectorAtmosphereDynamicLightIntersectsBounds(
        const SectorBillboardDynamicLightContext& lights,
        int lightIndex,
        Vector3 boundsMin,
        Vector3 boundsMax)
{
    if (lightIndex < 0 || lightIndex >= lights.dynamicLightCount
            || lightIndex >= static_cast<int>(MaxDynamicLights)) {
        return false;
    }
    if (!ValidBounds(boundsMin, boundsMax)) return true;
    const std::size_t index = static_cast<std::size_t>(lightIndex);
    const Vector3 center = lights.dynamicLightPositions[index];
    const float radius = lights.dynamicLightRadii[index];
    if (!IsFiniteVector(center) || !std::isfinite(radius)) return true;
    if (radius <= 0.0f) return false;
    const Vector3 closest{
            std::clamp(center.x, boundsMin.x, boundsMax.x),
            std::clamp(center.y, boundsMin.y, boundsMax.y),
            std::clamp(center.z, boundsMin.z, boundsMax.z)};
    return Vector3DistanceSqr(center, closest) <= radius * radius;
}

std::uint32_t BuildSectorAtmosphereDynamicLightMask(
        const SectorBillboardDynamicLightContext& lights,
        Vector3 boundsMin,
        Vector3 boundsMax)
{
    std::uint32_t mask = 0;
    const int count = std::clamp(
            lights.dynamicLightCount,
            0,
            static_cast<int>(MaxDynamicLights));
    for (int index = 0; index < count; ++index) {
        if (SectorAtmosphereDynamicLightIntersectsBounds(
                    lights, index, boundsMin, boundsMax)) {
            mask |= std::uint32_t{1} << static_cast<unsigned int>(index);
        }
    }
    return mask;
}

} // namespace game
