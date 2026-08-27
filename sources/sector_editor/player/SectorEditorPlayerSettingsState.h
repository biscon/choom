#pragma once

#include "engine/ui/UI.h"
#include "game/FpsWeaponRegistry.h"
#include "game/SoundSetAudio.h"

#include <array>
#include <string>
#include <vector>

namespace game {

enum class SectorEditorPlayerSettingsTab {
    Stamina,
    Inventory,
    Audio,
    Health
};

struct SectorEditorPlayerSoundEventDraft {
    PlayerSoundEventSettings value;
    std::array<char, 64> idBuffer{};
    engine::UIFloatInputState volumeInput;
};

struct SectorEditorPlayerAudioPreviewState {
    engine::AssetScopeHandle scope = engine::NullAssetScopeHandle();
    engine::SoundHandle sound = engine::NullSoundHandle();
    engine::SoundPlaybackHandle playback = engine::NullSoundPlaybackHandle();
    bool pending = false;
    std::string message;
};

struct SectorEditorPlayerSettingsState {
    bool open = false;
    SectorEditorPlayerSettingsTab activeTab =
            SectorEditorPlayerSettingsTab::Stamina;
    FpsApplicationSettings draft;
    std::vector<SectorEditorPlayerSoundEventDraft> soundEvents;
    SoundSetCatalog footstepCatalog;
    SoundSetCatalog playerSoundCatalog;
    std::vector<std::string> footstepLabelStorage;
    std::vector<const char*> footstepLabels;
    std::vector<std::string> playerSoundLabelStorage;
    std::vector<const char*> playerSoundLabels;
    SectorEditorPlayerAudioPreviewState audioPreview;
    engine::UIScrollState staminaScroll;
    engine::UIScrollState inventoryScroll;
    engine::UIScrollState audioScroll;
    engine::UIScrollState healthScroll;
    std::string errorMessage;

    engine::UIFloatInputState staminaMaximumInput;
    engine::UIFloatInputState staminaSprintDrainInput;
    engine::UIFloatInputState staminaJumpCostInput;
    engine::UIFloatInputState staminaRegenerationInput;
    engine::UIFloatInputState staminaRecoveryRatioInput;
    engine::UIFloatInputState windedThresholdInput;
    engine::UIFloatInputState windedVerticalAmplitudeInput;
    engine::UIFloatInputState windedPitchAmplitudeInput;
    engine::UIFloatInputState windedFrequencyInput;
    engine::UIFloatInputState windedResponseInput;
    engine::UIFloatInputState breathingThresholdInput;
    engine::UIFloatInputState breathingVolumeInput;
    engine::UIFloatInputState breathingFadeOutInput;

    engine::UIFloatInputState inventoryWeightInput;
    engine::UIIntInputState inventorySlotsInput;
    engine::UIFloatInputState inventoryVacuumDurationInput;
    engine::UIFloatInputState inventoryVacuumHeightInput;

    engine::UIFloatInputState footstepVolumeInput;
    engine::UIFloatInputState footstepLandingMultiplierInput;
    engine::UIFloatInputState footstepNoiseRadiusInput;
    engine::UIFloatInputState footstepLandingNoiseRadiusInput;

    engine::UIFloatInputState healthThresholdInput;
    engine::UIIntInputState healthVignetteRedInput;
    engine::UIIntInputState healthVignetteGreenInput;
    engine::UIIntInputState healthVignetteBlueInput;
    engine::UIFloatInputState healthInnerRadiusInput;
    engine::UIFloatInputState healthOuterRadiusInput;
    engine::UIFloatInputState healthOpacityInput;
    engine::UIFloatInputState healthDesaturationInput;
};

} // namespace game
