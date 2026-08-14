#include "sector_editor/inspector/SectorEditorTriggerInspector.h"

#include "sector_editor/SectorEditorUiHelpers.h"

#include <climits>
#include <cstdio>

namespace game {
namespace {

constexpr float ErrorHeight = 36.0f;

void DrawStackedLabel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle font,
        Rectangle bounds,
        const char* label)
{
    engine::Text(
            ui, config, assets, bounds, font, label,
            engine::UITextJustify::Left, config.mutedTextColor);
}

} // namespace

float MeasureSectorEditorTriggerInspectorContentHeight(
        const TriggerEditingUiState& uiState,
        float rowHeight,
        float gap)
{
    const float stacked = SectorEditorInspectorStackedOptionRowHeight(rowHeight, gap);
    return 38.0f
            + stacked + gap
            + (uiState.idError.empty() ? 0.0f : ErrorHeight)
            + (rowHeight + gap) * 2.0f
            + stacked + gap
            + stacked + gap
            + (uiState.scriptError.empty() ? 0.0f : ErrorHeight)
            + rowHeight;
}

bool DrawSectorEditorTriggerInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        float contentWidth,
        float rowHeight,
        float gap,
        const SectorAuthoringTrigger& trigger,
        TriggerEditingUiState& uiState,
        SectorEditorTriggerEditingService& editing)
{
    if (uiState.bufferedTriggerId != trigger.editorId) {
        std::snprintf(uiState.idBuffer, sizeof(uiState.idBuffer), "%s", trigger.id.c_str());
        std::snprintf(uiState.scriptBuffer, sizeof(uiState.scriptBuffer), "%s", trigger.script.c_str());
        uiState.bufferedTriggerId = trigger.editorId;
        uiState.idError.clear();
        uiState.scriptError.clear();
        uiState.delayInput = engine::UIIntInputState{};
    }

    float y = 0.0f;
    engine::Text(
            ui, config, assets, Rectangle{0.0f, y, contentWidth, 34.0f}, font,
            TextFormat("Trigger: %d", trigger.editorId),
            engine::UITextJustify::Left, config.textColor);
    y += 38.0f;

    SectorEditorInspectorStackedOptionRowLayout layout =
            BuildSectorEditorInspectorStackedOptionRowLayout(y, contentWidth, rowHeight, gap);
    DrawStackedLabel(ui, config, assets, font, layout.labelRect, "ID");
    const engine::UITextInputResult idResult = engine::TextInput(
            ui, config, input, assets, "sector_editor_trigger_id", layout.fieldRect, font,
            uiState.idBuffer, sizeof(uiState.idBuffer), 1, sizeof(uiState.idBuffer) - 1,
            engine::UITextJustify::Left);
    if (idResult.submitted) {
        editing.RenameSelected(std::string{uiState.idBuffer}, uiState.idError);
    }
    y += layout.height + gap;
    if (!uiState.idError.empty()) {
        engine::Text(
                ui, config, assets, Rectangle{0.0f, y, contentWidth, ErrorHeight - 2.0f}, font,
                uiState.idError.c_str(), engine::UITextJustify::Left, config.invalidColor, true);
        y += ErrorHeight;
    }

    bool enabled = trigger.enabled;
    if (engine::Checkbox(
                ui, config, input, assets, "sector_editor_trigger_enabled",
                Rectangle{0.0f, y, contentWidth, rowHeight}, font, "Enabled", enabled)) {
        editing.SetSelectedEnabled(enabled);
    }
    y += rowHeight + gap;

    bool repeat = trigger.repeat;
    if (engine::Checkbox(
                ui, config, input, assets, "sector_editor_trigger_repeat",
                Rectangle{0.0f, y, contentWidth, rowHeight}, font, "Repeat", repeat)) {
        editing.SetSelectedRepeat(repeat);
    }
    y += rowHeight + gap;

    layout = BuildSectorEditorInspectorStackedOptionRowLayout(y, contentWidth, rowHeight, gap);
    DrawStackedLabel(ui, config, assets, font, layout.labelRect, "Delay (milliseconds)");
    int delay = trigger.delayMilliseconds;
    const engine::UINumericInputResult delayResult = engine::IntInput(
            ui, config, input, assets, "sector_editor_trigger_delay", layout.fieldRect, font,
            delay, uiState.delayInput, 0, INT_MAX, 1);
    if (delayResult.changed) editing.SetSelectedDelay(delay);
    y += layout.height + gap;

    layout = BuildSectorEditorInspectorStackedOptionRowLayout(y, contentWidth, rowHeight, gap);
    DrawStackedLabel(ui, config, assets, font, layout.labelRect, "Script function");
    const engine::UITextInputResult scriptResult = engine::TextInput(
            ui, config, input, assets, "sector_editor_trigger_script", layout.fieldRect, font,
            uiState.scriptBuffer, sizeof(uiState.scriptBuffer), 0, sizeof(uiState.scriptBuffer) - 1,
            engine::UITextJustify::Left);
    if (scriptResult.submitted) {
        editing.SetSelectedScript(std::string{uiState.scriptBuffer}, uiState.scriptError);
    }
    y += layout.height + gap;
    if (!uiState.scriptError.empty()) {
        engine::Text(
                ui, config, assets, Rectangle{0.0f, y, contentWidth, ErrorHeight - 2.0f}, font,
                uiState.scriptError.c_str(), engine::UITextJustify::Left, config.invalidColor, true);
        y += ErrorHeight;
    }

    return engine::Button(
            ui, config, input, assets, "sector_editor_trigger_delete",
            Rectangle{0.0f, y, contentWidth, rowHeight}, font, "Delete Trigger");
}

} // namespace game
