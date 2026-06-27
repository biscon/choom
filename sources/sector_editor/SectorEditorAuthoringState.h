#pragma once

#include "sector_editor/SectorEditorTypes.h"

namespace game {

SectorAuthoringSelectionTarget MakeSectorAuthoringLineSelectionTarget(int lineId);
SectorAuthoringSelectionTarget MakeSectorAuthoringVertexSelectionTarget(int vertexId);

bool SectorAuthoringSelectionTargetsEqual(
        SectorAuthoringSelectionTarget lhs,
        SectorAuthoringSelectionTarget rhs);

bool IsSectorAuthoringSelectionTargetValid(
        const SectorAuthoringGraph& graph,
        SectorAuthoringSelectionTarget target);

void ClearSectorEditorAuthoringSelection(SectorEditorState& state);
bool SelectSectorEditorAuthoringLine(SectorEditorState& state, int lineId);
bool SelectSectorEditorAuthoringVertex(SectorEditorState& state, int vertexId);

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

void MarkSectorEditorAuthoringGraphEdited(
        SectorEditorState& state,
        const char* status);

bool RefreshSectorEditorAuthoringDerivation(
        SectorEditorState& state,
        const char* successStatus = nullptr,
        const char* failureStatus = nullptr);

} // namespace game
