#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/cameras/SectorEditorCameraEditingState.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorCameraEditingServiceContext {
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    uint64_t& topologyRenderRevision;
    SectorEditorTopologyRenderCache& topologyRenderCache;
    SelectionState& selectionState;
    CameraEditingState& editingState;
    std::string& statusText;
};

class SectorEditorCameraEditingService {
public:
    explicit SectorEditorCameraEditingService(
            SectorEditorCameraEditingServiceContext context);

    SectorAuthoringCamera* Selected();
    const SectorAuthoringCamera* Selected() const;
    bool Place(Vector2 snappedMapPoint, int* outId = nullptr);
    bool ValidateSelectedReferenceId(
            const std::string& referenceId,
            std::string& error) const;
    bool RenameSelected(const std::string& referenceId);
    bool SetSelectedPosition(Vector3 authoringPosition);
    bool SetSelectedOrientation(float degrees);
    bool SetSelectedLens(float pitch, float roll, float fov);
    bool ApplyPilot(int id, Vector3 authoringPosition, float yaw, float pitch, float roll, float fov);
    bool DeleteSelected();

    bool BeginMove(int cameraId);
    void UpdateMove(Vector2 snappedMapPoint);
    bool FinishMove();
    void CancelMove(const char* message = nullptr);
    const CameraDragState& Drag() const { return context_.editingState.drag; }

private:
    bool MutateSelected(
            const char* status,
            const std::function<bool(SectorAuthoringCamera&)>& mutate);
    bool CommitGraphMutation(const char* successStatus, const char* failureStatus);

    SectorEditorCameraEditingServiceContext context_;
};

} // namespace game
