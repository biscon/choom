#include "sector_editor/tools/placed_objects/SectorEditorDynamicModelInspector.h"

#include "engine/components/AnimatedModel.h"
#include "sector_editor/SectorEditorUiHelpers.h"

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
        const SectorEditorPlacedObjectInspectorMeasureContext&,
        const SectorPlacedRuntimeObject&)
{
    return 38.0f * 2.0f + 48.0f * 14.0f + 8.0f * 16.0f + 70.0f;
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
    if (modelAsset != nullptr && modelAsset->animationCount > 0) {
        std::vector<std::string> names;
        names.reserve(static_cast<size_t>(modelAsset->animationCount));
        int selected = 0;
        bool authoredAnimationFound = object->dynamicModel.animation.empty();
        for (int i = 0; i < modelAsset->animationCount; ++i) {
            names.emplace_back(modelAsset->animations[i].name);
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
