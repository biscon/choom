#include "sector_demo/SectorPortalVisibility.h"

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace game {
namespace {

constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = Pi * 2.0f;
constexpr float PortalVisibilityAngularMarginRadians = Pi / 90.0f;
constexpr float PortalVisibilityNearDistance = 0.125f;
constexpr float kMaxVisibilitySeedRadiusWorld = 0.5f;
constexpr float VisibilitySeedVerticalToleranceWorld = 0.75f;
constexpr float WindowEpsilon = 0.0001f;

struct AngularWindow {
    float min = 0.0f;
    float max = 0.0f;
};

struct PortalSpan {
    AngularWindow windows[2];
    size_t count = 0;
    bool full = false;
};

struct ViewTraversalItem {
    int sectorId = -1;
    AngularWindow window;
};

bool SetError(std::string* outError, const std::string& message)
{
    if (outError != nullptr) {
        *outError = message;
    }
    return false;
}

std::string IdText(int id)
{
    return std::to_string(id);
}

RuntimeSectorNode* FindNode(RuntimeSectorVisibilityGraph& graph, int sectorId)
{
    for (RuntimeSectorNode& node : graph.sectors) {
        if (node.sectorId == sectorId) {
            return &node;
        }
    }
    return nullptr;
}

void SortUnique(std::vector<int>& values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

void FinalizeVisibilitySectorSets(RuntimePortalVisibilityResult& result)
{
    SortUnique(result.visibleSectorIds);
    if (result.fallbackDrawAll) {
        result.boundarySurfaceSectorIds.clear();
        return;
    }

    SortUnique(result.boundarySurfaceSectorIds);
    result.boundarySurfaceSectorIds.erase(
            std::remove_if(
                    result.boundarySurfaceSectorIds.begin(),
                    result.boundarySurfaceSectorIds.end(),
                    [&result](int sectorId) {
                        return std::binary_search(
                                result.visibleSectorIds.begin(),
                                result.visibleSectorIds.end(),
                                sectorId);
                    }),
            result.boundarySurfaceSectorIds.end());
}

std::vector<int> AllSectorIds(const RuntimeSectorVisibilityGraph& graph)
{
    std::vector<int> ids;
    ids.reserve(graph.sectors.size());
    for (const RuntimeSectorNode& node : graph.sectors) {
        ids.push_back(node.sectorId);
    }
    SortUnique(ids);
    return ids;
}

RuntimePortalVisibilityResult MakeFallbackResult(int startSectorId, const std::string& status)
{
    RuntimePortalVisibilityResult result;
    result.startSectorId = startSectorId;
    if (startSectorId > 0) {
        result.startSectorIds.push_back(startSectorId);
    }
    result.validStartSector = false;
    result.fallbackDrawAll = true;
    result.status = status;
    return result;
}

float NormalizeAngle(float angle)
{
    while (angle <= -Pi) {
        angle += TwoPi;
    }
    while (angle > Pi) {
        angle -= TwoPi;
    }
    return angle;
}

float Dot(Vector2 a, Vector2 b)
{
    return a.x * b.x + a.y * b.y;
}

float Cross(Vector2 a, Vector2 b)
{
    return a.x * b.y - a.y * b.x;
}

float LengthSquared(Vector2 value)
{
    return value.x * value.x + value.y * value.y;
}

bool Normalize(Vector2 value, Vector2& out)
{
    const float lengthSq = LengthSquared(value);
    if (!std::isfinite(lengthSq) || lengthSq <= std::numeric_limits<float>::epsilon()) {
        return false;
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    out = Vector2{value.x * invLength, value.y * invLength};
    return std::isfinite(out.x) && std::isfinite(out.y);
}

bool IsVerticallyPlausibleSeed(
        const SectorCollisionWorld* collisionWorld,
        int sectorId,
        float eyeYWorld,
        bool validateEyeY,
        bool preferredSeed)
{
    if (!validateEyeY || collisionWorld == nullptr) {
        return true;
    }
    if (preferredSeed) {
        return true;
    }

    SectorCollisionHeights heights;
    if (!collisionWorld->GetSectorFloorCeiling(sectorId, &heights)) {
        return true;
    }
    return eyeYWorld + VisibilitySeedVerticalToleranceWorld >= heights.floorZ
            && eyeYWorld - VisibilitySeedVerticalToleranceWorld <= heights.ceilingZ;
}

int FindPointSeed(
        const RuntimeSectorVisibilityGraph& graph,
        const SectorCollisionWorld* collisionWorld,
        Vector2 xz,
        float eyeYWorld,
        bool validateEyeY)
{
    if (collisionWorld == nullptr) {
        return 0;
    }
    const int sectorId = collisionWorld->FindSectorContainingPoint(xz);
    if (sectorId == 0
            || FindRuntimeSectorVisibilityNode(graph, sectorId) == nullptr
            || !IsVerticallyPlausibleSeed(collisionWorld, sectorId, eyeYWorld, validateEyeY, false)) {
        return 0;
    }
    return sectorId;
}

bool HasOpenPortalBetween(
        const RuntimeSectorVisibilityGraph& graph,
        int fromSectorId,
        int toSectorId,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers)
{
    const RuntimeSectorNode* node = FindRuntimeSectorVisibilityNode(graph, fromSectorId);
    if (node == nullptr) {
        return false;
    }

    for (int edgeIndex : node->outgoingPortalEdgeIndices) {
        if (edgeIndex < 0 || static_cast<size_t>(edgeIndex) >= graph.portals.size()) {
            continue;
        }
        const RuntimePortalEdge& edge = graph.portals[static_cast<size_t>(edgeIndex)];
        if (edge.open
                && edge.toSectorId == toSectorId
                && !IsRuntimePortalDynamicallyBlocked(edge, dynamicBlockers)) {
            return true;
        }
    }
    return false;
}

std::vector<int> DeduplicateSortedSeeds(
        const RuntimeSectorVisibilityGraph& graph,
        const std::vector<int>& rawSeeds)
{
    std::vector<int> seeds;
    seeds.reserve(rawSeeds.size());
    for (int seed : rawSeeds) {
        if (FindRuntimeSectorVisibilityNode(graph, seed) != nullptr) {
            seeds.push_back(seed);
        }
    }
    SortUnique(seeds);
    return seeds;
}

struct GatheredRuntimeVisibilitySeeds {
    std::vector<int> sectorIds;
    int primarySectorId = -1;
};

GatheredRuntimeVisibilitySeeds GatherRuntimeVisibilityStartSeeds(
        const RuntimeSectorVisibilityGraph& graph,
        const SectorCollisionWorld* collisionWorld,
        Vector2 xz,
        int preferredStartSectorId,
        float visibilitySeedRadiusWorld,
        float eyeYWorld,
        bool validateEyeY,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers)
{
    GatheredRuntimeVisibilitySeeds gathered;
    std::vector<int>& seeds = gathered.sectorIds;
    seeds.reserve(6);

    const int cameraSectorId = FindPointSeed(
            graph,
            collisionWorld,
            xz,
            eyeYWorld,
            validateEyeY);
    if (cameraSectorId > 0) {
        seeds.push_back(cameraSectorId);
        gathered.primarySectorId = cameraSectorId;
    }

    if (FindRuntimeSectorVisibilityNode(graph, preferredStartSectorId) != nullptr) {
        if (seeds.empty()) {
            seeds.push_back(preferredStartSectorId);
            gathered.primarySectorId = preferredStartSectorId;
        } else if (preferredStartSectorId == cameraSectorId
                || HasOpenPortalBetween(
                        graph,
                        cameraSectorId,
                        preferredStartSectorId,
                        dynamicBlockers)) {
            seeds.push_back(preferredStartSectorId);
        }
    }

    const auto appendConnectedPointSeed = [
            &graph,
            collisionWorld,
            &seeds,
            eyeYWorld,
            validateEyeY,
            dynamicBlockers,
            &gathered](Vector2 sample) {
        const int sampledSectorId = FindPointSeed(
                graph,
                collisionWorld,
                sample,
                eyeYWorld,
                validateEyeY);
        if (sampledSectorId == 0
                || std::find(seeds.begin(), seeds.end(), sampledSectorId) != seeds.end()) {
            return;
        }

        if (seeds.empty()) {
            seeds.push_back(sampledSectorId);
            gathered.primarySectorId = sampledSectorId;
            return;
        }

        bool connectedToAcceptedSeed = false;
        for (const int seed : seeds) {
            if (HasOpenPortalBetween(
                            graph,
                            seed,
                            sampledSectorId,
                            dynamicBlockers)) {
                connectedToAcceptedSeed = true;
                break;
            }
        }
        if (connectedToAcceptedSeed) {
            seeds.push_back(sampledSectorId);
        }
    };

    if (collisionWorld != nullptr
            && std::isfinite(visibilitySeedRadiusWorld)
            && visibilitySeedRadiusWorld > 0.0f) {
        const Vector2 offsets[] = {
                Vector2{visibilitySeedRadiusWorld, 0.0f},
                Vector2{-visibilitySeedRadiusWorld, 0.0f},
                Vector2{0.0f, visibilitySeedRadiusWorld},
                Vector2{0.0f, -visibilitySeedRadiusWorld}};

        for (Vector2 offset : offsets) {
            const Vector2 sample{xz.x + offset.x, xz.y + offset.y};
            appendConnectedPointSeed(sample);
        }
    }

    gathered.sectorIds = DeduplicateSortedSeeds(graph, seeds);
    return gathered;
}

float RelativeAngle(Vector2 origin, Vector2 forward, Vector2 point)
{
    Vector2 toPoint{point.x - origin.x, point.y - origin.y};
    Vector2 direction{};
    if (!Normalize(toPoint, direction)) {
        return 0.0f;
    }
    return std::atan2(Cross(forward, direction), Dot(forward, direction));
}

float DistanceSquaredPointToSegment(Vector2 point, Vector2 a, Vector2 b)
{
    const Vector2 ab{b.x - a.x, b.y - a.y};
    const float lengthSq = LengthSquared(ab);
    if (lengthSq <= std::numeric_limits<float>::epsilon()) {
        const Vector2 ap{point.x - a.x, point.y - a.y};
        return LengthSquared(ap);
    }

    const Vector2 ap{point.x - a.x, point.y - a.y};
    const float t = std::clamp(Dot(ap, ab) / lengthSq, 0.0f, 1.0f);
    const Vector2 closest{a.x + ab.x * t, a.y + ab.y * t};
    const Vector2 delta{point.x - closest.x, point.y - closest.y};
    return LengthSquared(delta);
}

AngularWindow ClampWindow(AngularWindow window)
{
    window.min = std::clamp(window.min, -Pi, Pi);
    window.max = std::clamp(window.max, -Pi, Pi);
    if (window.min > window.max) {
        std::swap(window.min, window.max);
    }
    return window;
}

AngularWindow WidenWindow(AngularWindow window, float margin)
{
    window.min -= margin;
    window.max += margin;
    return ClampWindow(window);
}

bool IsFiniteWindow(AngularWindow window)
{
    return std::isfinite(window.min) && std::isfinite(window.max) && window.min <= window.max;
}

PortalSpan ComputePortalSpan(Vector2 origin, Vector2 forward, const RuntimePortalEdge& edge)
{
    PortalSpan span;
    if (!std::isfinite(edge.a.x)
            || !std::isfinite(edge.a.y)
            || !std::isfinite(edge.b.x)
            || !std::isfinite(edge.b.y)) {
        span.full = true;
        return span;
    }

    if (DistanceSquaredPointToSegment(origin, edge.a, edge.b)
            <= PortalVisibilityNearDistance * PortalVisibilityNearDistance) {
        span.full = true;
        return span;
    }

    const Vector2 toA{edge.a.x - origin.x, edge.a.y - origin.y};
    const Vector2 toB{edge.b.x - origin.x, edge.b.y - origin.y};
    if (LengthSquared(toA) <= PortalVisibilityNearDistance * PortalVisibilityNearDistance
            || LengthSquared(toB) <= PortalVisibilityNearDistance * PortalVisibilityNearDistance) {
        span.full = true;
        return span;
    }

    const float angleA = RelativeAngle(origin, forward, edge.a);
    const float angleB = RelativeAngle(origin, forward, edge.b);
    if (!std::isfinite(angleA) || !std::isfinite(angleB)) {
        span.full = true;
        return span;
    }

    const float delta = NormalizeAngle(angleB - angleA);
    if (!std::isfinite(delta)) {
        span.full = true;
        return span;
    }

    if (std::fabs(delta) >= Pi - WindowEpsilon) {
        span.full = true;
        return span;
    }

    const bool crossesPositivePi = angleA + delta > Pi;
    const bool crossesNegativePi = angleA + delta <= -Pi;
    if (!crossesPositivePi && !crossesNegativePi) {
        const float end = angleA + delta;
        span.windows[0] = AngularWindow{std::min(angleA, end), std::max(angleA, end)};
        span.count = 1;
        return span;
    }

    if (delta > 0.0f) {
        span.windows[0] = AngularWindow{angleA, Pi};
        span.windows[1] = AngularWindow{-Pi, NormalizeAngle(angleA + delta)};
    } else {
        span.windows[0] = AngularWindow{NormalizeAngle(angleA + delta), Pi};
        span.windows[1] = AngularWindow{-Pi, angleA};
    }
    span.count = 2;
    return span;
}

bool IntersectWindows(AngularWindow a, AngularWindow b, AngularWindow& out)
{
    if (!IsFiniteWindow(a) || !IsFiniteWindow(b)) {
        out = a;
        return true;
    }

    out.min = std::max(a.min, b.min);
    out.max = std::min(a.max, b.max);
    return out.min <= out.max;
}

bool ContainsWindow(AngularWindow outer, AngularWindow inner)
{
    return outer.min <= inner.min && outer.max >= inner.max;
}

bool AddReachedWindow(
        std::unordered_map<int, std::vector<AngularWindow>>& windowsBySector,
        int sectorId,
        AngularWindow window)
{
    std::vector<AngularWindow>& windows = windowsBySector[sectorId];
    for (const AngularWindow& existing : windows) {
        if (ContainsWindow(existing, window)) {
            return false;
        }
    }

    AngularWindow reached = window;
    for (std::size_t i = 0; i < windows.size();) {
        const AngularWindow existing = windows[i];
        if (reached.max < existing.min || existing.max < reached.min) {
            ++i;
            continue;
        }
        reached.min = std::min(reached.min, existing.min);
        reached.max = std::max(reached.max, existing.max);
        windows.erase(windows.begin() + static_cast<std::ptrdiff_t>(i));
        i = 0; // The enlarged union may now overlap an earlier interval.
    }

    windows.push_back(reached);
    return true;
}

PortalSpan TestPortalAgainstWindow(
        Vector2 origin,
        Vector2 forward,
        const RuntimePortalEdge& edge,
        AngularWindow currentWindow)
{
    PortalSpan result;
    const PortalSpan span = ComputePortalSpan(origin, forward, edge);
    if (span.full || !IsFiniteWindow(currentWindow)) {
        result.windows[0] = currentWindow;
        result.count = 1;
        return result;
    }

    for (size_t i = 0; i < span.count; ++i) {
        AngularWindow candidate{};
        // Numerical tolerance belongs to the tested portal, never the inherited
        // window. Every child remains a subset of its parent's opening.
        if (IntersectWindows(currentWindow, WidenWindow(span.windows[i], WindowEpsilon), candidate)) {
            result.windows[result.count++] = candidate;
        }
    }
    return result;
}

bool PortalFacesCamera(Vector2 origin, const RuntimePortalEdge& edge)
{
    if (!edge.hasFacing) return true;
    const Vector2 direction{edge.b.x - edge.a.x, edge.b.y - edge.a.y};
    const Vector2 toEye{origin.x - edge.a.x, origin.y - edge.a.y};
    const float side = Cross(direction, toEye);
    // Crossing a doorway/footprint-seeding its neighbor remains conservative.
    return !std::isfinite(side)
            || side >= -PortalVisibilityNearDistance * std::sqrt(LengthSquared(direction));
}

// Clip the portal's segment parameter to a positive half-plane. Values are
// signed world distances; inset the occluded region to preserve grazing views.
bool ClipOccludedInterval(float a, float b, RuntimePortalSegmentInterval& interval)
{
    constexpr float inset = 0.0001f;
    a -= inset;
    b -= inset;
    if (!std::isfinite(a) || !std::isfinite(b)) return false;
    if (a < 0 && b < 0) return false;
    if (a >= 0 && b >= 0) return true;
    const float t = a / (a - b);
    if (a < 0) interval.min = std::max(interval.min, t);
    else interval.max = std::min(interval.max, t);
    return interval.min < interval.max;
}

bool OccludedPortalInterval(Vector2 origin, float eyeY, const RuntimePortalEdge& edge,
                            const RuntimeVisibilityWall& wall,
                            RuntimePortalSegmentInterval& interval)
{
    // Only discard sightlines when the wall covers the entire vertical envelope
    // from the eye to the aperture. Low walls / elevated cameras stay conservative.
    if (!std::isfinite(eyeY) || !std::isfinite(wall.bottom) || !std::isfinite(wall.top)
            || !std::isfinite(edge.openBottom) || !std::isfinite(edge.openTop)
            || wall.bottom > std::min(eyeY, edge.openBottom)
            || wall.top < std::max(eyeY, edge.openTop)) return false;

    Vector2 rayA{}, rayB{}, wallDirection{};
    if (!Normalize({wall.a.x - origin.x, wall.a.y - origin.y}, rayA)
            || !Normalize({wall.b.x - origin.x, wall.b.y - origin.y}, rayB)
            || !Normalize({wall.b.x - wall.a.x, wall.b.y - wall.a.y}, wallDirection))
        return false;
    const float side = Cross(wallDirection, {origin.x - wall.a.x, origin.y - wall.a.y});
    const float winding = Cross(rayA, rayB);
    if (std::fabs(side) <= PortalVisibilityNearDistance || std::fabs(winding) < WindowEpsilon)
        return false;
    const float sign = winding > 0 ? 1.0f : -1.0f;
    const float farSign = side > 0 ? -1.0f : 1.0f;
    const Vector2 toA{edge.a.x - origin.x, edge.a.y - origin.y};
    const Vector2 toB{edge.b.x - origin.x, edge.b.y - origin.y};
    interval = {0, 1};
    return ClipOccludedInterval(sign * Cross(rayA, toA), sign * Cross(rayA, toB), interval)
            && ClipOccludedInterval(sign * Cross(toA, rayB), sign * Cross(toB, rayB), interval)
            && ClipOccludedInterval(
                    farSign * Cross(wallDirection, {edge.a.x - wall.a.x, edge.a.y - wall.a.y}),
                    farSign * Cross(wallDirection, {edge.b.x - wall.a.x, edge.b.y - wall.a.y}),
                    interval);
}

void ClipPortalAgainstWalls(Vector2 origin, float eyeY, const RuntimePortalEdge& edge,
                            const RuntimeSectorNode& node, RuntimePortalVisibilityScratch& scratch)
{
    scratch.fragments.clear();
    scratch.fragments.push_back({0, 1});
    for (const auto& wall : node.walls) {
        RuntimePortalSegmentInterval hidden;
        if (!OccludedPortalInterval(origin, eyeY, edge, wall, hidden)) continue;
        scratch.nextFragments.clear();
        for (const auto& fragment : scratch.fragments) {
            if (hidden.max <= fragment.min || hidden.min >= fragment.max) {
                scratch.nextFragments.push_back(fragment);
                continue;
            }
            if (hidden.min > fragment.min)
                scratch.nextFragments.push_back({fragment.min, hidden.min});
            if (hidden.max < fragment.max)
                scratch.nextFragments.push_back({hidden.max, fragment.max});
        }
        scratch.fragments.swap(scratch.nextFragments);
        if (scratch.fragments.empty()) break;
    }
}

bool AppendDirectedPortal(
        RuntimeSectorVisibilityGraph& graph,
        const SectorTopologyLineDef& lineDef,
        const SectorTopologySideDef& fromSideDef,
        const SectorTopologySideDef& toSideDef,
        const SectorTopologySector& fromSector,
        const SectorTopologySector& toSector,
        Vector2 a,
        Vector2 b)
{
    RuntimeSectorNode* node = FindNode(graph, fromSector.id);
    if (node == nullptr) {
        return false;
    }

    const float openBottom = std::max(
            SectorAuthoringToWorldDistance(fromSector.floorZ),
            SectorAuthoringToWorldDistance(toSector.floorZ));
    const float openTop = std::min(
            SectorAuthoringToWorldDistance(fromSector.ceilingZ),
            SectorAuthoringToWorldDistance(toSector.ceilingZ));

    RuntimePortalEdge edge;
    edge.lineDefId = lineDef.id;
    edge.sideDefId = fromSideDef.id;
    edge.fromSectorId = fromSideDef.sectorId;
    edge.toSectorId = toSideDef.sectorId;
    edge.a = a;
    edge.b = b;
    edge.openBottom = openBottom;
    edge.openTop = openTop;
    edge.open = openBottom < openTop;
    edge.hasFacing = true;

    node->outgoingPortalEdgeIndices.push_back(static_cast<int>(graph.portals.size()));
    graph.portals.push_back(edge);
    // Match generated lower/upper wall strips, including sky-sky suppression.
    // A closed height interval alone is not proof of an opaque full-height wall.
    if (toSector.floorZ > fromSector.floorZ) {
        node->walls.push_back({a, b, SectorAuthoringToWorldDistance(fromSector.floorZ),
                              SectorAuthoringToWorldDistance(toSector.floorZ)});
    }
    if (!(fromSector.ceilingSky && toSector.ceilingSky)
            && toSector.ceilingZ < fromSector.ceilingZ) {
        node->walls.push_back({a, b, SectorAuthoringToWorldDistance(toSector.ceilingZ),
                              SectorAuthoringToWorldDistance(fromSector.ceilingZ)});
    }
    return true;
}

bool BuildLookupTables(
        const SectorTopologyMap& map,
        std::unordered_map<int, const SectorTopologyVertex*>& verticesById,
        std::unordered_map<int, const SectorTopologySideDef*>& sideDefsById,
        std::unordered_map<int, const SectorTopologySector*>& sectorsById,
        std::string* outError)
{
    for (const SectorTopologyVertex& vertex : map.vertices) {
        if (!IsValidSectorTopologyId(vertex.id)) {
            return SetError(outError, "visibility graph has invalid vertex id");
        }
        if (!verticesById.emplace(vertex.id, &vertex).second) {
            return SetError(outError, "visibility graph has duplicate vertex id " + IdText(vertex.id));
        }
    }

    for (const SectorTopologySideDef& sideDef : map.sideDefs) {
        if (!IsValidSectorTopologyId(sideDef.id)) {
            return SetError(outError, "visibility graph has invalid sidedef id");
        }
        if (!sideDefsById.emplace(sideDef.id, &sideDef).second) {
            return SetError(outError, "visibility graph has duplicate sidedef id " + IdText(sideDef.id));
        }
    }

    for (const SectorTopologySector& sector : map.sectors) {
        if (!IsValidSectorTopologyId(sector.id)) {
            return SetError(outError, "visibility graph has invalid sector id");
        }
        if (!sectorsById.emplace(sector.id, &sector).second) {
            return SetError(outError, "visibility graph has duplicate sector id " + IdText(sector.id));
        }
    }

    return true;
}

} // namespace

void ReserveRuntimePortalVisibilityScratch(
        const RuntimeSectorVisibilityGraph& graph, RuntimePortalVisibilityScratch& scratch)
{
    std::size_t capacity = 1;
    for (const auto& node : graph.sectors) capacity = std::max(capacity, node.walls.size() + 1);
    scratch.fragments.reserve(capacity);
    scratch.nextFragments.reserve(capacity);
}

const RuntimeSectorNode* FindRuntimeSectorVisibilityNode(
        const RuntimeSectorVisibilityGraph& graph,
        int sectorId)
{
    for (const RuntimeSectorNode& node : graph.sectors) {
        if (node.sectorId == sectorId) {
            return &node;
        }
    }
    return nullptr;
}

bool BuildRuntimeSectorVisibilityGraph(
        const SectorTopologyMap& map,
        RuntimeSectorVisibilityGraph& outGraph,
        std::string* outError)
{
    outGraph = RuntimeSectorVisibilityGraph{};
    if (outError != nullptr) {
        outError->clear();
    }

    std::unordered_map<int, const SectorTopologyVertex*> verticesById;
    std::unordered_map<int, const SectorTopologySideDef*> sideDefsById;
    std::unordered_map<int, const SectorTopologySector*> sectorsById;
    if (!BuildLookupTables(map, verticesById, sideDefsById, sectorsById, outError)) {
        outGraph = RuntimeSectorVisibilityGraph{};
        return false;
    }

    outGraph.sectors.reserve(map.sectors.size());
    for (const SectorTopologySector& sector : map.sectors) {
        RuntimeSectorNode node;
        node.sectorId = sector.id;
        outGraph.sectors.push_back(node);
    }

    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        if (!IsValidSectorTopologyId(lineDef.id)) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph has invalid linedef id");
        }

        const auto startIt = verticesById.find(lineDef.startVertexId);
        const auto endIt = verticesById.find(lineDef.endVertexId);
        if (startIt == verticesById.end() || endIt == verticesById.end()) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph linedef " + IdText(lineDef.id)
                                      + " references a missing vertex");
        }

        const bool hasFront = IsValidSectorTopologyId(lineDef.frontSideDefId);
        const bool hasBack = IsValidSectorTopologyId(lineDef.backSideDefId);
        if (!hasFront && !hasBack) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph linedef " + IdText(lineDef.id)
                                      + " has no sidedefs");
        }
        if (!hasFront || !hasBack) {
            const auto side = sideDefsById.find(hasFront ? lineDef.frontSideDefId : lineDef.backSideDefId);
            if (side == sideDefsById.end() || side->second->lineDefId != lineDef.id
                    || sectorsById.find(side->second->sectorId) == sectorsById.end()) {
                outGraph = RuntimeSectorVisibilityGraph{};
                return SetError(outError, "visibility wall has a missing or invalid sidedef/sector");
            }
            const auto& sector = *sectorsById.at(side->second->sectorId);
            FindNode(outGraph, sector.id)->walls.push_back({
                    SectorCoordToWorldPosition2(startIt->second->x, startIt->second->y),
                    SectorCoordToWorldPosition2(endIt->second->x, endIt->second->y),
                    SectorAuthoringToWorldDistance(sector.floorZ),
                    SectorAuthoringToWorldDistance(sector.ceilingZ)});
            continue;
        }

        const auto frontIt = sideDefsById.find(lineDef.frontSideDefId);
        const auto backIt = sideDefsById.find(lineDef.backSideDefId);
        if (frontIt == sideDefsById.end() || backIt == sideDefsById.end()) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph linedef " + IdText(lineDef.id)
                                      + " references a missing sidedef");
        }

        const SectorTopologySideDef& frontSideDef = *frontIt->second;
        const SectorTopologySideDef& backSideDef = *backIt->second;
        if (frontSideDef.lineDefId != lineDef.id || backSideDef.lineDefId != lineDef.id) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph linedef " + IdText(lineDef.id)
                                      + " references a sidedef owned by another linedef");
        }

        const auto frontSectorIt = sectorsById.find(frontSideDef.sectorId);
        const auto backSectorIt = sectorsById.find(backSideDef.sectorId);
        if (frontSectorIt == sectorsById.end() || backSectorIt == sectorsById.end()) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph linedef " + IdText(lineDef.id)
                                      + " references a missing sector through a sidedef");
        }

        const Vector2 start = SectorCoordToWorldPosition2(startIt->second->x, startIt->second->y);
        const Vector2 end = SectorCoordToWorldPosition2(endIt->second->x, endIt->second->y);

        if (!AppendDirectedPortal(
                    outGraph,
                    lineDef,
                    frontSideDef,
                    backSideDef,
                    *frontSectorIt->second,
                    *backSectorIt->second,
                    start,
                    end)
                || !AppendDirectedPortal(
                    outGraph,
                    lineDef,
                    backSideDef,
                    frontSideDef,
                    *backSectorIt->second,
                    *frontSectorIt->second,
                    end,
                    start)) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph failed to attach portal for linedef "
                                      + IdText(lineDef.id));
        }
    }

    return true;
}

RuntimePortalVisibilityResult TraverseRuntimeSectorVisibilityFromSeeds(
        const RuntimeSectorVisibilityGraph& graph,
        const std::vector<int>& startSectorIds,
        int preferredStartSectorId,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers)
{
    RuntimePortalVisibilityResult result;
    result.startSectorIds = DeduplicateSortedSeeds(graph, startSectorIds);
    result.startSectorId = std::binary_search(
            result.startSectorIds.begin(),
            result.startSectorIds.end(),
            preferredStartSectorId)
            ? preferredStartSectorId
            : result.startSectorIds.empty() ? -1 : result.startSectorIds.front();
    result.mode = "connected portal multi-seed traversal";
    result.totalSectorCount = graph.sectors.size();

    if (result.startSectorIds.empty()) {
        result.validStartSector = false;
        result.fallbackDrawAll = true;
        result.status = "invalid start sectors; fallback draw all";
        return result;
    }

    result.validStartSector = true;
    result.fallbackDrawAll = false;

    std::unordered_set<int> visited;
    std::deque<int> pending;
    for (const int startSectorId : result.startSectorIds) {
        visited.insert(startSectorId);
        pending.push_back(startSectorId);
    }

    const size_t iterationCap = std::max<size_t>(graph.sectors.size() + graph.portals.size(), 1) * 4;
    size_t iterations = 0;
    bool hitIterationCap = false;

    while (!pending.empty()) {
        if (++iterations > iterationCap) {
            hitIterationCap = true;
            break;
        }

        const int sectorId = pending.front();
        pending.pop_front();
        const RuntimeSectorNode* node = FindRuntimeSectorVisibilityNode(graph, sectorId);
        if (node == nullptr) {
            continue;
        }

        for (const int edgeIndex : node->outgoingPortalEdgeIndices) {
            if (edgeIndex < 0 || static_cast<size_t>(edgeIndex) >= graph.portals.size()) {
                continue;
            }

            const RuntimePortalEdge& edge = graph.portals[static_cast<size_t>(edgeIndex)];
            if (!edge.open) {
                continue;
            }
            if (IsRuntimePortalDynamicallyBlocked(edge, dynamicBlockers)) {
                result.boundarySurfaceSectorIds.push_back(edge.toSectorId);
                continue;
            }

            result.traversedPortalLineDefIds.push_back(edge.lineDefId);
            if (visited.insert(edge.toSectorId).second) {
                pending.push_back(edge.toSectorId);
            }
        }
    }

    result.visibleSectorIds.assign(visited.begin(), visited.end());
    SortUnique(result.traversedPortalLineDefIds);

    if (hitIterationCap) {
        result.fallbackDrawAll = true;
        result.status = "visibility traversal hit iteration cap; fallback draw all";
    } else {
        result.status = "visibility traversal complete";
    }
    FinalizeVisibilitySectorSets(result);

    return result;
}

RuntimePortalVisibilityResult TraverseRuntimeSectorVisibility(
        const RuntimeSectorVisibilityGraph& graph,
        int startSectorId,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers)
{
    RuntimePortalVisibilityResult result = TraverseRuntimeSectorVisibilityFromSeeds(
            graph,
            std::vector<int>{startSectorId},
            startSectorId,
            dynamicBlockers);
    result.mode = "connected portal traversal";
    return result;
}

RuntimePortalVisibilityResult ComputeRuntimeSectorVisibilityFromPoint(
        const RuntimeSectorVisibilityGraph& graph,
        const SectorCollisionWorld* collisionWorld,
        Vector2 xz,
        int preferredStartSectorId,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers)
{
    int startSectorId = 0;
    if (FindRuntimeSectorVisibilityNode(graph, preferredStartSectorId) != nullptr) {
        startSectorId = preferredStartSectorId;
    } else if (collisionWorld != nullptr) {
        startSectorId = collisionWorld->FindSectorContainingPoint(xz);
    } else {
        return MakeFallbackResult(-1, "sector lookup unavailable; fallback draw all");
    }

    if (startSectorId == 0) {
        return MakeFallbackResult(-1, "outside sectors; fallback draw all");
    }

    return TraverseRuntimeSectorVisibility(graph, startSectorId, dynamicBlockers);
}

float ClampRuntimeVisibilitySeedRadiusWorld(float playerRadiusWorld)
{
    if (!std::isfinite(playerRadiusWorld) || playerRadiusWorld <= 0.0f) {
        return 0.0f;
    }
    return std::min(playerRadiusWorld, kMaxVisibilitySeedRadiusWorld);
}

RuntimePortalVisibilityResult ComputeRuntimeSectorCaptureVisibility(
        const RuntimeSectorVisibilityGraph& graph, const Camera3D& camera,
        const RuntimePortalVisibilityResult& connected,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers,
        size_t iterationCap,
        RuntimePortalVisibilityScratch* scratch)
{
    const Vector2 forward{camera.target.x - camera.position.x,
                          camera.target.z - camera.position.z};
    if (!connected.validStartSector || connected.fallbackDrawAll
            || !std::isfinite(forward.x) || !std::isfinite(forward.y)
            || forward.x * forward.x + forward.y * forward.y < 0.000001f
            || !std::isfinite(camera.fovy) || camera.fovy <= 0 || camera.fovy >= 180)
        return connected;
    auto visible = ComputeRuntimeSectorVisibilityFromViewSeeds(graph,
            {camera.position.x, camera.position.z}, forward, camera.fovy * Pi / 180.0f,
            connected.startSectorIds, connected.startSectorId, iterationCap, dynamicBlockers,
            camera.position.y, scratch);
    return !visible.validStartSector || visible.fallbackDrawAll ? connected : visible;
}

float ComputeRuntimePortalVisibilityHorizontalFovRadians(
        float verticalFovRadians,
        float aspectRatio,
        float pitchRadians)
{
    if (!std::isfinite(verticalFovRadians)
            || !std::isfinite(aspectRatio)
            || !std::isfinite(pitchRadians)
            || verticalFovRadians <= 0.0f
            || verticalFovRadians >= Pi
            || aspectRatio <= 0.0f) {
        return TwoPi;
    }

    const float absolutePitch = std::fabs(pitchRadians);
    if (absolutePitch >= Pi * 0.5f - WindowEpsilon) {
        return TwoPi;
    }

    const float verticalTangent = std::tan(verticalFovRadians * 0.5f);
    const float horizontalTangent = verticalTangent * aspectRatio;
    if (!std::isfinite(verticalTangent)
            || !std::isfinite(horizontalTangent)
            || horizontalTangent <= 0.0f) {
        return TwoPi;
    }

    // Pitch tilts one horizontal edge of the perspective frustum back toward
    // the camera in XZ. Use that edge to conservatively cover every screen ray.
    const float minimumForward = std::cos(absolutePitch)
            - verticalTangent * std::sin(absolutePitch);
    if (!std::isfinite(minimumForward) || minimumForward <= WindowEpsilon) {
        // The frustum contains a vertical ray, so its XZ projection covers all
        // azimuths even though its ordinary level-camera FOV does not.
        return TwoPi;
    }

    const float horizontalFov = 2.0f * std::atan2(horizontalTangent, minimumForward);
    if (!std::isfinite(horizontalFov) || horizontalFov <= 0.0f) {
        return TwoPi;
    }
    return std::clamp(horizontalFov, WindowEpsilon, TwoPi);
}

RuntimePortalVisibilityResult ComputeRuntimeSectorVisibilityFromViewSeeds(
        const RuntimeSectorVisibilityGraph& graph,
        Vector2 xz,
        Vector2 forward,
        float horizontalFovRadians,
        const std::vector<int>& startSectorIds,
        int preferredStartSectorId,
        size_t iterationCap,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers,
        float eyeYWorld,
        RuntimePortalVisibilityScratch* scratch)
{
    RuntimePortalVisibilityResult result;
    result.mode = "view-aware portal traversal";
    result.totalSectorCount = graph.sectors.size();

    Vector2 normalizedForward{};
    if (!Normalize(forward, normalizedForward)
            || !std::isfinite(horizontalFovRadians)
            || horizontalFovRadians <= 0.0f) {
        result = MakeFallbackResult(-1, "invalid view input; fallback draw all");
        result.mode = "view-aware portal traversal";
        result.totalSectorCount = graph.sectors.size();
        result.visibleSectorIds = AllSectorIds(graph);
        return result;
    }

    const std::vector<int> seeds = DeduplicateSortedSeeds(graph, startSectorIds);
    if (seeds.empty()) {
        result = MakeFallbackResult(-1, "outside sectors; fallback draw all");
        result.mode = "view-aware portal traversal";
        result.totalSectorCount = graph.sectors.size();
        result.visibleSectorIds = AllSectorIds(graph);
        return result;
    }

    result.startSectorIds = seeds;
    result.startSectorId = std::binary_search(seeds.begin(), seeds.end(), preferredStartSectorId)
            ? preferredStartSectorId
            : seeds.front();
    result.validStartSector = true;
    result.fallbackDrawAll = false;

    const float clampedFov = std::clamp(horizontalFovRadians, WindowEpsilon, TwoPi);
    // A small FOV guard is applied once, never recursively at each doorway.
    const AngularWindow initialWindow = WidenWindow(
            {-clampedFov * 0.5f, clampedFov * 0.5f}, PortalVisibilityAngularMarginRadians);

    RuntimePortalVisibilityScratch localScratch;
    if (!scratch) scratch = &localScratch;
    ReserveRuntimePortalVisibilityScratch(graph, *scratch);

    std::unordered_set<int> visible;
    std::unordered_map<int, std::vector<AngularWindow>> windowsBySector;
    std::deque<ViewTraversalItem> pending;

    for (int seed : seeds) {
        visible.insert(seed);
        AddReachedWindow(windowsBySector, seed, initialWindow);
        pending.push_back(ViewTraversalItem{seed, initialWindow});
    }

    const size_t cap = iterationCap == 0
            ? std::max<size_t>(graph.sectors.size() + graph.portals.size(), 1) * 8
            : iterationCap;
    size_t iterations = 0;
    bool hitIterationCap = false;

    while (!pending.empty()) {
        if (++iterations > cap) {
            hitIterationCap = true;
            break;
        }

        const ViewTraversalItem item = pending.front();
        pending.pop_front();

        const RuntimeSectorNode* node = FindRuntimeSectorVisibilityNode(graph, item.sectorId);
        if (node == nullptr) {
            continue;
        }

        for (const int edgeIndex : node->outgoingPortalEdgeIndices) {
            if (edgeIndex < 0 || static_cast<size_t>(edgeIndex) >= graph.portals.size()) {
                continue;
            }

            const RuntimePortalEdge& edge = graph.portals[static_cast<size_t>(edgeIndex)];
            if (!edge.open || !PortalFacesCamera(xz, edge)) {
                continue;
            }

            if (TestPortalAgainstWindow(xz, normalizedForward, edge, item.window).count == 0)
                continue;
            ClipPortalAgainstWalls(xz, eyeYWorld, edge, *node, *scratch);
            for (const auto& fragment : scratch->fragments) {
                RuntimePortalEdge clippedEdge = edge;
                clippedEdge.a = {edge.a.x + (edge.b.x - edge.a.x) * fragment.min,
                                 edge.a.y + (edge.b.y - edge.a.y) * fragment.min};
                clippedEdge.b = {edge.a.x + (edge.b.x - edge.a.x) * fragment.max,
                                 edge.a.y + (edge.b.y - edge.a.y) * fragment.max};
                const auto windows = TestPortalAgainstWindow(xz, normalizedForward, clippedEdge, item.window);
                if (windows.count == 0) continue;
                if (IsRuntimePortalDynamicallyBlocked(edge, dynamicBlockers)) {
                    result.boundarySurfaceSectorIds.push_back(edge.toSectorId);
                    break;
                }
                result.traversedPortalLineDefIds.push_back(edge.lineDefId);
                visible.insert(edge.toSectorId);
                for (size_t i = 0; i < windows.count; ++i) {
                    if (AddReachedWindow(windowsBySector, edge.toSectorId, windows.windows[i])) {
                        // Enqueue this path only, not the accumulated sector-wide union.
                        pending.push_back({edge.toSectorId, windows.windows[i]});
                    }
                }
            }
        }
    }

    if (hitIterationCap) {
        RuntimePortalVisibilityResult connected =
                TraverseRuntimeSectorVisibilityFromSeeds(
                        graph,
                        seeds,
                        result.startSectorId,
                        dynamicBlockers);
        connected.mode = "view-aware portal traversal";
        connected.status = connected.fallbackDrawAll
                ? "portal traversal cap hit; connected fallback failed"
                : "portal traversal cap hit; fallback connected component";
        return connected;
    } else {
        result.visibleSectorIds.assign(visible.begin(), visible.end());
        result.status = "visibility traversal complete";
    }

    FinalizeVisibilitySectorSets(result);
    SortUnique(result.traversedPortalLineDefIds);
    return result;
}

RuntimePortalVisibilityResult ComputeRuntimeSectorVisibilityFromView(
        const RuntimeSectorVisibilityGraph& graph,
        const SectorCollisionWorld* collisionWorld,
        Vector2 xz,
        Vector2 forward,
        float horizontalFovRadians,
        int preferredStartSectorId,
        size_t iterationCap,
        float visibilitySeedRadiusWorld,
        float eyeYWorld,
        bool validateEyeY,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers,
        RuntimePortalVisibilityScratch* scratch)
{
    if (collisionWorld == nullptr
            && FindRuntimeSectorVisibilityNode(graph, preferredStartSectorId) == nullptr) {
        RuntimePortalVisibilityResult result =
                MakeFallbackResult(-1, "sector lookup unavailable; fallback draw all");
        result.mode = "view-aware portal traversal";
        result.totalSectorCount = graph.sectors.size();
        result.visibleSectorIds = AllSectorIds(graph);
        return result;
    }

    const GatheredRuntimeVisibilitySeeds seeds = GatherRuntimeVisibilityStartSeeds(
            graph,
            collisionWorld,
            xz,
            preferredStartSectorId,
            ClampRuntimeVisibilitySeedRadiusWorld(visibilitySeedRadiusWorld),
            eyeYWorld,
            validateEyeY,
            dynamicBlockers);

    return ComputeRuntimeSectorVisibilityFromViewSeeds(
            graph,
            xz,
            forward,
            horizontalFovRadians,
            seeds.sectorIds,
            seeds.primarySectorId,
            iterationCap,
            dynamicBlockers,
            eyeYWorld,
            scratch);
}

bool IsRuntimePortalDynamicallyBlocked(
        const RuntimePortalEdge& edge,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicBlockers)
{
    if (dynamicBlockers == nullptr) {
        return false;
    }

    for (const RuntimePortalDynamicBlocker& blocker : *dynamicBlockers) {
        if (!blocker.blocksPortal || blocker.lineDefId != edge.lineDefId) {
            continue;
        }

        if (blocker.sideDefId > 0 && blocker.sideDefId == edge.sideDefId) {
            return true;
        }

        if (blocker.fromSectorId > 0
                && blocker.toSectorId > 0
                && blocker.fromSectorId == edge.fromSectorId
                && blocker.toSectorId == edge.toSectorId) {
            return true;
        }
    }

    return false;
}

std::string FormatRuntimePortalVisibilityDebugText(
        const RuntimePortalVisibilityResult& result)
{
    std::ostringstream text;
    if (result.validStartSector) {
        if (result.startSectorIds.empty()) {
            text << "start sector: ";
            text << result.startSectorId;
        } else {
            text << (result.startSectorIds.size() == 1 ? "start sector: " : "start sectors: ");
            for (size_t i = 0; i < result.startSectorIds.size(); ++i) {
                if (i != 0) {
                    text << ",";
                }
                text << result.startSectorIds[i];
            }
        }
    } else {
        text << "start sector: none";
    }
    text << " | visible sectors: ";
    for (size_t i = 0; i < result.visibleSectorIds.size(); ++i) {
        if (i != 0) {
            text << ",";
        }
        text << result.visibleSectorIds[i];
    }
    text << " | visible count: " << result.visibleSectorIds.size();
    if (result.totalSectorCount > 0) {
        text << " / " << result.totalSectorCount;
    }
    if (!result.boundarySurfaceSectorIds.empty()) {
        text << " | boundary surfaces: ";
        for (size_t i = 0; i < result.boundarySurfaceSectorIds.size(); ++i) {
            if (i != 0) {
                text << ",";
            }
            text << result.boundarySurfaceSectorIds[i];
        }
    }
    if (!result.mode.empty()) {
        text << " | mode: " << result.mode;
    }
    text << " | fallback: ";
    if (result.fallbackDrawAll) {
        text << (result.status.empty() ? "draw all" : result.status);
    } else {
        text << "none";
    }
    if (!result.fallbackDrawAll && !result.status.empty()) {
        text << " | " << result.status;
    }
    return text.str();
}

} // namespace game
