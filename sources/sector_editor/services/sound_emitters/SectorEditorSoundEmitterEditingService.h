#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/sound_emitters/SectorEditorSoundEmitterEditingState.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorSoundEmitterEditingServiceContext {
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    uint64_t& topologyRenderRevision;
    SectorEditorTopologyRenderCache& topologyRenderCache;
    SelectionState& selectionState;
    SoundEmitterEditingState& editingState;
    std::string& statusText;
};

class SectorEditorSoundEmitterEditingService {
public:
    explicit SectorEditorSoundEmitterEditingService(
            SectorEditorSoundEmitterEditingServiceContext context);

    SectorAuthoringSoundEmitter* Selected();
    const SectorAuthoringSoundEmitter* Selected() const;
    bool Place(Vector2 snappedMapPoint, int* outId = nullptr);
    bool ValidateSelectedReferenceId(const std::string& referenceId, std::string& error) const;
    bool RenameSelected(const std::string& referenceId);
    bool SetSelectedPosition(Vector3 authoringPosition);
    bool SetSelectedSoundId(const std::string& soundId);
    bool SetSelectedVolume(float volume);
    bool SetSelectedLoop(bool loop);
    bool DeleteSelected();

    bool BeginMove(int emitterId);
    void UpdateMove(Vector2 snappedMapPoint);
    bool FinishMove();
    void CancelMove(const char* message = nullptr);
    const SoundEmitterDragState& Drag() const { return context_.editingState.drag; }

private:
    bool MutateSelected(const char* status,
            const std::function<bool(SectorAuthoringSoundEmitter&)>& mutate);
    bool CommitGraphMutation(const char* successStatus, const char* failureStatus);

    SectorEditorSoundEmitterEditingServiceContext context_;
};

} // namespace game
