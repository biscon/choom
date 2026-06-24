#include "sector_editor/SectorEditorPreviewSettingsModal.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"

#include <algorithm>

namespace game {

void DrawPreviewSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorPreviewSettingsModalState& modalState,
        bool texturePickerOpen,
        const SectorEditorPreviewSettingsModalCallbacks& callbacks)
{
    if (!modalState.open) {
        return;
    }
    if (texturePickerOpen) {
        return;
    }

    bool okayRequested = false;
    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    cancelRequested = true;
                    engine::ConsumeEvent(event);
                }
            }
    );

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 145});
    const Rectangle modal{
            (EditorWidth - 620.0f) * 0.5f,
            (EditorHeight - 700.0f) * 0.5f,
            620.0f,
            700.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 22.0f;
    engine::Text(config, assets, Rectangle{modal.x + 26.0f, y, modal.width - 52.0f, 42.0f}, font, "Preview Settings");
    y += 54.0f;

    const float tabW = 132.0f;
    const float tabH = 38.0f;
    if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                "sector_editor_preview_settings_tab_general",
                Rectangle{modal.x + 30.0f, y, tabW, tabH},
                font,
                "General",
                modalState.activeTab == PreviewSettingsTab::General)) {
        modalState.activeTab = PreviewSettingsTab::General;
    }
    if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                "sector_editor_preview_settings_tab_sky",
                Rectangle{modal.x + 30.0f + tabW + 8.0f, y, tabW, tabH},
                font,
                "Sky",
                modalState.activeTab == PreviewSettingsTab::Sky)) {
        modalState.activeTab = PreviewSettingsTab::Sky;
    }
    if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                "sector_editor_preview_settings_tab_lighting",
                Rectangle{modal.x + 30.0f + (tabW + 8.0f) * 2.0f, y, tabW, tabH},
                font,
                "Lighting",
                modalState.activeTab == PreviewSettingsTab::Lighting)) {
        modalState.activeTab = PreviewSettingsTab::Lighting;
    }
    y += tabH + 16.0f;

    const float buttonY = modal.y + modal.height - 66.0f;
    const Rectangle scrollBounds{
            modal.x + 30.0f,
            y,
            modal.width - 60.0f,
            std::max(80.0f, buttonY - y - 18.0f)
    };
    const float scrollContentW = std::max(0.0f, scrollBounds.width - config.scrollbarSize);
    const float rowH = 40.0f;
    const float gap = 12.0f;
    const float labelW = 220.0f;
    const float inputW = 180.0f;
    const float inputX = labelW + 18.0f;
    auto drawFloat = [&](float& localY, const char* id, const char* label, float& value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, localY, labelW, rowH},
                Rectangle{inputX, localY, inputW, rowH},
                engine::UITextJustify::Left,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        value = result.value;
        if (result.changed) {
            modalState.errorMessage.clear();
        }
        localY += rowH + gap;
    };

    auto drawGeneralTab = [&]() {
        float contentY = 0.0f;
        const float contentH = 11.0f * (rowH + gap) + 12.0f;
        engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
                ui,
                config,
                input,
                "sector_editor_preview_settings_general_scroll",
                scrollBounds,
                Vector2{scrollContentW, contentH},
                modalState.generalScroll);
        const float contentW = scroll.viewport.width;
        (void)contentW;
        drawFloat(contentY, "sector_editor_preview_walk_speed", "Walk speed", modalState.draftConfig.walkSpeed, modalState.walkSpeedInput, 0.1f, 100.0f, 2);
        drawFloat(contentY, "sector_editor_preview_run_speed", "Run speed", modalState.draftConfig.runSpeed, modalState.runSpeedInput, 0.1f, 200.0f, 2);
        drawFloat(contentY, "sector_editor_preview_mouse_sensitivity", "Mouse sensitivity", modalState.draftConfig.mouseSensitivity, modalState.mouseSensitivityInput, 0.01f, 20.0f, 3);
        drawFloat(contentY, "sector_editor_preview_eye_height", "Camera eye height", modalState.draftConfig.eyeHeight, modalState.eyeHeightInput, 0.1f, 20.0f, 2);
        drawFloat(contentY, "sector_editor_preview_gravity", "Gravity", modalState.draftConfig.gravity, modalState.gravityInput, 0.0f, 200.0f, 2);
        drawFloat(contentY, "sector_editor_preview_player_radius", "Player radius", modalState.draftConfig.playerRadius, modalState.playerRadiusInput, 0.05f, 2.0f, 2);
        drawFloat(contentY, "sector_editor_preview_player_height", "Player height", modalState.draftConfig.playerHeight, modalState.playerHeightInput, 0.5f, 3.0f, 2);
        drawFloat(contentY, "sector_editor_preview_step_height", "Step height", modalState.draftConfig.stepHeight, modalState.stepHeightInput, 0.0f, 2.0f, 2);
        drawFloat(contentY, "sector_editor_preview_jump_height", "Jump height", modalState.draftConfig.jumpHeight, modalState.jumpHeightInput, 0.0f, 3.0f, 2);
        drawFloat(contentY, "sector_editor_preview_head_bob_strength", "Head bob strength", modalState.draftConfig.headBobStrength, modalState.headBobStrengthInput, 0.0f, 0.25f, 3);
        drawFloat(contentY, "sector_editor_preview_head_bob_frequency", "Head bob frequency", modalState.draftConfig.headBobFrequency, modalState.headBobFrequencyInput, 0.0f, 20.0f, 2);
        engine::EndScrollArea(ui, config, input, scroll, modalState.generalScroll);
    };

    auto drawColorChannel = [&](float& localY, const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, localY, 92.0f, rowH},
                Rectangle{104.0f, localY, 230.0f, rowH},
                engine::UITextJustify::Right,
                channel,
                inputState);
        if (result.changed && result.channel != channel) {
            channel = result.channel;
            modalState.errorMessage.clear();
        }
        localY += rowH + gap;
    };

    auto drawSkyTab = [&]() {
        float contentY = 0.0f;
        const float contentH = 9.0f * (rowH + gap) + 80.0f;
        engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
                ui,
                config,
                input,
                "sector_editor_preview_settings_sky_scroll",
                scrollBounds,
                Vector2{scrollContentW, contentH},
                modalState.skyScroll);
        const float contentW = scroll.viewport.width;

        engine::Text(ui, config, assets, Rectangle{0.0f, contentY, labelW, rowH}, font, "Sky texture", engine::UITextJustify::Left, config.mutedTextColor);
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{inputX, contentY, std::max(0.0f, contentW - inputX), rowH},
                font,
                modalState.draftSkySettings.textureId.empty() ? "<none>" : modalState.draftSkySettings.textureId.c_str(),
                engine::UITextJustify::Left,
                config.textColor);
        contentY += rowH + gap;
        if (engine::Button(ui, config, input, assets, "sector_editor_preview_sky_pick_texture", Rectangle{inputX, contentY, 150.0f, rowH}, font, "Pick Texture")) {
            callbacks.openSkyTexturePicker();
        }
        if (engine::Button(ui, config, input, assets, "sector_editor_preview_sky_clear_texture", Rectangle{inputX + 162.0f, contentY, 112.0f, rowH}, font, "Clear")) {
            modalState.draftSkySettings.textureId.clear();
            modalState.errorMessage.clear();
        }
        contentY += rowH + gap;

        drawFloat(contentY, "sector_editor_preview_sky_yaw", "Yaw offset", modalState.draftSkySettings.yawOffsetDegrees, modalState.skyYawOffsetInput, -3600.0f, 3600.0f, 2);
        drawFloat(contentY, "sector_editor_preview_sky_vertical_offset", "Vertical offset", modalState.draftSkySettings.verticalOffset, modalState.skyVerticalOffsetInput, -100.0f, 100.0f, 3);
        drawFloat(contentY, "sector_editor_preview_sky_vertical_scale", "Vertical scale", modalState.draftSkySettings.verticalScale, modalState.skyVerticalScaleInput, 0.01f, 100.0f, 3);
        modalState.draftSkySettings.verticalScale = NormalizeSectorTopologySkySettings(modalState.draftSkySettings).verticalScale;

        contentY += 8.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, contentY, contentW, 34.0f}, font, "Top color", engine::UITextJustify::Left, config.textColor);
        contentY += 38.0f;
        drawColorChannel(contentY, "sector_editor_preview_sky_top_r", "R:", modalState.draftSkySettings.topColor.r, modalState.skyTopColorRedInput);
        drawColorChannel(contentY, "sector_editor_preview_sky_top_g", "G:", modalState.draftSkySettings.topColor.g, modalState.skyTopColorGreenInput);
        drawColorChannel(contentY, "sector_editor_preview_sky_top_b", "B:", modalState.draftSkySettings.topColor.b, modalState.skyTopColorBlueInput);
        modalState.draftSkySettings.topColor.a = 255;
        const Rectangle swatch{
                scroll.viewport.x + 104.0f,
                scroll.viewport.y - modalState.skyScroll.offset.y + contentY + 2.0f,
                std::min(130.0f, contentW - 104.0f),
                28.0f
        };
        DrawColorSwatch(config, swatch, NormalizeSectorTopologySkySettings(modalState.draftSkySettings).topColor, 1.0f);
        contentY += 36.0f + gap;

        engine::EndScrollArea(ui, config, input, scroll, modalState.skyScroll);
    };

    auto drawLightingTab = [&]() {
        float contentY = 0.0f;
        const float contentH = 9.0f * (rowH + gap) + 80.0f;
        engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
                ui,
                config,
                input,
                "sector_editor_preview_settings_lighting_scroll",
                scrollBounds,
                Vector2{scrollContentW, contentH},
                modalState.lightingScroll);
        const float contentW = scroll.viewport.width;

        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_preview_lighting_enabled",
                    Rectangle{0.0f, contentY, contentW, rowH},
                    font,
                    "Enabled",
                    modalState.draftDirectionalLight.enabled)) {
            modalState.errorMessage.clear();
        }
        contentY += rowH + gap;

        contentY += 8.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, contentY, contentW, 34.0f}, font, "Direction to light", engine::UITextJustify::Left, config.textColor);
        contentY += 38.0f;
        drawFloat(contentY, "sector_editor_preview_light_dir_x", "X", modalState.draftDirectionalLight.directionToLight.x, modalState.lightDirectionXInput, -1.0f, 1.0f, 3);
        drawFloat(contentY, "sector_editor_preview_light_dir_y", "Y", modalState.draftDirectionalLight.directionToLight.y, modalState.lightDirectionYInput, -1.0f, 1.0f, 3);
        drawFloat(contentY, "sector_editor_preview_light_dir_z", "Z", modalState.draftDirectionalLight.directionToLight.z, modalState.lightDirectionZInput, -1.0f, 1.0f, 3);

        drawFloat(contentY, "sector_editor_preview_light_intensity", "Intensity", modalState.draftDirectionalLight.intensity, modalState.lightIntensityInput, 0.0f, 16.0f, 3);
        modalState.draftDirectionalLight.intensity = std::max(0.0f, modalState.draftDirectionalLight.intensity);

        contentY += 8.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, contentY, contentW, 34.0f}, font, "Color", engine::UITextJustify::Left, config.textColor);
        contentY += 38.0f;
        drawColorChannel(contentY, "sector_editor_preview_light_r", "R:", modalState.draftDirectionalLight.color.r, modalState.lightColorRedInput);
        drawColorChannel(contentY, "sector_editor_preview_light_g", "G:", modalState.draftDirectionalLight.color.g, modalState.lightColorGreenInput);
        drawColorChannel(contentY, "sector_editor_preview_light_b", "B:", modalState.draftDirectionalLight.color.b, modalState.lightColorBlueInput);
        modalState.draftDirectionalLight.color.a = 255;
        const Rectangle swatch{
                scroll.viewport.x + 104.0f,
                scroll.viewport.y - modalState.lightingScroll.offset.y + contentY + 2.0f,
                std::min(130.0f, contentW - 104.0f),
                28.0f
        };
        DrawColorSwatch(config, swatch, NormalizeSectorTopologyDirectionalLightSettings(modalState.draftDirectionalLight).color, 1.0f);
        contentY += 36.0f + gap;

        engine::EndScrollArea(ui, config, input, scroll, modalState.lightingScroll);
    };

    if (modalState.activeTab == PreviewSettingsTab::Lighting) {
        drawLightingTab();
    } else if (modalState.activeTab == PreviewSettingsTab::Sky) {
        drawSkyTab();
    } else {
        drawGeneralTab();
    }

    if (!modalState.errorMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{modal.x + 30.0f, modal.y + modal.height - 116.0f, modal.width - 60.0f, 36.0f},
                font,
                modalState.errorMessage.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor);
    }

    const float buttonW = 132.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_preview_settings_reset", Rectangle{modal.x + 30.0f, buttonY, 176.0f, 44.0f}, font, "Reset Defaults")) {
        if (modalState.activeTab == PreviewSettingsTab::Lighting) {
            modalState.draftDirectionalLight = DefaultSectorTopologyDirectionalLightSettings();
            modalState.lightDirectionXInput = engine::UIFloatInputState{};
            modalState.lightDirectionYInput = engine::UIFloatInputState{};
            modalState.lightDirectionZInput = engine::UIFloatInputState{};
            modalState.lightIntensityInput = engine::UIFloatInputState{};
            modalState.lightColorRedInput = engine::UIIntInputState{};
            modalState.lightColorGreenInput = engine::UIIntInputState{};
            modalState.lightColorBlueInput = engine::UIIntInputState{};
        } else if (modalState.activeTab == PreviewSettingsTab::Sky) {
            modalState.draftSkySettings = DefaultSectorTopologySkySettings();
            modalState.skyYawOffsetInput = engine::UIFloatInputState{};
            modalState.skyVerticalOffsetInput = engine::UIFloatInputState{};
            modalState.skyVerticalScaleInput = engine::UIFloatInputState{};
            modalState.skyTopColorRedInput = engine::UIIntInputState{};
            modalState.skyTopColorGreenInput = engine::UIIntInputState{};
            modalState.skyTopColorBlueInput = engine::UIIntInputState{};
        } else {
            modalState.draftConfig = DefaultSectorFpsControllerConfig();
            modalState.walkSpeedInput = engine::UIFloatInputState{};
            modalState.runSpeedInput = engine::UIFloatInputState{};
            modalState.mouseSensitivityInput = engine::UIFloatInputState{};
            modalState.eyeHeightInput = engine::UIFloatInputState{};
            modalState.gravityInput = engine::UIFloatInputState{};
            modalState.playerRadiusInput = engine::UIFloatInputState{};
            modalState.playerHeightInput = engine::UIFloatInputState{};
            modalState.stepHeightInput = engine::UIFloatInputState{};
            modalState.jumpHeightInput = engine::UIFloatInputState{};
            modalState.headBobStrengthInput = engine::UIFloatInputState{};
            modalState.headBobFrequencyInput = engine::UIFloatInputState{};
        }
        modalState.errorMessage.clear();
    }
    okayRequested = okayRequested || engine::Button(
            ui,
            config,
            input,
            assets,
            "sector_editor_preview_settings_ok",
            Rectangle{modal.x + modal.width - buttonW * 2.0f - 38.0f, buttonY, buttonW, 44.0f},
            font,
            "OK");
    cancelRequested = cancelRequested || engine::Button(
            ui,
            config,
            input,
            assets,
            "sector_editor_preview_settings_cancel",
            Rectangle{modal.x + modal.width - buttonW - 26.0f, buttonY, buttonW, 44.0f},
            font,
            "Cancel");

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });

    if (cancelRequested) {
        callbacks.close();
        return;
    }
    if (okayRequested) {
        callbacks.apply();
    }
}

} // namespace game
