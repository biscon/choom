#include "sector_editor/SectorEditorPreviewSettingsModal.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"

#include <algorithm>

namespace game {

namespace {

float ScrollAreaContentWidthForVerticalScrollbar(float boundsWidth, const engine::UIConfig& config)
{
    const float clientWidth = std::max(0.0f, boundsWidth - config.borderThickness * 2.0f);
    return std::max(0.0f, clientWidth - config.scrollbarSize - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

} // namespace

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
            (EditorWidth - 900.0f) * 0.5f,
            (EditorHeight - 700.0f) * 0.5f,
            900.0f,
            700.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 22.0f;
    engine::Text(config, assets, Rectangle{modal.x + 26.0f, y, modal.width - 52.0f, 42.0f}, font, "Preview Settings");
    y += 54.0f;

    const float tabH = 38.0f;
    const std::array<Rectangle, 6> tabRects =
            BuildSectorPreviewSettingsTabLayout(modal, y, tabH);
    if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                "sector_editor_preview_settings_tab_general",
                tabRects[0],
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
                tabRects[1],
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
                tabRects[2],
                font,
                "Lighting",
                modalState.activeTab == PreviewSettingsTab::Lighting)) {
        modalState.activeTab = PreviewSettingsTab::Lighting;
    }
    if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                "sector_editor_preview_settings_tab_fog",
                tabRects[3],
                font,
                "Fog",
                modalState.activeTab == PreviewSettingsTab::Fog)) {
        modalState.activeTab = PreviewSettingsTab::Fog;
    }
    if (engine::ToolButton(
                ui, config, input, assets,
                "sector_editor_preview_settings_tab_viewmodel",
                tabRects[4], font, "Arms",
                modalState.activeTab == PreviewSettingsTab::Viewmodel)) {
        modalState.activeTab = PreviewSettingsTab::Viewmodel;
    }
    if (engine::ToolButton(
                ui, config, input, assets,
                "sector_editor_preview_settings_tab_weapon",
                tabRects[5], font, "Weapon",
                modalState.activeTab == PreviewSettingsTab::Weapon)) {
        modalState.activeTab = PreviewSettingsTab::Weapon;
    }
    y += tabH + 16.0f;

    const float buttonY = modal.y + modal.height - 66.0f;
    const Rectangle scrollBounds{
            modal.x + 30.0f,
            y,
            modal.width - 60.0f,
            std::max(80.0f, buttonY - y - 18.0f)
    };
    const float scrollContentW = ScrollAreaContentWidthForVerticalScrollbar(scrollBounds.width, config);
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
    auto drawInt = [&](float& localY, const char* id, const char* label,
                       int& value, engine::UIIntInputState& inputState,
                       int minValue, int maxValue) {
        const SectorEditorIntInputResult result = DrawLabeledIntInput(
                ui, config, input, assets, font, id, label,
                Rectangle{0.0f, localY, labelW, rowH},
                Rectangle{inputX, localY, inputW, rowH},
                engine::UITextJustify::Left,
                value, inputState, minValue, maxValue, 1);
        value = result.value;
        if (result.changed) modalState.errorMessage.clear();
        localY += rowH + gap;
    };

    auto drawGeneralTab = [&]() {
        float contentY = 0.0f;
        const float contentH = 12.0f * (rowH + gap) + 12.0f;
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
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_preview_npc_to_npc_collision",
                    Rectangle{0.0f, contentY, contentW, rowH},
                    font,
                    "NPC-to-NPC collision",
                    modalState.draftNpcToNpcCollisionEnabled)) {
            modalState.errorMessage.clear();
        }
        contentY += rowH + gap;
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
        const float contentH =
                MeasureSectorPreviewSettingsLightingContentHeight(rowH, gap);
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

        contentY += 8.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, contentY, contentW, 34.0f}, font, "Object probes", engine::UITextJustify::Left, config.textColor);
        contentY += 38.0f;
        drawFloat(
                contentY,
                "sector_editor_preview_object_probe_spacing",
                "Probe spacing",
                modalState.draftLightmapSettings.objectProbeSpacingWorld,
                modalState.objectProbeSpacingInput,
                0.25f,
                128.0f,
                2);
        drawFloat(
                contentY,
                "sector_editor_preview_object_probe_lower_height",
                "Lower height",
                modalState.draftLightmapSettings.objectProbeLowerHeightWorld,
                modalState.objectProbeLowerHeightInput,
                0.0f,
                16.0f,
                2);
        drawFloat(
                contentY,
                "sector_editor_preview_object_probe_upper_height",
                "Upper height",
                modalState.draftLightmapSettings.objectProbeUpperHeightWorld,
                modalState.objectProbeUpperHeightInput,
                0.0f,
                16.0f,
                2);
        modalState.draftLightmapSettings =
                NormalizeSectorPreviewObjectProbeSettings(modalState.draftLightmapSettings);

        contentY += 8.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, contentY, contentW, 34.0f},
                font, "Scene-wide HDR bloom", engine::UITextJustify::Left, config.textColor);
        contentY += 38.0f;
        engine::Checkbox(ui, config, input, assets,
                "sector_editor_preview_bloom_enabled",
                Rectangle{0.0f, contentY, contentW, rowH}, font,
                "Enabled", modalState.draftHdrBloom.enabled);
        contentY += rowH + gap;
        drawFloat(contentY, "sector_editor_preview_bloom_threshold", "Linear threshold",
                modalState.draftHdrBloom.threshold, modalState.bloomThresholdInput,
                0.0f, engine::Rgba16fMaximumFinite, 3);
        drawFloat(contentY, "sector_editor_preview_bloom_knee", "Soft knee",
                modalState.draftHdrBloom.softKnee, modalState.bloomSoftKneeInput,
                0.0f, 1.0f, 3);
        drawFloat(contentY, "sector_editor_preview_bloom_intensity", "Intensity",
                modalState.draftHdrBloom.intensity, modalState.bloomIntensityInput,
                0.0f, 16.0f, 3);
        drawFloat(contentY, "sector_editor_preview_bloom_radius", "Radius",
                modalState.draftHdrBloom.radius, modalState.bloomRadiusInput,
                0.25f, 4.0f, 2);
        modalState.draftHdrBloom = engine::NormalizeHdrBloomSettings(modalState.draftHdrBloom);

        engine::EndScrollArea(ui, config, input, scroll, modalState.lightingScroll);
    };

    auto drawFogTab = [&]() {
        float contentY = 0.0f;
        const float contentH =
                MeasureSectorPreviewSettingsFogContentHeight(rowH, gap);
        engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
                ui,
                config,
                input,
                "sector_editor_preview_settings_fog_scroll",
                scrollBounds,
                Vector2{scrollContentW, contentH},
                modalState.fogScroll);
        const float contentW = scroll.viewport.width;

        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_preview_fog_enabled",
                    Rectangle{0.0f, contentY, contentW, rowH},
                    font,
                    "Enabled",
                    modalState.draftFogSettings.enabled)) {
            modalState.errorMessage.clear();
        }
        contentY += rowH + gap;

        bool distanceMode = modalState.draftFogSettings.mode
                == SectorTopologyFogMode::Distance;
        if (engine::Checkbox(
                    ui, config, input, assets,
                    "sector_editor_preview_fog_distance_mode",
                    Rectangle{0.0f, contentY, contentW, rowH},
                    font, "HPL-style distance mode", distanceMode)) {
            modalState.draftFogSettings.mode = distanceMode
                    ? SectorTopologyFogMode::Distance
                    : SectorTopologyFogMode::LegacyHeight;
            modalState.errorMessage.clear();
        }
        contentY += rowH + gap;

        drawFloat(contentY, "sector_editor_preview_fog_start_distance", "Start distance", modalState.draftFogSettings.startDistanceWorld, modalState.fogStartDistanceInput, 0.0f, 512.0f, 2);
        drawFloat(contentY, "sector_editor_preview_fog_max_opacity", "Maximum opacity", modalState.draftFogSettings.maxOpacity, modalState.fogMaxOpacityInput, 0.0f, 1.0f, 3);
        if (distanceMode) {
            drawFloat(contentY, "sector_editor_preview_fog_end_distance", "End distance", modalState.draftFogSettings.endDistanceWorld, modalState.fogEndDistanceInput, 0.01f, 4096.0f, 2);
            drawFloat(contentY, "sector_editor_preview_fog_falloff_exponent", "Falloff exponent", modalState.draftFogSettings.falloffExponent, modalState.fogFalloffExponentInput, 0.05f, 8.0f, 3);
            drawFloat(contentY, "sector_editor_preview_fog_brightness", "Brightness", modalState.draftFogSettings.brightness, modalState.fogBrightnessInput, 0.0f, 16.0f, 3);
        } else {
            drawFloat(contentY, "sector_editor_preview_fog_density", "Density", modalState.draftFogSettings.density, modalState.fogDensityInput, 0.0f, 1.0f, 4);
            drawFloat(contentY, "sector_editor_preview_fog_reference_height", "Reference height", modalState.draftFogSettings.referenceHeightWorld, modalState.fogReferenceHeightInput, -512.0f, 512.0f, 2);
            drawFloat(contentY, "sector_editor_preview_fog_height_falloff", "Height falloff", modalState.draftFogSettings.heightFalloff, modalState.fogHeightFalloffInput, 0.0f, 16.0f, 3);
        }
        modalState.draftFogSettings = NormalizeSectorTopologyFogSettings(modalState.draftFogSettings);

        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, contentY, contentW, 32.0f},
                font,
                distanceMode
                        ? "Fog reaches maximum opacity at the end distance."
                        : "Height falloff 0 produces uniform exponential fog.",
                engine::UITextJustify::Left,
                config.mutedTextColor);
        contentY += 36.0f + gap;

        engine::Text(ui, config, assets, Rectangle{0.0f, contentY, contentW, 34.0f}, font, "Color", engine::UITextJustify::Left, config.textColor);
        contentY += 38.0f;
        drawColorChannel(contentY, "sector_editor_preview_fog_r", "R:", modalState.draftFogSettings.color.r, modalState.fogColorRedInput);
        drawColorChannel(contentY, "sector_editor_preview_fog_g", "G:", modalState.draftFogSettings.color.g, modalState.fogColorGreenInput);
        drawColorChannel(contentY, "sector_editor_preview_fog_b", "B:", modalState.draftFogSettings.color.b, modalState.fogColorBlueInput);
        modalState.draftFogSettings.color.a = 255;
        const Rectangle swatch{
                scroll.viewport.x + 104.0f,
                scroll.viewport.y - modalState.fogScroll.offset.y + contentY + 2.0f,
                std::min(130.0f, contentW - 104.0f),
                28.0f
        };
        DrawColorSwatch(config, swatch, NormalizeSectorTopologyFogSettings(modalState.draftFogSettings).color, 1.0f);
        contentY += 36.0f + gap;

        engine::EndScrollArea(ui, config, input, scroll, modalState.fogScroll);
    };

    auto drawViewmodelTab = [&]() {
        float contentY = 0.0f;
        const float contentH = 20.0f * (rowH + gap) + 12.0f;
        engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
                ui, config, input, "sector_editor_preview_settings_viewmodel_scroll",
                scrollBounds, Vector2{scrollContentW, contentH}, modalState.viewmodelScroll);
        drawFloat(contentY, "sector_editor_viewmodel_position_x", "Position X (right)", modalState.draftViewmodel.position.x, modalState.viewmodelPositionXInput, -10.0f, 10.0f, 3);
        drawFloat(contentY, "sector_editor_viewmodel_position_y", "Position Y (up)", modalState.draftViewmodel.position.y, modalState.viewmodelPositionYInput, -10.0f, 10.0f, 3);
        drawFloat(contentY, "sector_editor_viewmodel_position_z", "Position Z (forward)", modalState.draftViewmodel.position.z, modalState.viewmodelPositionZInput, -10.0f, 10.0f, 3);
        drawFloat(contentY, "sector_editor_viewmodel_pitch", "Local pitch", modalState.draftViewmodel.rotationDegrees.x, modalState.viewmodelPitchInput, -360.0f, 360.0f, 2);
        drawFloat(contentY, "sector_editor_viewmodel_yaw", "Local yaw", modalState.draftViewmodel.rotationDegrees.y, modalState.viewmodelYawInput, -360.0f, 360.0f, 2);
        drawFloat(contentY, "sector_editor_viewmodel_roll", "Local roll", modalState.draftViewmodel.rotationDegrees.z, modalState.viewmodelRollInput, -360.0f, 360.0f, 2);
        drawFloat(contentY, "sector_editor_viewmodel_scale", "Scale", modalState.draftViewmodel.scale, modalState.viewmodelScaleInput, 0.01f, 10.0f, 3);
        drawFloat(contentY, "sector_editor_viewmodel_fov", "Vertical FOV", modalState.draftViewmodel.verticalFovDegrees, modalState.viewmodelFovInput, 20.0f, 120.0f, 2);

        engine::Text(
                ui, config, assets,
                Rectangle{0.0f, contentY, scrollContentW, rowH},
                font, "Camera recoil", engine::UITextJustify::Left,
                config.textColor);
        contentY += rowH + gap;
        FpsWeaponCameraRecoilDefinition& cameraRecoil =
                modalState.draftWeaponFiring.cameraRecoil;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_camera_recoil_enabled",
                    Rectangle{0.0f, contentY, scrollContentW, rowH},
                    font,
                    "Enabled",
                    cameraRecoil.enabled)) {
            modalState.errorMessage.clear();
        }
        contentY += rowH + gap;
        drawFloat(contentY, "sector_editor_camera_recoil_pitch_kick", "Pitch kick (degrees)", cameraRecoil.pitchKickDegrees, modalState.cameraRecoilPitchKickInput, 0.0f, 45.0f, 3);
        drawFloat(contentY, "sector_editor_camera_recoil_pitch_variation", "Pitch variation", cameraRecoil.pitchVariationDegrees, modalState.cameraRecoilPitchVariationInput, 0.0f, 45.0f, 3);
        drawFloat(contentY, "sector_editor_camera_recoil_yaw_variation", "Yaw variation", cameraRecoil.yawVariationDegrees, modalState.cameraRecoilYawVariationInput, 0.0f, 45.0f, 3);
        drawFloat(contentY, "sector_editor_camera_recoil_roll_variation", "Roll variation", cameraRecoil.rollVariationDegrees, modalState.cameraRecoilRollVariationInput, 0.0f, 45.0f, 3);
        drawFloat(contentY, "sector_editor_camera_recoil_frequency", "Spring frequency (Hz)", cameraRecoil.springFrequencyHz, modalState.cameraRecoilFrequencyInput, 0.5f, 40.0f, 2);
        drawFloat(contentY, "sector_editor_camera_recoil_damping", "Spring damping ratio", cameraRecoil.springDampingRatio, modalState.cameraRecoilDampingInput, 0.1f, 3.0f, 2);
        drawFloat(contentY, "sector_editor_camera_recoil_max_pitch", "Maximum pitch", cameraRecoil.maxPitchDegrees, modalState.cameraRecoilMaxPitchInput, 0.0f, 90.0f, 2);
        drawFloat(contentY, "sector_editor_camera_recoil_max_yaw", "Maximum yaw", cameraRecoil.maxYawDegrees, modalState.cameraRecoilMaxYawInput, 0.0f, 90.0f, 2);
        drawFloat(contentY, "sector_editor_camera_recoil_max_roll", "Maximum roll", cameraRecoil.maxRollDegrees, modalState.cameraRecoilMaxRollInput, 0.0f, 90.0f, 2);
        modalState.draftViewmodel = ClampFpsViewmodelPresentation(modalState.draftViewmodel);
        modalState.draftWeaponFiring = ClampFpsWeaponFiringDefinition(
                modalState.draftWeaponFiring);
        engine::EndScrollArea(ui, config, input, scroll, modalState.viewmodelScroll);
    };

    auto drawWeaponTab = [&]() {
        float contentY = 0.0f;
        const float contentH = 52.0f * (rowH + gap) + 160.0f;
        engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
                ui, config, input, "sector_editor_preview_settings_weapon_scroll",
                scrollBounds, Vector2{scrollContentW, contentH}, modalState.weaponScroll);
        const auto section = [&](const char* title) {
            engine::Text(ui, config, assets,
                    Rectangle{0.0f, contentY, scrollContentW, rowH},
                    font, title, engine::UITextJustify::Left, config.textColor);
            contentY += rowH + gap;
        };

        section("Holster animation");
        drawFloat(contentY, "sector_editor_weapon_holster_duration", "Holster duration (seconds)", modalState.draftViewmodelHolsterTransition.holsterDurationSeconds, modalState.viewmodelHolsterDurationInput, 0.05f, 2.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_unholster_duration", "Unholster duration (seconds)", modalState.draftViewmodelHolsterTransition.unholsterDurationSeconds, modalState.viewmodelUnholsterDurationInput, 0.05f, 2.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_hidden_x", "Hidden X (right)", modalState.draftViewmodelHolsterTransition.hiddenTranslation.x, modalState.viewmodelHiddenTranslationXInput, -10.0f, 10.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_hidden_y", "Hidden Y (up)", modalState.draftViewmodelHolsterTransition.hiddenTranslation.y, modalState.viewmodelHiddenTranslationYInput, -10.0f, 10.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_hidden_z", "Hidden Z (forward)", modalState.draftViewmodelHolsterTransition.hiddenTranslation.z, modalState.viewmodelHiddenTranslationZInput, -10.0f, 10.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_hidden_pitch", "Hidden pitch", modalState.draftViewmodelHolsterTransition.hiddenRotationDegrees.x, modalState.viewmodelHiddenPitchInput, -360.0f, 360.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_hidden_yaw", "Hidden yaw", modalState.draftViewmodelHolsterTransition.hiddenRotationDegrees.y, modalState.viewmodelHiddenYawInput, -360.0f, 360.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_hidden_roll", "Hidden roll", modalState.draftViewmodelHolsterTransition.hiddenRotationDegrees.z, modalState.viewmodelHiddenRollInput, -360.0f, 360.0f, 2);

        section("Weapon grip correction");
        drawFloat(contentY, "sector_editor_weapon_grip_x", "Grip translation X", modalState.draftViewmodelGrip.translation.x, modalState.viewmodelGripTranslationXInput, -1.0f, 1.0f, 4);
        drawFloat(contentY, "sector_editor_weapon_grip_y", "Grip translation Y", modalState.draftViewmodelGrip.translation.y, modalState.viewmodelGripTranslationYInput, -1.0f, 1.0f, 4);
        drawFloat(contentY, "sector_editor_weapon_grip_z", "Grip translation Z", modalState.draftViewmodelGrip.translation.z, modalState.viewmodelGripTranslationZInput, -1.0f, 1.0f, 4);
        drawFloat(contentY, "sector_editor_weapon_grip_pitch", "Grip pitch", modalState.draftViewmodelGrip.rotationDegrees.x, modalState.viewmodelGripPitchInput, -360.0f, 360.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_grip_yaw", "Grip yaw", modalState.draftViewmodelGrip.rotationDegrees.y, modalState.viewmodelGripYawInput, -360.0f, 360.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_grip_roll", "Grip roll", modalState.draftViewmodelGrip.rotationDegrees.z, modalState.viewmodelGripRollInput, -360.0f, 360.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_grip_scale", "Grip scale", modalState.draftViewmodelGrip.scale, modalState.viewmodelGripScaleInput, 0.01f, 10.0f, 4);

        section("Weapon lighting");
        drawFloat(contentY, "sector_editor_weapon_brightness", "Pistol brightness", modalState.draftViewmodelAttachmentLighting.brightnessAdjustment, modalState.viewmodelAttachmentBrightnessInput, -1.0f, 1.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_metallic", "Pistol metallic factor", modalState.draftViewmodelAttachmentLighting.materialOverride.metallicFactor, modalState.viewmodelAttachmentMetallicInput, 0.0f, 1.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_roughness", "Pistol roughness factor", modalState.draftViewmodelAttachmentLighting.materialOverride.roughnessFactor, modalState.viewmodelAttachmentRoughnessInput, 0.045f, 1.0f, 3);

        section("Firing and recoil");
        FpsWeaponFiringDefinition& firing = modalState.draftWeaponFiring;
        drawFloat(contentY, "sector_editor_weapon_shot_interval", "Shot interval", firing.shotIntervalSeconds, modalState.weaponShotIntervalInput, 0.03f, 5.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_recoil_x", "Recoil translation X", firing.recoil.translationImpulse.x, modalState.weaponRecoilTranslationXInput, -1.0f, 1.0f, 4);
        drawFloat(contentY, "sector_editor_weapon_recoil_y", "Recoil translation Y", firing.recoil.translationImpulse.y, modalState.weaponRecoilTranslationYInput, -1.0f, 1.0f, 4);
        drawFloat(contentY, "sector_editor_weapon_recoil_z", "Recoil translation Z", firing.recoil.translationImpulse.z, modalState.weaponRecoilTranslationZInput, -1.0f, 1.0f, 4);
        drawFloat(contentY, "sector_editor_weapon_recoil_pitch", "Recoil pitch", firing.recoil.rotationImpulseDegrees.x, modalState.weaponRecoilPitchInput, -45.0f, 45.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_recoil_yaw", "Recoil yaw", firing.recoil.rotationImpulseDegrees.y, modalState.weaponRecoilYawInput, -45.0f, 45.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_recoil_roll", "Recoil roll", firing.recoil.rotationImpulseDegrees.z, modalState.weaponRecoilRollInput, -45.0f, 45.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_roll_variation", "Roll variation", firing.recoil.rollVariationDegrees, modalState.weaponRecoilRollVariationInput, 0.0f, 10.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_recoil_frequency", "Spring frequency", firing.recoil.springFrequencyHz, modalState.weaponRecoilFrequencyInput, 0.5f, 40.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_recoil_damping", "Spring damping ratio", firing.recoil.dampingRatio, modalState.weaponRecoilDampingInput, 0.1f, 3.0f, 2);

        section("Muzzle socket and effects");
        drawFloat(contentY, "sector_editor_weapon_muzzle_x", "Muzzle position X", firing.muzzleSocket.position.x, modalState.weaponMuzzlePositionXInput, -2.0f, 2.0f, 4);
        drawFloat(contentY, "sector_editor_weapon_muzzle_y", "Muzzle position Y", firing.muzzleSocket.position.y, modalState.weaponMuzzlePositionYInput, -2.0f, 2.0f, 4);
        drawFloat(contentY, "sector_editor_weapon_muzzle_z", "Muzzle position Z", firing.muzzleSocket.position.z, modalState.weaponMuzzlePositionZInput, -2.0f, 2.0f, 4);
        drawFloat(contentY, "sector_editor_weapon_muzzle_pitch", "Muzzle pitch", firing.muzzleSocket.rotationDegrees.x, modalState.weaponMuzzlePitchInput, -360.0f, 360.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_muzzle_yaw", "Muzzle yaw", firing.muzzleSocket.rotationDegrees.y, modalState.weaponMuzzleYawInput, -360.0f, 360.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_muzzle_roll", "Muzzle roll", firing.muzzleSocket.rotationDegrees.z, modalState.weaponMuzzleRollInput, -360.0f, 360.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_flash_lifetime", "Flash lifetime", firing.muzzleFlash.lifetimeSeconds, modalState.weaponFlashLifetimeInput, 0.005f, 60.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_flash_size", "Flash size", firing.muzzleFlash.sizeWorld, modalState.weaponFlashSizeInput, 0.005f, 2.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_flash_radiance", "Flash radiance strength", firing.muzzleFlash.radianceStrength, modalState.weaponFlashRadianceStrengthInput, 0.0f, 64.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_flash_size_variation", "Flash size variation", firing.muzzleFlash.sizeVariation, modalState.weaponFlashSizeVariationInput, 0.0f, 0.5f, 3);
        drawFloat(contentY, "sector_editor_weapon_flash_irregularity", "Flash irregularity", firing.muzzleFlash.irregularity, modalState.weaponFlashIrregularityInput, 0.0f, 1.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_flash_forward_stretch", "Flash forward stretch", firing.muzzleFlash.forwardStretch, modalState.weaponFlashForwardStretchInput, 1.0f, 4.0f, 2);
        drawInt(contentY, "sector_editor_weapon_flash_min_lobes", "Flash minimum lobes", firing.muzzleFlash.minimumLobeCount, modalState.weaponFlashMinimumLobesInput, 3, MaxFpsMuzzleFlashLobes);
        drawInt(contentY, "sector_editor_weapon_flash_max_lobes", "Flash maximum lobes", firing.muzzleFlash.maximumLobeCount, modalState.weaponFlashMaximumLobesInput, 3, MaxFpsMuzzleFlashLobes);
        drawFloat(contentY, "sector_editor_weapon_flash_rear_suppression", "Flash rear suppression", firing.muzzleFlash.rearSuppression, modalState.weaponFlashRearSuppressionInput, 0.0f, 1.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_flash_edge_softness", "Flash edge softness", firing.muzzleFlash.edgeSoftness, modalState.weaponFlashEdgeSoftnessInput, 0.01f, 1.0f, 3);
        drawFloat(contentY, "sector_editor_weapon_light_intensity", "Muzzle-light intensity", firing.muzzleLight.intensity, modalState.weaponLightIntensityInput, 0.0f, 100.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_light_radius", "Muzzle-light radius", firing.muzzleLight.radiusWorld, modalState.weaponLightRadiusInput, 0.05f, 100.0f, 2);
        drawFloat(contentY, "sector_editor_weapon_light_lifetime", "Muzzle-light lifetime", firing.muzzleLight.lifetimeSeconds, modalState.weaponLightLifetimeInput, 0.005f, 2.0f, 3);

        modalState.draftViewmodelHolsterTransition = ClampFpsViewmodelHolsterTransition(modalState.draftViewmodelHolsterTransition);
        modalState.draftViewmodelGrip = ClampFpsViewmodelGripCorrection(modalState.draftViewmodelGrip);
        modalState.draftViewmodelAttachmentLighting = ClampFpsViewmodelAttachmentLighting(modalState.draftViewmodelAttachmentLighting);
        modalState.draftWeaponFiring = ClampFpsWeaponFiringDefinition(modalState.draftWeaponFiring);
        engine::EndScrollArea(ui, config, input, scroll, modalState.weaponScroll);
    };

    if (modalState.activeTab == PreviewSettingsTab::Weapon) {
        drawWeaponTab();
    } else if (modalState.activeTab == PreviewSettingsTab::Viewmodel) {
        drawViewmodelTab();
    } else if (modalState.activeTab == PreviewSettingsTab::Fog) {
        drawFogTab();
    } else if (modalState.activeTab == PreviewSettingsTab::Lighting) {
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
        if (modalState.activeTab == PreviewSettingsTab::Viewmodel) {
            modalState.draftViewmodel = modalState.viewmodelDefaults;
            modalState.draftWeaponFiring.cameraRecoil =
                    modalState.weaponFiringDefaults.cameraRecoil;
            modalState.viewmodelPositionXInput = {};
            modalState.viewmodelPositionYInput = {};
            modalState.viewmodelPositionZInput = {};
            modalState.viewmodelPitchInput = {};
            modalState.viewmodelYawInput = {};
            modalState.viewmodelRollInput = {};
            modalState.viewmodelScaleInput = {};
            modalState.viewmodelFovInput = {};
            modalState.cameraRecoilPitchKickInput = {};
            modalState.cameraRecoilPitchVariationInput = {};
            modalState.cameraRecoilYawVariationInput = {};
            modalState.cameraRecoilRollVariationInput = {};
            modalState.cameraRecoilFrequencyInput = {};
            modalState.cameraRecoilDampingInput = {};
            modalState.cameraRecoilMaxPitchInput = {};
            modalState.cameraRecoilMaxYawInput = {};
            modalState.cameraRecoilMaxRollInput = {};
        } else if (modalState.activeTab == PreviewSettingsTab::Weapon) {
            modalState.draftViewmodelHolsterTransition =
                    modalState.viewmodelHolsterTransitionDefaults;
            modalState.viewmodelHolsterDurationInput = {};
            modalState.viewmodelUnholsterDurationInput = {};
            modalState.viewmodelHiddenTranslationXInput = {};
            modalState.viewmodelHiddenTranslationYInput = {};
            modalState.viewmodelHiddenTranslationZInput = {};
            modalState.viewmodelHiddenPitchInput = {};
            modalState.viewmodelHiddenYawInput = {};
            modalState.viewmodelHiddenRollInput = {};
            modalState.draftViewmodelGrip = modalState.viewmodelGripDefaults;
            modalState.viewmodelGripTranslationXInput = {};
            modalState.viewmodelGripTranslationYInput = {};
            modalState.viewmodelGripTranslationZInput = {};
            modalState.viewmodelGripPitchInput = {};
            modalState.viewmodelGripYawInput = {};
            modalState.viewmodelGripRollInput = {};
            modalState.viewmodelGripScaleInput = {};
            modalState.draftViewmodelAttachmentLighting =
                    modalState.viewmodelAttachmentLightingDefaults;
            modalState.viewmodelAttachmentBrightnessInput = {};
            modalState.viewmodelAttachmentMetallicInput = {};
            modalState.viewmodelAttachmentRoughnessInput = {};
            modalState.draftWeaponFiring = modalState.weaponFiringDefaults;
            modalState.weaponShotIntervalInput = {};
            modalState.weaponRecoilTranslationXInput = {};
            modalState.weaponRecoilTranslationYInput = {};
            modalState.weaponRecoilTranslationZInput = {};
            modalState.weaponRecoilPitchInput = {};
            modalState.weaponRecoilYawInput = {};
            modalState.weaponRecoilRollInput = {};
            modalState.weaponRecoilRollVariationInput = {};
            modalState.weaponRecoilFrequencyInput = {};
            modalState.weaponRecoilDampingInput = {};
            modalState.weaponMuzzlePositionXInput = {};
            modalState.weaponMuzzlePositionYInput = {};
            modalState.weaponMuzzlePositionZInput = {};
            modalState.weaponMuzzlePitchInput = {};
            modalState.weaponMuzzleYawInput = {};
            modalState.weaponMuzzleRollInput = {};
            modalState.weaponFlashLifetimeInput = {};
            modalState.weaponFlashSizeInput = {};
            modalState.weaponFlashRadianceStrengthInput = {};
            modalState.weaponFlashSizeVariationInput = {};
            modalState.weaponFlashIrregularityInput = {};
            modalState.weaponFlashForwardStretchInput = {};
            modalState.weaponFlashMinimumLobesInput = {};
            modalState.weaponFlashMaximumLobesInput = {};
            modalState.weaponFlashRearSuppressionInput = {};
            modalState.weaponFlashEdgeSoftnessInput = {};
            modalState.weaponLightIntensityInput = {};
            modalState.weaponLightRadiusInput = {};
            modalState.weaponLightLifetimeInput = {};
        } else if (modalState.activeTab == PreviewSettingsTab::Fog) {
            ResetSectorPreviewSettingsModalFogDefaults(modalState);
        } else if (modalState.activeTab == PreviewSettingsTab::Lighting) {
            ResetSectorPreviewSettingsModalLightingDefaults(modalState);
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
            modalState.draftNpcToNpcCollisionEnabled =
                    DefaultSectorPreviewSettings().npcToNpcCollisionEnabled;
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
