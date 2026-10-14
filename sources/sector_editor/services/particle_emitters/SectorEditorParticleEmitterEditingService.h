#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/particle_emitters/SectorEditorParticleEmitterEditingState.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorParticleEmitterEditingServiceContext {
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    uint64_t& topologyRenderRevision;
    SectorEditorTopologyRenderCache& topologyRenderCache;
    SelectionState& selectionState;
    ParticleEmitterEditingState& editingState;
    std::string& statusText;
};

class SectorEditorParticleEmitterEditingService {
public:
    explicit SectorEditorParticleEmitterEditingService(
            SectorEditorParticleEmitterEditingServiceContext context);

    SectorAuthoringParticleEmitter* Selected();
    const SectorAuthoringParticleEmitter* Selected() const;
    bool Place(Vector2 snappedMapPoint, int* outId = nullptr);
    bool ValidateSelectedReferenceId(const std::string& referenceId, std::string& error) const;
    bool RenameSelected(const std::string& referenceId);
    bool SetSelectedPosition(Vector3 authoringPosition);
    bool DeleteSelected();
    bool DuplicateSelected();
    bool Apply(const SectorAuthoringParticleEmitter& candidate);

    bool BeginMove(int emitterId);
    void UpdateMove(Vector2 snappedMapPoint);
    bool FinishMove();
    void CancelMove(const char* message = nullptr);
    const ParticleEmitterDragState& Drag() const { return context_.editingState.drag; }

private:
    bool MutateSelected(const char* status,
            const std::function<bool(SectorAuthoringParticleEmitter&)>& mutate);
    bool CommitGraphMutation(const char* successStatus, const char* failureStatus);

    SectorEditorParticleEmitterEditingServiceContext context_;
};

} // namespace game
