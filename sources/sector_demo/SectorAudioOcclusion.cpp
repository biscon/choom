#include "sector_demo/SectorAudioOcclusion.h"

#include "sector_demo/SectorCollisionWorld.h"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace game {
namespace {

constexpr float AudioOcclusionEpsilon = 0.001f;
constexpr float AudioOcclusionEndpointEpsilon = 0.01f;
constexpr int AudioTransmissionIterationLimit = 32;
constexpr float PortalDiffractionVolumeBase = 0.7f;
constexpr float PortalDiffractionCutoffBase = 0.35f;
constexpr float PortalMinimumCutoffHz = 2000.0f;

bool IsFinite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

bool PointInsideDoorCollider(
        Vector3 point,
        const SectorDynamicDoorCollider& door)
{
    const Vector2 offset{point.x - door.center.x, point.z - door.center.y};
    const float localX = Vector2DotProduct(offset, door.tangent);
    const float localZ = Vector2DotProduct(offset, door.normal);
    return std::fabs(localX) <= door.halfExtents.x + AudioOcclusionEpsilon
            && std::fabs(localZ)
                    <= door.halfExtents.y + AudioOcclusionEpsilon
            && point.y >= door.bottom - AudioOcclusionEpsilon
            && point.y <= door.top + AudioOcclusionEpsilon;
}

bool SegmentIntersectsDoorCollider(
        Vector3 origin,
        Vector3 endpoint,
        Vector3 direction,
        float maximumDistance,
        const SectorDynamicDoorCollider& door)
{
    if (PointInsideDoorCollider(origin, door)
            || PointInsideDoorCollider(endpoint, door)) {
        return false;
    }
    const Vector2 offset{origin.x - door.center.x, origin.z - door.center.y};
    const Vector2 directionXZ{direction.x, direction.z};
    const float localOrigin[3] = {
            Vector2DotProduct(offset, door.tangent),
            origin.y - (door.bottom + door.top) * 0.5f,
            Vector2DotProduct(offset, door.normal)};
    const float localDirection[3] = {
            Vector2DotProduct(directionXZ, door.tangent),
            direction.y,
            Vector2DotProduct(directionXZ, door.normal)};
    const float extents[3] = {
            std::max(0.0f, door.halfExtents.x),
            std::max(0.0f, (door.top - door.bottom) * 0.5f),
            std::max(0.0f, door.halfExtents.y)};
    float nearDistance = 0.0f;
    float farDistance = maximumDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(localDirection[axis]) <= AudioOcclusionEpsilon) {
            if (localOrigin[axis] < -extents[axis]
                    || localOrigin[axis] > extents[axis]) {
                return false;
            }
            continue;
        }
        float first = (-extents[axis] - localOrigin[axis])
                / localDirection[axis];
        float second = (extents[axis] - localOrigin[axis])
                / localDirection[axis];
        if (first > second) std::swap(first, second);
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance) return false;
    }
    return nearDistance >= 0.0f && nearDistance <= maximumDistance;
}

uint64_t SurfaceKey(const SectorCollisionRayHit& hit)
{
    if (hit.lineDefId > 0) {
        return (uint64_t{1} << 63)
                | static_cast<uint32_t>(hit.lineDefId);
    }
    return (static_cast<uint64_t>(static_cast<uint32_t>(hit.sectorId)) << 8)
            | static_cast<unsigned int>(hit.surfaceKind);
}

int CountSectorBarriers(
        const SectorCollisionWorld* collisionWorld,
        Vector3 listenerPosition,
        Vector3 sourcePosition)
{
    if (collisionWorld == nullptr) return 0;
    const Vector3 offset = Vector3Subtract(sourcePosition, listenerPosition);
    const float totalDistance = Vector3Length(offset);
    if (totalDistance <= AudioOcclusionEndpointEpsilon) return 0;
    const Vector3 direction = Vector3Scale(offset, 1.0f / totalDistance);
    Vector3 origin = listenerPosition;
    float remaining = totalDistance - AudioOcclusionEndpointEpsilon;
    std::array<uint64_t, AudioTransmissionIterationLimit> seen{};
    int seenCount = 0;

    for (int iteration = 0;
            iteration < AudioTransmissionIterationLimit
                    && remaining > AudioOcclusionEndpointEpsilon;
            ++iteration) {
        const SectorCollisionRayHit hit = collisionWorld->Raycast(
                origin, direction, remaining);
        if (!hit.hit) break;
        const uint64_t key = SurfaceKey(hit);
        bool duplicate = false;
        for (int index = 0; index < seenCount; ++index) {
            if (seen[static_cast<size_t>(index)] == key) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate && seenCount < AudioTransmissionIterationLimit) {
            seen[static_cast<size_t>(seenCount++)] = key;
        }
        const float advance = std::max(
                hit.distance + AudioOcclusionEndpointEpsilon,
                AudioOcclusionEndpointEpsilon);
        origin = Vector3Add(origin, Vector3Scale(direction, advance));
        remaining -= advance;
    }
    return seenCount;
}

SectorSoundPropagationPath EvaluateTransmissionPath(
        const SectorCollisionWorld* collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        Vector3 listenerPosition,
        Vector3 sourcePosition)
{
    SectorSoundPropagationPath path;
    if (!IsFinite(listenerPosition) || !IsFinite(sourcePosition)) return path;
    path.valid = true;
    path.apparentPosition = sourcePosition;
    path.distanceWorld = Vector3Distance(listenerPosition, sourcePosition);
    int barrierCount = CountSectorBarriers(
            collisionWorld, listenerPosition, sourcePosition);
    if (path.distanceWorld > AudioOcclusionEndpointEpsilon) {
        const Vector3 direction = Vector3Normalize(
                Vector3Subtract(sourcePosition, listenerPosition));
        const float queryDistance =
                path.distanceWorld - AudioOcclusionEndpointEpsilon;
        for (const SectorDynamicDoorCollider& door : doorColliders) {
            if (SegmentIntersectsDoorCollider(
                        listenerPosition,
                        sourcePosition,
                        direction,
                        queryDistance,
                        door)) {
                ++barrierCount;
            }
        }
    }
    path.barrierCount = barrierCount;
    path.volumeScale = barrierCount > 0
            ? std::pow(SectorOccludedSoundVolumeScale,
                    static_cast<float>(barrierCount))
            : 1.0f;
    path.lowPassCutoffHz = barrierCount > 0
            ? std::max(
                    SectorMinimumSoundCutoffHz,
                    SectorFirstBarrierSoundCutoffHz
                            * std::pow(0.5f,
                                    static_cast<float>(barrierCount - 1)))
            : SectorUnfilteredSoundCutoffHz;
    return path;
}

Vector3 PortalPoint(
        const RuntimePortalEdge& portal,
        float preferredY)
{
    const float minimumY = std::min(portal.openBottom, portal.openTop);
    const float maximumY = std::max(portal.openBottom, portal.openTop);
    return Vector3{
            (portal.a.x + portal.b.x) * 0.5f,
            std::clamp(preferredY, minimumY, maximumY),
            (portal.a.y + portal.b.y) * 0.5f};
}

float TurnDiffraction(Vector3 previous, Vector3 point, Vector3 next)
{
    Vector3 incoming = Vector3Subtract(point, previous);
    Vector3 outgoing = Vector3Subtract(next, point);
    const float incomingLength = Vector3Length(incoming);
    const float outgoingLength = Vector3Length(outgoing);
    if (incomingLength <= AudioOcclusionEpsilon
            || outgoingLength <= AudioOcclusionEpsilon) return 0.0f;
    incoming = Vector3Scale(incoming, 1.0f / incomingLength);
    outgoing = Vector3Scale(outgoing, 1.0f / outgoingLength);
    return std::acos(std::clamp(
            Vector3DotProduct(incoming, outgoing), -1.0f, 1.0f)) / PI;
}

bool PortalUsable(
        const RuntimePortalEdge& portal,
        const std::vector<RuntimePortalDynamicBlocker>& blockers)
{
    return portal.open
            && !IsRuntimePortalDynamicallyBlocked(portal, &blockers);
}

const SectorSoundPropagationPath& SelectStrongerPath(
        const SectorSoundPropagationResult& result,
        const engine::PositionalSoundSettings& source)
{
    const float transmissionScore = result.transmission.valid
            ? engine::ComputeAudioDistanceAttenuation(
                    result.transmission.distanceWorld, source)
                    * result.transmission.volumeScale
            : -1.0f;
    const float portalScore = result.portal.valid
            ? engine::ComputeAudioDistanceAttenuation(
                    result.portal.distanceWorld, source)
                    * result.portal.volumeScale
            : -1.0f;
    return portalScore > transmissionScore
            ? result.portal : result.transmission;
}

} // namespace

bool SectorSoundPropagationWorld::Build(
        const SectorTopologyMap& map,
        std::string* errorMessage)
{
    Clear();
    if (!BuildRuntimeSectorVisibilityGraph(map, graph, errorMessage)) {
        return false;
    }
    portalDistances.resize(graph.portals.size());
    portalPredecessors.resize(graph.portals.size());
    portalVisited.resize(graph.portals.size());
    pathPortalIndices.reserve(graph.portals.size());
    valid = true;
    return true;
}

void SectorSoundPropagationWorld::Clear()
{
    graph = {};
    portalDistances.clear();
    portalPredecessors.clear();
    portalVisited.clear();
    pathPortalIndices.clear();
    valid = false;
}

SectorSoundPropagationResult SectorSoundPropagationWorld::Evaluate(
        const SectorCollisionWorld* collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<RuntimePortalDynamicBlocker>& portalBlockers,
        Vector3 listenerPosition,
        Vector3 sourcePosition) const
{
    SectorSoundPropagationResult result;
    result.transmission = EvaluateTransmissionPath(
            collisionWorld,
            doorColliders,
            listenerPosition,
            sourcePosition);
    if (!result.transmission.valid) return result;

    if (!valid || collisionWorld == nullptr || graph.portals.empty()) {
        return result;
    }
    const int listenerSectorId = collisionWorld->FindSectorContainingPoint(
            Vector2{listenerPosition.x, listenerPosition.z});
    const int sourceSectorId = collisionWorld->FindSectorContainingPoint(
            Vector2{sourcePosition.x, sourcePosition.z});
    if (listenerSectorId <= 0 || sourceSectorId <= 0) return result;

    const float infinity = std::numeric_limits<float>::infinity();
    std::fill(portalDistances.begin(), portalDistances.end(), infinity);
    std::fill(portalPredecessors.begin(), portalPredecessors.end(), -1);
    std::fill(portalVisited.begin(), portalVisited.end(), 0);
    const float preferredY = (listenerPosition.y + sourcePosition.y) * 0.5f;

    for (size_t index = 0; index < graph.portals.size(); ++index) {
        const RuntimePortalEdge& portal = graph.portals[index];
        if (portal.fromSectorId != listenerSectorId
                || !PortalUsable(portal, portalBlockers)) continue;
        portalDistances[index] = Vector3Distance(
                listenerPosition, PortalPoint(portal, preferredY));
    }

    for (size_t visitedCount = 0;
            visitedCount < graph.portals.size();
            ++visitedCount) {
        int bestIndex = -1;
        float bestDistance = infinity;
        for (size_t index = 0; index < graph.portals.size(); ++index) {
            if (!portalVisited[index]
                    && portalDistances[index] < bestDistance) {
                bestDistance = portalDistances[index];
                bestIndex = static_cast<int>(index);
            }
        }
        if (bestIndex < 0) break;
        portalVisited[static_cast<size_t>(bestIndex)] = 1;
        const RuntimePortalEdge& current =
                graph.portals[static_cast<size_t>(bestIndex)];
        const Vector3 currentPoint = PortalPoint(current, preferredY);
        for (size_t nextIndex = 0;
                nextIndex < graph.portals.size();
                ++nextIndex) {
            const RuntimePortalEdge& next = graph.portals[nextIndex];
            if (next.fromSectorId != current.toSectorId
                    || !PortalUsable(next, portalBlockers)) continue;
            const float candidate = bestDistance + Vector3Distance(
                    currentPoint, PortalPoint(next, preferredY));
            if (candidate < portalDistances[nextIndex]) {
                portalDistances[nextIndex] = candidate;
                portalPredecessors[nextIndex] = bestIndex;
            }
        }
    }

    int finalIndex = -1;
    float finalDistance = infinity;
    for (size_t index = 0; index < graph.portals.size(); ++index) {
        const RuntimePortalEdge& portal = graph.portals[index];
        if (portal.toSectorId != sourceSectorId
                || !std::isfinite(portalDistances[index])) continue;
        const float candidate = portalDistances[index] + Vector3Distance(
                PortalPoint(portal, preferredY), sourcePosition);
        if (candidate < finalDistance) {
            finalDistance = candidate;
            finalIndex = static_cast<int>(index);
        }
    }
    if (finalIndex < 0) return result;

    pathPortalIndices.clear();
    for (int index = finalIndex; index >= 0;
            index = portalPredecessors[static_cast<size_t>(index)]) {
        pathPortalIndices.push_back(index);
    }
    std::reverse(pathPortalIndices.begin(), pathPortalIndices.end());
    if (pathPortalIndices.empty()) return result;

    float diffraction = 0.0f;
    Vector3 previous = listenerPosition;
    for (size_t pathIndex = 0;
            pathIndex < pathPortalIndices.size();
            ++pathIndex) {
        const Vector3 point = PortalPoint(
                graph.portals[static_cast<size_t>(
                        pathPortalIndices[pathIndex])],
                preferredY);
        const Vector3 next = pathIndex + 1 < pathPortalIndices.size()
                ? PortalPoint(
                        graph.portals[static_cast<size_t>(
                                pathPortalIndices[pathIndex + 1])],
                        preferredY)
                : sourcePosition;
        diffraction += TurnDiffraction(previous, point, next);
        previous = point;
    }

    result.portal.valid = true;
    result.portal.apparentPosition = PortalPoint(
            graph.portals[static_cast<size_t>(pathPortalIndices.front())],
            preferredY);
    result.portal.distanceWorld = finalDistance;
    result.portal.portalCount = static_cast<int>(pathPortalIndices.size());
    result.portal.diffractionAmount = diffraction;
    result.portal.volumeScale = std::pow(
            PortalDiffractionVolumeBase, diffraction);
    result.portal.lowPassCutoffHz = std::max(
            PortalMinimumCutoffHz,
            SectorUnfilteredSoundCutoffHz
                    * std::pow(PortalDiffractionCutoffBase, diffraction));
    return result;
}

float ComputeSectorSoundOcclusion(
        const SectorCollisionWorld* collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        Vector3 listenerPosition,
        Vector3 sourcePosition)
{
    const SectorSoundPropagationPath path = EvaluateTransmissionPath(
            collisionWorld,
            doorColliders,
            listenerPosition,
            sourcePosition);
    return path.valid ? path.volumeScale : 1.0f;
}

float QuerySectorSoundOcclusion(
        void* rawContext,
        Vector3 listenerPosition,
        Vector3 sourcePosition)
{
    if (rawContext == nullptr) return 1.0f;
    const SectorAudioOcclusionContext& context =
            *static_cast<const SectorAudioOcclusionContext*>(rawContext);
    static const std::vector<SectorDynamicDoorCollider> noDoors;
    return ComputeSectorSoundOcclusion(
            context.collisionWorld,
            context.doorColliders != nullptr
                    ? *context.doorColliders
                    : noDoors,
            listenerPosition,
            sourcePosition);
}

engine::PositionalSoundPropagation QuerySectorSoundPropagation(
        void* rawContext,
        Vector3 listenerPosition,
        const engine::PositionalSoundSettings& source)
{
    engine::PositionalSoundPropagation output;
    output.apparentPosition = source.position;
    output.distanceWorld = Vector3Distance(listenerPosition, source.position);
    if (rawContext == nullptr) return output;
    const SectorAudioOcclusionContext& context =
            *static_cast<const SectorAudioOcclusionContext*>(rawContext);
    static const std::vector<SectorDynamicDoorCollider> noDoors;
    static const std::vector<RuntimePortalDynamicBlocker> noBlockers;
    if (context.propagationWorld == nullptr) {
        const SectorSoundPropagationPath transmission =
                EvaluateTransmissionPath(
                context.collisionWorld,
                context.doorColliders != nullptr
                        ? *context.doorColliders : noDoors,
                listenerPosition,
                source.position);
        if (transmission.valid) {
            output.distanceWorld = transmission.distanceWorld;
            output.volumeScale = transmission.volumeScale;
            output.lowPassCutoffHz = transmission.lowPassCutoffHz;
        }
        return output;
    }
    const SectorSoundPropagationResult propagation =
            context.propagationWorld->Evaluate(
                    context.collisionWorld,
                    context.doorColliders != nullptr
                            ? *context.doorColliders : noDoors,
                    context.portalBlockers != nullptr
                            ? *context.portalBlockers : noBlockers,
                    listenerPosition,
                    source.position);
    const SectorSoundPropagationPath& selected = SelectStrongerPath(
            propagation, source);
    if (!selected.valid) return output;
    output.apparentPosition = selected.apparentPosition;
    output.distanceWorld = selected.distanceWorld;
    output.volumeScale = selected.volumeScale;
    output.lowPassCutoffHz = selected.lowPassCutoffHz;
    return output;
}

} // namespace game
