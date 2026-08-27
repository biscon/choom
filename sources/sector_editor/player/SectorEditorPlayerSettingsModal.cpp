#include "sector_editor/player/SectorEditorPlayerSettingsModal.h"

#include "engine/EngineContext.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace game {
namespace {

constexpr float RowHeight = 40.0f;
constexpr float RowGap = 10.0f;
constexpr float LabelWidth = 300.0f;
constexpr float InputWidth = 210.0f;

int CatalogIndex(const SoundSetCatalog& catalog, std::string_view id)
{
    for (size_t i = 0; i < catalog.sets.size(); ++i) {
        if (catalog.sets[i].id == id) return static_cast<int>(i);
    }
    return catalog.sets.empty() ? -1 : 0;
}

} // namespace

SectorEditorPlayerSettingsSaveResult DrawSectorEditorPlayerSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        engine::EngineContext& engineContext,
        SectorEditorPlayerSettingsService& service)
{
    SectorEditorPlayerSettingsState& state = service.State();
    if (!state.open) return {};
    service.UpdateAudioPreview(engineContext);

    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested](engine::InputEvent& event) {
                if (event.key.key != KEY_ESCAPE) return;
                cancelRequested = true;
                engine::ConsumeEvent(event);
            });
    if (cancelRequested) {
        service.Cancel(engineContext);
        return {};
    }

    DrawRectangle(
            0, 0,
            static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight),
            Color{0, 0, 0, 150});
    const Rectangle modal{
            (EditorWidth - 1120.0f) * 0.5f,
            (EditorHeight - 940.0f) * 0.5f,
            1120.0f,
            940.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 252});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(
            config, assets,
            Rectangle{modal.x + 28.0f, modal.y + 18.0f,
                    modal.width - 56.0f, 42.0f},
            font, "Player Settings");

    const char* tabNames[] = {
            "Stamina", "Inventory", "Audio", "Health", "Sneaking"};
    const float tabY = modal.y + 70.0f;
    const float tabWidth = (modal.width - 56.0f - 32.0f) / 5.0f;
    for (int i = 0; i < 5; ++i) {
        const bool active = static_cast<int>(state.activeTab) == i;
        if (engine::ToolButton(
                    ui, config, input, assets,
                    TextFormat("sector_editor_player_settings_tab_%d", i),
                    Rectangle{modal.x + 28.0f
                                    + static_cast<float>(i)
                                            * (tabWidth + 8.0f),
                            tabY, tabWidth, 42.0f},
                    smallFont,
                    tabNames[i], active)) {
            state.activeTab = static_cast<SectorEditorPlayerSettingsTab>(i);
            state.errorMessage.clear();
        }
    }

    const Rectangle scrollBounds{
            modal.x + 28.0f,
            modal.y + 126.0f,
            modal.width - 56.0f,
            692.0f};
    engine::UIScrollState* scrollState = &state.staminaScroll;
    float contentHeight = 900.0f;
    switch (state.activeTab) {
        case SectorEditorPlayerSettingsTab::Stamina:
            scrollState = &state.staminaScroll;
            contentHeight = 980.0f;
            break;
        case SectorEditorPlayerSettingsTab::Inventory:
            scrollState = &state.inventoryScroll;
            contentHeight = 260.0f;
            break;
        case SectorEditorPlayerSettingsTab::Audio:
            scrollState = &state.audioScroll;
            contentHeight = 430.0f
                    + static_cast<float>(state.soundEvents.size()) * 58.0f;
            break;
        case SectorEditorPlayerSettingsTab::Health:
            scrollState = &state.healthScroll;
            contentHeight = 1740.0f;
            break;
        case SectorEditorPlayerSettingsTab::Sneaking:
            scrollState = &state.sneakingScroll;
            contentHeight = 520.0f;
            break;
    }
    const float contentWidth = std::max(
            0.0f,
            scrollBounds.width - config.borderThickness * 2.0f
                    - config.scrollbarSize
                    - engine::DefaultScrollAreaPaddingPx * 2.0f);
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui, config, input,
            "sector_editor_player_settings_scroll",
            scrollBounds,
            Vector2{contentWidth, contentHeight},
            *scrollState);
    float y = 0.0f;
    const float fieldX = LabelWidth + 20.0f;

    const auto section = [&](const char* label) {
        engine::Text(
                ui, config, assets,
                Rectangle{0.0f, y, scroll.viewport.width, RowHeight},
                font, label, engine::UITextJustify::Left,
                config.accentColor);
        y += RowHeight + RowGap;
    };
    const auto drawFloat = [&](const char* id, const char* label,
                               float& value,
                               engine::UIFloatInputState& inputState,
                               float minimum, float maximum,
                               int decimals) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui, config, input, assets, smallFont, id, label,
                Rectangle{0.0f, y, LabelWidth, RowHeight},
                Rectangle{fieldX, y, InputWidth, RowHeight},
                engine::UITextJustify::Left,
                value, inputState, minimum, maximum, decimals);
        value = result.value;
        if (result.changed) state.errorMessage.clear();
        y += RowHeight + RowGap;
    };
    const auto drawInt = [&](const char* id, const char* label,
                             int& value,
                             engine::UIIntInputState& inputState,
                             int minimum, int maximum) {
        const SectorEditorIntInputResult result = DrawLabeledIntInput(
                ui, config, input, assets, smallFont, id, label,
                Rectangle{0.0f, y, LabelWidth, RowHeight},
                Rectangle{fieldX, y, InputWidth, RowHeight},
                engine::UITextJustify::Left,
                value, inputState, minimum, maximum, 1);
        value = result.value;
        if (result.changed) state.errorMessage.clear();
        y += RowHeight + RowGap;
    };
    const auto drawCheckbox = [&](const char* id, const char* label,
                                  bool& value) {
        engine::Checkbox(
                ui, config, input, assets, id,
                Rectangle{0.0f, y, 520.0f, RowHeight},
                smallFont, label, value);
        y += RowHeight + RowGap;
    };

    if (state.activeTab == SectorEditorPlayerSettingsTab::Stamina) {
        PlayerStaminaApplicationSettings& stamina = state.draft.playerStamina;
        section("Stamina");
        drawFloat("player_stamina_maximum", "Maximum", stamina.maximum,
                state.staminaMaximumInput, 0.001f, 100000.0f, 2);
        drawFloat("player_stamina_sprint_drain", "Sprint drain / second",
                stamina.sprintDrainPerSecond,
                state.staminaSprintDrainInput, 0.0f, 100000.0f, 2);
        drawFloat("player_stamina_jump_cost", "Jump cost", stamina.jumpCost,
                state.staminaJumpCostInput, 0.0f, 100000.0f, 2);
        drawFloat("player_stamina_regeneration", "Regeneration / second",
                stamina.regenerationPerSecond,
                state.staminaRegenerationInput, 0.0f, 100000.0f, 2);
        drawFloat("player_stamina_recovery_ratio", "Exhausted recovery ratio",
                stamina.exhaustedRecoveryRatio,
                state.staminaRecoveryRatioInput, 0.0f, 1.0f, 3);
        section("Winded camera");
        drawCheckbox("player_winded_enabled", "Enabled",
                stamina.windedCamera.enabled);
        drawFloat("player_winded_threshold", "Start threshold ratio",
                stamina.windedCamera.startThresholdRatio,
                state.windedThresholdInput, 0.0f, 1.0f, 3);
        drawFloat("player_winded_vertical", "Vertical amplitude (world)",
                stamina.windedCamera.verticalAmplitudeWorld,
                state.windedVerticalAmplitudeInput, 0.0f, 100.0f, 3);
        drawFloat("player_winded_pitch", "Pitch amplitude (degrees)",
                stamina.windedCamera.pitchAmplitudeDegrees,
                state.windedPitchAmplitudeInput, 0.0f, 180.0f, 2);
        drawFloat("player_winded_frequency", "Frequency (Hz)",
                stamina.windedCamera.frequencyHz,
                state.windedFrequencyInput, 0.0f, 100.0f, 2);
        drawFloat("player_winded_response", "Response (seconds)",
                stamina.windedCamera.responseSeconds,
                state.windedResponseInput, 0.001f, 100.0f, 3);
        section("Breathing audio");
        drawFloat("player_breathing_threshold", "Threshold ratio",
                stamina.breathingAudio.thresholdRatio,
                state.breathingThresholdInput, 0.0f, 1.0f, 3);
        drawFloat("player_breathing_volume", "Volume",
                stamina.breathingAudio.volume,
                state.breathingVolumeInput, 0.0f, 1.0f, 3);
        drawFloat("player_breathing_fade", "Fade-out (seconds)",
                stamina.breathingAudio.fadeOutSeconds,
                state.breathingFadeOutInput, 0.001f, 100.0f, 3);
    } else if (state.activeTab
            == SectorEditorPlayerSettingsTab::Inventory) {
        PlayerInventoryApplicationSettings& inventory =
                state.draft.playerInventory;
        section("Inventory");
        drawFloat("player_inventory_weight", "Maximum carry weight (kg)",
                inventory.maxCarryWeightKg,
                state.inventoryWeightInput, 0.001f, 100000.0f, 2);
        drawInt("player_inventory_slots", "Maximum slots",
                inventory.maxSlots,
                state.inventorySlotsInput, 1, 1024);
        drawFloat("player_inventory_vacuum_duration",
                "Pickup vacuum duration (seconds)",
                inventory.pickupVacuumDurationSeconds,
                state.inventoryVacuumDurationInput, 0.001f, 100.0f, 3);
        drawFloat("player_inventory_vacuum_height",
                "Pickup target height (world)",
                inventory.pickupVacuumTargetHeightWorld,
                state.inventoryVacuumHeightInput, 0.0f, 1000.0f, 3);
    } else if (state.activeTab == SectorEditorPlayerSettingsTab::Audio) {
        FootstepApplicationSettings& footsteps = state.draft.footsteps;
        section("Footsteps and movement noise");
        engine::Text(
                ui, config, assets,
                Rectangle{0.0f, y, LabelWidth, RowHeight},
                smallFont, "Default footstep set");
        int footstepIndex = CatalogIndex(
                state.footstepCatalog, footsteps.defaultSet);
        if (!state.footstepLabels.empty()
                && engine::Option(
                        ui, config, input, assets,
                        "player_footstep_set",
                        Rectangle{fieldX, y, 330.0f, RowHeight},
                        smallFont,
                        state.footstepLabels.data(),
                        state.footstepLabels.size(),
                        footstepIndex)
                && footstepIndex >= 0) {
            footsteps.defaultSet = state.footstepCatalog.sets[
                    static_cast<size_t>(footstepIndex)].id;
            state.errorMessage.clear();
        }
        if (engine::Button(
                    ui, config, input, assets,
                    "player_footstep_preview",
                    Rectangle{fieldX + 344.0f, y, 120.0f, RowHeight},
                    smallFont, "Preview")) {
            service.PreviewFootstepSet(engineContext);
        }
        y += RowHeight + RowGap;
        drawFloat("player_footstep_volume", "Volume", footsteps.volume,
                state.footstepVolumeInput, 0.0f, 1.0f, 3);
        drawFloat("player_footstep_landing_multiplier",
                "Landing volume multiplier",
                footsteps.landingImpactVolumeMultiplier,
                state.footstepLandingMultiplierInput,
                0.0f, 100.0f, 3);
        drawFloat("player_footstep_noise", "Noise radius (world)",
                footsteps.noiseRadiusWorld,
                state.footstepNoiseRadiusInput, 0.0f, 10000.0f, 2);
        drawFloat("player_footstep_landing_noise",
                "Landing noise radius (world)",
                footsteps.landingNoiseRadiusWorld,
                state.footstepLandingNoiseRadiusInput,
                0.0f, 10000.0f, 2);
        section("Player sound events");
        size_t removeIndex = std::numeric_limits<size_t>::max();
        for (size_t index = 0; index < state.soundEvents.size(); ++index) {
            SectorEditorPlayerSoundEventDraft& event =
                    state.soundEvents[index];
            const float idX = 0.0f;
            const float setX = 190.0f;
            const float volumeX = 550.0f;
            engine::TextInput(
                    ui, config, input, assets,
                    TextFormat("player_sound_event_id_%d", static_cast<int>(index)),
                    Rectangle{idX, y, 176.0f, RowHeight},
                    smallFont,
                    event.idBuffer.data(),
                    event.idBuffer.size(),
                    1,
                    event.idBuffer.size() - 1);
            int soundIndex = CatalogIndex(
                    state.playerSoundCatalog, event.value.set);
            if (!state.playerSoundLabels.empty()
                    && engine::Option(
                            ui, config, input, assets,
                            TextFormat("player_sound_event_set_%d",
                                    static_cast<int>(index)),
                            Rectangle{setX, y, 340.0f, RowHeight},
                            smallFont,
                            state.playerSoundLabels.data(),
                            state.playerSoundLabels.size(),
                            soundIndex)
                    && soundIndex >= 0) {
                event.value.set = state.playerSoundCatalog.sets[
                        static_cast<size_t>(soundIndex)].id;
                state.errorMessage.clear();
            }
            engine::Slider(
                    ui, config, input,
                    TextFormat("player_sound_event_volume_%d",
                            static_cast<int>(index)),
                    Rectangle{volumeX, y, 150.0f, RowHeight},
                    0.0f, 1.0f, event.value.volume);
            char volumeText[16];
            std::snprintf(volumeText, sizeof(volumeText), "%.2f",
                    event.value.volume);
            engine::Text(
                    ui, config, assets,
                    Rectangle{volumeX + 154.0f, y, 52.0f, RowHeight},
                    smallFont, volumeText);
            if (engine::Button(
                        ui, config, input, assets,
                        TextFormat("player_sound_event_preview_%d",
                                static_cast<int>(index)),
                        Rectangle{volumeX + 216.0f, y, 92.0f, RowHeight},
                        smallFont, "Preview")) {
                service.PreviewPlayerSoundSet(
                        engineContext, event.value.set);
            }
            if (engine::Button(
                        ui, config, input, assets,
                        TextFormat("player_sound_event_remove_%d",
                                static_cast<int>(index)),
                        Rectangle{volumeX + 318.0f, y, 92.0f, RowHeight},
                        smallFont, "Remove")) {
                removeIndex = index;
            }
            y += RowHeight + 8.0f;
        }
        if (removeIndex != std::numeric_limits<size_t>::max()) {
            service.RemoveSoundEvent(removeIndex);
        }
        if (engine::Button(
                    ui, config, input, assets,
                    "player_sound_event_add",
                    Rectangle{0.0f, y, 170.0f, RowHeight},
                    smallFont, "Add Event")) {
            service.AddSoundEvent();
        }
        y += RowHeight + RowGap;
        if (!state.audioPreview.message.empty()) {
            engine::Text(
                    ui, config, assets,
                    Rectangle{0.0f, y, scroll.viewport.width, RowHeight},
                    smallFont, state.audioPreview.message.c_str(),
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
        }
    } else if (state.activeTab == SectorEditorPlayerSettingsTab::Sneaking) {
        PlayerSneakApplicationSettings& sneak = state.draft.playerSneak;
        section("Light-aware visual detection");
        drawFloat("player_sneak_full_visibility_light",
                "Full visibility light level",
                sneak.fullVisibilityLightLevel,
                state.sneakFullVisibilityLightInput,
                0.001f, 1000.0f, 3);
        drawFloat("player_sneak_darkness_cutoff",
                "Darkness cutoff (normalized)",
                sneak.darknessCutoffNormalized,
                state.sneakDarknessCutoffInput,
                0.0f, 0.999f, 3);
        drawFloat("player_sneak_light_half_response",
                "Light half-response above cutoff",
                sneak.lightHalfResponseRangeNormalized,
                state.sneakLightHalfResponseInput,
                0.0001f,
                std::max(
                        0.0001f,
                        1.0f - sneak.darknessCutoffNormalized - 0.0001f),
                4);
        drawFloat("player_sneak_detection_build",
                "Full-light detection time (seconds)",
                sneak.visualDetectionBuildSeconds,
                state.sneakDetectionBuildInput,
                0.001f, 600.0f, 3);
        drawFloat("player_sneak_detection_decay",
                "Detection decay time (seconds)",
                sneak.visualDetectionDecaySeconds,
                state.sneakDetectionDecayInput,
                0.001f, 600.0f, 3);
        drawFloat("player_sneak_proximity_range",
                "Darkness proximity outer range (world)",
                sneak.darknessProximityRangeWorld,
                state.sneakProximityRangeInput,
                0.0f, 1000.0f, 3);
        section("Crouching modifiers");
        drawFloat("player_sneak_crouch_visual",
                "Visual detection multiplier",
                sneak.crouchVisualDetectionMultiplier,
                state.sneakCrouchVisualInput,
                0.0f, 1.0f, 3);
        drawFloat("player_sneak_crouch_noise",
                "Movement noise multiplier",
                sneak.crouchMovementNoiseMultiplier,
                state.sneakCrouchNoiseInput,
                0.0f, 1.0f, 3);
    } else {
        PlayerLowHealthVisualApplicationSettings& health =
                state.draft.playerHealth.lowHealthVisual;
        section("Low-health screen effect (game runtime only)");
        drawCheckbox("player_health_visual_enabled", "Enabled",
                health.enabled);
        drawFloat("player_health_threshold", "Start threshold ratio",
                health.thresholdRatio,
                state.healthThresholdInput, 0.001f, 1.0f, 3);
        int red = health.vignetteColor.r;
        int green = health.vignetteColor.g;
        int blue = health.vignetteColor.b;
        drawInt("player_health_vignette_red", "Vignette red", red,
                state.healthVignetteRedInput, 0, 255);
        drawInt("player_health_vignette_green", "Vignette green", green,
                state.healthVignetteGreenInput, 0, 255);
        drawInt("player_health_vignette_blue", "Vignette blue", blue,
                state.healthVignetteBlueInput, 0, 255);
        health.vignetteColor = Color{
                static_cast<unsigned char>(red),
                static_cast<unsigned char>(green),
                static_cast<unsigned char>(blue),
                255};
        DrawColorSwatch(
                config,
                Rectangle{fieldX + InputWidth + 18.0f,
                        y - 3.0f * (RowHeight + RowGap),
                        96.0f, 96.0f},
                health.vignetteColor,
                1.0f);
        drawFloat("player_health_inner_radius", "Vignette inner radius",
                health.vignetteInnerRadius,
                state.healthInnerRadiusInput, 0.0f, 2.0f, 3);
        drawFloat("player_health_outer_radius", "Vignette outer radius",
                health.vignetteOuterRadius,
                state.healthOuterRadiusInput, 0.0f, 2.0f, 3);
        drawFloat("player_health_opacity", "Maximum vignette opacity",
                health.maximumVignetteOpacity,
                state.healthOpacityInput, 0.0f, 1.0f, 3);
        drawFloat("player_health_desaturation", "Maximum desaturation",
                health.maximumDesaturation,
                state.healthDesaturationInput, 0.0f, 1.0f, 3);

        PlayerHeartbeatAudioApplicationSettings& heartbeat =
                state.draft.playerHealth.heartbeatAudio;
        section("Low-health heartbeat (game runtime only)");
        drawCheckbox("player_heartbeat_enabled", "Enabled",
                heartbeat.enabled);
        drawFloat("player_heartbeat_threshold", "Start threshold ratio",
                heartbeat.startThresholdRatio,
                state.heartbeatStartThresholdInput, 0.001f, 1.0f, 3);
        drawFloat("player_heartbeat_full", "Full effect ratio",
                heartbeat.fullEffectRatio,
                state.heartbeatFullEffectInput, 0.0f, 0.999f, 3);
        drawFloat("player_heartbeat_volume", "Maximum volume",
                heartbeat.maximumVolume,
                state.heartbeatMaximumVolumeInput, 0.0f, 1.0f, 3);
        drawFloat("player_heartbeat_start_pitch", "Start pitch",
                heartbeat.startPitch,
                state.heartbeatStartPitchInput, 0.01f, 4.0f, 3);
        drawFloat("player_heartbeat_max_pitch", "Maximum pitch",
                heartbeat.maximumPitch,
                state.heartbeatMaximumPitchInput, 0.01f, 4.0f, 3);
        drawFloat("player_heartbeat_response", "Response (seconds)",
                heartbeat.responseSeconds,
                state.heartbeatResponseInput, 0.001f, 100.0f, 3);

        PlayerLowHealthMovementApplicationSettings& movement =
                state.draft.playerHealth.lowHealthMovement;
        section("Low-health movement speed (game runtime only)");
        drawCheckbox("player_health_movement_enabled", "Enabled",
                movement.enabled);
        drawFloat("player_health_movement_threshold",
                "Start threshold ratio",
                movement.startThresholdRatio,
                state.healthMovementThresholdInput, 0.001f, 1.0f, 3);
        drawFloat("player_health_movement_minimum",
                "Walk scale at zero health",
                movement.minimumSpeedScale,
                state.healthMovementMinimumSpeedInput, 0.0f, 1.0f, 3);
        drawFloat("player_health_movement_minimum_sprint",
                "Sprint scale at zero health",
                movement.minimumSprintSpeedScale,
                state.healthMovementMinimumSprintSpeedInput,
                0.0f, 1.0f, 3);

        PlayerLowHealthCameraApplicationSettings& camera =
                state.draft.playerHealth.lowHealthCamera;
        section("Low-health camera sway (game runtime only)");
        drawCheckbox("player_health_camera_enabled", "Enabled",
                camera.enabled);
        drawFloat("player_health_camera_threshold", "Start threshold ratio",
                camera.startThresholdRatio,
                state.healthCameraThresholdInput, 0.001f, 1.0f, 3);
        drawFloat("player_health_camera_full", "Full effect ratio",
                camera.fullEffectRatio,
                state.healthCameraFullEffectInput, 0.0f, 0.999f, 3);
        drawFloat("player_health_camera_lateral",
                "Lateral amplitude (world)",
                camera.lateralAmplitudeWorld,
                state.healthCameraLateralInput, 0.0f, 1.0f, 3);
        drawFloat("player_health_camera_vertical",
                "Vertical amplitude (world)",
                camera.verticalAmplitudeWorld,
                state.healthCameraVerticalInput, 0.0f, 1.0f, 3);
        drawFloat("player_health_camera_pitch",
                "Pitch amplitude (degrees)",
                camera.pitchAmplitudeDegrees,
                state.healthCameraPitchInput, 0.0f, 45.0f, 2);
        drawFloat("player_health_camera_yaw",
                "Yaw amplitude (degrees)",
                camera.yawAmplitudeDegrees,
                state.healthCameraYawInput, 0.0f, 45.0f, 2);
        drawFloat("player_health_camera_roll",
                "Roll amplitude (degrees)",
                camera.rollAmplitudeDegrees,
                state.healthCameraRollInput, 0.0f, 45.0f, 2);
        drawFloat("player_health_camera_frequency", "Frequency (Hz)",
                camera.frequencyHz,
                state.healthCameraFrequencyInput, 0.0f, 20.0f, 2);
        drawFloat("player_health_camera_response", "Response (seconds)",
                camera.responseSeconds,
                state.healthCameraResponseInput, 0.001f, 100.0f, 3);
        engine::Text(
                ui, config, assets,
                Rectangle{0.0f, y, scroll.viewport.width, 54.0f},
                smallFont,
                "Low-health effects are never applied in editor 3D preview.",
                engine::UITextJustify::Left,
                config.mutedTextColor,
                true);
    }
    engine::EndScrollArea(ui, config, input, scroll, *scrollState);

    if (!state.errorMessage.empty()) {
        engine::Text(
                config, assets,
                Rectangle{modal.x + 28.0f, modal.y + 826.0f,
                        modal.width - 56.0f, 38.0f},
                smallFont, state.errorMessage.c_str(),
                engine::UITextJustify::Left,
                Color{255, 120, 120, 255},
                true);
    }
    const float buttonY = modal.y + modal.height - 62.0f;
    if (engine::Button(
                ui, config, input, assets,
                "player_settings_reset_tab",
                Rectangle{modal.x + 28.0f, buttonY, 180.0f, 42.0f},
                smallFont, "Reset Tab")) {
        service.ResetActiveTab();
    }
    if (engine::Button(
                ui, config, input, assets,
                "player_settings_cancel",
                Rectangle{modal.x + modal.width - 360.0f,
                        buttonY, 150.0f, 42.0f},
                smallFont, "Cancel")) {
        service.Cancel(engineContext);
        return {};
    }
    if (engine::Button(
                ui, config, input, assets,
                "player_settings_apply",
                Rectangle{modal.x + modal.width - 190.0f,
                        buttonY, 162.0f, 42.0f},
                smallFont, "Apply")) {
        return service.SaveAndClose(engineContext);
    }
    return {};
}

} // namespace game
