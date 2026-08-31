#include "sector_editor/SectorEditorAuthoringState.h"

#include "sector_demo/SectorTopologyGeometry.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/services/runtime_objects/SectorEditorAuthoringDoorReconciliation.h"
#include "util/earcut.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace game {

bool HasAuthoringGraphData(const SectorAuthoringGraph& graph)
{
    return !graph.vertices.empty()
            || !graph.lines.empty()
            || !graph.lineSides.empty()
            || !graph.faceAnchors.empty()
            || !graph.fogVolumes.empty()
            || !graph.levelMarkers.empty()
            || !graph.patrols.empty()
            || !graph.soundEmitters.empty()
            || !graph.triggers.empty();
}

namespace {

struct SectorEditorAuthoringSegmentInsertResult {
    std::vector<SectorEditorAuthoringLineSegmentResult> segments;
    std::string errorMessage;
};

void CopyEditorMapLevelFields(SectorTopologyMap& target, const SectorTopologyMap& source)
{
    target.resolvedMaterialsById = source.resolvedMaterialsById;
    target.staticLights = source.staticLights;
    target.staticSpotLights = source.staticSpotLights;
    target.staticRectLights = source.staticRectLights;
    target.dynamicPointLights = source.dynamicPointLights;
    target.dynamicSpotLights = source.dynamicSpotLights;
    target.dynamicRectLights = source.dynamicRectLights;
    target.runtimeObjects = source.runtimeObjects;
    target.previewSettings = source.previewSettings;
    target.skySettings = source.skySettings;
    target.directionalLight = source.directionalLight;
    target.fogSettings = source.fogSettings;
    target.lightmapSettings = source.lightmapSettings;
    target.bakedLightmap = source.bakedLightmap;
    target.bakedReflectionProbes = source.bakedReflectionProbes;
}

void InvalidateEditorTopologyRenderCache(
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache)
{
    topologyRenderCache.valid = false;
    ++topologyRenderRevision;
}

void InvalidateEditorTopologyRenderCache(SectorEditorState& state)
{
    InvalidateEditorTopologyRenderCache(state.topologyRenderRevision, state.topologyRenderCache);
}

void InvalidateEditorTopologyRenderCacheIfNeeded(
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache)
{
    if (topologyRenderCache.valid) {
        InvalidateEditorTopologyRenderCache(topologyRenderRevision, topologyRenderCache);
        return;
    }
    topologyRenderCache.valid = false;
}

void InvalidateEditorTopologyRenderCacheIfNeeded(SectorEditorState& state)
{
    InvalidateEditorTopologyRenderCacheIfNeeded(state.topologyRenderRevision, state.topologyRenderCache);
}

SectorAuthoringSelectionTarget EmptyAuthoringSelectionTarget()
{
    return SectorAuthoringSelectionTarget{};
}

bool CanUseCurrentAuthoringDerivedTopology(
        SectorEditorAuthoringDerivationState derivationState,
        bool derivedTopologyStale,
        const SectorAuthoringDerivationResult& authoringDerivation,
        const char* action,
        std::string* outMessage)
{
    const auto setMessage = [outMessage, action](const char* reason) {
        if (outMessage != nullptr) {
            *outMessage = TextFormat("%s requires current valid derived topology: %s", action, reason);
        }
    };

    if (derivationState == SectorEditorAuthoringDerivationState::ValidCurrent
            && !derivedTopologyStale
            && authoringDerivation.success) {
        if (outMessage != nullptr) {
            outMessage->clear();
        }
        return true;
    }

    if (derivationState == SectorEditorAuthoringDerivationState::InvalidLastValid) {
        setMessage("latest derivation failed; fix authoring diagnostics first");
        return false;
    }

    if (derivationState == SectorEditorAuthoringDerivationState::InvalidNoDerived) {
        setMessage("no valid derived topology is available");
        return false;
    }

    if (derivationState == SectorEditorAuthoringDerivationState::ValidStale
            || derivedTopologyStale) {
        setMessage("authoring graph changed; re-derive before using runtime topology");
        return false;
    }

    setMessage("no valid derived topology is available");
    return false;
}

bool CurrentAuthoringDerivationAvailable(
        bool authoringDerivationCurrent,
        const SectorAuthoringDerivationResult& authoringDerivation)
{
    return authoringDerivationCurrent && authoringDerivation.success;
}

SectorEditorInspectorTarget UnavailableInspectorTarget(const char* status)
{
    SectorEditorInspectorTarget target;
    target.kind = SectorEditorInspectorTargetKind::AuthoringUnavailable;
    target.status = status == nullptr ? "" : status;
    return target;
}

SectorEditorInspectorTarget ResolveMappedTopologySectorInspectorTarget(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        int topologySectorId)
{
    if (!CurrentAuthoringDerivationAvailable(authoringDerivationCurrent, authoringDerivation)) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: derived topology is not current");
    }
    if (FindSectorTopologySector(topologyMap, topologySectorId) == nullptr) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: selected sector is not current");
    }

    bool found = false;
    int faceAnchorId = -1;
    for (const SectorAuthoringDerivedSectorMapping& mapping : authoringDerivation.mapping.sectors) {
        if (mapping.topologySectorId != topologySectorId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.faceAnchorId)
                || FindSectorAuthoringFaceAnchor(authoringGraph, mapping.faceAnchorId) == nullptr) {
            continue;
        }
        if (found && faceAnchorId != mapping.faceAnchorId) {
            return UnavailableInspectorTarget("Authoring inspector unavailable: selected sector has ambiguous face anchor mapping");
        }
        faceAnchorId = mapping.faceAnchorId;
        found = true;
    }
    if (!found) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: selected sector has no face anchor mapping");
    }

    SectorEditorInspectorTarget target;
    target.kind = SectorEditorInspectorTargetKind::AuthoringFaceAnchor;
    target.faceAnchorId = faceAnchorId;
    return target;
}

SectorEditorInspectorTarget ResolveMappedTopologySideInspectorTarget(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        int topologySideDefId)
{
    if (!CurrentAuthoringDerivationAvailable(authoringDerivationCurrent, authoringDerivation)) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: derived topology is not current");
    }
    if (FindSectorTopologySideDef(topologyMap, topologySideDefId) == nullptr) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: selected sidedef is not current");
    }

    bool found = false;
    SectorAuthoringSideId sideId;
    for (const SectorAuthoringDerivedSideMapping& mapping : authoringDerivation.mapping.sides) {
        if (mapping.topologySideDefId != topologySideDefId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.authoringLineId)
                || FindSectorAuthoringLine(authoringGraph, mapping.authoringLineId) == nullptr) {
            continue;
        }
        const SectorAuthoringSideId candidate{mapping.authoringLineId, mapping.authoringSide};
        if (found && (sideId.lineId != candidate.lineId || sideId.side != candidate.side)) {
            return UnavailableInspectorTarget("Authoring inspector unavailable: selected sidedef has ambiguous authoring side mapping");
        }
        sideId = candidate;
        found = true;
    }
    if (!found) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: selected sidedef has no authoring side mapping");
    }

    SectorEditorInspectorTarget target;
    target.kind = SectorEditorInspectorTargetKind::AuthoringLine;
    target.lineId = sideId.lineId;
    target.side = sideId;
    return target;
}

SectorEditorInspectorTarget ResolveMappedTopologyLineInspectorTarget(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        int topologyLineDefId)
{
    if (!CurrentAuthoringDerivationAvailable(authoringDerivationCurrent, authoringDerivation)) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: derived topology is not current");
    }
    if (FindSectorTopologyLineDef(topologyMap, topologyLineDefId) == nullptr) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: selected linedef is not current");
    }

    bool found = false;
    int authoringLineId = -1;
    for (const SectorAuthoringDerivedLineMapping& mapping : authoringDerivation.mapping.lines) {
        if (mapping.topologyLineDefId != topologyLineDefId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.authoringLineId)
                || FindSectorAuthoringLine(authoringGraph, mapping.authoringLineId) == nullptr) {
            continue;
        }
        if (found && authoringLineId != mapping.authoringLineId) {
            return UnavailableInspectorTarget("Authoring inspector unavailable: selected linedef has ambiguous authoring line mapping");
        }
        authoringLineId = mapping.authoringLineId;
        found = true;
    }
    if (!found) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: selected linedef has no authoring line mapping");
    }

    SectorEditorInspectorTarget target;
    target.kind = SectorEditorInspectorTargetKind::AuthoringLine;
    target.lineId = authoringLineId;
    return target;
}

bool PointInTopologyLoop(
        const SectorTopologyMap& map,
        Vector2 mapPoint,
        const SectorTopologyLoop& loop)
{
    std::vector<SectorPoint> points;
    points.reserve(loop.vertexIds.size());
    for (int vertexId : loop.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
        if (vertex == nullptr) {
            return false;
        }
        points.push_back(Vector2ToSectorPoint(SectorTopologyVertexToMap(*vertex)));
    }

    const SectorPoint point = Vector2ToSectorPoint(mapPoint);
    return StrictPointInPolygon(point, points) || PointOnPolygonBoundary(point, points);
}

bool PointStrictlyInTopologyLoop(
        const SectorTopologyMap& map,
        Vector2 mapPoint,
        const SectorTopologyLoop& loop)
{
    std::vector<SectorPoint> points;
    points.reserve(loop.vertexIds.size());
    for (int vertexId : loop.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
        if (vertex == nullptr) {
            return false;
        }
        points.push_back(Vector2ToSectorPoint(SectorTopologyVertexToMap(*vertex)));
    }

    return StrictPointInPolygon(Vector2ToSectorPoint(mapPoint), points);
}

bool PointInTopologySector(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        Vector2 mapPoint,
        int sectorId)
{
    SectorTopologyLoopSet loops;
    std::vector<SectorTopologyValidationIssue> loopIssues;
    if (!ExtractSectorTopologyLoops(map, indexes, sectorId, loops, &loopIssues)) {
        return false;
    }
    if (!PointInTopologyLoop(map, mapPoint, loops.outer)) {
        return false;
    }
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (PointInTopologyLoop(map, mapPoint, hole)) {
            return false;
        }
    }
    return true;
}

bool PointStrictlyInTopologySector(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        Vector2 mapPoint,
        int sectorId)
{
    SectorTopologyLoopSet loops;
    std::vector<SectorTopologyValidationIssue> loopIssues;
    if (!ExtractSectorTopologyLoops(map, indexes, sectorId, loops, &loopIssues)) {
        return false;
    }
    if (!PointStrictlyInTopologyLoop(map, mapPoint, loops.outer)) {
        return false;
    }
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (PointInTopologyLoop(map, mapPoint, hole)) {
            return false;
        }
    }
    return true;
}

bool TryComputeTopologySectorArea(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        int sectorId,
        double* outArea)
{
    if (outArea != nullptr) {
        *outArea = 0.0;
    }

    SectorTopologyLoopSet loops;
    std::vector<SectorTopologyValidationIssue> loopIssues;
    if (!ExtractSectorTopologyLoops(map, indexes, sectorId, loops, &loopIssues)) {
        return false;
    }

    double area = std::fabs(static_cast<double>(loops.outer.signedAreaTwice)) * 0.5;
    for (const SectorTopologyLoop& hole : loops.holes) {
        area -= std::fabs(static_cast<double>(hole.signedAreaTwice)) * 0.5;
    }
    if (area <= 0.0) {
        return false;
    }

    if (outArea != nullptr) {
        *outArea = area;
    }
    return true;
}

bool IsGeneratedSectorName(const std::string& name)
{
    constexpr const char* prefix = "Sector ";
    constexpr std::size_t prefixLength = 7;
    if (name.size() <= prefixLength || name.compare(0, prefixLength, prefix) != 0) {
        return false;
    }
    for (std::size_t i = prefixLength; i < name.size(); ++i) {
        if (name[i] < '0' || name[i] > '9') {
            return false;
        }
    }
    return true;
}

std::string AllocateGeneratedFaceAnchorName(const SectorAuthoringGraph& graph)
{
    std::set<std::string> usedNames;
    for (const SectorAuthoringFaceAnchor& anchor : graph.faceAnchors) {
        if (!anchor.name.empty()) {
            usedNames.insert(anchor.name);
        }
    }

    for (int id = 1; id < 10000; ++id) {
        const std::string candidate = "Sector " + std::to_string(id);
        if (usedNames.find(candidate) == usedNames.end()) {
            return candidate;
        }
    }
    return "Sector " + std::to_string(graph.faceAnchors.size() + 1);
}

bool TryFindInteriorPointForTopologySector(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        int topologySectorId,
        SectorTopologyCoordPoint* outPoint)
{
    if (outPoint != nullptr) {
        *outPoint = SectorTopologyCoordPoint{};
    }

    SectorTopologyLoopSet loops;
    std::vector<SectorTopologyValidationIssue> loopIssues;
    if (!ExtractSectorTopologyLoops(map, indexes, topologySectorId, loops, &loopIssues)
            || loops.outer.vertexIds.empty()) {
        return false;
    }

    SectorCoord minX = 0;
    SectorCoord maxX = 0;
    SectorCoord minY = 0;
    SectorCoord maxY = 0;
    bool hasBounds = false;
    for (int vertexId : loops.outer.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
        if (vertex == nullptr) {
            return false;
        }
        if (!hasBounds) {
            minX = maxX = vertex->x;
            minY = maxY = vertex->y;
            hasBounds = true;
        } else {
            minX = std::min(minX, vertex->x);
            maxX = std::max(maxX, vertex->x);
            minY = std::min(minY, vertex->y);
            maxY = std::max(maxY, vertex->y);
        }
    }
    if (!hasBounds || minX >= maxX || minY >= maxY) {
        return false;
    }

    const auto pointSegmentDistanceSquared = [](
            double px,
            double py,
            const SectorTopologyVertex& start,
            const SectorTopologyVertex& end) {
        const double ax = static_cast<double>(start.x);
        const double ay = static_cast<double>(start.y);
        const double bx = static_cast<double>(end.x);
        const double by = static_cast<double>(end.y);
        const double dx = bx - ax;
        const double dy = by - ay;
        const double lengthSquared = dx * dx + dy * dy;
        if (lengthSquared <= 0.0) {
            const double offsetX = px - ax;
            const double offsetY = py - ay;
            return offsetX * offsetX + offsetY * offsetY;
        }
        const double t = std::clamp(
                ((px - ax) * dx + (py - ay) * dy) / lengthSquared,
                0.0,
                1.0);
        const double closestX = ax + t * dx;
        const double closestY = ay + t * dy;
        const double offsetX = px - closestX;
        const double offsetY = py - closestY;
        return offsetX * offsetX + offsetY * offsetY;
    };
    const auto boundaryClearanceSquared = [&](SectorCoord x, SectorCoord y) {
        double clearanceSquared = std::numeric_limits<double>::max();
        const auto scoreLoop = [&](const SectorTopologyLoop& loop) {
            if (loop.vertexIds.size() < 2) {
                return false;
            }
            for (std::size_t index = 0; index < loop.vertexIds.size(); ++index) {
                const SectorTopologyVertex* start =
                        FindSectorTopologyVertex(map, loop.vertexIds[index]);
                const SectorTopologyVertex* end = FindSectorTopologyVertex(
                        map,
                        loop.vertexIds[(index + 1) % loop.vertexIds.size()]);
                if (start == nullptr || end == nullptr) {
                    return false;
                }
                clearanceSquared = std::min(
                        clearanceSquared,
                        pointSegmentDistanceSquared(
                                static_cast<double>(x),
                                static_cast<double>(y),
                                *start,
                                *end));
            }
            return true;
        };
        if (!scoreLoop(loops.outer)) {
            return -1.0;
        }
        for (const SectorTopologyLoop& hole : loops.holes) {
            if (!scoreLoop(hole)) {
                return -1.0;
            }
        }
        return clearanceSquared;
    };

    bool found = false;
    SectorTopologyCoordPoint bestPoint{};
    double bestClearanceSquared = -1.0;
    const auto considerPoint = [&](SectorCoord x, SectorCoord y) {
        const Vector2 mapPoint{
                SectorCoordToVisibleAuthoring(x),
                SectorCoordToVisibleAuthoring(y)};
        if (!PointStrictlyInTopologySector(map, indexes, mapPoint, topologySectorId)) {
            return;
        }
        const double clearanceSquared = boundaryClearanceSquared(x, y);
        if (clearanceSquared < 0.0) {
            return;
        }
        constexpr double scoreEpsilon = 1.0e-9;
        if (!found
                || clearanceSquared > bestClearanceSquared + scoreEpsilon
                || (std::fabs(clearanceSquared - bestClearanceSquared) <= scoreEpsilon
                        && (y < bestPoint.y || (y == bestPoint.y && x < bestPoint.x)))) {
            found = true;
            bestPoint = SectorTopologyCoordPoint{x, y};
            bestClearanceSquared = clearanceSquared;
        }
    };

    const SectorCoord boundsCenterX = static_cast<SectorCoord>(
            static_cast<int64_t>(minX)
            + (static_cast<int64_t>(maxX) - static_cast<int64_t>(minX)) / 2);
    const SectorCoord boundsCenterY = static_cast<SectorCoord>(
            static_cast<int64_t>(minY)
            + (static_cast<int64_t>(maxY) - static_cast<int64_t>(minY)) / 2);
    considerPoint(boundsCenterX, boundsCenterY);

    constexpr int divisions = 32;
    for (int yIndex = 1; yIndex < divisions; ++yIndex) {
        const SectorCoord y = static_cast<SectorCoord>(
                static_cast<int64_t>(minY)
                + (static_cast<int64_t>(maxY) - static_cast<int64_t>(minY)) * yIndex / divisions);
        for (int xIndex = 1; xIndex < divisions; ++xIndex) {
            const SectorCoord x = static_cast<SectorCoord>(
                    static_cast<int64_t>(minX)
                    + (static_cast<int64_t>(maxX) - static_cast<int64_t>(minX)) * xIndex / divisions);
            considerPoint(x, y);
        }
    }

    using EarcutPoint = std::array<double, 2>;
    std::vector<std::vector<EarcutPoint>> polygon;
    std::vector<SectorTopologyCoordPoint> flattened;
    const auto appendLoop = [&](const SectorTopologyLoop& loop) {
        polygon.emplace_back();
        polygon.back().reserve(loop.vertexIds.size());
        for (int vertexId : loop.vertexIds) {
            const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
            if (vertex == nullptr) {
                return false;
            }
            polygon.back().push_back(EarcutPoint{
                    static_cast<double>(vertex->x),
                    static_cast<double>(vertex->y)});
            flattened.push_back(SectorTopologyCoordPoint{vertex->x, vertex->y});
        }
        return true;
    };
    if (!appendLoop(loops.outer)) {
        return false;
    }
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (!appendLoop(hole)) {
            return false;
        }
    }

    const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);
    for (std::size_t index = 0; index + 2 < indices.size(); index += 3) {
        if (indices[index] >= flattened.size()
                || indices[index + 1] >= flattened.size()
                || indices[index + 2] >= flattened.size()) {
            continue;
        }
        const SectorTopologyCoordPoint a = flattened[indices[index]];
        const SectorTopologyCoordPoint b = flattened[indices[index + 1]];
        const SectorTopologyCoordPoint c = flattened[indices[index + 2]];
        const double oppositeA = std::hypot(
                static_cast<double>(b.x) - c.x,
                static_cast<double>(b.y) - c.y);
        const double oppositeB = std::hypot(
                static_cast<double>(a.x) - c.x,
                static_cast<double>(a.y) - c.y);
        const double oppositeC = std::hypot(
                static_cast<double>(a.x) - b.x,
                static_cast<double>(a.y) - b.y);
        const double perimeter = oppositeA + oppositeB + oppositeC;
        if (perimeter <= 0.0) {
            continue;
        }
        const double incenterX =
                (oppositeA * a.x + oppositeB * b.x + oppositeC * c.x) / perimeter;
        const double incenterY =
                (oppositeA * a.y + oppositeB * b.y + oppositeC * c.y) / perimeter;
        const SectorCoord roundedX = static_cast<SectorCoord>(std::llround(incenterX));
        const SectorCoord roundedY = static_cast<SectorCoord>(std::llround(incenterY));
        for (SectorCoord yOffset = -1; yOffset <= 1; ++yOffset) {
            for (SectorCoord xOffset = -1; xOffset <= 1; ++xOffset) {
                considerPoint(
                        static_cast<SectorCoord>(roundedX + xOffset),
                        static_cast<SectorCoord>(roundedY + yOffset));
            }
        }
    }

    if (!found) {
        return false;
    }
    constexpr std::array<SectorTopologyCoordPoint, 8> interiorOffsets{{
            {-1, -2},
            {1, -2},
            {-2, -1},
            {2, -1},
            {-2, 1},
            {2, 1},
            {-1, 2},
            {1, 2},
    }};
    for (SectorTopologyCoordPoint offset : interiorOffsets) {
        const SectorTopologyCoordPoint candidate{
                static_cast<SectorCoord>(bestPoint.x + offset.x),
                static_cast<SectorCoord>(bestPoint.y + offset.y)};
        const Vector2 mapPoint{
                SectorCoordToVisibleAuthoring(candidate.x),
                SectorCoordToVisibleAuthoring(candidate.y)};
        if (!PointStrictlyInTopologySector(
                    map,
                    indexes,
                    mapPoint,
                    topologySectorId)) {
            continue;
        }
        const double clearanceSquared =
                boundaryClearanceSquared(candidate.x, candidate.y);
        if (clearanceSquared >= bestClearanceSquared * 0.75) {
            bestPoint = candidate;
            break;
        }
    }
    if (outPoint != nullptr) {
        *outPoint = bestPoint;
    }
    return true;
}

const SectorAuthoringExtractedFace* FindExtractedFaceById(
        const SectorAuthoringFaceExtractionResult& faces,
        int faceId)
{
    for (const SectorAuthoringExtractedFace& face : faces.faces) {
        if (face.id == faceId) {
            return &face;
        }
    }
    return nullptr;
}

bool AuthoringSideIdLess(SectorAuthoringSideId lhs, SectorAuthoringSideId rhs)
{
    if (lhs.lineId != rhs.lineId) {
        return lhs.lineId < rhs.lineId;
    }
    return static_cast<int>(lhs.side) < static_cast<int>(rhs.side);
}

std::vector<SectorAuthoringSideId> BuildFaceBoundaryIdentity(
        const SectorAuthoringExtractedFace& face)
{
    std::vector<SectorAuthoringSideId> identity;
    identity.reserve(face.boundary.size());
    for (const SectorAuthoringFaceBoundaryEdge& boundary : face.boundary) {
        if (!IsValidSectorAuthoringId(boundary.sourceLineId)) {
            continue;
        }
        identity.push_back(SectorAuthoringSideId{
                boundary.sourceLineId,
                boundary.sourceSide});
    }
    std::sort(identity.begin(), identity.end(), AuthoringSideIdLess);
    identity.erase(
            std::unique(
                    identity.begin(),
                    identity.end(),
                    [](SectorAuthoringSideId lhs, SectorAuthoringSideId rhs) {
                        return SectorAuthoringSideIdsEqual(lhs, rhs);
                    }),
            identity.end());
    return identity;
}

bool FaceBoundaryIdentitiesEqual(
        const std::vector<SectorAuthoringSideId>& lhs,
        const std::vector<SectorAuthoringSideId>& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (!SectorAuthoringSideIdsEqual(lhs[index], rhs[index])) {
            return false;
        }
    }
    return true;
}

const SectorAuthoringExtractedFace* FindUniqueFaceWithBoundaryIdentity(
        const SectorAuthoringFaceExtractionResult& faces,
        const std::vector<SectorAuthoringSideId>& boundarySides,
        bool& outAmbiguous)
{
    outAmbiguous = false;
    const SectorAuthoringExtractedFace* matchedFace = nullptr;
    for (const SectorAuthoringExtractedFace& face : faces.faces) {
        if (!FaceBoundaryIdentitiesEqual(
                    boundarySides,
                    BuildFaceBoundaryIdentity(face))) {
            continue;
        }
        if (matchedFace != nullptr) {
            outAmbiguous = true;
            return nullptr;
        }
        matchedFace = &face;
    }
    return matchedFace;
}

std::vector<SectorEditorFaceAnchorBinding> BuildFaceAnchorBindings(
        const SectorAuthoringDerivationResult& result)
{
    std::vector<SectorEditorFaceAnchorBinding> bindings;
    if (!result.success) {
        return bindings;
    }

    bindings.reserve(result.mapping.resolvedFaces.size());
    for (const SectorAuthoringResolvedFaceMapping& mapping : result.mapping.resolvedFaces) {
        if (!IsValidSectorAuthoringId(mapping.faceAnchorId)) {
            continue;
        }
        const SectorAuthoringExtractedFace* face =
                FindExtractedFaceById(result.faces, mapping.extractedFaceId);
        if (face == nullptr) {
            continue;
        }
        SectorEditorFaceAnchorBinding binding;
        binding.faceAnchorId = mapping.faceAnchorId;
        binding.boundarySides = BuildFaceBoundaryIdentity(*face);
        if (!binding.boundarySides.empty()) {
            bindings.push_back(std::move(binding));
        }
    }
    std::sort(
            bindings.begin(),
            bindings.end(),
            [](const SectorEditorFaceAnchorBinding& lhs,
                    const SectorEditorFaceAnchorBinding& rhs) {
                return lhs.faceAnchorId < rhs.faceAnchorId;
            });
    return bindings;
}

const SectorEditorFaceAnchorBinding* FindFaceAnchorBinding(
        const std::vector<SectorEditorFaceAnchorBinding>& bindings,
        int faceAnchorId)
{
    for (const SectorEditorFaceAnchorBinding& binding : bindings) {
        if (binding.faceAnchorId == faceAnchorId) {
            return &binding;
        }
    }
    return nullptr;
}

const SectorAuthoringResolvedFaceMapping* FindResolvedFaceMappingForAnchor(
        const SectorAuthoringDerivationResult& result,
        int faceAnchorId)
{
    const SectorAuthoringResolvedFaceMapping* found = nullptr;
    for (const SectorAuthoringResolvedFaceMapping& mapping : result.mapping.resolvedFaces) {
        if (mapping.faceAnchorId != faceAnchorId) {
            continue;
        }
        if (found != nullptr) {
            return nullptr;
        }
        found = &mapping;
    }
    return found;
}

std::vector<int> CollectFaceAnchorIdsWithChangedBindings(
        const SectorAuthoringDerivationResult& result,
        const SectorAuthoringGraph& graph,
        const std::vector<SectorEditorFaceAnchorBinding>& lastValidBindings)
{
    std::vector<int> anchorIds;
    if (!result.success) {
        return anchorIds;
    }

    for (const SectorEditorFaceAnchorBinding& binding : lastValidBindings) {
        if (FindSectorAuthoringFaceAnchor(graph, binding.faceAnchorId) == nullptr) {
            continue;
        }
        const SectorAuthoringResolvedFaceMapping* mapping =
                FindResolvedFaceMappingForAnchor(result, binding.faceAnchorId);
        if (mapping == nullptr) {
            continue;
        }
        const SectorAuthoringExtractedFace* face =
                FindExtractedFaceById(result.faces, mapping->extractedFaceId);
        if (face == nullptr
                || !FaceBoundaryIdentitiesEqual(
                        binding.boundarySides,
                        BuildFaceBoundaryIdentity(*face))) {
            anchorIds.push_back(binding.faceAnchorId);
        }
    }
    std::sort(anchorIds.begin(), anchorIds.end());
    anchorIds.erase(
            std::unique(anchorIds.begin(), anchorIds.end()),
            anchorIds.end());
    return anchorIds;
}

int FindTopologySectorIdForExtractedFace(
        const SectorAuthoringDerivationResult& result,
        int extractedFaceId)
{
    int topologySectorId = -1;
    for (const SectorAuthoringResolvedFaceMapping& mapping : result.mapping.resolvedFaces) {
        if (mapping.extractedFaceId != extractedFaceId
                || mapping.kind != SectorAuthoringFaceResolutionKind::DerivedSector
                || !IsValidSectorTopologyId(mapping.topologySectorId)) {
            continue;
        }
        if (IsValidSectorTopologyId(topologySectorId)
                && topologySectorId != mapping.topologySectorId) {
            return -1;
        }
        topologySectorId = mapping.topologySectorId;
    }
    return topologySectorId;
}

enum class FaceAnchorAutoFollowOutcome {
    NotNeeded,
    Repaired,
    Failed
};

FaceAnchorAutoFollowOutcome TryAutoFollowFaceAnchors(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringDerivationResult& currentResult,
        const std::vector<SectorEditorFaceAnchorBinding>& lastValidBindings,
        SectorAuthoringGraph& outGraph,
        SectorAuthoringDerivationResult& outResult,
        int& outRelocatedAnchorCount,
        int& outFailureAnchorId,
        std::string& outFailureReason)
{
    outRelocatedAnchorCount = 0;
    outFailureAnchorId = -1;
    outFailureReason.clear();

    std::vector<int> anchorIds = currentResult.success
            ? CollectFaceAnchorIdsWithChangedBindings(
                    currentResult,
                    graph,
                    lastValidBindings)
            : std::vector<int>{};
    if (currentResult.success && anchorIds.empty()) {
        return FaceAnchorAutoFollowOutcome::NotNeeded;
    }

    SectorAuthoringGraph geometryGraph = graph;
    geometryGraph.faceAnchors.clear();
    const SectorAuthoringDerivationResult geometryResult =
            DeriveSectorTopologyMapFromAuthoringGraph(geometryGraph);
    if (!geometryResult.success) {
        return FaceAnchorAutoFollowOutcome::NotNeeded;
    }

    if (!currentResult.success) {
        for (const SectorAuthoringFaceAnchor& anchor : graph.faceAnchors) {
            const SectorEditorFaceAnchorBinding* binding =
                    FindFaceAnchorBinding(lastValidBindings, anchor.id);
            SectorAuthoringGraph singleAnchorGraph = geometryGraph;
            singleAnchorGraph.faceAnchors.push_back(anchor);
            const SectorAuthoringDerivationResult singleAnchorResult =
                    DeriveSectorTopologyMapFromAuthoringGraph(singleAnchorGraph);
            if (singleAnchorResult.success) {
                if (binding == nullptr || binding->boundarySides.empty()) {
                    continue;
                }
                const SectorAuthoringResolvedFaceMapping* mapping =
                        FindResolvedFaceMappingForAnchor(
                                singleAnchorResult,
                                anchor.id);
                const SectorAuthoringExtractedFace* resolvedFace =
                        mapping == nullptr
                        ? nullptr
                        : FindExtractedFaceById(
                                singleAnchorResult.faces,
                                mapping->extractedFaceId);
                if (resolvedFace != nullptr
                        && FaceBoundaryIdentitiesEqual(
                                binding->boundarySides,
                                BuildFaceBoundaryIdentity(*resolvedFace))) {
                    continue;
                }

                bool ambiguousMatch = false;
                const SectorAuthoringExtractedFace* previousFace =
                        FindUniqueFaceWithBoundaryIdentity(
                                geometryResult.faces,
                                binding->boundarySides,
                                ambiguousMatch);
                if (ambiguousMatch) {
                    outFailureAnchorId = anchor.id;
                    outFailureReason =
                            "the previous boundary matches more than one edited face";
                    return FaceAnchorAutoFollowOutcome::Failed;
                }
                if (previousFace == nullptr) {
                    continue;
                }
                anchorIds.push_back(anchor.id);
                continue;
            }

            if (binding == nullptr || binding->boundarySides.empty()) {
                outFailureAnchorId = anchor.id;
                outFailureReason =
                        "previous face binding is unavailable, so the editor cannot safely choose a replacement face";
                return FaceAnchorAutoFollowOutcome::Failed;
            }
            anchorIds.push_back(anchor.id);
        }
        std::sort(anchorIds.begin(), anchorIds.end());
        anchorIds.erase(
                std::unique(anchorIds.begin(), anchorIds.end()),
                anchorIds.end());
        if (anchorIds.empty()) {
            return FaceAnchorAutoFollowOutcome::NotNeeded;
        }
    }

    const SectorTopologyIndexes geometryIndexes =
            BuildSectorTopologyIndexes(geometryResult.topology);

    SectorAuthoringGraph repairedGraph = graph;
    int relocatedAnchorCount = 0;
    for (int anchorId : anchorIds) {
        const SectorEditorFaceAnchorBinding* binding =
                FindFaceAnchorBinding(lastValidBindings, anchorId);
        if (binding == nullptr || binding->boundarySides.empty()) {
            outFailureAnchorId = anchorId;
            outFailureReason =
                    "previous face binding is unavailable, so the editor cannot safely choose a replacement face";
            return FaceAnchorAutoFollowOutcome::Failed;
        }

        bool ambiguousMatch = false;
        const SectorAuthoringExtractedFace* matchedFace =
                FindUniqueFaceWithBoundaryIdentity(
                        geometryResult.faces,
                        binding->boundarySides,
                        ambiguousMatch);
        if (ambiguousMatch) {
            outFailureAnchorId = anchorId;
            outFailureReason =
                    "the previous boundary matches more than one edited face";
            return FaceAnchorAutoFollowOutcome::Failed;
        }
        if (matchedFace == nullptr) {
            if (currentResult.success) {
                continue;
            }
            outFailureAnchorId = anchorId;
            outFailureReason =
                    "the edited geometry no longer has one face with the previous boundary";
            return FaceAnchorAutoFollowOutcome::Failed;
        }

        const int topologySectorId =
                FindTopologySectorIdForExtractedFace(geometryResult, matchedFace->id);
        SectorTopologyCoordPoint interiorPoint{};
        if (!IsValidSectorTopologyId(topologySectorId)
                || !TryFindInteriorPointForTopologySector(
                        geometryResult.topology,
                        geometryIndexes,
                        topologySectorId,
                        &interiorPoint)) {
            outFailureAnchorId = anchorId;
            outFailureReason =
                    "the matched edited face has no safe interior anchor position";
            return FaceAnchorAutoFollowOutcome::Failed;
        }

        SectorAuthoringFaceAnchor* anchor =
                FindSectorAuthoringFaceAnchor(repairedGraph, anchorId);
        if (anchor == nullptr) {
            outFailureAnchorId = anchorId;
            outFailureReason = "the bound face anchor no longer exists";
            return FaceAnchorAutoFollowOutcome::Failed;
        }
        anchor->x = interiorPoint.x;
        anchor->y = interiorPoint.y;
        ++relocatedAnchorCount;
    }

    if (relocatedAnchorCount == 0) {
        return FaceAnchorAutoFollowOutcome::NotNeeded;
    }

    SectorAuthoringDerivationResult repairedResult =
            DeriveSectorTopologyMapFromAuthoringGraph(repairedGraph);
    if (!repairedResult.success) {
        outFailureReason =
                "relocated anchors still do not produce a valid derived topology";
        return FaceAnchorAutoFollowOutcome::Failed;
    }

    outRelocatedAnchorCount = relocatedAnchorCount;
    outGraph = std::move(repairedGraph);
    outResult = std::move(repairedResult);
    return FaceAnchorAutoFollowOutcome::Repaired;
}

std::string FormatAuthoringDerivationDiagnostic(
        const SectorAuthoringDerivationDiagnostic& diagnostic)
{
    const char* objectLabel = "Object";
    switch (diagnostic.kind) {
    case SectorAuthoringDerivationDiagnosticKind::UnresolvedFaceAnchor:
    case SectorAuthoringDerivationDiagnosticKind::AmbiguousFaceAnchor:
        objectLabel = "Face anchor";
        break;
    case SectorAuthoringDerivationDiagnosticKind::DanglingLine:
    case SectorAuthoringDerivationDiagnosticKind::ZeroLengthLine:
    case SectorAuthoringDerivationDiagnosticKind::DuplicateLine:
    case SectorAuthoringDerivationDiagnosticKind::CollinearOverlap:
    case SectorAuthoringDerivationDiagnosticKind::NearMiss:
    case SectorAuthoringDerivationDiagnosticKind::InvalidSideProjection:
        objectLabel = "Authoring line";
        break;
    case SectorAuthoringDerivationDiagnosticKind::NonIntegerVertex:
        objectLabel = "Vertex";
        break;
    case SectorAuthoringDerivationDiagnosticKind::UnresolvedFogVolume:
        objectLabel = "Fog volume";
        break;
    case SectorAuthoringDerivationDiagnosticKind::UnresolvedReflectionProbe:
        objectLabel = "Reflection probe";
        break;
    case SectorAuthoringDerivationDiagnosticKind::AuthoringReference:
    case SectorAuthoringDerivationDiagnosticKind::Planarization:
    case SectorAuthoringDerivationDiagnosticKind::FaceExtraction:
    case SectorAuthoringDerivationDiagnosticKind::TinySliverFace:
    case SectorAuthoringDerivationDiagnosticKind::InvalidTopology:
        break;
    }

    if (diagnostic.objectId > 0) {
        return std::string{objectLabel} + " " + std::to_string(diagnostic.objectId)
                + ": " + diagnostic.message;
    }
    return diagnostic.message;
}

std::string BuildAuthoringDerivationFailureStatus(
        const std::string& failureStatus,
        const SectorAuthoringDerivationResult& result)
{
    std::string status = failureStatus.empty()
            ? "Authoring graph: derivation failed"
            : failureStatus;
    if (result.diagnostics.empty()) {
        return status;
    }

    const SectorAuthoringDerivationDiagnostic* primaryDiagnostic =
            &result.diagnostics.front();
    for (const SectorAuthoringDerivationDiagnostic& diagnostic : result.diagnostics) {
        if (diagnostic.severity == SectorAuthoringValidationSeverity::Error) {
            primaryDiagnostic = &diagnostic;
            break;
        }
    }
    status += ": ";
    status += FormatAuthoringDerivationDiagnostic(*primaryDiagnostic);
    if (result.diagnostics.size() > 1) {
        status += " (+";
        status += std::to_string(result.diagnostics.size() - 1);
        status += " more)";
    }
    return status;
}

bool MappingHasValidFaceAnchor(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringDerivedSectorMapping& mapping)
{
    return IsValidSectorAuthoringId(mapping.faceAnchorId)
            && FindSectorAuthoringFaceAnchor(graph, mapping.faceAnchorId) != nullptr;
}

bool AllDerivedSectorsHaveUniqueFaceAnchorMappings(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringDerivationResult& result)
{
    for (const SectorTopologySector& sector : result.topology.sectors) {
        int matchCount = 0;
        for (const SectorAuthoringDerivedSectorMapping& mapping : result.mapping.sectors) {
            if (mapping.topologySectorId == sector.id
                    && MappingHasValidFaceAnchor(graph, mapping)) {
                ++matchCount;
            }
        }
        if (matchCount != 1) {
            return false;
        }
    }
    return true;
}

bool AllExtractedFacesHaveUniqueFaceAnchorMappings(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringDerivationResult& result)
{
    for (const SectorAuthoringExtractedFace& face : result.faces.faces) {
        int matchCount = 0;
        for (const SectorAuthoringResolvedFaceMapping& mapping
                : result.mapping.resolvedFaces) {
            if (mapping.extractedFaceId == face.id
                    && IsValidSectorAuthoringId(mapping.faceAnchorId)
                    && FindSectorAuthoringFaceAnchor(
                               graph,
                               mapping.faceAnchorId) != nullptr) {
                ++matchCount;
            }
        }
        if (matchCount != 1) {
            return false;
        }
    }
    return true;
}

bool BuildDissolvedFaceAnchorBindings(
        const std::vector<SectorEditorFaceAnchorBinding>& currentBindings,
        int survivingLineId,
        int removedLineId,
        std::vector<SectorEditorFaceAnchorBinding>& outBindings,
        std::string& outError)
{
    outBindings = currentBindings;
    outError.clear();
    for (SectorEditorFaceAnchorBinding& binding : outBindings) {
        const bool containsSurvivingLine = std::any_of(
                binding.boundarySides.begin(),
                binding.boundarySides.end(),
                [survivingLineId](SectorAuthoringSideId side) {
                    return side.lineId == survivingLineId;
                });
        const bool containsRemovedLine = std::any_of(
                binding.boundarySides.begin(),
                binding.boundarySides.end(),
                [removedLineId](SectorAuthoringSideId side) {
                    return side.lineId == removedLineId;
                });
        if (containsSurvivingLine != containsRemovedLine) {
            outError =
                    "Cannot dissolve vertex: its two lines do not bound the same authoring faces";
            return false;
        }
        if (!containsRemovedLine) {
            continue;
        }
        binding.boundarySides.erase(
                std::remove_if(
                        binding.boundarySides.begin(),
                        binding.boundarySides.end(),
                        [removedLineId](SectorAuthoringSideId side) {
                            return side.lineId == removedLineId;
                        }),
                binding.boundarySides.end());
    }
    return true;
}

void CopyTopologySectorDefaultsToFaceAnchor(
        const SectorTopologySector& sector,
        SectorAuthoringFaceAnchor& anchor)
{
    anchor.floorZ = sector.floorZ;
    anchor.ceilingZ = sector.ceilingZ;
    anchor.floorMaterialId = sector.floorMaterialId;
    anchor.ceilingMaterialId = sector.ceilingMaterialId;
    anchor.ceilingSky = sector.ceilingSky;
    anchor.floorUv = sector.floorUv;
    anchor.ceilingUv = sector.ceilingUv;
    anchor.floorDecal = sector.floorDecal;
    anchor.ceilingDecal = sector.ceilingDecal;
    anchor.ambientColor = sector.ambientColor;
    anchor.ambientIntensity = sector.ambientIntensity;
    anchor.defaultWall = sector.defaultWall;
    anchor.defaultLower = sector.defaultLower;
    anchor.defaultUpper = sector.defaultUpper;
}

std::vector<int> FindTopologySectorsContainingAnchorPoint(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        const SectorAuthoringFaceAnchor& anchor)
{
    const Vector2 mapPoint{
            SectorCoordToVisibleAuthoring(anchor.x),
            SectorCoordToVisibleAuthoring(anchor.y)};

    std::vector<int> sectorIds;
    for (const SectorTopologySector& sector : map.sectors) {
        if (PointStrictlyInTopologySector(map, indexes, mapPoint, sector.id)) {
            sectorIds.push_back(sector.id);
        }
    }
    return sectorIds;
}

bool BuildExistingFaceAnchorClaims(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringDerivationResult& result,
        const SectorTopologyIndexes& indexes,
        std::map<int, int>& claimedAnchorIdBySectorId)
{
    claimedAnchorIdBySectorId.clear();

    std::set<int> claimedAnchorIds;
    for (const SectorAuthoringDerivedSectorMapping& mapping : result.mapping.sectors) {
        if (!MappingHasValidFaceAnchor(graph, mapping)) {
            continue;
        }
        const auto inserted = claimedAnchorIdBySectorId.emplace(
                mapping.topologySectorId,
                mapping.faceAnchorId);
        if (!inserted.second && inserted.first->second != mapping.faceAnchorId) {
            return false;
        }
        claimedAnchorIds.insert(mapping.faceAnchorId);
    }

    for (const SectorAuthoringFaceAnchor& anchor : graph.faceAnchors) {
        if (!IsValidSectorAuthoringId(anchor.id)
                || claimedAnchorIds.find(anchor.id) != claimedAnchorIds.end()) {
            continue;
        }

        const std::vector<int> containingSectorIds =
                FindTopologySectorsContainingAnchorPoint(result.topology, indexes, anchor);
        if (containingSectorIds.empty()) {
            continue;
        }
        if (containingSectorIds.size() > 1) {
            return false;
        }

        const int sectorId = containingSectorIds.front();
        const auto inserted = claimedAnchorIdBySectorId.emplace(sectorId, anchor.id);
        if (!inserted.second && inserted.first->second != anchor.id) {
            return false;
        }
        claimedAnchorIds.insert(anchor.id);
    }

    return true;
}

bool ReconcileMissingDerivedFaceAnchors(
        SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& result,
        int* outAddedCount,
        bool* outFailed)
{
    if (outAddedCount != nullptr) {
        *outAddedCount = 0;
    }
    if (outFailed != nullptr) {
        *outFailed = false;
    }
    if (!result.success) {
        return false;
    }

    const SectorTopologyIndexes indexes = BuildSectorTopologyIndexes(result.topology);
    std::map<int, int> claimedAnchorIdBySectorId;
    if (!BuildExistingFaceAnchorClaims(authoringGraph, result, indexes, claimedAnchorIdBySectorId)) {
        if (outFailed != nullptr) {
            *outFailed = true;
        }
        return false;
    }

    int addedCount = 0;
    for (const SectorTopologySector& sector : result.topology.sectors) {
        bool hasMappingForSector = false;
        for (const SectorAuthoringDerivedSectorMapping& mapping : result.mapping.sectors) {
            if (mapping.topologySectorId != sector.id) {
                continue;
            }
            hasMappingForSector = true;
        }
        if (claimedAnchorIdBySectorId.find(sector.id) != claimedAnchorIdBySectorId.end()) {
            continue;
        }
        if (!hasMappingForSector) {
            continue;
        }

        SectorTopologyCoordPoint anchorPoint{};
        if (!TryFindInteriorPointForTopologySector(result.topology, indexes, sector.id, &anchorPoint)) {
            continue;
        }

        SectorAuthoringFaceAnchor anchor;
        anchor.id = AllocateSectorAuthoringFaceAnchorId(authoringGraph);
        if (!IsValidSectorAuthoringId(anchor.id)) {
            continue;
        }
        anchor.name = AllocateGeneratedFaceAnchorName(authoringGraph);
        anchor.x = anchorPoint.x;
        anchor.y = anchorPoint.y;
        CopyTopologySectorDefaultsToFaceAnchor(sector, anchor);
        authoringGraph.faceAnchors.push_back(std::move(anchor));
        ++addedCount;
    }

    if (outAddedCount != nullptr) {
        *outAddedCount = addedCount;
    }
    return addedCount > 0;
}

} // namespace

void CopySectorEditorMapLevelFields(
        SectorTopologyMap& target,
        const SectorTopologyMap& source)
{
    CopyEditorMapLevelFields(target, source);
}

std::string BuildSectorEditorAuthoringDerivationDisplayStatus(
        SectorEditorConstDerivationDocumentAccess derivation,
        const char* fallbackStatus)
{
    if (!derivation.authoringDerivationStatus.empty()) {
        return derivation.authoringDerivationStatus;
    }

    const std::string fallback =
            fallbackStatus == nullptr ? std::string{} : std::string{fallbackStatus};
    if (!derivation.authoringDerivation.success
            || !derivation.authoringDerivation.diagnostics.empty()) {
        return BuildAuthoringDerivationFailureStatus(
                fallback,
                derivation.authoringDerivation);
    }
    return fallback.empty()
            ? "Authoring graph: derived topology status unavailable"
            : fallback;
}

SectorAuthoringSelectionTarget MakeSectorAuthoringLineSelectionTarget(int lineId)
{
    SectorAuthoringSelectionTarget target;
    if (IsValidSectorAuthoringId(lineId)) {
        target.kind = SectorAuthoringSelectionKind::Line;
        target.lineId = lineId;
    }
    return target;
}

SectorAuthoringSelectionTarget MakeSectorAuthoringVertexSelectionTarget(int vertexId)
{
    SectorAuthoringSelectionTarget target;
    if (IsValidSectorAuthoringId(vertexId)) {
        target.kind = SectorAuthoringSelectionKind::Vertex;
        target.vertexId = vertexId;
    }
    return target;
}

SectorAuthoringSelectionTarget MakeSectorAuthoringFaceAnchorSelectionTarget(int faceAnchorId)
{
    SectorAuthoringSelectionTarget target;
    if (IsValidSectorAuthoringId(faceAnchorId)) {
        target.kind = SectorAuthoringSelectionKind::FaceAnchor;
        target.faceAnchorId = faceAnchorId;
    }
    return target;
}

SectorAuthoringSelectionTarget MakeSectorAuthoringFogVolumeSelectionTarget(int fogVolumeId)
{
    SectorAuthoringSelectionTarget target;
    if (IsValidSectorAuthoringId(fogVolumeId)) {
        target.kind = SectorAuthoringSelectionKind::FogVolume;
        target.fogVolumeId = fogVolumeId;
    }
    return target;
}

SectorAuthoringSelectionTarget MakeSectorAuthoringReflectionProbeSelectionTarget(int probeId)
{
    SectorAuthoringSelectionTarget target;
    if (IsValidSectorAuthoringId(probeId)) {
        target.kind = SectorAuthoringSelectionKind::ReflectionProbe;
        target.reflectionProbeId = probeId;
    }
    return target;
}

SectorAuthoringSelectionTarget MakeSectorAuthoringLevelMarkerSelectionTarget(int markerId)
{
    SectorAuthoringSelectionTarget target;
    if (IsValidSectorAuthoringId(markerId)) {
        target.kind = SectorAuthoringSelectionKind::LevelMarker;
        target.levelMarkerId = markerId;
    }
    return target;
}

SectorAuthoringSelectionTarget MakeSectorAuthoringSoundEmitterSelectionTarget(int emitterId)
{
    SectorAuthoringSelectionTarget target;
    if (IsValidSectorAuthoringId(emitterId)) {
        target.kind = SectorAuthoringSelectionKind::SoundEmitter;
        target.soundEmitterId = emitterId;
    }
    return target;
}

SectorAuthoringSelectionTarget MakeSectorAuthoringTriggerSelectionTarget(int triggerId)
{
    SectorAuthoringSelectionTarget target;
    if (IsValidSectorAuthoringId(triggerId)) {
        target.kind = SectorAuthoringSelectionKind::Trigger;
        target.triggerId = triggerId;
    }
    return target;
}

SectorAuthoringSelectionTarget MakeSectorAuthoringStructuralPrimitiveSelectionTarget(
        int primitiveId)
{
    SectorAuthoringSelectionTarget target;
    if (IsValidSectorAuthoringId(primitiveId)) {
        target.kind = SectorAuthoringSelectionKind::StructuralPrimitive;
        target.structuralPrimitiveId = primitiveId;
    }
    return target;
}

bool SectorAuthoringSelectionTargetsEqual(
        SectorAuthoringSelectionTarget lhs,
        SectorAuthoringSelectionTarget rhs)
{
    return lhs.kind == rhs.kind
            && lhs.lineId == rhs.lineId
            && lhs.vertexId == rhs.vertexId
            && lhs.faceAnchorId == rhs.faceAnchorId
            && lhs.fogVolumeId == rhs.fogVolumeId
            && lhs.reflectionProbeId == rhs.reflectionProbeId
            && lhs.levelMarkerId == rhs.levelMarkerId
            && lhs.soundEmitterId == rhs.soundEmitterId
            && lhs.triggerId == rhs.triggerId
            && lhs.structuralPrimitiveId == rhs.structuralPrimitiveId;
}

bool IsSectorAuthoringSelectionTargetValid(
        const SectorAuthoringGraph& graph,
        SectorAuthoringSelectionTarget target)
{
    switch (target.kind) {
    case SectorAuthoringSelectionKind::None:
        return target.lineId == -1 && target.vertexId == -1 && target.faceAnchorId == -1
                && target.fogVolumeId == -1 && target.reflectionProbeId == -1
                && target.levelMarkerId == -1 && target.soundEmitterId == -1
                && target.triggerId == -1 && target.structuralPrimitiveId == -1;
    case SectorAuthoringSelectionKind::Line:
        return target.vertexId == -1
                && target.faceAnchorId == -1
                && target.fogVolumeId == -1
                && target.levelMarkerId == -1
                && target.soundEmitterId == -1
                && target.triggerId == -1
                && FindSectorAuthoringLine(graph, target.lineId) != nullptr;
    case SectorAuthoringSelectionKind::Vertex:
        return target.lineId == -1
                && target.faceAnchorId == -1
                && target.fogVolumeId == -1
                && target.levelMarkerId == -1
                && target.soundEmitterId == -1
                && target.triggerId == -1
                && FindSectorAuthoringVertex(graph, target.vertexId) != nullptr;
    case SectorAuthoringSelectionKind::FaceAnchor:
        return target.lineId == -1
                && target.vertexId == -1
                && target.fogVolumeId == -1
                && target.levelMarkerId == -1
                && target.soundEmitterId == -1
                && target.triggerId == -1
                && FindSectorAuthoringFaceAnchor(graph, target.faceAnchorId) != nullptr;
    case SectorAuthoringSelectionKind::FogVolume:
        return target.lineId == -1
                && target.vertexId == -1
                && target.faceAnchorId == -1
                && target.levelMarkerId == -1
                && target.soundEmitterId == -1
                && target.triggerId == -1
                && FindSectorAuthoringFogVolume(graph, target.fogVolumeId) != nullptr;
    case SectorAuthoringSelectionKind::ReflectionProbe:
        return target.lineId == -1 && target.vertexId == -1
                && target.faceAnchorId == -1 && target.fogVolumeId == -1
                && target.levelMarkerId == -1 && target.soundEmitterId == -1
                && target.triggerId == -1
                && FindSectorAuthoringReflectionProbe(
                        graph, target.reflectionProbeId) != nullptr;
    case SectorAuthoringSelectionKind::LevelMarker:
        return target.lineId == -1
                && target.vertexId == -1
                && target.faceAnchorId == -1
                && target.fogVolumeId == -1
                && target.soundEmitterId == -1
                && target.triggerId == -1
                && FindSectorAuthoringLevelMarker(graph, target.levelMarkerId) != nullptr;
    case SectorAuthoringSelectionKind::SoundEmitter:
        return target.lineId == -1 && target.vertexId == -1
                && target.faceAnchorId == -1 && target.fogVolumeId == -1
                && target.reflectionProbeId == -1 && target.levelMarkerId == -1
                && target.triggerId == -1
                && FindSectorAuthoringSoundEmitter(graph, target.soundEmitterId) != nullptr;
    case SectorAuthoringSelectionKind::Trigger:
        return target.lineId == -1 && target.vertexId == -1 && target.faceAnchorId == -1
                && target.fogVolumeId == -1 && target.levelMarkerId == -1
                && target.soundEmitterId == -1
                && FindSectorAuthoringTrigger(graph, target.triggerId) != nullptr;
    case SectorAuthoringSelectionKind::StructuralPrimitive:
        return target.lineId == -1 && target.vertexId == -1
                && target.faceAnchorId == -1 && target.fogVolumeId == -1
                && target.reflectionProbeId == -1 && target.levelMarkerId == -1
                && target.soundEmitterId == -1 && target.triggerId == -1
                && FindSectorAuthoringStructuralPrimitive(
                        graph, target.structuralPrimitiveId) != nullptr;
    }
    return false;
}

void ClearSectorEditorAuthoringSelection(SelectionState& selectionState)
{
    selectionState.selectedAuthoring = EmptyAuthoringSelectionTarget();
    selectionState.selectedAuthoringFaceAnchorIds.clear();
}

void ReserveSectorEditorAuthoringFaceSelection(
        SelectionState& selectionState,
        std::size_t capacity)
{
    constexpr std::size_t MinimumReservedFaceSelectionCapacity = 64;
    selectionState.selectedAuthoringFaceAnchorIds.reserve(
            std::max(capacity, MinimumReservedFaceSelectionCapacity));
}

bool IsSectorEditorAuthoringFaceSelected(
        const SelectionState& selectionState,
        int faceAnchorId)
{
    return std::find(
            selectionState.selectedAuthoringFaceAnchorIds.begin(),
            selectionState.selectedAuthoringFaceAnchorIds.end(),
            faceAnchorId) != selectionState.selectedAuthoringFaceAnchorIds.end();
}

bool ToggleSectorEditorAuthoringFaceSelection(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int faceAnchorId)
{
    if (FindSectorAuthoringFaceAnchor(graph, faceAnchorId) == nullptr) {
        return false;
    }

    auto& selectedIds = selectionState.selectedAuthoringFaceAnchorIds;
    const auto found = std::find(selectedIds.begin(), selectedIds.end(), faceAnchorId);
    if (found != selectedIds.end()) {
        selectedIds.erase(found);
        if (selectedIds.empty()) {
            selectionState.selectedAuthoring = EmptyAuthoringSelectionTarget();
        } else {
            selectionState.selectedAuthoring =
                    MakeSectorAuthoringFaceAnchorSelectionTarget(selectedIds.back());
        }
        return true;
    }

    if (selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::FaceAnchor) {
        selectedIds.clear();
    }
    selectedIds.push_back(faceAnchorId);
    selectionState.selectedAuthoring =
            MakeSectorAuthoringFaceAnchorSelectionTarget(faceAnchorId);
    return true;
}

bool SelectSectorEditorAuthoringLine(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int lineId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringLineSelectionTarget(lineId);
    if (target.kind != SectorAuthoringSelectionKind::Line
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }

    selectionState.selectedAuthoring = target;
    selectionState.selectedAuthoringFaceAnchorIds.clear();
    return true;
}

bool SelectSectorEditorAuthoringVertex(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int vertexId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringVertexSelectionTarget(vertexId);
    if (target.kind != SectorAuthoringSelectionKind::Vertex
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }

    selectionState.selectedAuthoring = target;
    selectionState.selectedAuthoringFaceAnchorIds.clear();
    return true;
}

bool SelectSectorEditorAuthoringFaceAnchor(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int faceAnchorId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringFaceAnchorSelectionTarget(faceAnchorId);
    if (target.kind != SectorAuthoringSelectionKind::FaceAnchor
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }

    selectionState.selectedAuthoring = target;
    selectionState.selectedAuthoringFaceAnchorIds.clear();
    selectionState.selectedAuthoringFaceAnchorIds.push_back(faceAnchorId);
    return true;
}

bool SelectSectorEditorAuthoringFogVolume(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int fogVolumeId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringFogVolumeSelectionTarget(fogVolumeId);
    if (target.kind != SectorAuthoringSelectionKind::FogVolume
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }
    selectionState.selectedAuthoring = target;
    selectionState.selectedAuthoringFaceAnchorIds.clear();
    return true;
}

bool SelectSectorEditorAuthoringReflectionProbe(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int probeId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringReflectionProbeSelectionTarget(probeId);
    if (target.kind != SectorAuthoringSelectionKind::ReflectionProbe
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) return false;
    selectionState.selectedAuthoring = target;
    selectionState.selectedAuthoringFaceAnchorIds.clear();
    return true;
}

bool SelectSectorEditorAuthoringLevelMarker(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int markerId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringLevelMarkerSelectionTarget(markerId);
    if (target.kind != SectorAuthoringSelectionKind::LevelMarker
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }
    selectionState.selectedAuthoring = target;
    selectionState.selectedAuthoringFaceAnchorIds.clear();
    return true;
}

bool SelectSectorEditorAuthoringSoundEmitter(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int emitterId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringSoundEmitterSelectionTarget(emitterId);
    if (target.kind != SectorAuthoringSelectionKind::SoundEmitter
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }
    selectionState.selectedAuthoring = target;
    selectionState.selectedAuthoringFaceAnchorIds.clear();
    return true;
}

bool SelectSectorEditorAuthoringTrigger(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int triggerId)
{
    const SectorAuthoringSelectionTarget target = MakeSectorAuthoringTriggerSelectionTarget(triggerId);
    if (target.kind != SectorAuthoringSelectionKind::Trigger
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) return false;
    selectionState.selectedAuthoring = target;
    selectionState.selectedAuthoringFaceAnchorIds.clear();
    return true;
}

bool SelectSectorEditorAuthoringStructuralPrimitive(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int primitiveId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringStructuralPrimitiveSelectionTarget(primitiveId);
    if (target.kind != SectorAuthoringSelectionKind::StructuralPrimitive
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }
    selectionState.selectedAuthoring = target;
    selectionState.selectedAuthoringFaceAnchorIds.clear();
    return true;
}

void ClearSectorEditorAuthoringHover(SelectionState& selectionState)
{
    selectionState.hoveredAuthoring = EmptyAuthoringSelectionTarget();
}

bool SetHoveredSectorEditorAuthoringLine(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int lineId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringLineSelectionTarget(lineId);
    if (target.kind != SectorAuthoringSelectionKind::Line
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }

    selectionState.hoveredAuthoring = target;
    return true;
}

bool SetHoveredSectorEditorAuthoringVertex(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int vertexId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringVertexSelectionTarget(vertexId);
    if (target.kind != SectorAuthoringSelectionKind::Vertex
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }

    selectionState.hoveredAuthoring = target;
    return true;
}

bool SetHoveredSectorEditorAuthoringFogVolume(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int fogVolumeId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringFogVolumeSelectionTarget(fogVolumeId);
    if (target.kind != SectorAuthoringSelectionKind::FogVolume
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }
    selectionState.hoveredAuthoring = target;
    return true;
}

bool SetHoveredSectorEditorAuthoringReflectionProbe(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int probeId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringReflectionProbeSelectionTarget(probeId);
    if (target.kind != SectorAuthoringSelectionKind::ReflectionProbe
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) return false;
    selectionState.hoveredAuthoring = target;
    return true;
}

bool SetHoveredSectorEditorAuthoringLevelMarker(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int markerId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringLevelMarkerSelectionTarget(markerId);
    if (target.kind != SectorAuthoringSelectionKind::LevelMarker
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }
    selectionState.hoveredAuthoring = target;
    return true;
}

bool SetHoveredSectorEditorAuthoringSoundEmitter(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int emitterId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringSoundEmitterSelectionTarget(emitterId);
    if (target.kind != SectorAuthoringSelectionKind::SoundEmitter
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }
    selectionState.hoveredAuthoring = target;
    return true;
}

bool SetHoveredSectorEditorAuthoringTrigger(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int triggerId)
{
    const SectorAuthoringSelectionTarget target = MakeSectorAuthoringTriggerSelectionTarget(triggerId);
    if (target.kind != SectorAuthoringSelectionKind::Trigger
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) return false;
    selectionState.hoveredAuthoring = target;
    return true;
}

bool SetHoveredSectorEditorAuthoringStructuralPrimitive(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int primitiveId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringStructuralPrimitiveSelectionTarget(primitiveId);
    if (target.kind != SectorAuthoringSelectionKind::StructuralPrimitive
            || !IsSectorAuthoringSelectionTargetValid(graph, target)) {
        return false;
    }
    selectionState.hoveredAuthoring = target;
    return true;
}

void PruneSectorEditorAuthoringSelectionToGraph(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState)
{
    selectionState.selectedAuthoringFaceAnchorIds.erase(
            std::remove_if(
                    selectionState.selectedAuthoringFaceAnchorIds.begin(),
                    selectionState.selectedAuthoringFaceAnchorIds.end(),
                    [&graph](int faceAnchorId) {
                        return FindSectorAuthoringFaceAnchor(graph, faceAnchorId) == nullptr;
                    }),
            selectionState.selectedAuthoringFaceAnchorIds.end());
    if (!IsSectorAuthoringSelectionTargetValid(graph, selectionState.selectedAuthoring)) {
        if (!selectionState.selectedAuthoringFaceAnchorIds.empty()) {
            selectionState.selectedAuthoring =
                    MakeSectorAuthoringFaceAnchorSelectionTarget(
                            selectionState.selectedAuthoringFaceAnchorIds.back());
        } else {
            ClearSectorEditorAuthoringSelection(selectionState);
        }
    }
    if (!IsSectorAuthoringSelectionTargetValid(graph, selectionState.hoveredAuthoring)) {
        ClearSectorEditorAuthoringHover(selectionState);
    }
}

bool FindSectorAuthoringVertexAtPoint(
        const SectorAuthoringGraph& graph,
        SectorTopologyCoordPoint point,
        int* outVertexId)
{
    for (const SectorAuthoringVertex& vertex : graph.vertices) {
        if (vertex.x == point.x && vertex.y == point.y) {
            if (outVertexId != nullptr) {
                *outVertexId = vertex.id;
            }
            return true;
        }
    }
    return false;
}

bool SectorAuthoringPointOnLineInterior(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringLine& line,
        SectorTopologyCoordPoint point)
{
    const SectorAuthoringVertex* start =
            FindSectorAuthoringVertex(graph, line.startVertexId);
    const SectorAuthoringVertex* end =
            FindSectorAuthoringVertex(graph, line.endVertexId);
    if (start == nullptr || end == nullptr) {
        return false;
    }
    if ((point.x == start->x && point.y == start->y)
            || (point.x == end->x && point.y == end->y)) {
        return false;
    }

    const int64_t ax = static_cast<int64_t>(end->x) - start->x;
    const int64_t ay = static_cast<int64_t>(end->y) - start->y;
    const int64_t px = static_cast<int64_t>(point.x) - start->x;
    const int64_t py = static_cast<int64_t>(point.y) - start->y;
    if (ax * py - ay * px != 0) {
        return false;
    }

    const SectorCoord minX = std::min(start->x, end->x);
    const SectorCoord maxX = std::max(start->x, end->x);
    const SectorCoord minY = std::min(start->y, end->y);
    const SectorCoord maxY = std::max(start->y, end->y);
    return point.x >= minX && point.x <= maxX
            && point.y >= minY && point.y <= maxY;
}

bool MaterializeSectorAuthoringLineEndpoint(
        SectorAuthoringGraph& graph,
        SectorTopologyCoordPoint point,
        int& outVertexId)
{
    outVertexId = -1;
    const bool hadExistingVertex =
            FindSectorAuthoringVertexAtPoint(graph, point, &outVertexId);

    bool splitAny = false;
    bool keepScanning = true;
    while (keepScanning) {
        keepScanning = false;
        int lineToSplit = -1;
        for (const SectorAuthoringLine& line : graph.lines) {
            if (SectorAuthoringPointOnLineInterior(graph, line, point)) {
                lineToSplit = line.id;
                break;
            }
        }

        if (!IsValidSectorAuthoringId(lineToSplit)) {
            continue;
        }

        SectorAuthoringInsertVertexResult splitResult;
        if (!InsertSectorAuthoringVertexOnLine(graph, lineToSplit, point, &splitResult)) {
            return false;
        }
        outVertexId = splitResult.vertexId;
        splitAny = true;
        keepScanning = true;
    }

    if (splitAny) {
        return FindSectorAuthoringVertexAtPoint(graph, point, &outVertexId);
    }

    if (hadExistingVertex) {
        return true;
    }

    return AddSectorAuthoringVertex(graph, point.x, point.y, &outVertexId);
}

bool SameSectorTopologyPoint(SectorTopologyCoordPoint lhs, SectorTopologyCoordPoint rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool AddUniqueSectorTopologyPoint(
        std::vector<SectorTopologyCoordPoint>& points,
        SectorTopologyCoordPoint point)
{
    for (const SectorTopologyCoordPoint existing : points) {
        if (SameSectorTopologyPoint(existing, point)) {
            return false;
        }
    }
    points.push_back(point);
    return true;
}

bool MakeIntegerIntersectionPoint(
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b,
        SectorTopologyCoordPoint c,
        SectorTopologyCoordPoint d,
        SectorTopologyCoordPoint& outPoint)
{
    const int64_t ax = static_cast<int64_t>(b.x) - a.x;
    const int64_t ay = static_cast<int64_t>(b.y) - a.y;
    const int64_t cx = static_cast<int64_t>(d.x) - c.x;
    const int64_t cy = static_cast<int64_t>(d.y) - c.y;
    const int64_t qpx = static_cast<int64_t>(c.x) - a.x;
    const int64_t qpy = static_cast<int64_t>(c.y) - a.y;
    const __int128 denominator =
            static_cast<__int128>(ax) * cy - static_cast<__int128>(ay) * cx;
    if (denominator == 0) {
        return false;
    }

    const __int128 numerator =
            static_cast<__int128>(qpx) * cy - static_cast<__int128>(qpy) * cx;
    const __int128 xNumerator =
            static_cast<__int128>(a.x) * denominator
            + static_cast<__int128>(ax) * numerator;
    const __int128 yNumerator =
            static_cast<__int128>(a.y) * denominator
            + static_cast<__int128>(ay) * numerator;
    if (xNumerator % denominator != 0 || yNumerator % denominator != 0) {
        return false;
    }

    outPoint.x = static_cast<SectorCoord>(xNumerator / denominator);
    outPoint.y = static_cast<SectorCoord>(yNumerator / denominator);
    return true;
}

void AddTouchIntersectionPoints(
        std::vector<SectorTopologyCoordPoint>& points,
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b,
        SectorTopologyCoordPoint c,
        SectorTopologyCoordPoint d)
{
    const SectorTopologyCoordPoint candidates[4] = {a, b, c, d};
    for (const SectorTopologyCoordPoint point : candidates) {
        if (SectorTopologyPointOnSegment(point, a, b)
                && SectorTopologyPointOnSegment(point, c, d)) {
            AddUniqueSectorTopologyPoint(points, point);
        }
    }
}

int64_t SegmentSortKey(
        SectorTopologyCoordPoint start,
        SectorTopologyCoordPoint end,
        SectorTopologyCoordPoint point)
{
    const int64_t dx = static_cast<int64_t>(end.x) - start.x;
    const int64_t dy = static_cast<int64_t>(end.y) - start.y;
    const int64_t px = static_cast<int64_t>(point.x) - start.x;
    const int64_t py = static_cast<int64_t>(point.y) - start.y;
    return px * dx + py * dy;
}

bool InsertSectorEditorAuthoringLineSegmentIntoGraphWithSplits(
        SectorAuthoringGraph& graph,
        SectorTopologyCoordPoint start,
        SectorTopologyCoordPoint end,
        SectorEditorAuthoringSegmentInsertResult* outResult = nullptr)
{
    SectorEditorAuthoringSegmentInsertResult result;
    if (start.x == end.x && start.y == end.y) {
        result.errorMessage = "Cannot insert line: zero-length segment.";
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    SectorAuthoringGraph candidate = graph;
    std::vector<SectorTopologyCoordPoint> splitPoints;
    AddUniqueSectorTopologyPoint(splitPoints, start);
    AddUniqueSectorTopologyPoint(splitPoints, end);

    const std::vector<SectorAuthoringLine> sourceLines = candidate.lines;
    for (const SectorAuthoringLine& line : sourceLines) {
        const SectorAuthoringVertex* lineStart =
                FindSectorAuthoringVertex(candidate, line.startVertexId);
        const SectorAuthoringVertex* lineEnd =
                FindSectorAuthoringVertex(candidate, line.endVertexId);
        if (lineStart == nullptr || lineEnd == nullptr) {
            result.errorMessage = "Cannot insert line: existing authoring line is invalid.";
            if (outResult != nullptr) {
                *outResult = result;
            }
            return false;
        }

        const SectorTopologyCoordPoint existingStart{lineStart->x, lineStart->y};
        const SectorTopologyCoordPoint existingEnd{lineEnd->x, lineEnd->y};
        const SectorTopologySegmentIntersectionKind intersection =
                SectorTopologySegmentIntersection(start, end, existingStart, existingEnd);
        if (intersection == SectorTopologySegmentIntersectionKind::None) {
            continue;
        }
        if (intersection == SectorTopologySegmentIntersectionKind::CollinearOverlap) {
            result.errorMessage = "Cannot insert line: edge overlaps an existing line.";
            if (outResult != nullptr) {
                *outResult = result;
            }
            return false;
        }

        if (intersection == SectorTopologySegmentIntersectionKind::Proper) {
            SectorTopologyCoordPoint point{};
            if (!MakeIntegerIntersectionPoint(start, end, existingStart, existingEnd, point)) {
                result.errorMessage =
                        "Cannot insert line: intersection is not representable on the authoring grid.";
                if (outResult != nullptr) {
                    *outResult = result;
                }
                return false;
            }
            AddUniqueSectorTopologyPoint(splitPoints, point);
            continue;
        }

        AddTouchIntersectionPoints(splitPoints, start, end, existingStart, existingEnd);
    }

    std::sort(
            splitPoints.begin(),
            splitPoints.end(),
            [start, end](SectorTopologyCoordPoint lhs, SectorTopologyCoordPoint rhs) {
                const int64_t lhsKey = SegmentSortKey(start, end, lhs);
                const int64_t rhsKey = SegmentSortKey(start, end, rhs);
                if (lhsKey != rhsKey) {
                    return lhsKey < rhsKey;
                }
                if (lhs.x != rhs.x) {
                    return lhs.x < rhs.x;
                }
                return lhs.y < rhs.y;
            });

    for (const SectorTopologyCoordPoint point : splitPoints) {
        int vertexId = -1;
        if (!MaterializeSectorAuthoringLineEndpoint(candidate, point, vertexId)) {
            result.errorMessage = "Cannot insert line: failed to split existing authoring line.";
            if (outResult != nullptr) {
                *outResult = result;
            }
            return false;
        }
    }

    for (std::size_t index = 1; index < splitPoints.size(); ++index) {
        const SectorTopologyCoordPoint segmentStart = splitPoints[index - 1];
        const SectorTopologyCoordPoint segmentEnd = splitPoints[index];
        if (SameSectorTopologyPoint(segmentStart, segmentEnd)) {
            continue;
        }

        int startVertexId = -1;
        int endVertexId = -1;
        if (!FindSectorAuthoringVertexAtPoint(candidate, segmentStart, &startVertexId)
                || !FindSectorAuthoringVertexAtPoint(candidate, segmentEnd, &endVertexId)
                || startVertexId == endVertexId) {
            result.errorMessage = "Cannot insert line: zero-length segment.";
            if (outResult != nullptr) {
                *outResult = result;
            }
            return false;
        }

        int lineId = -1;
        if (!AddSectorAuthoringLine(candidate, startVertexId, endVertexId, &lineId)) {
            result.errorMessage = "Cannot insert line: duplicate edge.";
            if (outResult != nullptr) {
                *outResult = result;
            }
            return false;
        }

        SectorEditorAuthoringLineSegmentResult segment;
        segment.lineId = lineId;
        segment.startVertexId = startVertexId;
        segment.endVertexId = endVertexId;
        segment.startPoint = segmentStart;
        segment.endPoint = segmentEnd;
        result.segments.push_back(segment);
    }

    if (result.segments.empty()) {
        result.errorMessage = "Cannot insert line: zero-length segment.";
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    const std::vector<SectorAuthoringValidationIssue> issues =
            ValidateSectorAuthoringGraphReferences(candidate);
    if (HasSectorAuthoringValidationErrors(issues)) {
        result.errorMessage = "Cannot insert line: authoring graph validation failed.";
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    graph = std::move(candidate);
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

bool FindSectorEditorAuthoringLineNearMapPoint(
        const SectorAuthoringGraph& graph,
        Vector2 mapPoint,
        float maxDistance,
        int* outLineId)
{
    if (maxDistance < 0.0f) {
        return false;
    }

    const float maxDistance2 = maxDistance * maxDistance;
    float bestDistance2 = maxDistance2;
    int bestLineId = -1;

    for (const SectorAuthoringLine& line : graph.lines) {
        const SectorAuthoringVertex* start =
                FindSectorAuthoringVertex(graph, line.startVertexId);
        const SectorAuthoringVertex* end =
                FindSectorAuthoringVertex(graph, line.endVertexId);
        if (start == nullptr || end == nullptr) {
            continue;
        }

        const Vector2 a{
                SectorCoordToVisibleAuthoring(start->x),
                SectorCoordToVisibleAuthoring(start->y)};
        const Vector2 b{
                SectorCoordToVisibleAuthoring(end->x),
                SectorCoordToVisibleAuthoring(end->y)};
        const Vector2 ab{b.x - a.x, b.y - a.y};
        const Vector2 ap{mapPoint.x - a.x, mapPoint.y - a.y};
        const float length2 = ab.x * ab.x + ab.y * ab.y;
        if (length2 <= 0.0f) {
            continue;
        }

        const float t = std::clamp((ap.x * ab.x + ap.y * ab.y) / length2, 0.0f, 1.0f);
        const Vector2 closest{a.x + ab.x * t, a.y + ab.y * t};
        const float dx = mapPoint.x - closest.x;
        const float dy = mapPoint.y - closest.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > maxDistance2) {
            continue;
        }
        if (bestLineId >= 0
                && (distance2 > bestDistance2 + 0.001f
                        || (std::fabs(distance2 - bestDistance2) <= 0.001f
                                && line.id >= bestLineId))) {
            continue;
        }

        bestDistance2 = distance2;
        bestLineId = line.id;
    }

    if (bestLineId < 0) {
        return false;
    }
    if (outLineId != nullptr) {
        *outLineId = bestLineId;
    }
    return true;
}

bool FindSectorEditorAuthoringVertexNearMapPoint(
        const SectorAuthoringGraph& graph,
        Vector2 mapPoint,
        float maxDistance,
        int* outVertexId,
        SectorTopologyCoordPoint* outPoint)
{
    if (maxDistance < 0.0f) {
        return false;
    }

    const float maxDistance2 = maxDistance * maxDistance;
    float bestDistance2 = maxDistance2;
    int bestVertexId = -1;
    SectorTopologyCoordPoint bestPoint{};

    for (const SectorAuthoringVertex& vertex : graph.vertices) {
        const Vector2 vertexMap{
                SectorCoordToVisibleAuthoring(vertex.x),
                SectorCoordToVisibleAuthoring(vertex.y)};
        const float dx = vertexMap.x - mapPoint.x;
        const float dy = vertexMap.y - mapPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > maxDistance2) {
            continue;
        }
        if (bestVertexId >= 0
                && (distance2 > bestDistance2 + 0.001f
                        || (std::fabs(distance2 - bestDistance2) <= 0.001f
                                && vertex.id >= bestVertexId))) {
            continue;
        }

        bestDistance2 = distance2;
        bestVertexId = vertex.id;
        bestPoint = SectorTopologyCoordPoint{vertex.x, vertex.y};
    }

    if (bestVertexId < 0) {
        return false;
    }
    if (outVertexId != nullptr) {
        *outVertexId = bestVertexId;
    }
    if (outPoint != nullptr) {
        *outPoint = bestPoint;
    }
    return true;
}

bool FindSectorEditorAuthoringSelectionNearMapPoint(
        const SectorAuthoringGraph& graph,
        Vector2 mapPoint,
        float vertexMaxDistance,
        float lineMaxDistance,
        SectorAuthoringSelectionTarget* outTarget,
        SectorTopologyCoordPoint* outVertexPoint)
{
    int vertexId = -1;
    SectorTopologyCoordPoint vertexPoint{};
    if (FindSectorEditorAuthoringVertexNearMapPoint(
                graph,
                mapPoint,
                vertexMaxDistance,
                &vertexId,
                &vertexPoint)) {
        if (outTarget != nullptr) {
            *outTarget = MakeSectorAuthoringVertexSelectionTarget(vertexId);
        }
        if (outVertexPoint != nullptr) {
            *outVertexPoint = vertexPoint;
        }
        return true;
    }

    int lineId = -1;
    if (FindSectorEditorAuthoringLineNearMapPoint(
                graph,
                mapPoint,
                lineMaxDistance,
                &lineId)) {
        if (outTarget != nullptr) {
            *outTarget = MakeSectorAuthoringLineSelectionTarget(lineId);
        }
        if (outVertexPoint != nullptr) {
            *outVertexPoint = SectorTopologyCoordPoint{};
        }
        return true;
    }

    if (outTarget != nullptr) {
        *outTarget = EmptyAuthoringSelectionTarget();
    }
    if (outVertexPoint != nullptr) {
        *outVertexPoint = SectorTopologyCoordPoint{};
    }
    return false;
}

bool FindSectorEditorAuthoringFaceAnchorAtMapPoint(
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        Vector2 mapPoint,
        int* outFaceAnchorId,
        std::string* outStatus)
{
    if (outFaceAnchorId != nullptr) {
        *outFaceAnchorId = -1;
    }
    if (outStatus != nullptr) {
        outStatus->clear();
    }

    if (!CurrentAuthoringDerivationAvailable(authoringDerivationCurrent, authoringDerivation)) {
        if (outStatus != nullptr) {
            *outStatus = "Authoring face selection requires current valid derived topology: no valid derived topology is available";
        }
        return false;
    }

    int bestFaceAnchorId = -1;
    double bestArea = 0.0;
    bool foundCandidate = false;
    bool foundContainingFace = false;
    for (const SectorAuthoringExtractedFace& face : authoringDerivation.faces.faces) {
        if (!SectorAuthoringFaceContainsMapPoint(authoringDerivation.planar, face, mapPoint)) {
            continue;
        }
        foundContainingFace = true;

        int faceAnchorId = -1;
        bool foundMapping = false;
        for (const SectorAuthoringResolvedFaceMapping& mapping
                : authoringDerivation.mapping.resolvedFaces) {
            if (mapping.extractedFaceId != face.id
                    || !IsValidSectorAuthoringId(mapping.faceAnchorId)
                    || FindSectorAuthoringFaceAnchor(authoringGraph, mapping.faceAnchorId) == nullptr) {
                continue;
            }
            if (foundMapping) {
                if (outStatus != nullptr) {
                    *outStatus = "Authoring face selection unavailable: derived face has ambiguous face anchor mapping";
                }
                return false;
            }
            faceAnchorId = mapping.faceAnchorId;
            foundMapping = true;
        }

        if (!foundMapping) {
            continue;
        }

        if (face.signedArea <= 0.0) {
            continue;
        }

        if (!foundCandidate
                || face.signedArea < bestArea
                || (face.signedArea == bestArea && faceAnchorId < bestFaceAnchorId)) {
            bestFaceAnchorId = faceAnchorId;
            bestArea = face.signedArea;
            foundCandidate = true;
        }
    }

    if (!foundCandidate) {
        if (outStatus != nullptr) {
            if (!foundContainingFace) {
                *outStatus = "Authoring face selection unavailable: no authoring face under cursor";
            } else {
                *outStatus = "Authoring face selection unavailable: derived face has no face anchor mapping";
            }
        }
        return false;
    }

    if (outFaceAnchorId != nullptr) {
        *outFaceAnchorId = bestFaceAnchorId;
    }
    return true;
}

bool FindSectorEditorAuthoringSelectionAtMapPoint(
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        Vector2 mapPoint,
        float vertexMaxDistance,
        float lineMaxDistance,
        SectorAuthoringSelectionTarget* outTarget,
        SectorTopologyCoordPoint* outVertexPoint,
        std::string* outStatus)
{
    if (outStatus != nullptr) {
        outStatus->clear();
    }

    if (FindSectorEditorAuthoringSelectionNearMapPoint(
                authoringGraph,
                mapPoint,
                vertexMaxDistance,
                lineMaxDistance,
                outTarget,
                outVertexPoint)) {
        return true;
    }

    int faceAnchorId = -1;
    if (FindSectorEditorAuthoringFaceAnchorAtMapPoint(
                authoringGraph,
                authoringDerivation,
                authoringDerivationCurrent,
                mapPoint,
                &faceAnchorId,
                outStatus)) {
        if (outTarget != nullptr) {
            *outTarget = MakeSectorAuthoringFaceAnchorSelectionTarget(faceAnchorId);
        }
        if (outVertexPoint != nullptr) {
            *outVertexPoint = SectorTopologyCoordPoint{};
        }
        return true;
    }

    if (outTarget != nullptr) {
        *outTarget = EmptyAuthoringSelectionTarget();
    }
    if (outVertexPoint != nullptr) {
        *outVertexPoint = SectorTopologyCoordPoint{};
    }
    return false;
}

bool AddSectorEditorAuthoringLineSegment(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SelectionState& selectionState,
        SectorTopologyCoordPoint start,
        SectorTopologyCoordPoint end,
        int* outLineId,
        SectorEditorAuthoringLineSegmentResult* outResult)
{
    SectorAuthoringGraph candidate = authoringGraph;
    SectorEditorAuthoringSegmentInsertResult insertResult;
    if (!InsertSectorEditorAuthoringLineSegmentIntoGraphWithSplits(
                candidate,
                start,
                end,
                &insertResult)) {
        if (outResult != nullptr) {
            outResult->errorMessage = insertResult.errorMessage;
        }
        return false;
    }

    std::string doorError;
    if (!ValidateSectorEditorAuthoringCandidateDoorPortalSpans(
                topologyMap,
                derivation.authoringDerivation,
                candidate,
                doorError)) {
        if (outResult != nullptr) {
            outResult->errorMessage = std::move(doorError);
        }
        return false;
    }

    authoringGraph = std::move(candidate);
    PruneSectorEditorAuthoringSelectionToGraph(authoringGraph, selectionState);

    const SectorEditorAuthoringLineSegmentResult segment = insertResult.segments.back();
    if (outLineId != nullptr) {
        *outLineId = segment.lineId;
    }
    if (outResult != nullptr) {
        *outResult = segment;
    }
    MarkSectorEditorAuthoringGraphEdited(
            state,
            lifecycle,
            derivation,
            TextFormat("Added authoring line %d", segment.lineId));
    RefreshSectorEditorAuthoringDerivation(
            state,
            lifecycle,
            topologyMap,
            authoringGraph,
            derivation,
            TextFormat("Added authoring line %d; derived topology current", segment.lineId),
            TextFormat("Added authoring line %d; derivation failed", segment.lineId));
    return true;
}
SectorEditorAuthoringLineToolClickResult ClickSectorEditorAuthoringLineTool(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SelectionState& selectionState,
        SectorTopologyCoordPoint point)
{
    SectorEditorAuthoringLineToolClickResult result;

    if (!state.pendingAuthoringLine.active) {
        state.pendingAuthoringLine.active = true;
        state.pendingAuthoringLine.startPoint = point;
        state.pendingAuthoringLine.errorMessage.clear();
        state.pendingAuthoringLine.startVertexId = -1;
        FindSectorAuthoringVertexAtPoint(
                authoringGraph,
                point,
                &state.pendingAuthoringLine.startVertexId);
        result.status = SectorEditorAuthoringLineToolClickStatus::StartedChain;
        return result;
    }

    if (state.pendingAuthoringLine.startPoint.x == point.x
            && state.pendingAuthoringLine.startPoint.y == point.y) {
        state.pendingAuthoringLine.errorMessage = "Line needs non-zero length";
        result.status = SectorEditorAuthoringLineToolClickStatus::ZeroLength;
        return result;
    }

    int lineId = -1;
    SectorEditorAuthoringLineSegmentResult segment;
    if (!AddSectorEditorAuthoringLineSegment(
                state,
                lifecycle,
                topologyMap,
                authoringGraph,
                derivation,
                selectionState,
                state.pendingAuthoringLine.startPoint,
                point,
                &lineId,
                &segment)) {
        state.pendingAuthoringLine.errorMessage = segment.errorMessage.empty()
                ? "Authoring line segment rejected"
                : segment.errorMessage;
        result.status = SectorEditorAuthoringLineToolClickStatus::Rejected;
        return result;
    }

    state.pendingAuthoringLine.active = true;
    state.pendingAuthoringLine.startPoint = segment.endPoint;
    state.pendingAuthoringLine.startVertexId = segment.endVertexId;
    state.pendingAuthoringLine.errorMessage.clear();
    result.status = SectorEditorAuthoringLineToolClickStatus::CreatedSegment;
    result.segment = segment;
    return result;
}
void CancelSectorEditorAuthoringLineToolChain(SectorEditorState& state)
{
    state.pendingAuthoringLine = PendingAuthoringLineDraw{};
}

bool CreateSectorAuthoringRectangle(
        SectorAuthoringGraph& graph,
        SectorTopologyCoordPoint firstCorner,
        SectorTopologyCoordPoint oppositeCorner,
        SectorEditorAuthoringRectangleResult* outResult)
{
    const SectorCoord minX = std::min(firstCorner.x, oppositeCorner.x);
    const SectorCoord maxX = std::max(firstCorner.x, oppositeCorner.x);
    const SectorCoord minY = std::min(firstCorner.y, oppositeCorner.y);
    const SectorCoord maxY = std::max(firstCorner.y, oppositeCorner.y);
    if (minX == maxX || minY == maxY) {
        return false;
    }

    SectorAuthoringGraph candidate = graph;
    SectorEditorAuthoringRectangleResult result;
    const std::array<SectorTopologyCoordPoint, 4> corners{{
            {minX, minY},
            {maxX, minY},
            {maxX, maxY},
            {minX, maxY}
    }};

    for (std::size_t index = 0; index < corners.size(); ++index) {
        int vertexId = -1;
        if (!FindSectorAuthoringVertexAtPoint(candidate, corners[index], &vertexId)) {
            if (!AddSectorAuthoringVertex(
                        candidate,
                        corners[index].x,
                        corners[index].y,
                        &vertexId)) {
                return false;
            }
        }
        result.vertexIds[index] = vertexId;
    }

    for (std::size_t index = 0; index < corners.size(); ++index) {
        SectorEditorAuthoringSegmentInsertResult edgeResult;
        if (!InsertSectorEditorAuthoringLineSegmentIntoGraphWithSplits(
                    candidate,
                    corners[index],
                    corners[(index + 1) % corners.size()],
                    &edgeResult)) {
            result.errorMessage = edgeResult.errorMessage.empty()
                    ? "Cannot insert rectangle: duplicate edge."
                    : edgeResult.errorMessage;
            const std::string prefix = "Cannot insert line:";
            if (result.errorMessage.compare(0, prefix.size(), prefix) == 0) {
                result.errorMessage.replace(0, prefix.size(), "Cannot insert rectangle:");
            }
            if (outResult != nullptr) {
                *outResult = result;
            }
            return false;
        }
        result.lineIds[index] = edgeResult.segments.front().lineId;
        for (const SectorEditorAuthoringLineSegmentResult& segment : edgeResult.segments) {
            result.insertedLineIds.push_back(segment.lineId);
        }
    }

    graph = std::move(candidate);
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

bool AddSectorEditorAuthoringRectangle(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SectorTopologyCoordPoint firstCorner,
        SectorTopologyCoordPoint oppositeCorner,
        SectorEditorAuthoringRectangleResult* outResult)
{
    SectorEditorAuthoringRectangleResult result;
    SectorAuthoringGraph candidate = authoringGraph;
    if (!CreateSectorAuthoringRectangle(
                candidate,
                firstCorner,
                oppositeCorner,
                &result)) {
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    std::string doorError;
    if (!ValidateSectorEditorAuthoringCandidateDoorPortalSpans(
                topologyMap,
                derivation.authoringDerivation,
                candidate,
                doorError)) {
        result.errorMessage = std::move(doorError);
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    authoringGraph = std::move(candidate);

    MarkSectorEditorAuthoringGraphEdited(state, lifecycle, derivation, "Created authoring rectangle");
    RefreshSectorEditorAuthoringDerivation(
            state,
            lifecycle,
            topologyMap,
            authoringGraph,
            derivation,
            "Created authoring rectangle; derived topology current",
            "Created authoring rectangle; derivation failed");
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}
bool InsertSectorEditorAuthoringVertexOnLine(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SelectionState& selectionState,
        int lineId,
        SectorTopologyCoordPoint point,
        SectorAuthoringInsertVertexResult* outResult)
{
    SectorAuthoringInsertVertexResult result;
    if (!InsertSectorAuthoringVertexOnLine(authoringGraph, lineId, point, &result)) {
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    PruneSectorEditorAuthoringSelectionToGraph(authoringGraph, selectionState);
    SelectSectorEditorAuthoringVertex(authoringGraph, selectionState, result.vertexId);
    MarkSectorEditorAuthoringGraphEdited(state, lifecycle, derivation, "Inserted vertex on authoring line");
    RefreshSectorEditorAuthoringDerivation(
            state,
            lifecycle,
            topologyMap,
            authoringGraph,
            derivation,
            "Inserted vertex on authoring line; derived topology current",
            "Inserted vertex on authoring line; derivation failed");
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}
bool MoveSectorEditorAuthoringVertex(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SelectionState& selectionState,
        int vertexId,
        SectorTopologyCoordPoint target)
{
    SectorAuthoringVertex* vertex = FindSectorAuthoringVertex(authoringGraph, vertexId);
    if (vertex == nullptr) {
        return false;
    }
    if (vertex->x == target.x && vertex->y == target.y) {
        return false;
    }

    vertex->x = target.x;
    vertex->y = target.y;
    PruneSectorEditorAuthoringSelectionToGraph(authoringGraph, selectionState);
    MarkSectorEditorAuthoringGraphEdited(
            state,
            lifecycle,
            derivation,
            TextFormat("Moved authoring vertex %d", vertexId));
    RefreshSectorEditorAuthoringDerivation(
            state,
            lifecycle,
            topologyMap,
            authoringGraph,
            derivation,
            TextFormat("Moved authoring vertex %d; derived topology current", vertexId),
            TextFormat("Moved authoring vertex %d; derivation failed", vertexId));
    return true;
}
bool DeleteSectorEditorSelectedAuthoringLine(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SelectionState& selectionState)
{
    if (selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::Line
            || !IsSectorAuthoringSelectionTargetValid(
                    authoringGraph,
                    selectionState.selectedAuthoring)) {
        return false;
    }

    const int lineId = selectionState.selectedAuthoring.lineId;
    SectorAuthoringGraph candidateGraph = authoringGraph;
    std::set<int> endpointIds;
    if (const SectorAuthoringLine* line =
                FindSectorAuthoringLine(candidateGraph, lineId)) {
        endpointIds.insert(line->startVertexId);
        endpointIds.insert(line->endVertexId);
    }
    candidateGraph.lines.erase(
            std::remove_if(
                    candidateGraph.lines.begin(),
                    candidateGraph.lines.end(),
                    [lineId](const SectorAuthoringLine& line) {
                        return line.id == lineId;
                    }),
            candidateGraph.lines.end());
    candidateGraph.lineSides.erase(
            std::remove_if(
                    candidateGraph.lineSides.begin(),
                    candidateGraph.lineSides.end(),
                    [lineId](const SectorAuthoringLineSide& side) {
                        return side.id.lineId == lineId;
                    }),
            candidateGraph.lineSides.end());

    std::set<int> referencedVertexIds;
    for (const SectorAuthoringLine& line : candidateGraph.lines) {
        referencedVertexIds.insert(line.startVertexId);
        referencedVertexIds.insert(line.endVertexId);
    }
    candidateGraph.vertices.erase(
            std::remove_if(
                    candidateGraph.vertices.begin(),
                    candidateGraph.vertices.end(),
                    [&endpointIds, &referencedVertexIds](
                            const SectorAuthoringVertex& vertex) {
                        return endpointIds.find(vertex.id) != endpointIds.end()
                                && referencedVertexIds.find(vertex.id)
                                        == referencedVertexIds.end();
                    }),
            candidateGraph.vertices.end());

    SectorAuthoringDerivationResult candidateDerivation =
            DeriveSectorTopologyMapFromAuthoringGraph(candidateGraph);
    if (!candidateDerivation.success
            || !AllDerivedSectorsHaveUniqueFaceAnchorMappings(
                    candidateGraph,
                    candidateDerivation)) {
        derivation.authoringDerivationStatus = candidateDerivation.success
                ? TextFormat(
                        "Cannot delete authoring line %d: the result leaves a face without exactly one anchor; use Merge Selected Into",
                        lineId)
                : BuildAuthoringDerivationFailureStatus(
                        TextFormat(
                                "Cannot delete authoring line %d atomically; use Merge Selected Into",
                                lineId),
                        candidateDerivation);
        lifecycle.topologyDocumentStatus = derivation.authoringDerivationStatus;
        return false;
    }

    return CommitSectorEditorAuthoringGraphCandidate(
            state,
            lifecycle,
            topologyMap,
            authoringGraph,
            derivation,
            selectionState,
            std::move(candidateGraph),
            std::move(candidateDerivation),
            topologyMap,
            TextFormat("Deleted authoring line %d; derived topology current", lineId));
}

bool CommitSectorEditorAuthoringGraphCandidate(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SelectionState& selectionState,
        SectorAuthoringGraph candidateGraph,
        SectorAuthoringDerivationResult candidateDerivation,
        const SectorTopologyMap& candidateMapData,
        const char* status)
{
    if (!candidateDerivation.success) {
        return false;
    }

    CopyEditorMapLevelFields(candidateDerivation.topology, candidateMapData);
    authoringGraph = std::move(candidateGraph);
    topologyMap = candidateDerivation.topology;
    derivation.authoringDerivation = std::move(candidateDerivation);
    derivation.lastValidAuthoringDerivedTopology = topologyMap;
    derivation.lastValidFaceAnchorBindings =
            BuildFaceAnchorBindings(derivation.authoringDerivation);
    derivation.authoringDerivedTopologyStale = false;
    derivation.authoringDerivationState =
            SectorEditorAuthoringDerivationState::ValidCurrent;
    derivation.authoringDerivationStatus = status == nullptr || status[0] == '\0'
            ? "Authoring graph: derived topology current"
            : status;
    lifecycle.topologyDocumentStatus = derivation.authoringDerivationStatus;
    lifecycle.topologyDocumentDirty = true;
    lifecycle.hasUnsavedChanges = true;
    InvalidateEditorTopologyRenderCache(
            state.topologyRenderRevision,
            state.topologyRenderCache);
    PruneSectorEditorAuthoringSelectionToGraph(authoringGraph, selectionState);
    return true;
}

bool DeleteSectorEditorSelectedAuthoringVertex(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SelectionState& selectionState)
{
    if (selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::Vertex
            || !IsSectorAuthoringSelectionTargetValid(
                    authoringGraph,
                    selectionState.selectedAuthoring)) {
        return false;
    }

    const auto reject = [&](std::string status) {
        derivation.authoringDerivationStatus = std::move(status);
        lifecycle.topologyDocumentStatus = derivation.authoringDerivationStatus;
        return false;
    };
    const int vertexId = selectionState.selectedAuthoring.vertexId;
    std::vector<SectorAuthoringLine> incidentLines;
    incidentLines.reserve(2);
    for (const SectorAuthoringLine& line : authoringGraph.lines) {
        if (line.startVertexId == vertexId || line.endVertexId == vertexId) {
            incidentLines.push_back(line);
        }
    }
    if (incidentLines.size() == 1) {
        return reject(
                "Cannot dissolve authoring vertex: degree-1 vertices keep their connected line");
    }
    if (incidentLines.size() > 2) {
        return reject(
                "Cannot dissolve authoring vertex: branching vertices require exactly two incident lines");
    }
    if (!IsSectorEditorAuthoringDerivationCurrent(derivation)) {
        return reject(
                "Cannot delete authoring vertex: derived topology is not current");
    }

    SectorAuthoringGraph candidateGraph = authoringGraph;
    int survivingLineId = -1;
    int removedLineId = -1;
    std::vector<SectorEditorFaceAnchorBinding> candidateBindings =
            derivation.lastValidFaceAnchorBindings;
    if (incidentLines.size() == 2) {
        std::sort(
                incidentLines.begin(),
                incidentLines.end(),
                [](const SectorAuthoringLine& lhs, const SectorAuthoringLine& rhs) {
                    return lhs.id < rhs.id;
                });
        const SectorAuthoringLine& survivingLine = incidentLines[0];
        const SectorAuthoringLine& removedLine = incidentLines[1];
        survivingLineId = survivingLine.id;
        removedLineId = removedLine.id;

        const auto otherEndpoint = [vertexId](const SectorAuthoringLine& line) {
            if (line.startVertexId == vertexId && line.endVertexId != vertexId) {
                return line.endVertexId;
            }
            if (line.endVertexId == vertexId && line.startVertexId != vertexId) {
                return line.startVertexId;
            }
            return -1;
        };
        const int survivingOtherVertexId = otherEndpoint(survivingLine);
        const int removedOtherVertexId = otherEndpoint(removedLine);
        if (!IsValidSectorAuthoringId(survivingOtherVertexId)
                || !IsValidSectorAuthoringId(removedOtherVertexId)
                || FindSectorAuthoringVertex(
                           authoringGraph,
                           survivingOtherVertexId) == nullptr
                || FindSectorAuthoringVertex(
                           authoringGraph,
                           removedOtherVertexId) == nullptr) {
            return reject(
                    "Cannot dissolve authoring vertex: an incident line endpoint is invalid");
        }
        if (survivingOtherVertexId == removedOtherVertexId) {
            return reject(
                    "Cannot dissolve authoring vertex: replacement line would collapse");
        }
        for (const SectorAuthoringLine& line : authoringGraph.lines) {
            if (line.id == survivingLineId || line.id == removedLineId) {
                continue;
            }
            const bool sameEndpoints =
                    (line.startVertexId == survivingOtherVertexId
                            && line.endVertexId == removedOtherVertexId)
                    || (line.startVertexId == removedOtherVertexId
                            && line.endVertexId == survivingOtherVertexId);
            if (sameEndpoints) {
                return reject(
                        "Cannot dissolve authoring vertex: replacement line already exists");
            }
        }

        std::string bindingError;
        if (!BuildDissolvedFaceAnchorBindings(
                    derivation.lastValidFaceAnchorBindings,
                    survivingLineId,
                    removedLineId,
                    candidateBindings,
                    bindingError)) {
            return reject(std::move(bindingError));
        }

        SectorAuthoringLine* candidateSurvivor =
                FindSectorAuthoringLine(candidateGraph, survivingLineId);
        if (candidateSurvivor == nullptr) {
            return reject(
                    "Cannot dissolve authoring vertex: surviving line is missing");
        }
        if (candidateSurvivor->startVertexId == vertexId) {
            candidateSurvivor->startVertexId = removedOtherVertexId;
        } else if (candidateSurvivor->endVertexId == vertexId) {
            candidateSurvivor->endVertexId = removedOtherVertexId;
        } else {
            return reject(
                    "Cannot dissolve authoring vertex: surviving line is no longer incident");
        }
        candidateGraph.lines.erase(
                std::remove_if(
                        candidateGraph.lines.begin(),
                        candidateGraph.lines.end(),
                        [removedLineId](const SectorAuthoringLine& line) {
                            return line.id == removedLineId;
                        }),
                candidateGraph.lines.end());
        candidateGraph.lineSides.erase(
                std::remove_if(
                        candidateGraph.lineSides.begin(),
                        candidateGraph.lineSides.end(),
                        [removedLineId](const SectorAuthoringLineSide& side) {
                            return side.id.lineId == removedLineId;
                        }),
                candidateGraph.lineSides.end());
    }

    candidateGraph.vertices.erase(
            std::remove_if(
                    candidateGraph.vertices.begin(),
                    candidateGraph.vertices.end(),
                    [vertexId](const SectorAuthoringVertex& vertex) {
                        return vertex.id == vertexId;
                    }),
            candidateGraph.vertices.end());

    SectorAuthoringDerivationResult candidateDerivation =
            DeriveSectorTopologyMapFromAuthoringGraph(candidateGraph);
    int relocatedAnchorCount = 0;
    int repairFailureAnchorId = -1;
    std::string repairFailureReason;
    SectorAuthoringGraph repairedGraph;
    SectorAuthoringDerivationResult repairedDerivation;
    const FaceAnchorAutoFollowOutcome repairOutcome = TryAutoFollowFaceAnchors(
            candidateGraph,
            candidateDerivation,
            candidateBindings,
            repairedGraph,
            repairedDerivation,
            relocatedAnchorCount,
            repairFailureAnchorId,
            repairFailureReason);
    if (repairOutcome == FaceAnchorAutoFollowOutcome::Repaired) {
        candidateGraph = std::move(repairedGraph);
        candidateDerivation = std::move(repairedDerivation);
    } else if (repairOutcome == FaceAnchorAutoFollowOutcome::Failed) {
        std::string status = "Cannot dissolve authoring vertex: face-anchor auto-follow failed";
        if (IsValidSectorAuthoringId(repairFailureAnchorId)) {
            status += " for anchor " + std::to_string(repairFailureAnchorId);
        }
        if (!repairFailureReason.empty()) {
            status += ": " + repairFailureReason;
        }
        return reject(std::move(status));
    }

    if (!candidateDerivation.success) {
        return reject(BuildAuthoringDerivationFailureStatus(
                incidentLines.empty()
                        ? "Cannot delete isolated authoring vertex atomically"
                        : "Cannot dissolve authoring vertex atomically",
                candidateDerivation));
    }
    if (!AllExtractedFacesHaveUniqueFaceAnchorMappings(
                candidateGraph,
                candidateDerivation)) {
        return reject(
                "Cannot dissolve authoring vertex: result leaves a face without exactly one anchor");
    }
    if (IsValidSectorAuthoringId(survivingLineId)) {
        int survivingFragmentCount = 0;
        for (const SectorAuthoringDerivedLineMapping& mapping
                : candidateDerivation.mapping.lines) {
            if (mapping.authoringLineId == survivingLineId) {
                ++survivingFragmentCount;
            }
        }
        if (survivingFragmentCount != 1) {
            return reject(
                    "Cannot dissolve authoring vertex: replacement line crosses or passes through other authoring geometry");
        }
    }

    SectorTopologyMap candidateMapData = topologyMap;
    const std::set<int> removedAuthoringLineIds =
            IsValidSectorAuthoringId(removedLineId)
            ? std::set<int>{removedLineId}
            : std::set<int>{};
    const std::set<int> rejectDoorAuthoringLineIds =
            IsValidSectorAuthoringId(survivingLineId)
            ? std::set<int>{survivingLineId, removedLineId}
            : std::set<int>{};
    std::vector<int> removedDoorIds;
    std::string doorError;
    if (!ReconcileSectorEditorAuthoringCandidateDoors(
                topologyMap,
                derivation.authoringDerivation,
                candidateDerivation,
                removedAuthoringLineIds,
                rejectDoorAuthoringLineIds,
                candidateMapData,
                removedDoorIds,
                doorError)) {
        return reject(std::move(doorError));
    }

    std::string successStatus = incidentLines.empty()
            ? TextFormat("Deleted isolated authoring vertex %d", vertexId)
            : TextFormat(
                    "Dissolved authoring vertex %d into line %d",
                    vertexId,
                    survivingLineId);
    if (relocatedAnchorCount > 0) {
        successStatus += TextFormat(
                "; auto-followed %d face anchor%s",
                relocatedAnchorCount,
                relocatedAnchorCount == 1 ? "" : "s");
    }
    if (!CommitSectorEditorAuthoringGraphCandidate(
                state,
                lifecycle,
                topologyMap,
                authoringGraph,
                derivation,
                selectionState,
                std::move(candidateGraph),
                std::move(candidateDerivation),
                candidateMapData,
                successStatus.c_str())) {
        return reject("Authoring vertex delete failed during commit");
    }
    if (IsValidSectorAuthoringId(survivingLineId)) {
        SelectSectorEditorAuthoringLine(
                authoringGraph,
                selectionState,
                survivingLineId);
    }
    return true;
}
void InitializeSectorEditorAuthoringStateFromTopology(
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        const SectorTopologyMap& sourceMap)
{
    authoringGraph = ImportSectorTopologyMapToAuthoringGraph(sourceMap);
    derivation.authoringDerivation = DeriveSectorTopologyMapFromAuthoringGraph(authoringGraph);
    if (derivation.authoringDerivation.success) {
        CopyEditorMapLevelFields(derivation.authoringDerivation.topology, sourceMap);
        derivation.lastValidAuthoringDerivedTopology = sourceMap;
        derivation.lastValidFaceAnchorBindings =
                BuildFaceAnchorBindings(derivation.authoringDerivation);
        derivation.authoringDerivationState = SectorEditorAuthoringDerivationState::ValidCurrent;
        derivation.authoringDerivedTopologyStale = false;
        derivation.authoringDerivationStatus = "Authoring graph: derived topology current";
    } else {
        derivation.lastValidAuthoringDerivedTopology.reset();
        derivation.lastValidFaceAnchorBindings.clear();
        derivation.authoringDerivationState = SectorEditorAuthoringDerivationState::InvalidNoDerived;
        derivation.authoringDerivedTopologyStale = true;
        derivation.authoringDerivationStatus = "Authoring graph: no valid derived topology";
    }
}
void MarkSectorEditorAuthoringGraphEdited(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorEditorDerivationDocumentAccess derivation,
        const char* status)
{
    MarkSectorEditorAuthoringGraphEdited(
            lifecycle,
            state.topologyRenderRevision,
            state.topologyRenderCache,
            derivation,
            status);
}

void MarkSectorEditorAuthoringGraphEdited(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorEditorDerivationDocumentAccess derivation,
        const char* status)
{
    lifecycle.topologyDocumentDirty = true;
    lifecycle.hasUnsavedChanges = true;
    derivation.authoringDerivedTopologyStale = true;
    derivation.authoringDerivationState = derivation.lastValidAuthoringDerivedTopology.has_value()
            ? SectorEditorAuthoringDerivationState::ValidStale
            : SectorEditorAuthoringDerivationState::InvalidNoDerived;
    derivation.authoringDerivationStatus = status == nullptr || status[0] == '\0'
            ? "Authoring graph edited; derived topology stale"
            : status;
    lifecycle.topologyDocumentStatus = derivation.authoringDerivationStatus;
    InvalidateEditorTopologyRenderCache(topologyRenderRevision, topologyRenderCache);
}

bool SetSectorEditorSectorLighting(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        const SelectionState& selectionState,
        SectorEditorSectorLightingScope scope,
        float ambientIntensity,
        Color ambientColor,
        std::string* outStatus)
{
    ambientIntensity = ClampAmbientIntensity(ambientIntensity);
    ambientColor.a = 255;

    int sectorCount = 0;
    bool changed = false;
    for (SectorAuthoringFaceAnchor& anchor : authoringGraph.faceAnchors) {
        if (anchor.isVoid
                || (scope == SectorEditorSectorLightingScope::Selected
                        && !IsSectorEditorAuthoringFaceSelected(
                                selectionState,
                                anchor.id))) {
            continue;
        }
        ++sectorCount;
        if (anchor.ambientIntensity == ambientIntensity
                && anchor.ambientColor.r == ambientColor.r
                && anchor.ambientColor.g == ambientColor.g
                && anchor.ambientColor.b == ambientColor.b
                && anchor.ambientColor.a == ambientColor.a) {
            continue;
        }
        anchor.ambientIntensity = ambientIntensity;
        anchor.ambientColor = ambientColor;
        changed = true;
    }

    if (sectorCount == 0) {
        if (outStatus != nullptr) {
            *outStatus = scope == SectorEditorSectorLightingScope::Selected
                    ? "Set All: no selected sectors to update."
                    : "Set All: no sectors to update.";
        }
        return false;
    }
    if (!changed) {
        if (outStatus != nullptr) {
            *outStatus = scope == SectorEditorSectorLightingScope::Selected
                    ? "Selected sectors already use this lighting."
                    : "All sectors already use this lighting.";
        }
        return true;
    }

    const std::string successStatus = TextFormat(
            scope == SectorEditorSectorLightingScope::Selected
                    ? "Set lighting for %d selected sector%s."
                    : "Set lighting for %d sector%s.",
            sectorCount,
            sectorCount == 1 ? "" : "s");
    const char* failureStatus =
            "Set All updated authoring sector lighting; derivation failed.";
    MarkSectorEditorAuthoringGraphEdited(
            state,
            lifecycle,
            derivation,
            successStatus.c_str());
    const bool derivationCurrent = RefreshSectorEditorAuthoringDerivation(
            state,
            lifecycle,
            topologyMap,
            authoringGraph,
            derivation,
            successStatus.c_str(),
            failureStatus);
    if (outStatus != nullptr) {
        *outStatus = derivationCurrent ? successStatus : failureStatus;
    }
    return derivationCurrent;
}

int FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        int topologySectorId)
{
    if (!IsValidSectorTopologyId(topologySectorId)) {
        return -1;
    }

    for (const SectorAuthoringDerivedSectorMapping& mapping
            : authoringDerivation.mapping.sectors) {
        if (mapping.topologySectorId == topologySectorId
                && IsValidSectorAuthoringId(mapping.faceAnchorId)
                && FindSectorAuthoringFaceAnchor(
                        authoringGraph,
                        mapping.faceAnchorId) != nullptr) {
            return mapping.faceAnchorId;
        }
    }
    return -1;
}

bool FindSectorEditorAuthoringSideIdForTopologySideDef(
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        int topologySideDefId,
        SectorAuthoringSideId& outSideId)
{
    outSideId = SectorAuthoringSideId{};
    if (!IsValidSectorTopologyId(topologySideDefId)) {
        return false;
    }

    for (const SectorAuthoringDerivedSideMapping& mapping
            : authoringDerivation.mapping.sides) {
        if (mapping.topologySideDefId == topologySideDefId
                && IsValidSectorAuthoringId(mapping.authoringLineId)
                && FindSectorAuthoringLine(
                        authoringGraph,
                        mapping.authoringLineId) != nullptr) {
            outSideId = SectorAuthoringSideId{mapping.authoringLineId, mapping.authoringSide};
            return true;
        }
    }
    return false;
}

int FindSectorEditorAuthoringLineIdForTopologyLineDef(
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        int topologyLineDefId)
{
    if (!IsValidSectorTopologyId(topologyLineDefId)) {
        return -1;
    }

    for (const SectorAuthoringDerivedLineMapping& mapping
            : authoringDerivation.mapping.lines) {
        if (mapping.topologyLineDefId == topologyLineDefId
                && IsValidSectorAuthoringId(mapping.authoringLineId)
                && FindSectorAuthoringLine(
                        authoringGraph,
                        mapping.authoringLineId) != nullptr) {
            return mapping.authoringLineId;
        }
    }
    return -1;
}

SectorEditorInspectorTarget ResolveSectorEditorInspectorTarget(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        const SelectionState& selectionState)
{
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line
            && FindSectorAuthoringLine(authoringGraph, selectionState.selectedAuthoring.lineId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringLine;
        target.lineId = selectionState.selectedAuthoring.lineId;
        return target;
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor
            && FindSectorAuthoringFaceAnchor(authoringGraph, selectionState.selectedAuthoring.faceAnchorId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringFaceAnchor;
        target.faceAnchorId = selectionState.selectedAuthoring.faceAnchorId;
        return target;
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex
            && FindSectorAuthoringVertex(authoringGraph, selectionState.selectedAuthoring.vertexId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringVertex;
        target.vertexId = selectionState.selectedAuthoring.vertexId;
        return target;
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::FogVolume
            && FindSectorAuthoringFogVolume(
                    authoringGraph,
                    selectionState.selectedAuthoring.fogVolumeId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringFogVolume;
        target.fogVolumeId = selectionState.selectedAuthoring.fogVolumeId;
        return target;
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::ReflectionProbe
            && FindSectorAuthoringReflectionProbe(
                    authoringGraph,
                    selectionState.selectedAuthoring.reflectionProbeId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringReflectionProbe;
        target.reflectionProbeId = selectionState.selectedAuthoring.reflectionProbeId;
        return target;
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::LevelMarker
            && FindSectorAuthoringLevelMarker(
                    authoringGraph,
                    selectionState.selectedAuthoring.levelMarkerId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringLevelMarker;
        target.levelMarkerId = selectionState.selectedAuthoring.levelMarkerId;
        return target;
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::SoundEmitter
            && FindSectorAuthoringSoundEmitter(
                    authoringGraph,
                    selectionState.selectedAuthoring.soundEmitterId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringSoundEmitter;
        target.soundEmitterId = selectionState.selectedAuthoring.soundEmitterId;
        return target;
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Trigger
            && FindSectorAuthoringTrigger(
                    authoringGraph,
                    selectionState.selectedAuthoring.triggerId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringTrigger;
        target.triggerId = selectionState.selectedAuthoring.triggerId;
        return target;
    }
    if (selectionState.selectedAuthoring.kind
                    == SectorAuthoringSelectionKind::StructuralPrimitive
            && FindSectorAuthoringStructuralPrimitive(
                    authoringGraph,
                    selectionState.selectedAuthoring.structuralPrimitiveId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringStructuralPrimitive;
        target.structuralPrimitiveId =
                selectionState.selectedAuthoring.structuralPrimitiveId;
        return target;
    }

    if (!HasAuthoringGraphData(authoringGraph)) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::LegacyTopology;
        return target;
    }

    if (selectionState.topologySelectionKind == TopologySelectionKind::Sector
            && IsValidSectorTopologyId(selectionState.selectedTopologySectorId)) {
        return ResolveMappedTopologySectorInspectorTarget(
                topologyMap,
                authoringGraph,
                authoringDerivation,
                authoringDerivationCurrent,
                selectionState.selectedTopologySectorId);
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::SideDef
            && IsValidSectorTopologyId(selectionState.selectedTopologySideDefId)) {
        return ResolveMappedTopologySideInspectorTarget(
                topologyMap,
                authoringGraph,
                authoringDerivation,
                authoringDerivationCurrent,
                selectionState.selectedTopologySideDefId);
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::LineDef
            && IsValidSectorTopologyId(selectionState.selectedTopologyLineDefId)) {
        return ResolveMappedTopologyLineInspectorTarget(
                topologyMap,
                authoringGraph,
                authoringDerivation,
                authoringDerivationCurrent,
                selectionState.selectedTopologyLineDefId);
    }

    return SectorEditorInspectorTarget{};
}

std::string BuildSectorEditorSurface3DTargetLabel(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        SectorSurfaceRef surface,
        TopologySurfaceEditTarget target)
{
    SectorEditorAuthoringSurfaceTarget authoringTarget;
    std::string status;
    const bool mapped = HasAuthoringGraphData(authoringGraph)
            && ResolveSectorEditorAuthoringSurfaceTarget(
                    topologyMap,
                    authoringGraph,
                    authoringDerivation,
                    authoringDerivationCurrent,
                    surface,
                    authoringTarget,
                    &status);

    std::ostringstream label;
    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        if (mapped && authoringTarget.kind == SectorEditorAuthoringSurfaceTargetKind::FaceAnchor) {
            label << (target.kind == TopologySurfaceEditTargetKind::SectorFloor
                            ? "Authoring Floor"
                            : "Authoring Ceiling")
                  << " | derived sector " << target.sectorId;
            return label.str();
        }
        label << (target.kind == TopologySurfaceEditTargetKind::SectorFloor ? "Floor" : "Ceiling")
              << " | sector " << target.sectorId;
        return label.str();
    }

    if (mapped && authoringTarget.kind == SectorEditorAuthoringSurfaceTargetKind::Side) {
        label << "Authoring Side | derived sideDef " << target.sideDefId
              << " line " << target.lineDefId;
        return label.str();
    }

    label << SurfaceKindName(surface.kind)
          << " | sideDef " << target.sideDefId
          << " line " << target.lineDefId;
    return label.str();
}

bool ResolveSectorEditorAuthoringSurfaceTarget(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        SectorSurfaceRef surface,
        SectorEditorAuthoringSurfaceTarget& outTarget,
        std::string* outStatus)
{
    outTarget = SectorEditorAuthoringSurfaceTarget{};
    const auto fail = [outStatus](const char* status) {
        if (outStatus != nullptr) {
            *outStatus = status;
        }
        return false;
    };

    if (surface.kind == SectorSurfaceKind::None) {
        return fail("3D surface edit unavailable: no selected surface");
    }
    if (!authoringDerivationCurrent) {
        return fail("3D surface edit unavailable: derived topology is not current");
    }

    if (IsWallSurface(surface.kind)) {
        const SectorTopologySideDef* sideDef =
                FindSectorTopologySideDef(topologyMap, surface.topologySideDefId);
        if (sideDef == nullptr
                || sideDef->lineDefId != surface.topologyLineDefId
                || sideDef->side != surface.topologySide
                || (surface.kind == SectorSurfaceKind::Middle
                        && !IsTopologyMiddleEligible(topologyMap, sideDef))) {
            return fail("3D surface edit unavailable: selected sidedef is not current");
        }

        bool found = false;
        SectorAuthoringSideId resolvedSide;
        for (const SectorAuthoringDerivedSideMapping& mapping
                : authoringDerivation.mapping.sides) {
            if (mapping.topologySideDefId != surface.topologySideDefId) {
                continue;
            }
            if (!IsValidSectorAuthoringId(mapping.authoringLineId)
                    || FindSectorAuthoringLine(
                            authoringGraph,
                            mapping.authoringLineId) == nullptr) {
                continue;
            }
            const SectorAuthoringSideId candidate{mapping.authoringLineId, mapping.authoringSide};
            if (found && (resolvedSide.lineId != candidate.lineId
                    || resolvedSide.side != candidate.side)) {
                return fail("3D surface edit unavailable: selected sidedef has ambiguous authoring mapping");
            }
            resolvedSide = candidate;
            found = true;
        }
        if (!found) {
            return fail("3D surface edit unavailable: selected sidedef has no authoring side mapping");
        }

        outTarget.kind = SectorEditorAuthoringSurfaceTargetKind::Side;
        outTarget.side = resolvedSide;
        return true;
    }

    if (FindSectorTopologySector(topologyMap, surface.topologySectorId) == nullptr) {
        return fail("3D surface edit unavailable: selected sector is not current");
    }

    bool found = false;
    int resolvedFaceAnchorId = -1;
    for (const SectorAuthoringDerivedSectorMapping& mapping
            : authoringDerivation.mapping.sectors) {
        if (mapping.topologySectorId != surface.topologySectorId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.faceAnchorId)
                || FindSectorAuthoringFaceAnchor(
                        authoringGraph,
                        mapping.faceAnchorId) == nullptr) {
            continue;
        }
        if (found && resolvedFaceAnchorId != mapping.faceAnchorId) {
            return fail("3D surface edit unavailable: selected sector has ambiguous face anchor mapping");
        }
        resolvedFaceAnchorId = mapping.faceAnchorId;
        found = true;
    }
    if (!found) {
        return fail("3D surface edit unavailable: selected sector has no face anchor mapping");
    }

    outTarget.kind = SectorEditorAuthoringSurfaceTargetKind::FaceAnchor;
    outTarget.faceAnchorId = resolvedFaceAnchorId;
    return true;
}

SectorAuthoringSelectionTarget MakeSectorEditorAuthoringSelectionTargetForSurfaceTarget(
        SectorEditorAuthoringSurfaceTarget target)
{
    if (target.kind == SectorEditorAuthoringSurfaceTargetKind::Side) {
        return MakeSectorAuthoringLineSelectionTarget(target.side.lineId);
    }
    if (target.kind == SectorEditorAuthoringSurfaceTargetKind::FaceAnchor) {
        return MakeSectorAuthoringFaceAnchorSelectionTarget(target.faceAnchorId);
    }
    return EmptyAuthoringSelectionTarget();
}

bool ClearSelectedSectorEditorSurface3DIfAuthoringMappingUnavailable(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        SectorEditorPreviewSelectionState& previewSelectionState,
        std::string* outStatus)
{
    if (outStatus != nullptr) {
        outStatus->clear();
    }

    if (!HasAuthoringGraphData(authoringGraph)
            || previewSelectionState.selectedSurface3D.kind == SectorSurfaceKind::None) {
        return true;
    }

    SectorEditorAuthoringSurfaceTarget authoringTarget;
    std::string status;
    if (ResolveSectorEditorAuthoringSurfaceTarget(
                topologyMap,
                authoringGraph,
                authoringDerivation,
                authoringDerivationCurrent,
                previewSelectionState.selectedSurface3D,
                authoringTarget,
                &status)) {
        return true;
    }

    previewSelectionState.selectedSurface3D = SectorSurfaceRef{};
    previewSelectionState.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    if (outStatus != nullptr) {
        *outStatus = status;
    }
    return false;
}

bool MutateSectorEditorAuthoringFaceAnchorForTopologySector(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologySectorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate)
{
    return MutateSectorEditorAuthoringFaceAnchorForTopologySector(
            lifecycle,
            state.topologyRenderRevision,
            state.topologyRenderCache,
            topologyMap,
            authoringGraph,
            derivation,
            topologySectorId,
            status,
            mutate);
}

bool MutateSectorEditorAuthoringFaceAnchorForTopologySector(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologySectorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate)
{
    if (!mutate) {
        return false;
    }

    const int faceAnchorId =
            FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                    authoringGraph,
                    derivation.authoringDerivation,
                    topologySectorId);
    return MutateSectorEditorAuthoringFaceAnchorById(
            lifecycle,
            topologyRenderRevision,
            topologyRenderCache,
            topologyMap,
            authoringGraph,
            derivation,
            faceAnchorId,
            status,
            mutate);
}

bool MutateSectorEditorAuthoringFaceAnchorById(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int faceAnchorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate)
{
    return MutateSectorEditorAuthoringFaceAnchorById(
            lifecycle,
            state.topologyRenderRevision,
            state.topologyRenderCache,
            topologyMap,
            authoringGraph,
            derivation,
            faceAnchorId,
            status,
            mutate);
}

bool MutateSectorEditorAuthoringFaceAnchorById(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int faceAnchorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate)
{
    if (!mutate) {
        return false;
    }

    SectorAuthoringFaceAnchor* anchor =
            FindSectorAuthoringFaceAnchor(authoringGraph, faceAnchorId);
    if (anchor == nullptr) {
        return false;
    }

    if (!mutate(*anchor)) {
        return false;
    }

    MarkSectorEditorAuthoringGraphEdited(
            lifecycle,
            topologyRenderRevision,
            topologyRenderCache,
            derivation,
            status);
    return RefreshSectorEditorAuthoringDerivation(
            lifecycle,
            topologyRenderRevision,
            topologyRenderCache,
            topologyMap,
            authoringGraph,
            derivation,
            status,
            "Updated authoring face anchor; derivation failed");
}

bool MutateSectorEditorAuthoringSideForTopologySideDef(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologySideDefId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate)
{
    return MutateSectorEditorAuthoringSideForTopologySideDef(
            lifecycle,
            state.topologyRenderRevision,
            state.topologyRenderCache,
            topologyMap,
            authoringGraph,
            derivation,
            topologySideDefId,
            status,
            mutate);
}

bool MutateSectorEditorAuthoringSideForTopologySideDef(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologySideDefId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate)
{
    if (!mutate) {
        return false;
    }

    SectorAuthoringSideId sideId;
    if (!FindSectorEditorAuthoringSideIdForTopologySideDef(
                authoringGraph,
                derivation.authoringDerivation,
                topologySideDefId,
                sideId)) {
        return false;
    }

    SectorAuthoringLineSide* side = FindSectorAuthoringLineSide(authoringGraph, sideId);
    if (side == nullptr) {
        const SectorTopologySideDef* topologySide =
                FindSectorTopologySideDef(topologyMap, topologySideDefId);
        SectorAuthoringLineSide newSide;
        newSide.id = sideId;
        if (topologySide != nullptr) {
            newSide.wall = topologySide->wall;
            newSide.lower = topologySide->lower;
            newSide.upper = topologySide->upper;
            newSide.middle = topologySide->middle;
        }
        authoringGraph.lineSides.push_back(newSide);
        side = &authoringGraph.lineSides.back();
    }

    if (!mutate(*side)) {
        return false;
    }

    MarkSectorEditorAuthoringGraphEdited(
            lifecycle,
            topologyRenderRevision,
            topologyRenderCache,
            derivation,
            status);
    return RefreshSectorEditorAuthoringDerivation(
            lifecycle,
            topologyRenderRevision,
            topologyRenderCache,
            topologyMap,
            authoringGraph,
            derivation,
            status,
            "Updated authoring side material; derivation failed");
}

bool MutateSectorEditorAuthoringSideById(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SectorAuthoringSideId sideId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate)
{
    return MutateSectorEditorAuthoringSideById(
            lifecycle,
            state.topologyRenderRevision,
            state.topologyRenderCache,
            topologyMap,
            authoringGraph,
            derivation,
            sideId,
            status,
            mutate);
}

bool MutateSectorEditorAuthoringSideById(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SectorAuthoringSideId sideId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate)
{
    if (!mutate
            || !IsValidSectorAuthoringId(sideId.lineId)
            || FindSectorAuthoringLine(authoringGraph, sideId.lineId) == nullptr) {
        return false;
    }

    SectorAuthoringLineSide* side = FindSectorAuthoringLineSide(authoringGraph, sideId);
    if (side == nullptr) {
        SectorAuthoringLineSide newSide;
        newSide.id = sideId;
        authoringGraph.lineSides.push_back(newSide);
        side = &authoringGraph.lineSides.back();
    }

    if (!mutate(*side)) {
        return false;
    }

    MarkSectorEditorAuthoringGraphEdited(
            lifecycle,
            topologyRenderRevision,
            topologyRenderCache,
            derivation,
            status);
    return RefreshSectorEditorAuthoringDerivation(
            lifecycle,
            topologyRenderRevision,
            topologyRenderCache,
            topologyMap,
            authoringGraph,
            derivation,
            status,
            "Updated authoring side material; derivation failed");
}

bool MutateSectorEditorAuthoringLineForTopologyLineDef(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologyLineDefId,
        const char* status,
        const std::function<bool(SectorAuthoringLine&)>& mutate)
{
    if (!mutate) {
        return false;
    }

    const int authoringLineId =
            FindSectorEditorAuthoringLineIdForTopologyLineDef(
                    authoringGraph,
                    derivation.authoringDerivation,
                    topologyLineDefId);
    return MutateSectorEditorAuthoringLineById(
            state,
            lifecycle,
            topologyMap,
            authoringGraph,
            derivation,
            authoringLineId,
            status,
            mutate);
}

bool MutateSectorEditorAuthoringLineById(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int lineId,
        const char* status,
        const std::function<bool(SectorAuthoringLine&)>& mutate)
{
    if (!mutate) {
        return false;
    }

    SectorAuthoringLine* line =
            FindSectorAuthoringLine(authoringGraph, lineId);
    if (line == nullptr) {
        return false;
    }

    if (!mutate(*line)) {
        return false;
    }

    MarkSectorEditorAuthoringGraphEdited(state, lifecycle, derivation, status);
    return RefreshSectorEditorAuthoringDerivation(
            state,
            lifecycle,
            topologyMap,
            authoringGraph,
            derivation,
            status,
            "Updated authoring line flags; derivation failed");
}

bool SetSectorEditorAuthoringLineDefBlocksPlayer(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologyLineDefId,
        bool blocksPlayer,
        std::string* outStatus)
{
    const auto fail = [outStatus](const char* status) {
        if (outStatus != nullptr) {
            *outStatus = status == nullptr ? "" : status;
        }
        return false;
    };

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
            topologyMap,
            topologyLineDefId);
    if (lineDef == nullptr) {
        return fail("Selected linedef is no longer valid.");
    }
    if (lineDef->frontSideDefId == -1 || lineDef->backSideDefId == -1) {
        return fail("Blocks Player is only editable on two-sided portals.");
    }
    if (!HasAuthoringGraphData(authoringGraph)) {
        return fail("Cannot edit line flag: authoring data is required.");
    }
    if (derivation.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
            || derivation.authoringDerivedTopologyStale
            || !derivation.authoringDerivation.success) {
        return fail("Blocks Player unavailable: derived topology is not current.");
    }

    const int authoringLineId =
            FindSectorEditorAuthoringLineIdForTopologyLineDef(
                    authoringGraph,
                    derivation.authoringDerivation,
                    topologyLineDefId);
    const SectorAuthoringLine* authoringLine =
            FindSectorAuthoringLine(authoringGraph, authoringLineId);
    if (authoringLine == nullptr) {
        return fail("Blocks Player unavailable: selected derived linedef has no authoring line mapping.");
    }
    if (authoringLine->flags.blocksPlayer == blocksPlayer) {
        if (outStatus != nullptr) {
            outStatus->clear();
        }
        return true;
    }

    const char* status = blocksPlayer
            ? "Enabled player blocking on authoring portal."
            : "Disabled player blocking on authoring portal.";
    const bool changed = MutateSectorEditorAuthoringLineById(
            state,
            lifecycle,
            topologyMap,
            authoringGraph,
            derivation,
            authoringLineId,
            status,
            [blocksPlayer](SectorAuthoringLine& line) {
                line.flags.blocksPlayer = blocksPlayer;
                return true;
            });
    if (!changed) {
        return fail("Blocks Player edit failed: authoring derivation failed.");
    }
    if (outStatus != nullptr) {
        *outStatus = status;
    }
    return true;
}

bool RefreshSectorEditorAuthoringDerivation(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        const char* successStatus,
        const char* failureStatus)
{
    return RefreshSectorEditorAuthoringDerivation(
            lifecycle,
            state.topologyRenderRevision,
            state.topologyRenderCache,
            topologyMap,
            authoringGraph,
            derivation,
            successStatus,
            failureStatus);
}

bool RefreshSectorEditorAuthoringDerivation(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        const char* successStatus,
        const char* failureStatus)
{
    const std::string ownedSuccessStatus =
            successStatus == nullptr ? std::string{} : std::string{successStatus};
    const std::string ownedFailureStatus =
            failureStatus == nullptr ? std::string{} : std::string{failureStatus};

    std::vector<SectorEditorFaceAnchorBinding> effectiveBindings =
            derivation.lastValidFaceAnchorBindings;
    if (derivation.authoringDerivation.success) {
        const std::vector<SectorEditorFaceAnchorBinding> previousBindings =
                BuildFaceAnchorBindings(derivation.authoringDerivation);
        bool recoveredBinding = false;
        for (const SectorEditorFaceAnchorBinding& binding : previousBindings) {
            if (FindFaceAnchorBinding(
                        effectiveBindings,
                        binding.faceAnchorId) != nullptr) {
                continue;
            }
            effectiveBindings.push_back(binding);
            recoveredBinding = true;
        }
        if (recoveredBinding) {
            std::sort(
                    effectiveBindings.begin(),
                    effectiveBindings.end(),
                    [](const SectorEditorFaceAnchorBinding& lhs,
                            const SectorEditorFaceAnchorBinding& rhs) {
                        return lhs.faceAnchorId < rhs.faceAnchorId;
                    });
            derivation.lastValidFaceAnchorBindings = effectiveBindings;
        }
    }

    SectorAuthoringDerivationResult result =
            DeriveSectorTopologyMapFromAuthoringGraph(authoringGraph);
    int relocatedAnchorCount = 0;
    int repairFailureAnchorId = -1;
    std::string repairFailureReason;
    SectorAuthoringGraph repairedGraph;
    SectorAuthoringDerivationResult repairedResult;
    bool repairFailureAddedAsDiagnostic = false;
    const FaceAnchorAutoFollowOutcome repairOutcome = TryAutoFollowFaceAnchors(
                authoringGraph,
                result,
                effectiveBindings,
                repairedGraph,
                repairedResult,
                relocatedAnchorCount,
                repairFailureAnchorId,
                repairFailureReason);
    if (repairOutcome == FaceAnchorAutoFollowOutcome::Repaired) {
        authoringGraph = std::move(repairedGraph);
        result = std::move(repairedResult);
    } else if (repairOutcome == FaceAnchorAutoFollowOutcome::Failed
            && result.success) {
        SectorAuthoringDerivationDiagnostic diagnostic;
        diagnostic.severity = SectorAuthoringValidationSeverity::Error;
        diagnostic.kind =
                SectorAuthoringDerivationDiagnosticKind::UnresolvedFaceAnchor;
        diagnostic.objectId = repairFailureAnchorId;
        diagnostic.message = "Face-anchor auto-follow failed: "
                + repairFailureReason;
        result.diagnostics.push_back(std::move(diagnostic));
        result.success = false;
        result.topology = SectorTopologyMap{};
        result.mapping = SectorAuthoringDerivationMapping{};
        repairFailureAddedAsDiagnostic = true;
    }
    if (result.success) {
        int reconciledAnchorCount = 0;
        bool reconciliationFailed = false;
        if (ReconcileMissingDerivedFaceAnchors(
                    authoringGraph,
                    result,
                    &reconciledAnchorCount,
                    &reconciliationFailed)) {
            MarkSectorEditorAuthoringGraphEdited(
                    lifecycle,
                    topologyRenderRevision,
                    topologyRenderCache,
                    derivation,
                    TextFormat("Added %d generated authoring face anchor%s",
                            reconciledAnchorCount,
                            reconciledAnchorCount == 1 ? "" : "s"));
            result = DeriveSectorTopologyMapFromAuthoringGraph(authoringGraph);
            if (!result.success
                    || !AllDerivedSectorsHaveUniqueFaceAnchorMappings(authoringGraph, result)) {
                derivation.authoringDerivation = std::move(result);
                derivation.authoringDerivedTopologyStale = true;
                derivation.authoringDerivationState = derivation.lastValidAuthoringDerivedTopology.has_value()
                        ? SectorEditorAuthoringDerivationState::InvalidLastValid
                        : SectorEditorAuthoringDerivationState::InvalidNoDerived;
                derivation.authoringDerivationStatus =
                        "Authoring graph: generated face-anchor reconciliation failed";
                lifecycle.topologyDocumentStatus = derivation.authoringDerivationStatus;
                InvalidateEditorTopologyRenderCacheIfNeeded(
                        topologyRenderRevision,
                        topologyRenderCache);
                return false;
            }
        } else if (reconciliationFailed) {
            derivation.authoringDerivation = std::move(result);
            derivation.authoringDerivedTopologyStale = true;
            derivation.authoringDerivationState = derivation.lastValidAuthoringDerivedTopology.has_value()
                    ? SectorEditorAuthoringDerivationState::InvalidLastValid
                    : SectorEditorAuthoringDerivationState::InvalidNoDerived;
            derivation.authoringDerivationStatus =
                    "Authoring graph: generated face-anchor reconciliation failed";
            lifecycle.topologyDocumentStatus = derivation.authoringDerivationStatus;
            InvalidateEditorTopologyRenderCacheIfNeeded(
                    topologyRenderRevision,
                    topologyRenderCache);
            return false;
        }
        SectorTopologyMap candidateMapData = topologyMap;
        std::vector<int> removedDoorIds;
        std::string doorError;
        if (!ReconcileSectorEditorAuthoringCandidateDoors(
                    topologyMap,
                    derivation.authoringDerivation,
                    result,
                    {},
                    {},
                    candidateMapData,
                    removedDoorIds,
                    doorError)) {
            derivation.authoringDerivation = std::move(result);
            derivation.authoringDerivedTopologyStale = true;
            derivation.authoringDerivationState =
                    derivation.lastValidAuthoringDerivedTopology.has_value()
                    ? SectorEditorAuthoringDerivationState::InvalidLastValid
                    : SectorEditorAuthoringDerivationState::InvalidNoDerived;
            derivation.authoringDerivationStatus = doorError.empty()
                    ? "Authoring graph: door reconciliation failed"
                    : doorError;
            lifecycle.topologyDocumentStatus = derivation.authoringDerivationStatus;
            InvalidateEditorTopologyRenderCacheIfNeeded(
                    topologyRenderRevision,
                    topologyRenderCache);
            return false;
        }

        CopyEditorMapLevelFields(result.topology, candidateMapData);
        topologyMap = result.topology;
        derivation.lastValidAuthoringDerivedTopology = result.topology;
        derivation.authoringDerivation = std::move(result);
        derivation.lastValidFaceAnchorBindings =
                BuildFaceAnchorBindings(derivation.authoringDerivation);
        derivation.authoringDerivedTopologyStale = false;
        derivation.authoringDerivationState = SectorEditorAuthoringDerivationState::ValidCurrent;
        derivation.authoringDerivationStatus = ownedSuccessStatus.empty()
                ? "Authoring graph: derived topology current"
                : ownedSuccessStatus;
        if (relocatedAnchorCount > 0) {
            derivation.authoringDerivationStatus += TextFormat(
                    "; auto-followed %d face anchor%s",
                    relocatedAnchorCount,
                    relocatedAnchorCount == 1 ? "" : "s");
        }
        lifecycle.topologyDocumentStatus = derivation.authoringDerivationStatus;
        InvalidateEditorTopologyRenderCacheIfNeeded(
                topologyRenderRevision,
                topologyRenderCache);
        return true;
    }

    derivation.authoringDerivation = std::move(result);
    derivation.authoringDerivedTopologyStale = true;
    derivation.authoringDerivationState = derivation.lastValidAuthoringDerivedTopology.has_value()
            ? SectorEditorAuthoringDerivationState::InvalidLastValid
            : SectorEditorAuthoringDerivationState::InvalidNoDerived;
    derivation.authoringDerivationStatus =
            BuildAuthoringDerivationFailureStatus(
                    ownedFailureStatus,
                    derivation.authoringDerivation);
    if (repairOutcome == FaceAnchorAutoFollowOutcome::Failed
            && !repairFailureAddedAsDiagnostic
            && !repairFailureReason.empty()) {
        derivation.authoringDerivationStatus += "; ";
        if (IsValidSectorAuthoringId(repairFailureAnchorId)) {
            derivation.authoringDerivationStatus +=
                    "Face anchor " + std::to_string(repairFailureAnchorId) + " ";
        }
        derivation.authoringDerivationStatus +=
                "auto-follow failed: " + repairFailureReason;
    }
    lifecycle.topologyDocumentStatus = derivation.authoringDerivationStatus;
    InvalidateEditorTopologyRenderCacheIfNeeded(
            topologyRenderRevision,
            topologyRenderCache);
    return false;
}

bool CanUseCurrentAuthoringDerivedTopologyForPreview(
        SectorEditorConstDerivationDocumentAccess derivation,
        std::string* outMessage)
{
    return CanUseCurrentAuthoringDerivedTopology(
            derivation.authoringDerivationState,
            derivation.authoringDerivedTopologyStale,
            derivation.authoringDerivation,
            "3D preview",
            outMessage);
}

bool CanUseCurrentAuthoringDerivedTopologyForLightmapBake(
        SectorEditorConstDerivationDocumentAccess derivation,
        std::string* outMessage)
{
    return CanUseCurrentAuthoringDerivedTopology(
            derivation.authoringDerivationState,
            derivation.authoringDerivedTopologyStale,
            derivation.authoringDerivation,
            "Lightmap bake",
            outMessage);
}

} // namespace game
