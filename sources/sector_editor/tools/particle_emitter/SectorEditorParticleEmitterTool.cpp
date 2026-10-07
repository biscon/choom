#include "sector_editor/tools/particle_emitter/SectorEditorParticleEmitterTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"

namespace game {
namespace {

bool UpdateParticleEmitterTool(SectorEditorToolContext& context)
{
    if (context.input == nullptr || context.particleEmitterEditing == nullptr) return false;
    bool handled = false;
    context.input->ForEachEvent(engine::InputEventType::MouseClick, true,
            [&context, &handled](engine::InputEvent& event) {
        if (handled || event.mouseClick.button != MOUSE_LEFT_BUTTON
                || !CheckCollisionPointRec(event.mouseClick.releasePosition, context.canvasRect)) return;
        context.particleEmitterEditing->Place(context.state.snappedMouseMap);
        engine::ConsumeEvent(event);
        handled = true;
    });
    return handled;
}

const SectorEditorToolModule Module{
        SectorEditorTool::ParticleEmitter, "Particle Emitter", nullptr, nullptr, nullptr,
        UpdateParticleEmitterTool, nullptr, nullptr};

} // namespace

const SectorEditorToolModule& SectorEditorParticleEmitterToolModule() { return Module; }

} // namespace game
