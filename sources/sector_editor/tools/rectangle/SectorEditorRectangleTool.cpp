#include "sector_editor/tools/rectangle/SectorEditorRectangleTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace game {

namespace {

bool CancelRectangleTool(SectorEditorToolContext& context, const char* message)
{
    const bool wasActive = context.state.pendingAuthoringRectangle.active;
    context.state.pendingAuthoringRectangle = PendingAuthoringRectangleDraw{};
    if (message != nullptr && message[0] != '\0') {
        context.statusText = message;
    }
    return wasActive;
}

void CommitRectanglePoint(SectorEditorToolContext& context, SectorPoint point)
{
    if (context.clearTopologySelectionOnly) {
        context.clearTopologySelectionOnly();
    }

    std::string error;
    SectorTopologyCoordPoint topologyPoint;
    if (!context.toTopologyCoordPoint
            || !context.toTopologyCoordPoint(point, topologyPoint, error)) {
        context.state.pendingAuthoringRectangle.errorMessage = error;
        context.statusText = error;
        return;
    }

    if (!context.state.pendingAuthoringRectangle.active) {
        context.state.pendingAuthoringRectangle.active = true;
        context.state.pendingAuthoringRectangle.firstCorner = topologyPoint;
        context.state.pendingAuthoringRectangle.currentCorner = topologyPoint;
        context.state.pendingAuthoringRectangle.errorMessage.clear();
        context.statusText = "Rectangle: click opposite corner, right click/Esc cancels";
        return;
    }

    SectorEditorAuthoringRectangleResult result;
    if (!context.commitAuthoringRectangle
            || !context.commitAuthoringRectangle(
                    context.state.pendingAuthoringRectangle.firstCorner,
                    topologyPoint,
                    &result)) {
        context.state.pendingAuthoringRectangle.currentCorner = topologyPoint;
        context.state.pendingAuthoringRectangle.errorMessage =
                "Rectangle needs non-zero width and height";
        context.statusText = context.state.pendingAuthoringRectangle.errorMessage;
        return;
    }

    context.state.pendingAuthoringRectangle = PendingAuthoringRectangleDraw{};
    if (context.clearSelection) {
        context.clearSelection();
    }
    if (context.selectAuthoringLine) {
        context.selectAuthoringLine(result.lineIds[0]);
    }
    context.statusText = "Created authoring rectangle";
}

bool UpdateRectangleTool(SectorEditorToolContext& context)
{
    if (context.input == nullptr) {
        return false;
    }

    bool handled = false;
    context.input->ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [&context, &handled](engine::InputEvent& event) {
                if (handled || !CheckCollisionPointRec(event.mouseClick.releasePosition, context.canvasRect)) {
                    return;
                }

                if (event.mouseClick.button == MOUSE_RIGHT_BUTTON) {
                    if (context.state.pendingAuthoringRectangle.active) {
                        CancelRectangleTool(context, "Rectangle cancelled");
                        engine::ConsumeEvent(event);
                        handled = true;
                    }
                    return;
                }

                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }

                if (context.currentSnappedSectorPoint) {
                    CommitRectanglePoint(context, context.currentSnappedSectorPoint());
                    engine::ConsumeEvent(event);
                    handled = true;
                }
            });
    return handled;
}

void DrawRectangleToolOverlay(SectorEditorToolContext& context)
{
    if (!context.state.pendingAuthoringRectangle.active || !context.mapToScreen) {
        return;
    }

    const SectorPoint first = SectorTopologyCoordPointToSectorPoint(
            context.state.pendingAuthoringRectangle.firstCorner);
    const SectorPoint cursor = SectorTopologyCoordPointToSectorPoint(
            context.state.pendingAuthoringRectangle.currentCorner);
    const bool invalid = first.x == cursor.x
            || first.y == cursor.y
            || !context.state.pendingAuthoringRectangle.errorMessage.empty();
    const Color lineColor = invalid ? Color{220, 88, 88, 190} : Color{122, 220, 244, 205};
    const Color firstColor = Color{245, 226, 154, 255};
    const Color cursorColor = invalid ? Color{220, 88, 88, 255} : Color{120, 230, 154, 255};

    const float minX = std::min(first.x, cursor.x);
    const float maxX = std::max(first.x, cursor.x);
    const float minY = std::min(first.y, cursor.y);
    const float maxY = std::max(first.y, cursor.y);
    const Vector2 a = context.mapToScreen(Vector2{minX, minY});
    const Vector2 b = context.mapToScreen(Vector2{maxX, minY});
    const Vector2 c = context.mapToScreen(Vector2{maxX, maxY});
    const Vector2 d = context.mapToScreen(Vector2{minX, maxY});
    if (!invalid) {
        DrawLineEx(a, b, 3.0f, lineColor);
        DrawLineEx(b, c, 3.0f, lineColor);
        DrawLineEx(c, d, 3.0f, lineColor);
        DrawLineEx(d, a, 3.0f, lineColor);
    } else if (!SamePoint(first, cursor)) {
        DrawLineEx(
                context.mapToScreen(SectorPointToVector2(first)),
                context.mapToScreen(SectorPointToVector2(cursor)),
                3.0f,
                lineColor);
    }

    const Vector2 firstScreen = context.mapToScreen(SectorPointToVector2(first));
    const Vector2 cursorScreen = context.mapToScreen(SectorPointToVector2(cursor));
    DrawCircleV(firstScreen, 5.5f, firstColor);
    DrawCircleLines(
            static_cast<int>(std::round(firstScreen.x)),
            static_cast<int>(std::round(firstScreen.y)),
            8.0f,
            Color{20, 24, 32, 255});
    DrawCircleV(cursorScreen, 5.0f, cursorColor);
    DrawCircleLines(
            static_cast<int>(std::round(cursorScreen.x)),
            static_cast<int>(std::round(cursorScreen.y)),
            7.5f,
            Color{20, 24, 32, 255});
}

const SectorEditorToolModule RectangleModule{
        SectorEditorTool::AuthoringRectangle,
        "Authoring Rectangle",
        nullptr,
        UpdateRectangleTool,
        DrawRectangleToolOverlay,
        CancelRectangleTool};

} // namespace

const SectorEditorToolModule& SectorEditorRectangleToolModule()
{
    return RectangleModule;
}

} // namespace game
