#include "sector_editor/tools/level_marker/SectorEditorLevelMarkerTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"

namespace game {
namespace {

bool UpdateLevelMarkerTool(SectorEditorToolContext& context)
{
    if (context.input == nullptr || context.levelMarkerEditing == nullptr) {
        return false;
    }
    bool handled = false;
    context.input->ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [&context, &handled](engine::InputEvent& event) {
                if (handled
                        || event.mouseClick.button != MOUSE_LEFT_BUTTON
                        || !CheckCollisionPointRec(event.mouseClick.releasePosition, context.canvasRect)) {
                    return;
                }
                context.levelMarkerEditing->Place(context.state.snappedMouseMap);
                engine::ConsumeEvent(event);
                handled = true;
            });
    return handled;
}

const SectorEditorToolModule Module{
        SectorEditorTool::LevelMarker,
        "Level Marker",
        nullptr,
        nullptr,
        nullptr,
        UpdateLevelMarkerTool,
        nullptr,
        nullptr};

} // namespace

const SectorEditorToolModule& SectorEditorLevelMarkerToolModule()
{
    return Module;
}

} // namespace game
