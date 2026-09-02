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
    Health,
    Sneaking,
    Lighting,
    Liquids
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
    engine::UIScrollState sneakingScroll;
    engine::UIScrollState lightingScroll;
    engine::UIScrollState liquidsScroll;
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

    engine::UIFloatInputState oxygenMaximumInput;
    engine::UIFloatInputState oxygenDepletionInput;
    engine::UIFloatInputState oxygenRegenerationInput;
    engine::UIIntInputState drowningDamageInput;
    engine::UIFloatInputState drowningIntervalInput;

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
    engine::UIFloatInputState heartbeatStartThresholdInput;
    engine::UIFloatInputState heartbeatFullEffectInput;
    engine::UIFloatInputState heartbeatMaximumVolumeInput;
    engine::UIFloatInputState heartbeatStartPitchInput;
    engine::UIFloatInputState heartbeatMaximumPitchInput;
    engine::UIFloatInputState heartbeatResponseInput;
    engine::UIFloatInputState healthMovementThresholdInput;
    engine::UIFloatInputState healthMovementMinimumSpeedInput;
    engine::UIFloatInputState healthMovementMinimumSprintSpeedInput;
    engine::UIFloatInputState healthCameraThresholdInput;
    engine::UIFloatInputState healthCameraFullEffectInput;
    engine::UIFloatInputState healthCameraLateralInput;
    engine::UIFloatInputState healthCameraVerticalInput;
    engine::UIFloatInputState healthCameraPitchInput;
    engine::UIFloatInputState healthCameraYawInput;
    engine::UIFloatInputState healthCameraRollInput;
    engine::UIFloatInputState healthCameraFrequencyInput;
    engine::UIFloatInputState healthCameraResponseInput;

    engine::UIFloatInputState sneakFullVisibilityLightInput;
    engine::UIFloatInputState sneakDarknessCutoffInput;
    engine::UIFloatInputState sneakLightHalfResponseInput;
    engine::UIFloatInputState sneakDetectionBuildInput;
    engine::UIFloatInputState sneakDetectionDecayInput;
    engine::UIFloatInputState sneakProximityRangeInput;
    engine::UIFloatInputState sneakCrouchVisualInput;
    engine::UIFloatInputState sneakCrouchNoiseInput;

    engine::UIFloatInputState flashlightIntensityInput;
    engine::UIFloatInputState flashlightReachInput;
    engine::UIFloatInputState flashlightConeRadiusInput;
    engine::UIIntInputState flashlightTintRedInput;
    engine::UIIntInputState flashlightTintGreenInput;
    engine::UIIntInputState flashlightTintBlueInput;
    engine::UIFloatInputState flashlightHotspotInput;
    engine::UIFloatInputState flashlightSpillInput;
    engine::UIFloatInputState flashlightEdgeInput;
    engine::UIFloatInputState flashlightShadowStrengthInput;
    engine::UIFloatInputState flashlightShadowSoftnessInput;
    engine::UIFloatInputState flashlightShadowContactOffsetInput;
    engine::UIFloatInputState flashlightHeightInput;
    engine::UIFloatInputState flashlightLateralOffsetInput;
    engine::UIFloatInputState flashlightAimConvergenceInput;
    engine::UIFloatInputState flashlightAimResponseInput;
};

} // namespace game
