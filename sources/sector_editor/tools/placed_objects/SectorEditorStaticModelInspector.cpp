#include "sector_editor/tools/placed_objects/SectorEditorStaticModelInspector.h"

#include "sector_editor/SectorEditorUiHelpers.h"

#include <raylib.h>

#include <cmath>
#include <functional>
#include <string>

namespace game {
namespace {

enum class StaticModelPreviewStatus {
    Unassigned,
    Pending,
    Ready,
    Failed,
    Unavailable
};

struct StaticModelPreviewStatusText {
    StaticModelPreviewStatus status = StaticModelPreviewStatus::Unavailable;
    const char* text = "Model: preview object unavailable";
};

StaticModelPreviewStatusText ModelStatus(
        const SectorPlacedRuntimeObject& object,
        const SectorRuntimeObjectState& runtimeObjects,
        engine::EngineContext* engineContext,
        engine::AssetManager& assets)
{
    if (object.staticModel.modelPath.empty()) {
        return {StaticModelPreviewStatus::Unassigned, "Model: None selected"};
    }
    if (engineContext == nullptr) {
        return {StaticModelPreviewStatus::Unavailable, "Model: preview unavailable"};
    }
    for (const SectorPlacedRuntimeObjectEntity& entry :
            runtimeObjects.placedObjectEntities) {
        if (entry.placedObjectId != object.id
                || !engineContext->world.IsAlive(entry.entity)
                || !engineContext->world.Has<SectorStaticModel>(entry.entity)) {
            continue;
        }
        const engine::ModelHandle handle =
                engineContext->world.Get<SectorStaticModel>(entry.entity).model;
        if (engine::IsNull(handle)) {
            return {StaticModelPreviewStatus::Failed, "Model: request failed"};
        }
        if (assets.IsReady(handle)) {
            return {StaticModelPreviewStatus::Ready, "Model: ready"};
        }
        if (assets.HasFailed(handle)) {
            return {StaticModelPreviewStatus::Failed, "Model: failed to load"};
        }
        return {StaticModelPreviewStatus::Pending, "Model: loading"};
    }
    return {StaticModelPreviewStatus::Unavailable, "Model: preview object unavailable"};
}

} // namespace

float MeasureSectorEditorStaticModelInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext&,
        const SectorPlacedRuntimeObject&)
{
    return 38.0f * 2.0f + 48.0f * 9.0f + 8.0f * 11.0f + 40.0f
            + 48.0f;
}

void DrawSectorEditorStaticModelInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y)
{
    const SectorPlacedRuntimeObject* object = context.editing.SelectedObject();
    if (object == nullptr || object->kind != "static_model") {
        return;
    }
    const float contentW = context.contentW;
    const float rowH = context.rowH;
    const float gap = context.gap;
    const engine::UIConfig smallConfig =
            SectorEditorSmallFontConfig(
                    context.config,
                    context.assets,
                    context.smallFont);
    const std::string_view filename = SectorEditorModelFilename(
            object->staticModel.modelPath);
    std::string modelLabel = "Model file: ";
    modelLabel.append(filename.empty() ? "<none>" : filename);
    engine::Text(
            context.ui,
            smallConfig,
            context.assets,
            Rectangle{0.0f, y, contentW, 38.0f},
            context.smallFont,
            modelLabel.c_str(),
            engine::UITextJustify::Left,
            object->staticModel.modelPath.empty()
                    ? context.config.invalidColor
                    : context.config.textColor,
            false);
    y += 42.0f;
    const StaticModelPreviewStatusText modelStatus =
            ModelStatus(
                    *object,
                    context.runtimeObjects,
                    context.engineContext,
                    context.assets);
    engine::Text(
            context.ui,
            smallConfig,
            context.assets,
            Rectangle{0.0f, y, contentW, 30.0f},
            context.smallFont,
            modelStatus.text,
            engine::UITextJustify::Left,
            modelStatus.status == StaticModelPreviewStatus::Failed
                    ? context.config.invalidColor
                    : (modelStatus.status == StaticModelPreviewStatus::Ready
                            ? context.config.accentColor
                            : context.config.mutedTextColor));
    y += 34.0f + gap;

    if (engine::Button(
                context.ui,
                context.config,
                context.input,
                context.assets,
                "sector_editor_static_model_choose",
                Rectangle{0.0f, y, contentW, rowH},
                context.font,
                "Choose Model")) {
        context.staticModelPicker.Open(object->staticModel.modelPath);
    }
    y += rowH + gap;

    const auto drawFloat = [&](
            const char* id,
            const char* label,
            float value,
            engine::UIFloatInputState& inputState,
            const char* status,
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
                context.ui,
                context.config,
                context.input,
                context.assets,
                context.font,
                id,
                label,
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Left,
                value,
                inputState,
                minimum,
                maximum,
                3);
        if (result.changed && result.finite && result.value != value) {
            context.editing.MutateSelected(
                    status,
                    [apply, newValue = result.value](
                            SectorPlacedRuntimeObject& target) {
                        return apply(target, newValue);
                    });
        }
        y += rowH + gap;
    };

    drawFloat(
            "sector_editor_static_model_x",
            "Position X",
            object->position.x,
            context.uiState.xInput,
            "Updated 3D prop transform",
            [](SectorPlacedRuntimeObject& target, float value) {
                if (target.kind != "static_model" || target.position.x == value) {
                    return false;
                }
                target.position.x = value;
                return true;
            });
    object = context.editing.SelectedObject();
    if (object == nullptr) return;
    drawFloat(
            "sector_editor_static_model_z",
            "Position Z",
            object->position.z,
            context.uiState.zInput,
            "Updated 3D prop transform",
            [](SectorPlacedRuntimeObject& target, float value) {
                if (target.kind != "static_model" || target.position.z == value) {
                    return false;
                }
                target.position.z = value;
                return true;
            });
    object = context.editing.SelectedObject();
    if (object == nullptr) return;
    constexpr float radiansToDegrees = 180.0f / PI;
    constexpr float degreesToRadians = PI / 180.0f;
    drawFloat(
            "sector_editor_static_model_rotation_x",
            "Rotation X",
            object->staticModel.rotationXRadians * radiansToDegrees,
            context.uiState.rotationXInput,
            "Updated 3D prop rotation",
            [degreesToRadians](SectorPlacedRuntimeObject& target, float value) {
                const float radians = value * degreesToRadians;
                if (target.kind != "static_model"
                        || target.staticModel.rotationXRadians == radians) {
                    return false;
                }
                target.staticModel.rotationXRadians = radians;
                return true;
            });
    object = context.editing.SelectedObject();
    if (object == nullptr) return;
    drawFloat(
            "sector_editor_static_model_yaw",
            "Yaw (Y)",
            object->yawRadians * radiansToDegrees,
            context.uiState.yawInput,
            "Updated 3D prop yaw",
            [degreesToRadians](SectorPlacedRuntimeObject& target, float value) {
                const float radians = value * degreesToRadians;
                if (target.kind != "static_model"
                        || target.yawRadians == radians) {
                    return false;
                }
                target.yawRadians = radians;
                return true;
            });
    object = context.editing.SelectedObject();
    if (object == nullptr) return;
    drawFloat(
            "sector_editor_static_model_rotation_z",
            "Rotation Z",
            object->staticModel.rotationZRadians * radiansToDegrees,
            context.uiState.rotationZInput,
            "Updated 3D prop rotation",
            [degreesToRadians](SectorPlacedRuntimeObject& target, float value) {
                const float radians = value * degreesToRadians;
                if (target.kind != "static_model"
                        || target.staticModel.rotationZRadians == radians) {
                    return false;
                }
                target.staticModel.rotationZRadians = radians;
                return true;
            });
    object = context.editing.SelectedObject();
    if (object == nullptr) return;
    drawFloat(
            "sector_editor_static_model_height_offset",
            "Height Offset",
            object->staticModel.heightOffsetWorld,
            context.uiState.heightOffsetInput,
            "Updated 3D prop height offset",
            [](SectorPlacedRuntimeObject& target, float value) {
                if (target.kind != "static_model"
                        || target.staticModel.heightOffsetWorld == value) {
                    return false;
                }
                target.staticModel.heightOffsetWorld = value;
                return true;
            });
    object = context.editing.SelectedObject();
    if (object == nullptr) return;
    drawFloat(
            "sector_editor_static_model_scale",
            "Scale",
            object->staticModel.scale,
            context.uiState.scaleInput,
            "Updated 3D prop scale",
            [](SectorPlacedRuntimeObject& target, float value) {
                if (target.kind != "static_model"
                        || target.staticModel.scale == value) {
                    return false;
                }
                target.staticModel.scale = value;
                return true;
            },
            0.001f,
            100000.0f);
    object = context.editing.SelectedObject();
    if (object == nullptr) return;

    bool collision = object->staticModel.collision;
    if (engine::Checkbox(
                context.ui,
                context.config,
                context.input,
                context.assets,
                "sector_editor_static_model_collision",
                Rectangle{0.0f, y, contentW, rowH},
                context.font,
                "Collision",
                collision)) {
        context.editing.MutateSelected(
                "Updated 3D prop collision",
                [collision](SectorPlacedRuntimeObject& target) {
                    if (target.kind != "static_model"
                            || target.staticModel.collision == collision) {
                        return false;
                    }
                    target.staticModel.collision = collision;
                    return true;
                });
    }
    y += rowH + gap;
    object = context.editing.SelectedObject();
    if (object == nullptr) return;

    bool castsShadow = object->staticModel.castsShadow;
    if (engine::Checkbox(
                context.ui,
                context.config,
                context.input,
                context.assets,
                "sector_editor_static_model_casts_shadow",
                Rectangle{0.0f, y, contentW, rowH},
                context.font,
                "Cast Shadow",
                castsShadow)) {
        context.editing.MutateSelected(
                "Updated 3D prop shadow casting",
                [castsShadow](SectorPlacedRuntimeObject& target) {
                    if (target.kind != "static_model"
                            || target.staticModel.castsShadow == castsShadow) {
                        return false;
                    }
                    target.staticModel.castsShadow = castsShadow;
                    return true;
                });
    }
    y += rowH + gap;

    if (engine::Button(
                context.ui,
                context.config,
                context.input,
                context.assets,
                "sector_editor_delete_static_model",
                Rectangle{0.0f, y, contentW, rowH},
                context.font,
                "Delete")) {
        context.deleteRequested = true;
    }
}

} // namespace game
