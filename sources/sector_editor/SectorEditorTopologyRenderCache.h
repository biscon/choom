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
    SectorEditorTool currentTool = SectorEditorTool::Select;
    TopologySelectionKind selectionKind = TopologySelectionKind::None;
    int selectedSectorId = -1;
    int selectedVertexId = -1;
    int selectedLightId = -1;
    bool hasHoveredVertex = false;
    int hoveredVertexId = -1;
    int hoveredLightId = -1;
};

SectorEditorTopologyRenderCache BuildSectorEditorTopologyRenderCache(
        const SectorTopologyMap& map,
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

} // namespace game
