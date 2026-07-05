#include "sector_editor/tools/insert_vertex/SectorEditorInsertVertexTool.h"

#include "engine/input/Input.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <raylib.h>

#include <cmath>
#include <string>

namespace game {

namespace {

const char* InsertVertexFailureStatus(SectorAuthoringInsertVertexStatus status)
{
    switch (status) {
        case SectorAuthoringInsertVertexStatus::Inserted:
            return "Inserted vertex on authoring line";
        case SectorAuthoringInsertVertexStatus::InvalidLine:
            return "Insert Vertex: select or click an authoring line";
        case SectorAuthoringInsertVertexStatus::InvalidEndpoint:
            return "Insert Vertex unavailable: selected authoring line is invalid";
        case SectorAuthoringInsertVertexStatus::OffLine:
            return "Insert point must lie on the selected line";
        case SectorAuthoringInsertVertexStatus::Endpoint:
            return "Insert point is too close to an endpoint";
        case SectorAuthoringInsertVertexStatus::IdAllocationFailed:
            return "Insert Vertex failed: could not allocate authoring IDs";
    }
    return "Insert Vertex failed";
}

bool CancelInsertVertexTool(SectorEditorToolContext& context, const char* message)
{
    const bool wasActive = context.state.pendingAuthoringInsertVertex.active
            || context.state.currentTool == SectorEditorTool::AuthoringInsertVertex;
    context.state.pendingAuthoringInsertVertex = PendingAuthoringInsertVertex{};
    if (message != nullptr && message[0] != '\0') {
        context.statusText = message;
    }
    return wasActive;
}

void UpdateInsertVertexToolHover(SectorEditorToolContext& context, Vector2 mapPoint)
{
    PendingAuthoringInsertVertex& pending = context.state.pendingAuthoringInsertVertex;
    pending.hasPreviewPoint = false;
    pending.errorMessage.clear();

    int lineId = pending.active ? pending.lineId : -1;
    if (!pending.active) {
        if (!context.mapToScreen || !context.findAuthoringLineNearScreenPoint) {
            pending.lineId = -1;
            pending.errorMessage = "Insert Vertex: select or click an authoring line";
            return;
        }
        lineId = context.findAuthoringLineNearScreenPoint(context.mapToScreen(mapPoint));
    }

    if (FindSectorAuthoringLine(context.state.authoringGraph, lineId) == nullptr) {
        pending.lineId = -1;
        pending.errorMessage = "Insert Vertex: select or click an authoring line";
        return;
    }

    pending.lineId = lineId;
    if (context.hoverAuthoringLine) {
        context.hoverAuthoringLine(lineId);
    }

    SectorTopologyCoordPoint point;
    std::string error;
    if (!context.resolveAuthoringInsertVertexPoint
            || !context.resolveAuthoringInsertVertexPoint(lineId, mapPoint, point, error)) {
        pending.errorMessage = error;
        return;
    }

    pending.previewPoint = point;
    pending.hasPreviewPoint = true;
}

void CommitInsertVertexTool(SectorEditorToolContext& context, Vector2 screenPoint)
{
    const int lineId = context.state.pendingAuthoringInsertVertex.active
            ? context.state.pendingAuthoringInsertVertex.lineId
            : (context.findAuthoringLineNearScreenPoint
                    ? context.findAuthoringLineNearScreenPoint(screenPoint)
                    : -1);
    if (FindSectorAuthoringLine(context.state.authoringGraph, lineId) == nullptr) {
        context.statusText = "Insert Vertex: select or click an authoring line";
        return;
    }

    SectorTopologyCoordPoint point;
    std::string error;
    if (!context.screenToMap
            || !context.resolveAuthoringInsertVertexPoint
            || !context.resolveAuthoringInsertVertexPoint(
                    lineId,
                    context.screenToMap(screenPoint),
                    point,
                    error)) {
        context.state.pendingAuthoringInsertVertex.errorMessage = error;
        context.statusText = error;
        return;
    }

    SectorAuthoringInsertVertexResult result;
    if (!context.commitAuthoringInsertVertex
            || !context.commitAuthoringInsertVertex(lineId, point, &result)) {
        context.statusText = InsertVertexFailureStatus(result.status);
        context.state.pendingAuthoringInsertVertex.errorMessage = context.statusText;
        return;
    }

    context.state.pendingAuthoringInsertVertex = PendingAuthoringInsertVertex{};
    context.statusText = "Inserted vertex on authoring line";
}

bool UpdateInsertVertexTool(SectorEditorToolContext& context)
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
                    if (context.state.pendingAuthoringInsertVertex.active
                            || context.state.currentTool == SectorEditorTool::AuthoringInsertVertex) {
                        CancelInsertVertexTool(context, "Insert Vertex cancelled");
                        engine::ConsumeEvent(event);
                        handled = true;
                    }
                    return;
                }

                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }

                CommitInsertVertexTool(context, event.mouseClick.releasePosition);
                engine::ConsumeEvent(event);
                handled = true;
            });
    return handled;
}

void DrawInsertVertexToolOverlay(SectorEditorToolContext& context)
{
    if (context.state.currentTool != SectorEditorTool::AuthoringInsertVertex
            && !context.state.pendingAuthoringInsertVertex.active) {
        return;
    }
    if (!context.mapToScreen) {
        return;
    }

    const PendingAuthoringInsertVertex& pending = context.state.pendingAuthoringInsertVertex;
    const SectorAuthoringLine* line =
            FindSectorAuthoringLine(context.state.authoringGraph, pending.lineId);
    if (line == nullptr) {
        return;
    }
    const SectorAuthoringVertex* start =
            FindSectorAuthoringVertex(context.state.authoringGraph, line->startVertexId);
    const SectorAuthoringVertex* end =
            FindSectorAuthoringVertex(context.state.authoringGraph, line->endVertexId);
    if (start == nullptr || end == nullptr) {
        return;
    }

    const bool invalid = !pending.hasPreviewPoint || !pending.errorMessage.empty();
    const Color lineColor = invalid ? Color{220, 88, 88, 170} : Color{122, 220, 244, 205};
    const Color pointColor = invalid ? Color{220, 88, 88, 255} : Color{120, 230, 154, 255};
    const Vector2 startScreen = context.mapToScreen(Vector2{
            SectorCoordToVisibleAuthoring(start->x),
            SectorCoordToVisibleAuthoring(start->y)});
    const Vector2 endScreen = context.mapToScreen(Vector2{
            SectorCoordToVisibleAuthoring(end->x),
            SectorCoordToVisibleAuthoring(end->y)});
    DrawLineEx(startScreen, endScreen, 5.0f, lineColor);

    if (!pending.hasPreviewPoint) {
        return;
    }

    const Vector2 point = context.mapToScreen(Vector2{
            SectorCoordToVisibleAuthoring(pending.previewPoint.x),
            SectorCoordToVisibleAuthoring(pending.previewPoint.y)});
    DrawCircleV(point, 5.0f, pointColor);
    DrawCircleLines(
            static_cast<int>(std::round(point.x)),
            static_cast<int>(std::round(point.y)),
            9.0f,
            Color{20, 24, 32, 255});
    DrawLineEx(Vector2{point.x - 10.0f, point.y}, Vector2{point.x + 10.0f, point.y}, 2.0f, pointColor);
    DrawLineEx(Vector2{point.x, point.y - 10.0f}, Vector2{point.x, point.y + 10.0f}, 2.0f, pointColor);
    DrawLineEx(Vector2{point.x - 9.0f, point.y}, Vector2{point.x + 9.0f, point.y}, 2.0f, Color{235, 224, 130, 255});
    DrawLineEx(Vector2{point.x, point.y - 9.0f}, Vector2{point.x, point.y + 9.0f}, 2.0f, Color{235, 224, 130, 255});
}

const SectorEditorToolModule InsertVertexModule{
        SectorEditorTool::AuthoringInsertVertex,
        "Authoring Insert Vertex",
        UpdateInsertVertexToolHover,
        nullptr,
        nullptr,
        UpdateInsertVertexTool,
        DrawInsertVertexToolOverlay,
        CancelInsertVertexTool};

} // namespace

const SectorEditorToolModule& SectorEditorInsertVertexToolModule()
{
    return InsertVertexModule;
}

} // namespace game
