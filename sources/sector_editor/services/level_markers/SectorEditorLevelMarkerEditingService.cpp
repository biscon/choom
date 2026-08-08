#include "sector_editor/services/level_markers/SectorEditorLevelMarkerEditingService.h"

#include "sector_demo/SectorTopologyUnits.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game {

SectorEditorLevelMarkerEditingService::SectorEditorLevelMarkerEditingService(
        SectorEditorLevelMarkerEditingServiceContext context)
    : context_(std::move(context))
{
}

SectorAuthoringLevelMarker* SectorEditorLevelMarkerEditingService::Selected()
{
    if (context_.selectionState.selectedAuthoring.kind
            != SectorAuthoringSelectionKind::LevelMarker) {
        return nullptr;
    }
    return FindSectorAuthoringLevelMarker(
            context_.authoringGraph,
            context_.selectionState.selectedAuthoring.levelMarkerId);
}

const SectorAuthoringLevelMarker* SectorEditorLevelMarkerEditingService::Selected() const
{
    if (context_.selectionState.selectedAuthoring.kind
            != SectorAuthoringSelectionKind::LevelMarker) {
        return nullptr;
    }
    return FindSectorAuthoringLevelMarker(
            context_.authoringGraph,
            context_.selectionState.selectedAuthoring.levelMarkerId);
}

bool SectorEditorLevelMarkerEditingService::CommitGraphMutation(
        const char* successStatus,
        const char* failureStatus)
{
    MarkSectorEditorAuthoringGraphEdited(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.derivation,
            successStatus);
    const bool refreshed = RefreshSectorEditorAuthoringDerivation(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
            successStatus,
            failureStatus);
    context_.statusText = context_.derivation.authoringDerivationStatus;
    return refreshed;
}

bool SectorEditorLevelMarkerEditingService::Place(Vector2 snappedMapPoint, int* outId)
{
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText = "Level Marker placement requires current authoring derivation";
        return false;
    }
    SectorCoord x = 0;
    SectorCoord z = 0;
    if (!VisibleAuthoringToSectorCoord(snappedMapPoint.x, x)
            || !VisibleAuthoringToSectorCoord(snappedMapPoint.y, z)) {
        context_.statusText = "Level Marker placement is outside the authoring coordinate range";
        return false;
    }
    int sectorId = -1;
    if (!ResolveSectorAuthoringPointToDerivedSector(
                context_.derivation.authoringDerivation,
                SectorTopologyCoordPoint{x, z},
                &sectorId)) {
        context_.statusText = "Level Marker placement requires a point strictly inside a non-void sector";
        return false;
    }
    const SectorTopologySector* sector = FindSectorTopologySector(
            context_.derivation.authoringDerivation.topology,
            sectorId);
    const int id = AllocateSectorAuthoringLevelMarkerId(context_.authoringGraph);
    const std::string referenceId =
            AllocateSectorAuthoringLevelMarkerReferenceId(context_.authoringGraph);
    if (sector == nullptr || !IsValidSectorAuthoringId(id) || referenceId.empty()) {
        context_.statusText = "Level Marker placement failed: no marker identity is available";
        return false;
    }

    SectorAuthoringLevelMarker marker;
    marker.id = id;
    marker.referenceId = referenceId;
    marker.x = x;
    marker.y = sector->floorZ;
    marker.z = z;
    context_.authoringGraph.levelMarkers.push_back(std::move(marker));
    SelectSectorEditorAuthoringLevelMarker(context_.authoringGraph, context_.selectionState, id);
    if (outId != nullptr) {
        *outId = id;
    }
    return CommitGraphMutation("Placed Level Marker", "Level Marker placed; derivation failed");
}

bool SectorEditorLevelMarkerEditingService::MutateSelected(
        const char* status,
        const std::function<bool(SectorAuthoringLevelMarker&)>& mutate)
{
    SectorAuthoringLevelMarker* marker = Selected();
    if (marker == nullptr || !mutate || !mutate(*marker)) {
        return false;
    }
    return CommitGraphMutation(status, "Level Marker edit saved; derivation failed");
}

bool SectorEditorLevelMarkerEditingService::ValidateSelectedReferenceId(
        const std::string& referenceId,
        std::string& error) const
{
    const SectorAuthoringLevelMarker* selected = Selected();
    if (selected == nullptr) {
        error = "No Level Marker is selected";
        return false;
    }
    if (!IsValidSectorAuthoringLevelMarkerReferenceId(referenceId)) {
        error = "Use 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    const SectorAuthoringLevelMarker* existing =
            FindSectorAuthoringLevelMarkerByReferenceId(context_.authoringGraph, referenceId);
    if (existing != nullptr && existing->id != selected->id) {
        error = "Marker ID must be unique inside the level";
        return false;
    }
    error.clear();
    return true;
}

bool SectorEditorLevelMarkerEditingService::RenameSelected(const std::string& referenceId)
{
    std::string error;
    if (!ValidateSelectedReferenceId(referenceId, error)) {
        context_.statusText = error;
        return false;
    }
    return MutateSelected("Renamed Level Marker", [&referenceId](SectorAuthoringLevelMarker& marker) {
        if (marker.referenceId == referenceId) {
            return false;
        }
        marker.referenceId = referenceId;
        return true;
    });
}

bool SectorEditorLevelMarkerEditingService::SetSelectedPosition(Vector3 position)
{
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
        context_.statusText = "Level Marker position must be finite";
        return false;
    }
    SectorCoord x = 0;
    SectorCoord z = 0;
    if (!VisibleAuthoringToSectorCoord(position.x, x)
            || !VisibleAuthoringToSectorCoord(position.z, z)) {
        context_.statusText = "Level Marker X/Z must be exactly representable authoring coordinates";
        return false;
    }
    return MutateSelected("Updated Level Marker position", [x, z, position](SectorAuthoringLevelMarker& marker) {
        if (marker.x == x && marker.y == position.y && marker.z == z) {
            return false;
        }
        marker.x = x;
        marker.y = position.y;
        marker.z = z;
        return true;
    });
}

bool SectorEditorLevelMarkerEditingService::SetSelectedOrientation(float degrees)
{
    if (!std::isfinite(degrees)) {
        context_.statusText = "Level Marker orientation must be finite";
        return false;
    }
    return MutateSelected("Updated Level Marker orientation", [degrees](SectorAuthoringLevelMarker& marker) {
        if (marker.orientationDegrees == degrees) {
            return false;
        }
        marker.orientationDegrees = degrees;
        return true;
    });
}

bool SectorEditorLevelMarkerEditingService::DeleteSelected()
{
    const SectorAuthoringLevelMarker* selected = Selected();
    if (selected == nullptr) {
        return false;
    }
    const int id = selected->id;
    const auto oldSize = context_.authoringGraph.levelMarkers.size();
    context_.authoringGraph.levelMarkers.erase(
            std::remove_if(
                    context_.authoringGraph.levelMarkers.begin(),
                    context_.authoringGraph.levelMarkers.end(),
                    [id](const SectorAuthoringLevelMarker& marker) { return marker.id == id; }),
            context_.authoringGraph.levelMarkers.end());
    if (context_.authoringGraph.levelMarkers.size() == oldSize) {
        return false;
    }
    context_.editingState.drag = LevelMarkerDragState{};
    ClearSectorEditorAuthoringSelection(context_.selectionState);
    return CommitGraphMutation("Deleted Level Marker", "Level Marker deleted; derivation failed");
}

bool SectorEditorLevelMarkerEditingService::BeginMove(int markerId)
{
    const SectorAuthoringLevelMarker* marker =
            FindSectorAuthoringLevelMarker(context_.authoringGraph, markerId);
    if (marker == nullptr) {
        return false;
    }
    SelectSectorEditorAuthoringLevelMarker(context_.authoringGraph, context_.selectionState, markerId);
    LevelMarkerDragState& drag = context_.editingState.drag;
    drag = LevelMarkerDragState{};
    drag.active = true;
    drag.markerId = markerId;
    drag.originalX = marker->x;
    drag.originalZ = marker->z;
    drag.previewX = marker->x;
    drag.previewZ = marker->z;
    context_.statusText = "Moving Level Marker " + marker->referenceId;
    return true;
}

void SectorEditorLevelMarkerEditingService::UpdateMove(Vector2 snappedMapPoint)
{
    LevelMarkerDragState& drag = context_.editingState.drag;
    if (!drag.active) {
        return;
    }
    SectorCoord x = 0;
    SectorCoord z = 0;
    if (VisibleAuthoringToSectorCoord(snappedMapPoint.x, x)
            && VisibleAuthoringToSectorCoord(snappedMapPoint.y, z)) {
        drag.previewX = x;
        drag.previewZ = z;
    }
}

bool SectorEditorLevelMarkerEditingService::FinishMove()
{
    const LevelMarkerDragState drag = context_.editingState.drag;
    if (!drag.active) {
        return false;
    }
    context_.editingState.drag = LevelMarkerDragState{};
    SelectSectorEditorAuthoringLevelMarker(context_.authoringGraph, context_.selectionState, drag.markerId);
    if (drag.previewX == drag.originalX && drag.previewZ == drag.originalZ) {
        context_.statusText = "Level Marker move unchanged";
        return true;
    }
    return MutateSelected("Moved Level Marker", [drag](SectorAuthoringLevelMarker& marker) {
        marker.x = drag.previewX;
        marker.z = drag.previewZ;
        return true;
    });
}

void SectorEditorLevelMarkerEditingService::CancelMove(const char* message)
{
    context_.editingState.drag = LevelMarkerDragState{};
    if (message != nullptr && message[0] != '\0') {
        context_.statusText = message;
    }
}

} // namespace game
