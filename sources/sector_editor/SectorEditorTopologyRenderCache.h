#pragma once

#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <cstdint>

namespace game {

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
    int selectedDynamicLightId = -1;
    int selectedDynamicSpotLightId = -1;
    bool hasHoveredVertex = false;
    int hoveredVertexId = -1;
    int hoveredLightId = -1;
    int hoveredDynamicLightId = -1;
    int hoveredDynamicSpotLightId = -1;
    SectorAuthoringSelectionTarget selectedAuthoring;
    SectorAuthoringSelectionTarget hoveredAuthoring;
};

SectorEditorTopologyRenderCache BuildSectorEditorTopologyRenderCache(
        const SectorTopologyMap& map,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        uint64_t revision);

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
void DrawCachedTopologyDynamicLights(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);
void DrawCachedTopologyDynamicSpotLights(
        const SectorEditorTopologyRenderCache& cache,
        const SectorEditorTopologyDrawContext& context);
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
