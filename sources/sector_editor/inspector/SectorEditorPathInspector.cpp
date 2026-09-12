#include "sector_editor/inspector/SectorEditorPathInspector.h"
#include "sector_editor/services/paths/SectorEditorPathEditingService.h"
#include <cstdio>
namespace game
{
void DrawSectorEditorPathInspector(SectorEditorInspectorPanelContext &c, float width,
                                   float rowHeight, float gap)
{
    auto &editing = *c.pathEditing;
    const auto *path = editing.Selected();
    if (!path)
        return;
    auto &state = editing.State();
    if (state.bufferedPathId != path->editorId)
    {
        std::snprintf(state.idBuffer, sizeof(state.idBuffer), "%s", path->id.c_str());
        state.bufferedPathId = path->editorId;
    }
    float y = 0;
    const auto text = [&](const char *label)
    {
        engine::Text(c.ui, c.config, c.assets, {0, y, width, rowHeight}, c.font, label,
                     engine::UITextJustify::Left, c.config.textColor);
        y += rowHeight + gap;
    };
    text("Path ID");
    auto result = engine::TextInput(c.ui, c.config, c.input, c.assets, "path_id",
                                    {0, y, width, rowHeight}, c.font, state.idBuffer,
                                    sizeof(state.idBuffer), 1, 63, engine::UITextJustify::Left);
    y += rowHeight + gap;
    if (result.submitted)
        editing.Rename(state.idBuffer);
    path = editing.Selected();
    if (!path)
        return;
    text(TextFormat("%d waypoints", static_cast<int>(path->waypoints.size())));
    text(state.waypointId ? TextFormat("Waypoint %d selected", state.waypointId)
                          : "Drag a waypoint to move");
    if (engine::Button(c.ui, c.config, c.input, c.assets, "path_insert", {0, y, width, rowHeight},
                       c.font, "Insert Waypoint"))
        c.state.currentTool = SectorEditorTool::AuthoringInsertVertex;
    y += rowHeight + gap;
    if (engine::Button(c.ui, c.config, c.input, c.assets, "path_dissolve", {0, y, width, rowHeight},
                       c.font, "Dissolve Waypoint"))
        editing.Dissolve();
    y += rowHeight + gap;
    if (engine::Button(c.ui, c.config, c.input, c.assets, "path_delete", {0, y, width, rowHeight},
                       c.font, "Delete Path"))
        editing.Delete();
}
} // namespace game
