#pragma once

#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorTopologyRenderCacheTypes.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <cstdint>

namespace game {

void UpdateCachedSectorEditorRuntimeObjectDraw(
        SectorEditorTopologyRenderCache& cache,
        const SectorPlacedRuntimeObject& object);

struct SectorEditorTopologyDrawContext {
    Rectangle canvasRect = {};
    Vector2 viewCenter = {};
    float viewZoom = 1.0f;
    bool showSectorIds = false;
    bool derivedTopologyStale = false;
    SectorEditorTool currentTool = SectorEditorTool::Select;
    TopologySelectionKind selectionKind = TopologySelectionKind::None;
    int selectedSectorId = -1;
    int selectedVertexId = -1;
    int selectedLightId = -1;
    int selectedStaticSpotLightId = -1;
    int selectedDynamicLightId = -1;
    int selectedDynamicSpotLightId = -1;
    int selectedRuntimeObjectId = -1;
    bool hasHoveredVertex = false;
    int hoveredVertexId = -1;
    int hoveredLightId = -1;
    int hoveredStaticSpotLightId = -1;
    int hoveredDynamicLightId = -1;
    int hoveredDynamicSpotLightId = -1;
    SectorAuthoringSelectionTarget selectedAuthoring;
    SectorAuthoringSelectionTarget hoveredAuthoring;
    const std::vector<int>* selectedAuthoringFaceAnchorIds = nullptr;
    int authoringFaceMergeTargetId = -1;
};

SectorEditorTopologyRenderCache BuildSectorEditorTopologyRenderCache(
        const SectorTopologyMap& map,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        uint64_t revision);

void AppendCachedRuntimeObjectPickCandidates(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context,
        Vector2 screenPoint,
        float tolerancePixels,
        std::vector<SectorEditorPickCandidate>& outCandidates);
void AppendCachedLevelMarkerPickCandidates(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context,
        Vector2 screenPoint,
        float tolerancePixels,
        std::vector<SectorEditorPickCandidate>& outCandidates);

void DrawCachedTopologySectors(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);
void DrawCachedTopologyLineDefs(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);
void DrawCachedTopologyVertices(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);
void DrawCachedTopologyStaticLights(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);
void DrawCachedTopologyStaticSpotLights(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);
void DrawCachedTopologyDynamicLights(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);
void DrawCachedTopologyDynamicSpotLights(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);
void DrawCachedRuntimeObjects(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);
void DrawCachedLevelMarkers(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context,
        const LevelMarkerDragState* drag = nullptr);
void DrawCachedAuthoringGraphOverlay(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);
void DrawCachedAuthoringDiagnostics(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);

bool ShouldDrawLegacyTopologySelectionHighlight(
        bool hasAuthoringGraphData,
        TopologySelectionKind selectionKind);

bool ShouldDrawAuthoringLineSelectionHighlight(
        SectorAuthoringSelectionTarget selectedAuthoring,
        int lineId);

bool ShouldDrawAuthoringVertexSelectionHighlight(
        SectorAuthoringSelectionTarget selectedAuthoring,
        int vertexId);

bool ShouldDrawAuthoringFaceSelectionHighlight(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context,
        int faceAnchorId);

const CachedAuthoringFaceHighlightDraw* FindCachedAuthoringFaceHighlight(
        const SectorEditorTopologyRenderCache& cache,
        int faceAnchorId);

} // namespace game
