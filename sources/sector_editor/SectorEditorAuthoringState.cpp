#include "sector_editor/SectorEditorAuthoringState.h"

#include "sector_editor/SectorEditorHelpers.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace game {
namespace {

bool HasAuthoringGraphData(const SectorEditorState& state)
{
    return !state.authoringGraph.vertices.empty()
            || !state.authoringGraph.lines.empty()
            || !state.authoringGraph.lineSides.empty()
            || !state.authoringGraph.faceAnchors.empty();
}

bool IsFlatSurfaceEditTarget(TopologySurfaceEditTargetKind kind)
{
    return kind == TopologySurfaceEditTargetKind::SectorFloor
            || kind == TopologySurfaceEditTargetKind::SectorCeiling;
}

bool SameSectorSurfaceRef(SectorSurfaceRef a, SectorSurfaceRef b)
{
    return a.kind == b.kind
            && a.topologySectorId == b.topologySectorId
            && a.topologyLineDefId == b.topologyLineDefId
            && a.topologySideDefId == b.topologySideDefId
            && a.topologySide == b.topologySide;
}

void CopyEditorMapLevelFields(SectorTopologyMap& target, const SectorTopologyMap& source)
{
    target.texturesById = source.texturesById;
    target.staticLights = source.staticLights;
    target.previewSettings = source.previewSettings;
    target.skySettings = source.skySettings;
    target.directionalLight = source.directionalLight;
    target.lightmapSettings = source.lightmapSettings;
    target.bakedLightmap = {};
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

bool SectorAuthoringSelectionTargetsEqual(
        SectorAuthoringSelectionTarget lhs,
        SectorAuthoringSelectionTarget rhs)
{
    return lhs.kind == rhs.kind
            && lhs.lineId == rhs.lineId
            && lhs.vertexId == rhs.vertexId;
}

bool IsSectorAuthoringSelectionTargetValid(
        const SectorAuthoringGraph& graph,
        SectorAuthoringSelectionTarget target)
{
    switch (target.kind) {
    case SectorAuthoringSelectionKind::None:
        return target.lineId == -1 && target.vertexId == -1;
    case SectorAuthoringSelectionKind::Line:
        return target.vertexId == -1
                && FindSectorAuthoringLine(graph, target.lineId) != nullptr;
    case SectorAuthoringSelectionKind::Vertex:
        return target.lineId == -1
                && FindSectorAuthoringVertex(graph, target.vertexId) != nullptr;
    }
    return false;
}

void ClearSectorEditorAuthoringSelection(SectorEditorState& state)
{
    state.selectedAuthoring = EmptyAuthoringSelectionTarget();
}

bool SelectSectorEditorAuthoringLine(SectorEditorState& state, int lineId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringLineSelectionTarget(lineId);
    if (target.kind != SectorAuthoringSelectionKind::Line
            || !IsSectorAuthoringSelectionTargetValid(state.authoringGraph, target)) {
        return false;
    }

    state.selectedAuthoring = target;
    return true;
}

bool SelectSectorEditorAuthoringVertex(SectorEditorState& state, int vertexId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringVertexSelectionTarget(vertexId);
    if (target.kind != SectorAuthoringSelectionKind::Vertex
            || !IsSectorAuthoringSelectionTargetValid(state.authoringGraph, target)) {
        return false;
    }

    state.selectedAuthoring = target;
    return true;
}

void ClearSectorEditorAuthoringHover(SectorEditorState& state)
{
    state.hoveredAuthoring = EmptyAuthoringSelectionTarget();
}

bool SetHoveredSectorEditorAuthoringLine(SectorEditorState& state, int lineId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringLineSelectionTarget(lineId);
    if (target.kind != SectorAuthoringSelectionKind::Line
            || !IsSectorAuthoringSelectionTargetValid(state.authoringGraph, target)) {
        return false;
    }

    state.hoveredAuthoring = target;
    return true;
}

bool SetHoveredSectorEditorAuthoringVertex(SectorEditorState& state, int vertexId)
{
    const SectorAuthoringSelectionTarget target =
            MakeSectorAuthoringVertexSelectionTarget(vertexId);
    if (target.kind != SectorAuthoringSelectionKind::Vertex
            || !IsSectorAuthoringSelectionTargetValid(state.authoringGraph, target)) {
        return false;
    }

    state.hoveredAuthoring = target;
    return true;
}

void PruneSectorEditorAuthoringSelectionToGraph(SectorEditorState& state)
{
    if (!IsSectorAuthoringSelectionTargetValid(state.authoringGraph, state.selectedAuthoring)) {
        ClearSectorEditorAuthoringSelection(state);
    }
    if (!IsSectorAuthoringSelectionTargetValid(state.authoringGraph, state.hoveredAuthoring)) {
        ClearSectorEditorAuthoringHover(state);
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

bool AddSectorEditorAuthoringLineSegment(
        SectorEditorState& state,
        SectorTopologyCoordPoint start,
        SectorTopologyCoordPoint end,
        int* outLineId)
{
    if (start.x == end.x && start.y == end.y) {
        return false;
    }

    const auto eraseVertex = [](SectorAuthoringGraph& graph, int vertexId) {
        graph.vertices.erase(
                std::remove_if(
                        graph.vertices.begin(),
                        graph.vertices.end(),
                        [vertexId](const SectorAuthoringVertex& vertex) {
                            return vertex.id == vertexId;
                        }),
                graph.vertices.end());
    };

    int startVertexId = -1;
    int endVertexId = -1;
    bool addedStart = false;
    bool addedEnd = false;
    if (!FindSectorAuthoringVertexAtPoint(state.authoringGraph, start, &startVertexId)) {
        if (!AddSectorAuthoringVertex(state.authoringGraph, start.x, start.y, &startVertexId)) {
            return false;
        }
        addedStart = true;
    }
    if (!FindSectorAuthoringVertexAtPoint(state.authoringGraph, end, &endVertexId)) {
        if (!AddSectorAuthoringVertex(state.authoringGraph, end.x, end.y, &endVertexId)) {
            if (addedStart) {
                eraseVertex(state.authoringGraph, startVertexId);
            }
            return false;
        }
        addedEnd = true;
    }

    int lineId = -1;
    if (!AddSectorAuthoringLine(state.authoringGraph, startVertexId, endVertexId, &lineId)) {
        if (addedEnd) {
            eraseVertex(state.authoringGraph, endVertexId);
        }
        if (addedStart) {
            eraseVertex(state.authoringGraph, startVertexId);
        }
        return false;
    }

    if (outLineId != nullptr) {
        *outLineId = lineId;
    }
    MarkSectorEditorAuthoringGraphEdited(
            state,
            TextFormat("Added authoring line %d", lineId));
    RefreshSectorEditorAuthoringDerivation(
            state,
            TextFormat("Added authoring line %d; derived topology current", lineId),
            TextFormat("Added authoring line %d; derivation failed", lineId));
    return true;
}

bool MoveSectorEditorAuthoringVertex(
        SectorEditorState& state,
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
    PruneSectorEditorAuthoringSelectionToGraph(state);
    MarkSectorEditorAuthoringGraphEdited(
            state,
            TextFormat("Moved authoring vertex %d", vertexId));
    RefreshSectorEditorAuthoringDerivation(
            state,
            TextFormat("Moved authoring vertex %d; derived topology current", vertexId),
            TextFormat("Moved authoring vertex %d; derivation failed", vertexId));
    return true;
}

bool DeleteSectorEditorSelectedAuthoringLine(SectorEditorState& state)
{
    if (state.selectedAuthoring.kind != SectorAuthoringSelectionKind::Line
            || !IsSectorAuthoringSelectionTargetValid(
                    state.authoringGraph,
                    state.selectedAuthoring)) {
        return false;
    }

    const int lineId = state.selectedAuthoring.lineId;
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
    PruneSectorEditorAuthoringSelectionToGraph(state);
    MarkSectorEditorAuthoringGraphEdited(
            state,
            TextFormat("Deleted authoring line %d", lineId));
    RefreshSectorEditorAuthoringDerivation(
            state,
            TextFormat("Deleted authoring line %d; derived topology current", lineId),
            TextFormat("Deleted authoring line %d; derivation failed", lineId));
    return true;
}

bool DeleteSectorEditorSelectedAuthoringVertex(SectorEditorState& state)
{
    if (state.selectedAuthoring.kind != SectorAuthoringSelectionKind::Vertex
            || !IsSectorAuthoringSelectionTargetValid(
                    state.authoringGraph,
                    state.selectedAuthoring)) {
        return false;
    }

    const int vertexId = state.selectedAuthoring.vertexId;
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
    PruneSectorEditorAuthoringSelectionToGraph(state);
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

bool ResolveSectorEditorAuthoringSurfaceTarget(
        const SectorEditorState& state,
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
    if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
            || state.authoringDerivedTopologyStale
            || !state.authoringDerivation.success) {
        return fail("3D surface edit unavailable: derived topology is not current");
    }

    if (IsWallSurface(surface.kind)) {
        const SectorTopologySideDef* sideDef =
                FindSectorTopologySideDef(state.topologyMap, surface.topologySideDefId);
        if (sideDef == nullptr
                || sideDef->lineDefId != surface.topologyLineDefId
                || sideDef->side != surface.topologySide
                || (surface.kind == SectorSurfaceKind::Middle
                        && !IsTopologyMiddleEligible(state.topologyMap, sideDef))) {
            return fail("3D surface edit unavailable: selected sidedef is not current");
        }

        bool found = false;
        SectorAuthoringSideId resolvedSide;
        for (const SectorAuthoringDerivedSideMapping& mapping
                : state.authoringDerivation.mapping.sides) {
            if (mapping.topologySideDefId != surface.topologySideDefId) {
                continue;
            }
            if (!IsValidSectorAuthoringId(mapping.authoringLineId)
                    || FindSectorAuthoringLine(
                            state.authoringGraph,
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

    if (FindSectorTopologySector(state.topologyMap, surface.topologySectorId) == nullptr) {
        return fail("3D surface edit unavailable: selected sector is not current");
    }

    bool found = false;
    int resolvedFaceAnchorId = -1;
    for (const SectorAuthoringDerivedSectorMapping& mapping
            : state.authoringDerivation.mapping.sectors) {
        if (mapping.topologySectorId != surface.topologySectorId) {
            continue;
        }
        if (!IsValidSectorAuthoringId(mapping.faceAnchorId)
                || FindSectorAuthoringFaceAnchor(
                        state.authoringGraph,
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

bool ApplySectorEditorAuthoringFaceAnchorFlatMaterialAction(
        SectorEditorState& state,
        SectorSurfaceRef surface,
        TopologySurfaceEditTarget target,
        const std::function<SectorEditorMaterialActionResult(SectorTopologyMap&)>& action,
        SectorEditorAuthoringFlatMaterialActionResult* outResult)
{
    SectorEditorAuthoringFlatMaterialActionResult result;
    const auto finish = [&result, outResult](bool returnValue) {
        if (outResult != nullptr) {
            *outResult = result;
        }
        return returnValue;
    };

    if (!HasAuthoringGraphData(state) || !IsFlatSurfaceEditTarget(target.kind)) {
        return finish(false);
    }

    result.handled = true;
    if (!action) {
        result.status = "Flat material edit unavailable.";
        return finish(true);
    }

    SectorEditorAuthoringSurfaceTarget authoringTarget;
    std::string unavailableStatus;
    if (!ResolveSectorEditorAuthoringSurfaceTarget(
                state,
                surface,
                authoringTarget,
                &unavailableStatus)
            || authoringTarget.kind != SectorEditorAuthoringSurfaceTargetKind::FaceAnchor) {
        if (state.selectedSurface3D.kind != SectorSurfaceKind::None
                && SameSectorSurfaceRef(state.selectedSurface3D, surface)) {
            state.selectedSurface3D = SectorSurfaceRef{};
            state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
        }
        result.status = unavailableStatus.empty()
                ? "3D flat surface edit unavailable: selected surface has no face anchor mapping"
                : unavailableStatus;
        return finish(true);
    }

    SectorTopologyMap editedTopology = state.topologyMap;
    result.materialResult = action(editedTopology);
    result.status = result.materialResult.status;
    if (!result.materialResult.changed) {
        return finish(true);
    }

    const SectorTopologySector* editedSector =
            FindSectorTopologySector(editedTopology, target.sectorId);
    if (editedSector == nullptr) {
        result.status = "3D flat surface edit unavailable: selected sector is no longer valid";
        result.materialResult.changed = false;
        return finish(true);
    }

    const char* status = result.materialResult.status.empty()
            ? "Updated authoring face anchor material"
            : result.materialResult.status.c_str();
    const bool refreshed = MutateSectorEditorAuthoringFaceAnchorForTopologySector(
            state,
            target.sectorId,
            status,
            [target, editedSector](SectorAuthoringFaceAnchor& anchor) {
                if (target.kind == TopologySurfaceEditTargetKind::SectorFloor) {
                    anchor.floorTextureId = editedSector->floorTextureId;
                    anchor.floorUv = editedSector->floorUv;
                    anchor.floorDecal = editedSector->floorDecal;
                } else if (target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
                    anchor.ceilingTextureId = editedSector->ceilingTextureId;
                    anchor.ceilingUv = editedSector->ceilingUv;
                    anchor.ceilingDecal = editedSector->ceilingDecal;
                } else {
                    return false;
                }
                return true;
            });
    result.changed = refreshed;
    if (!refreshed) {
        result.status = "3D flat surface edit unavailable: selected sector has no face anchor mapping";
    }
    return finish(true);
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
    SectorAuthoringLine* line =
            FindSectorAuthoringLine(state.authoringGraph, authoringLineId);
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

bool RefreshSectorEditorAuthoringDerivation(
        SectorEditorState& state,
        const char* successStatus,
        const char* failureStatus)
{
    SectorAuthoringDerivationResult result =
            DeriveSectorTopologyMapFromAuthoringGraph(state.authoringGraph);
    if (result.success) {
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
