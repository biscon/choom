#include "sector_editor/tools/camera/SectorEditorCameraTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"

namespace game {
namespace {

bool UpdateCameraTool(SectorEditorToolContext& context)
{
    if (context.input == nullptr || context.cameraEditing == nullptr) {
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
                context.cameraEditing->Place(context.state.snappedMouseMap);
                engine::ConsumeEvent(event);
                handled = true;
            });
    return handled;
}

const SectorEditorToolModule Module{
        SectorEditorTool::Camera,
        "Camera",
        nullptr,
        nullptr,
        nullptr,
        UpdateCameraTool,
        nullptr,
        nullptr};

} // namespace

const SectorEditorToolModule& SectorEditorCameraToolModule()
{
    return Module;
}

} // namespace game
