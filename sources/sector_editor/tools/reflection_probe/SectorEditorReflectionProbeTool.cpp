#include "sector_editor/tools/reflection_probe/SectorEditorReflectionProbeTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/tools/SectorEditorToolModule.h"

namespace game {
namespace {

bool UpdateReflectionProbeTool(SectorEditorToolContext& context)
{
    if (context.input == nullptr || context.reflectionProbeEditing == nullptr) return false;
    bool handled = false;
    context.input->ForEachEvent(engine::InputEventType::MouseClick, true,
            [&](engine::InputEvent& event) {
                if (handled || event.mouseClick.button != MOUSE_LEFT_BUTTON
                        || !CheckCollisionPointRec(event.mouseClick.releasePosition, context.canvasRect)
                        || !context.currentSnappedSectorPoint || !context.toTopologyCoordPoint) return;
                SectorTopologyCoordPoint point;
                std::string error;
                if (context.toTopologyCoordPoint(context.currentSnappedSectorPoint(), point, error)) {
                    context.reflectionProbeEditing->Place(point);
                } else {
                    context.statusText = error;
                }
                engine::ConsumeEvent(event);
                handled = true;
            });
    return handled;
}

void DrawReflectionProbeToolOverlay(SectorEditorToolContext& context)
{
    if (context.currentTool != SectorEditorTool::ReflectionProbe
            || !context.currentSnappedSectorPoint || !context.mapToScreen) return;
    const Vector2 center = context.mapToScreen(
            SectorPointToVector2(context.currentSnappedSectorPoint()));
    DrawCircleV(center, 8.0f, Color{190, 110, 255, 100});
    DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
            8.0f, Color{220, 160, 255, 240});
}

const SectorEditorToolModule Module{
        SectorEditorTool::ReflectionProbe,
        "Reflection Probe",
        nullptr, nullptr, nullptr,
        UpdateReflectionProbeTool,
        DrawReflectionProbeToolOverlay,
        nullptr};

} // namespace

const SectorEditorToolModule& SectorEditorReflectionProbeToolModule()
{
    return Module;
}

} // namespace game
