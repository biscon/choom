#include "sector_editor/patrols/SectorEditorPatrolEditorService.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>

namespace game {
namespace {

bool SameWaypoint(
        const SectorAuthoringPatrolWaypoint& left,
        const SectorAuthoringPatrolWaypoint& right)
{
    return left.levelMarkerId == right.levelMarkerId
            && left.delayMilliseconds == right.delayMilliseconds
            && left.gait == right.gait
            && left.lookAround == right.lookAround
            && left.lookArcDegrees == right.lookArcDegrees;
}

bool SamePatrols(
        const std::vector<SectorAuthoringPatrol>& left,
        const std::vector<SectorAuthoringPatrol>& right)
{
    if (left.size() != right.size()) return false;
    for (size_t index = 0; index < left.size(); ++index) {
        const SectorAuthoringPatrol& a = left[index];
        const SectorAuthoringPatrol& b = right[index];
        if (a.editorId != b.editorId || a.id != b.id || a.mode != b.mode
                || a.shuffleWaypoints != b.shuffleWaypoints
                || a.faceWaypointOrientation != b.faceWaypointOrientation
                || a.waypoints.size() != b.waypoints.size()) {
            return false;
        }
        for (size_t waypoint = 0; waypoint < a.waypoints.size(); ++waypoint) {
            if (!SameWaypoint(a.waypoints[waypoint], b.waypoints[waypoint])) {
                return false;
            }
        }
    }
    return true;
}

const char* ModeLabel(SectorPatrolMode mode)
{
    switch (mode) {
        case SectorPatrolMode::Once: return "Once";
        case SectorPatrolMode::Loop: return "Loop";
        case SectorPatrolMode::PingPong: return "Ping-pong";
    }
    return "Unknown";
}

} // namespace

SectorEditorPatrolEditorService::SectorEditorPatrolEditorService(
        SectorEditorPatrolEditorState& state,
        SectorAuthoringGraph& graph,
        SectorTopologyMap& map,
        SectorEditorDerivationDocumentAccess derivation,
        SectorEditorDocumentLifecycleAccess lifecycle,
        uint64_t& topologyRenderRevision,
        SectorEditorTopologyRenderCache& topologyRenderCache,
        std::string& statusText)
    : state_(state)
    , graph_(graph)
    , map_(map)
    , derivation_(derivation)
    , lifecycle_(lifecycle)
    , topologyRenderRevision_(topologyRenderRevision)
    , topologyRenderCache_(topologyRenderCache)
    , statusText_(statusText)
{
}

void SectorEditorPatrolEditorService::Open()
{
    state_ = SectorEditorPatrolEditorState{};
    state_.open = true;
    state_.drafts.reserve(graph_.patrols.size());
    for (const SectorAuthoringPatrol& patrol : graph_.patrols) {
        state_.drafts.push_back(SectorEditorPatrolDraft{patrol});
    }
    state_.selectedIndex = state_.drafts.empty() ? -1 : 0;
    RebuildMarkerOptions();
    RebuildListLabels();
    SyncSelection();
    statusText_ = "Patrol Editor opened";
}

void SectorEditorPatrolEditorService::Close()
{
    state_ = SectorEditorPatrolEditorState{};
}

void SectorEditorPatrolEditorService::Cancel()
{
    if (!state_.open) return;
    Close();
    statusText_ = "Patrol changes discarded";
}

SectorAuthoringPatrol* SectorEditorPatrolEditorService::Selected()
{
    return state_.selectedIndex >= 0
                    && state_.selectedIndex < static_cast<int>(state_.drafts.size())
            ? &state_.drafts[static_cast<size_t>(state_.selectedIndex)].patrol
            : nullptr;
}

const SectorAuthoringPatrol* SectorEditorPatrolEditorService::Selected() const
{
    return state_.selectedIndex >= 0
                    && state_.selectedIndex < static_cast<int>(state_.drafts.size())
            ? &state_.drafts[static_cast<size_t>(state_.selectedIndex)].patrol
            : nullptr;
}

bool SectorEditorPatrolEditorService::SelectIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(state_.drafts.size())) return false;
    state_.selectedIndex = index;
    state_.validationMessage.clear();
    SyncSelection();
    return true;
}

void SectorEditorPatrolEditorService::AddPatrol()
{
    SectorAuthoringGraph identityGraph = graph_;
    identityGraph.patrols.clear();
    for (const SectorEditorPatrolDraft& draft : state_.drafts) {
        identityGraph.patrols.push_back(draft.patrol);
    }
    SectorAuthoringPatrol patrol;
    patrol.editorId = AllocateSectorAuthoringPatrolId(identityGraph);
    patrol.id = AllocateSectorAuthoringPatrolReferenceId(identityGraph);
    patrol.mode = SectorPatrolMode::Loop;
    state_.drafts.push_back(SectorEditorPatrolDraft{std::move(patrol)});
    state_.selectedIndex = static_cast<int>(state_.drafts.size()) - 1;
    state_.formScroll = {};
    state_.validationMessage = state_.markerIds.empty()
            ? "Create a Level Marker before adding a waypoint"
            : "Add at least one waypoint before saving";
    RebuildListLabels();
    SyncSelection();
}

bool SectorEditorPatrolEditorService::SelectedIsAssigned() const
{
    const SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr) return false;
    return std::any_of(map_.runtimeObjects.begin(), map_.runtimeObjects.end(),
            [patrol](const SectorPlacedRuntimeObject& object) {
                return object.kind == "npc"
                        && object.npc.patrolEditorId == patrol->editorId;
            });
}

std::string SectorEditorPatrolEditorService::SelectedUsageText() const
{
    const SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr) return {};
    std::string result;
    for (const SectorPlacedRuntimeObject& object : map_.runtimeObjects) {
        if (object.kind != "npc"
                || object.npc.patrolEditorId != patrol->editorId) continue;
        if (!result.empty()) result += ", ";
        result += object.npc.instanceId.empty()
                ? "NPC " + std::to_string(object.id)
                : object.npc.instanceId;
    }
    return result;
}

bool SectorEditorPatrolEditorService::RequestDeleteSelected()
{
    const SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr) return false;
    if (SelectedIsAssigned()) {
        state_.validationMessage = "Unassign this patrol from: "
                + SelectedUsageText();
        return false;
    }
    state_.deleteConfirmationOpen = true;
    state_.deleteConfirmationEditorId = patrol->editorId;
    return true;
}

void SectorEditorPatrolEditorService::CancelDelete()
{
    state_.deleteConfirmationOpen = false;
    state_.deleteConfirmationEditorId = -1;
}

bool SectorEditorPatrolEditorService::ConfirmDeleteSelected()
{
    if (!state_.deleteConfirmationOpen || SelectedIsAssigned()) {
        CancelDelete();
        return false;
    }
    const int index = state_.selectedIndex;
    if (index < 0 || index >= static_cast<int>(state_.drafts.size())) {
        CancelDelete();
        return false;
    }
    state_.drafts.erase(state_.drafts.begin() + index);
    state_.selectedIndex = state_.drafts.empty()
            ? -1 : std::min(index, static_cast<int>(state_.drafts.size()) - 1);
    CancelDelete();
    state_.validationMessage.clear();
    RebuildListLabels();
    SyncSelection();
    return true;
}

bool SectorEditorPatrolEditorService::ApplyIdBuffer()
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr) return false;
    const std::string id = state_.idBuffer;
    if (!IsValidSectorAuthoringPatrolReferenceId(id)) {
        state_.validationMessage =
                "ID must use 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    for (const SectorEditorPatrolDraft& draft : state_.drafts) {
        if (&draft.patrol != patrol && draft.patrol.id == id) {
            state_.validationMessage = "Patrol ID already exists";
            return false;
        }
    }
    patrol->id = id;
    state_.validationMessage.clear();
    RebuildListLabels();
    return true;
}

bool SectorEditorPatrolEditorService::SetMode(SectorPatrolMode mode)
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr || patrol->mode == mode
            || (patrol->shuffleWaypoints
                    && mode == SectorPatrolMode::PingPong)) return false;
    patrol->mode = mode;
    RebuildListLabels();
    return true;
}

bool SectorEditorPatrolEditorService::SetShuffleWaypoints(bool enabled)
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr || patrol->shuffleWaypoints == enabled) return false;
    patrol->shuffleWaypoints = enabled;
    if (enabled && patrol->mode == SectorPatrolMode::PingPong) {
        patrol->mode = SectorPatrolMode::Loop;
    }
    RebuildListLabels();
    return true;
}

bool SectorEditorPatrolEditorService::SetFaceWaypointOrientation(bool enabled)
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr
            || patrol->faceWaypointOrientation == enabled) return false;
    patrol->faceWaypointOrientation = enabled;
    return true;
}

bool SectorEditorPatrolEditorService::AddWaypoint()
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr || state_.markerIds.empty()) {
        state_.validationMessage = "Create a Level Marker before adding a waypoint";
        return false;
    }
    SectorAuthoringPatrolWaypoint waypoint;
    waypoint.levelMarkerId = state_.markerIds.front();
    patrol->waypoints.push_back(waypoint);
    state_.validationMessage.clear();
    RebuildListLabels();
    SyncSelection();
    return true;
}

bool SectorEditorPatrolEditorService::RemoveWaypoint(size_t index)
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr || index >= patrol->waypoints.size()) return false;
    patrol->waypoints.erase(patrol->waypoints.begin()
            + static_cast<std::ptrdiff_t>(index));
    RebuildListLabels();
    SyncSelection();
    return true;
}

bool SectorEditorPatrolEditorService::MoveWaypoint(size_t index, int direction)
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr || index >= patrol->waypoints.size()) return false;
    const int target = static_cast<int>(index) + direction;
    if (target < 0 || target >= static_cast<int>(patrol->waypoints.size())) return false;
    std::swap(patrol->waypoints[index], patrol->waypoints[static_cast<size_t>(target)]);
    SyncSelection();
    return true;
}

bool SectorEditorPatrolEditorService::SetWaypointMarker(size_t index, int markerId)
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr || index >= patrol->waypoints.size()
            || FindSectorAuthoringLevelMarker(graph_, markerId) == nullptr
            || patrol->waypoints[index].levelMarkerId == markerId) return false;
    patrol->waypoints[index].levelMarkerId = markerId;
    return true;
}

bool SectorEditorPatrolEditorService::SetWaypointDelaySeconds(
        size_t index,
        float seconds)
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr || index >= patrol->waypoints.size()
            || !std::isfinite(seconds) || seconds < 0.0f
            || static_cast<double>(seconds) * 1000.0
                    > static_cast<double>(std::numeric_limits<int>::max())) {
        state_.validationMessage = "Waypoint delay is outside the supported range";
        return false;
    }
    const int milliseconds = static_cast<int>(std::lround(seconds * 1000.0f));
    if (patrol->waypoints[index].delayMilliseconds == milliseconds) return false;
    patrol->waypoints[index].delayMilliseconds = milliseconds;
    state_.validationMessage.clear();
    return true;
}

bool SectorEditorPatrolEditorService::SetWaypointGait(
        size_t index,
        SectorPatrolGait gait)
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr || index >= patrol->waypoints.size()
            || patrol->waypoints[index].gait == gait) return false;
    patrol->waypoints[index].gait = gait;
    return true;
}

bool SectorEditorPatrolEditorService::SetWaypointLookAround(
        size_t index,
        bool enabled)
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr || index >= patrol->waypoints.size()
            || patrol->waypoints[index].lookAround == enabled) return false;
    patrol->waypoints[index].lookAround = enabled;
    return true;
}

bool SectorEditorPatrolEditorService::SetWaypointLookArc(
        size_t index,
        float degrees)
{
    SectorAuthoringPatrol* patrol = Selected();
    if (patrol == nullptr || index >= patrol->waypoints.size()
            || !std::isfinite(degrees) || degrees < 0.0f || degrees > 360.0f) {
        state_.validationMessage = "Look arc must be between 0 and 360 degrees";
        return false;
    }
    if (patrol->waypoints[index].lookArcDegrees == degrees) return false;
    patrol->waypoints[index].lookArcDegrees = degrees;
    state_.validationMessage.clear();
    return true;
}

void SectorEditorPatrolEditorService::RebuildMarkerOptions()
{
    std::vector<const SectorAuthoringLevelMarker*> markers;
    markers.reserve(graph_.levelMarkers.size());
    for (const SectorAuthoringLevelMarker& marker : graph_.levelMarkers) {
        markers.push_back(&marker);
    }
    std::sort(markers.begin(), markers.end(), [](const auto* left, const auto* right) {
        return left->referenceId < right->referenceId;
    });
    state_.markerIds.clear();
    state_.markerLabelStorage.clear();
    state_.markerLabels.clear();
    state_.markerIds.reserve(markers.size());
    state_.markerLabelStorage.reserve(markers.size());
    for (const SectorAuthoringLevelMarker* marker : markers) {
        state_.markerIds.push_back(marker->id);
        state_.markerLabelStorage.push_back(marker->referenceId
                + "  [#" + std::to_string(marker->id) + "]");
    }
    state_.markerLabels.reserve(state_.markerLabelStorage.size());
    for (const std::string& label : state_.markerLabelStorage) {
        state_.markerLabels.push_back(label.c_str());
    }
}

void SectorEditorPatrolEditorService::RebuildListLabels()
{
    int selectedId = -1;
    if (const SectorAuthoringPatrol* selected = Selected()) {
        selectedId = selected->editorId;
    }
    state_.selectedIndex = -1;
    state_.listLabelStorage.clear();
    state_.listLabels.clear();
    state_.listLabelStorage.reserve(state_.drafts.size());
    for (size_t index = 0; index < state_.drafts.size(); ++index) {
        const SectorAuthoringPatrol& patrol = state_.drafts[index].patrol;
        if (patrol.editorId == selectedId) state_.selectedIndex = static_cast<int>(index);
        state_.listLabelStorage.push_back(patrol.id + "  [#"
                + std::to_string(patrol.editorId) + ", "
                + ModeLabel(patrol.mode) + ", "
                + std::to_string(patrol.waypoints.size()) + " WP]");
    }
    state_.listLabels.reserve(state_.listLabelStorage.size());
    for (const std::string& label : state_.listLabelStorage) {
        state_.listLabels.push_back(label.c_str());
    }
}

void SectorEditorPatrolEditorService::SyncSelection()
{
    const SectorAuthoringPatrol* patrol = Selected();
    std::snprintf(state_.idBuffer, sizeof(state_.idBuffer), "%s",
            patrol == nullptr ? "" : patrol->id.c_str());
    const size_t count = patrol == nullptr ? 0 : patrol->waypoints.size();
    state_.delayInputs.assign(count, engine::UIFloatInputState{});
    state_.arcInputs.assign(count, engine::UIFloatInputState{});
}

bool SectorEditorPatrolEditorService::ValidateDrafts(std::string& error) const
{
    SectorAuthoringGraph candidate = graph_;
    candidate.patrols.clear();
    candidate.patrols.reserve(state_.drafts.size());
    for (const SectorEditorPatrolDraft& draft : state_.drafts) {
        candidate.patrols.push_back(draft.patrol);
    }
    for (const SectorPlacedRuntimeObject& object : map_.runtimeObjects) {
        if (object.kind == "npc" && object.npc.patrolEditorId > 0
                && FindSectorAuthoringPatrol(candidate, object.npc.patrolEditorId)
                        == nullptr) {
            error = "NPC " + std::to_string(object.id)
                    + " references a patrol that would be removed";
            return false;
        }
    }
    const auto issues = ValidateSectorAuthoringGraphReferences(candidate);
    const auto found = std::find_if(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.objectKind == SectorAuthoringObjectKind::Patrol
                && issue.severity == SectorAuthoringValidationSeverity::Error;
    });
    if (found != issues.end()) {
        error = found->message;
        return false;
    }
    return true;
}

bool SectorEditorPatrolEditorService::SaveAndClose()
{
    if (!state_.open) return false;
    if (Selected() != nullptr && !ApplyIdBuffer()) return false;
    std::string error;
    if (!ValidateDrafts(error)) {
        state_.validationMessage = error;
        statusText_ = error;
        return false;
    }
    std::vector<SectorAuthoringPatrol> edited;
    edited.reserve(state_.drafts.size());
    for (const SectorEditorPatrolDraft& draft : state_.drafts) {
        edited.push_back(draft.patrol);
    }
    std::sort(edited.begin(), edited.end(), [](const auto& left, const auto& right) {
        return left.editorId < right.editorId;
    });
    std::vector<SectorAuthoringPatrol> current = graph_.patrols;
    std::sort(current.begin(), current.end(), [](const auto& left, const auto& right) {
        return left.editorId < right.editorId;
    });
    const bool changed = !SamePatrols(edited, current);
    if (changed) {
        graph_.patrols = std::move(edited);
        MarkSectorEditorAuthoringGraphEdited(
                lifecycle_, topologyRenderRevision_, topologyRenderCache_,
                derivation_, "Updated patrols");
        RefreshSectorEditorAuthoringDerivation(
                lifecycle_, topologyRenderRevision_, topologyRenderCache_,
                map_, graph_, derivation_, "Updated patrols",
                "Patrols saved; derivation failed");
        statusText_ = derivation_.authoringDerivationStatus;
    }
    Close();
    if (!changed) statusText_ = "Patrols unchanged";
    return true;
}

} // namespace game
