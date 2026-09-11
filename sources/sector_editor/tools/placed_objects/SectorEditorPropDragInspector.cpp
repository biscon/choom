#include "sector_editor/tools/placed_objects/SectorEditorPropDragInspector.h"
#include "sector_editor/services/sounds/SectorEditorSoundService.h"
namespace game
{
void DrawSectorEditorPropDragInspector(SectorEditorPlacedObjectInspectorContext &c, float &y)
{
    const auto label = [&](const char *text)
    {
        engine::Text(c.ui, c.config, c.assets, {0, y, c.contentW, c.rowH}, c.font, text,
                     engine::UITextJustify::Left, c.config.textColor);
        y += c.rowH + c.gap;
    };
    const auto *object = c.editing.SelectedObject();
    if (!object)
        return;
    std::vector<std::string> names{"None"};
    std::vector<int> ids{0};
    int selected = 0;
    for (const auto &path : c.topologyMap.paths)
    {
        ids.push_back(path.editorId);
        names.push_back(path.id);
        if (path.editorId == object->dynamicModel.drag.pathEditorId)
            selected = static_cast<int>(ids.size() - 1);
    }
    if (object->dynamicModel.drag.pathEditorId && !selected)
    {
        ids.push_back(object->dynamicModel.drag.pathEditorId);
        names.push_back("Missing path");
        selected = static_cast<int>(ids.size() - 1);
    }
    label("Drag path");
    if (engine::Option(c.ui, c.config, c.input, c.assets, "prop_drag_path",
                       {0, y, c.contentW, c.rowH}, c.font, names, selected))
    {
        const int id = ids[selected];
        c.editing.MutateSelected("Assigned drag path",
                                 [id](auto &o)
                                 {
                                     if (o.dynamicModel.drag.pathEditorId == id)
                                         return false;
                                     o.dynamicModel.drag.pathEditorId = id;
                                     if (id)
                                         o.dynamicModel.collision = true;
                                     return true;
                                 });
    }
    y += c.rowH + c.gap;
    object = c.editing.SelectedObject();
    if (!object || !object->dynamicModel.drag.pathEditorId)
        return;
    label("Initial endpoint");
    static const std::vector<std::string> ends{"Start", "End"};
    int endpoint = object->dynamicModel.drag.startAtEnd ? 1 : 0;
    if (engine::Option(c.ui, c.config, c.input, c.assets, "prop_drag_end",
                       {0, y, c.contentW, c.rowH}, c.font, ends, endpoint))
        c.editing.MutateSelected("Changed drag endpoint",
                                 [endpoint](auto &o)
                                 {
                                     o.dynamicModel.drag.startAtEnd = endpoint != 0;
                                     return true;
                                 });
    y += c.rowH + c.gap;
    object = c.editing.SelectedObject();
    if (!object)
        return;
    label("Drag speed (m/s)");
    float speedValue = object->dynamicModel.drag.speedWorld;
    const auto speed = engine::FloatInput(c.ui, c.config, c.input, c.assets, "prop_drag_speed",
                                          {0, y, c.contentW, c.rowH}, c.font, speedValue,
                                          c.uiState.dragSpeedInput, 0.01f, 10.0f, 3);
    if (speed.changed && std::isfinite(speedValue))
        c.editing.MutateSelected("Changed drag speed",
                                 [&](auto &o)
                                 {
                                     o.dynamicModel.drag.speedWorld = speedValue;
                                     return true;
                                 });
    y += c.rowH + c.gap;
    const auto soundIds = c.sounds.SortedIds(SectorSoundType::Sound);
    for (int i = 0; i < 3; ++i)
    {
        object = c.editing.SelectedObject();
        if (!object)
            return;
        const auto &drag = object->dynamicModel.drag;
        const std::string saved = i == 0   ? drag.startSound
                                  : i == 1 ? drag.movingSound
                                           : drag.endSound;
        std::vector<std::string> options{"None"};
        options.insert(options.end(), soundIds.begin(), soundIds.end());
        int option = 0;
        for (size_t j = 1; j < options.size(); ++j)
            if (options[j] == saved)
                option = static_cast<int>(j);
        if (!saved.empty() && !option)
        {
            options.push_back(saved);
            option = static_cast<int>(options.size() - 1);
        }
        label(i == 0 ? "Start sound" : i == 1 ? "Moving sound (loop)" : "End sound");
        if (engine::Option(c.ui, c.config, c.input, c.assets, TextFormat("prop_drag_sound_%d", i),
                           {0, y, c.contentW, c.rowH}, c.font, options, option))
        {
            const std::string value = option ? options[option] : "";
            c.editing.MutateSelected(
                "Changed drag sound",
                [i, value](auto &o)
                {
                    auto &d = o.dynamicModel.drag;
                    (i == 0 ? d.startSound : i == 1 ? d.movingSound : d.endSound) = value;
                    return true;
                });
        }
        y += c.rowH + c.gap;
    }
    label("Dragging owns Use");
    label("Collision enabled");
}
} // namespace game
