#include "sector_editor/SectorEditorVertexInspector.h"

#include "sector_demo/SectorTopologyUnits.h"

namespace game {

float SelectedVertexInspectorContentHeight()
{
    return 280.0f;
}

float InspectedVertexInspectorContentHeight()
{
    return 170.0f;
}

bool DrawTopologyVertexInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        float contentW,
        float rowH,
        float gap,
        const SectorTopologyVertex* inspectedVertex,
        bool hasSelectedTopologyVertex,
        SectorEditorState& state,
        const SectorEditorVertexInspectorCallbacks& callbacks)
{
    float y = 0.0f;
    const int vertexId = state.pendingTopologyVertexMerge.active
            ? state.pendingTopologyVertexMerge.sourceVertexId
            : (hasSelectedTopologyVertex ? state.selectedTopologyVertexId : inspectedVertex->id);
    const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
    if (vertex != nullptr) {
        int incidentLineCount = 0;
        for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
            if (lineDef.startVertexId == vertex->id || lineDef.endVertexId == vertex->id) {
                ++incidentLineCount;
            }
        }
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Topology Vertex: %d", vertex->id),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                TextFormat("ID: %d", vertex->id),
                engine::UITextJustify::Left,
                config.mutedTextColor);
        y += 30.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                TextFormat(
                        "Position: %.2f, %.2f",
                        SectorCoordToVisibleAuthoring(vertex->x),
                        SectorCoordToVisibleAuthoring(vertex->y)),
                engine::UITextJustify::Left,
                config.mutedTextColor);
        y += 34.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                TextFormat("Incident linedefs: %d", incidentLineCount),
                engine::UITextJustify::Left,
                config.mutedTextColor);
        y += 34.0f;

        if (state.pendingTopologyVertexMerge.active) {
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 44.0f},
                    font,
                    state.pendingTopologyVertexMerge.message.c_str(),
                    engine::UITextJustify::Left,
                    state.pendingTopologyVertexMerge.hasValidTarget
                            ? config.textColor
                            : config.mutedTextColor);
            y += 48.0f;
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_cancel_vertex_merge",
                        Rectangle{0.0f, y, contentW, rowH},
                        font,
                        "Cancel Merge")) {
                callbacks.cancelPendingTopologyVertexMerge("Cancelled vertex merge");
            }
        } else if (hasSelectedTopologyVertex) {
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_dissolve_vertex",
                        Rectangle{0.0f, y, contentW, rowH},
                        font,
                        "Dissolve Vertex")) {
                callbacks.dissolveSelectedTopologyVertex();
            }
            y += rowH + gap;
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_merge_vertex_into",
                        Rectangle{0.0f, y, contentW, rowH},
                        font,
                        "Merge Into...")) {
                callbacks.startPendingTopologyVertexMerge(vertex->id);
            }
        } else if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_merge_vertex_into",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Merge Into...")) {
            callbacks.startPendingTopologyVertexMerge(vertex->id);
        }
    } else {
        state.inspectedTopologyVertexId = -1;
        if (hasSelectedTopologyVertex) {
            callbacks.clearStaleTopologySelection();
        }
    }

    return true;
}

} // namespace game
