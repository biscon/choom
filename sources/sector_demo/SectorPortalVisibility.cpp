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
constexpr float WindowEpsilon = 0.0001f;
constexpr size_t MaxWindowsPerSector = 8;

struct AngularWindow {
    float min = 0.0f;
    float max = 0.0f;
};

struct PortalSpan {
    AngularWindow windows[2];
    size_t count = 0;
    bool full = false;
};

struct PortalVisibilityTest {
    bool visible = false;
    bool uncertain = false;
    AngularWindow clippedWindow;
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

AngularWindow ClampToWindow(AngularWindow window, AngularWindow bounds)
{
    if (!IsFiniteWindow(window) || !IsFiniteWindow(bounds)) {
        return window;
    }

    AngularWindow clamped{
            std::max(window.min, bounds.min),
            std::min(window.max, bounds.max)};
    if (clamped.max + WindowEpsilon < clamped.min) {
        return window;
    }
    if (clamped.max < clamped.min) {
        const float center = (clamped.min + clamped.max) * 0.5f;
        clamped.min = center;
        clamped.max = center;
    }
    return clamped;
}

bool AngleInWindow(float angle, AngularWindow window, float margin)
{
    if (!std::isfinite(angle) || !IsFiniteWindow(window)) {
        return true;
    }
    return angle + margin >= window.min && angle - margin <= window.max;
}

Vector2 DirectionAtRelativeAngle(Vector2 forward, float angle)
{
    const Vector2 right{-forward.y, forward.x};
    return Vector2{
            forward.x * std::cos(angle) + right.x * std::sin(angle),
            forward.y * std::cos(angle) + right.y * std::sin(angle)};
}

bool SegmentIntersectsRay(Vector2 a, Vector2 b, Vector2 rayOrigin, Vector2 rayDirection)
{
    const Vector2 segment{b.x - a.x, b.y - a.y};
    const float denominator = Cross(rayDirection, segment);
    if (!std::isfinite(denominator)) {
        return true;
    }
    if (std::fabs(denominator) <= WindowEpsilon) {
        return false;
    }

    const Vector2 toSegment{a.x - rayOrigin.x, a.y - rayOrigin.y};
    const float rayT = Cross(toSegment, segment) / denominator;
    const float segmentT = Cross(toSegment, rayDirection) / denominator;
    if (!std::isfinite(rayT) || !std::isfinite(segmentT)) {
        return true;
    }

    return rayT >= -WindowEpsilon
            && segmentT >= -WindowEpsilon
            && segmentT <= 1.0f + WindowEpsilon;
}

bool PortalSegmentCrossesWindowBoundary(
        Vector2 origin,
        Vector2 forward,
        const RuntimePortalEdge& edge,
        AngularWindow window)
{
    if (!IsFiniteWindow(window)) {
        return true;
    }

    const Vector2 minDirection = DirectionAtRelativeAngle(forward, window.min);
    const Vector2 maxDirection = DirectionAtRelativeAngle(forward, window.max);
    if (!std::isfinite(minDirection.x)
            || !std::isfinite(minDirection.y)
            || !std::isfinite(maxDirection.x)
            || !std::isfinite(maxDirection.y)) {
        return true;
    }

    return SegmentIntersectsRay(edge.a, edge.b, origin, minDirection)
            || SegmentIntersectsRay(edge.a, edge.b, origin, maxDirection);
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

bool IntersectWindows(AngularWindow a, AngularWindow b, AngularWindow& out, float margin = WindowEpsilon)
{
    if (!IsFiniteWindow(a) || !IsFiniteWindow(b)) {
        out = WidenWindow(a, PortalVisibilityAngularMarginRadians);
        return true;
    }

    out.min = std::max(a.min, b.min);
    out.max = std::min(a.max, b.max);
    if (out.max + margin < out.min) {
        return false;
    }
    if (out.max < out.min) {
        const float center = (out.min + out.max) * 0.5f;
        out.min = center;
        out.max = center;
    }
    return true;
}

bool ContainsWindow(AngularWindow outer, AngularWindow inner)
{
    return outer.min <= inner.min + WindowEpsilon
            && outer.max + WindowEpsilon >= inner.max;
}

bool AddReachedWindow(
        std::unordered_map<int, std::vector<AngularWindow>>& windowsBySector,
        int sectorId,
        AngularWindow window,
        bool* outHitWindowCap = nullptr)
{
    if (outHitWindowCap != nullptr) {
        *outHitWindowCap = false;
    }

    std::vector<AngularWindow>& windows = windowsBySector[sectorId];
    for (const AngularWindow& existing : windows) {
        if (ContainsWindow(existing, window)) {
            return false;
        }
    }

    if (windows.size() >= MaxWindowsPerSector) {
        if (outHitWindowCap != nullptr) {
            *outHitWindowCap = true;
        }
        return false;
    }

    windows.push_back(window);
    return true;
}

PortalVisibilityTest TestPortalAgainstWindow(
        Vector2 origin,
        Vector2 forward,
        const RuntimePortalEdge& edge,
        AngularWindow currentWindow)
{
    PortalVisibilityTest result;
    result.clippedWindow = currentWindow;

    if (!IsFiniteWindow(currentWindow)
            || !std::isfinite(edge.a.x)
            || !std::isfinite(edge.a.y)
            || !std::isfinite(edge.b.x)
            || !std::isfinite(edge.b.y)) {
        result.visible = true;
        result.uncertain = true;
        return result;
    }

    if (DistanceSquaredPointToSegment(origin, edge.a, edge.b)
            <= PortalVisibilityNearDistance * PortalVisibilityNearDistance) {
        result.visible = true;
        return result;
    }

    const Vector2 toA{edge.a.x - origin.x, edge.a.y - origin.y};
    const Vector2 toB{edge.b.x - origin.x, edge.b.y - origin.y};
    if (LengthSquared(toA) <= PortalVisibilityNearDistance * PortalVisibilityNearDistance
            || LengthSquared(toB) <= PortalVisibilityNearDistance * PortalVisibilityNearDistance) {
        result.visible = true;
        return result;
    }

    const float angleA = RelativeAngle(origin, forward, edge.a);
    const float angleB = RelativeAngle(origin, forward, edge.b);
    if (!std::isfinite(angleA) || !std::isfinite(angleB)) {
        result.visible = true;
        result.uncertain = true;
        return result;
    }

    if (AngleInWindow(angleA, currentWindow, PortalVisibilityAngularMarginRadians)
            || AngleInWindow(angleB, currentWindow, PortalVisibilityAngularMarginRadians)) {
        result.visible = true;
    }

    const PortalSpan span = ComputePortalSpan(origin, forward, edge);
    if (span.full) {
        result.visible = true;
        result.clippedWindow = currentWindow;
        return result;
    }

    AngularWindow bestClipped{};
    bool hasClipped = false;
    const AngularWindow paddedCurrent = WidenWindow(currentWindow, PortalVisibilityAngularMarginRadians);
    for (size_t i = 0; i < span.count; ++i) {
        AngularWindow candidate{};
        const AngularWindow paddedSpan = WidenWindow(span.windows[i], PortalVisibilityAngularMarginRadians);
        if (IntersectWindows(paddedCurrent, paddedSpan, candidate, PortalVisibilityAngularMarginRadians)) {
            if (!hasClipped || (candidate.max - candidate.min) > (bestClipped.max - bestClipped.min)) {
                bestClipped = candidate;
            }
            hasClipped = true;
        }
    }

    if (hasClipped) {
        result.visible = true;
        result.clippedWindow = ClampToWindow(bestClipped, currentWindow);
        return result;
    }

    if (PortalSegmentCrossesWindowBoundary(origin, forward, edge, paddedCurrent)) {
        result.visible = true;
        result.clippedWindow = currentWindow;
        return result;
    }

    if (span.count == 0) {
        result.visible = true;
        result.uncertain = true;
        result.clippedWindow = currentWindow;
    }

    return result;
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

    node->outgoingPortalEdgeIndices.push_back(static_cast<int>(graph.portals.size()));
    graph.portals.push_back(edge);
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

RuntimePortalVisibilityResult TraverseRuntimeSectorVisibility(
        const RuntimeSectorVisibilityGraph& graph,
        int startSectorId)
{
    RuntimePortalVisibilityResult result;
    result.startSectorId = startSectorId;
    result.mode = "connected portal traversal";
    result.totalSectorCount = graph.sectors.size();

    if (FindRuntimeSectorVisibilityNode(graph, startSectorId) == nullptr) {
        result.validStartSector = false;
        result.fallbackDrawAll = true;
        result.status = "invalid start sector; fallback draw all";
        return result;
    }

    result.validStartSector = true;
    result.fallbackDrawAll = false;

    std::unordered_set<int> visited;
    std::deque<int> pending;
    visited.insert(startSectorId);
    pending.push_back(startSectorId);

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

            result.traversedPortalLineDefIds.push_back(edge.lineDefId);
            if (visited.insert(edge.toSectorId).second) {
                pending.push_back(edge.toSectorId);
            }
        }
    }

    result.visibleSectorIds.assign(visited.begin(), visited.end());
    SortUnique(result.visibleSectorIds);
    SortUnique(result.traversedPortalLineDefIds);

    if (hitIterationCap) {
        result.fallbackDrawAll = true;
        result.status = "visibility traversal hit iteration cap; fallback draw all";
    } else {
        result.status = "visibility traversal complete";
    }

    return result;
}

RuntimePortalVisibilityResult ComputeRuntimeSectorVisibilityFromPoint(
        const RuntimeSectorVisibilityGraph& graph,
        const SectorCollisionWorld* collisionWorld,
        Vector2 xz,
        int preferredStartSectorId)
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

    return TraverseRuntimeSectorVisibility(graph, startSectorId);
}

RuntimePortalVisibilityResult ComputeRuntimeSectorVisibilityFromView(
        const RuntimeSectorVisibilityGraph& graph,
        const SectorCollisionWorld* collisionWorld,
        Vector2 xz,
        Vector2 forward,
        float horizontalFovRadians,
        int preferredStartSectorId,
        size_t iterationCap)
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

    int startSectorId = 0;
    if (FindRuntimeSectorVisibilityNode(graph, preferredStartSectorId) != nullptr) {
        startSectorId = preferredStartSectorId;
    } else if (collisionWorld != nullptr) {
        startSectorId = collisionWorld->FindSectorContainingPoint(xz);
    } else {
        result = MakeFallbackResult(-1, "sector lookup unavailable; fallback draw all");
        result.mode = "view-aware portal traversal";
        result.totalSectorCount = graph.sectors.size();
        result.visibleSectorIds = AllSectorIds(graph);
        return result;
    }

    result.startSectorId = startSectorId;
    if (startSectorId == 0 || FindRuntimeSectorVisibilityNode(graph, startSectorId) == nullptr) {
        result = MakeFallbackResult(-1, "outside sectors; fallback draw all");
        result.mode = "view-aware portal traversal";
        result.totalSectorCount = graph.sectors.size();
        result.visibleSectorIds = AllSectorIds(graph);
        return result;
    }

    result.validStartSector = true;
    result.fallbackDrawAll = false;

    const float clampedFov = std::clamp(horizontalFovRadians, WindowEpsilon, TwoPi);
    const AngularWindow initialWindow{-clampedFov * 0.5f, clampedFov * 0.5f};

    std::unordered_set<int> visible;
    std::unordered_map<int, std::vector<AngularWindow>> windowsBySector;
    std::deque<ViewTraversalItem> pending;

    visible.insert(startSectorId);
    AddReachedWindow(windowsBySector, startSectorId, initialWindow);
    pending.push_back(ViewTraversalItem{startSectorId, initialWindow});

    const size_t cap = iterationCap == 0
            ? std::max<size_t>(graph.sectors.size() + graph.portals.size(), 1) * 8
            : iterationCap;
    size_t iterations = 0;
    bool hitIterationCap = false;
    bool hitWindowCap = false;

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
            if (!edge.open) {
                continue;
            }

            const PortalVisibilityTest portalVisibility =
                    TestPortalAgainstWindow(xz, normalizedForward, edge, item.window);
            if (!portalVisibility.visible) {
                continue;
            }

            result.traversedPortalLineDefIds.push_back(edge.lineDefId);
            visible.insert(edge.toSectorId);
            bool windowCapForSector = false;
            if (AddReachedWindow(
                        windowsBySector,
                        edge.toSectorId,
                        portalVisibility.clippedWindow,
                        &windowCapForSector)) {
                pending.push_back(ViewTraversalItem{edge.toSectorId, portalVisibility.clippedWindow});
            } else if (windowCapForSector) {
                hitWindowCap = true;
                break;
            }
        }

        if (hitWindowCap) {
            break;
        }
    }

    if (hitIterationCap) {
        result.fallbackDrawAll = true;
        result.status = "portal traversal cap hit";
        result.visibleSectorIds = AllSectorIds(graph);
    } else if (hitWindowCap) {
        result.fallbackDrawAll = true;
        result.status = "portal visibility window cap hit";
        result.visibleSectorIds = AllSectorIds(graph);
    } else {
        result.visibleSectorIds.assign(visible.begin(), visible.end());
        result.status = "visibility traversal complete";
    }

    SortUnique(result.visibleSectorIds);
    SortUnique(result.traversedPortalLineDefIds);
    return result;
}

std::string FormatRuntimePortalVisibilityDebugText(
        const RuntimePortalVisibilityResult& result)
{
    std::ostringstream text;
    text << "start sector: ";
    if (result.validStartSector) {
        text << result.startSectorId;
    } else {
        text << "none";
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
