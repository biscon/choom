#include "sector_editor/weapons/SectorEditorWeaponEditorService.h"

#include "engine/assets/AssetManager.h"

#include <algorithm>
#include <cstring>
#include <unordered_set>

namespace game {
namespace {

void CopyBuffer(char* destination, size_t size, const std::string& value)
{
    if (size == 0) return;
    std::strncpy(destination, value.c_str(), size - 1);
    destination[size - 1] = '\0';
}

} // namespace

SectorEditorWeaponEditorService::SectorEditorWeaponEditorService(
        SectorEditorWeaponEditorState& state,
        SectorEditorWeaponEditorSessionState& session,
        FpsWeaponRegistry& registry,
        FpsApplicationSettings& applicationSettings,
        std::string& statusText,
        std::filesystem::path registryPath,
        std::filesystem::path applicationSettingsPath)
    : state_(state)
    , session_(session)
    , registry_(registry)
    , applicationSettings_(applicationSettings)
    , statusText_(statusText)
    , registryPath_(std::move(registryPath))
    , applicationSettingsPath_(std::move(applicationSettingsPath))
{
}

bool SectorEditorWeaponEditorService::Open(
        std::string_view activeWeaponId,
        bool fromPreview3D)
{
    if (state_.open) return true;
    state_ = SectorEditorWeaponEditorState{};
    state_.open = true;
    state_.openedFromPreview3D = fromPreview3D;
    state_.draftRegistry = registry_;
    ApplyFpsApplicationWeaponOverrides(state_.draftRegistry, applicationSettings_);
    state_.previewApplicationSettings = applicationSettings_;
    state_.previewApplicationSettings.weapons.clear();
    state_.originalActiveWeaponId = std::string(activeWeaponId);
    RebuildListLabels();

    const std::string desired = !session_.selectedWeaponId.empty()
            ? session_.selectedWeaponId
            : (!activeWeaponId.empty()
                    ? std::string(activeWeaponId)
                    : state_.draftRegistry.initialWeaponId);
    const auto found = std::find_if(
            state_.draftRegistry.weapons.begin(),
            state_.draftRegistry.weapons.end(),
            [&desired](const FpsWeaponDefinition& weapon) {
                return weapon.id == desired;
            });
    state_.selectedIndex = found == state_.draftRegistry.weapons.end()
            ? (state_.draftRegistry.weapons.empty() ? -1 : 0)
            : static_cast<int>(std::distance(
                    state_.draftRegistry.weapons.begin(), found));
    SyncBuffersFromSelection();
    state_.previewReloadRequested = state_.openedFromPreview3D
            && SelectedWeaponId() != activeWeaponId;
    statusText_ = "Weapon Editor opened";
    return true;
}

void SectorEditorWeaponEditorService::Cancel()
{
    if (!state_.open) return;
    statusText_ = "Weapon definition changes discarded";
    Close();
}

bool SectorEditorWeaponEditorService::SaveAndClose(engine::AssetManager& assets)
{
    std::string error;
    if (!ValidateFpsWeaponRegistry(state_.draftRegistry, &error)) {
        state_.validationMessage = error;
        statusText_ = error;
        return false;
    }
    if (!SaveFpsWeaponRegistry(
                registryPath_.generic_string(),
                state_.draftRegistry,
                &error)) {
        state_.validationMessage = error;
        statusText_ = error;
        return false;
    }
    FpsApplicationSettings migratedSettings = applicationSettings_;
    migratedSettings.weapons.clear();
    if (!SaveFpsApplicationSettings(
                applicationSettingsPath_.generic_string(),
                migratedSettings,
                &error)) {
        state_.validationMessage = error;
        statusText_ = error;
        return false;
    }

    session_.selectedWeaponId = SelectedWeaponId();
    registry_ = state_.draftRegistry;
    applicationSettings_ = std::move(migratedSettings);
    RequestFpsWeaponAudioAssets(assets, registry_);
    statusText_ = "Weapon definitions saved";
    Close();
    return true;
}

void SectorEditorWeaponEditorService::Shutdown()
{
    state_ = SectorEditorWeaponEditorState{};
    session_ = SectorEditorWeaponEditorSessionState{};
}

FpsWeaponDefinition* SectorEditorWeaponEditorService::SelectedWeapon()
{
    return state_.selectedIndex >= 0
                    && state_.selectedIndex < static_cast<int>(
                            state_.draftRegistry.weapons.size())
            ? &state_.draftRegistry.weapons[static_cast<size_t>(
                    state_.selectedIndex)]
            : nullptr;
}

const FpsWeaponDefinition* SectorEditorWeaponEditorService::SelectedWeapon() const
{
    return state_.selectedIndex >= 0
                    && state_.selectedIndex < static_cast<int>(
                            state_.draftRegistry.weapons.size())
            ? &state_.draftRegistry.weapons[static_cast<size_t>(
                    state_.selectedIndex)]
            : nullptr;
}

bool SectorEditorWeaponEditorService::SelectIndex(int index)
{
    if (index < 0
            || index >= static_cast<int>(state_.draftRegistry.weapons.size())) {
        return false;
    }
    if (state_.selectedIndex == index) return true;
    state_.selectedIndex = index;
    session_.selectedWeaponId = SelectedWeaponId();
    session_.formScroll = engine::UIScrollState{};
    state_.validationMessage.clear();
    state_.warningMessage.clear();
    SyncBuffersFromSelection();
    RequestPreviewReload();
    return true;
}

std::string SectorEditorWeaponEditorService::UniqueId(std::string base) const
{
    if (base.empty()) base = "new_weapon";
    std::unordered_set<std::string> used;
    for (const FpsWeaponDefinition& weapon : state_.draftRegistry.weapons) {
        used.insert(weapon.id);
    }
    if (used.find(base) == used.end()) return base;
    for (int suffix = 2;; ++suffix) {
        const std::string candidate = base + "_" + std::to_string(suffix);
        if (used.find(candidate) == used.end()) return candidate;
    }
}

void SectorEditorWeaponEditorService::AddDefault()
{
    FpsWeaponDefinition weapon = MakeDefaultFpsWeaponDefinition();
    weapon.id = UniqueId(weapon.id);
    state_.draftRegistry.weapons.push_back(std::move(weapon));
    state_.selectedIndex = static_cast<int>(state_.draftRegistry.weapons.size()) - 1;
    session_.selectedWeaponId = SelectedWeaponId();
    session_.formScroll = {};
    RebuildListLabels();
    SyncBuffersFromSelection();
    RequestPreviewReload();
}

void SectorEditorWeaponEditorService::DuplicateSelected()
{
    const FpsWeaponDefinition* selected = SelectedWeapon();
    if (selected == nullptr) return;
    FpsWeaponDefinition copy = *selected;
    copy.id = UniqueId(selected->id + "_copy");
    state_.draftRegistry.weapons.push_back(std::move(copy));
    state_.selectedIndex = static_cast<int>(state_.draftRegistry.weapons.size()) - 1;
    session_.selectedWeaponId = SelectedWeaponId();
    session_.formScroll = {};
    RebuildListLabels();
    SyncBuffersFromSelection();
    RequestPreviewReload();
}

void SectorEditorWeaponEditorService::RequestDeleteSelected()
{
    const FpsWeaponDefinition* selected = SelectedWeapon();
    if (selected == nullptr) return;
    if (state_.draftRegistry.weapons.size() <= 1) {
        state_.validationMessage = "A weapon registry must contain at least one weapon";
        return;
    }
    state_.deleteConfirmationOpen = true;
    state_.deleteConfirmationId = selected->id;
}

void SectorEditorWeaponEditorService::CancelDelete()
{
    state_.deleteConfirmationOpen = false;
    state_.deleteConfirmationId.clear();
}

void SectorEditorWeaponEditorService::ConfirmDeleteSelected()
{
    const FpsWeaponDefinition* selected = SelectedWeapon();
    if (selected == nullptr || state_.draftRegistry.weapons.size() <= 1) {
        CancelDelete();
        return;
    }
    const int removedIndex = state_.selectedIndex;
    const bool removedInitial = selected->id == state_.draftRegistry.initialWeaponId;
    state_.draftRegistry.weapons.erase(
            state_.draftRegistry.weapons.begin() + removedIndex);
    state_.selectedIndex = std::min(
            removedIndex,
            static_cast<int>(state_.draftRegistry.weapons.size()) - 1);
    if (removedInitial) {
        state_.draftRegistry.initialWeaponId = SelectedWeapon()->id;
    }
    session_.selectedWeaponId = SelectedWeaponId();
    session_.formScroll = {};
    CancelDelete();
    RebuildListLabels();
    SyncBuffersFromSelection();
    RequestPreviewReload();
}

void SectorEditorWeaponEditorService::SetSelectedInitial()
{
    const FpsWeaponDefinition* selected = SelectedWeapon();
    if (selected == nullptr) return;
    state_.draftRegistry.initialWeaponId = selected->id;
    RebuildListLabels();
    state_.validationMessage.clear();
}

void SectorEditorWeaponEditorService::ApplyIdBuffer()
{
    FpsWeaponDefinition* selected = SelectedWeapon();
    if (selected == nullptr) return;
    const std::string oldId = selected->id;
    selected->id = state_.idBuffer;
    if (state_.draftRegistry.initialWeaponId == oldId) {
        state_.draftRegistry.initialWeaponId = selected->id;
    }
    session_.selectedWeaponId = selected->id;
    RebuildListLabels();
    RequestPreviewReload();
}

void SectorEditorWeaponEditorService::ApplyArmsModelPathBuffer()
{
    SetArmsModelPath(state_.armsModelPathBuffer);
}

void SectorEditorWeaponEditorService::ApplyIdleAnimationBuffer()
{
    FpsWeaponDefinition* selected = SelectedWeapon();
    if (selected == nullptr) return;
    selected->viewmodel.idleAnimation = state_.idleAnimationBuffer;
    RequestPreviewReload();
}

void SectorEditorWeaponEditorService::ApplyAttachmentModelPathBuffer()
{
    SetAttachmentModelPath(state_.attachmentModelPathBuffer);
}

void SectorEditorWeaponEditorService::ApplyAttachmentBoneBuffer()
{
    FpsWeaponDefinition* selected = SelectedWeapon();
    if (selected == nullptr) return;
    selected->viewmodel.attachment.boneName = state_.attachmentBoneBuffer;
    RequestPreviewReload();
}

void SectorEditorWeaponEditorService::ApplyShootSoundBuffer(
        engine::AssetManager& assets)
{
    SetShootSoundPath(state_.shootSoundBuffer, assets);
}

void SectorEditorWeaponEditorService::SetArmsModelPath(const std::string& path)
{
    FpsWeaponDefinition* selected = SelectedWeapon();
    if (selected == nullptr) return;
    selected->viewmodel.modelPath = path;
    CopyBuffer(state_.armsModelPathBuffer, sizeof(state_.armsModelPathBuffer), path);
    RequestPreviewReload();
}

void SectorEditorWeaponEditorService::SetAttachmentModelPath(
        const std::string& path)
{
    FpsWeaponDefinition* selected = SelectedWeapon();
    if (selected == nullptr) return;
    selected->viewmodel.attachment.modelPath = path;
    CopyBuffer(
            state_.attachmentModelPathBuffer,
            sizeof(state_.attachmentModelPathBuffer),
            path);
    RequestPreviewReload();
}

void SectorEditorWeaponEditorService::SetShootSoundPath(
        const std::string& path,
        engine::AssetManager& assets)
{
    FpsWeaponDefinition* selected = SelectedWeapon();
    if (selected == nullptr) return;
    selected->firing.shootSoundPath = path;
    selected->firing.shootSound = engine::NullSoundHandle();
    if (!path.empty()) {
        FpsWeaponRegistry one;
        one.initialWeaponId = selected->id;
        one.weapons.push_back(*selected);
        RequestFpsWeaponAudioAssets(assets, one);
        selected->firing.shootSound = one.weapons.front().firing.shootSound;
    }
    CopyBuffer(state_.shootSoundBuffer, sizeof(state_.shootSoundBuffer), path);
    state_.validationMessage.clear();
}

bool SectorEditorWeaponEditorService::ConsumePreviewReloadRequest()
{
    const bool requested = state_.previewReloadRequested;
    state_.previewReloadRequested = false;
    return requested;
}

std::string SectorEditorWeaponEditorService::SelectedWeaponId() const
{
    const FpsWeaponDefinition* selected = SelectedWeapon();
    return selected == nullptr ? std::string{} : selected->id;
}

void SectorEditorWeaponEditorService::Close()
{
    const std::string selected = SelectedWeaponId();
    if (!selected.empty()) session_.selectedWeaponId = selected;
    state_ = SectorEditorWeaponEditorState{};
}

void SectorEditorWeaponEditorService::SyncBuffersFromSelection()
{
    const FpsWeaponDefinition* selected = SelectedWeapon();
    const FpsWeaponDefinition empty;
    const FpsWeaponDefinition& weapon = selected == nullptr ? empty : *selected;
    CopyBuffer(state_.idBuffer, sizeof(state_.idBuffer), weapon.id);
    CopyBuffer(
            state_.armsModelPathBuffer,
            sizeof(state_.armsModelPathBuffer),
            weapon.viewmodel.modelPath);
    CopyBuffer(
            state_.idleAnimationBuffer,
            sizeof(state_.idleAnimationBuffer),
            weapon.viewmodel.idleAnimation);
    CopyBuffer(
            state_.attachmentModelPathBuffer,
            sizeof(state_.attachmentModelPathBuffer),
            weapon.viewmodel.attachment.modelPath);
    CopyBuffer(
            state_.attachmentBoneBuffer,
            sizeof(state_.attachmentBoneBuffer),
            weapon.viewmodel.attachment.boneName);
    CopyBuffer(
            state_.shootSoundBuffer,
            sizeof(state_.shootSoundBuffer),
            weapon.firing.shootSoundPath);
    state_.floatInputs = {};
    state_.intInputs = {};
}

void SectorEditorWeaponEditorService::RebuildListLabels()
{
    state_.listLabelStorage.clear();
    state_.listLabelStorage.reserve(state_.draftRegistry.weapons.size());
    for (const FpsWeaponDefinition& weapon : state_.draftRegistry.weapons) {
        state_.listLabelStorage.push_back(
                weapon.id == state_.draftRegistry.initialWeaponId
                        ? weapon.id + "  [initial]"
                        : weapon.id);
    }
    state_.listLabels.clear();
    state_.listLabels.reserve(state_.listLabelStorage.size());
    for (const std::string& label : state_.listLabelStorage) {
        state_.listLabels.push_back(label.c_str());
    }
}

void SectorEditorWeaponEditorService::RequestPreviewReload()
{
    state_.previewReloadRequested = state_.openedFromPreview3D;
    state_.validationMessage.clear();
}

} // namespace game
