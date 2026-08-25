#include "sector_editor/tools/sound_emitter/SectorEditorSoundEmitterTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"

namespace game {
namespace {

bool UpdateSoundEmitterTool(SectorEditorToolContext& context)
{
    if (context.input == nullptr || context.soundEmitterEditing == nullptr) return false;
    bool handled = false;
    context.input->ForEachEvent(engine::InputEventType::MouseClick, true,
            [&context, &handled](engine::InputEvent& event) {
        if (handled || event.mouseClick.button != MOUSE_LEFT_BUTTON
                || !CheckCollisionPointRec(event.mouseClick.releasePosition, context.canvasRect)) return;
        context.soundEmitterEditing->Place(context.state.snappedMouseMap);
        engine::ConsumeEvent(event);
        handled = true;
    });
    return handled;
}

const SectorEditorToolModule Module{
        SectorEditorTool::SoundEmitter, "Sound Emitter", nullptr, nullptr, nullptr,
        UpdateSoundEmitterTool, nullptr, nullptr};

} // namespace

const SectorEditorToolModule& SectorEditorSoundEmitterToolModule() { return Module; }

} // namespace game
