#include "sector_editor/services/cameras/SectorEditorCameraEditingService.h"

#include "sector_demo/SectorTopologyUnits.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game {

SectorEditorCameraEditingService::SectorEditorCameraEditingService(
        SectorEditorCameraEditingServiceContext context)
    : context_(std::move(context))
{
}

SectorAuthoringCamera* SectorEditorCameraEditingService::Selected()
{
    if (context_.selectionState.selectedAuthoring.kind
            != SectorAuthoringSelectionKind::Camera) {
        return nullptr;
    }
    return FindSectorAuthoringCamera(
            context_.authoringGraph,
            context_.selectionState.selectedAuthoring.cameraId);
}

const SectorAuthoringCamera* SectorEditorCameraEditingService::Selected() const
{
    if (context_.selectionState.selectedAuthoring.kind
            != SectorAuthoringSelectionKind::Camera) {
        return nullptr;
    }
    return FindSectorAuthoringCamera(
            context_.authoringGraph,
            context_.selectionState.selectedAuthoring.cameraId);
}

bool SectorEditorCameraEditingService::CommitGraphMutation(
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

bool SectorEditorCameraEditingService::Place(Vector2 snappedMapPoint, int* outId)
{
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText = "Camera placement requires current authoring derivation";
        return false;
    }
    SectorCoord x = 0;
    SectorCoord z = 0;
    if (!VisibleAuthoringToSectorCoord(snappedMapPoint.x, x)
            || !VisibleAuthoringToSectorCoord(snappedMapPoint.y, z)) {
        context_.statusText = "Camera placement is outside the authoring coordinate range";
        return false;
    }
    int sectorId = -1;
    ResolveSectorAuthoringPointToDerivedSector(context_.derivation.authoringDerivation,
            SectorTopologyCoordPoint{x, z}, &sectorId);
    const SectorTopologySector* sector = FindSectorTopologySector(
            context_.derivation.authoringDerivation.topology,
            sectorId);
    const int id = AllocateSectorAuthoringCameraId(context_.authoringGraph);
    const std::string referenceId =
            AllocateSectorAuthoringCameraReferenceId(context_.authoringGraph);
    if (!IsValidSectorAuthoringId(id) || referenceId.empty()) {
        context_.statusText = "Camera placement failed: no camera identity is available";
        return false;
    }

    SectorAuthoringCamera camera;
    camera.id = id;
    camera.referenceId = referenceId;
    camera.x = x;
    camera.y = (sector ? sector->floorZ : 0.0f) + SectorWorldToAuthoringDistance(1.6f);
    camera.z = z;
    context_.authoringGraph.cameras.push_back(std::move(camera));
    SelectSectorEditorAuthoringCamera(context_.authoringGraph, context_.selectionState, id);
    if (outId != nullptr) {
        *outId = id;
    }
    return CommitGraphMutation("Placed Camera", "Camera placed; derivation failed");
}

bool SectorEditorCameraEditingService::MutateSelected(
        const char* status,
        const std::function<bool(SectorAuthoringCamera&)>& mutate)
{
    SectorAuthoringCamera* camera = Selected();
    if (camera == nullptr || !mutate || !mutate(*camera)) {
        return false;
    }
    return CommitGraphMutation(status, "Camera edit saved; derivation failed");
}

bool SectorEditorCameraEditingService::ValidateSelectedReferenceId(
        const std::string& referenceId,
        std::string& error) const
{
    const SectorAuthoringCamera* selected = Selected();
    if (selected == nullptr) {
        error = "No Camera is selected";
        return false;
    }
    if (!IsValidSectorAuthoringCameraReferenceId(referenceId)) {
        error = referenceId == "player" ? "Reserved ID: player" : "Use 1-63 letters/digits/_/-";
        return false;
    }
    const SectorAuthoringCamera* existing =
            FindSectorAuthoringCameraByReferenceId(context_.authoringGraph, referenceId);
    if (existing != nullptr && existing->id != selected->id) {
        error = "ID already exists";
        return false;
    }
    error.clear();
    return true;
}

bool SectorEditorCameraEditingService::RenameSelected(const std::string& referenceId)
{
    std::string error;
    if (!ValidateSelectedReferenceId(referenceId, error)) {
        context_.statusText = error;
        return false;
    }
    return MutateSelected("Renamed Camera", [&referenceId](SectorAuthoringCamera& camera) {
        if (camera.referenceId == referenceId) {
            return false;
        }
        camera.referenceId = referenceId;
        return true;
    });
}

bool SectorEditorCameraEditingService::SetSelectedPosition(Vector3 position)
{
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
        context_.statusText = "Camera position must be finite";
        return false;
    }
    SectorCoord x = 0;
    SectorCoord z = 0;
    if (!VisibleAuthoringToSectorCoord(position.x, x)
            || !VisibleAuthoringToSectorCoord(position.z, z)) {
        context_.statusText = "Camera X/Z must be exactly representable authoring coordinates";
        return false;
    }
    return MutateSelected("Updated Camera position", [x, z, position](SectorAuthoringCamera& camera) {
        if (camera.x == x && camera.y == position.y && camera.z == z) {
            return false;
        }
        camera.x = x;
        camera.y = position.y;
        camera.z = z;
        return true;
    });
}

bool SectorEditorCameraEditingService::SetSelectedOrientation(float degrees)
{
    if (!std::isfinite(degrees)) {
        context_.statusText = "Camera orientation must be finite";
        return false;
    }
    return MutateSelected("Updated Camera orientation", [degrees](SectorAuthoringCamera& camera) {
        if (camera.yawDegrees == degrees) {
            return false;
        }
        camera.yawDegrees = degrees;
        return true;
    });
}

bool SectorEditorCameraEditingService::SetSelectedLens(float pitch, float roll, float fov)
{
    if (!std::isfinite(pitch) || std::fabs(pitch) > 89.9f || !std::isfinite(roll)
            || !std::isfinite(fov) || fov < 1.0f || fov > 179.0f) return false;
    return MutateSelected("Updated Camera lens", [=](SectorAuthoringCamera& camera) {
        if (camera.pitchDegrees == pitch && camera.rollDegrees == roll
                && camera.verticalFovDegrees == fov) return false;
        camera.pitchDegrees = pitch;
        camera.rollDegrees = roll;
        camera.verticalFovDegrees = fov;
        return true;
    });
}

bool SectorEditorCameraEditingService::ApplyPilot(int id, Vector3 position,
        float yaw, float pitch, float roll, float fov)
{
    SectorCoord x, z;
    if (!VisibleAuthoringToSectorCoord(position.x, x)
            || !VisibleAuthoringToSectorCoord(position.z, z)
            || !std::isfinite(position.y) || !std::isfinite(yaw)
            || !std::isfinite(pitch) || std::fabs(pitch) > 89.9f
            || !std::isfinite(roll) || !std::isfinite(fov) || fov < 1 || fov > 179) return false;
    if (!SelectSectorEditorAuthoringCamera(context_.authoringGraph, context_.selectionState, id)) return false;
    return MutateSelected("Applied Camera pilot", [=](SectorAuthoringCamera& camera) {
        camera.x = x; camera.y = position.y; camera.z = z;
        camera.yawDegrees = yaw; camera.pitchDegrees = pitch;
        camera.rollDegrees = roll; camera.verticalFovDegrees = fov;
        return true;
    });
}

bool SectorEditorCameraEditingService::DeleteSelected()
{
    const SectorAuthoringCamera* selected = Selected();
    if (selected == nullptr) {
        return false;
    }
    const int id = selected->id;
    const auto oldSize = context_.authoringGraph.cameras.size();
    context_.authoringGraph.cameras.erase(
            std::remove_if(
                    context_.authoringGraph.cameras.begin(),
                    context_.authoringGraph.cameras.end(),
                    [id](const SectorAuthoringCamera& camera) { return camera.id == id; }),
            context_.authoringGraph.cameras.end());
    if (context_.authoringGraph.cameras.size() == oldSize) {
        return false;
    }
    context_.editingState.drag = CameraDragState{};
    ClearSectorEditorAuthoringSelection(context_.selectionState);
    return CommitGraphMutation("Deleted Camera", "Camera deleted; derivation failed");
}

bool SectorEditorCameraEditingService::BeginMove(int cameraId)
{
    const SectorAuthoringCamera* camera =
            FindSectorAuthoringCamera(context_.authoringGraph, cameraId);
    if (camera == nullptr) {
        return false;
    }
    SelectSectorEditorAuthoringCamera(context_.authoringGraph, context_.selectionState, cameraId);
    CameraDragState& drag = context_.editingState.drag;
    drag = CameraDragState{};
    drag.active = true;
    drag.cameraId = cameraId;
    drag.originalX = camera->x;
    drag.originalZ = camera->z;
    drag.previewX = camera->x;
    drag.previewZ = camera->z;
    context_.statusText = "Moving Camera " + camera->referenceId;
    return true;
}

void SectorEditorCameraEditingService::UpdateMove(Vector2 snappedMapPoint)
{
    CameraDragState& drag = context_.editingState.drag;
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

bool SectorEditorCameraEditingService::FinishMove()
{
    const CameraDragState drag = context_.editingState.drag;
    if (!drag.active) {
        return false;
    }
    context_.editingState.drag = CameraDragState{};
    SelectSectorEditorAuthoringCamera(context_.authoringGraph, context_.selectionState, drag.cameraId);
    if (drag.previewX == drag.originalX && drag.previewZ == drag.originalZ) {
        context_.statusText = "Camera move unchanged";
        return true;
    }
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText = "Camera movement requires current authoring derivation";
        return false;
    }
    return MutateSelected("Moved Camera", [drag](SectorAuthoringCamera& camera) {
        camera.x = drag.previewX;
        camera.z = drag.previewZ;
        return true;
    });
}

void SectorEditorCameraEditingService::CancelMove(const char* message)
{
    context_.editingState.drag = CameraDragState{};
    if (message != nullptr && message[0] != '\0') {
        context_.statusText = message;
    }
}

} // namespace game
