#include "sector_editor/tools/fog_volume/SectorEditorFogVolumeTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"

namespace game {
namespace {

bool UpdateFogVolumeTool(SectorEditorToolContext& context)
{
    if (context.input == nullptr || context.fogVolumeEditing == nullptr) {
        return false;
    }
    bool handled = false;
    context.input->ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [&context, &handled](engine::InputEvent& event) {
                if (handled
                        || event.mouseClick.button != MOUSE_LEFT_BUTTON
                        || !CheckCollisionPointRec(event.mouseClick.releasePosition, context.canvasRect)
                        || !context.currentSnappedSectorPoint
                        || !context.toTopologyCoordPoint) {
                    return;
                }
                SectorTopologyCoordPoint point;
                std::string error;
                if (context.toTopologyCoordPoint(context.currentSnappedSectorPoint(), point, error)) {
                    context.fogVolumeEditing->Place(point);
                } else {
                    context.statusText = error;
                }
                engine::ConsumeEvent(event);
                handled = true;
            });
    return handled;
}

void DrawFogVolumeToolOverlay(SectorEditorToolContext& context)
{
    if (context.currentTool != SectorEditorTool::AuthoringFogVolume
            || !context.currentSnappedSectorPoint || !context.mapToScreen) {
        return;
    }
    const Vector2 center = context.mapToScreen(
            SectorPointToVector2(context.currentSnappedSectorPoint()));
    DrawCircleV(center, 8.0f, Color{105, 180, 150, 100});
    DrawCircleLines(
            static_cast<int>(center.x),
            static_cast<int>(center.y),
            8.0f,
            Color{120, 230, 190, 230});
}

const SectorEditorToolModule Module{
        SectorEditorTool::AuthoringFogVolume,
        "Fog Volume",
        nullptr,
        nullptr,
        nullptr,
        UpdateFogVolumeTool,
        DrawFogVolumeToolOverlay,
        nullptr};

} // namespace

const SectorEditorToolModule& SectorEditorFogVolumeToolModule()
{
    return Module;
}

} // namespace game
