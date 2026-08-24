#include "sector_editor/sounds/SectorEditorSoundEditorService.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace game {
namespace {

const char* TypeLabel(SectorSoundType type)
{
    return type == SectorSoundType::Music ? "Music" : "Sound";
}

bool SameDefinitions(
        const std::unordered_map<std::string, SectorSoundDefinition>& lhs,
        const std::unordered_map<std::string, SectorSoundDefinition>& rhs)
{
    if (lhs.size() != rhs.size()) return false;
    for (const auto& entry : lhs) {
        const auto found = rhs.find(entry.first);
        if (found == rhs.end()) return false;
        const SectorSoundDefinition& other = found->second;
        if (entry.second.id != other.id || entry.second.path != other.path
                || entry.second.type != other.type) return false;
    }
    return true;
}

bool IsValidSoundPath(const std::string& path)
{
    if (path.empty()) return false;
    const std::filesystem::path parsed(path);
    const bool windowsDrive = path.size() >= 2
            && std::isalpha(static_cast<unsigned char>(path[0]))
            && path[1] == ':';
    if (parsed.is_absolute() || windowsDrive || path.front() == '\\'
            || path.find('\\') != std::string::npos
            || path.find("..") != std::string::npos) {
        return false;
    }
    std::string extension = parsed.extension().generic_string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".ogg" || extension == ".wav" || extension == ".mp3";
}

bool IsValidSoundId(const std::string& id)
{
    if (id.empty()) return false;
    for (const char character : id) {
        const unsigned char value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '_' && character != '-') return false;
    }
    return true;
}

const SectorTopologySector* FindMappedSector(
        const SectorTopologyMap& map,
        const SectorAuthoringDerivationResult& derivation,
        int faceAnchorId)
{
    for (const SectorAuthoringDerivedSectorMapping& mapping : derivation.mapping.sectors) {
        if (mapping.faceAnchorId == faceAnchorId) {
            const auto found = std::find_if(
                    map.sectors.begin(), map.sectors.end(),
                    [&mapping](const SectorTopologySector& sector) {
                        return sector.id == mapping.topologySectorId;
                    });
            return found == map.sectors.end() ? nullptr : &*found;
        }
    }
    return nullptr;
}

} // namespace

SectorEditorSoundEditorService::SectorEditorSoundEditorService(
        SectorEditorSoundEditorState& state,
        SectorAuthoringGraph& authoringGraph,
        SectorTopologyMap& topologyMap,
        SectorEditorDerivationDocumentAccess derivation,
        SectorEditorDocumentLifecycleAccess lifecycle,
        std::string& statusText)
    : state_(state)
    , authoringGraph_(authoringGraph)
    , topologyMap_(topologyMap)
    , derivation_(derivation)
    , lifecycle_(lifecycle)
    , statusText_(statusText)
{
}

void SectorEditorSoundEditorService::Open()
{
    state_ = SectorEditorSoundEditorState{};
    state_.open = true;
    std::vector<std::string> ids;
    ids.reserve(authoringGraph_.audioSettings.soundsById.size());
    for (const auto& entry : authoringGraph_.audioSettings.soundsById) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    state_.drafts.reserve(ids.size());
    for (const std::string& id : ids) {
        state_.drafts.push_back(SectorEditorSoundDraft{
                authoringGraph_.audioSettings.soundsById.at(id), id});
    }
    state_.selectedIndex = state_.drafts.empty() ? -1 : 0;
    RebuildListLabels();
    SyncBuffers();
    statusText_ = "Sound Editor opened";
}

void SectorEditorSoundEditorService::Close()
{
    state_ = SectorEditorSoundEditorState{};
}

void SectorEditorSoundEditorService::Cancel()
{
    if (!state_.open) return;
    Close();
    statusText_ = "Sound changes discarded";
}

SectorEditorSoundDraft* SectorEditorSoundEditorService::SelectedDraft()
{
    return state_.selectedIndex >= 0
                    && state_.selectedIndex < static_cast<int>(state_.drafts.size())
            ? &state_.drafts[static_cast<size_t>(state_.selectedIndex)] : nullptr;
}

const SectorEditorSoundDraft* SectorEditorSoundEditorService::SelectedDraft() const
{
    return state_.selectedIndex >= 0
                    && state_.selectedIndex < static_cast<int>(state_.drafts.size())
            ? &state_.drafts[static_cast<size_t>(state_.selectedIndex)] : nullptr;
}

bool SectorEditorSoundEditorService::SelectIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(state_.drafts.size())) return false;
    state_.selectedIndex = index;
    state_.validationMessage.clear();
    SyncBuffers();
    return true;
}

void SectorEditorSoundEditorService::AddSound()
{
    std::unordered_set<std::string> used;
    used.reserve(state_.drafts.size());
    for (const auto& draft : state_.drafts) used.insert(draft.definition.id);
    std::string id = "new_sound";
    for (int suffix = 2; used.find(id) != used.end(); ++suffix) {
        id = "new_sound_" + std::to_string(suffix);
    }
    SectorEditorSoundDraft draft;
    draft.definition.id = id;
    state_.drafts.push_back(std::move(draft));
    state_.selectedIndex = static_cast<int>(state_.drafts.size()) - 1;
    state_.formScroll = {};
    state_.validationMessage = "Choose an audio file before saving";
    RebuildListLabels();
    SyncBuffers();
}

bool SectorEditorSoundEditorService::ApplyIdBuffer()
{
    SectorEditorSoundDraft* draft = SelectedDraft();
    if (draft == nullptr) return false;
    if (SelectedIsReferenced() && state_.idBuffer != draft->definition.id) {
        state_.validationMessage = "Referenced sound IDs cannot be renamed";
        SyncBuffers();
        return false;
    }
    const std::string id = state_.idBuffer;
    if (id.size() >= sizeof(state_.idBuffer) || !IsValidSoundId(id)) {
        state_.validationMessage = "ID must use letters, digits, underscores, or dashes";
        return false;
    }
    for (const SectorEditorSoundDraft& other : state_.drafts) {
        if (&other != draft && other.definition.id == id) {
            state_.validationMessage = "Sound ID already exists";
            return false;
        }
    }
    draft->definition.id = id;
    state_.validationMessage.clear();
    RebuildListLabels();
    RefreshSelectedUsage();
    return true;
}

bool SectorEditorSoundEditorService::SetSelectedType(SectorSoundType type)
{
    SectorEditorSoundDraft* draft = SelectedDraft();
    if (draft == nullptr || draft->definition.type == type) return false;
    if (SelectedIsReferenced()) {
        state_.validationMessage = "Referenced sounds cannot change Sound/Music type";
        return false;
    }
    draft->definition.type = type;
    state_.validationMessage.clear();
    RebuildListLabels();
    return true;
}

bool SectorEditorSoundEditorService::SetSelectedPath(const std::string& path)
{
    SectorEditorSoundDraft* draft = SelectedDraft();
    if (draft == nullptr) return false;
    if (!IsValidSoundPath(path)) {
        state_.validationMessage = "Choose a relative .ogg, .wav, or .mp3 audio path";
        return false;
    }
    if (draft->definition.path == path) return false;
    draft->definition.path = path;
    state_.validationMessage.clear();
    return true;
}

bool SectorEditorSoundEditorService::RequestDeleteSelected()
{
    const SectorEditorSoundDraft* draft = SelectedDraft();
    if (draft == nullptr) return false;
    if (SelectedIsReferenced()) {
        state_.validationMessage = "Referenced sounds cannot be removed";
        return false;
    }
    state_.deleteConfirmationOpen = true;
    state_.deleteConfirmationId = draft->definition.id;
    return true;
}

void SectorEditorSoundEditorService::CancelDelete()
{
    state_.deleteConfirmationOpen = false;
    state_.deleteConfirmationId.clear();
}

bool SectorEditorSoundEditorService::ConfirmDeleteSelected()
{
    if (!state_.deleteConfirmationOpen || SelectedIsReferenced()) {
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
    SyncBuffers();
    return true;
}

std::vector<std::string> SectorEditorSoundEditorService::UsageLabels(std::string_view id) const
{
    std::vector<std::string> labels;
    if (id.empty()) return labels;
    for (const SectorAuthoringFaceAnchor& anchor : authoringGraph_.faceAnchors) {
        if (anchor.roomtone.mode != SectorRoomtoneMode::Play
                || anchor.roomtone.soundId != id) continue;
        const SectorTopologySector* sector = FindMappedSector(
                topologyMap_, derivation_.authoringDerivation, anchor.id);
        if (sector == nullptr) {
            labels.push_back("Face " + std::to_string(anchor.id) + " roomtone");
        } else if (sector->name.empty()) {
            labels.push_back("Sector " + std::to_string(sector->id) + " roomtone");
        } else {
            labels.push_back("Sector " + std::to_string(sector->id) + " \""
                    + sector->name + "\" roomtone");
        }
    }
    for (const SectorAuthoringSoundEmitter& emitter : authoringGraph_.soundEmitters) {
        if (emitter.soundId != id) continue;
        labels.push_back("Sound Emitter \"" + emitter.referenceId + "\" (editor "
                + std::to_string(emitter.id) + ")");
    }
    for (const SectorPlacedRuntimeObject& object : topologyMap_.runtimeObjects) {
        if (object.kind != "door") continue;
        const std::string doorName = object.door.instanceId.empty()
                ? "Door " + std::to_string(object.id)
                : "Door \"" + object.door.instanceId + "\"";
        if (object.door.openSoundId == id) labels.push_back(doorName + " open sound");
        if (object.door.closeSoundId == id) labels.push_back(doorName + " close sound");
    }
    return labels;
}

bool SectorEditorSoundEditorService::SelectedIsReferenced() const
{
    const SectorEditorSoundDraft* draft = SelectedDraft();
    if (draft == nullptr) return false;
    return !UsageLabels(draft->definition.id).empty();
}

void SectorEditorSoundEditorService::RefreshSelectedUsage()
{
    state_.usageText.clear();
    const SectorEditorSoundDraft* draft = SelectedDraft();
    if (draft == nullptr) return;
    const std::vector<std::string> labels = UsageLabels(draft->definition.id);
    for (size_t index = 0; index < labels.size(); ++index) {
        if (index != 0) state_.usageText += "\n";
        state_.usageText += labels[index];
    }
}

void SectorEditorSoundEditorService::SyncBuffers()
{
    const SectorEditorSoundDraft* draft = SelectedDraft();
    std::snprintf(state_.idBuffer, sizeof(state_.idBuffer), "%s",
            draft == nullptr ? "" : draft->definition.id.c_str());
    RefreshSelectedUsage();
}

void SectorEditorSoundEditorService::RebuildListLabels()
{
    std::string selectedId;
    std::string selectedOriginalId;
    if (const SectorEditorSoundDraft* selected = SelectedDraft()) {
        selectedId = selected->definition.id;
        selectedOriginalId = selected->originalId;
    }
    std::sort(state_.drafts.begin(), state_.drafts.end(),
            [](const SectorEditorSoundDraft& lhs, const SectorEditorSoundDraft& rhs) {
                return lhs.definition.id < rhs.definition.id;
            });
    if (!selectedId.empty()) {
        const auto selected = std::find_if(
                state_.drafts.begin(), state_.drafts.end(),
                [&selectedId, &selectedOriginalId](const SectorEditorSoundDraft& draft) {
                    return draft.definition.id == selectedId
                            && draft.originalId == selectedOriginalId;
                });
        state_.selectedIndex = selected == state_.drafts.end()
                ? -1 : static_cast<int>(std::distance(state_.drafts.begin(), selected));
    }
    state_.listLabelStorage.clear();
    state_.listLabels.clear();
    state_.listLabelStorage.reserve(state_.drafts.size());
    for (const SectorEditorSoundDraft& draft : state_.drafts) {
        state_.listLabelStorage.push_back(draft.definition.id + "  ["
                + TypeLabel(draft.definition.type) + "]");
    }
    state_.listLabels.reserve(state_.listLabelStorage.size());
    for (const std::string& label : state_.listLabelStorage) {
        state_.listLabels.push_back(label.c_str());
    }
}

bool SectorEditorSoundEditorService::ValidateDrafts(std::string& error) const
{
    std::unordered_set<std::string> ids;
    ids.reserve(state_.drafts.size());
    for (const SectorEditorSoundDraft& draft : state_.drafts) {
        if (draft.definition.id.size() >= sizeof(state_.idBuffer)
                || !IsValidSoundId(draft.definition.id)) {
            error = "Invalid sound ID '" + draft.definition.id + "'";
            return false;
        }
        if (!ids.insert(draft.definition.id).second) {
            error = "Duplicate sound ID '" + draft.definition.id + "'";
            return false;
        }
        if (!IsValidSoundPath(draft.definition.path)) {
            error = "Sound '" + draft.definition.id
                    + "' requires a relative .ogg, .wav, or .mp3 path";
            return false;
        }
        const std::vector<std::string> uses = UsageLabels(draft.definition.id);
        if (uses.empty()) continue;
        for (const SectorAuthoringFaceAnchor& anchor : authoringGraph_.faceAnchors) {
            if (anchor.roomtone.mode == SectorRoomtoneMode::Play
                    && anchor.roomtone.soundId == draft.definition.id
                    && draft.definition.type != SectorSoundType::Music) {
                error = "Roomtone sound '" + draft.definition.id + "' must be Music";
                return false;
            }
        }
        if (draft.definition.type != SectorSoundType::Sound) {
            for (const SectorPlacedRuntimeObject& object : topologyMap_.runtimeObjects) {
                if (object.kind == "door" && (object.door.openSoundId == draft.definition.id
                        || object.door.closeSoundId == draft.definition.id)) {
                    error = "Door sound '" + draft.definition.id + "' must be Sound";
                    return false;
                }
            }
        }
    }
    return true;
}

void SectorEditorSoundEditorService::SyncCompiledAudio(
        const SectorLevelAudioSettings& settings)
{
    topologyMap_.audioSettings = settings;
    if (derivation_.authoringDerivation.success) {
        derivation_.authoringDerivation.topology.audioSettings = settings;
    }
    if (derivation_.lastValidAuthoringDerivedTopology.has_value()) {
        derivation_.lastValidAuthoringDerivedTopology->audioSettings = settings;
    }
}

bool SectorEditorSoundEditorService::SaveAndClose()
{
    if (!state_.open) return false;
    if (SelectedDraft() != nullptr && !ApplyIdBuffer()) return false;
    std::string error;
    if (!ValidateDrafts(error)) {
        state_.validationMessage = error;
        statusText_ = error;
        return false;
    }
    SectorLevelAudioSettings edited = authoringGraph_.audioSettings;
    edited.soundsById.clear();
    edited.soundsById.reserve(state_.drafts.size());
    for (const SectorEditorSoundDraft& draft : state_.drafts) {
        edited.soundsById.emplace(draft.definition.id, draft.definition);
    }
    const bool changed = !SameDefinitions(
            edited.soundsById, authoringGraph_.audioSettings.soundsById);
    if (changed) {
        authoringGraph_.audioSettings = edited;
        SyncCompiledAudio(edited);
        lifecycle_.topologyDocumentDirty = true;
        lifecycle_.hasUnsavedChanges = true;
    }
    Close();
    statusText_ = changed ? "Map sounds updated" : "Map sounds unchanged";
    return true;
}

bool SectorEditorSoundEditorService::SetRoomtoneFadeMilliseconds(int milliseconds)
{
    if (milliseconds < 0 || milliseconds > 60000
            || authoringGraph_.audioSettings.roomtoneFadeMilliseconds == milliseconds) {
        return false;
    }
    authoringGraph_.audioSettings.roomtoneFadeMilliseconds = milliseconds;
    SyncCompiledAudio(authoringGraph_.audioSettings);
    lifecycle_.topologyDocumentDirty = true;
    lifecycle_.hasUnsavedChanges = true;
    statusText_ = "Updated default roomtone fade";
    return true;
}

} // namespace game
