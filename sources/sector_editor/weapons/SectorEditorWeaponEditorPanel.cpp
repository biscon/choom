#include "sector_editor/weapons/SectorEditorWeaponEditorPanel.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/services/static_model_picker/SectorEditorModelPickerModal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace game {
namespace {

constexpr float RowHeight = 40.0f;
constexpr float RowGap = 9.0f;
constexpr float LabelWidth = 220.0f;

float ScrollContentWidth(float boundsWidth, const engine::UIConfig& config)
{
    const float clientWidth = std::max(
            0.0f,
            boundsWidth - config.borderThickness * 2.0f);
    return std::max(
            0.0f,
            clientWidth - config.scrollbarSize
                    - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

void ConsumeRemainingInput(engine::Input& input)
{
    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
}

void RefreshAnimationOptions(
        engine::AssetManager& assets,
        const FpsViewmodelRuntimeState* viewmodel,
        SectorEditorWeaponEditorState& state)
{
    const engine::ModelHandle model = viewmodel == nullptr
            ? engine::NullModelHandle()
            : viewmodel->modelInstance.model;
    if (state.animationOptionsModel == model) return;
    state.animationOptionsModel = model;
    state.animationOptionStorage.clear();
    state.animationOptions.clear();
    const engine::ModelAsset* asset = assets.GetModelAsset(model);
    if (asset == nullptr) return;
    state.animationOptionStorage.reserve(
            static_cast<size_t>(std::max(0, asset->animationCount)));
    for (int index = 0; index < asset->animationCount; ++index) {
        state.animationOptionStorage.emplace_back(asset->animations[index].name);
    }
    state.animationOptions.reserve(state.animationOptionStorage.size());
    for (const std::string& name : state.animationOptionStorage) {
        state.animationOptions.push_back(name.c_str());
    }
}

} // namespace

SectorEditorWeaponEditorPanelResult DrawSectorEditorWeaponEditorPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        const FpsViewmodelRuntimeState* viewmodel,
        SectorEditorWeaponEditorService& editor,
        SectorEditorStaticModelPickerService& modelPicker,
        SectorEditorAudioAssetPickerService& audioPicker)
{
    SectorEditorWeaponEditorPanelResult result;
    SectorEditorWeaponEditorState& state = editor.State();
    if (!state.open) return result;
    const SectorEditorWeaponEditorLayout layout =
            BuildSectorEditorWeaponEditorLayoutForViewport(
                    EditorWidth,
                    EditorHeight,
                    state.openedFromPreview3D);

    if (modelPicker.State().open
            && (modelPicker.State().target == ModelPickerTarget::WeaponArms
                    || modelPicker.State().target
                            == ModelPickerTarget::WeaponAttachment)) {
        const SectorEditorModelPickerModalResult pickerResult =
                DrawSectorEditorModelPickerModal(
                        ui, config, input, assets, font, modelPicker);
        if (pickerResult == SectorEditorModelPickerModalResult::Selected) {
            if (modelPicker.State().target == ModelPickerTarget::WeaponArms) {
                editor.SetArmsModelPath(modelPicker.SelectedModelPath());
            } else {
                editor.SetAttachmentModelPath(modelPicker.SelectedModelPath());
            }
            modelPicker.State().open = false;
        }
        return result;
    }
    if (state.audioPicker.open) {
        const SectorEditorAudioAssetPickerResult pickerResult =
                audioPicker.DrawModal(ui, config, input, font, state.audioPicker);
        if (pickerResult == SectorEditorAudioAssetPickerResult::Selected) {
            const std::string path = audioPicker.SelectedPath(state.audioPicker);
            switch (state.audioPickerTarget) {
                case SectorEditorWeaponAudioTarget::Shoot:
                    editor.SetShootSoundPath(path, assets);
                    break;
                case SectorEditorWeaponAudioTarget::DryFire:
                    editor.SetDryFireSoundPath(path, assets);
                    break;
                case SectorEditorWeaponAudioTarget::Reload:
                    editor.SetReloadSoundPath(path, assets);
                    break;
            }
            audioPicker.Close(state.audioPicker);
        } else if (pickerResult == SectorEditorAudioAssetPickerResult::Cancelled) {
            audioPicker.Close(state.audioPicker);
        }
        return result;
    }

    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested, &state](engine::InputEvent& event) {
                if (event.key.key != KEY_ESCAPE) return;
                if (state.deleteConfirmationOpen) {
                    state.deleteConfirmationOpen = false;
                    state.deleteConfirmationId.clear();
                } else {
                    cancelRequested = true;
                }
                engine::ConsumeEvent(event);
            });

    if (!state.openedFromPreview3D) {
        DrawRectangle(0, 0, static_cast<int>(EditorWidth),
                static_cast<int>(EditorHeight), Color{0, 0, 0, 150});
    }
    DrawRectangleRec(layout.panel, Color{20, 24, 32, 250});
    DrawRectangleLinesEx(layout.panel, config.borderThickness, config.borderColor);
    engine::Text(
            config, assets,
            Rectangle{layout.panel.x + 20.0f, layout.panel.y + 14.0f,
                    layout.panel.width - 40.0f, 40.0f},
            font, "Weapon Editor");

    const float listContentWidth = ScrollContentWidth(
            layout.listBounds.width, config);
    const Vector2 listContentSize{
            listContentWidth,
            std::max(layout.listBounds.height,
                    config.listItemHeight
                            * static_cast<float>(state.listLabels.size()))};
    engine::UIScrollAreaResult listScroll = engine::BeginScrollArea(
            ui, config, input, "sector_editor_weapon_list_scroll",
            layout.listBounds, listContentSize, editor.Session().listScroll);
    if (!state.listLabels.empty()) {
        int selectedIndex = state.selectedIndex;
        engine::List(
                ui, config, input, assets,
                "sector_editor_weapon_list",
                Rectangle{0.0f, 0.0f, listScroll.viewport.width,
                        listContentSize.y},
                smallFont, state.listLabels.data(), state.listLabels.size(),
                selectedIndex);
        if (selectedIndex != state.selectedIndex) {
            editor.SelectIndex(selectedIndex);
            ui.focusedId = 0;
            ui.openOptionId = 0;
        }
    }
    engine::EndScrollArea(
            ui, config, input, listScroll, editor.Session().listScroll);

    if (engine::Button(ui, config, input, assets,
                "sector_editor_weapon_add", layout.addButton,
                smallFont, "Add Default")) {
        editor.AddDefault();
    }
    if (engine::Button(ui, config, input, assets,
                "sector_editor_weapon_duplicate", layout.duplicateButton,
                smallFont, "Duplicate")) {
        editor.DuplicateSelected();
    }
    if (engine::Button(ui, config, input, assets,
                "sector_editor_weapon_delete", layout.deleteButton,
                smallFont, "Delete")) {
        editor.RequestDeleteSelected();
    }

    FpsWeaponDefinition* weapon = editor.SelectedWeapon();
    if (weapon == nullptr) {
        engine::Text(
                config, assets,
                Rectangle{layout.formBounds.x + 20.0f,
                        layout.formBounds.y + 20.0f,
                        layout.formBounds.width - 40.0f, 70.0f},
                font, "No weapons are available.", engine::UITextJustify::Left,
                config.mutedTextColor, true);
    } else {
        RefreshAnimationOptions(assets, viewmodel, state);
        const float contentWidth = ScrollContentWidth(
                layout.formBounds.width, config);
        constexpr float ContentHeight = 8600.0f;
        engine::UIScrollAreaResult formScroll = engine::BeginScrollArea(
                ui, config, input, "sector_editor_weapon_form_scroll",
                layout.formBounds, Vector2{contentWidth, ContentHeight},
                editor.Session().formScroll);
        float y = 0.0f;
        size_t floatIndex = 0;
        size_t intIndex = 0;
        const float fieldX = std::min(
                LabelWidth + 14.0f,
                std::max(130.0f, formScroll.viewport.width * 0.46f));
        const float fieldWidth = std::max(
                110.0f, formScroll.viewport.width - fieldX);

        const auto section = [&](const char* title) {
            y += 8.0f;
            engine::Text(
                    ui, config, assets,
                    Rectangle{0.0f, y, formScroll.viewport.width, RowHeight},
                    font, title, engine::UITextJustify::Left,
                    config.textColor);
            y += RowHeight + RowGap;
        };
        const auto label = [&](const char* text) {
            engine::Text(
                    ui, config, assets,
                    Rectangle{0.0f, y, fieldX - 10.0f, RowHeight},
                    smallFont, text, engine::UITextJustify::Left,
                    config.mutedTextColor);
        };
        const auto drawFloat = [&](const char* id, const char* text,
                                   float& value, float minimum, float maximum,
                                   int decimals) {
            label(text);
            engine::UINumericInputResult inputResult = engine::FloatInput(
                    ui, config, input, assets, id,
                    Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                    value, state.floatInputs[floatIndex++], minimum, maximum,
                    decimals);
            if (inputResult.changed) state.validationMessage.clear();
            y += RowHeight + RowGap;
        };
        const auto drawInt = [&](const char* id, const char* text,
                                 int& value, int minimum, int maximum,
                                 int step) {
            label(text);
            engine::UINumericInputResult inputResult = engine::IntInput(
                    ui, config, input, assets, id,
                    Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                    value, state.intInputs[intIndex++], minimum, maximum, step);
            if (inputResult.changed) state.validationMessage.clear();
            y += RowHeight + RowGap;
        };
        const auto drawVector = [&](const char* prefix, const char* text,
                                    Vector3& value, float minimum,
                                    float maximum, int decimals) {
            const std::string xId = std::string(prefix) + "_x";
            const std::string yId = std::string(prefix) + "_y";
            const std::string zId = std::string(prefix) + "_z";
            drawFloat(xId.c_str(), (std::string(text) + " X").c_str(),
                    value.x, minimum, maximum, decimals);
            drawFloat(yId.c_str(), (std::string(text) + " Y").c_str(),
                    value.y, minimum, maximum, decimals);
            drawFloat(zId.c_str(), (std::string(text) + " Z").c_str(),
                    value.z, minimum, maximum, decimals);
        };
        const auto drawColor = [&](const char* prefix, const char* text,
                                   Color& value) {
            int channels[] = {value.r, value.g, value.b, value.a};
            const char* suffixes[] = {"r", "g", "b", "a"};
            const char* names[] = {" R", " G", " B", " A"};
            for (int channel = 0; channel < 4; ++channel) {
                const std::string id = std::string(prefix) + "_" + suffixes[channel];
                const std::string channelLabel = std::string(text) + names[channel];
                drawInt(id.c_str(), channelLabel.c_str(), channels[channel], 0, 255, 1);
            }
            value = Color{
                    static_cast<unsigned char>(channels[0]),
                    static_cast<unsigned char>(channels[1]),
                    static_cast<unsigned char>(channels[2]),
                    static_cast<unsigned char>(channels[3])};
        };
        const auto drawCheckbox = [&](const char* id, const char* text,
                                      bool& value) {
            if (engine::Checkbox(
                        ui, config, input, assets, id,
                        Rectangle{fieldX, y, fieldWidth, RowHeight},
                        smallFont, text, value)) {
                state.validationMessage.clear();
            }
            y += RowHeight + RowGap;
        };

        section("Identity");
        label("ID");
        const engine::UITextInputResult idResult = engine::TextInput(
                ui, config, input, assets, "sector_editor_weapon_id",
                Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                state.idBuffer, sizeof(state.idBuffer), 1,
                sizeof(state.idBuffer) - 1);
        if (idResult.submitted) editor.ApplyIdBuffer();
        y += RowHeight + RowGap;
        label("Weapon slot");
        const char* const slotOptions[] = {
                "Unassigned", "1", "2", "3", "4", "5", "6"};
        int weaponSlot = weapon->weaponSlot;
        if (engine::Option(
                    ui, config, input, assets,
                    "sector_editor_weapon_slot",
                    Rectangle{fieldX, y, fieldWidth, RowHeight},
                    smallFont,
                    slotOptions,
                    std::size(slotOptions),
                    weaponSlot)) {
            editor.SetSelectedWeaponSlot(weaponSlot);
        }
        y += RowHeight + RowGap;

        section("Arms model and animation");
        label("Arms model");
        const float pickWidth = 92.0f;
        const engine::UITextInputResult armsPathResult = engine::TextInput(
                ui, config, input, assets, "sector_editor_weapon_arms_model",
                Rectangle{fieldX, y, std::max(80.0f, fieldWidth - pickWidth - 6.0f), RowHeight},
                smallFont, state.armsModelPathBuffer,
                sizeof(state.armsModelPathBuffer), 0,
                sizeof(state.armsModelPathBuffer) - 1);
        if (armsPathResult.submitted) editor.ApplyArmsModelPathBuffer();
        if (engine::Button(ui, config, input, assets,
                    "sector_editor_weapon_pick_arms",
                    Rectangle{fieldX + std::max(80.0f, fieldWidth - pickWidth - 6.0f) + 6.0f,
                            y, pickWidth, RowHeight},
                    smallFont, "Pick")) {
            modelPicker.Open(
                    weapon->viewmodel.modelPath,
                    ModelPickerTarget::WeaponArms);
        }
        y += RowHeight + RowGap;
        label("Idle animation");
        const engine::UITextInputResult animationResult = engine::TextInput(
                ui, config, input, assets, "sector_editor_weapon_idle_animation",
                Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                state.idleAnimationBuffer, sizeof(state.idleAnimationBuffer),
                0, sizeof(state.idleAnimationBuffer) - 1);
        if (animationResult.submitted) editor.ApplyIdleAnimationBuffer();
        y += RowHeight + RowGap;
        if (!state.animationOptions.empty()) {
            int animationIndex = -1;
            for (size_t index = 0; index < state.animationOptionStorage.size(); ++index) {
                if (state.animationOptionStorage[index]
                        == weapon->viewmodel.idleAnimation) {
                    animationIndex = static_cast<int>(index);
                    break;
                }
            }
            label("Loaded animations");
            if (engine::Option(
                        ui, config, input, assets,
                        "sector_editor_weapon_animation_options",
                        Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                        state.animationOptions.data(), state.animationOptions.size(),
                        animationIndex)
                    && animationIndex >= 0) {
                std::strncpy(
                        state.idleAnimationBuffer,
                        state.animationOptionStorage[static_cast<size_t>(animationIndex)].c_str(),
                        sizeof(state.idleAnimationBuffer) - 1);
                state.idleAnimationBuffer[sizeof(state.idleAnimationBuffer) - 1] = '\0';
                editor.ApplyIdleAnimationBuffer();
            }
            y += RowHeight + RowGap;
        }
        drawFloat("sector_editor_weapon_source_fps", "Source FPS",
                weapon->viewmodel.sourceFps, 0.01f, 1000.0f, 2);
        drawInt("sector_editor_weapon_first_frame", "First frame",
                weapon->viewmodel.firstFrame, 0, 1000000, 1);
        drawInt("sector_editor_weapon_last_frame", "Last frame",
                weapon->viewmodel.lastFrame, 1, 1000000, 1);
        drawFloat("sector_editor_weapon_playback", "Playback speed",
                weapon->viewmodel.playbackSpeed, 0.01f, 100.0f, 3);

        section("Arms presentation");
        drawVector("sector_editor_weapon_position", "Position",
                weapon->viewmodel.presentation.position, -10.0f, 10.0f, 4);
        drawVector("sector_editor_weapon_rotation", "Rotation",
                weapon->viewmodel.presentation.rotationDegrees,
                -360.0f, 360.0f, 3);
        drawFloat("sector_editor_weapon_scale", "Scale",
                weapon->viewmodel.presentation.scale, 0.001f, 100.0f, 4);
        drawFloat("sector_editor_weapon_fov", "Vertical FOV",
                weapon->viewmodel.presentation.verticalFovDegrees,
                1.01f, 178.99f, 2);
        drawFloat("sector_editor_weapon_arms_brightness", "Brightness",
                weapon->viewmodel.brightnessAdjustment, -1.0f, 1.0f, 3);
        drawCheckbox("sector_editor_weapon_arms_material_enabled",
                "Material override", weapon->viewmodel.materialOverride.enabled);
        drawFloat("sector_editor_weapon_arms_metallic", "Metallic factor",
                weapon->viewmodel.materialOverride.metallicFactor,
                0.0f, 1.0f, 3);
        drawFloat("sector_editor_weapon_arms_roughness", "Roughness factor",
                weapon->viewmodel.materialOverride.roughnessFactor,
                0.045f, 1.0f, 3);
        drawCheckbox("sector_editor_weapon_arms_packed_material",
                "Use packed material texture",
                weapon->viewmodel.materialOverride.useMetallicRoughnessTexture);

        section("Holster transition");
        drawFloat("sector_editor_weapon_holster_duration", "Holster seconds",
                weapon->viewmodel.holsterTransition.holsterDurationSeconds,
                0.001f, 60.0f, 3);
        drawFloat("sector_editor_weapon_unholster_duration", "Unholster seconds",
                weapon->viewmodel.holsterTransition.unholsterDurationSeconds,
                0.001f, 60.0f, 3);
        drawVector("sector_editor_weapon_hidden_translation", "Hidden translation",
                weapon->viewmodel.holsterTransition.hiddenTranslation,
                -10.0f, 10.0f, 4);
        drawVector("sector_editor_weapon_hidden_rotation", "Hidden rotation",
                weapon->viewmodel.holsterTransition.hiddenRotationDegrees,
                -360.0f, 360.0f, 3);

        section("Attached weapon model");
        label("Weapon model");
        const engine::UITextInputResult attachmentPathResult = engine::TextInput(
                ui, config, input, assets,
                "sector_editor_weapon_attachment_model",
                Rectangle{fieldX, y, std::max(80.0f, fieldWidth - pickWidth - 6.0f), RowHeight},
                smallFont, state.attachmentModelPathBuffer,
                sizeof(state.attachmentModelPathBuffer), 0,
                sizeof(state.attachmentModelPathBuffer) - 1);
        if (attachmentPathResult.submitted) {
            editor.ApplyAttachmentModelPathBuffer();
        }
        if (engine::Button(ui, config, input, assets,
                    "sector_editor_weapon_pick_attachment",
                    Rectangle{fieldX + std::max(80.0f, fieldWidth - pickWidth - 6.0f) + 6.0f,
                            y, pickWidth, RowHeight},
                    smallFont, "Pick")) {
            modelPicker.Open(
                    weapon->viewmodel.attachment.modelPath,
                    ModelPickerTarget::WeaponAttachment);
        }
        y += RowHeight + RowGap;
        label("Attachment bone");
        const engine::UITextInputResult boneResult = engine::TextInput(
                ui, config, input, assets,
                "sector_editor_weapon_attachment_bone",
                Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                state.attachmentBoneBuffer, sizeof(state.attachmentBoneBuffer),
                0, sizeof(state.attachmentBoneBuffer) - 1);
        if (boneResult.submitted) editor.ApplyAttachmentBoneBuffer();
        y += RowHeight + RowGap;
        drawVector("sector_editor_weapon_grip_translation", "Grip translation",
                weapon->viewmodel.attachment.gripCorrection.translation,
                -10.0f, 10.0f, 4);
        drawVector("sector_editor_weapon_grip_rotation", "Grip rotation",
                weapon->viewmodel.attachment.gripCorrection.rotationDegrees,
                -360.0f, 360.0f, 3);
        drawFloat("sector_editor_weapon_grip_scale", "Grip scale",
                weapon->viewmodel.attachment.gripCorrection.scale,
                0.001f, 100.0f, 4);
        drawFloat("sector_editor_weapon_attachment_brightness", "Weapon brightness",
                weapon->viewmodel.attachment.lighting.brightnessAdjustment,
                -1.0f, 1.0f, 3);
        drawFloat("sector_editor_weapon_attachment_metallic", "Weapon metallic",
                weapon->viewmodel.attachment.lighting.materialOverride.metallicFactor,
                0.0f, 1.0f, 3);
        drawFloat("sector_editor_weapon_attachment_roughness", "Weapon roughness",
                weapon->viewmodel.attachment.lighting.materialOverride.roughnessFactor,
                0.045f, 1.0f, 3);
        drawCheckbox("sector_editor_weapon_attachment_packed_material",
                "Use packed material texture",
                weapon->viewmodel.attachment.lighting.materialOverride
                        .useMetallicRoughnessTexture);

        section("Crosshair");
        drawCheckbox("sector_editor_weapon_crosshair_enabled", "Enabled",
                weapon->crosshair.enabled);
        drawFloat("sector_editor_weapon_crosshair_gap", "Center gap (pixels)",
                weapon->crosshair.centerGapPixels, 0.001f, 1000.0f, 2);
        drawFloat("sector_editor_weapon_crosshair_length", "Segment length",
                weapon->crosshair.segmentLengthPixels, 0.001f, 1000.0f, 2);
        drawFloat("sector_editor_weapon_crosshair_inner_width", "Inner thickness",
                weapon->crosshair.innerThicknessPixels, 0.001f, 1000.0f, 2);
        drawFloat("sector_editor_weapon_crosshair_outline_width", "Outline thickness",
                weapon->crosshair.outlineThicknessPixels, 0.001f, 1000.0f, 2);
        drawColor("sector_editor_weapon_crosshair_inner_color", "Inner color",
                weapon->crosshair.innerColor);
        drawColor("sector_editor_weapon_crosshair_outline_color", "Outline color",
                weapon->crosshair.outlineColor);

        section("Firing");
        drawFloat("sector_editor_weapon_shot_interval", "Shot interval seconds",
                weapon->firing.shotIntervalSeconds, 0.001f, 60.0f, 3);
        drawFloat("sector_editor_weapon_max_range", "Maximum range",
                weapon->firing.maximumRangeWorld, 0.001f, 1000000.0f, 2);
        drawFloat("sector_editor_weapon_noise_radius", "Noise radius",
                weapon->firing.noiseRadiusWorld, 0.0f, 10000.0f, 2);
        drawCheckbox("sector_editor_weapon_pellets_enabled", "Pellets enabled",
                weapon->firing.pellets.enabled);
        drawInt("sector_editor_weapon_pellet_count", "Pellet count",
                weapon->firing.pellets.count, 1, MaxFpsWeaponPellets, 1);
        drawFloat(
                "sector_editor_weapon_pellet_spread",
                "Spread half-angle (degrees)",
                weapon->firing.pellets.spreadHalfAngleDegrees,
                0.0f,
                45.0f,
                2);
        label("Shoot sound");
        const engine::UITextInputResult soundResult = engine::TextInput(
                ui, config, input, assets, "sector_editor_weapon_shoot_sound",
                Rectangle{fieldX, y, std::max(80.0f, fieldWidth - pickWidth - 6.0f), RowHeight},
                smallFont, state.shootSoundBuffer, sizeof(state.shootSoundBuffer),
                0, sizeof(state.shootSoundBuffer) - 1);
        if (soundResult.submitted) editor.ApplyShootSoundBuffer(assets);
        if (engine::Button(ui, config, input, assets,
                    "sector_editor_weapon_pick_sound",
                    Rectangle{fieldX + std::max(80.0f, fieldWidth - pickWidth - 6.0f) + 6.0f,
                    y, pickWidth, RowHeight}, smallFont, "Pick")) {
            state.audioPickerTarget = SectorEditorWeaponAudioTarget::Shoot;
            audioPicker.Open(
                    state.audioPicker,
                    "Choose Weapon Fire Sound",
                    weapon->firing.shootSoundPath,
                    SectorSoundType::Sound);
        }
        y += RowHeight + RowGap;

        section("Reload");
        drawInt("sector_editor_weapon_magazine_size", "Magazine size",
                weapon->reload.magazineSize, 1, 1000000, 1);
        drawFloat("sector_editor_weapon_reload_duration", "Hidden reload seconds",
                weapon->reload.durationSeconds, 0.001f, 60.0f, 3);
        label("Dry-fire sound");
        const engine::UITextInputResult drySoundResult = engine::TextInput(
                ui, config, input, assets, "sector_editor_weapon_dry_sound",
                Rectangle{fieldX, y, std::max(80.0f, fieldWidth - pickWidth - 6.0f), RowHeight},
                smallFont, state.dryFireSoundBuffer,
                sizeof(state.dryFireSoundBuffer),
                0, sizeof(state.dryFireSoundBuffer) - 1);
        if (drySoundResult.submitted) editor.ApplyDryFireSoundBuffer(assets);
        if (engine::Button(ui, config, input, assets,
                    "sector_editor_weapon_pick_dry_sound",
                    Rectangle{fieldX + std::max(80.0f, fieldWidth - pickWidth - 6.0f) + 6.0f,
                            y, pickWidth, RowHeight}, smallFont, "Pick")) {
            state.audioPickerTarget = SectorEditorWeaponAudioTarget::DryFire;
            audioPicker.Open(
                    state.audioPicker,
                    "Choose Weapon Dry-fire Sound",
                    weapon->reload.dryFireSoundPath,
                    SectorSoundType::Sound);
        }
        y += RowHeight + RowGap;
        label("Reload sound");
        const engine::UITextInputResult reloadSoundResult = engine::TextInput(
                ui, config, input, assets, "sector_editor_weapon_reload_sound",
                Rectangle{fieldX, y, std::max(80.0f, fieldWidth - pickWidth - 6.0f), RowHeight},
                smallFont, state.reloadSoundBuffer,
                sizeof(state.reloadSoundBuffer),
                0, sizeof(state.reloadSoundBuffer) - 1);
        if (reloadSoundResult.submitted) editor.ApplyReloadSoundBuffer(assets);
        if (engine::Button(ui, config, input, assets,
                    "sector_editor_weapon_pick_reload_sound",
                    Rectangle{fieldX + std::max(80.0f, fieldWidth - pickWidth - 6.0f) + 6.0f,
                            y, pickWidth, RowHeight}, smallFont, "Pick")) {
            state.audioPickerTarget = SectorEditorWeaponAudioTarget::Reload;
            audioPicker.Open(
                    state.audioPicker,
                    "Choose Weapon Reload Sound",
                    weapon->reload.reloadSoundPath,
                    SectorSoundType::Sound);
        }
        y += RowHeight + RowGap;

        section("Viewmodel recoil");
        drawVector("sector_editor_weapon_recoil_translation", "Translation impulse",
                weapon->firing.recoil.translationImpulse, -10.0f, 10.0f, 4);
        drawVector("sector_editor_weapon_recoil_rotation", "Rotation impulse",
                weapon->firing.recoil.rotationImpulseDegrees,
                -360.0f, 360.0f, 3);
        drawFloat("sector_editor_weapon_recoil_roll_variation", "Roll variation",
                weapon->firing.recoil.rollVariationDegrees, 0.0f, 360.0f, 3);
        drawFloat("sector_editor_weapon_recoil_frequency", "Spring frequency Hz",
                weapon->firing.recoil.springFrequencyHz, 0.001f, 1000.0f, 3);
        drawFloat("sector_editor_weapon_recoil_damping", "Damping ratio",
                weapon->firing.recoil.dampingRatio, 0.001f, 100.0f, 3);
        drawVector("sector_editor_weapon_recoil_max_translation", "Maximum translation",
                weapon->firing.recoil.maximumTranslation, 0.0f, 100.0f, 4);
        drawVector("sector_editor_weapon_recoil_max_rotation", "Maximum rotation",
                weapon->firing.recoil.maximumRotationDegrees, 0.0f, 360.0f, 3);

        section("Camera recoil");
        drawCheckbox("sector_editor_weapon_camera_recoil_enabled", "Enabled",
                weapon->firing.cameraRecoil.enabled);
        drawFloat("sector_editor_weapon_camera_pitch", "Pitch kick",
                weapon->firing.cameraRecoil.pitchKickDegrees, 0.0f, 360.0f, 3);
        drawFloat("sector_editor_weapon_camera_pitch_variation", "Pitch variation",
                weapon->firing.cameraRecoil.pitchVariationDegrees, 0.0f, 360.0f, 3);
        drawFloat("sector_editor_weapon_camera_yaw_variation", "Yaw variation",
                weapon->firing.cameraRecoil.yawVariationDegrees, 0.0f, 360.0f, 3);
        drawFloat("sector_editor_weapon_camera_roll_variation", "Roll variation",
                weapon->firing.cameraRecoil.rollVariationDegrees, 0.0f, 360.0f, 3);
        drawFloat("sector_editor_weapon_camera_frequency", "Spring frequency Hz",
                weapon->firing.cameraRecoil.springFrequencyHz, 0.001f, 1000.0f, 3);
        drawFloat("sector_editor_weapon_camera_damping", "Damping ratio",
                weapon->firing.cameraRecoil.springDampingRatio, 0.001f, 100.0f, 3);
        drawFloat("sector_editor_weapon_camera_max_pitch", "Maximum pitch",
                weapon->firing.cameraRecoil.maxPitchDegrees, 0.0f, 360.0f, 3);
        drawFloat("sector_editor_weapon_camera_max_yaw", "Maximum yaw",
                weapon->firing.cameraRecoil.maxYawDegrees, 0.0f, 360.0f, 3);
        drawFloat("sector_editor_weapon_camera_max_roll", "Maximum roll",
                weapon->firing.cameraRecoil.maxRollDegrees, 0.0f, 360.0f, 3);

        section("Muzzle socket");
        drawVector("sector_editor_weapon_muzzle_position", "Position",
                weapon->firing.muzzleSocket.position, -100.0f, 100.0f, 4);
        drawVector("sector_editor_weapon_muzzle_rotation", "Rotation",
                weapon->firing.muzzleSocket.rotationDegrees,
                -360.0f, 360.0f, 3);

        section("Muzzle flash");
        drawCheckbox("sector_editor_weapon_flash_enabled", "Enabled",
                weapon->firing.muzzleFlash.enabled);
        drawFloat("sector_editor_weapon_flash_lifetime", "Lifetime seconds",
                weapon->firing.muzzleFlash.lifetimeSeconds, 0.001f, 60.0f, 3);
        drawFloat("sector_editor_weapon_flash_size", "Size world",
                weapon->firing.muzzleFlash.sizeWorld, 0.001f, 100.0f, 3);
        drawFloat("sector_editor_weapon_flash_variation", "Size variation",
                weapon->firing.muzzleFlash.sizeVariation, 0.0f, 0.5f, 3);
        drawFloat("sector_editor_weapon_flash_irregularity", "Irregularity",
                weapon->firing.muzzleFlash.irregularity, 0.0f, 1.0f, 3);
        drawFloat("sector_editor_weapon_flash_stretch", "Forward stretch",
                weapon->firing.muzzleFlash.forwardStretch, 1.0f, 4.0f, 3);
        drawInt("sector_editor_weapon_flash_min_lobes", "Minimum lobes",
                weapon->firing.muzzleFlash.minimumLobeCount,
                3, MaxFpsMuzzleFlashLobes, 1);
        drawInt("sector_editor_weapon_flash_max_lobes", "Maximum lobes",
                weapon->firing.muzzleFlash.maximumLobeCount,
                3, MaxFpsMuzzleFlashLobes, 1);
        drawFloat("sector_editor_weapon_flash_rear", "Rear suppression",
                weapon->firing.muzzleFlash.rearSuppression, 0.0f, 1.0f, 3);
        drawFloat("sector_editor_weapon_flash_softness", "Edge softness",
                weapon->firing.muzzleFlash.edgeSoftness, 0.01f, 1.0f, 3);
        drawFloat("sector_editor_weapon_flash_radiance", "Radiance strength",
                weapon->firing.muzzleFlash.radianceStrength, 0.0f, 64.0f, 3);
        drawColor("sector_editor_weapon_flash_core", "Core color",
                weapon->firing.muzzleFlash.coreColor);
        drawColor("sector_editor_weapon_flash_hot", "Hot color",
                weapon->firing.muzzleFlash.hotColor);
        drawColor("sector_editor_weapon_flash_warm", "Warm color",
                weapon->firing.muzzleFlash.warmColor);
        drawColor("sector_editor_weapon_flash_edge", "Edge color",
                weapon->firing.muzzleFlash.edgeColor);

        section("Muzzle light");
        drawCheckbox("sector_editor_weapon_light_enabled", "Enabled",
                weapon->firing.muzzleLight.enabled);
        drawColor("sector_editor_weapon_light_color", "Color",
                weapon->firing.muzzleLight.color);
        drawFloat("sector_editor_weapon_light_intensity", "Intensity",
                weapon->firing.muzzleLight.intensity, 0.0f, 100000.0f, 3);
        drawFloat("sector_editor_weapon_light_radius", "Radius world",
                weapon->firing.muzzleLight.radiusWorld, 0.001f, 10000.0f, 3);
        drawFloat("sector_editor_weapon_light_lifetime", "Lifetime seconds",
                weapon->firing.muzzleLight.lifetimeSeconds, 0.001f, 60.0f, 3);
        drawFloat("sector_editor_weapon_light_decay", "Decay exponent",
                weapon->firing.muzzleLight.decayExponent, 0.001f, 100.0f, 3);

        section("Impact");
        drawInt("sector_editor_weapon_damage", "Damage",
                weapon->firing.impact.damage, 0, 1000000, 1);
        drawFloat("sector_editor_weapon_stagger", "Stagger seconds",
                weapon->firing.impact.staggerSeconds, 0.0f, 10.0f, 3);
        drawFloat("sector_editor_weapon_knockback", "Knockback impulse",
                weapon->firing.impact.knockbackImpulseWorldPerSecond,
                0.0f, 100.0f, 3);
        drawCheckbox("sector_editor_weapon_blood_enabled", "Blood enabled",
                weapon->firing.impact.blood.enabled);
        drawInt("sector_editor_weapon_blood_count", "Blood particle count",
                weapon->firing.impact.blood.particleCount, 0, 256, 1);
        drawFloat("sector_editor_weapon_blood_size", "Blood size scale",
                weapon->firing.impact.blood.sizeScale, 0.05f, 10.0f, 3);
        drawFloat("sector_editor_weapon_blood_intensity", "Blood intensity",
                weapon->firing.impact.blood.intensity, 0.0f, 10.0f, 3);
        drawCheckbox("sector_editor_weapon_debris_enabled", "Debris enabled",
                weapon->firing.impact.surfaceDebris.enabled);
        drawInt("sector_editor_weapon_debris_count", "Debris particle count",
                weapon->firing.impact.surfaceDebris.particleCount, 0, 256, 1);
        drawFloat("sector_editor_weapon_debris_size", "Debris size scale",
                weapon->firing.impact.surfaceDebris.sizeScale, 0.05f, 10.0f, 3);
        drawFloat("sector_editor_weapon_debris_intensity", "Debris intensity",
                weapon->firing.impact.surfaceDebris.intensity, 0.0f, 10.0f, 3);

        engine::EndScrollArea(
                ui, config, input, formScroll, editor.Session().formScroll);
    }

    if (state.openedFromPreview3D && weapon != nullptr) {
        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_weapon_preview_fire",
                    layout.previewFireButton,
                    smallFont, "Preview Fire")) {
            result.previewFireRequested = true;
        }
        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_weapon_preview_reload",
                    layout.previewReloadButton,
                    smallFont, "Preview Reload")) {
            result.previewReloadRequested = true;
        }
        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_weapon_preview_holster",
                    layout.holsterToggleButton,
                    smallFont, "Holster / Unholster")) {
            result.holsterToggleRequested = true;
        }
    }

    if (!state.validationMessage.empty()) {
        engine::Text(
                config, assets,
                layout.validationMessage,
                smallFont, state.validationMessage.c_str(),
                engine::UITextJustify::Left, config.invalidColor, true);
    } else if (!state.warningMessage.empty()) {
        engine::Text(
                config, assets,
                layout.validationMessage,
                smallFont, state.warningMessage.c_str(),
                engine::UITextJustify::Left, config.invalidColor, true);
    }

    bool saveRequested = engine::Button(
            ui, config, input, assets,
            "sector_editor_weapon_save", layout.saveButton, font, "Save");
    cancelRequested = cancelRequested || engine::Button(
            ui, config, input, assets,
            "sector_editor_weapon_cancel", layout.cancelButton, font, "Cancel");

    if (state.deleteConfirmationOpen) {
        const Rectangle popup{
                layout.panel.x + (layout.panel.width - 520.0f) * 0.5f,
                layout.panel.y + (layout.panel.height - 230.0f) * 0.5f,
                520.0f,
                230.0f};
        DrawRectangleRec(popup, Color{27, 32, 42, 255});
        DrawRectangleLinesEx(popup, config.borderThickness, config.borderColor);
        const std::string message = "Delete weapon '"
                + state.deleteConfirmationId
                + "'? Model and audio assets will not be deleted.";
        engine::Text(
                config, assets,
                Rectangle{popup.x + 20.0f, popup.y + 20.0f,
                        popup.width - 40.0f, 90.0f},
                font, message.c_str(), engine::UITextJustify::Left,
                config.textColor, true);
        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_weapon_delete_confirm",
                    Rectangle{popup.x + popup.width - 300.0f,
                            popup.y + 160.0f, 130.0f, 42.0f},
                    font, "Delete")) {
            editor.ConfirmDeleteSelected();
        }
        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_weapon_delete_cancel",
                    Rectangle{popup.x + popup.width - 150.0f,
                            popup.y + 160.0f, 130.0f, 42.0f},
                    font, "Cancel")) {
            editor.CancelDelete();
        }
        saveRequested = false;
        cancelRequested = false;
    }

    ConsumeRemainingInput(input);
    if (cancelRequested) {
        editor.Cancel();
        result.cancelled = true;
    } else if (saveRequested && editor.SaveAndClose(assets)) {
        result.saved = true;
    }
    return result;
}

} // namespace game
