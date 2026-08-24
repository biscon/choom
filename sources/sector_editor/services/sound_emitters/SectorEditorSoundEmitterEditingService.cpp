#include "sector_editor/services/sound_emitters/SectorEditorSoundEmitterEditingService.h"

#include "sector_demo/SectorTopologyUnits.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game {

SectorEditorSoundEmitterEditingService::SectorEditorSoundEmitterEditingService(
        SectorEditorSoundEmitterEditingServiceContext context)
    : context_(std::move(context))
{
}

SectorAuthoringSoundEmitter* SectorEditorSoundEmitterEditingService::Selected()
{
    if (context_.selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::SoundEmitter) {
        return nullptr;
    }
    return FindSectorAuthoringSoundEmitter(
            context_.authoringGraph, context_.selectionState.selectedAuthoring.soundEmitterId);
}

const SectorAuthoringSoundEmitter* SectorEditorSoundEmitterEditingService::Selected() const
{
    if (context_.selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::SoundEmitter) {
        return nullptr;
    }
    return FindSectorAuthoringSoundEmitter(
            context_.authoringGraph, context_.selectionState.selectedAuthoring.soundEmitterId);
}

bool SectorEditorSoundEmitterEditingService::CommitGraphMutation(
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

bool SectorEditorSoundEmitterEditingService::Place(Vector2 snappedMapPoint, int* outId)
{
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText = "Sound Emitter placement requires current authoring derivation";
        return false;
    }
    SectorCoord x = 0;
    SectorCoord z = 0;
    if (!VisibleAuthoringToSectorCoord(snappedMapPoint.x, x)
            || !VisibleAuthoringToSectorCoord(snappedMapPoint.y, z)) {
        context_.statusText = "Sound Emitter placement is outside the authoring coordinate range";
        return false;
    }
    int sectorId = -1;
    if (!ResolveSectorAuthoringPointToDerivedSector(
                context_.derivation.authoringDerivation, {x, z}, &sectorId)) {
        context_.statusText = "Sound Emitter placement requires a point inside a sector";
        return false;
    }
    const SectorTopologySector* sector = FindSectorTopologySector(
            context_.derivation.authoringDerivation.topology, sectorId);
    const int id = AllocateSectorAuthoringSoundEmitterId(context_.authoringGraph);
    const std::string referenceId =
            AllocateSectorAuthoringSoundEmitterReferenceId(context_.authoringGraph);
    if (sector == nullptr || !IsValidSectorAuthoringId(id) || referenceId.empty()) {
        context_.statusText = "Sound Emitter placement failed: no identity is available";
        return false;
    }
    SectorAuthoringSoundEmitter emitter;
    emitter.id = id;
    emitter.referenceId = referenceId;
    emitter.x = x;
    emitter.y = sector->floorZ;
    emitter.z = z;
    context_.authoringGraph.soundEmitters.push_back(std::move(emitter));
    SelectSectorEditorAuthoringSoundEmitter(context_.authoringGraph, context_.selectionState, id);
    if (outId != nullptr) *outId = id;
    return CommitGraphMutation("Placed Sound Emitter", "Sound Emitter placed; derivation failed");
}

bool SectorEditorSoundEmitterEditingService::MutateSelected(
        const char* status, const std::function<bool(SectorAuthoringSoundEmitter&)>& mutate)
{
    SectorAuthoringSoundEmitter* emitter = Selected();
    if (emitter == nullptr || !mutate || !mutate(*emitter)) return false;
    return CommitGraphMutation(status, "Sound Emitter edit saved; derivation failed");
}

bool SectorEditorSoundEmitterEditingService::ValidateSelectedReferenceId(
        const std::string& referenceId, std::string& error) const
{
    const SectorAuthoringSoundEmitter* selected = Selected();
    if (selected == nullptr) { error = "No Sound Emitter is selected"; return false; }
    if (!IsValidSectorAuthoringSoundEmitterReferenceId(referenceId)) {
        error = "Use 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    const SectorAuthoringSoundEmitter* existing =
            FindSectorAuthoringSoundEmitterByReferenceId(context_.authoringGraph, referenceId);
    if (existing != nullptr && existing->id != selected->id) {
        error = "Emitter ID must be unique inside the level";
        return false;
    }
    error.clear();
    return true;
}

bool SectorEditorSoundEmitterEditingService::RenameSelected(const std::string& referenceId)
{
    std::string error;
    if (!ValidateSelectedReferenceId(referenceId, error)) {
        context_.statusText = error;
        return false;
    }
    return MutateSelected("Renamed Sound Emitter", [&referenceId](SectorAuthoringSoundEmitter& emitter) {
        if (emitter.referenceId == referenceId) return false;
        emitter.referenceId = referenceId;
        return true;
    });
}

bool SectorEditorSoundEmitterEditingService::ValidateSelectedSoundId(
        const std::string& soundId, std::string& error) const
{
    if (Selected() == nullptr) {
        error = "No Sound Emitter is selected";
        return false;
    }
    if (!soundId.empty()
            && context_.authoringGraph.audioSettings.soundsById.find(soundId)
                    == context_.authoringGraph.audioSettings.soundsById.end()) {
        error = "Sound ID must be empty or match a registered map Sound/Music ID";
        return false;
    }
    error.clear();
    return true;
}

bool SectorEditorSoundEmitterEditingService::SetSelectedPosition(Vector3 position)
{
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
        context_.statusText = "Sound Emitter position must be finite";
        return false;
    }
    SectorCoord x = 0;
    SectorCoord z = 0;
    if (!VisibleAuthoringToSectorCoord(position.x, x)
            || !VisibleAuthoringToSectorCoord(position.z, z)) {
        context_.statusText = "Sound Emitter X/Z must be exact authoring coordinates";
        return false;
    }
    return MutateSelected("Updated Sound Emitter position", [x, z, position](SectorAuthoringSoundEmitter& emitter) {
        if (emitter.x == x && emitter.y == position.y && emitter.z == z) return false;
        emitter.x = x; emitter.y = position.y; emitter.z = z; return true;
    });
}

bool SectorEditorSoundEmitterEditingService::SetSelectedSoundId(const std::string& soundId)
{
    std::string error;
    if (!ValidateSelectedSoundId(soundId, error)) {
        context_.statusText = error;
        return false;
    }
    return MutateSelected("Updated Sound Emitter sound", [&soundId](SectorAuthoringSoundEmitter& emitter) {
        if (emitter.soundId == soundId) return false;
        emitter.soundId = soundId; return true;
    });
}

bool SectorEditorSoundEmitterEditingService::SetSelectedVolume(float volume)
{
    if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
        context_.statusText = "Sound Emitter volume must be between 0 and 1";
        return false;
    }
    return MutateSelected("Updated Sound Emitter volume", [volume](SectorAuthoringSoundEmitter& emitter) {
        if (emitter.volume == volume) return false;
        emitter.volume = volume; return true;
    });
}

bool SectorEditorSoundEmitterEditingService::SetSelectedLoop(bool loop)
{
    return MutateSelected("Updated Sound Emitter loop", [loop](SectorAuthoringSoundEmitter& emitter) {
        if (emitter.loop == loop) return false;
        emitter.loop = loop; return true;
    });
}

bool SectorEditorSoundEmitterEditingService::DeleteSelected()
{
    const SectorAuthoringSoundEmitter* selected = Selected();
    if (selected == nullptr) return false;
    const int id = selected->id;
    const auto oldSize = context_.authoringGraph.soundEmitters.size();
    context_.authoringGraph.soundEmitters.erase(
            std::remove_if(context_.authoringGraph.soundEmitters.begin(),
                    context_.authoringGraph.soundEmitters.end(),
                    [id](const SectorAuthoringSoundEmitter& emitter) { return emitter.id == id; }),
            context_.authoringGraph.soundEmitters.end());
    if (oldSize == context_.authoringGraph.soundEmitters.size()) return false;
    context_.editingState.drag = {};
    ClearSectorEditorAuthoringSelection(context_.selectionState);
    return CommitGraphMutation("Deleted Sound Emitter", "Sound Emitter deleted; derivation failed");
}

bool SectorEditorSoundEmitterEditingService::BeginMove(int emitterId)
{
    const SectorAuthoringSoundEmitter* emitter =
            FindSectorAuthoringSoundEmitter(context_.authoringGraph, emitterId);
    if (emitter == nullptr) return false;
    SelectSectorEditorAuthoringSoundEmitter(context_.authoringGraph, context_.selectionState, emitterId);
    SoundEmitterDragState& drag = context_.editingState.drag;
    drag = {};
    drag.active = true;
    drag.emitterId = emitterId;
    drag.originalX = drag.previewX = emitter->x;
    drag.originalZ = drag.previewZ = emitter->z;
    context_.statusText = "Moving Sound Emitter " + emitter->referenceId;
    return true;
}

void SectorEditorSoundEmitterEditingService::UpdateMove(Vector2 snappedMapPoint)
{
    SoundEmitterDragState& drag = context_.editingState.drag;
    if (!drag.active) return;
    SectorCoord x = 0;
    SectorCoord z = 0;
    if (VisibleAuthoringToSectorCoord(snappedMapPoint.x, x)
            && VisibleAuthoringToSectorCoord(snappedMapPoint.y, z)) {
        drag.previewX = x; drag.previewZ = z;
    }
}

bool SectorEditorSoundEmitterEditingService::FinishMove()
{
    const SoundEmitterDragState drag = context_.editingState.drag;
    if (!drag.active) return false;
    context_.editingState.drag = {};
    SelectSectorEditorAuthoringSoundEmitter(context_.authoringGraph, context_.selectionState, drag.emitterId);
    if (drag.previewX == drag.originalX && drag.previewZ == drag.originalZ) {
        context_.statusText = "Sound Emitter move unchanged";
        return true;
    }
    return MutateSelected("Moved Sound Emitter", [drag](SectorAuthoringSoundEmitter& emitter) {
        emitter.x = drag.previewX; emitter.z = drag.previewZ; return true;
    });
}

void SectorEditorSoundEmitterEditingService::CancelMove(const char* message)
{
    context_.editingState.drag = {};
    if (message != nullptr && message[0] != '\0') context_.statusText = message;
}

} // namespace game
