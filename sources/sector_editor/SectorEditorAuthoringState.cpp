#include "sector_editor/SectorEditorAuthoringState.h"

#include "sector_demo/SectorTopologyGeometry.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace game {

bool HasAuthoringGraphData(const SectorEditorState& state)
{
    return HasAuthoringGraphData(state.authoringGraph);
}

bool HasAuthoringGraphData(const SectorAuthoringGraph& graph)
{
    return !graph.vertices.empty()
            || !graph.lines.empty()
            || !graph.lineSides.empty()
            || !graph.faceAnchors.empty();
}

namespace {

struct SectorEditorAuthoringSegmentInsertResult {
    std::vector<SectorEditorAuthoringLineSegmentResult> segments;
    std::string errorMessage;
};

void CopyEditorMapLevelFields(SectorTopologyMap& target, const SectorTopologyMap& source)
{
    target.texturesById = source.texturesById;
    target.staticLights = source.staticLights;
    target.staticSpotLights = source.staticSpotLights;
    target.dynamicPointLights = source.dynamicPointLights;
    target.dynamicSpotLights = source.dynamicSpotLights;
    target.runtimeObjects = source.runtimeObjects;
    target.previewSettings = source.previewSettings;
    target.skySettings = source.skySettings;
    target.directionalLight = source.directionalLight;
    target.lightmapSettings = source.lightmapSettings;
    target.bakedLightmap = source.bakedLightmap;
}

void InvalidateEditorTopologyRenderCache(SectorEditorState& state)
{
    state.topologyRenderCache.valid = false;
    ++state.topologyRenderRevision;
}

void InvalidateEditorTopologyRenderCacheIfNeeded(SectorEditorState& state)
{
    if (state.topologyRenderCache.valid) {
        InvalidateEditorTopologyRenderCache(state);
        return;
    }
    state.topologyRenderCache.valid = false;
}

SectorAuthoringSelectionTarget EmptyAuthoringSelectionTarget()
{
    return SectorAuthoringSelectionTarget{};
}

bool CanUseCurrentAuthoringDerivedTopology(
        const SectorEditorState& state,
        const char* action,
        std::string* outMessage)
{
    const auto setMessage = [outMessage, action](const char* reason) {
        if (outMessage != nullptr) {
            *outMessage = TextFormat("%s requires current valid derived topology: %s", action, reason);
        }
    };

    if (state.authoringDerivationState == SectorEditorAuthoringDerivationState::ValidCurrent
            && !state.authoringDerivedTopologyStale
            && state.authoringDerivation.success) {
        if (outMessage != nullptr) {
            outMessage->clear();
        }
        return true;
    }

    if (state.authoringDerivationState == SectorEditorAuthoringDerivationState::InvalidLastValid) {
        setMessage("latest derivation failed; fix authoring diagnostics first");
        return false;
    }

    if (state.authoringDerivationState == SectorEditorAuthoringDerivationState::InvalidNoDerived) {
        setMessage("no valid derived topology is available");
        return false;
    }

    if (state.authoringDerivationState == SectorEditorAuthoringDerivationState::ValidStale
            || state.authoringDerivedTopologyStale) {
        setMessage("authoring graph changed; re-derive before using runtime topology");
        return false;
    }

    setMessage("no valid derived topology is available");
    return false;
}

bool CurrentAuthoringDerivationAvailable(const SectorEditorState& state)
{
    return state.authoringDerivationState == SectorEditorAuthoringDerivationState::ValidCurrent
            && !state.authoringDerivedTopologyStale
            && state.authoringDerivation.success;
}

SectorEditorInspectorTarget UnavailableInspectorTarget(const char* status)
{
    SectorEditorInspectorTarget target;
    target.kind = SectorEditorInspectorTargetKind::AuthoringUnavailable;
    target.status = status == nullptr ? "" : status;
    return target;
}

SectorEditorInspectorTarget ResolveMappedTopologySectorInspectorTarget(
        const SectorEditorState& state,
        int topologySectorId)
{
    if (!CurrentAuthoringDerivationAvailable(state)) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: derived topology is not current");
    }
    if (FindSectorTopologySector(state.topologyMap, topologySectorId) == nullptr) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: selected sector is not current");
    }

    bool found = false;
    int faceAnchorId = -1;
    for (const SectorAuthoringDerivedSectorMapping& mapping : state.authoringDerivation.mapping.sectors) {
        if (mapping.topologySectorId != topologySectorId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.faceAnchorId)
                || FindSectorAuthoringFaceAnchor(state.authoringGraph, mapping.faceAnchorId) == nullptr) {
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
        const SectorEditorState& state,
        int topologySideDefId)
{
    if (!CurrentAuthoringDerivationAvailable(state)) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: derived topology is not current");
    }
    if (FindSectorTopologySideDef(state.topologyMap, topologySideDefId) == nullptr) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: selected sidedef is not current");
    }

    bool found = false;
    SectorAuthoringSideId sideId;
    for (const SectorAuthoringDerivedSideMapping& mapping : state.authoringDerivation.mapping.sides) {
        if (mapping.topologySideDefId != topologySideDefId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.authoringLineId)
                || FindSectorAuthoringLine(state.authoringGraph, mapping.authoringLineId) == nullptr) {
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
        const SectorEditorState& state,
        int topologyLineDefId)
{
    if (!CurrentAuthoringDerivationAvailable(state)) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: derived topology is not current");
    }
    if (FindSectorTopologyLineDef(state.topologyMap, topologyLineDefId) == nullptr) {
        return UnavailableInspectorTarget("Authoring inspector unavailable: selected linedef is not current");
    }

    bool found = false;
    int authoringLineId = -1;
    for (const SectorAuthoringDerivedLineMapping& mapping : state.authoringDerivation.mapping.lines) {
        if (mapping.topologyLineDefId != topologyLineDefId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.authoringLineId)
                || FindSectorAuthoringLine(state.authoringGraph, mapping.authoringLineId) == nullptr) {
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

bool FindUniqueMappedFaceAnchorForTopologySector(
        const SectorEditorState& state,
        int topologySectorId,
        int* outFaceAnchorId,
        std::string* outStatus)
{
    if (outFaceAnchorId != nullptr) {
        *outFaceAnchorId = -1;
    }

    int faceAnchorId = -1;
    int anchorMatchCount = 0;
    for (const SectorAuthoringDerivedSectorMapping& mapping
            : state.authoringDerivation.mapping.sectors) {
        if (mapping.topologySectorId != topologySectorId
                || !IsValidSectorAuthoringId(mapping.faceAnchorId)
                || FindSectorAuthoringFaceAnchor(state.authoringGraph, mapping.faceAnchorId) == nullptr) {
            continue;
        }
        faceAnchorId = mapping.faceAnchorId;
        ++anchorMatchCount;
    }

    if (anchorMatchCount == 0) {
        if (outStatus != nullptr) {
            *outStatus = "Authoring face selection unavailable: derived face has no face anchor mapping";
        }
        return false;
    }
    if (anchorMatchCount > 1) {
        if (outStatus != nullptr) {
            *outStatus = "Authoring face selection unavailable: derived face has ambiguous face anchor mapping";
        }
        return false;
    }

    if (outFaceAnchorId != nullptr) {
        *outFaceAnchorId = faceAnchorId;
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

    const auto tryPoint = [&](SectorCoord x, SectorCoord y, SectorTopologyCoordPoint* point) {
        const Vector2 mapPoint{
                SectorCoordToVisibleAuthoring(x),
                SectorCoordToVisibleAuthoring(y)};
        if (!PointStrictlyInTopologySector(map, indexes, mapPoint, topologySectorId)) {
            return false;
        }
        if (point != nullptr) {
            point->x = x;
            point->y = y;
        }
        return true;
    };

    constexpr int divisions = 32;
    for (int yIndex = 1; yIndex < divisions; ++yIndex) {
        const SectorCoord y = static_cast<SectorCoord>(
                static_cast<int64_t>(minY)
                + (static_cast<int64_t>(maxY) - static_cast<int64_t>(minY)) * yIndex / divisions);
        for (int xOffset = 0; xOffset < divisions - 1; ++xOffset) {
            const int xIndex = ((yIndex * 13 + xOffset * 7) % (divisions - 1)) + 1;
            const SectorCoord x = static_cast<SectorCoord>(
                    static_cast<int64_t>(minX)
                    + (static_cast<int64_t>(maxX) - static_cast<int64_t>(minX)) * xIndex / divisions);
            if (tryPoint(x, y, outPoint)) {
                return true;
            }
        }
    }

    return false;
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

void CopyTopologySectorDefaultsToFaceAnchor(
        const SectorTopologySector& sector,
        SectorAuthoringFaceAnchor& anchor)
{
    anchor.floorZ = sector.floorZ;
    anchor.ceilingZ = sector.ceilingZ;
    anchor.floorTextureId = sector.floorTextureId;
    anchor.ceilingTextureId = sector.ceilingTextureId;
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
        SectorEditorState& state,
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
    if (!BuildExistingFaceAnchorClaims(state.authoringGraph, result, indexes, claimedAnchorIdBySectorId)) {
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
        anchor.id = AllocateSectorAuthoringFaceAnchorId(state.authoringGraph);
        if (!IsValidSectorAuthoringId(anchor.id)) {
            continue;
        }
        anchor.name = AllocateGeneratedFaceAnchorName(state.authoringGraph);
        anchor.x = anchorPoint.x;
        anchor.y = anchorPoint.y;
        CopyTopologySectorDefaultsToFaceAnchor(sector, anchor);
        state.authoringGraph.faceAnchors.push_back(std::move(anchor));
        ++addedCount;
    }

    if (outAddedCount != nullptr) {
        *outAddedCount = addedCount;
    }
    return addedCount > 0;
}

} // namespace

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

bool SectorAuthoringSelectionTargetsEqual(
        SectorAuthoringSelectionTarget lhs,
        SectorAuthoringSelectionTarget rhs)
{
    return lhs.kind == rhs.kind
            && lhs.lineId == rhs.lineId
            && lhs.vertexId == rhs.vertexId
            && lhs.faceAnchorId == rhs.faceAnchorId;
}

bool IsSectorAuthoringSelectionTargetValid(
        const SectorAuthoringGraph& graph,
        SectorAuthoringSelectionTarget target)
{
    switch (target.kind) {
    case SectorAuthoringSelectionKind::None:
        return target.lineId == -1 && target.vertexId == -1 && target.faceAnchorId == -1;
    case SectorAuthoringSelectionKind::Line:
        return target.vertexId == -1
                && target.faceAnchorId == -1
                && FindSectorAuthoringLine(graph, target.lineId) != nullptr;
    case SectorAuthoringSelectionKind::Vertex:
        return target.lineId == -1
                && target.faceAnchorId == -1
                && FindSectorAuthoringVertex(graph, target.vertexId) != nullptr;
    case SectorAuthoringSelectionKind::FaceAnchor:
        return target.lineId == -1
                && target.vertexId == -1
                && FindSectorAuthoringFaceAnchor(graph, target.faceAnchorId) != nullptr;
    }
    return false;
}

void ClearSectorEditorAuthoringSelection(SelectionState& selectionState)
{
    selectionState.selectedAuthoring = EmptyAuthoringSelectionTarget();
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

void PruneSectorEditorAuthoringSelectionToGraph(SectorEditorState& state, SelectionState& selectionState)
{
    if (!IsSectorAuthoringSelectionTargetValid(state.authoringGraph, selectionState.selectedAuthoring)) {
        ClearSectorEditorAuthoringSelection(selectionState);
    }
    if (!IsSectorAuthoringSelectionTargetValid(state.authoringGraph, selectionState.hoveredAuthoring)) {
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
        const SectorEditorState& state,
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

    std::string message;
    if (!CanUseCurrentAuthoringDerivedTopology(state, "Authoring face selection", &message)) {
        if (outStatus != nullptr) {
            *outStatus = message;
        }
        return false;
    }

    int bestFaceAnchorId = -1;
    double bestArea = 0.0;
    bool foundCandidate = false;
    bool foundContainingFace = false;
    for (const SectorAuthoringExtractedFace& face : state.authoringDerivation.faces.faces) {
        if (!SectorAuthoringFaceContainsMapPoint(state.authoringDerivation.planar, face, mapPoint)) {
            continue;
        }
        foundContainingFace = true;

        int faceAnchorId = -1;
        bool foundMapping = false;
        for (const SectorAuthoringResolvedFaceMapping& mapping
                : state.authoringDerivation.mapping.resolvedFaces) {
            if (mapping.extractedFaceId != face.id
                    || !IsValidSectorAuthoringId(mapping.faceAnchorId)
                    || FindSectorAuthoringFaceAnchor(state.authoringGraph, mapping.faceAnchorId) == nullptr) {
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
        const SectorEditorState& state,
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
                state.authoringGraph,
                mapPoint,
                vertexMaxDistance,
                lineMaxDistance,
                outTarget,
                outVertexPoint)) {
        return true;
    }

    int faceAnchorId = -1;
    if (FindSectorEditorAuthoringFaceAnchorAtMapPoint(
                state,
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
        SelectionState& selectionState,
        SectorTopologyCoordPoint start,
        SectorTopologyCoordPoint end,
        int* outLineId,
        SectorEditorAuthoringLineSegmentResult* outResult)
{
    SectorAuthoringGraph candidate = state.authoringGraph;
    SectorEditorAuthoringSegmentInsertResult insertResult;
    if (!InsertSectorEditorAuthoringLineSegmentIntoGraphWithSplits(
                candidate,
                start,
                end,
                &insertResult)) {
        return false;
    }

    state.authoringGraph = std::move(candidate);
    PruneSectorEditorAuthoringSelectionToGraph(state, selectionState);

    const SectorEditorAuthoringLineSegmentResult segment = insertResult.segments.back();
    if (outLineId != nullptr) {
        *outLineId = segment.lineId;
    }
    if (outResult != nullptr) {
        *outResult = segment;
    }
    MarkSectorEditorAuthoringGraphEdited(
            state,
            TextFormat("Added authoring line %d", segment.lineId));
    RefreshSectorEditorAuthoringDerivation(
            state,
            TextFormat("Added authoring line %d; derived topology current", segment.lineId),
            TextFormat("Added authoring line %d; derivation failed", segment.lineId));
    return true;
}

SectorEditorAuthoringLineToolClickResult ClickSectorEditorAuthoringLineTool(
        SectorEditorState& state,
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
                state.authoringGraph,
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
                selectionState,
                state.pendingAuthoringLine.startPoint,
                point,
                &lineId,
                &segment)) {
        state.pendingAuthoringLine.errorMessage = "Authoring line segment rejected";
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
        SectorTopologyCoordPoint firstCorner,
        SectorTopologyCoordPoint oppositeCorner,
        SectorEditorAuthoringRectangleResult* outResult)
{
    SectorEditorAuthoringRectangleResult result;
    if (!CreateSectorAuthoringRectangle(
                state.authoringGraph,
                firstCorner,
                oppositeCorner,
                &result)) {
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    MarkSectorEditorAuthoringGraphEdited(state, "Created authoring rectangle");
    RefreshSectorEditorAuthoringDerivation(
            state,
            "Created authoring rectangle; derived topology current",
            "Created authoring rectangle; derivation failed");
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

bool InsertSectorEditorAuthoringVertexOnLine(
        SectorEditorState& state,
        SelectionState& selectionState,
        int lineId,
        SectorTopologyCoordPoint point,
        SectorAuthoringInsertVertexResult* outResult)
{
    SectorAuthoringInsertVertexResult result;
    if (!InsertSectorAuthoringVertexOnLine(state.authoringGraph, lineId, point, &result)) {
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    PruneSectorEditorAuthoringSelectionToGraph(state, selectionState);
    SelectSectorEditorAuthoringVertex(state.authoringGraph, selectionState, result.vertexId);
    MarkSectorEditorAuthoringGraphEdited(state, "Inserted vertex on authoring line");
    RefreshSectorEditorAuthoringDerivation(
            state,
            "Inserted vertex on authoring line; derived topology current",
            "Inserted vertex on authoring line; derivation failed");
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

bool MoveSectorEditorAuthoringVertex(
        SectorEditorState& state,
        SelectionState& selectionState,
        int vertexId,
        SectorTopologyCoordPoint target)
{
    SectorAuthoringVertex* vertex = FindSectorAuthoringVertex(state.authoringGraph, vertexId);
    if (vertex == nullptr) {
        return false;
    }
    if (vertex->x == target.x && vertex->y == target.y) {
        return false;
    }

    vertex->x = target.x;
    vertex->y = target.y;
    PruneSectorEditorAuthoringSelectionToGraph(state, selectionState);
    MarkSectorEditorAuthoringGraphEdited(
            state,
            TextFormat("Moved authoring vertex %d", vertexId));
    RefreshSectorEditorAuthoringDerivation(
            state,
            TextFormat("Moved authoring vertex %d; derived topology current", vertexId),
            TextFormat("Moved authoring vertex %d; derivation failed", vertexId));
    return true;
}

bool DeleteSectorEditorSelectedAuthoringLine(SectorEditorState& state, SelectionState& selectionState)
{
    if (selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::Line
            || !IsSectorAuthoringSelectionTargetValid(
                    state.authoringGraph,
                    selectionState.selectedAuthoring)) {
        return false;
    }

    const int lineId = selectionState.selectedAuthoring.lineId;
    state.authoringGraph.lines.erase(
            std::remove_if(
                    state.authoringGraph.lines.begin(),
                    state.authoringGraph.lines.end(),
                    [lineId](const SectorAuthoringLine& line) {
                        return line.id == lineId;
                    }),
            state.authoringGraph.lines.end());
    state.authoringGraph.lineSides.erase(
            std::remove_if(
                    state.authoringGraph.lineSides.begin(),
                    state.authoringGraph.lineSides.end(),
                    [lineId](const SectorAuthoringLineSide& side) {
                        return side.id.lineId == lineId;
                    }),
            state.authoringGraph.lineSides.end());
    PruneSectorEditorAuthoringSelectionToGraph(state, selectionState);
    MarkSectorEditorAuthoringGraphEdited(
            state,
            TextFormat("Deleted authoring line %d", lineId));
    RefreshSectorEditorAuthoringDerivation(
            state,
            TextFormat("Deleted authoring line %d; derived topology current", lineId),
            TextFormat("Deleted authoring line %d; derivation failed", lineId));
    return true;
}

bool DeleteSectorEditorSelectedAuthoringVertex(SectorEditorState& state, SelectionState& selectionState)
{
    if (selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::Vertex
            || !IsSectorAuthoringSelectionTargetValid(
                    state.authoringGraph,
                    selectionState.selectedAuthoring)) {
        return false;
    }

    const int vertexId = selectionState.selectedAuthoring.vertexId;
    for (const SectorAuthoringLine& line : state.authoringGraph.lines) {
        if (line.startVertexId == vertexId || line.endVertexId == vertexId) {
            state.authoringDerivationStatus =
                    "Authoring vertex is connected; delete its lines first.";
            state.topologyDocumentStatus = state.authoringDerivationStatus;
            return false;
        }
    }

    state.authoringGraph.vertices.erase(
            std::remove_if(
                    state.authoringGraph.vertices.begin(),
                    state.authoringGraph.vertices.end(),
                    [vertexId](const SectorAuthoringVertex& vertex) {
                        return vertex.id == vertexId;
                    }),
            state.authoringGraph.vertices.end());
    PruneSectorEditorAuthoringSelectionToGraph(state, selectionState);
    MarkSectorEditorAuthoringGraphEdited(
            state,
            TextFormat("Deleted authoring vertex %d", vertexId));
    RefreshSectorEditorAuthoringDerivation(
            state,
            TextFormat("Deleted authoring vertex %d; derived topology current", vertexId),
            TextFormat("Deleted authoring vertex %d; derivation failed", vertexId));
    return true;
}

void InitializeSectorEditorAuthoringStateFromTopology(
        SectorEditorState& state,
        const SectorTopologyMap& sourceMap)
{
    state.authoringGraph = ImportSectorTopologyMapToAuthoringGraph(sourceMap);
    state.authoringDerivation = DeriveSectorTopologyMapFromAuthoringGraph(state.authoringGraph);
    if (state.authoringDerivation.success) {
        CopyEditorMapLevelFields(state.authoringDerivation.topology, sourceMap);
        state.lastValidAuthoringDerivedTopology = sourceMap;
        state.authoringDerivationState = SectorEditorAuthoringDerivationState::ValidCurrent;
        state.authoringDerivedTopologyStale = false;
        state.authoringDerivationStatus = "Authoring graph: derived topology current";
    } else {
        state.lastValidAuthoringDerivedTopology.reset();
        state.authoringDerivationState = SectorEditorAuthoringDerivationState::InvalidNoDerived;
        state.authoringDerivedTopologyStale = true;
        state.authoringDerivationStatus = "Authoring graph: no valid derived topology";
    }
}

void MarkSectorEditorAuthoringGraphEdited(
        SectorEditorState& state,
        const char* status)
{
    state.topologyDocumentDirty = true;
    state.hasUnsavedChanges = true;
    state.authoringDerivedTopologyStale = true;
    state.authoringDerivationState = state.lastValidAuthoringDerivedTopology.has_value()
            ? SectorEditorAuthoringDerivationState::ValidStale
            : SectorEditorAuthoringDerivationState::InvalidNoDerived;
    state.authoringDerivationStatus = status == nullptr || status[0] == '\0'
            ? "Authoring graph edited; derived topology stale"
            : status;
    state.topologyDocumentStatus = state.authoringDerivationStatus;
    InvalidateEditorTopologyRenderCache(state);
}

int FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
        const SectorEditorState& state,
        int topologySectorId)
{
    if (!IsValidSectorTopologyId(topologySectorId)) {
        return -1;
    }

    for (const SectorAuthoringDerivedSectorMapping& mapping
            : state.authoringDerivation.mapping.sectors) {
        if (mapping.topologySectorId == topologySectorId
                && IsValidSectorAuthoringId(mapping.faceAnchorId)
                && FindSectorAuthoringFaceAnchor(
                        state.authoringGraph,
                        mapping.faceAnchorId) != nullptr) {
            return mapping.faceAnchorId;
        }
    }
    return -1;
}

bool FindSectorEditorAuthoringSideIdForTopologySideDef(
        const SectorEditorState& state,
        int topologySideDefId,
        SectorAuthoringSideId& outSideId)
{
    outSideId = SectorAuthoringSideId{};
    if (!IsValidSectorTopologyId(topologySideDefId)) {
        return false;
    }

    for (const SectorAuthoringDerivedSideMapping& mapping
            : state.authoringDerivation.mapping.sides) {
        if (mapping.topologySideDefId == topologySideDefId
                && IsValidSectorAuthoringId(mapping.authoringLineId)
                && FindSectorAuthoringLine(
                        state.authoringGraph,
                        mapping.authoringLineId) != nullptr) {
            outSideId = SectorAuthoringSideId{mapping.authoringLineId, mapping.authoringSide};
            return true;
        }
    }
    return false;
}

int FindSectorEditorAuthoringLineIdForTopologyLineDef(
        const SectorEditorState& state,
        int topologyLineDefId)
{
    if (!IsValidSectorTopologyId(topologyLineDefId)) {
        return -1;
    }

    for (const SectorAuthoringDerivedLineMapping& mapping
            : state.authoringDerivation.mapping.lines) {
        if (mapping.topologyLineDefId == topologyLineDefId
                && IsValidSectorAuthoringId(mapping.authoringLineId)
                && FindSectorAuthoringLine(
                        state.authoringGraph,
                        mapping.authoringLineId) != nullptr) {
            return mapping.authoringLineId;
        }
    }
    return -1;
}

SectorEditorInspectorTarget ResolveSectorEditorInspectorTarget(
        const SectorEditorState& state,
        const SelectionState& selectionState)
{
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line
            && FindSectorAuthoringLine(state.authoringGraph, selectionState.selectedAuthoring.lineId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringLine;
        target.lineId = selectionState.selectedAuthoring.lineId;
        return target;
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor
            && FindSectorAuthoringFaceAnchor(state.authoringGraph, selectionState.selectedAuthoring.faceAnchorId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringFaceAnchor;
        target.faceAnchorId = selectionState.selectedAuthoring.faceAnchorId;
        return target;
    }
    if (selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex
            && FindSectorAuthoringVertex(state.authoringGraph, selectionState.selectedAuthoring.vertexId) != nullptr) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::AuthoringVertex;
        target.vertexId = selectionState.selectedAuthoring.vertexId;
        return target;
    }

    if (!HasAuthoringGraphData(state)) {
        SectorEditorInspectorTarget target;
        target.kind = SectorEditorInspectorTargetKind::LegacyTopology;
        return target;
    }

    if (selectionState.topologySelectionKind == TopologySelectionKind::Sector
            && IsValidSectorTopologyId(selectionState.selectedTopologySectorId)) {
        return ResolveMappedTopologySectorInspectorTarget(state, selectionState.selectedTopologySectorId);
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::SideDef
            && IsValidSectorTopologyId(selectionState.selectedTopologySideDefId)) {
        return ResolveMappedTopologySideInspectorTarget(state, selectionState.selectedTopologySideDefId);
    }
    if (selectionState.topologySelectionKind == TopologySelectionKind::LineDef
            && IsValidSectorTopologyId(selectionState.selectedTopologyLineDefId)) {
        return ResolveMappedTopologyLineInspectorTarget(state, selectionState.selectedTopologyLineDefId);
    }

    return SectorEditorInspectorTarget{};
}

std::string BuildSectorEditorSurface3DTargetLabel(
        const SectorEditorState& state,
        SectorSurfaceRef surface,
        TopologySurfaceEditTarget target)
{
    SectorEditorAuthoringSurfaceTarget authoringTarget;
    std::string status;
    const bool mapped = HasAuthoringGraphData(state)
            && ResolveSectorEditorAuthoringSurfaceTarget(state, surface, authoringTarget, &status);

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
        const SectorEditorState& state,
        SectorSurfaceRef surface,
        SectorEditorAuthoringSurfaceTarget& outTarget,
        std::string* outStatus)
{
    const bool authoringDerivationCurrent =
            state.authoringDerivationState == SectorEditorAuthoringDerivationState::ValidCurrent
            && !state.authoringDerivedTopologyStale
            && state.authoringDerivation.success;
    return ResolveSectorEditorAuthoringSurfaceTarget(
            state.topologyMap,
            state.authoringGraph,
            state.authoringDerivation,
            authoringDerivationCurrent,
            surface,
            outTarget,
            outStatus);
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
        SectorEditorState& state,
        std::string* outStatus)
{
    if (outStatus != nullptr) {
        outStatus->clear();
    }

    if (!HasAuthoringGraphData(state) || state.selectedSurface3D.kind == SectorSurfaceKind::None) {
        return true;
    }

    SectorEditorAuthoringSurfaceTarget authoringTarget;
    std::string status;
    if (ResolveSectorEditorAuthoringSurfaceTarget(
                state,
                state.selectedSurface3D,
                authoringTarget,
                &status)) {
        return true;
    }

    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    if (outStatus != nullptr) {
        *outStatus = status;
    }
    return false;
}

bool MutateSectorEditorAuthoringFaceAnchorForTopologySector(
        SectorEditorState& state,
        int topologySectorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate)
{
    if (!mutate) {
        return false;
    }

    const int faceAnchorId =
            FindSectorEditorAuthoringFaceAnchorIdForTopologySector(state, topologySectorId);
    return MutateSectorEditorAuthoringFaceAnchorById(state, faceAnchorId, status, mutate);
}

bool MutateSectorEditorAuthoringFaceAnchorById(
        SectorEditorState& state,
        int faceAnchorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate)
{
    if (!mutate) {
        return false;
    }

    SectorAuthoringFaceAnchor* anchor =
            FindSectorAuthoringFaceAnchor(state.authoringGraph, faceAnchorId);
    if (anchor == nullptr) {
        return false;
    }

    if (!mutate(*anchor)) {
        return false;
    }

    MarkSectorEditorAuthoringGraphEdited(state, status);
    return RefreshSectorEditorAuthoringDerivation(
            state,
            status,
            "Updated authoring face anchor; derivation failed");
}

bool MutateSectorEditorAuthoringSideForTopologySideDef(
        SectorEditorState& state,
        int topologySideDefId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate)
{
    if (!mutate) {
        return false;
    }

    SectorAuthoringSideId sideId;
    if (!FindSectorEditorAuthoringSideIdForTopologySideDef(
                state,
                topologySideDefId,
                sideId)) {
        return false;
    }

    SectorAuthoringLineSide* side = FindSectorAuthoringLineSide(state.authoringGraph, sideId);
    if (side == nullptr) {
        const SectorTopologySideDef* topologySide =
                FindSectorTopologySideDef(state.topologyMap, topologySideDefId);
        SectorAuthoringLineSide newSide;
        newSide.id = sideId;
        if (topologySide != nullptr) {
            newSide.wall = topologySide->wall;
            newSide.lower = topologySide->lower;
            newSide.upper = topologySide->upper;
            newSide.middle = topologySide->middle;
        }
        state.authoringGraph.lineSides.push_back(newSide);
        side = &state.authoringGraph.lineSides.back();
    }

    if (!mutate(*side)) {
        return false;
    }

    MarkSectorEditorAuthoringGraphEdited(state, status);
    return RefreshSectorEditorAuthoringDerivation(
            state,
            status,
            "Updated authoring side material; derivation failed");
}

bool MutateSectorEditorAuthoringSideById(
        SectorEditorState& state,
        SectorAuthoringSideId sideId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate)
{
    if (!mutate
            || !IsValidSectorAuthoringId(sideId.lineId)
            || FindSectorAuthoringLine(state.authoringGraph, sideId.lineId) == nullptr) {
        return false;
    }

    SectorAuthoringLineSide* side = FindSectorAuthoringLineSide(state.authoringGraph, sideId);
    if (side == nullptr) {
        SectorAuthoringLineSide newSide;
        newSide.id = sideId;
        state.authoringGraph.lineSides.push_back(newSide);
        side = &state.authoringGraph.lineSides.back();
    }

    if (!mutate(*side)) {
        return false;
    }

    MarkSectorEditorAuthoringGraphEdited(state, status);
    return RefreshSectorEditorAuthoringDerivation(
            state,
            status,
            "Updated authoring side material; derivation failed");
}

bool MutateSectorEditorAuthoringLineForTopologyLineDef(
        SectorEditorState& state,
        int topologyLineDefId,
        const char* status,
        const std::function<bool(SectorAuthoringLine&)>& mutate)
{
    if (!mutate) {
        return false;
    }

    const int authoringLineId =
            FindSectorEditorAuthoringLineIdForTopologyLineDef(state, topologyLineDefId);
    return MutateSectorEditorAuthoringLineById(state, authoringLineId, status, mutate);
}

bool MutateSectorEditorAuthoringLineById(
        SectorEditorState& state,
        int lineId,
        const char* status,
        const std::function<bool(SectorAuthoringLine&)>& mutate)
{
    if (!mutate) {
        return false;
    }

    SectorAuthoringLine* line =
            FindSectorAuthoringLine(state.authoringGraph, lineId);
    if (line == nullptr) {
        return false;
    }

    if (!mutate(*line)) {
        return false;
    }

    MarkSectorEditorAuthoringGraphEdited(state, status);
    return RefreshSectorEditorAuthoringDerivation(
            state,
            status,
            "Updated authoring line flags; derivation failed");
}

bool SetSectorEditorAuthoringLineDefBlocksPlayer(
        SectorEditorState& state,
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
            state.topologyMap,
            topologyLineDefId);
    if (lineDef == nullptr) {
        return fail("Selected linedef is no longer valid.");
    }
    if (lineDef->frontSideDefId == -1 || lineDef->backSideDefId == -1) {
        return fail("Blocks Player is only editable on two-sided portals.");
    }
    if (!HasAuthoringGraphData(state)) {
        return fail("Cannot edit line flag: authoring data is required.");
    }
    if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
            || state.authoringDerivedTopologyStale
            || !state.authoringDerivation.success) {
        return fail("Blocks Player unavailable: derived topology is not current.");
    }

    const int authoringLineId =
            FindSectorEditorAuthoringLineIdForTopologyLineDef(state, topologyLineDefId);
    const SectorAuthoringLine* authoringLine =
            FindSectorAuthoringLine(state.authoringGraph, authoringLineId);
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
        const char* successStatus,
        const char* failureStatus)
{
    SectorAuthoringDerivationResult result =
            DeriveSectorTopologyMapFromAuthoringGraph(state.authoringGraph);
    if (result.success) {
        int reconciledAnchorCount = 0;
        bool reconciliationFailed = false;
        if (ReconcileMissingDerivedFaceAnchors(
                    state,
                    result,
                    &reconciledAnchorCount,
                    &reconciliationFailed)) {
            MarkSectorEditorAuthoringGraphEdited(
                    state,
                    TextFormat("Added %d generated authoring face anchor%s",
                            reconciledAnchorCount,
                            reconciledAnchorCount == 1 ? "" : "s"));
            result = DeriveSectorTopologyMapFromAuthoringGraph(state.authoringGraph);
            if (!result.success
                    || !AllDerivedSectorsHaveUniqueFaceAnchorMappings(state.authoringGraph, result)) {
                state.authoringDerivation = std::move(result);
                state.authoringDerivedTopologyStale = true;
                state.authoringDerivationState = state.lastValidAuthoringDerivedTopology.has_value()
                        ? SectorEditorAuthoringDerivationState::InvalidLastValid
                        : SectorEditorAuthoringDerivationState::InvalidNoDerived;
                state.authoringDerivationStatus =
                        "Authoring graph: generated face-anchor reconciliation failed";
                state.topologyDocumentStatus = state.authoringDerivationStatus;
                InvalidateEditorTopologyRenderCacheIfNeeded(state);
                return false;
            }
        } else if (reconciliationFailed) {
            state.authoringDerivation = std::move(result);
            state.authoringDerivedTopologyStale = true;
            state.authoringDerivationState = state.lastValidAuthoringDerivedTopology.has_value()
                    ? SectorEditorAuthoringDerivationState::InvalidLastValid
                    : SectorEditorAuthoringDerivationState::InvalidNoDerived;
            state.authoringDerivationStatus =
                    "Authoring graph: generated face-anchor reconciliation failed";
            state.topologyDocumentStatus = state.authoringDerivationStatus;
            InvalidateEditorTopologyRenderCacheIfNeeded(state);
            return false;
        }
        CopyEditorMapLevelFields(result.topology, state.topologyMap);
        state.topologyMap = result.topology;
        state.lastValidAuthoringDerivedTopology = result.topology;
        state.authoringDerivation = std::move(result);
        state.authoringDerivedTopologyStale = false;
        state.authoringDerivationState = SectorEditorAuthoringDerivationState::ValidCurrent;
        state.authoringDerivationStatus = successStatus == nullptr || successStatus[0] == '\0'
                ? "Authoring graph: derived topology current"
                : successStatus;
        state.topologyDocumentStatus = state.authoringDerivationStatus;
        InvalidateEditorTopologyRenderCacheIfNeeded(state);
        return true;
    }

    state.authoringDerivation = std::move(result);
    state.authoringDerivedTopologyStale = true;
    state.authoringDerivationState = state.lastValidAuthoringDerivedTopology.has_value()
            ? SectorEditorAuthoringDerivationState::InvalidLastValid
            : SectorEditorAuthoringDerivationState::InvalidNoDerived;
    const int diagnosticCount = static_cast<int>(state.authoringDerivation.diagnostics.size());
    state.authoringDerivationStatus = failureStatus == nullptr || failureStatus[0] == '\0'
            ? TextFormat("Authoring graph: derivation failed (%d diagnostics)", diagnosticCount)
            : failureStatus;
    state.topologyDocumentStatus = state.authoringDerivationStatus;
    InvalidateEditorTopologyRenderCacheIfNeeded(state);
    return false;
}

bool CanUseCurrentAuthoringDerivedTopologyForPreview(
        const SectorEditorState& state,
        std::string* outMessage)
{
    return CanUseCurrentAuthoringDerivedTopology(state, "3D preview", outMessage);
}

bool CanUseCurrentAuthoringDerivedTopologyForLightmapBake(
        const SectorEditorState& state,
        std::string* outMessage)
{
    return CanUseCurrentAuthoringDerivedTopology(state, "Lightmap bake", outMessage);
}

} // namespace game
