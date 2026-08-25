#include "sector_editor/tools/placed_objects/SectorEditorDynamicModelInspector.h"

#include "engine/components/AnimatedModel.h"
#include "sector_editor/SectorEditorUiHelpers.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace game {
namespace {

const engine::AnimatedModelInstance* RuntimeModelInstance(
        const SectorPlacedRuntimeObject& object,
        const SectorRuntimeObjectState& runtimeObjects,
        engine::EngineContext* engineContext)
{
    if (engineContext == nullptr) return nullptr;
    for (const SectorPlacedRuntimeObjectEntity& entry : runtimeObjects.placedObjectEntities) {
        if (entry.placedObjectId == object.id
                && engineContext->world.IsAlive(entry.entity)
                && engineContext->world.Has<engine::AnimatedModelInstance>(entry.entity)) {
            return &engineContext->world.Get<engine::AnimatedModelInstance>(entry.entity);
        }
    }
    return nullptr;
}

} // namespace

float MeasureSectorEditorDynamicModelInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject&)
{
    return 38.0f * 2.0f
            + 48.0f * 19.0f
            + 8.0f * 16.0f
            + 70.0f
            + context.rowH
            + context.gap
            + 40.0f;
}

void DrawSectorEditorDynamicModelInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y)
{
    const SectorPlacedRuntimeObject* object = context.editing.SelectedObject();
    if (object == nullptr || object->kind != "dynamic_model") return;
    const float contentW = context.contentW;
    const float rowH = context.rowH;
    const float gap = context.gap;
    const engine::UIConfig smallConfig = SectorEditorSmallFontConfig(
            context.config, context.assets, context.smallFont);

    const std::string_view filename = SectorEditorModelFilename(
            object->dynamicModel.modelPath);
    std::string modelLabel = "Model file: ";
    modelLabel.append(filename.empty() ? "<none>" : filename);
    engine::Text(
            context.ui, smallConfig, context.assets,
            Rectangle{0.0f, y, contentW, 38.0f}, context.smallFont,
            modelLabel.c_str(), engine::UITextJustify::Left,
            object->dynamicModel.modelPath.empty()
                    ? context.config.invalidColor
                    : context.config.textColor,
            false);
    y += 42.0f;

    const engine::AnimatedModelInstance* runtimeInstance = RuntimeModelInstance(
            *object, context.runtimeObjects, context.engineContext);
    const engine::ModelAsset* modelAsset = runtimeInstance == nullptr
            ? nullptr
            : context.assets.GetModelAsset(runtimeInstance->model);
    const char* modelStatus = object->dynamicModel.modelPath.empty()
            ? "Model: None selected"
            : (modelAsset != nullptr
                    ? "Model: ready"
                    : (runtimeInstance != nullptr
                            && context.assets.HasFailed(runtimeInstance->model)
                            ? "Model: failed to load"
                            : "Model: loading or unavailable"));
    engine::Text(
            context.ui, smallConfig, context.assets,
            Rectangle{0.0f, y, contentW, 30.0f}, context.smallFont,
            modelStatus, engine::UITextJustify::Left,
            modelAsset != nullptr ? context.config.accentColor : context.config.mutedTextColor);
    y += 34.0f + gap;

    if (context.uiState.dynamicModelInstanceIdObjectId != object->id) {
        std::snprintf(
                context.uiState.dynamicModelInstanceIdBuffer,
                sizeof(context.uiState.dynamicModelInstanceIdBuffer),
                "%s",
                object->dynamicModel.instanceId.c_str());
        context.uiState.dynamicModelInstanceIdObjectId = object->id;
        context.uiState.dynamicModelInstanceIdError.clear();
    }
    engine::Text(
            context.ui, context.config, context.assets,
            Rectangle{0.0f, y, 118.0f, rowH}, context.font,
            "Instance ID", engine::UITextJustify::Left,
            context.config.mutedTextColor);
    const engine::UITextInputResult instanceResult = engine::TextInput(
            context.ui, context.config, context.input, context.assets,
            "sector_editor_dynamic_model_instance_id",
            Rectangle{122.0f, y, std::max(0.0f, contentW - 122.0f), rowH},
            context.font,
            context.uiState.dynamicModelInstanceIdBuffer,
            sizeof(context.uiState.dynamicModelInstanceIdBuffer),
            1,
            sizeof(context.uiState.dynamicModelInstanceIdBuffer) - 1,
            engine::UITextJustify::Left);
    if (instanceResult.submitted) {
        context.editing.SetSelectedDynamicModelInstanceId(
                std::string{context.uiState.dynamicModelInstanceIdBuffer},
                context.uiState.dynamicModelInstanceIdError);
    }
    y += rowH + gap;
    if (!context.uiState.dynamicModelInstanceIdError.empty()) {
        engine::Text(
                context.ui, context.config, context.assets,
                Rectangle{0.0f, y, contentW, 36.0f}, context.smallFont,
                context.uiState.dynamicModelInstanceIdError.c_str(),
                engine::UITextJustify::Left,
                context.config.invalidColor,
                true);
        y += 40.0f;
    }

    if (engine::Button(
                context.ui, context.config, context.input, context.assets,
                "sector_editor_dynamic_model_choose",
                Rectangle{0.0f, y, contentW, rowH}, context.font,
                "Choose Model")) {
        context.staticModelPicker.Open(
                object->dynamicModel.modelPath,
                ModelPickerTarget::DynamicModel);
    }
    y += rowH + gap;

    const auto drawFloat = [&] (
            const char* id,
            const char* label,
            float value,
            engine::UIFloatInputState& inputState,
            const std::function<bool(SectorPlacedRuntimeObject&, float)>& apply,
            float minimum = -100000.0f,
            float maximum = 100000.0f) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(
                        y,
                        contentW,
                        rowH,
                        gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                context.ui, context.config, context.input, context.assets,
                context.font, id, label,
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Left, value, inputState,
                minimum, maximum, 3);
        if (result.changed && result.finite && result.value != value) {
            context.editing.MutateSelected(
                    "Updated dynamic prop",
                    [apply, newValue = result.value](SectorPlacedRuntimeObject& target) {
                        return apply(target, newValue);
                    });
        }
        y += rowH + gap;
    };

    drawFloat("sector_editor_dynamic_x", "Position X", object->position.x,
            context.uiState.xInput, [](auto& target, float value) {
                if (target.kind != "dynamic_model" || target.position.x == value) return false;
                target.position.x = value; return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawFloat("sector_editor_dynamic_z", "Position Z", object->position.z,
            context.uiState.zInput, [](auto& target, float value) {
                if (target.kind != "dynamic_model" || target.position.z == value) return false;
                target.position.z = value; return true;
            });
    constexpr float radiansToDegrees = 180.0f / PI;
    constexpr float degreesToRadians = PI / 180.0f;
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawFloat("sector_editor_dynamic_rx", "Rotation X",
            object->dynamicModel.rotationXRadians * radiansToDegrees,
            context.uiState.rotationXInput, [degreesToRadians](auto& target, float value) {
                const float radians = value * degreesToRadians;
                if (target.kind != "dynamic_model" || target.dynamicModel.rotationXRadians == radians) return false;
                target.dynamicModel.rotationXRadians = radians; return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawFloat("sector_editor_dynamic_yaw", "Yaw (Y)",
            object->yawRadians * radiansToDegrees, context.uiState.yawInput,
            [degreesToRadians](auto& target, float value) {
                const float radians = value * degreesToRadians;
                if (target.kind != "dynamic_model" || target.yawRadians == radians) return false;
                target.yawRadians = radians; return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawFloat("sector_editor_dynamic_rz", "Rotation Z",
            object->dynamicModel.rotationZRadians * radiansToDegrees,
            context.uiState.rotationZInput, [degreesToRadians](auto& target, float value) {
                const float radians = value * degreesToRadians;
                if (target.kind != "dynamic_model" || target.dynamicModel.rotationZRadians == radians) return false;
                target.dynamicModel.rotationZRadians = radians; return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawFloat("sector_editor_dynamic_height", "Height Offset",
            object->dynamicModel.heightOffsetWorld, context.uiState.heightOffsetInput,
            [](auto& target, float value) {
                if (target.kind != "dynamic_model" || target.dynamicModel.heightOffsetWorld == value) return false;
                target.dynamicModel.heightOffsetWorld = value; return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawFloat("sector_editor_dynamic_scale", "Scale", object->dynamicModel.scale,
            context.uiState.scaleInput, [](auto& target, float value) {
                if (target.kind != "dynamic_model" || target.dynamicModel.scale == value) return false;
                target.dynamicModel.scale = value; return true;
            }, 0.001f, 100000.0f);
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawFloat("sector_editor_dynamic_speed", "Anim Speed", object->dynamicModel.animationSpeed,
            context.uiState.animationSpeedInput, [](auto& target, float value) {
                if (target.kind != "dynamic_model" || target.dynamicModel.animationSpeed == value) return false;
                target.dynamicModel.animationSpeed = value; return true;
            }, 0.01f, 10.0f);

    object = context.editing.SelectedObject(); if (object == nullptr) return;
    runtimeInstance = RuntimeModelInstance(*object, context.runtimeObjects, context.engineContext);
    modelAsset = runtimeInstance == nullptr
            ? nullptr
            : context.assets.GetModelAsset(runtimeInstance->model);
    const size_t clipCount = modelAsset != nullptr
            ? engine::ModelAnimationClipCount(*modelAsset)
            : 0;
    if (modelAsset != nullptr && clipCount > 0) {
        std::vector<std::string> names;
        names.reserve(clipCount);
        int selected = 0;
        bool authoredAnimationFound = object->dynamicModel.animation.empty();
        for (size_t i = 0; i < clipCount; ++i) {
            const char* name =
                    engine::ModelAnimationClipName(*modelAsset, i);
            names.emplace_back(name != nullptr ? name : "");
            if (names.back() == object->dynamicModel.animation) {
                selected = i;
                authoredAnimationFound = true;
            }
        }
        if (!authoredAnimationFound) {
            engine::Text(
                    context.ui, smallConfig, context.assets,
                    Rectangle{0.0f, y + rowH, contentW, 24.0f}, context.smallFont,
                    "Saved animation missing; preview uses the first clip",
                    engine::UITextJustify::Left,
                    context.config.invalidColor);
            y += 28.0f;
        }
        const int previous = selected;
        if (engine::Option(
                    context.ui, context.config, context.input, context.assets,
                    "sector_editor_dynamic_animation", Rectangle{0.0f, y, contentW, rowH},
                    context.font, names, selected)
                && selected != previous && selected >= 0
                && selected < static_cast<int>(names.size())) {
            const std::string name = names[static_cast<size_t>(selected)];
            context.editing.MutateSelected("Updated dynamic prop animation", [name](auto& target) {
                if (target.kind != "dynamic_model" || target.dynamicModel.animation == name) return false;
                target.dynamicModel.animation = name; return true;
            });
        }
    } else {
        engine::Text(context.ui, smallConfig, context.assets,
                Rectangle{0.0f, y, contentW, rowH}, context.smallFont,
                "Animations: none available", engine::UITextJustify::Left,
                context.config.mutedTextColor);
    }
    y += rowH + gap;

    object = context.editing.SelectedObject(); if (object == nullptr) return;
    static const std::vector<std::string> shadowModes = {
            "None", "Contact", "Dynamic"};
    int shadowMode = static_cast<int>(object->dynamicModel.shadowMode);
    const int previousShadowMode = shadowMode;
    if (engine::Option(
                context.ui, context.config, context.input, context.assets,
                "sector_editor_dynamic_shadow_mode",
                Rectangle{0.0f, y, contentW, rowH}, context.font,
                shadowModes, shadowMode)
            && shadowMode != previousShadowMode
            && shadowMode >= 0
            && shadowMode < static_cast<int>(shadowModes.size())) {
        const auto mode = static_cast<SectorDynamicModelShadowMode>(shadowMode);
        context.editing.MutateSelected("Updated dynamic prop shadow", [mode](auto& target) {
            if (target.kind != "dynamic_model" || target.dynamicModel.shadowMode == mode) {
                return false;
            }
            target.dynamicModel.shadowMode = mode;
            return true;
        });
    }
    y += rowH + gap;

    object = context.editing.SelectedObject(); if (object == nullptr) return;
    bool loop = object->dynamicModel.loop;
    if (engine::Checkbox(context.ui, context.config, context.input, context.assets,
                "sector_editor_dynamic_loop", Rectangle{0.0f, y, contentW, rowH},
                context.font, "Loop animation", loop)) {
        context.editing.MutateSelected("Updated dynamic prop looping", [loop](auto& target) {
            if (target.kind != "dynamic_model" || target.dynamicModel.loop == loop) return false;
            target.dynamicModel.loop = loop; return true;
        });
    }
    y += rowH + gap;
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    if (context.uiState.dynamicModelUseObjectId != object->id) {
        std::snprintf(
                context.uiState.dynamicModelUseTitleBuffer,
                sizeof(context.uiState.dynamicModelUseTitleBuffer),
                "%s",
                object->dynamicModel.useTitle.c_str());
        std::snprintf(
                context.uiState.dynamicModelOnUseScriptBuffer,
                sizeof(context.uiState.dynamicModelOnUseScriptBuffer),
                "%s",
                object->dynamicModel.onUseScript.c_str());
        context.uiState.dynamicModelUseObjectId = object->id;
        context.uiState.dynamicModelUseError.clear();
    }
    const auto drawTextField = [&] (
            const char* id,
            const char* label,
            char* buffer,
            size_t capacity,
            const std::function<bool(SectorPlacedRuntimeObject&, const std::string&)>& apply) {
        engine::Text(
                context.ui, context.config, context.assets,
                Rectangle{0.0f, y, 118.0f, rowH}, context.font,
                label, engine::UITextJustify::Left,
                context.config.mutedTextColor);
        const engine::UITextInputResult result = engine::TextInput(
                context.ui, context.config, context.input, context.assets,
                id,
                Rectangle{122.0f, y, std::max(0.0f, contentW - 122.0f), rowH},
                context.font,
                buffer,
                capacity,
                0,
                capacity - 1,
                engine::UITextJustify::Left);
        if (result.submitted) {
            const std::string value{buffer};
            context.editing.MutateSelected(
                    "Updated dynamic prop interaction",
                    [apply, value](SectorPlacedRuntimeObject& target) {
                        return apply(target, value);
                    });
        }
        y += rowH + gap;
    };
    drawTextField(
            "sector_editor_dynamic_use_title",
            "Use Title",
            context.uiState.dynamicModelUseTitleBuffer,
            sizeof(context.uiState.dynamicModelUseTitleBuffer),
            [](auto& target, const std::string& value) {
                if (target.kind != "dynamic_model"
                        || !IsValidSectorUseTitle(value)
                        || target.dynamicModel.useTitle == value) return false;
                target.dynamicModel.useTitle = value;
                return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawTextField(
            "sector_editor_dynamic_on_use_script",
            "On Use",
            context.uiState.dynamicModelOnUseScriptBuffer,
            sizeof(context.uiState.dynamicModelOnUseScriptBuffer),
            [](auto& target, const std::string& value) {
                if (target.kind != "dynamic_model"
                        || (!value.empty() && !IsValidSectorTriggerScriptName(value))
                        || target.dynamicModel.onUseScript == value) return false;
                target.dynamicModel.onUseScript = value;
                return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawFloat(
            "sector_editor_dynamic_use_distance",
            "Use Dist",
            object->dynamicModel.useDistance,
            context.uiState.dynamicModelUseDistanceInput,
            [](auto& target, float value) {
                if (target.kind != "dynamic_model"
                        || target.dynamicModel.useDistance == value) return false;
                target.dynamicModel.useDistance = value;
                return true;
            },
            0.001f,
            100000.0f);
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    bool singleUse = object->dynamicModel.singleUse;
    if (engine::Checkbox(
                context.ui, context.config, context.input, context.assets,
                "sector_editor_dynamic_single_use",
                Rectangle{0.0f, y, contentW, rowH},
                context.font,
                "Single use",
                singleUse)) {
        context.editing.MutateSelected(
                "Updated dynamic prop interaction",
                [singleUse](auto& target) {
                    if (target.kind != "dynamic_model"
                            || target.dynamicModel.singleUse == singleUse) return false;
                    target.dynamicModel.singleUse = singleUse;
                    return true;
                });
    }
    y += rowH + gap;
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    bool collision = object->dynamicModel.collision;
    if (engine::Checkbox(context.ui, context.config, context.input, context.assets,
                "sector_editor_dynamic_collision", Rectangle{0.0f, y, contentW, rowH},
                context.font, "Collision", collision)) {
        context.editing.MutateSelected("Updated dynamic prop collision", [collision](auto& target) {
            if (target.kind != "dynamic_model" || target.dynamicModel.collision == collision) return false;
            target.dynamicModel.collision = collision; return true;
        });
    }
    y += rowH + gap;
    if (engine::Button(context.ui, context.config, context.input, context.assets,
                "sector_editor_delete_dynamic_model", Rectangle{0.0f, y, contentW, rowH},
                context.font, "Delete")) {
        context.deleteRequested = true;
    }
}

} // namespace game
