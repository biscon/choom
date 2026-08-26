#include "sector_editor/tools/placed_objects/SectorEditorItemInspector.h"

#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"
#include "sector_demo/SectorTriggers.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace game {

float MeasureSectorEditorItemInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject& object)
{
    const ItemRegistry* registry = context.editing.ItemRegistryView();
    const ItemDefinition* definition = registry != nullptr
            ? FindItemDefinition(*registry, object.item.definitionId) : nullptr;
    return 180.0f + context.rowH * (definition != nullptr
                    && definition->type == ItemType::Object ? 15.0f : 14.0f)
            + context.gap * 16.0f;
}

void DrawSectorEditorItemInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y)
{
    const SectorPlacedRuntimeObject* object = context.editing.SelectedObject();
    if (object == nullptr || object->kind != "item") return;
    const float contentW = context.contentW;
    const float rowH = context.rowH;
    const float gap = context.gap;
    const ItemRegistry* registry = context.editing.ItemRegistryView();
    const ItemDefinition* definition = registry != nullptr
            ? FindItemDefinition(*registry, object->item.definitionId) : nullptr;

    auto& placement = context.editingState.itemPlacement;
    int definitionIndex = -1;
    for (std::size_t index = 0; index < placement.definitionIds.size(); ++index) {
        if (placement.definitionIds[index] == object->item.definitionId) {
            definitionIndex = static_cast<int>(index);
            break;
        }
    }
    engine::Text(
            context.ui, context.config, context.assets,
            Rectangle{0.0f, y, 112.0f, rowH}, context.font,
            "Definition", engine::UITextJustify::Left,
            context.config.mutedTextColor);
    if (!placement.labels.empty()) {
        const int previous = definitionIndex;
        engine::Option(
                context.ui, context.config, context.input, context.assets,
                "sector_editor_item_definition",
                Rectangle{116.0f, y, std::max(0.0f, contentW - 116.0f), rowH},
                context.font,
                placement.labels.data(), placement.labels.size(), definitionIndex);
        if (definitionIndex != previous && definitionIndex >= 0) {
            context.editing.AssignSelectedItemDefinition(
                    placement.definitionIds[static_cast<std::size_t>(definitionIndex)]);
        }
    }
    y += rowH + gap;
    object = context.editing.SelectedObject();
    if (object == nullptr) return;
    definition = registry != nullptr
            ? FindItemDefinition(*registry, object->item.definitionId) : nullptr;
    const std::string status = definition == nullptr
            ? "Missing definition: " + object->item.definitionId
            : definition->title + " | " + ItemTypeName(definition->type);
    engine::Text(
            context.ui, context.config, context.assets,
            Rectangle{0.0f, y, contentW, 34.0f}, context.smallFont,
            status.c_str(), engine::UITextJustify::Left,
            definition == nullptr ? context.config.invalidColor
                                  : context.config.accentColor,
            true);
    y += 38.0f;
    const std::string model = definition == nullptr
            ? "Model: unavailable"
            : "Model: " + definition->modelPath;
    engine::Text(
            context.ui, context.config, context.assets,
            Rectangle{0.0f, y, contentW, 42.0f}, context.smallFont,
            model.c_str(), engine::UITextJustify::Left,
            context.config.mutedTextColor, true);
    y += 46.0f + gap;

    if (context.uiState.itemBufferedObjectId != object->id) {
        std::snprintf(context.uiState.itemInstanceIdBuffer,
                sizeof(context.uiState.itemInstanceIdBuffer), "%s",
                object->item.instanceId.c_str());
        std::snprintf(context.uiState.itemOnTakeScriptBuffer,
                sizeof(context.uiState.itemOnTakeScriptBuffer), "%s",
                object->item.onTakeScript.c_str());
        std::snprintf(context.uiState.itemOnUseScriptBuffer,
                sizeof(context.uiState.itemOnUseScriptBuffer), "%s",
                object->item.onUseScript.c_str());
        context.uiState.itemBufferedObjectId = object->id;
        context.uiState.itemError.clear();
    }
    const auto textField = [&](const char* id, const char* label,
            char* buffer, std::size_t capacity,
            const std::function<void(const std::string&)>& apply) {
        engine::Text(context.ui, context.config, context.assets,
                Rectangle{0.0f, y, 112.0f, rowH}, context.font,
                label, engine::UITextJustify::Left,
                context.config.mutedTextColor);
        const engine::UITextInputResult result = engine::TextInput(
                context.ui, context.config, context.input, context.assets,
                id,
                Rectangle{116.0f, y, std::max(0.0f, contentW - 116.0f), rowH},
                context.font, buffer, capacity, 0, capacity - 1,
                engine::UITextJustify::Left);
        if (result.submitted) apply(std::string{buffer});
        y += rowH + gap;
    };
    textField("sector_editor_item_instance", "Instance ID",
            context.uiState.itemInstanceIdBuffer,
            sizeof(context.uiState.itemInstanceIdBuffer),
            [&](const std::string& value) {
                context.editing.SetSelectedItemInstanceId(
                        value, context.uiState.itemError);
            });

    const auto floatField = [&](const char* id, const char* label, float value,
            engine::UIFloatInputState& inputState,
            const std::function<bool(SectorPlacedRuntimeObject&, float)>& apply,
            float minimum = -100000.0f, float maximum = 100000.0f) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(
                        y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                context.ui, context.config, context.input, context.assets,
                context.font, id, label, layout.labelRect, layout.inputRect,
                engine::UITextJustify::Left, value, inputState,
                minimum, maximum, 3);
        if (result.changed && result.finite && result.value != value) {
            context.editing.MutateSelected("Updated item", [=](auto& target) {
                return apply(target, result.value);
            });
        }
        y += rowH + gap;
    };
    floatField("sector_editor_item_x", "Position X", object->position.x,
            context.uiState.xInput, [](auto& target, float value) {
                if (target.kind != "item" || target.position.x == value) return false;
                target.position.x = value; return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    floatField("sector_editor_item_z", "Position Z", object->position.z,
            context.uiState.zInput, [](auto& target, float value) {
                if (target.kind != "item" || target.position.z == value) return false;
                target.position.z = value; return true;
            });
    constexpr float radiansToDegrees = 180.0f / PI;
    constexpr float degreesToRadians = PI / 180.0f;
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    floatField("sector_editor_item_rx", "Rotation X",
            object->item.rotationXRadians * radiansToDegrees,
            context.uiState.rotationXInput, [=](auto& target, float value) {
                const float radians = value * degreesToRadians;
                if (target.kind != "item" || target.item.rotationXRadians == radians) return false;
                target.item.rotationXRadians = radians; return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    floatField("sector_editor_item_yaw", "Yaw (Y)",
            object->yawRadians * radiansToDegrees, context.uiState.yawInput,
            [=](auto& target, float value) {
                const float radians = value * degreesToRadians;
                if (target.kind != "item" || target.yawRadians == radians) return false;
                target.yawRadians = radians; return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    floatField("sector_editor_item_rz", "Rotation Z",
            object->item.rotationZRadians * radiansToDegrees,
            context.uiState.rotationZInput, [=](auto& target, float value) {
                const float radians = value * degreesToRadians;
                if (target.kind != "item" || target.item.rotationZRadians == radians) return false;
                target.item.rotationZRadians = radians; return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    floatField("sector_editor_item_height", "Height Offset",
            object->item.heightOffsetWorld, context.uiState.heightOffsetInput,
            [](auto& target, float value) {
                if (target.kind != "item" || target.item.heightOffsetWorld == value) return false;
                target.item.heightOffsetWorld = value; return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    floatField("sector_editor_item_scale", "Scale", object->item.scale,
            context.uiState.scaleInput, [](auto& target, float value) {
                if (target.kind != "item" || target.item.scale == value) return false;
                target.item.scale = value; return true;
            }, 0.001f, 100000.0f);

    object = context.editing.SelectedObject(); if (object == nullptr) return;
    const SectorEditorIntInputResult quantity = DrawLabeledIntInput(
            context.ui, context.config, context.input, context.assets,
            context.font, "sector_editor_item_quantity", "Quantity",
            Rectangle{0.0f, y, 112.0f, rowH},
            Rectangle{116.0f, y, std::max(0.0f, contentW - 116.0f), rowH},
            engine::UITextJustify::Left, object->item.quantity,
            context.uiState.itemQuantityInput, 1, 1000000, 1);
    if (quantity.changed && quantity.value != object->item.quantity) {
        context.editing.MutateSelected("Updated item quantity", [=](auto& target) {
            if (target.kind != "item") return false;
            target.item.quantity = quantity.value; return true;
        });
    }
    y += rowH + gap;
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    floatField("sector_editor_item_take_distance", "Take Distance",
            object->item.takeDistance, context.uiState.itemTakeDistanceInput,
            [](auto& target, float value) {
                if (target.kind != "item" || target.item.takeDistance == value) return false;
                target.item.takeDistance = value; return true;
            }, 0.001f, 100000.0f);

    object = context.editing.SelectedObject(); if (object == nullptr) return;
    static const std::vector<std::string> shadowModes = {
            "None", "Contact", "Dynamic"};
    int shadowMode = static_cast<int>(object->item.shadowMode);
    const int previousShadow = shadowMode;
    engine::Option(context.ui, context.config, context.input, context.assets,
            "sector_editor_item_shadow", Rectangle{0.0f, y, contentW, rowH},
            context.font, shadowModes, shadowMode);
    if (shadowMode != previousShadow && shadowMode >= 0 && shadowMode < 3) {
        context.editing.MutateSelected("Updated item shadow", [=](auto& target) {
            if (target.kind != "item") return false;
            target.item.shadowMode = static_cast<SectorDynamicModelShadowMode>(shadowMode);
            return true;
        });
    }
    y += rowH + gap;
    textField("sector_editor_item_on_take", "On Take",
            context.uiState.itemOnTakeScriptBuffer,
            sizeof(context.uiState.itemOnTakeScriptBuffer),
            [&](const std::string& value) {
                if (!IsValidSectorTriggerScriptName(value)) {
                    context.uiState.itemError = "On Take script name is invalid";
                    return;
                }
                context.editing.MutateSelected("Updated item On Take", [=](auto& target) {
                    if (target.kind != "item" || target.item.onTakeScript == value) return false;
                    target.item.onTakeScript = value; return true;
                });
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    definition = registry != nullptr
            ? FindItemDefinition(*registry, object->item.definitionId) : nullptr;
    if (definition != nullptr && definition->type == ItemType::Object) {
        textField("sector_editor_item_on_use", "On Use",
                context.uiState.itemOnUseScriptBuffer,
                sizeof(context.uiState.itemOnUseScriptBuffer),
                [&](const std::string& value) {
                    if (!IsValidSectorTriggerScriptName(value)) {
                        context.uiState.itemError = "On Use script name is invalid";
                        return;
                    }
                    context.editing.MutateSelected("Updated item On Use", [=](auto& target) {
                        if (target.kind != "item" || target.item.onUseScript == value) return false;
                        target.item.onUseScript = value; return true;
                    });
                });
    }
    if (!context.uiState.itemError.empty()) {
        engine::Text(context.ui, context.config, context.assets,
                Rectangle{0.0f, y, contentW, 36.0f}, context.smallFont,
                context.uiState.itemError.c_str(), engine::UITextJustify::Left,
                context.config.invalidColor, true);
        y += 40.0f;
    }
    if (engine::Button(context.ui, context.config, context.input, context.assets,
                "sector_editor_delete_item", Rectangle{0.0f, y, contentW, rowH},
                context.font, "Delete")) {
        context.deleteRequested = true;
    }
}

} // namespace game
