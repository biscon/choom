#include "sector_editor/player/SectorEditorPlayerSettingsService.h"

#include "engine/EngineContext.h"
#include "sector_demo/SectorAssetPaths.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_set>

namespace game {
namespace {

bool SamePlayerSounds(
        const PlayerSoundApplicationSettings& left,
        const PlayerSoundApplicationSettings& right)
{
    if (left.events.size() != right.events.size()) return false;
    for (size_t i = 0; i < left.events.size(); ++i) {
        const PlayerSoundEventSettings& a = left.events[i];
        const PlayerSoundEventSettings& b = right.events[i];
        if (a.id != b.id || a.set != b.set || a.volume != b.volume) {
            return false;
        }
    }
    return true;
}

bool SameFootsteps(
        const FootstepApplicationSettings& left,
        const FootstepApplicationSettings& right)
{
    return left.defaultSet == right.defaultSet
            && left.volume == right.volume
            && left.landingImpactVolumeMultiplier
                    == right.landingImpactVolumeMultiplier
            && left.noiseRadiusWorld == right.noiseRadiusWorld
            && left.landingNoiseRadiusWorld
                    == right.landingNoiseRadiusWorld;
}

void CopyEventIdToBuffer(SectorEditorPlayerSoundEventDraft& draft)
{
    std::snprintf(
            draft.idBuffer.data(),
            draft.idBuffer.size(),
            "%s",
            draft.value.id.c_str());
}

} // namespace

SectorEditorPlayerSettingsService::SectorEditorPlayerSettingsService(
        SectorEditorPlayerSettingsState& state,
        FpsApplicationSettings& settings,
        std::string& statusText,
        std::filesystem::path settingsPath)
    : state_(state)
    , settings_(settings)
    , statusText_(statusText)
    , settingsPath_(std::move(settingsPath))
{
}

void SectorEditorPlayerSettingsService::Open(
        engine::EngineContext& context,
        std::optional<SectorEditorPlayerSettingsTab> selectedTab)
{
    StopAudioPreview(context);
    const SectorEditorPlayerSettingsTab activeTab = state_.activeTab;
    state_ = SectorEditorPlayerSettingsState{};
    state_.open = true;
    state_.activeTab = selectedTab.value_or(activeTab);
    state_.draft = settings_;
    state_.soundEvents.reserve(settings_.playerSounds.events.size() + 4u);
    for (const PlayerSoundEventSettings& event : settings_.playerSounds.events) {
        SectorEditorPlayerSoundEventDraft draft;
        draft.value = event;
        CopyEventIdToBuffer(draft);
        state_.soundEvents.push_back(std::move(draft));
    }
    state_.footstepCatalog = DiscoverSoundSetCatalog(
            ASSETS_PATH "audio/footsteps",
            "footsteps");
    state_.playerSoundCatalog = DiscoverSoundSetCatalog(
            ASSETS_PATH "audio/player",
            "player");
    BuildCatalogLabels();
}

void SectorEditorPlayerSettingsService::Cancel(engine::EngineContext& context)
{
    const SectorEditorPlayerSettingsTab activeTab = state_.activeTab;
    StopAudioPreview(context);
    state_ = SectorEditorPlayerSettingsState{};
    state_.activeTab = activeTab;
}

void SectorEditorPlayerSettingsService::Shutdown(engine::EngineContext& context)
{
    StopAudioPreview(context);
    state_ = SectorEditorPlayerSettingsState{};
}

SectorEditorPlayerSettingsSaveResult
SectorEditorPlayerSettingsService::SaveAndClose(engine::EngineContext& context)
{
    SectorEditorPlayerSettingsSaveResult result;
    state_.draft.playerSounds.events.clear();
    state_.draft.playerSounds.events.reserve(state_.soundEvents.size());
    for (SectorEditorPlayerSoundEventDraft& draft : state_.soundEvents) {
        draft.value.id = draft.idBuffer.data();
        state_.draft.playerSounds.events.push_back(draft.value);
    }

    std::string catalogError;
    if (!ValidateCatalogReferences(catalogError)) {
        state_.errorMessage = catalogError;
        return result;
    }

    FpsApplicationSettings candidate = settings_;
    candidate.footsteps = state_.draft.footsteps;
    candidate.playerSounds = state_.draft.playerSounds;
    candidate.playerStamina = state_.draft.playerStamina;
    candidate.playerInventory = state_.draft.playerInventory;
    candidate.playerHealth = state_.draft.playerHealth;
    candidate.playerSneak = state_.draft.playerSneak;
    std::string saveError;
    if (!SaveFpsApplicationSettings(
                settingsPath_.string(), candidate, &saveError)) {
        state_.errorMessage = saveError.empty()
                ? "Could not save player settings"
                : saveError;
        return result;
    }

    result.playerAudioChanged = !SamePlayerSounds(
            settings_.playerSounds,
            candidate.playerSounds);
    result.footstepsChanged = !SameFootsteps(
            settings_.footsteps,
            candidate.footsteps);
    settings_ = std::move(candidate);
    result.saved = true;
    statusText_ = "Player settings saved";
    Cancel(context);
    return result;
}

void SectorEditorPlayerSettingsService::ResetActiveTab()
{
    const FpsApplicationSettings defaults;
    switch (state_.activeTab) {
        case SectorEditorPlayerSettingsTab::Stamina:
            state_.draft.playerStamina = defaults.playerStamina;
            break;
        case SectorEditorPlayerSettingsTab::Inventory:
            state_.draft.playerInventory = defaults.playerInventory;
            break;
        case SectorEditorPlayerSettingsTab::Audio:
            state_.draft.footsteps = defaults.footsteps;
            state_.draft.playerSounds = defaults.playerSounds;
            state_.soundEvents.clear();
            for (const PlayerSoundEventSettings& event :
                    defaults.playerSounds.events) {
                SectorEditorPlayerSoundEventDraft draft;
                draft.value = event;
                CopyEventIdToBuffer(draft);
                state_.soundEvents.push_back(std::move(draft));
            }
            break;
        case SectorEditorPlayerSettingsTab::Health:
            state_.draft.playerHealth = defaults.playerHealth;
            break;
        case SectorEditorPlayerSettingsTab::Sneaking:
            state_.draft.playerSneak = defaults.playerSneak;
            break;
    }
    state_.errorMessage.clear();
}

void SectorEditorPlayerSettingsService::AddSoundEvent()
{
    std::unordered_set<std::string> ids;
    for (const SectorEditorPlayerSoundEventDraft& draft : state_.soundEvents) {
        ids.insert(draft.idBuffer.data());
    }
    std::string id = "event";
    for (int suffix = 2; ids.find(id) != ids.end(); ++suffix) {
        id = "event_" + std::to_string(suffix);
    }
    SectorEditorPlayerSoundEventDraft draft;
    draft.value.id = id;
    if (!state_.playerSoundCatalog.sets.empty()) {
        draft.value.set = state_.playerSoundCatalog.sets.front().id;
    }
    CopyEventIdToBuffer(draft);
    state_.soundEvents.push_back(std::move(draft));
    state_.errorMessage.clear();
}

void SectorEditorPlayerSettingsService::RemoveSoundEvent(size_t index)
{
    if (index >= state_.soundEvents.size()) return;
    state_.soundEvents.erase(state_.soundEvents.begin()
            + static_cast<std::ptrdiff_t>(index));
    state_.errorMessage.clear();
}

void SectorEditorPlayerSettingsService::PreviewFootstepSet(
        engine::EngineContext& context)
{
    const SoundSetCatalogSet* set = FindSoundSetCatalogSet(
            state_.footstepCatalog,
            state_.draft.footsteps.defaultSet);
    if (set == nullptr || set->relativePaths.empty()) {
        state_.audioPreview.message = "Footstep set is unavailable";
        return;
    }
    StopAudioPreview(context);
    state_.audioPreview.scope = context.assets.CreateScope(
            "sector_editor_player_settings_audio_preview");
    const std::string path = ResolveSectorAudioAssetPath(
            set->relativePaths.front());
    state_.audioPreview.sound = context.assets.RequestSound(
            state_.audioPreview.scope, path.c_str());
    state_.audioPreview.pending = !engine::IsNull(
            state_.audioPreview.sound);
    state_.audioPreview.message = state_.audioPreview.pending
            ? "Loading preview..." : "Preview request failed";
}

void SectorEditorPlayerSettingsService::PreviewPlayerSoundSet(
        engine::EngineContext& context,
        std::string_view setId)
{
    const SoundSetCatalogSet* set = FindSoundSetCatalogSet(
            state_.playerSoundCatalog, setId);
    if (set == nullptr || set->relativePaths.empty()) {
        state_.audioPreview.message = "Player sound set is unavailable";
        return;
    }
    StopAudioPreview(context);
    state_.audioPreview.scope = context.assets.CreateScope(
            "sector_editor_player_settings_audio_preview");
    const std::string path = ResolveSectorAudioAssetPath(
            set->relativePaths.front());
    state_.audioPreview.sound = context.assets.RequestSound(
            state_.audioPreview.scope, path.c_str());
    state_.audioPreview.pending = !engine::IsNull(state_.audioPreview.sound);
    state_.audioPreview.message = state_.audioPreview.pending
            ? "Loading preview..." : "Preview request failed";
}

void SectorEditorPlayerSettingsService::UpdateAudioPreview(
        engine::EngineContext& context)
{
    if (!state_.audioPreview.pending) return;
    if (context.assets.HasFailed(state_.audioPreview.sound)) {
        state_.audioPreview.pending = false;
        state_.audioPreview.message = "Preview failed to load";
        return;
    }
    if (!context.assets.IsReady(state_.audioPreview.sound)) return;
    state_.audioPreview.playback = context.audio.PlaySound(
            context.assets, state_.audioPreview.sound);
    state_.audioPreview.pending = false;
    state_.audioPreview.message = engine::IsNull(
            state_.audioPreview.playback)
            ? "Preview could not start" : "Previewing sound";
}

void SectorEditorPlayerSettingsService::StopAudioPreview(
        engine::EngineContext& context)
{
    if (!engine::IsNull(state_.audioPreview.playback)) {
        context.audio.StopSound(
                context.assets, state_.audioPreview.playback);
    }
    if (!engine::IsNull(state_.audioPreview.scope)) {
        context.assets.UnloadScope(state_.audioPreview.scope);
    }
    state_.audioPreview = SectorEditorPlayerAudioPreviewState{};
}

void SectorEditorPlayerSettingsService::BuildCatalogLabels()
{
    state_.footstepLabelStorage.reserve(state_.footstepCatalog.sets.size());
    for (const SoundSetCatalogSet& set : state_.footstepCatalog.sets) {
        state_.footstepLabelStorage.push_back(set.id);
    }
    state_.footstepLabels.reserve(state_.footstepLabelStorage.size());
    for (const std::string& label : state_.footstepLabelStorage) {
        state_.footstepLabels.push_back(label.c_str());
    }
    state_.playerSoundLabelStorage.reserve(
            state_.playerSoundCatalog.sets.size());
    for (const SoundSetCatalogSet& set : state_.playerSoundCatalog.sets) {
        state_.playerSoundLabelStorage.push_back(set.id);
    }
    state_.playerSoundLabels.reserve(state_.playerSoundLabelStorage.size());
    for (const std::string& label : state_.playerSoundLabelStorage) {
        state_.playerSoundLabels.push_back(label.c_str());
    }
}

bool SectorEditorPlayerSettingsService::ValidateCatalogReferences(
        std::string& error) const
{
    if (FindSoundSetCatalogSet(
                state_.footstepCatalog,
                state_.draft.footsteps.defaultSet) == nullptr) {
        error = "Default footstep set is unavailable";
        return false;
    }
    std::unordered_set<std::string> ids;
    for (const SectorEditorPlayerSoundEventDraft& draft : state_.soundEvents) {
        const std::string id = draft.idBuffer.data();
        if (!IsValidSoundSetId(id)) {
            error = "Player sound event IDs must be non-empty safe IDs";
            return false;
        }
        if (!ids.insert(id).second) {
            error = "Player sound event IDs must be unique";
            return false;
        }
        if (FindSoundSetCatalogSet(
                    state_.playerSoundCatalog,
                    draft.value.set) == nullptr) {
            error = "Player sound event '" + id
                    + "' references an unavailable set";
            return false;
        }
    }
    return true;
}

} // namespace game
