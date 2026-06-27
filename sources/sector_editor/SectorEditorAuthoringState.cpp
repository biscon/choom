#include "sector_editor/SectorEditorAuthoringState.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace game {
namespace {

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

} // namespace game
