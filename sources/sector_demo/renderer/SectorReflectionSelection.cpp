#include "sector_demo/renderer/SectorPbrEnvironment.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace game
{
namespace
{
Vector3 ToProbeLocal(Vector3 point, const SectorCompiledReflectionProbe &probe)
{
    const float c = std::cos(-probe.yawRadians);
    const float s = std::sin(-probe.yawRadians);
    const Vector3 relative = Vector3Subtract(point, probe.influenceCenterWorld);
    return Vector3{relative.x * c - relative.z * s, relative.y, relative.x * s + relative.z * c};
}
} // namespace

SectorPbrEnvironmentSelection SelectSectorPbrEnvironment(const SectorPbrEnvironment &environment,
                                                         Vector3 receiverPosition,
                                                         int receiverSectorId,
                                                         bool includeLocalProbes)
{
    const SectorPbrEnvironment::LocalProbe *best = nullptr;
    float bestDistanceSquared = 0.0f;
    for (const SectorPbrEnvironment::LocalProbe &candidate : environment.localProbes)
    {
        if (!includeLocalProbes)
            break;
        const SectorCompiledReflectionProbe &probe = candidate.definition;
        if (!probe.enabled || !candidate.ready || engine::IsNull(candidate.cubemap))
            continue;
        const Vector3 local = ToProbeLocal(receiverPosition, probe);
        if (std::fabs(local.x) > probe.halfExtentsWorld.x ||
            std::fabs(local.y) > probe.halfExtentsWorld.y ||
            std::fabs(local.z) > probe.halfExtentsWorld.z)
        {
            continue;
        }
        const float distanceSquared =
            Vector3DistanceSqr(receiverPosition, probe.influenceCenterWorld);
        const bool better =
            best == nullptr || probe.priority > best->definition.priority ||
            (probe.priority == best->definition.priority &&
             probe.topologySectorId == receiverSectorId &&
             best->definition.topologySectorId != receiverSectorId) ||
            (probe.priority == best->definition.priority &&
             (probe.topologySectorId == receiverSectorId) ==
                 (best->definition.topologySectorId == receiverSectorId) &&
             (distanceSquared < bestDistanceSquared ||
              (distanceSquared == bestDistanceSquared &&
               probe.sourceAuthoringProbeId < best->definition.sourceAuthoringProbeId)));
        if (better)
        {
            best = &candidate;
            bestDistanceSquared = distanceSquared;
        }
    }
    if (best != nullptr)
    {
        const SectorCompiledReflectionProbe &probe = best->definition;
        return SectorPbrEnvironmentSelection{
            best->cubemap,
            probe.capturePositionWorld,
            probe.influenceCenterWorld,
            probe.halfExtentsWorld,
            probe.yawRadians,
            probe.intensity,
            static_cast<float>(std::max(0, best->mipCount - 1)),
            true,
            true,
            probe.sourceAuthoringProbeId,
            best->hasPrevious ? best->inactive : engine::NullTextureHandle(),
            static_cast<float>(
                std::clamp((environment.seconds - best->publishedAt) / 0.1, 0.0, 1.0)),
            probe.blendDistanceWorld};
    }
    if (!engine::IsNull(environment.cubemap))
    {
        return SectorPbrEnvironmentSelection{
            environment.cubemap, {}, {}, {1.0f, 1.0f, 1.0f}, 0.0f, 1.0f, 8.0f, false, false};
    }
    return {};
}

namespace
{
SectorPbrEnvironmentSelection ProbeSelection(const SectorPbrEnvironment &environment,
                                             const SectorPbrEnvironment::LocalProbe &source)
{
    const auto &p = source.definition;
    return {
        source.cubemap,
        p.capturePositionWorld,
        p.influenceCenterWorld,
        p.halfExtentsWorld,
        p.yawRadians,
        p.intensity,
        static_cast<float>(source.mipCount - 1),
        true,
        true,
        p.sourceAuthoringProbeId,
        source.hasPrevious ? source.inactive : engine::NullTextureHandle(),
        static_cast<float>(std::clamp((environment.seconds - source.publishedAt) / 0.1, 0.0, 1.0)),
        p.blendDistanceWorld};
}
const SectorPbrEnvironment::LocalProbe *ProbeInSector(const SectorPbrEnvironment &environment,
                                                      int sector, Vector3 position)
{
    const SectorPbrEnvironment::LocalProbe *best = nullptr;
    for (const auto &p : environment.localProbes)
    {
        if (!p.ready || !p.definition.enabled || engine::IsNull(p.cubemap) ||
            p.definition.topologySectorId != sector)
            continue;
        const float distance = Vector3DistanceSqr(position, p.definition.influenceCenterWorld);
        const float bestDistance =
            best ? Vector3DistanceSqr(position, best->definition.influenceCenterWorld) : 0.0f;
        if (!best || p.definition.priority > best->definition.priority ||
            (p.definition.priority == best->definition.priority &&
             (distance < bestDistance ||
              (distance == bestDistance &&
               p.definition.sourceAuthoringProbeId < best->definition.sourceAuthoringProbeId))))
            best = &p;
    }
    return best;
}
} // namespace

SectorPbrEnvironmentBlend SelectSectorPbrEnvironmentBlend(const SectorPbrEnvironment &environment,
                                                          Vector3 position, int sector,
                                                          bool includeLocalProbes,
                                                          const BoundingBox *bounds)
{
    SectorPbrEnvironmentBlend result;
    result.first = SelectSectorPbrEnvironment(environment, position, sector, includeLocalProbes);
    if (!includeLocalProbes)
        return result;
    // A receiver retains its own room's source when seen through another room.
    const auto *own = ProbeInSector(environment, sector, position);
    if (own)
        result.first = ProbeSelection(environment, *own);
    else if (sector > 0)
        result.first = SelectSectorPbrEnvironment(environment, position, sector, false);
    const Vector3 half =
        bounds ? Vector3Scale(Vector3Subtract(bounds->max, bounds->min), 0.5f) : Vector3{};
    float nearest = std::numeric_limits<float>::max();
    int nearestId = std::numeric_limits<int>::max();
    for (const auto &portal : environment.portals)
    {
        if (!portal.open || portal.fromSectorId != sector ||
            position.y + half.y < portal.openBottom || position.y - half.y > portal.openTop)
            continue;
        const bool blocked =
            std::any_of(environment.blockers.begin(), environment.blockers.end(), [&](const auto &b)
                        { return b.lineDefId == portal.lineDefId && b.blocksPortal; });
        if (blocked)
            continue;
        const auto *other = ProbeInSector(environment, portal.toSectorId, position);
        if (!own || !other)
            continue;
        const Vector2 edge = Vector2Subtract(portal.b, portal.a);
        const float length = Vector2Length(edge);
        if (length < 0.00001f)
            continue;
        const Vector2 tangent = Vector2Scale(edge, 1.0f / length);
        const Vector2 offset{position.x - portal.a.x, position.z - portal.a.y};
        const float along = Vector2DotProduct(offset, tangent);
        const float alongRadius = std::fabs(tangent.x) * half.x + std::fabs(tangent.y) * half.z;
        if (along + alongRadius < 0 || along - alongRadius > length)
            continue;
        Vector3 normal{-tangent.y, 0, tangent.x};
        const Vector3 point{portal.a.x, 0, portal.a.y};
        if (Vector3DotProduct(Vector3Subtract(own->definition.capturePositionWorld, point),
                              normal) > 0)
            normal = Vector3Negate(normal);
        const float d = Vector3DotProduct(Vector3Subtract(position, point), normal);
        const float width = own->definition.blendDistanceWorld;
        const float otherWidth = other->definition.blendDistanceWorld;
        const float normalRadius = std::fabs(normal.x) * half.x + std::fabs(normal.z) * half.z;
        if (width <= 0 || otherWidth <= 0 || d + normalRadius < -width ||
            d - normalRadius > otherWidth)
            continue;
        if (std::fabs(d) > nearest || (std::fabs(d) == nearest && portal.lineDefId >= nearestId))
            continue;
        nearest = std::fabs(d);
        nearestId = portal.lineDefId;
        result.first = ProbeSelection(environment, *own);
        result.second = ProbeSelection(environment, *other);
        result.portal = true;
        result.portalPlane = {normal.x, normal.y, normal.z, -Vector3DotProduct(normal, point)};
        result.portalWidths = {width, otherWidth};
        result.portalAperture = {tangent.x, tangent.y, Vector2DotProduct(portal.a, tangent),
                                 length};
        result.portalHeights = {portal.openBottom, portal.openTop};
    }
    if (result.portal)
        return result;
    // Same-sector overlapping volumes blend without importing a neighbor
    // through a solid wall. Cross-sector blending belongs to portal apertures.
    float bestDistance = std::numeric_limits<float>::max();
    for (const auto &p : environment.localProbes)
    {
        if (!p.ready || !p.definition.enabled || p.definition.topologySectorId != sector ||
            p.definition.sourceAuthoringProbeId == result.first.probeId)
            continue;
        const Vector3 local = ToProbeLocal(position, p.definition);
        const auto &e = p.definition.halfExtentsWorld;
        if (std::fabs(local.x) > e.x || std::fabs(local.y) > e.y || std::fabs(local.z) > e.z)
            continue;
        const float distance = Vector3DistanceSqr(position, p.definition.influenceCenterWorld);
        if (distance < bestDistance)
        {
            result.second = ProbeSelection(environment, p);
            bestDistance = distance;
        }
    }
    return result;
}

float SectorReflectionBlendWeight(const SectorPbrEnvironmentBlend &blend, Vector3 position)
{
    if (engine::IsNull(blend.second.cubemap))
        return 0;
    if (blend.portal)
    {
        const auto &aperture = blend.portalAperture;
        const float along = position.x * aperture.x + position.z * aperture.y - aperture.z;
        if (along < 0 || along > aperture.w || position.y < blend.portalHeights.x ||
            position.y > blend.portalHeights.y)
            return 0;
        const auto &p = blend.portalPlane;
        const float d = p.x * position.x + p.y * position.y + p.z * position.z + p.w;
        const float t =
            std::clamp((d + blend.portalWidths.x) /
                           std::max(blend.portalWidths.x + blend.portalWidths.y, 0.00001f),
                       0.0f, 1.0f);
        return t * t * (3 - 2 * t);
    }
    const auto weight = [&](const SectorPbrEnvironmentSelection &p)
    {
        SectorCompiledReflectionProbe definition;
        definition.influenceCenterWorld = p.influenceCenter;
        definition.yawRadians = p.yawRadians;
        const auto v = ToProbeLocal(position, definition);
        const float edge =
            std::min({p.halfExtents.x - std::fabs(v.x), p.halfExtents.y - std::fabs(v.y),
                      p.halfExtents.z - std::fabs(v.z)});
        return p.blendDistance > 0 ? std::clamp(edge / p.blendDistance, 0.0f, 1.0f)
                                   : (edge >= 0 ? 1.0f : 0.0f);
    };
    const float a = weight(blend.first), b = weight(blend.second);
    return a + b > 0 ? b / (a + b) : 0;
}

} // namespace game
