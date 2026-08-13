#include "sector_editor/services/triggers/SectorEditorTriggerEditingService.h"

#include "sector_demo/SectorTriggers.h"
#include "sector_demo/SectorTopologyUnits.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game {
namespace {

float DistanceSquaredToSegment(Vector2 p, Vector2 a, Vector2 b)
{
    const float dx = b.x - a.x;
    const float dz = b.y - a.y;
    const float length2 = dx * dx + dz * dz;
    const float t = length2 > 0.0f
            ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dz) / length2, 0.0f, 1.0f)
            : 0.0f;
    const float px = p.x - (a.x + dx * t);
    const float pz = p.y - (a.y + dz * t);
    return px * px + pz * pz;
}

} // namespace

bool SectorEditorTriggerHitTest(
        const SectorAuthoringTrigger& trigger,
        Vector2 mapPoint,
        float outlineToleranceMap)
{
    if (SectorTriggerContainsAuthoringPoint(trigger.points, mapPoint.x, mapPoint.y)) return true;
    const float tolerance2 = outlineToleranceMap * outlineToleranceMap;
    for (size_t i = 0; i < trigger.points.size(); ++i) {
        const SectorTriggerPoint a = trigger.points[i];
        const SectorTriggerPoint b = trigger.points[(i + 1) % trigger.points.size()];
        if (DistanceSquaredToSegment(
                    mapPoint,
                    Vector2{SectorCoordToVisibleAuthoring(a.x), SectorCoordToVisibleAuthoring(a.z)},
                    Vector2{SectorCoordToVisibleAuthoring(b.x), SectorCoordToVisibleAuthoring(b.z)})
                <= tolerance2) {
            return true;
        }
    }
    return false;
}

SectorEditorTriggerEditingService::SectorEditorTriggerEditingService(
        SectorEditorTriggerEditingServiceContext context)
    : context_(std::move(context))
{
}

SectorAuthoringTrigger* SectorEditorTriggerEditingService::Selected()
{
    return context_.selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Trigger
            ? FindSectorAuthoringTrigger(context_.authoringGraph,
                    context_.selectionState.selectedAuthoring.triggerId)
            : nullptr;
}

const SectorAuthoringTrigger* SectorEditorTriggerEditingService::Selected() const
{
    return context_.selectionState.selectedAuthoring.kind == SectorAuthoringSelectionKind::Trigger
            ? FindSectorAuthoringTrigger(context_.authoringGraph,
                    context_.selectionState.selectedAuthoring.triggerId)
            : nullptr;
}

bool SectorEditorTriggerEditingService::CommitGraphMutation(
        const char* successStatus,
        const char* failureStatus)
{
    MarkSectorEditorAuthoringGraphEdited(
            context_.lifecycle, context_.topologyRenderRevision,
            context_.topologyRenderCache, context_.derivation, successStatus);
    const bool refreshed = RefreshSectorEditorAuthoringDerivation(
            context_.lifecycle, context_.topologyRenderRevision,
            context_.topologyRenderCache, context_.topologyMap,
            context_.authoringGraph, context_.derivation,
            successStatus, failureStatus);
    context_.statusText = context_.derivation.authoringDerivationStatus;
    return refreshed;
}

bool SectorEditorTriggerEditingService::Place(
        SectorTriggerShapeKind shape,
        const std::vector<SectorTriggerPoint>& points,
        int* outId)
{
    std::string error;
    if (!ValidateSectorTriggerPolygon(points, shape, &error)) {
        context_.statusText = "Trigger creation failed: " + error;
        return false;
    }
    SectorAuthoringTrigger trigger;
    trigger.editorId = AllocateSectorAuthoringTriggerId(context_.authoringGraph);
    trigger.id = AllocateSectorAuthoringTriggerReferenceId(context_.authoringGraph);
    trigger.shape = shape;
    trigger.points = points;
    if (!IsValidSectorAuthoringId(trigger.editorId) || trigger.id.empty()) {
        context_.statusText = "Trigger creation failed: no trigger identity is available";
        return false;
    }
    context_.authoringGraph.triggers.push_back(std::move(trigger));
    SelectSectorEditorAuthoringTrigger(context_.authoringGraph, context_.selectionState,
            context_.authoringGraph.triggers.back().editorId);
    if (outId != nullptr) *outId = context_.authoringGraph.triggers.back().editorId;
    return CommitGraphMutation("Created trigger", "Trigger created; derivation failed");
}

bool SectorEditorTriggerEditingService::MutateSelected(
        const char* status,
        const std::function<bool(SectorAuthoringTrigger&)>& mutate)
{
    SectorAuthoringTrigger* trigger = Selected();
    if (trigger == nullptr || !mutate || !mutate(*trigger)) return false;
    return CommitGraphMutation(status, "Trigger edit saved; derivation failed");
}

bool SectorEditorTriggerEditingService::RenameSelected(const std::string& id, std::string& error)
{
    const SectorAuthoringTrigger* selected = Selected();
    if (selected == nullptr) { error = "No trigger is selected"; return false; }
    if (!IsValidSectorTriggerReferenceId(id)) {
        error = "Use 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    const SectorAuthoringTrigger* existing = FindSectorAuthoringTriggerByReferenceId(context_.authoringGraph, id);
    if (existing != nullptr && existing->editorId != selected->editorId) {
        error = "Trigger ID must be unique inside the level";
        return false;
    }
    error.clear();
    if (id == selected->id) return true;
    return MutateSelected("Renamed trigger", [&id](SectorAuthoringTrigger& trigger) {
        trigger.id = id; return true;
    });
}

bool SectorEditorTriggerEditingService::SetSelectedEnabled(bool enabled)
{
    return MutateSelected("Updated trigger enabled state", [enabled](SectorAuthoringTrigger& trigger) {
        if (trigger.enabled == enabled) return false; trigger.enabled = enabled; return true;
    });
}

bool SectorEditorTriggerEditingService::SetSelectedRepeat(bool repeat)
{
    return MutateSelected("Updated trigger repeat state", [repeat](SectorAuthoringTrigger& trigger) {
        if (trigger.repeat == repeat) return false; trigger.repeat = repeat; return true;
    });
}

bool SectorEditorTriggerEditingService::SetSelectedDelay(int milliseconds)
{
    if (milliseconds < 0) { context_.statusText = "Trigger delay must be non-negative"; return false; }
    return MutateSelected("Updated trigger delay", [milliseconds](SectorAuthoringTrigger& trigger) {
        if (trigger.delayMilliseconds == milliseconds) return false;
        trigger.delayMilliseconds = milliseconds; return true;
    });
}

bool SectorEditorTriggerEditingService::SetSelectedScript(const std::string& script, std::string& error)
{
    if (!IsValidSectorTriggerScriptName(script)) {
        error = "Use an empty or valid Lua global function name";
        return false;
    }
    error.clear();
    const SectorAuthoringTrigger* selected = Selected();
    if (selected == nullptr) { error = "No trigger is selected"; return false; }
    if (selected->script == script) return true;
    return MutateSelected("Updated trigger script", [&script](SectorAuthoringTrigger& trigger) {
        trigger.script = script; return true;
    });
}

bool SectorEditorTriggerEditingService::DeleteSelected()
{
    const SectorAuthoringTrigger* selected = Selected();
    if (selected == nullptr) return false;
    const int id = selected->editorId;
    context_.authoringGraph.triggers.erase(
            std::remove_if(context_.authoringGraph.triggers.begin(), context_.authoringGraph.triggers.end(),
                    [id](const SectorAuthoringTrigger& trigger) { return trigger.editorId == id; }),
            context_.authoringGraph.triggers.end());
    context_.editingState.drag = TriggerDragState{};
    ClearSectorEditorAuthoringSelection(context_.selectionState);
    return CommitGraphMutation("Deleted trigger", "Trigger deleted; derivation failed");
}

bool SectorEditorTriggerEditingService::BeginMove(int triggerId, SectorTriggerPoint pressPoint)
{
    const SectorAuthoringTrigger* trigger = FindSectorAuthoringTrigger(context_.authoringGraph, triggerId);
    if (trigger == nullptr) return false;
    SelectSectorEditorAuthoringTrigger(context_.authoringGraph, context_.selectionState, triggerId);
    TriggerDragState& drag = context_.editingState.drag;
    drag = TriggerDragState{};
    drag.active = true;
    drag.triggerId = triggerId;
    drag.originalPoints = trigger->points;
    drag.previewPoints = trigger->points;
    drag.pressPoint = pressPoint;
    context_.statusText = "Moving trigger " + trigger->id;
    return true;
}

void SectorEditorTriggerEditingService::UpdateMove(SectorTriggerPoint point)
{
    TriggerDragState& drag = context_.editingState.drag;
    if (!drag.active) return;
    const SectorCoord dx = point.x - drag.pressPoint.x;
    const SectorCoord dz = point.z - drag.pressPoint.z;
    for (size_t i = 0; i < drag.previewPoints.size(); ++i) {
        drag.previewPoints[i].x = drag.originalPoints[i].x + dx;
        drag.previewPoints[i].z = drag.originalPoints[i].z + dz;
    }
}

bool SectorEditorTriggerEditingService::FinishMove()
{
    TriggerDragState drag = context_.editingState.drag;
    if (!drag.active) return false;
    context_.editingState.drag = TriggerDragState{};
    const bool unchanged = drag.previewPoints.size() == drag.originalPoints.size()
            && std::equal(drag.previewPoints.begin(), drag.previewPoints.end(),
                    drag.originalPoints.begin(), [](SectorTriggerPoint a, SectorTriggerPoint b) {
                        return a.x == b.x && a.z == b.z;
                    });
    if (unchanged) { context_.statusText = "Trigger move unchanged"; return true; }
    return MutateSelected("Moved trigger", [&drag](SectorAuthoringTrigger& trigger) {
        trigger.points = std::move(drag.previewPoints); return true;
    });
}

void SectorEditorTriggerEditingService::CancelMove(const char* message)
{
    context_.editingState.drag = TriggerDragState{};
    if (message != nullptr && message[0] != '\0') context_.statusText = message;
}

int SectorEditorTriggerEditingService::FindAtMapPoint(Vector2 mapPoint, float outlineToleranceMap) const
{
    int best = -1;
    for (const SectorAuthoringTrigger& trigger : context_.authoringGraph.triggers) {
        if (SectorEditorTriggerHitTest(trigger, mapPoint, outlineToleranceMap)) best = trigger.editorId;
    }
    return best;
}

} // namespace game
