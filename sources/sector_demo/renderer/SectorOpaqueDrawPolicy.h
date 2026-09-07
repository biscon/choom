#pragma once

#include "engine/assets/ModelAssets.h"
#include "engine/ecs/Entity.h"
#include "sector_demo/SectorPortalVisibility.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace game
{

// Bounds use the same world transform as the color pass. Unknown bounds fail open.
inline bool SectorBoundsInView(const Camera3D &camera, float aspect, float nearPlane,
                               float farPlane, BoundingBox bounds)
{
    const auto finite = [](Vector3 v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    if (!finite(bounds.min) || !finite(bounds.max) || bounds.min.x > bounds.max.x ||
        bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z || !finite(camera.position) ||
        !finite(camera.target) || !finite(camera.up) || !std::isfinite(aspect) || aspect <= 0 ||
        !std::isfinite(camera.fovy) || camera.fovy <= 0 || camera.fovy >= 180)
        return true;
    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    if (Vector3LengthSqr(forward) < 0.000001f)
        return true;
    forward = Vector3Normalize(forward);
    Vector3 right = Vector3CrossProduct(forward, camera.up);
    if (Vector3LengthSqr(right) < 0.000001f)
        return true;
    right = Vector3Normalize(right);
    const Vector3 up = Vector3CrossProduct(right, forward);
    const float vertical = std::tan(camera.fovy * DEG2RAD * 0.5f);
    const float horizontal = vertical * aspect;
    const Vector3 center = Vector3Scale(Vector3Add(bounds.min, bounds.max), 0.5f);
    const Vector3 half = Vector3Scale(Vector3Subtract(bounds.max, bounds.min), 0.5f);
    const Vector3 relative = Vector3Subtract(center, camera.position);
    const auto outside = [&](Vector3 normal, float offset) {
        const float radius = std::fabs(normal.x) * half.x + std::fabs(normal.y) * half.y +
                             std::fabs(normal.z) * half.z;
        return Vector3DotProduct(normal, relative) + radius < offset - 0.0001f;
    };
    return !outside(forward, nearPlane) && !outside(Vector3Negate(forward), -farPlane) &&
           !outside(Vector3Add(Vector3Scale(forward, horizontal), right), 0) &&
           !outside(Vector3Subtract(Vector3Scale(forward, horizontal), right), 0) &&
           !outside(Vector3Add(Vector3Scale(forward, vertical), up), 0) &&
           !outside(Vector3Subtract(Vector3Scale(forward, vertical), up), 0);
}

inline float SectorNearestViewDepth(const Camera3D &camera, BoundingBox bounds)
{
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 nearCorner{forward.x >= 0 ? bounds.min.x : bounds.max.x,
                             forward.y >= 0 ? bounds.min.y : bounds.max.y,
                             forward.z >= 0 ? bounds.min.z : bounds.max.z};
    const float depth = Vector3DotProduct(forward, Vector3Subtract(nearCorner, camera.position));
    return std::isfinite(depth) ? depth : -std::numeric_limits<float>::max();
}

inline bool SectorOpaqueModelVisible(bool objectVisible, int sectorId,
                                     const RuntimePortalVisibilityResult &visibility,
                                     const Camera3D &camera, float aspect, float nearPlane,
                                     float farPlane, BoundingBox bounds, bool hasBounds)
{
    return objectVisible && ShouldDrawRuntimeSectorForVisibility(sectorId, visibility) &&
           (!hasBounds || SectorBoundsInView(camera, aspect, nearPlane, farPlane, bounds));
}

inline bool SectorPaneVisible(bool objectVisible, bool paneVisible, int frontSector, int backSector,
                              const RuntimePortalVisibilityResult *visibility,
                              const Camera3D &camera, float aspect, float nearPlane, float farPlane,
                              BoundingBox bounds)
{
    const bool sectorVisible = !visibility ||
                               ShouldDrawRuntimeSectorForVisibility(frontSector, *visibility) ||
                               ShouldDrawRuntimeSectorForVisibility(backSector, *visibility);
    return objectVisible && paneVisible && sectorVisible &&
           SectorBoundsInView(camera, aspect, nearPlane, farPlane, bounds);
}

struct SectorOpaqueDrawItem {
    engine::Entity entity = engine::NullEntity();
    int id = 0;
    std::size_t index = 0;
    Matrix transform = MatrixIdentity();
    BoundingBox bounds{};
    float depth = 0;
};

inline bool SectorOpaqueDrawLess(const SectorOpaqueDrawItem &a, const SectorOpaqueDrawItem &b)
{
    if (a.depth != b.depth)
        return a.depth < b.depth;
    if (a.id != b.id)
        return a.id < b.id;
    return a.index < b.index;
}

inline void AppendSectorOpaqueDraw(std::vector<SectorOpaqueDrawItem> &items,
                                   SectorOpaqueDrawItem item, bool &warned)
{
    if (items.size() == items.capacity() && !warned) {
        TraceLog(LOG_WARNING, "RENDER: visible draw buffer capacity exceeded; frame allocation");
        warned = true;
    }
    items.push_back(item);
}

// 0 means conservative two-sided; negative means reflected winding.
inline int SectorMaterialCullWinding(const engine::ModelMaterialAsset &material, Matrix transform)
{
    if (material.sidedness != engine::ModelMaterialSidedness::SingleSided)
        return 0;
    const float determinant =
        transform.m0 * (transform.m5 * transform.m10 - transform.m9 * transform.m6) -
        transform.m4 * (transform.m1 * transform.m10 - transform.m9 * transform.m2) +
        transform.m8 * (transform.m1 * transform.m6 - transform.m5 * transform.m2);
    if (!std::isfinite(determinant) || std::fabs(determinant) < 1e-8f)
        return 0;
    return determinant < 0 ? -1 : 1;
}

inline bool SectorMaterialDepthEligible(const engine::ModelMaterialAsset &material)
{
    return material.alphaMode == engine::ModelMaterialAlphaMode::Opaque;
}

void ApplySectorMaterialCulling(const engine::ModelMaterialAsset &material, Matrix transform);
void RestoreSectorMaterialCulling();

} // namespace game
