#pragma once

#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/preview/SectorEditorPreviewState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace game {

SectorAuthoringSelectionTarget MakeSectorAuthoringLineSelectionTarget(int lineId);
SectorAuthoringSelectionTarget MakeSectorAuthoringVertexSelectionTarget(int vertexId);
SectorAuthoringSelectionTarget MakeSectorAuthoringFaceAnchorSelectionTarget(int faceAnchorId);
SectorAuthoringSelectionTarget MakeSectorAuthoringFogVolumeSelectionTarget(int fogVolumeId);
SectorAuthoringSelectionTarget MakeSectorAuthoringReflectionProbeSelectionTarget(int probeId);
SectorAuthoringSelectionTarget MakeSectorAuthoringLevelMarkerSelectionTarget(int markerId);
SectorAuthoringSelectionTarget MakeSectorAuthoringSoundEmitterSelectionTarget(int emitterId);
SectorAuthoringSelectionTarget MakeSectorAuthoringTriggerSelectionTarget(int triggerId);
SectorAuthoringSelectionTarget MakeSectorAuthoringStructuralPrimitiveSelectionTarget(int primitiveId);

bool SectorAuthoringSelectionTargetsEqual(
        SectorAuthoringSelectionTarget lhs,
        SectorAuthoringSelectionTarget rhs);

bool IsSectorAuthoringSelectionTargetValid(
        const SectorAuthoringGraph& graph,
        SectorAuthoringSelectionTarget target);

void ClearSectorEditorAuthoringSelection(SelectionState& selectionState);
void ReserveSectorEditorAuthoringFaceSelection(
        SelectionState& selectionState,
        std::size_t capacity);
bool IsSectorEditorAuthoringFaceSelected(
        const SelectionState& selectionState,
        int faceAnchorId);
bool ToggleSectorEditorAuthoringFaceSelection(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int faceAnchorId);
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
bool SelectSectorEditorAuthoringFogVolume(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int fogVolumeId);
bool SelectSectorEditorAuthoringReflectionProbe(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int probeId);
bool SelectSectorEditorAuthoringLevelMarker(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int markerId);
bool SelectSectorEditorAuthoringSoundEmitter(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int emitterId);
bool SelectSectorEditorAuthoringTrigger(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int triggerId);
bool SelectSectorEditorAuthoringStructuralPrimitive(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int primitiveId);

void ClearSectorEditorAuthoringHover(SelectionState& selectionState);
bool SetHoveredSectorEditorAuthoringLine(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int lineId);
bool SetHoveredSectorEditorAuthoringVertex(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int vertexId);
bool SetHoveredSectorEditorAuthoringFogVolume(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int fogVolumeId);
bool SetHoveredSectorEditorAuthoringReflectionProbe(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int probeId);
bool SetHoveredSectorEditorAuthoringLevelMarker(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int markerId);
bool SetHoveredSectorEditorAuthoringSoundEmitter(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int emitterId);
bool SetHoveredSectorEditorAuthoringTrigger(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int triggerId);
bool SetHoveredSectorEditorAuthoringStructuralPrimitive(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState,
        int primitiveId);

void PruneSectorEditorAuthoringSelectionToGraph(
        const SectorAuthoringGraph& graph,
        SelectionState& selectionState);

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
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        Vector2 mapPoint,
        int* outFaceAnchorId = nullptr,
        std::string* outStatus = nullptr);
bool FindSectorEditorAuthoringSelectionAtMapPoint(
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
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
    std::string errorMessage;
};

bool AddSectorEditorAuthoringLineSegment(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
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
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
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
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SectorTopologyCoordPoint firstCorner,
        SectorTopologyCoordPoint oppositeCorner,
        SectorEditorAuthoringRectangleResult* outResult = nullptr);

bool InsertSectorEditorAuthoringVertexOnLine(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SelectionState& selectionState,
        int lineId,
        SectorTopologyCoordPoint point,
        SectorAuthoringInsertVertexResult* outResult = nullptr);

bool DeleteSectorEditorSelectedAuthoringLine(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SelectionState& selectionState);

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
        const char* status);
void CopySectorEditorMapLevelFields(
        SectorTopologyMap& target,
        const SectorTopologyMap& source);
bool MoveSectorEditorAuthoringVertex(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SelectionState& selectionState,
        int vertexId,
        SectorTopologyCoordPoint target);
bool DeleteSectorEditorSelectedAuthoringVertex(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SelectionState& selectionState);

void InitializeSectorEditorAuthoringStateFromTopology(
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        const SectorTopologyMap& sourceMap);

bool HasAuthoringGraphData(const SectorAuthoringGraph& graph);

void MarkSectorEditorAuthoringGraphEdited(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorEditorDerivationDocumentAccess derivation,
        const char* status);
void MarkSectorEditorAuthoringGraphEdited(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorEditorDerivationDocumentAccess derivation,
        const char* status);

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
        std::string* outStatus = nullptr);

int FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        int topologySectorId);

bool FindSectorEditorAuthoringSideIdForTopologySideDef(
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        int topologySideDefId,
        SectorAuthoringSideId& outSideId);

int FindSectorEditorAuthoringLineIdForTopologyLineDef(
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        int topologyLineDefId);

enum class SectorEditorInspectorTargetKind {
    None,
    AuthoringLine,
    AuthoringFaceAnchor,
    AuthoringVertex,
    AuthoringFogVolume,
    AuthoringReflectionProbe,
    AuthoringLevelMarker,
    AuthoringSoundEmitter,
    AuthoringTrigger,
    AuthoringStructuralPrimitive,
    AuthoringUnavailable,
    LegacyTopology
};

struct SectorEditorInspectorTarget {
    SectorEditorInspectorTargetKind kind = SectorEditorInspectorTargetKind::None;
    int lineId = -1;
    int faceAnchorId = -1;
    int vertexId = -1;
    int fogVolumeId = -1;
    int reflectionProbeId = -1;
    int levelMarkerId = -1;
    int soundEmitterId = -1;
    int triggerId = -1;
    int structuralPrimitiveId = -1;
    SectorAuthoringSideId side;
    std::string status;
};

SectorEditorInspectorTarget ResolveSectorEditorInspectorTarget(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        const SelectionState& selectionState);

std::string BuildSectorEditorSurface3DTargetLabel(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
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
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        SectorEditorPreviewSelectionState& previewSelectionState,
        std::string* outStatus = nullptr);

bool MutateSectorEditorAuthoringFaceAnchorForTopologySector(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologySectorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate);
bool MutateSectorEditorAuthoringFaceAnchorForTopologySector(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologySectorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate);

bool MutateSectorEditorAuthoringFaceAnchorById(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int faceAnchorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate);
bool MutateSectorEditorAuthoringFaceAnchorById(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int faceAnchorId,
        const char* status,
        const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate);

bool SetSectorEditorAuthoringFaceRoomtoneMode(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int faceAnchorId,
        SectorRoomtoneMode mode,
        std::string* outStatus = nullptr);

bool SetSectorEditorAuthoringFaceRoomtoneSoundId(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int faceAnchorId,
        const std::string& soundId,
        std::string* outStatus = nullptr);

bool MutateSectorEditorAuthoringSideForTopologySideDef(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologySideDefId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate);
bool MutateSectorEditorAuthoringSideForTopologySideDef(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologySideDefId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate);

bool MutateSectorEditorAuthoringSideById(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SectorAuthoringSideId sideId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate);
bool MutateSectorEditorAuthoringSideById(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SectorAuthoringSideId sideId,
        const char* status,
        const std::function<bool(SectorAuthoringLineSide&)>& mutate);

bool MutateSectorEditorAuthoringLineForTopologyLineDef(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologyLineDefId,
        const char* status,
        const std::function<bool(SectorAuthoringLine&)>& mutate);

bool MutateSectorEditorAuthoringLineById(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int lineId,
        const char* status,
        const std::function<bool(SectorAuthoringLine&)>& mutate);

bool SetSectorEditorAuthoringLineDefBlocksPlayer(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        int topologyLineDefId,
        bool blocksPlayer,
        std::string* outStatus = nullptr);

bool RefreshSectorEditorAuthoringDerivation(
        SectorEditorState& state,
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        const char* successStatus = nullptr,
        const char* failureStatus = nullptr);
bool RefreshSectorEditorAuthoringDerivation(
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        const char* successStatus = nullptr,
        const char* failureStatus = nullptr);

std::string BuildSectorEditorAuthoringDerivationDisplayStatus(
        SectorEditorConstDerivationDocumentAccess derivation,
        const char* fallbackStatus = nullptr);

bool CanUseCurrentAuthoringDerivedTopologyForPreview(
        SectorEditorConstDerivationDocumentAccess derivation,
        std::string* outMessage = nullptr);

bool CanUseCurrentAuthoringDerivedTopologyForLightmapBake(
        SectorEditorConstDerivationDocumentAccess derivation,
        std::string* outMessage = nullptr);

} // namespace game
