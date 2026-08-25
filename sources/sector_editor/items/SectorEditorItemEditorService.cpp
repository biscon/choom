#include "sector_editor/items/SectorEditorItemEditorService.h"

#include "sector_editor/items/SectorItemReferenceScanner.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace game {
namespace {

template <size_t Size>
void Copy(std::array<char, Size>& destination, const std::string& source)
{
    const size_t count = std::min(Size - 1, source.size());
    std::memcpy(destination.data(), source.data(), count);
    destination[count] = '\0';
}

} // namespace

SectorEditorItemEditorService::SectorEditorItemEditorService(
        SectorEditorItemEditorState& state,
        SectorEditorItemEditorSessionState& session,
        ItemRegistry& registry,
        const FpsWeaponRegistry& weapons,
        bool gameSessionExists,
        std::string& statusText,
        std::filesystem::path registryPath,
        std::filesystem::path levelsRoot)
    : state_(state)
    , session_(session)
    , registry_(registry)
    , weapons_(weapons)
    , gameSessionExists_(gameSessionExists)
    , statusText_(statusText)
    , registryPath_(std::move(registryPath))
    , levelsRoot_(std::move(levelsRoot))
{
}

bool SectorEditorItemEditorService::Open()
{
    if (state_.open) return true;
    state_ = SectorEditorItemEditorState{};
    state_.open = true;
    state_.draftRegistry = registry_;
    RebuildLabels();
    if (!session_.selectedItemId.empty()) {
        const auto found = std::find_if(
                state_.draftRegistry.items.begin(),
                state_.draftRegistry.items.end(),
                [this](const ItemDefinition& definition) {
                    return definition.id == session_.selectedItemId;
                });
        if (found != state_.draftRegistry.items.end()) {
            state_.selectedIndex = static_cast<int>(std::distance(
                    state_.draftRegistry.items.begin(), found));
        }
    }
    if (state_.selectedIndex < 0 && !state_.draftRegistry.items.empty()) {
        state_.selectedIndex = 0;
    }
    SyncBuffers();
    statusText_ = "Item Editor opened";
    return true;
}

void SectorEditorItemEditorService::Cancel()
{
    if (!state_.open) return;
    statusText_ = "Item definition changes discarded";
    Close();
}

void SectorEditorItemEditorService::Shutdown()
{
    state_ = SectorEditorItemEditorState{};
    session_ = SectorEditorItemEditorSessionState{};
}

bool SectorEditorItemEditorService::SaveAndClose()
{
    ApplyTitleBuffer();
    ApplyDescriptionBuffer();
    ApplyModelPathBuffer();
    if (gameSessionExists_) {
        state_.validationMessage =
                "Item definitions cannot be saved while a game session exists";
        statusText_ = state_.validationMessage;
        return false;
    }
    std::string error;
    if (!ValidateItemRegistry(state_.draftRegistry, weapons_, error)
            || !ScanDeletedReferences(error)
            || !SaveItemRegistry(
                    registryPath_, state_.draftRegistry, weapons_, error)) {
        state_.validationMessage = error;
        statusText_ = error;
        return false;
    }
    session_.selectedItemId = SelectedItem() == nullptr
            ? std::string{} : SelectedItem()->id;
    const std::uint64_t nextRevision = registry_.revision
            == std::numeric_limits<std::uint64_t>::max()
            ? 1 : registry_.revision + 1;
    registry_ = state_.draftRegistry;
    registry_.revision = nextRevision;
    statusText_ = "Item definitions saved";
    Close();
    return true;
}

ItemDefinition* SectorEditorItemEditorService::SelectedItem()
{
    return state_.selectedIndex >= 0
                    && state_.selectedIndex
                            < static_cast<int>(state_.draftRegistry.items.size())
            ? &state_.draftRegistry.items[
                    static_cast<size_t>(state_.selectedIndex)]
            : nullptr;
}

const ItemDefinition* SectorEditorItemEditorService::SelectedItem() const
{
    return state_.selectedIndex >= 0
                    && state_.selectedIndex
                            < static_cast<int>(state_.draftRegistry.items.size())
            ? &state_.draftRegistry.items[
                    static_cast<size_t>(state_.selectedIndex)]
            : nullptr;
}

bool SectorEditorItemEditorService::SelectIndex(int index)
{
    if (index < 0
            || index >= static_cast<int>(state_.draftRegistry.items.size())) {
        return false;
    }
    state_.selectedIndex = index;
    session_.selectedItemId = state_.draftRegistry.items[
            static_cast<size_t>(index)].id;
    session_.formScroll = {};
    state_.validationMessage.clear();
    SyncBuffers();
    return true;
}

std::string SectorEditorItemEditorService::UniqueId() const
{
    const auto used = [this](const std::string& id) {
        return std::any_of(
                state_.draftRegistry.items.begin(),
                state_.draftRegistry.items.end(),
                [&id](const ItemDefinition& definition) {
                    return definition.id == id;
                });
    };
    if (!used("new_item")) return "new_item";
    for (int suffix = 2;; ++suffix) {
        const std::string candidate = "new_item_" + std::to_string(suffix);
        if (!used(candidate)) return candidate;
    }
}

void SectorEditorItemEditorService::AddItem()
{
    ItemDefinition definition = MakeDefaultItemDefinition();
    definition.id = UniqueId();
    state_.draftRegistry.items.push_back(std::move(definition));
    state_.selectedIndex = static_cast<int>(
            state_.draftRegistry.items.size()) - 1;
    session_.selectedItemId = SelectedItem()->id;
    session_.formScroll = {};
    RebuildLabels();
    SyncBuffers();
}

bool SectorEditorItemEditorService::RequestDeleteSelected()
{
    const ItemDefinition* selected = SelectedItem();
    if (selected == nullptr) return false;
    std::unordered_map<std::string, size_t> counts;
    std::string error;
    if (!CountItemDefinitionReferencesInLevels(
                levelsRoot_, {selected->id}, counts, error)) {
        state_.validationMessage = error;
        return false;
    }
    if (counts[selected->id] != 0) {
        state_.validationMessage = "Cannot delete '" + selected->id
                + "': it is referenced by the open or a saved level";
        return false;
    }
    state_.deleteConfirmationOpen = true;
    state_.deleteConfirmationId = selected->id;
    return true;
}

void SectorEditorItemEditorService::CancelDelete()
{
    state_.deleteConfirmationOpen = false;
    state_.deleteConfirmationId.clear();
}

void SectorEditorItemEditorService::ConfirmDeleteSelected()
{
    if (!state_.deleteConfirmationOpen || SelectedItem() == nullptr) return;
    state_.draftRegistry.items.erase(
            state_.draftRegistry.items.begin() + state_.selectedIndex);
    state_.selectedIndex = state_.draftRegistry.items.empty()
            ? -1
            : std::min(state_.selectedIndex,
                    static_cast<int>(state_.draftRegistry.items.size()) - 1);
    CancelDelete();
    RebuildLabels();
    SyncBuffers();
}

void SectorEditorItemEditorService::ApplyTitleBuffer()
{
    if (ItemDefinition* selected = SelectedItem()) {
        selected->title = state_.titleBuffer.data();
        RebuildLabels();
    }
}

void SectorEditorItemEditorService::ApplyDescriptionBuffer()
{
    if (ItemDefinition* selected = SelectedItem()) {
        selected->description = state_.descriptionBuffer.data();
    }
}

void SectorEditorItemEditorService::ApplyModelPathBuffer()
{
    SetModelPath(state_.modelPathBuffer.data());
}

void SectorEditorItemEditorService::SetModelPath(const std::string& path)
{
    if (ItemDefinition* selected = SelectedItem()) {
        selected->modelPath = path;
        Copy(state_.modelPathBuffer, path);
        state_.validationMessage.clear();
    }
}

void SectorEditorItemEditorService::SetType(ItemType type)
{
    ItemDefinition* selected = SelectedItem();
    if (selected == nullptr || selected->type == type) return;
    selected->type = type;
    selected->weaponId.clear();
    selected->healAmount = 0;
    selected->healOverTime = false;
    selected->healDurationSeconds = 0.0f;
    if (type == ItemType::Weapon || type == ItemType::Ammo) {
        if (!weapons_.weapons.empty()) {
            selected->weaponId = weapons_.weapons.front().id;
        }
    } else if (type == ItemType::Health) {
        selected->healAmount = 1;
    }
    state_.healAmountInput = {};
    state_.healDurationInput = {};
    state_.validationMessage.clear();
}

void SectorEditorItemEditorService::SetWeaponIndex(int index)
{
    ItemDefinition* selected = SelectedItem();
    if (selected == nullptr || index < 0
            || index >= static_cast<int>(weapons_.weapons.size())) {
        return;
    }
    selected->weaponId = weapons_.weapons[static_cast<size_t>(index)].id;
    state_.validationMessage.clear();
}

int SectorEditorItemEditorService::SelectedWeaponIndex() const
{
    const ItemDefinition* selected = SelectedItem();
    if (selected == nullptr) return -1;
    for (size_t index = 0; index < weapons_.weapons.size(); ++index) {
        if (weapons_.weapons[index].id == selected->weaponId) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void SectorEditorItemEditorService::Close()
{
    if (const ItemDefinition* selected = SelectedItem()) {
        session_.selectedItemId = selected->id;
    }
    state_ = SectorEditorItemEditorState{};
}

void SectorEditorItemEditorService::SyncBuffers()
{
    const ItemDefinition* selected = SelectedItem();
    const ItemDefinition empty;
    const ItemDefinition& definition = selected == nullptr ? empty : *selected;
    Copy(state_.titleBuffer, definition.title);
    Copy(state_.descriptionBuffer, definition.description);
    Copy(state_.modelPathBuffer, definition.modelPath);
    state_.weightInput = {};
    state_.healAmountInput = {};
    state_.healDurationInput = {};
}

void SectorEditorItemEditorService::RebuildLabels()
{
    state_.listLabelStorage.clear();
    state_.listLabelStorage.reserve(state_.draftRegistry.items.size());
    for (const ItemDefinition& definition : state_.draftRegistry.items) {
        state_.listLabelStorage.push_back(
                definition.title + "  [" + definition.id + "]");
    }
    state_.listLabels.clear();
    state_.listLabels.reserve(state_.listLabelStorage.size());
    for (const std::string& label : state_.listLabelStorage) {
        state_.listLabels.push_back(label.c_str());
    }
    state_.weaponLabelStorage.clear();
    state_.weaponLabels.clear();
    state_.weaponLabelStorage.reserve(weapons_.weapons.size());
    for (const FpsWeaponDefinition& weapon : weapons_.weapons) {
        state_.weaponLabelStorage.push_back(weapon.id);
    }
    state_.weaponLabels.reserve(state_.weaponLabelStorage.size());
    for (const std::string& label : state_.weaponLabelStorage) {
        state_.weaponLabels.push_back(label.c_str());
    }
}

bool SectorEditorItemEditorService::ScanDeletedReferences(
        std::string& error) const
{
    std::unordered_set<std::string> retained;
    for (const ItemDefinition& definition : state_.draftRegistry.items) {
        retained.insert(definition.id);
    }
    std::unordered_set<std::string> deleted;
    for (const ItemDefinition& definition : registry_.items) {
        if (retained.find(definition.id) == retained.end()) {
            deleted.insert(definition.id);
        }
    }
    if (deleted.empty()) return true;
    std::unordered_map<std::string, size_t> counts;
    if (!CountItemDefinitionReferencesInLevels(
                levelsRoot_, deleted, counts, error)) {
        return false;
    }
    for (const auto& entry : counts) {
        if (entry.second != 0) {
            error = "Cannot delete '" + entry.first
                    + "': it is referenced by the open or a saved level";
            return false;
        }
    }
    return true;
}

} // namespace game
