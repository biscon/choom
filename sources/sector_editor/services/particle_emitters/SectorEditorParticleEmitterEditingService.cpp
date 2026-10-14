#include "sector_editor/services/particle_emitters/SectorEditorParticleEmitterEditingService.h"

#include "sector_demo/SectorTopologyUnits.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game {

SectorEditorParticleEmitterEditingService::SectorEditorParticleEmitterEditingService(
        SectorEditorParticleEmitterEditingServiceContext context)
    : context_(std::move(context))
{
}

SectorAuthoringParticleEmitter* SectorEditorParticleEmitterEditingService::Selected()
{
    if (context_.selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::ParticleEmitter) {
        return nullptr;
    }
    return FindSectorAuthoringParticleEmitter(
            context_.authoringGraph, context_.selectionState.selectedAuthoring.particleEmitterId);
}

const SectorAuthoringParticleEmitter* SectorEditorParticleEmitterEditingService::Selected() const
{
    if (context_.selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::ParticleEmitter) {
        return nullptr;
    }
    return FindSectorAuthoringParticleEmitter(
            context_.authoringGraph, context_.selectionState.selectedAuthoring.particleEmitterId);
}

bool SectorEditorParticleEmitterEditingService::CommitGraphMutation(
        const char* successStatus, const char* failureStatus)
{
    MarkSectorEditorAuthoringGraphEdited(
            context_.lifecycle, context_.topologyRenderRevision,
            context_.topologyRenderCache, context_.derivation, successStatus);
    const bool refreshed = RefreshSectorEditorAuthoringDerivation(
            context_.lifecycle, context_.topologyRenderRevision,
            context_.topologyRenderCache, context_.topologyMap,
            context_.authoringGraph, context_.derivation, successStatus, failureStatus);
    context_.statusText = context_.derivation.authoringDerivationStatus;
    return refreshed;
}

bool SectorEditorParticleEmitterEditingService::Place(Vector2 snappedMapPoint, int* outId)
{
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText = "Particle Emitter placement requires current authoring derivation";
        return false;
    }
    SectorCoord x = 0;
    SectorCoord z = 0;
    if (!VisibleAuthoringToSectorCoord(snappedMapPoint.x, x)
            || !VisibleAuthoringToSectorCoord(snappedMapPoint.y, z)) {
        context_.statusText = "Particle Emitter placement is outside the authoring coordinate range";
        return false;
    }
    int sectorId = -1;
    if (!ResolveSectorAuthoringPointToDerivedSector(
                context_.derivation.authoringDerivation, {x, z}, &sectorId)) {
        context_.statusText = "Particle Emitter placement requires a point inside a sector";
        return false;
    }
    const SectorTopologySector* sector = FindSectorTopologySector(
            context_.derivation.authoringDerivation.topology, sectorId);
    const int id = AllocateSectorAuthoringParticleEmitterId(context_.authoringGraph);
    const std::string referenceId =
            AllocateSectorAuthoringParticleEmitterReferenceId(context_.authoringGraph);
    if (sector == nullptr || !IsValidSectorAuthoringId(id) || referenceId.empty()) {
        context_.statusText = "Particle Emitter placement failed: no identity is available";
        return false;
    }
    SectorAuthoringParticleEmitter emitter;
    emitter.id = id;
    emitter.referenceId = referenceId;
    emitter.x = x;
    emitter.heightWorld = 0.05f;
    emitter.z = z;
    context_.authoringGraph.particleEmitters.push_back(std::move(emitter));
    SelectSectorEditorAuthoringParticleEmitter(context_.authoringGraph, context_.selectionState, id);
    if (outId != nullptr) *outId = id;
    return CommitGraphMutation("Placed Particle Emitter", "Particle Emitter placed; derivation failed");
}

bool SectorEditorParticleEmitterEditingService::MutateSelected(
        const char* status, const std::function<bool(SectorAuthoringParticleEmitter&)>& mutate)
{
    SectorAuthoringParticleEmitter* emitter = Selected();
    if (emitter == nullptr || !mutate || !IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) return false;
    auto candidate = *emitter;
    if (!mutate(candidate)) return false;
    std::string error;
    int sectorId = -1;
    if (!ValidateSelectedReferenceId(candidate.referenceId, error)
            || !ValidateSectorParticleSettings(candidate.settings, error)
            || !std::isfinite(candidate.heightWorld) || !std::isfinite(candidate.yawDegrees)
            || !std::isfinite(candidate.pitchDegrees)
            || !ResolveSectorAuthoringPointToDerivedSector(context_.derivation.authoringDerivation,
                    {candidate.x,candidate.z}, &sectorId)) {
        context_.statusText = error.empty() ? "Particle emitter requires a finite transform strictly inside a sector" : error;
        return false;
    }
    *emitter = std::move(candidate);
    return CommitGraphMutation(status, "Particle Emitter edit saved; derivation failed");
}

bool SectorEditorParticleEmitterEditingService::ValidateSelectedReferenceId(
        const std::string& referenceId, std::string& error) const
{
    const SectorAuthoringParticleEmitter* selected = Selected();
    if (selected == nullptr) { error = "No Particle Emitter is selected"; return false; }
    if (!IsValidSectorAuthoringSoundEmitterReferenceId(referenceId)) {
        error = "Use 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    const SectorAuthoringParticleEmitter* existing = nullptr;
    for (const auto& e : context_.authoringGraph.particleEmitters)
        if (e.referenceId == referenceId) { existing = &e; break; }
    if (existing != nullptr && existing->id != selected->id) {
        error = "Emitter ID must be unique inside the level";
        return false;
    }
    error.clear();
    return true;
}

bool SectorEditorParticleEmitterEditingService::RenameSelected(const std::string& referenceId)
{
    std::string error;
    if (!ValidateSelectedReferenceId(referenceId, error)) {
        context_.statusText = error;
        return false;
    }
    return MutateSelected("Renamed Particle Emitter", [&referenceId](SectorAuthoringParticleEmitter& emitter) {
        if (emitter.referenceId == referenceId) return false;
        emitter.referenceId = referenceId;
        return true;
    });
}
bool SectorEditorParticleEmitterEditingService::SetSelectedPosition(Vector3 position)
{
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
        context_.statusText = "Particle Emitter position must be finite";
        return false;
    }
    SectorCoord x = 0;
    SectorCoord z = 0;
    if (!VisibleAuthoringToSectorCoord(position.x, x)
            || !VisibleAuthoringToSectorCoord(position.z, z)) {
        context_.statusText = "Particle Emitter X/Z must be exact authoring coordinates";
        return false;
    }
    return MutateSelected("Updated Particle Emitter position", [x, z, position](SectorAuthoringParticleEmitter& emitter) {
        if (emitter.x == x && emitter.heightWorld == position.y && emitter.z == z) return false;
        emitter.x = x; emitter.heightWorld = position.y; emitter.z = z; return true;
    });
}
bool SectorEditorParticleEmitterEditingService::DeleteSelected()
{
    const SectorAuthoringParticleEmitter* selected = Selected();
    if (selected == nullptr) return false;
    const int id = selected->id;
    const auto oldSize = context_.authoringGraph.particleEmitters.size();
    context_.authoringGraph.particleEmitters.erase(
            std::remove_if(context_.authoringGraph.particleEmitters.begin(),
                    context_.authoringGraph.particleEmitters.end(),
                    [id](const SectorAuthoringParticleEmitter& emitter) { return emitter.id == id; }),
            context_.authoringGraph.particleEmitters.end());
    if (oldSize == context_.authoringGraph.particleEmitters.size()) return false;
    context_.editingState.drag = {};
    ClearSectorEditorAuthoringSelection(context_.selectionState);
    return CommitGraphMutation("Deleted Particle Emitter", "Particle Emitter deleted; derivation failed");
}

bool SectorEditorParticleEmitterEditingService::BeginMove(int emitterId)
{
    const SectorAuthoringParticleEmitter* emitter =
            FindSectorAuthoringParticleEmitter(context_.authoringGraph, emitterId);
    if (emitter == nullptr) return false;
    SelectSectorEditorAuthoringParticleEmitter(context_.authoringGraph, context_.selectionState, emitterId);
    ParticleEmitterDragState& drag = context_.editingState.drag;
    drag = {};
    drag.active = true;
    drag.emitterId = emitterId;
    drag.originalX = drag.previewX = emitter->x;
    drag.originalZ = drag.previewZ = emitter->z;
    context_.statusText = "Moving Particle Emitter " + emitter->referenceId;
    return true;
}

void SectorEditorParticleEmitterEditingService::UpdateMove(Vector2 snappedMapPoint)
{
    ParticleEmitterDragState& drag = context_.editingState.drag;
    if (!drag.active) return;
    SectorCoord x = 0;
    SectorCoord z = 0;
    if (VisibleAuthoringToSectorCoord(snappedMapPoint.x, x)
            && VisibleAuthoringToSectorCoord(snappedMapPoint.y, z)) {
        drag.previewX = x; drag.previewZ = z;
    }
}

bool SectorEditorParticleEmitterEditingService::FinishMove()
{
    const ParticleEmitterDragState drag = context_.editingState.drag;
    if (!drag.active) return false;
    context_.editingState.drag = {};
    SelectSectorEditorAuthoringParticleEmitter(context_.authoringGraph, context_.selectionState, drag.emitterId);
    if (drag.previewX == drag.originalX && drag.previewZ == drag.originalZ) {
        context_.statusText = "Particle Emitter move unchanged";
        return true;
    }
    return MutateSelected("Moved Particle Emitter", [drag](SectorAuthoringParticleEmitter& emitter) {
        emitter.x = drag.previewX; emitter.z = drag.previewZ; return true;
    });
}

void SectorEditorParticleEmitterEditingService::CancelMove(const char* message)
{
    context_.editingState.drag = {};
    if (message != nullptr && message[0] != '\0') context_.statusText = message;
}

bool SectorEditorParticleEmitterEditingService::Apply(const SectorAuthoringParticleEmitter& candidate)
{
    std::string error;
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText = "Particle editing requires current authoring derivation"; return false;
    }
    if (!ValidateSelectedReferenceId(candidate.referenceId, error)
            || !ValidateSectorParticleSettings(candidate.settings, error)
            || !std::isfinite(candidate.heightWorld) || !std::isfinite(candidate.yawDegrees)
            || !std::isfinite(candidate.pitchDegrees)) {
        context_.statusText = error.empty() ? "Particle transform must be finite" : error; return false;
    }
    const auto* selected = Selected();
    if (!selected || selected->id != candidate.id) return false;
    return MutateSelected("Updated Particle Emitter", [&](auto& e) { e = candidate; return true; });
}
bool SectorEditorParticleEmitterEditingService::DuplicateSelected()
{
    const auto* selected = Selected();
    if (!selected || !IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) return false;
    auto copy = *selected;
    copy.id = AllocateSectorAuthoringParticleEmitterId(context_.authoringGraph);
    copy.referenceId = AllocateSectorAuthoringParticleEmitterReferenceId(context_.authoringGraph);
    if (copy.id <= 0 || copy.referenceId.empty()) return false;
    context_.authoringGraph.particleEmitters.push_back(copy);
    SelectSectorEditorAuthoringParticleEmitter(context_.authoringGraph, context_.selectionState, copy.id);
    return CommitGraphMutation("Duplicated Particle Emitter", "Particle duplication derivation failed");
}

} // namespace game
