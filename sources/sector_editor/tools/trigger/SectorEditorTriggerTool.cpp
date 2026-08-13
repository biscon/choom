#include "sector_editor/tools/trigger/SectorEditorTriggerTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"
#include "sector_demo/SectorTriggers.h"
#include "sector_demo/SectorTopologyUnits.h"

#include <cmath>

namespace game {
namespace {

bool Same(SectorTriggerPoint a, SectorTriggerPoint b)
{
    return a.x == b.x && a.z == b.z;
}

void CancelDraw(SectorEditorToolContext& context, const char* message)
{
    if (context.triggerEditingState != nullptr) {
        context.triggerEditingState->pending = PendingTriggerDrawState{};
    }
    if (message != nullptr) context.statusText = message;
}

bool CancelTriggerTool(SectorEditorToolContext& context, const char* message)
{
    const bool active = context.triggerEditingState != nullptr
            && context.triggerEditingState->pending.active;
    CancelDraw(context, message);
    return active;
}

bool ToPoint(SectorEditorToolContext& context, SectorTriggerPoint& out)
{
    if (!context.currentSnappedSectorPoint || !context.toTopologyCoordPoint) return false;
    SectorTopologyCoordPoint point;
    std::string error;
    if (!context.toTopologyCoordPoint(context.currentSnappedSectorPoint(), point, error)) {
        context.statusText = error;
        return false;
    }
    out = SectorTriggerPoint{point.x, point.y};
    return true;
}

bool CommitShape(SectorEditorToolContext& context, SectorTriggerShapeKind shape,
                 const std::vector<SectorTriggerPoint>& points)
{
    if (context.triggerEditing == nullptr) return false;
    if (!context.triggerEditing->Place(shape, points)) return false;
    context.triggerEditingState->pending = PendingTriggerDrawState{};
    return true;
}

void Click(SectorEditorToolContext& context)
{
    TriggerEditingState& state = *context.triggerEditingState;
    SectorTriggerPoint point;
    if (!ToPoint(context, point)) return;
    if (state.drawMode == TriggerDrawMode::Rectangle) {
        if (!state.pending.active) {
            state.pending.active = true;
            state.pending.points = {point};
            context.statusText = "Trigger rectangle: click opposite corner, right click/Esc cancels";
            return;
        }
        const SectorTriggerPoint first = state.pending.points.front();
        const std::vector<SectorTriggerPoint> rectangle{
                first, {point.x, first.z}, point, {first.x, point.z}};
        CommitShape(context, SectorTriggerShapeKind::Rectangle, rectangle);
        return;
    }

    if (!state.pending.active) {
        state.pending.active = true;
        state.pending.points = {point};
        context.statusText = "Trigger polygon: click points; click first point to close";
        return;
    }
    if (state.pending.points.size() >= 3 && Same(point, state.pending.points.front())) {
        CommitShape(context, SectorTriggerShapeKind::Polygon, state.pending.points);
        return;
    }
    if (Same(point, state.pending.points.back())) {
        context.statusText = "Trigger polygon point must differ from the previous point";
        return;
    }
    std::vector<SectorTriggerPoint> candidate = state.pending.points;
    candidate.push_back(point);
    if (candidate.size() >= 4) {
        std::string error;
        if (!ValidateSectorTriggerPolygon(candidate, SectorTriggerShapeKind::Polygon, &error)
                && error.find("self-intersect") != std::string::npos) {
            context.statusText = error;
            return;
        }
    }
    state.pending.points.push_back(point);
}

void UpdateHover(SectorEditorToolContext& context, Vector2)
{
    if (context.triggerEditingState == nullptr || !context.triggerEditingState->pending.active) return;
    SectorTriggerPoint point;
    if (ToPoint(context, point)) {
        context.triggerEditingState->pending.hoverPoint = point;
        context.triggerEditingState->pending.hasHoverPoint = true;
    }
}

bool Update(SectorEditorToolContext& context)
{
    if (context.input == nullptr || context.triggerEditingState == nullptr) return false;
    bool handled = false;
    context.input->ForEachEvent(engine::InputEventType::MouseClick, true,
            [&context, &handled](engine::InputEvent& event) {
                if (handled || !CheckCollisionPointRec(event.mouseClick.releasePosition, context.canvasRect)) return;
                if (event.mouseClick.button == MOUSE_RIGHT_BUTTON) {
                    if (context.triggerEditingState->pending.active) {
                        CancelDraw(context, "Trigger drawing cancelled");
                        engine::ConsumeEvent(event); handled = true;
                    }
                } else if (event.mouseClick.button == MOUSE_LEFT_BUTTON) {
                    Click(context); engine::ConsumeEvent(event); handled = true;
                }
            });
    return handled;
}

Vector2 ScreenPoint(SectorEditorToolContext& context, SectorTriggerPoint point)
{
    return context.mapToScreen(Vector2{
            SectorCoordToVisibleAuthoring(point.x), SectorCoordToVisibleAuthoring(point.z)});
}

void DrawOverlay(SectorEditorToolContext& context)
{
    if (context.triggerEditingState == nullptr || !context.triggerEditingState->pending.active
            || !context.mapToScreen) return;
    const TriggerEditingState& state = *context.triggerEditingState;
    const Color color{255, 72, 184, 230};
    if (state.drawMode == TriggerDrawMode::Rectangle && !state.pending.points.empty()
            && state.pending.hasHoverPoint) {
        const SectorTriggerPoint first = state.pending.points.front();
        const SectorTriggerPoint point = state.pending.hoverPoint;
        const SectorTriggerPoint corners[4]{first, {point.x, first.z}, point, {first.x, point.z}};
        for (int i = 0; i < 4; ++i) DrawLineEx(ScreenPoint(context, corners[i]),
                ScreenPoint(context, corners[(i + 1) % 4]), 3.0f, color);
        return;
    }
    for (size_t i = 1; i < state.pending.points.size(); ++i) {
        DrawLineEx(ScreenPoint(context, state.pending.points[i - 1]),
                ScreenPoint(context, state.pending.points[i]), 3.0f, color);
    }
    if (!state.pending.points.empty() && state.pending.hasHoverPoint) {
        DrawLineEx(ScreenPoint(context, state.pending.points.back()),
                ScreenPoint(context, state.pending.hoverPoint), 2.0f, color);
    }
    for (SectorTriggerPoint point : state.pending.points) DrawCircleV(ScreenPoint(context, point), 5.0f, color);
}

const SectorEditorToolModule Module{
        SectorEditorTool::Trigger, "Trigger", UpdateHover, nullptr, nullptr,
        Update, DrawOverlay, CancelTriggerTool};

} // namespace

const SectorEditorToolModule& SectorEditorTriggerToolModule() { return Module; }
} // namespace game
