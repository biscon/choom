#include "sector_editor/tools/line/SectorEditorLineTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <raylib.h>

#include <cmath>
#include <string>

namespace game {

namespace {

bool CancelLineTool(SectorEditorToolContext& context, const char* message)
{
    const bool wasActive = context.state.pendingAuthoringLine.active;
    if (context.cancelAuthoringLineChain) {
        context.cancelAuthoringLineChain();
    }
    if (message != nullptr && message[0] != '\0') {
        context.statusText = message;
    }
    return wasActive;
}

void CommitLinePoint(SectorEditorToolContext& context, SectorPoint point)
{
    if (context.clearTopologySelectionOnly) {
        context.clearTopologySelectionOnly();
    }

    std::string error;
    SectorTopologyCoordPoint topologyPoint;
    if (!context.toTopologyCoordPoint
            || !context.toTopologyCoordPoint(point, topologyPoint, error)) {
        context.state.pendingAuthoringLine.errorMessage = error;
        context.statusText = error;
        return;
    }

    if (!context.commitAuthoringLinePoint) {
        context.statusText = "Authoring line segment rejected";
        return;
    }

    const SectorEditorAuthoringLineToolClickResult result =
            context.commitAuthoringLinePoint(topologyPoint);
    switch (result.status) {
        case SectorEditorAuthoringLineToolClickStatus::StartedChain:
            context.statusText = "Line: click next point, Esc/right click stops chain";
            return;
        case SectorEditorAuthoringLineToolClickStatus::CreatedSegment:
            if (context.clearSelection) {
                context.clearSelection();
            }
            if (context.selectAuthoringLine) {
                context.selectAuthoringLine(result.segment.lineId);
            }
            context.statusText = "Created authoring line segment";
            return;
        case SectorEditorAuthoringLineToolClickStatus::ZeroLength:
            context.statusText = context.state.pendingAuthoringLine.errorMessage;
            return;
        case SectorEditorAuthoringLineToolClickStatus::Rejected:
            context.statusText = context.state.pendingAuthoringLine.errorMessage.empty()
                    ? "Authoring line segment rejected"
                    : context.state.pendingAuthoringLine.errorMessage;
            return;
    }
}

bool UpdateLineTool(SectorEditorToolContext& context)
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
                    if (context.state.pendingAuthoringLine.active) {
                        CancelLineTool(context, "Line chain stopped");
                        engine::ConsumeEvent(event);
                        handled = true;
                    }
                    return;
                }

                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }

                if (context.currentSnappedSectorPoint) {
                    CommitLinePoint(context, context.currentSnappedSectorPoint());
                    engine::ConsumeEvent(event);
                    handled = true;
                }
            });
    return handled;
}

void DrawLineToolOverlay(SectorEditorToolContext& context)
{
    if (!context.state.pendingAuthoringLine.active
            || !context.currentSnappedSectorPoint
            || !context.mapToScreen) {
        return;
    }

    const SectorPoint start = SectorTopologyCoordPointToSectorPoint(
            context.state.pendingAuthoringLine.startPoint);
    const SectorPoint cursor = context.currentSnappedSectorPoint();
    const bool invalid = SamePoint(start, cursor)
            || !context.state.pendingAuthoringLine.errorMessage.empty();
    const Color lineColor = invalid ? Color{220, 88, 88, 190} : Color{122, 220, 244, 205};
    const Color startColor = Color{245, 226, 154, 255};
    const Color cursorColor = invalid ? Color{220, 88, 88, 255} : Color{120, 230, 154, 255};
    const Vector2 startScreen = context.mapToScreen(SectorPointToVector2(start));
    const Vector2 cursorScreen = context.mapToScreen(SectorPointToVector2(cursor));

    if (!SamePoint(start, cursor)) {
        DrawLineEx(startScreen, cursorScreen, 3.0f, lineColor);
    }
    DrawCircleV(startScreen, 5.5f, startColor);
    DrawCircleLines(
            static_cast<int>(std::round(startScreen.x)),
            static_cast<int>(std::round(startScreen.y)),
            8.0f,
            Color{20, 24, 32, 255});
    DrawCircleV(cursorScreen, 5.0f, cursorColor);
    DrawCircleLines(
            static_cast<int>(std::round(cursorScreen.x)),
            static_cast<int>(std::round(cursorScreen.y)),
            7.5f,
            Color{20, 24, 32, 255});
}

const SectorEditorToolModule LineModule{
        SectorEditorTool::AuthoringLine,
        "Authoring Line",
        nullptr,
        UpdateLineTool,
        DrawLineToolOverlay,
        CancelLineTool};

} // namespace

const SectorEditorToolModule& SectorEditorLineToolModule()
{
    return LineModule;
}

} // namespace game
