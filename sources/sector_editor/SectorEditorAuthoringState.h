#pragma once

#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"

#include <functional>
#include <string>
#include <vector>

namespace game {

SectorAuthoringSelectionTarget MakeSectorAuthoringLineSelectionTarget(int lineId);
SectorAuthoringSelectionTarget MakeSectorAuthoringVertexSelectionTarget(int vertexId);
SectorAuthoringSelectionTarget MakeSectorAuthoringFaceAnchorSelectionTarget(int faceAnchorId);

bool SectorAuthoringSelectionTargetsEqual(
        SectorAuthoringSelectionTarget lhs,
        SectorAuthoringSelectionTarget rhs);

bool IsSectorAuthoringSelectionTargetValid(
        const SectorAuthoringGraph& graph,
        SectorAuthoringSelectionTarget target);

void ClearSectorEditorAuthoringSelection(SelectionState& selectionState);
bool SelectSectorEditorAuthoringLine(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int lineId);
bool SelectSectorEditorAuthoringVertex(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int vertexId);
bool SelectSectorEditorAuthoringFaceAnchor(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int faceAnchorId);

void ClearSectorEditorAuthoringHover(SelectionState& selectionState);
bool SetHoveredSectorEditorAuthoringLine(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int lineId);
bool SetHoveredSectorEditorAuthoringVertex(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int vertexId);

void PruneSectorEditorAuthoringSelectionToGraph(SectorEditorState& state, SelectionState& selectionState);

bool FindSectorAuthoringVertexAtPoint(
        const SectorAuthoringGraph& graph,
        SectorTopologyCoordPoint point,
        int* outVertexId = nullptr);

bool FindSectorEditorAuthoringLineNearMapPoint(
        const SectorAuthoringGraph& graph,
        Vector2 mapPoint,
        float maxDistance,
        int* outLineId = nullptr);

bool FindSectorEditorAuthoringVertexNearMapPoint(
        const SectorAuthoringGraph& graph,
        Vector2 mapPoint,
        float maxDistance,
        int* outVertexId = nullptr,
        SectorTopologyCoordPoint* outPoint = nullptr);

bool FindSectorEditorAuthoringSelectionNearMapPoint(
        const SectorAuthoringGraph& graph,
        Vector2 mapPoint,
        float vertexMaxDistance,
        float lineMaxDistance,
        SectorAuthoringSelectionTarget* outTarget = nullptr,
        SectorTopologyCoordPoint* outVertexPoint = nullptr);

bool FindSectorEditorAuthoringFaceAnchorAtMapPoint(
        const SectorEditorState& state,
        Vector2 mapPoint,
        int* outFaceAnchorId = nullptr,
        std::string* outStatus = nullptr);

bool FindSectorEditorAuthoringSelectionAtMapPoint(
        const SectorEditorState& state,
        Vector2 mapPoint,
        float vertexMaxDistance,
        float lineMaxDistance,
        SectorAuthoringSelectionTarget* outTarget = nullptr,
        SectorTopologyCoordPoint* outVertexPoint = nullptr,
        std::string* outStatus = nullptr);

struct SectorEditorAuthoringLineSegmentResult {
    int lineId = -1;
    int startVertexId = -1;
    int endVertexId = -1;
    SectorTopologyCoordPoint startPoint = {};
    SectorTopologyCoordPoint endPoint = {};
};

bool AddSectorEditorAuthoringLineSegment(
        SectorEditorState& state,
        SelectionState& selectionState,
        SectorTopologyCoordPoint start,
        SectorTopologyCoordPoint end,
        int* outLineId = nullptr,
        SectorEditorAuthoringLineSegmentResult* outResult = nullptr);

enum class SectorEditorAuthoringLineToolClickStatus {
    StartedChain,
    CreatedSegment,
    ZeroLength,
    Rejected
};

struct SectorEditorAuthoringLineToolClickResult {
    SectorEditorAuthoringLineToolClickStatus status =
            SectorEditorAuthoringLineToolClickStatus::Rejected;
    SectorEditorAuthoringLineSegmentResult segment;
};

SectorEditorAuthoringLineToolClickResult ClickSectorEditorAuthoringLineTool(
        SectorEditorState& state,
        SelectionState& selectionState,
        SectorTopologyCoordPoint point);

void CancelSectorEditorAuthoringLineToolChain(SectorEditorState& state);

struct SectorEditorAuthoringRectangleResult {
    int vertexIds[4] = {-1, -1, -1, -1};
    int lineIds[4] = {-1, -1, -1, -1};
    std::vector<int> insertedLineIds;
    std::string errorMessage;
};

bool CreateSectorAuthoringRectangle(
        SectorAuthoringGraph& graph,
        SectorTopologyCoordPoint firstCorner,
        SectorTopologyCoordPoint oppositeCorner,
        SectorEditorAuthoringRectangleResult* outResult = nullptr);

bool AddSectorEditorAuthoringRectangle(
        SectorEditorState& state,
        SectorTopologyCoordPoint firstCorner,
        SectorTopologyCoordPoint oppositeCorner,
        SectorEditorAuthoringRectangleResult* outResult = nullptr);

bool InsertSectorEditorAuthoringVertexOnLine(
        SectorEditorState& state,
        SelectionState& selectionState,
        int lineId,
        SectorTopologyCoordPoint point,
        SectorAuthoringInsertVertexResult* outResult = nullptr);

bool DeleteSectorEditorSelectedAuthoringLine(SectorEditorState& state, SelectionState& selectionState);
bool MoveSectorEditorAuthoringVertex(
        SectorEditorState& state,
        SelectionState& selectionState,
        int vertexId,
        SectorTopologyCoordPoint target);
bool DeleteSectorEditorSelectedAuthoringVertex(SectorEditorState& state, SelectionState& selectionState);

void InitializeSectorEditorAuthoringStateFromTopology(
        SectorEditorState& state,
        const SectorTopologyMap& sourceMap);

bool HasAuthoringGraphData(const SectorEditorState& state);
bool HasAuthoringGraphData(const SectorAuthoringGraph& graph);

void MarkSectorEditorAuthoringGraphEdited(
        SectorEditorState& state,
        const char* status);

int FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
        const SectorEditorState& state,
        int topologySectorId);

bool FindSectorEditorAuthoringSideIdForTopologySideDef(
        const SectorEditorState& state,
        int topologySideDefId,
        SectorAuthoringSideId& outSideId);

int FindSectorEditorAuthoringLineIdForTopologyLineDef(
        const SectorEditorState& state,
        int topologyLineDefId);

enum class SectorEditorInspectorTargetKind {
    None,
    AuthoringLine,
    AuthoringFaceAnchor,
    AuthoringVertex,
    AuthoringUnavailable,
    LegacyTopology
};

struct SectorEditorInspectorTarget {
    SectorEditorInspectorTargetKind kind = SectorEditorInspectorTargetKind::None;
    int lineId = -1;
    int faceAnchorId = -1;
    int vertexId = -1;
    SectorAuthoringSideId side;
    std::string status;
};

SectorEditorInspectorTarget ResolveSectorEditorInspectorTarget(
        const SectorEditorState& state,
        const SelectionState& selectionState);

std::string BuildSectorEditorSurface3DTargetLabel(
        const SectorEditorState& state,
        SectorSurfaceRef surface,
        TopologySurfaceEditTarget target);

enum class SectorEditorAuthoringSurfaceTargetKind {
    None,
    FaceAnchor,
    Side
};

struct SectorEditorAuthoringSurfaceTarget {
    SectorEditorAuthoringSurfaceTargetKind kind = SectorEditorAuthoringSurfaceTargetKind::None;
    int faceAnchorId = -1;
    SectorAuthoringSideId side;
};

bool ResolveSectorEditorAuthoringSurfaceTarget(
        const SectorEditorState& state,
        SectorSurfaceRef surface,
        SectorEditorAuthoringSurfaceTarget& outTarget,
        std::string* outStatus = nullptr);
bool ResolveSectorEditorAuthoringSurfaceTarget(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        SectorSurfaceRef surface,
        SectorEditorAuthoringSurfaceTarget& outTarget,
        std::string* outStatus = nullptr);

SectorAuthoringSelectionTarget MakeSectorEditorAuthoringSelectionTargetForSurfaceTarget(
        SectorEditorAuthoringSurfaceTarget target);

bool ClearSelectedSectorEditorSurface3DIfAuthoringMappingUnavailable(
        SectorEditorState& state,
        std::string* outStatus = nullptr);

bool MutateSectorEditorAuthoringFaceAnchorForTopologySector(
        SectorEditorState& state,
        int topologySectorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate);

bool MutateSectorEditorAuthoringFaceAnchorById(
        SectorEditorState& state,
        int faceAnchorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate);

bool MutateSectorEditorAuthoringSideForTopologySideDef(
        SectorEditorState& state,
        int topologySideDefId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate);

bool MutateSectorEditorAuthoringSideById(
        SectorEditorState& state,
        SectorAuthoringSideId sideId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate);

bool MutateSectorEditorAuthoringLineForTopologyLineDef(
        SectorEditorState& state,
        int topologyLineDefId,
        const char* status,
        const std::function<bool(SectorAuthoringLine&)>& mutate);

bool MutateSectorEditorAuthoringLineById(
        SectorEditorState& state,
        int lineId,
        const char* status,
        const std::function<bool(SectorAuthoringLine&)>& mutate);

bool SetSectorEditorAuthoringLineDefBlocksPlayer(
        SectorEditorState& state,
        int topologyLineDefId,
        bool blocksPlayer,
        std::string* outStatus = nullptr);

bool RefreshSectorEditorAuthoringDerivation(
        SectorEditorState& state,
        const char* successStatus = nullptr,
        const char* failureStatus = nullptr);

bool CanUseCurrentAuthoringDerivedTopologyForPreview(
        const SectorEditorState& state,
        std::string* outMessage = nullptr);

bool CanUseCurrentAuthoringDerivedTopologyForLightmapBake(
        const SectorEditorState& state,
        std::string* outMessage = nullptr);

} // namespace game
