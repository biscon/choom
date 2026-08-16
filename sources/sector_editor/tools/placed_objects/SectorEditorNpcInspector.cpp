#include "sector_editor/tools/placed_objects/SectorEditorNpcInspector.h"

#include "sector_editor/SectorEditorUiHelpers.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace game {

float MeasureSectorEditorNpcInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject&)
{
    const SectorEditorNpcPlacementState& state =
            context.editingState.npcPlacement;
    return 38.0f * 2.0f
            + (context.rowH + context.gap) * 8.0f
            + 68.0f
            + (state.instanceIdError.empty() ? 0.0f : 40.0f);
}

void DrawSectorEditorNpcInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y)
{
    const SectorPlacedRuntimeObject* object = context.editing.SelectedObject();
    if (object == nullptr || object->kind != "npc") return;

    const float contentW = context.contentW;
    const float rowH = context.rowH;
    const float gap = context.gap;
    SectorEditorNpcPlacementState& state = context.editingState.npcPlacement;
    RefreshSectorEditorNpcPlacementOptions(
            state,
            context.runtimeObjects.npcDefinitionCatalog,
            context.runtimeObjects.npcDefinitionCatalogRevision);

    engine::Text(
            context.ui, context.config, context.assets,
            Rectangle{0.0f, y, contentW, 28.0f},
            context.font,
            "NPC Definition",
            engine::UITextJustify::Left,
            context.config.mutedTextColor);
    y += 32.0f;

    int selectedDefinition = PrepareSectorEditorNpcInspectorOptions(
            state,
            object->npc.definitionId);
    const int previousDefinition = selectedDefinition;
    if (!state.inspectorOptionLabels.empty()
            && engine::Option(
                    context.ui, context.config, context.input, context.assets,
                    "sector_editor_npc_definition",
                    Rectangle{0.0f, y, contentW, rowH},
                    context.font,
                    state.inspectorOptionLabels.data(),
                    state.inspectorOptionLabels.size(),
                    selectedDefinition)
            && selectedDefinition != previousDefinition
            && selectedDefinition >= 0
            && selectedDefinition < static_cast<int>(state.definitionIds.size())) {
        context.editing.AssignSelectedNpcDefinition(
                state.definitionIds[static_cast<size_t>(selectedDefinition)]);
    }
    y += rowH + gap;

    const bool definitionMissing = FindNpcDefinition(
            context.runtimeObjects.npcDefinitionCatalog,
            object->npc.definitionId) == nullptr;
    std::string catalogMessage;
    if (definitionMissing) {
        catalogMessage = "Definition is missing; this NPC will not spawn in 3D";
    } else if (!context.runtimeObjects.npcDefinitionCatalogWarning.empty()) {
        catalogMessage = context.runtimeObjects.npcDefinitionCatalogWarning;
    }
    if (!catalogMessage.empty()) {
        const engine::UIConfig smallConfig = SectorEditorSmallFontConfig(
                context.config,
                context.assets,
                context.smallFont);
        engine::Text(
                context.ui, smallConfig, context.assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                context.smallFont,
                catalogMessage.c_str(),
                engine::UITextJustify::Left,
                definitionMissing
                        ? context.config.invalidColor
                        : context.config.mutedTextColor,
                true);
        y += 38.0f;
    }

    object = context.editing.SelectedObject();
    if (object == nullptr || object->kind != "npc") return;
    if (state.bufferedObjectId != object->id) {
        std::snprintf(
                state.instanceIdBuffer,
                sizeof(state.instanceIdBuffer),
                "%s",
                object->npc.instanceId.c_str());
        state.bufferedObjectId = object->id;
        state.instanceIdError.clear();
    }
    engine::Text(
            context.ui, context.config, context.assets,
            Rectangle{0.0f, y, 118.0f, rowH},
            context.font,
            "Instance ID",
            engine::UITextJustify::Left,
            context.config.mutedTextColor);
    const engine::UITextInputResult instanceResult = engine::TextInput(
            context.ui, context.config, context.input, context.assets,
            "sector_editor_npc_instance_id",
            Rectangle{122.0f, y, std::max(0.0f, contentW - 122.0f), rowH},
            context.font,
            state.instanceIdBuffer,
            sizeof(state.instanceIdBuffer),
            0,
            sizeof(state.instanceIdBuffer) - 1,
            engine::UITextJustify::Left);
    if (instanceResult.submitted) {
        context.editing.SetSelectedNpcInstanceId(
                std::string{state.instanceIdBuffer},
                state.instanceIdError);
    }
    y += rowH + gap;
    if (!state.instanceIdError.empty()) {
        engine::Text(
                context.ui, context.config, context.assets,
                Rectangle{0.0f, y, contentW, 36.0f},
                context.smallFont,
                state.instanceIdError.c_str(),
                engine::UITextJustify::Left,
                context.config.invalidColor,
                true);
        y += 40.0f;
    }

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
                        y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                context.ui, context.config, context.input, context.assets,
                context.font, id, label,
                layout.labelRect, layout.inputRect,
                engine::UITextJustify::Left, value, inputState,
                minimum, maximum, 3);
        if (result.changed && result.finite && result.value != value) {
            context.editing.MutateSelected(
                    "Updated NPC placement",
                    [apply, newValue = result.value](SectorPlacedRuntimeObject& target) {
                        return apply(target, newValue);
                    });
        }
        y += rowH + gap;
    };

    drawFloat(
            "sector_editor_npc_x", "Position X", object->position.x,
            context.uiState.xInput,
            [](auto& target, float value) {
                if (target.kind != "npc" || target.position.x == value) return false;
                target.position.x = value;
                return true;
            });
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawFloat(
            "sector_editor_npc_z", "Position Z", object->position.z,
            context.uiState.zInput,
            [](auto& target, float value) {
                if (target.kind != "npc" || target.position.z == value) return false;
                target.position.z = value;
                return true;
            });

    constexpr float radiansToDegrees = 180.0f / PI;
    constexpr float degreesToRadians = PI / 180.0f;
    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawFloat(
            "sector_editor_npc_yaw", "Initial Yaw",
            object->yawRadians * radiansToDegrees,
            context.uiState.yawInput,
            [degreesToRadians](auto& target, float value) {
                const float radians = value * degreesToRadians;
                if (target.kind != "npc" || target.yawRadians == radians) return false;
                target.yawRadians = radians;
                return true;
            },
            -3600.0f,
            3600.0f);

    object = context.editing.SelectedObject(); if (object == nullptr) return;
    drawFloat(
            "sector_editor_npc_scale", "Scale", object->npc.scale,
            context.uiState.scaleInput,
            [](auto& target, float value) {
                if (target.kind != "npc" || target.npc.scale == value) return false;
                target.npc.scale = value;
                return true;
            },
            0.001f,
            100000.0f);

    object = context.editing.SelectedObject(); if (object == nullptr) return;
    static const std::vector<std::string> shadowModes = {
            "None", "Contact", "Silhouette"};
    int shadowMode = static_cast<int>(object->npc.shadowMode);
    const int previousShadowMode = shadowMode;
    if (engine::Option(
                context.ui, context.config, context.input, context.assets,
                "sector_editor_npc_shadow_mode",
                Rectangle{0.0f, y, contentW, rowH},
                context.font,
                shadowModes,
                shadowMode)
            && shadowMode != previousShadowMode
            && shadowMode >= 0
            && shadowMode < static_cast<int>(shadowModes.size())) {
        const auto mode = static_cast<SectorDynamicModelShadowMode>(shadowMode);
        context.editing.MutateSelected(
                "Updated NPC shadow",
                [mode](SectorPlacedRuntimeObject& target) {
                    if (target.kind != "npc" || target.npc.shadowMode == mode) {
                        return false;
                    }
                    target.npc.shadowMode = mode;
                    return true;
                });
    }
    y += rowH + gap;

    if (engine::Button(
                context.ui, context.config, context.input, context.assets,
                "sector_editor_delete_npc",
                Rectangle{0.0f, y, contentW, rowH},
                context.font,
                "Delete")) {
        context.deleteRequested = true;
    }
}

} // namespace game
