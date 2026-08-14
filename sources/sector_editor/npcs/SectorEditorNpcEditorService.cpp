#include "sector_editor/npcs/SectorEditorNpcEditorService.h"

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorAssetPaths.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_set>

namespace game {
namespace {

std::string LowerAscii(std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());
    for (const unsigned char character : value) {
        lowered.push_back(static_cast<char>(std::tolower(character)));
    }
    return lowered;
}

void CopyBuffer(char* destination, size_t capacity, const std::string& source)
{
    if (capacity == 0) return;
    const size_t count = std::min(capacity - 1, source.size());
    std::memcpy(destination, source.data(), count);
    destination[count] = '\0';
}

bool SameAction(
        const NpcActionDefinition& left,
        const NpcActionDefinition& right)
{
    return left.animation == right.animation
            && left.animationSpeed == right.animationSpeed
            && left.movementSpeed == right.movementSpeed;
}

bool SameDefinition(const NpcDefinition& left, const NpcDefinition& right)
{
    if (left.id != right.id
            || left.name != right.name
            || left.hostile != right.hostile
            || left.modelPath != right.modelPath) {
        return false;
    }
    for (size_t i = 0; i < kNpcActionCount; ++i) {
        if (!SameAction(left.actions[i], right.actions[i])) return false;
    }
    return true;
}

} // namespace

SectorEditorNpcEditorService::SectorEditorNpcEditorService(
        SectorEditorNpcEditorState& state,
        SectorEditorNpcEditorSessionState& session,
        std::string& statusText,
        std::filesystem::path definitionsRoot)
    : state_(state)
    , session_(session)
    , statusText_(statusText)
    , definitionsRoot_(std::move(definitionsRoot))
{
}

bool SectorEditorNpcEditorService::Open()
{
    if (state_.open) return true;
    NpcDefinitionCatalog catalog;
    const bool catalogValid = DiscoverNpcDefinitions(definitionsRoot_, catalog);

    state_ = SectorEditorNpcEditorState{};
    state_.open = true;
    state_.catalogErrors = std::move(catalog.errors);
    state_.drafts.reserve(catalog.definitions.size());
    for (NpcDefinition& definition : catalog.definitions) {
        SectorEditorNpcDefinitionDraft draft;
        draft.originalId = definition.id;
        draft.originalDefinition = definition;
        draft.definition = std::move(definition);
        state_.drafts.push_back(std::move(draft));
    }
    RebuildListLabels();

    state_.selectedIndex = -1;
    if (!session_.selectedNpcId.empty()) {
        const auto found = std::find_if(
                state_.drafts.begin(),
                state_.drafts.end(),
                [this](const SectorEditorNpcDefinitionDraft& draft) {
                    return draft.definition.id == session_.selectedNpcId;
                });
        if (found != state_.drafts.end()) {
            state_.selectedIndex = static_cast<int>(
                    std::distance(state_.drafts.begin(), found));
        }
    }
    if (state_.selectedIndex < 0 && !state_.drafts.empty()) {
        state_.selectedIndex = 0;
        session_.selectedNpcId = state_.drafts[0].definition.id;
    }
    if (state_.drafts.empty()) session_.selectedNpcId.clear();
    SyncBuffersFromSelection();
    state_.validationMessage = state_.catalogErrors.empty()
            ? std::string{}
            : "Fix malformed or duplicate NPC definition files before saving";
    statusText_ = catalogValid
            ? "NPC Editor opened"
            : "NPC Editor opened with catalog errors";
    return catalogValid;
}

void SectorEditorNpcEditorService::Cancel(engine::AssetManager* assets)
{
    if (!state_.open) return;
    const SectorEditorNpcDefinitionDraft* selected = SelectedDraft();
    session_.selectedNpcId = selected == nullptr || selected->isNew
            ? std::string{}
            : selected->originalId;
    statusText_ = "NPC definition changes discarded";
    Close(assets);
}

bool SectorEditorNpcEditorService::SaveAndClose(engine::AssetManager* assets)
{
    std::string error;
    if (!ValidateDrafts(error)) {
        state_.validationMessage = error;
        statusText_ = error;
        return false;
    }

    std::unordered_set<std::string> finalIds;
    for (const SectorEditorNpcDefinitionDraft& draft : state_.drafts) {
        finalIds.insert(draft.definition.id);
        if (!draft.isNew && SameDefinition(draft.definition, draft.originalDefinition)) {
            continue;
        }
        if (!SaveNpcDefinition(
                    DefinitionPath(draft.definition.id),
                    draft.definition,
                    error)) {
            state_.validationMessage = error;
            statusText_ = error;
            return false;
        }
    }

    std::vector<std::string> obsoleteIds = state_.stagedDeleteIds;
    for (const SectorEditorNpcDefinitionDraft& draft : state_.drafts) {
        if (!draft.originalId.empty()
                && draft.originalId != draft.definition.id) {
            obsoleteIds.push_back(draft.originalId);
        }
    }
    std::sort(obsoleteIds.begin(), obsoleteIds.end());
    obsoleteIds.erase(
            std::unique(obsoleteIds.begin(), obsoleteIds.end()),
            obsoleteIds.end());
    for (const std::string& id : obsoleteIds) {
        if (finalIds.find(id) != finalIds.end()) continue;
        std::error_code removeError;
        const std::filesystem::path path = DefinitionPath(id);
        bool aliasesFinalPath = false;
        for (const SectorEditorNpcDefinitionDraft& draft : state_.drafts) {
            std::error_code equivalentError;
            if (std::filesystem::equivalent(
                        path,
                        DefinitionPath(draft.definition.id),
                        equivalentError)
                    && !equivalentError) {
                aliasesFinalPath = true;
                break;
            }
        }
        if (aliasesFinalPath) continue;
        const bool removed = std::filesystem::remove(path, removeError);
        if (removeError) {
            state_.validationMessage = "Could not delete NPC definition '"
                    + path.generic_string() + "': " + removeError.message();
            statusText_ = state_.validationMessage;
            return false;
        }
        (void)removed;
    }

    const SectorEditorNpcDefinitionDraft* selected = SelectedDraft();
    session_.selectedNpcId = selected == nullptr
            ? std::string{}
            : selected->definition.id;
    statusText_ = "NPC definitions saved";
    Close(assets);
    return true;
}

void SectorEditorNpcEditorService::Shutdown(engine::AssetManager& assets)
{
    ResetTransientStatePreservingSession(&assets);
    session_ = SectorEditorNpcEditorSessionState{};
}

SectorEditorNpcDefinitionDraft* SectorEditorNpcEditorService::SelectedDraft()
{
    return state_.selectedIndex >= 0
                    && state_.selectedIndex < static_cast<int>(state_.drafts.size())
            ? &state_.drafts[static_cast<size_t>(state_.selectedIndex)]
            : nullptr;
}

const SectorEditorNpcDefinitionDraft* SectorEditorNpcEditorService::SelectedDraft() const
{
    return state_.selectedIndex >= 0
                    && state_.selectedIndex < static_cast<int>(state_.drafts.size())
            ? &state_.drafts[static_cast<size_t>(state_.selectedIndex)]
            : nullptr;
}

bool SectorEditorNpcEditorService::SelectIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(state_.drafts.size())) return false;
    if (state_.selectedIndex == index) return true;
    state_.selectedIndex = index;
    session_.selectedNpcId = state_.drafts[static_cast<size_t>(index)].definition.id;
    state_.validationMessage.clear();
    state_.warningMessage.clear();
    state_.selectedModel = engine::NullModelHandle();
    state_.selectedModelPath.clear();
    state_.animationOptionStorage.clear();
    state_.animationOptions.clear();
    SyncBuffersFromSelection();
    return true;
}

void SectorEditorNpcEditorService::AddDefinition()
{
    std::unordered_set<std::string> used;
    for (const auto& draft : state_.drafts) used.insert(LowerAscii(draft.definition.id));
    std::string id = "new_npc";
    for (int suffix = 2; used.find(LowerAscii(id)) != used.end(); ++suffix) {
        id = "new_npc_" + std::to_string(suffix);
    }
    SectorEditorNpcDefinitionDraft draft;
    draft.definition = MakeDefaultNpcDefinition();
    draft.definition.id = id;
    draft.isNew = true;
    state_.drafts.push_back(std::move(draft));
    RebuildListLabels();
    state_.selectedIndex = static_cast<int>(state_.drafts.size()) - 1;
    session_.selectedNpcId = id;
    session_.formScroll = engine::UIScrollState{};
    state_.selectedModel = engine::NullModelHandle();
    state_.selectedModelPath.clear();
    state_.animationOptionStorage.clear();
    state_.animationOptions.clear();
    state_.validationMessage.clear();
    state_.warningMessage.clear();
    SyncBuffersFromSelection();
}

void SectorEditorNpcEditorService::RequestDeleteSelected()
{
    const SectorEditorNpcDefinitionDraft* draft = SelectedDraft();
    if (draft == nullptr) return;
    state_.deleteConfirmationOpen = true;
    state_.deleteConfirmationId = draft->definition.id;
}

void SectorEditorNpcEditorService::CancelDelete()
{
    state_.deleteConfirmationOpen = false;
    state_.deleteConfirmationId.clear();
}

void SectorEditorNpcEditorService::ConfirmDeleteSelected()
{
    SectorEditorNpcDefinitionDraft* selected = SelectedDraft();
    if (selected == nullptr) {
        CancelDelete();
        return;
    }
    const int removedIndex = state_.selectedIndex;
    if (!selected->originalId.empty()) {
        state_.stagedDeleteIds.push_back(selected->originalId);
    }
    state_.drafts.erase(state_.drafts.begin() + removedIndex);
    state_.selectedIndex = state_.drafts.empty()
            ? -1
            : std::min(removedIndex, static_cast<int>(state_.drafts.size()) - 1);
    const SectorEditorNpcDefinitionDraft* replacement = SelectedDraft();
    session_.selectedNpcId = replacement == nullptr
            ? std::string{}
            : replacement->definition.id;
    state_.selectedModel = engine::NullModelHandle();
    state_.selectedModelPath.clear();
    state_.animationOptionStorage.clear();
    state_.animationOptions.clear();
    state_.validationMessage.clear();
    state_.warningMessage.clear();
    CancelDelete();
    RebuildListLabels();
    SyncBuffersFromSelection();
}

void SectorEditorNpcEditorService::ApplyIdBuffer()
{
    SectorEditorNpcDefinitionDraft* draft = SelectedDraft();
    if (draft == nullptr) return;
    draft->definition.id = state_.idBuffer;
    RebuildListLabels();
    state_.validationMessage.clear();
}

void SectorEditorNpcEditorService::ApplyNameBuffer()
{
    SectorEditorNpcDefinitionDraft* draft = SelectedDraft();
    if (draft == nullptr) return;
    draft->definition.name = state_.nameBuffer;
    RebuildListLabels();
    state_.validationMessage.clear();
}

void SectorEditorNpcEditorService::SetSelectedHostile(bool hostile)
{
    SectorEditorNpcDefinitionDraft* draft = SelectedDraft();
    if (draft == nullptr) return;
    draft->definition.hostile = hostile;
    state_.validationMessage.clear();
}

void SectorEditorNpcEditorService::SetSelectedModelPath(
        const std::string& modelPath,
        engine::AssetManager& assets)
{
    SectorEditorNpcDefinitionDraft* draft = SelectedDraft();
    if (draft == nullptr || draft->definition.modelPath == modelPath) return;
    draft->definition.modelPath = modelPath;
    state_.selectedModel = engine::NullModelHandle();
    state_.selectedModelPath.clear();
    state_.animationOptionStorage.clear();
    state_.animationOptions.clear();
    state_.validationMessage.clear();
    EnsureSelectedModelRequested(assets);
}

void SectorEditorNpcEditorService::SetSelectedAnimation(
        NpcAction action,
        const std::string& animation)
{
    SectorEditorNpcDefinitionDraft* draft = SelectedDraft();
    if (draft == nullptr) return;
    GetNpcAction(draft->definition, action).animation = animation;
    state_.validationMessage.clear();
}

void SectorEditorNpcEditorService::SetSelectedAnimationSpeed(
        NpcAction action,
        float speed)
{
    SectorEditorNpcDefinitionDraft* draft = SelectedDraft();
    if (draft == nullptr) return;
    GetNpcAction(draft->definition, action).animationSpeed = speed;
    state_.validationMessage.clear();
}

void SectorEditorNpcEditorService::SetSelectedMovementSpeed(
        NpcAction action,
        float speed)
{
    SectorEditorNpcDefinitionDraft* draft = SelectedDraft();
    if (draft == nullptr) return;
    GetNpcAction(draft->definition, action).movementSpeed = speed;
    state_.validationMessage.clear();
}

void SectorEditorNpcEditorService::EnsureSelectedModelRequested(
        engine::AssetManager& assets)
{
    const SectorEditorNpcDefinitionDraft* draft = SelectedDraft();
    if (draft == nullptr || draft->definition.modelPath.empty()) return;
    if (state_.selectedModelPath == draft->definition.modelPath
            && !engine::IsNull(state_.selectedModel)) {
        return;
    }
    if (engine::IsNull(state_.modelScope)) {
        state_.modelScope = assets.CreateScope("sector_editor_npc_models");
        if (engine::IsNull(state_.modelScope)) {
            state_.warningMessage = "Could not create NPC editor model asset scope";
            return;
        }
    }
    const std::string resolvedPath = ResolveSectorAssetPath(draft->definition.modelPath);
    state_.selectedModel = assets.RequestModel(
            state_.modelScope,
            draft->definition.modelPath.c_str(),
            resolvedPath.c_str(),
            engine::ModelLoad_Animations);
    state_.selectedModelPath = draft->definition.modelPath;
    state_.warningMessage = engine::IsNull(state_.selectedModel)
            ? "Could not request the selected NPC model"
            : std::string{};
    state_.animationOptionStorage.clear();
    state_.animationOptions.clear();
}

void SectorEditorNpcEditorService::RefreshAnimationOptions(
        engine::AssetManager& assets)
{
    EnsureSelectedModelRequested(assets);
    const engine::ModelAsset* asset = assets.GetModelAsset(state_.selectedModel);
    if (asset == nullptr || !state_.animationOptionStorage.empty()) return;
    state_.animationOptionStorage.reserve(
            static_cast<size_t>(std::max(0, asset->animationCount)) + 1);
    state_.animationOptionStorage.emplace_back("<Unassigned>");
    for (int index = 0; index < asset->animationCount; ++index) {
        state_.animationOptionStorage.emplace_back(asset->animations[index].name);
    }
    state_.animationOptions.reserve(state_.animationOptionStorage.size());
    for (const std::string& option : state_.animationOptionStorage) {
        state_.animationOptions.push_back(option.c_str());
    }
}

bool SectorEditorNpcEditorService::SelectedModelReady(
        const engine::AssetManager& assets) const
{
    return !engine::IsNull(state_.selectedModel)
            && assets.GetModelAsset(state_.selectedModel) != nullptr;
}

bool SectorEditorNpcEditorService::SelectedModelFailed(
        const engine::AssetManager& assets) const
{
    return !engine::IsNull(state_.selectedModel)
            && assets.HasFailed(state_.selectedModel);
}

bool SectorEditorNpcEditorService::SelectedAnimationExists(
        const engine::AssetManager& assets,
        std::string_view animation) const
{
    if (animation.empty()) return false;
    const engine::ModelAsset* asset = assets.GetModelAsset(state_.selectedModel);
    if (asset == nullptr || asset->animations == nullptr) return false;
    for (int index = 0; index < asset->animationCount; ++index) {
        if (animation == asset->animations[index].name) return true;
    }
    return false;
}

void SectorEditorNpcEditorService::Close(engine::AssetManager* assets)
{
    ResetTransientStatePreservingSession(assets);
}

void SectorEditorNpcEditorService::ResetTransientStatePreservingSession(
        engine::AssetManager* assets)
{
    if (assets != nullptr && !engine::IsNull(state_.modelScope)) {
        assets->UnloadScope(state_.modelScope);
    }
    state_ = SectorEditorNpcEditorState{};
}

void SectorEditorNpcEditorService::SyncBuffersFromSelection()
{
    const SectorEditorNpcDefinitionDraft* draft = SelectedDraft();
    CopyBuffer(
            state_.idBuffer,
            sizeof(state_.idBuffer),
            draft == nullptr ? std::string{} : draft->definition.id);
    CopyBuffer(
            state_.nameBuffer,
            sizeof(state_.nameBuffer),
            draft == nullptr ? std::string{} : draft->definition.name);
    state_.animationSpeedInputs = {};
    state_.movementSpeedInputs = {};
}

void SectorEditorNpcEditorService::RebuildListLabels()
{
    state_.listLabelStorage.clear();
    state_.listLabelStorage.reserve(state_.drafts.size());
    for (const SectorEditorNpcDefinitionDraft& draft : state_.drafts) {
        std::string label = draft.definition.id.empty()
                ? "<new NPC>"
                : draft.definition.id;
        if (!draft.definition.name.empty()) {
            label += " - " + draft.definition.name;
        }
        state_.listLabelStorage.push_back(std::move(label));
    }
    state_.listLabels.clear();
    state_.listLabels.reserve(state_.listLabelStorage.size());
    for (const std::string& label : state_.listLabelStorage) {
        state_.listLabels.push_back(label.c_str());
    }
}

bool SectorEditorNpcEditorService::ValidateDrafts(std::string& outError) const
{
    if (!state_.catalogErrors.empty()) {
        outError = "Fix malformed or duplicate NPC definition files before saving";
        return false;
    }
    std::unordered_set<std::string> ids;
    for (const SectorEditorNpcDefinitionDraft& draft : state_.drafts) {
        if (!ValidateNpcDefinition(draft.definition, outError)) return false;
        if (!ids.insert(LowerAscii(draft.definition.id)).second) {
            outError = "NPC IDs must be unique, ignoring letter case";
            return false;
        }
    }
    outError.clear();
    return true;
}

std::filesystem::path SectorEditorNpcEditorService::DefinitionPath(
        std::string_view id) const
{
    return definitionsRoot_ / (std::string{id} + ".json");
}

} // namespace game
