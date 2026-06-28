#pragma once

#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorTypes.h"

#include <functional>
#include <string>

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

void ClearSectorEditorAuthoringSelection(SectorEditorState& state);
bool SelectSectorEditorAuthoringLine(SectorEditorState& state, int lineId);
bool SelectSectorEditorAuthoringVertex(SectorEditorState& state, int vertexId);
bool SelectSectorEditorAuthoringFaceAnchor(SectorEditorState& state, int faceAnchorId);

void ClearSectorEditorAuthoringHover(SectorEditorState& state);
bool SetHoveredSectorEditorAuthoringLine(SectorEditorState& state, int lineId);
bool SetHoveredSectorEditorAuthoringVertex(SectorEditorState& state, int vertexId);

void PruneSectorEditorAuthoringSelectionToGraph(SectorEditorState& state);

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

bool AddSectorEditorAuthoringLineSegment(
        SectorEditorState& state,
        SectorTopologyCoordPoint start,
        SectorTopologyCoordPoint end,
        int* outLineId = nullptr);

bool DeleteSectorEditorSelectedAuthoringLine(SectorEditorState& state);
bool MoveSectorEditorAuthoringVertex(
        SectorEditorState& state,
        int vertexId,
        SectorTopologyCoordPoint target);
bool DeleteSectorEditorSelectedAuthoringVertex(SectorEditorState& state);

void InitializeSectorEditorAuthoringStateFromTopology(
        SectorEditorState& state,
        const SectorTopologyMap& sourceMap);

bool HasAuthoringGraphData(const SectorEditorState& state);

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

SectorEditorInspectorTarget ResolveSectorEditorInspectorTarget(const SectorEditorState& state);

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

bool ClearSelectedSectorEditorSurface3DIfAuthoringMappingUnavailable(
        SectorEditorState& state,
        std::string* outStatus = nullptr);

struct SectorEditorAuthoringFlatMaterialActionResult {
    bool handled = false;
    bool changed = false;
    SectorEditorMaterialActionResult materialResult;
    std::string status;
};

bool ApplySectorEditorAuthoringFaceAnchorFlatMaterialAction(
        SectorEditorState& state,
        SectorSurfaceRef surface,
        TopologySurfaceEditTarget target,
        const std::function<SectorEditorMaterialActionResult(SectorTopologyMap&)>& action,
        SectorEditorAuthoringFlatMaterialActionResult* outResult = nullptr);

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
