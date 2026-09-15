#include "game/GameSettingsLayout.h"
#include "sector_editor/materials/SectorEditorMaterialFormLayout.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/SectorEditorMainMenu.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorLightmapModal.h"
#include "sector_editor/SectorEditorPreviewSettingsModal.h"
#include "sector_editor/preview/SectorEditorLightProxyPlacement.h"
#include "sector_editor/preview/SectorEditorPreviewOverlayLayout.h"
#include "sector_editor/inspector/SectorEditorInspectorPanel.h"
#include "sector_editor/inspector/SectorEditorCameraInspector.h"
#include "sector_editor/npcs/SectorEditorNpcEditorModal.h"
#include "sector_editor/weapons/SectorEditorWeaponEditorPanel.h"
#include "sector_editor/items/SectorEditorItemEditorPanel.h"
#include "sector_editor/sounds/SectorEditorSoundEditorPanel.h"
#include "sector_editor/patrols/SectorEditorPatrolEditorPanel.h"
#include "sector_demo/SectorLightmap.h"

#include <cmath>
#include <array>
#include <cstdio>
#include <iostream>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

bool Overlaps(Rectangle a, Rectangle b)
{
    return a.x < b.x + b.width
            && a.x + a.width > b.x
            && a.y < b.y + b.height
            && a.y + a.height > b.y;
}

void TestGameSettingsLayout()
{
    using Row = game::GameSettingsRow;
    for (float width : {248.0f, 400.0f, 664.0f}) {
        for (float fontSize : {18.0f, 24.0f, 32.0f}) {
            std::array<float, game::GameSettingsLabels.size()> heights;
            heights.fill(fontSize + 8.0f);
            heights[static_cast<std::size_t>(Row::GammaHelp)] = fontSize * 3.0f;
            heights[static_cast<std::size_t>(Row::VsyncHelp)] = fontSize * 4.0f;
            heights[static_cast<std::size_t>(Row::Performance)] = fontSize * 2.0f;
            for (float statusHeight : {0.0f, 160.0f, 600.0f}) {
                const auto layout = game::MeasureGameSettingsLayout(width, 20.0f,
                        fontSize * 13.0f, 48.0f, heights, statusHeight);
                float previousBottom = 0.0f;
                for (std::size_t i = 0; i < layout.rows.size(); ++i) {
                    const Rectangle row = layout.rows[i];
                    Check(row.y >= previousBottom, "settings rows never overlap preceding content");
                    Check(row.x >= 0 && row.x + row.width <= width,
                            "settings labels stay within the reserved content width");
                    previousBottom = row.y + row.height;
                    if (game::GameSettingsRowHasValue(static_cast<Row>(i))) {
                        const Rectangle field = layout.fields[i];
                        Check(!Overlaps(row, field), "settings labels and controls never overlap");
                        Check(field.width > 0 && field.x + field.width <= width,
                                "settings fields remain usable on narrow panels");
                        previousBottom = std::max(previousBottom, field.y + field.height);
                    }
                }
                const Rectangle apply = layout.buttons.back();
                Check(layout.buttons.front().y >= previousBottom,
                        "settings buttons follow all controls and help text");
                Check(layout.buttons.front().y >= layout.status.y + statusHeight,
                        "settings errors appear before the action buttons");
                Check(layout.contentHeight >= apply.y + apply.height + 24.0f,
                        "settings scroll extent includes the final button and bottom padding");
                for (float viewportHeight : {300.0f, 672.0f, 1032.0f}) {
                    const float maxScroll = std::max(0.0f, layout.contentHeight - viewportHeight);
                    Check(apply.y - maxScroll >= 0.0f
                                    && apply.y + apply.height - maxScroll <= viewportHeight,
                            "Apply is fully visible at maximum scroll");
                }
            }
        }
    }
}

void TestPreviewObjectAdjustmentLayout()
{
    using Row = game::SectorEditorPreviewObjectAdjustmentRow;
    engine::AssetManager assets;
    engine::UIConfig config;
    config.paddingX = 6.0f;
    config.paddingY = 4.0f;
    for (float width : {280.0f, 360.0f}) {
        for (float fontSize : {16.0f, 24.0f}) {
            config.fontSize = fontSize;
            const float controlHeight = fontSize + config.paddingY * 2.0f;
            for (bool pathBound : {false, true}) {
                for (bool stacked : {false, true}) {
                    std::array<float, game::SectorEditorPreviewObjectAdjustmentRowCount> heights;
                    heights.fill(controlHeight);
                    heights[static_cast<size_t>(Row::PositionX)] = engine::MeasureWrappedTextHeight(
                            config, assets, width - 28.0f, engine::NullFontHandle(),
                            "World X: -12345678901234567890.000 m");
                    heights[static_cast<size_t>(Row::Help)] = engine::MeasureWrappedTextHeight(
                            config, assets, width - 28.0f, engine::NullFontHandle(),
                            "Arrows: world X/Z   PgUp/PgDn: world Y   Q/E: yaw\n"
                            "Enter: apply   Esc: cancel   F11: unlock cursor");
                    heights[static_cast<size_t>(Row::PathWarning)] = pathBound
                            ? engine::MeasureWrappedTextHeight(config, assets, width - 28.0f,
                                    engine::NullFontHandle(), "Snap now / X/Z disabled: move the assigned path. "
                                    "Height and yaw remain adjustable.") : 0.0f;
                    for (Row row : {Row::Presets, Row::SnapControls, Row::Actions}) {
                        const int count = row == Row::Presets ? 3 : 2;
                        heights[static_cast<size_t>(row)] = stacked
                                ? count * controlHeight + (count - 1) * 6.0f : controlHeight;
                    }
                    const auto layout = game::BuildSectorEditorPreviewObjectAdjustmentLayout(
                            Rectangle{1500, 18, width, 0}, heights);
                    float previousBottom = layout.panel.y;
                    for (size_t i = 0; i < heights.size(); ++i) {
                        if (heights[i] == 0.0f) continue;
                        const auto row = layout.rows[i];
                        Check(row.y >= previousBottom && row.x >= layout.panel.x
                                      && row.x + row.width <= layout.panel.x + layout.panel.width,
                              "measured adjustment rows fit panel and do not overlap at narrow widths");
                        previousBottom = row.y + row.height;
                    }
                    Check(previousBottom + 12.0f <= layout.panel.y + layout.panel.height,
                          "adjustment panel includes final actions and bottom padding");
                    for (Row row : {Row::Presets, Row::SnapControls, Row::Actions}) {
                        const int count = row == Row::Presets ? 3 : 2;
                        const auto bounds = layout.rows[static_cast<size_t>(row)];
                        Rectangle previous{};
                        for (int i = 0; i < count; ++i) {
                            const auto button = game::SectorEditorPreviewAdjustmentButtonRect(bounds, i, count, stacked);
                            Check(button.height >= controlHeight && !Overlaps(previous, button)
                                          && button.x + button.width <= bounds.x + bounds.width + 0.001f
                                          && button.y + button.height <= bounds.y + bounds.height + 0.001f,
                                  "adjustment button draw and hit rectangles fit horizontal or stacked rows");
                            previous = button;
                        }
                    }
                }
            }
        }
    }
}

void TestMaterialBrowserFilterLayout()
{
    for (float width : {280.0f, 300.0f, 420.0f}) {
        const Rectangle bounds{20.0f, 66.0f, width, 710.0f};
        const auto layout = game::MeasureSectorMaterialBrowser(bounds);
        Check(!Overlaps(layout.filter, layout.list) && !Overlaps(layout.list, layout.add)
                && !Overlaps(layout.list, layout.remove) && !Overlaps(layout.add, layout.remove),
                "material filter, list, and action buttons remain separate");
        Check(layout.add.y + layout.add.height <= bounds.y + bounds.height
                && layout.remove.x + layout.remove.width <= bounds.x + bounds.width,
                "material browser actions fit at narrow pane widths");
        for (float rowHeight : {32.0f, 48.0f}) {
            const float viewport = layout.list.height - 20.0f;
            const float offset = game::SectorMaterialSelectionScrollOffset(0.0f, 100, rowHeight, viewport);
            Check(offset <= 100.0f * rowHeight && offset + viewport >= 101.0f * rowHeight,
                    "restoring a distant material scrolls its entire row into view");
            Check(Near(game::SectorMaterialSelectionScrollOffset(offset, 0, rowHeight, viewport), 0.0f),
                    "restoring an earlier material scrolls upward");
            Check(Near(game::SectorMaterialSelectionScrollOffset(20.0f, 2, rowHeight, viewport), 20.0f),
                    "already visible selection leaves scroll unchanged");
        }
    }
}

void TestMaterialFormMacroLayout()
{
    using Row = game::SectorMaterialFormRow;
    for (float width : {280.0f, 600.0f, 900.0f}) {
        for (float labelHeight : {24.0f, 36.0f}) {
            for (bool expanded : {false, true}) {
                const auto layout = game::MeasureSectorMaterialForm(width, 230.0f, labelHeight,
                        expanded, 240.0f, 120.0f, 180.0f);
                float bottom = 0.0f;
                for (const auto& row : layout.rows) {
                    if (row.height == 0.0f) continue;
                    Check(row.y >= bottom && row.width <= width, "material form rows fit without overlap");
                    bottom = row.y + row.height;
                }
                Check(layout.contentHeight >= bottom + 12.0f, "material form scroll extent includes preview and padding");
                Check((layout.fieldOffsetY > 0.0f) == (width < 402.0f), "narrow material forms stack labels above fields");
                const auto& macro = layout.rows[static_cast<std::size_t>(Row::MacroRepeat)];
                Check((macro.height > 0.0f) == expanded, "macro rows occupy space only when expanded");
                const auto& preview = layout.rows[static_cast<std::size_t>(Row::PreviewImage)];
                Check(preview.y + preview.height <= layout.contentHeight, "final material preview is reachable");
            }
        }
    }
}

void TestWrappedDiagnosticHeight()
{
    engine::AssetManager assets;
    engine::UIConfig config;
    // Exercise the missing-font path without a window or user-owned assets.
    const auto height = [&](const char* text, float width = 420.0f) {
        return engine::MeasureWrappedTextHeight(
                config, assets, width, engine::NullFontHandle(), text);
    };
    Check(Near(height(""), 0.0f) && Near(height(nullptr), 0.0f),
            "empty menu status reserves no text rows");
    Check(Near(height("failure", 0.0f), 0.0f),
            "collapsed status width has no drawable text");
    const char* diagnostic = "hub.lua:227: 'then' expected near 'playMapSound'\n"
            "stack traceback:\nLua syntax error [map=hub]";
    Check(height(diagnostic) > 56.0f
                    && height(diagnostic) >= config.paddingY * 2.0f
                            + config.fontSize * 3.0f,
            "multiline diagnostics reserve all rows instead of the old 56-pixel box");
    Check(height("failure\n\ndetails") > height("failure\ndetails"),
            "blank diagnostic lines contribute to scroll extent");
    const std::string longDiagnostic(5000, 'x');
    Check(height(longDiagnostic.c_str()) >= config.fontSize * 3.0f,
            "long diagnostic lines include the renderer's buffer-length wraps");
}

void TestMainMenuShortcutMatching()
{
    const engine::UIMenuShortcut save{KEY_S, true, false, false};
    Check(engine::MatchesUIMenuShortcut(
                  save, KEY_S, true, false, false),
          "menu shortcut matches its exact Ctrl key chord");
    Check(!engine::MatchesUIMenuShortcut(
                  save, KEY_S, true, true, false),
          "menu shortcut rejects an extra Shift modifier");
    Check(!engine::MatchesUIMenuShortcut(
                  save, KEY_S, false, false, false),
          "menu shortcut requires its Ctrl modifier");
    Check(!engine::MatchesUIMenuShortcut(
                  save, KEY_O, true, false, false),
          "menu shortcut rejects a different key");

    const engine::UIMenuShortcut adjust =
            game::SectorEditorAdjustSelectedShortcut();
    Check(engine::MatchesUIMenuShortcut(
                  adjust, KEY_A, true, false, false),
          "Adjust selected uses the exact Ctrl+A chord");
    Check(!game::CanBeginSectorEditorPreviewAdjustment(
                  game::SectorEditorMode::Edit2D, true, false)
                  && game::CanBeginSectorEditorPreviewAdjustment(
                          game::SectorEditorMode::Preview3D, true, false)
                  && !game::CanBeginSectorEditorPreviewAdjustment(
                          game::SectorEditorMode::Preview3D, true, true)
                  && !game::CanBeginSectorEditorPreviewAdjustment(
                          game::SectorEditorMode::Preview3D, false, false),
          "Adjust selected is disabled in 2D and requires an idle supported 3D selection");
}

void TestKeyboardPanModifierPolicy()
{
    Check(game::ShouldApplySectorEditorKeyboardPan(false, false, false),
          "2D keyboard navigation remains enabled without Ctrl");
    Check(!game::ShouldApplySectorEditorKeyboardPan(false, true, false),
          "left Ctrl suppresses 2D keyboard navigation");
    Check(!game::ShouldApplySectorEditorKeyboardPan(false, false, true),
          "right Ctrl suppresses 2D keyboard navigation");
    Check(!game::ShouldApplySectorEditorKeyboardPan(true, false, false),
          "keyboard capture continues to suppress 2D keyboard navigation");
}

void TestLightmapBakeSetupModalStateLifecycle()
{
    game::SectorLightmapBakeSetupModalState state;
    state.errorMessage = "old error";
    game::OpenSectorEditorLightmapBakeSetupModal(
            state,
            game::SectorLightmapBakeQualityPreset::High);
    Check(state.open
                  && state.selectedQuality
                             == game::SectorLightmapBakeQualityPreset::High
                  && state.errorMessage.empty(),
          "lightmap bake setup opens on the level's current preset");

    game::CloseSectorEditorLightmapBakeSetupModal(state);
    Check(!state.open
                  && state.selectedQuality
                             == game::SectorLightmapBakeQualityPreset::Standard
                  && state.errorMessage.empty(),
          "cancelling lightmap bake setup clears draft modal state");
}

bool Contains(Rectangle outer, Rectangle inner)
{
    return inner.x >= outer.x
            && inner.y >= outer.y
            && inner.x + inner.width <= outer.x + outer.width
            && inner.y + inner.height <= outer.y + outer.height;
}

void TestModelFilenameExtraction()
{
    Check(game::SectorEditorModelFilename(
                  "assets/models/props/medical_cart.glb")
                    == "medical_cart.glb",
          "model filename removes nested asset directories");
    Check(game::SectorEditorModelFilename("crate.gltf") == "crate.gltf",
          "model filename preserves a bare filename and extension");
    Check(game::SectorEditorModelFilename(
                  "assets\\models\\characters\\guard.glb")
                    == "guard.glb",
          "model filename accepts backslash-separated paths");
    Check(game::SectorEditorModelFilename("").empty(),
          "empty model path produces an empty filename");
}

void TestAudioAssetPickerSession()
{
    game::SectorEditorAudioAssetPickerSessionState session;
    game::SectorEditorAudioAssetPickerState firstPicker;
    firstPicker.open = true;
    firstPicker.paths = {"sfx/door.wav", "sfx/window.wav"};
    firstPicker.selectedPathIndex = 1;
    firstPicker.browsing.scroll.offset = Vector2{0.0f, 144.0f};
    std::snprintf(firstPicker.browsing.filterBuffer,
            sizeof(firstPicker.browsing.filterBuffer), "%s", "sfx/");
    game::RememberSectorEditorAudioAssetPickerSession(firstPicker, session);

    game::SectorEditorAudioAssetPickerState reopenedPicker;
    game::RestoreSectorEditorAudioAssetPickerSession(reopenedPicker, session);
    Check(Near(reopenedPicker.browsing.scroll.offset.y, 144.0f)
                    && std::string(reopenedPicker.browsing.filterBuffer) == "sfx/"
                    && reopenedPicker.browsing.selectedPath == "sfx/window.wav",
          "audio picker restores filter, selection, and scroll from memory");
    reopenedPicker.open = true;
    reopenedPicker.allPaths = firstPicker.paths;
    game::RebuildSectorEditorAudioAssetPickerOptions(reopenedPicker, "sfx/door.wav");
    Check(reopenedPicker.selectedPathIndex == 1,
          "reopening prefers the remembered file over the caller's current file");

    reopenedPicker.browsing.scroll.offset.y = 48.0f;
    game::RememberSectorEditorAudioAssetPickerSession(reopenedPicker, session);
    game::SectorEditorAudioAssetPickerState otherPicker;
    game::RememberSectorEditorAudioAssetPickerSession(otherPicker, session);
    game::RestoreSectorEditorAudioAssetPickerSession(otherPicker, session);
    Check(Near(otherPicker.browsing.scroll.offset.y, 48.0f)
                    && otherPicker.browsing.selectedPath == "sfx/window.wav"
                    && std::string(otherPicker.browsing.filterBuffer) == "sfx/",
          "other audio pickers share browsing state; inactive close cannot overwrite it");

    std::snprintf(reopenedPicker.browsing.filterBuffer,
            sizeof(reopenedPicker.browsing.filterBuffer), "%s", "no matches");
    game::RebuildSectorEditorAudioAssetPickerOptions(reopenedPicker);
    game::RememberSectorEditorAudioAssetPickerSession(reopenedPicker, session);
    game::RestoreSectorEditorAudioAssetPickerSession(otherPicker, session);
    otherPicker.allPaths = firstPicker.paths;
    game::RebuildSectorEditorAudioAssetPickerOptions(otherPicker);
    Check(otherPicker.paths.empty() && otherPicker.selectedPathIndex == -1
                    && otherPicker.browsing.selectedPath == "sfx/window.wav"
                    && std::string(otherPicker.browsing.filterBuffer) == "no matches",
          "closing and reopening empty results retains the filter and remembered path");
    otherPicker.browsing.filterBuffer[0] = '\0';
    game::RebuildSectorEditorAudioAssetPickerOptions(otherPicker);
    Check(otherPicker.selectedPathIndex == 1,
          "clearing an empty filter result restores the remembered selection");
    Check(game::SectorEditorAudioAssetPickerSessionState{}.selectedPath.empty(),
          "a fresh app session starts without a remembered audio file");
}

void TestAudioAssetPickerFiltering()
{
    game::SectorEditorAudioAssetPickerState state;
    state.allPaths = {"ambience/wind.ogg", "sfx/DOOR.WAV", "sfx/window.wav"};
    game::RebuildSectorEditorAudioAssetPickerOptions(state, "sfx/DOOR.WAV");
    Check(state.paths == state.allPaths && state.selectedPathIndex == 1,
          "empty audio filter preserves scan order and selects the current file initially");

    std::snprintf(state.browsing.filterBuffer, sizeof(state.browsing.filterBuffer),
            "%s", "SfX/");
    Check(!game::RebuildSectorEditorAudioAssetPickerOptions(state)
                    && state.paths.size() == 2 && state.selectedPathIndex == 0,
          "folder filtering is case insensitive and a changed index alone does not stop preview");
    std::snprintf(state.browsing.filterBuffer, sizeof(state.browsing.filterBuffer),
            "%s", "window.WaV");
    Check(game::RebuildSectorEditorAudioAssetPickerOptions(state)
                    && state.paths.size() == 1 && state.selectedPathIndex == 0
                    && state.paths[0] == "sfx/window.wav",
          "filename and extension filtering reports a changed file even at the same row index");
    std::snprintf(state.browsing.filterBuffer, sizeof(state.browsing.filterBuffer),
            "%s", ".OGG");
    Check(game::RebuildSectorEditorAudioAssetPickerOptions(state)
                    && state.paths.size() == 1 && state.paths[0] == "ambience/wind.ogg",
          "extension filtering falls back to the first match when selection is hidden");
    state.browsing.filterBuffer[0] = '\0';
    game::RebuildSectorEditorAudioAssetPickerOptions(state);
    Check(state.paths == state.allPaths && state.selectedPathIndex == 0,
          "clearing the audio filter restores the full list");
    for (size_t i = 0; i < state.paths.size(); ++i) {
        Check(state.optionLabels[i] == state.paths[i].c_str(),
              "audio option labels refer to the rebuilt visible paths");
    }

    std::snprintf(state.browsing.filterBuffer, sizeof(state.browsing.filterBuffer),
            "%s", "missing");
    Check(game::RebuildSectorEditorAudioAssetPickerOptions(state)
                    && state.selectedPathIndex == -1 && state.optionLabels.empty()
                    && state.filterMessage == "No audio files match the filter",
          "zero matches clear selection and signal that preview must stop");
    state.allPaths.clear();
    state.scanMessage = "assets/audio was not found";
    game::RebuildSectorEditorAudioAssetPickerOptions(state);
    Check(state.filterMessage.empty() && state.scanMessage == "assets/audio was not found",
          "scan errors remain distinct from a filter with zero matches");

    state.browsing.selectedPath = "deleted.wav";
    state.browsing.filterBuffer[0] = '\0';
    state.allPaths = {"sfx/door.wav", "sfx/window.wav"};
    game::RebuildSectorEditorAudioAssetPickerOptions(state, "sfx/window.wav");
    Check(state.selectedPathIndex == 1,
          "a removed remembered file falls back to the caller's current path");
}

void TestTextureRowWithoutClear()
{
    const game::SectorEditorInspectorTextureRowLayout layout =
            game::BuildSectorEditorInspectorTextureRowLayout(12.0f, 260.0f, 8.0f, 38.0f, 0.0f);

    Check(Near(layout.pickerButtonRect.x, 222.0f), "picker button is right aligned");
    Check(Near(layout.labelRect.width, 214.0f), "label leaves a gap before picker");
    Check(layout.clearButtonRect.width == 0.0f, "missing clear button has zero width");
    Check(layout.valueRect.y > layout.labelRect.y + layout.labelRect.height,
          "texture value is on its own line");
    Check(!Overlaps(layout.valueRect, layout.pickerButtonRect),
          "texture value line does not overlap picker button");
}

void TestTextureRowWithClear()
{
    const game::SectorEditorInspectorTextureRowLayout layout =
            game::BuildSectorEditorInspectorTextureRowLayout(0.0f, 260.0f, 8.0f, 38.0f, 92.0f);

    Check(Near(layout.clearButtonRect.x, 122.0f), "clear button sits before picker");
    Check(Near(layout.pickerButtonRect.x, 222.0f), "picker remains right aligned with clear button");
    Check(!Overlaps(layout.labelRect, layout.clearButtonRect), "label does not overlap clear button");
    Check(!Overlaps(layout.clearButtonRect, layout.pickerButtonRect), "clear button does not overlap picker");
    Check(layout.valueRect.width > layout.labelRect.width, "texture value line has readable width");
}

void TestCompactNumericRow()
{
    const game::SectorEditorInspectorNumericRowLayout narrow =
            game::BuildSectorEditorInspectorCompactNumericRowLayout(4.0f, 150.0f, 40.0f);
    const game::SectorEditorInspectorNumericRowLayout wide =
            game::BuildSectorEditorInspectorCompactNumericRowLayout(4.0f, 320.0f, 40.0f);

    Check(Near(narrow.inputRect.x + narrow.inputRect.width, 150.0f),
          "compact numeric input clamps to narrow content width");
    Check(Near(wide.inputRect.width, game::SectorEditorInspectorCompactInputWidth),
          "compact numeric input keeps fixed width when space allows");
    Check(!Overlaps(wide.labelRect, wide.inputRect), "compact numeric label does not overlap input");
}

void TestRightFloatNumericRow()
{
    const game::SectorEditorInspectorNumericRowLayout layout =
            game::BuildSectorEditorInspectorRightFloatRowLayout(8.0f, 260.0f, 36.0f, 8.0f);

    Check(Near(layout.inputRect.width, game::SectorEditorInspectorFloatInputWidth),
          "right float numeric input keeps fixed width when space allows");
    Check(Near(layout.inputRect.x + layout.inputRect.width, 260.0f),
          "right float numeric input is right aligned");
    Check(!Overlaps(layout.labelRect, layout.inputRect),
          "right float numeric label does not overlap input");
    Check(layout.labelRect.width > 120.0f,
          "right float numeric label has room for long labels");
}

void TestRightIntNumericRow()
{
    const game::SectorEditorInspectorNumericRowLayout layout =
            game::BuildSectorEditorInspectorRightIntRowLayout(8.0f, 260.0f, 36.0f, 8.0f);

    Check(Near(layout.inputRect.width, game::SectorEditorInspectorIntInputWidth),
          "right int numeric input keeps fixed width when space allows");
    Check(Near(layout.inputRect.x + layout.inputRect.width, 260.0f),
          "right int numeric input is right aligned");
    Check(!Overlaps(layout.labelRect, layout.inputRect),
          "right int numeric label does not overlap input");
}

void TestRightNumericRowClamps()
{
    const game::SectorEditorInspectorNumericRowLayout layout =
            game::BuildSectorEditorInspectorRightFloatRowLayout(8.0f, 72.0f, 36.0f, 8.0f);

    Check(Near(layout.inputRect.x, 0.0f), "right numeric input clamps to narrow content x");
    Check(Near(layout.inputRect.width, 72.0f), "right numeric input clamps to narrow content width");
    Check(!Overlaps(layout.labelRect, layout.inputRect), "clamped right numeric label does not overlap input");
}

void TestInspectorNumericWidthsMatchControlKinds()
{
    const game::SectorEditorInspectorNumericRowLayout floatLayout =
            game::BuildSectorEditorInspectorRightFloatRowLayout(
                    0.0f, 320.0f, 40.0f, 8.0f);
    const game::SectorEditorInspectorNumericRowLayout intLayout =
            game::BuildSectorEditorInspectorRightIntRowLayout(
                    0.0f, 320.0f, 40.0f, 8.0f);

    Check(Near(floatLayout.inputRect.width, 112.0f),
          "prop float fields use the compact fixed input width");
    Check(floatLayout.labelRect.width >= 200.0f,
          "prop float rows leave room for transform labels");
    Check(Near(intLayout.inputRect.width, 150.0f),
          "integer steppers reserve enough width for buttons and value text");
    Check(intLayout.inputRect.width > floatLayout.inputRect.width,
          "integer steppers are wider than float fields");
}

void TestFogVolumeInspectorLayoutIncludesConditionalRows()
{
    constexpr float rowH = 40.0f;
    constexpr float gap = 8.0f;
    game::SectorAuthoringFogVolume volume;

    const float fogStyleRowHeight =
            game::SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap;
    Check(Near(
                  game::MeasureSectorEditorAuthoringFogVolumeInspectorContentHeight(
                          volume, rowH, gap),
                  38.0f + 21.0f * (rowH + gap) + 2.0f * fogStyleRowHeight),
          "ellipsoid fog inspector includes style and path controls");

    volume.shape = game::SectorLocalFogShape::Box;
    Check(Near(
                  game::MeasureSectorEditorAuthoringFogVolumeInspectorContentHeight(
                          volume, rowH, gap),
                  38.0f + 22.0f * (rowH + gap) + 2.0f * fogStyleRowHeight),
          "box fog inspector includes instance ID, style, yaw, and reaches the delete row");
    Check(Near(game::MeasureSectorEditorAuthoringFogVolumeInspectorContentHeight(
                       volume, rowH, gap, 96.0f),
                  game::MeasureSectorEditorAuthoringFogVolumeInspectorContentHeight(
                       volume, rowH, gap) + 96.0f + gap),
          "wrapped fog ID validation errors contribute their full height");
    const auto idLayout = game::BuildSectorEditorInspectorStackedOptionRowLayout(
            0.0f, 220.0f, rowH, gap);
    Check(idLayout.fieldRect.y >= idLayout.labelRect.y + idLayout.labelRect.height
                  && idLayout.fieldRect.x + idLayout.fieldRect.width <= 220.0f,
          "fog instance ID uses full width below its label in narrow panes");

    const game::SectorEditorInspectorNumericRowLayout rgbLayout =
            game::BuildSectorEditorInspectorRightRgb8RowLayout(
                    0.0f, 320.0f, rowH, gap);
    Check(Near(rgbLayout.inputRect.width, game::SectorEditorInspectorRgb8InputWidth)
                  && rgbLayout.inputRect.width > game::SectorEditorInspectorIntInputWidth,
          "fog RGB steppers reserve extra width for three-digit values");
}

void TestTextureRowHeight()
{
    Check(Near(game::SectorEditorInspectorTextureRowHeight(), 60.0f),
          "texture row height accounts for action and value lines");
}

void TestStackedOptionRow()
{
    const game::SectorEditorInspectorStackedOptionRowLayout layout =
            game::BuildSectorEditorInspectorStackedOptionRowLayout(12.0f, 260.0f, 40.0f, 8.0f);

    Check(Near(layout.labelRect.x, 0.0f), "stacked option label starts at content x");
    Check(Near(layout.labelRect.width, 260.0f), "stacked option label is full width");
    Check(Near(layout.fieldRect.y, layout.labelRect.y + layout.labelRect.height + 8.0f),
          "stacked option field is below label with gap");
    Check(Near(layout.fieldRect.width, 260.0f), "stacked option field is full width");
    Check(!Overlaps(layout.labelRect, layout.fieldRect), "stacked option label does not overlap field");
    Check(Near(layout.height, 74.0f), "stacked option height accounts for label gap and field");
}

void TestRuntimeObjectInspectorHeightCountsBillboardRows()
{
    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float spriteLabelHeight = 54.0f;
    const float aspectWarningHeight = 28.0f;
    const float unsupportedHeight = game::SectorEditorRuntimeObjectInspectorContentHeight(
            rowH,
            gap,
            false,
            false,
            false,
            spriteLabelHeight,
            aspectWarningHeight);
    const float singleClipHeight = game::SectorEditorRuntimeObjectInspectorContentHeight(
            rowH,
            gap,
            true,
            false,
            false,
            spriteLabelHeight,
            aspectWarningHeight);
    const float directionalHeight = game::SectorEditorRuntimeObjectInspectorContentHeight(
            rowH,
            gap,
            true,
            false,
            true,
            spriteLabelHeight,
            aspectWarningHeight);
    const float warningHeight = game::SectorEditorRuntimeObjectInspectorContentHeight(
            rowH,
            gap,
            true,
            true,
            true,
            spriteLabelHeight,
            aspectWarningHeight);

    Check(singleClipHeight > unsupportedHeight,
          "billboard inspector height includes billboard controls");
    Check(Near(directionalHeight - singleClipHeight,
               (game::SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap) * 3.0f),
          "directional billboard height includes three extra stacked clip rows");
    Check(Near(warningHeight - directionalHeight, aspectWarningHeight + gap),
          "aspect warning height includes text row and trailing gap");
}

void TestDoorInspectorHeightCountsConditionalRows()
{
    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float anchorStatusHeight = 44.0f;
    const float assetStatusHeight = 20.0f;
    const float modelDiagnosticHeight = 92.0f;
    const float proceduralSlideHeight = game::SectorEditorDoorInspectorContentHeight(
            rowH,
            gap,
            anchorStatusHeight,
            assetStatusHeight,
            0.0f,
            false,
            false);
    const float proceduralSwingHeight = game::SectorEditorDoorInspectorContentHeight(
            rowH,
            gap,
            anchorStatusHeight,
            assetStatusHeight,
            0.0f,
            false,
            true);
    const float modelSwingHeight = game::SectorEditorDoorInspectorContentHeight(
            rowH,
            gap,
            anchorStatusHeight,
            assetStatusHeight,
            modelDiagnosticHeight,
            true,
            true);
    const float stacked =
            game::SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap;
    const float scriptRowsAndValidation =
            (rowH + gap) * 5.0f + 36.0f; // Includes Item drop target.
    const float expectedProceduralSlideHeight =
            38.0f + 34.0f
            + scriptRowsAndValidation
            + anchorStatusHeight + gap
            + (rowH + gap) * 4.0f
            + stacked
            + (rowH + gap) + stacked + (rowH + gap) * 2.0f
            + (rowH + gap) * 5.0f
            + assetStatusHeight + gap
            + (rowH + gap) * 5.0f;
    Check(Near(proceduralSlideHeight, expectedProceduralSlideHeight),
          "door inspector height includes script fields, validation space, target dimensions, normal offset, and height offset rows");
    Check(Near(proceduralSwingHeight - proceduralSlideHeight, stacked * 2.0f),
          "procedural swing inspector reserves two additional stacked hinge/side rows without clipping later controls");
    Check(modelSwingHeight > proceduralSwingHeight
                  && modelSwingHeight >= modelDiagnosticHeight + stacked * 5.0f,
          "model swing inspector reserves style, fit, diagnostics, and swing controls without overlapping the final actions");
}

void TestDoorTextureSettingsModalLayoutDoesNotOverlap()
{
    const Rectangle modal{100.0f, 80.0f, 680.0f, 600.0f};
    const game::SectorEditorDoorTextureSettingsModalLayout layout =
            game::BuildSectorEditorDoorTextureSettingsModalLayout(modal, 26.0f, 10.0f);

    Check(Contains(modal, layout.titleRect), "door texture modal title fits inside modal");
    Check(Contains(modal, layout.statusRect), "door texture modal status fits inside modal");
    Check(Contains(modal, layout.doneButtonRect), "door texture modal done button fits inside modal");

    for (int i = 0; i < 6; ++i) {
        Check(Contains(modal, layout.faceButtonRects[i]), "door texture modal face button fits inside modal");
        Check(Contains(modal, layout.actionButtonRects[i]), "door texture modal action button fits inside modal");
        for (int j = i + 1; j < 6; ++j) {
            Check(!Overlaps(layout.faceButtonRects[i], layout.faceButtonRects[j]),
                  "door texture modal face buttons do not overlap");
            Check(!Overlaps(layout.actionButtonRects[i], layout.actionButtonRects[j]),
                  "door texture modal action buttons do not overlap");
        }
    }

    for (int i = 0; i < 4; ++i) {
        Check(Contains(modal, layout.uvLabelRects[i]), "door texture modal uv label fits inside modal");
        Check(Contains(modal, layout.uvInputRects[i]), "door texture modal uv input fits inside modal");
        Check(!Overlaps(layout.uvLabelRects[i], layout.uvInputRects[i]),
              "door texture modal uv label does not overlap input");
        for (int j = i + 1; j < 4; ++j) {
            Check(!Overlaps(layout.uvInputRects[i], layout.uvInputRects[j]),
                  "door texture modal uv inputs do not overlap");
        }
        for (int j = 0; j < 6; ++j) {
            Check(!Overlaps(layout.faceButtonRects[j], layout.uvInputRects[i]),
                  "door texture modal face buttons do not overlap uv inputs");
            Check(!Overlaps(layout.actionButtonRects[j], layout.uvInputRects[i]),
                  "door texture modal action buttons do not overlap uv inputs");
        }
    }

    for (int i = 0; i < 6; ++i) {
        Check(!Overlaps(layout.actionButtonRects[i], layout.statusRect),
              "door texture modal action buttons do not overlap status");
        Check(!Overlaps(layout.actionButtonRects[i], layout.doneButtonRect),
              "door texture modal action buttons do not overlap done button");
    }
    Check(!Overlaps(layout.statusRect, layout.doneButtonRect),
          "door texture modal status does not overlap done button");
}

void TestPreviewSettingsModalCopiesObjectProbeSettings()
{
    game::SectorTopologyMap map;
    map.lightmapSettings.objectProbeSpacingWorld = 6.5f;
    map.lightmapSettings.objectProbeLowerHeightWorld = 0.75f;
    map.lightmapSettings.objectProbeUpperHeightWorld = 2.25f;

    game::SectorPreviewSettingsModalState modal;
    modal.draftLightmapSettings =
            game::NormalizeSectorPreviewObjectProbeSettings(map.lightmapSettings);

    Check(Near(modal.draftLightmapSettings.objectProbeSpacingWorld, 6.5f),
          "preview settings modal draft copies object probe spacing");
    Check(Near(modal.draftLightmapSettings.objectProbeLowerHeightWorld, 0.75f)
                  && Near(modal.draftLightmapSettings.objectProbeUpperHeightWorld, 2.25f),
          "preview settings modal draft copies layered object probe heights");
}

void TestPreviewSettingsScrollableContentHeightsReachLastControls()
{
    const float rowH = 40.0f;
    const float gap = 12.0f;
    const float lightingLastControlBottom =
            20.0f * (rowH + gap)
            + 5.0f * (8.0f + 38.0f)
            + 36.0f + gap;
    const float fogLastControlBottom =
            10.0f * (rowH + gap)
            + 36.0f + gap
            + 38.0f
            + 36.0f + gap;

    Check(Near(
                  game::MeasureSectorPreviewSettingsLightingContentHeight(rowH, gap),
                  lightingLastControlBottom + 12.0f),
          "lighting scroll reaches indirect bounce strength with bottom padding");
    Check(Near(
                  game::MeasureSectorPreviewSettingsFogContentHeight(rowH, gap),
                  fogLastControlBottom + 12.0f),
          "fog scroll reaches the complete color swatch with bottom padding");
}

void TestMainMenuWorkspaceAndToolsLayouts()
{
    const game::SectorEditorWorkspaceLayout layout =
            game::BuildSectorEditorWorkspaceLayout();
    Check(Near(layout.mainMenu.height, game::EditorMainMenuHeight),
          "main menu uses the reserved editor band height");
    Check(!Overlaps(layout.mainMenu, layout.leftPanel)
                  && !Overlaps(layout.mainMenu, layout.rightPanel)
                  && !Overlaps(layout.mainMenu, layout.canvas),
          "main menu band does not overlap 2D editor workspace regions");
    Check(layout.leftPanel.y == game::EditorMainMenuHeight
                  && layout.rightPanel.y == game::EditorMainMenuHeight
                  && layout.canvas.y > game::EditorMainMenuHeight,
          "2D editor workspace starts below the main menu");
    Check(Near(layout.bottomPanel.y + layout.bottomPanel.height,
                  game::EditorHeight),
          "bottom status panel remains anchored to the viewport bottom");

    for (float width : {180.0f, 240.0f, 320.0f}) {
        for (float row : {24.0f, 46.0f}) {
            game::CameraEditingUiState cameraUi;
            const float normal = game::MeasureSectorEditorCameraInspectorContentHeight(cameraUi, row, 8);
            cameraUi.referenceIdError = "ID already exists";
            const float expanded = game::MeasureSectorEditorCameraInspectorContentHeight(cameraUi, row, 8);
            Check(Near(expanded - normal, row * 2 + 8), "camera inspector budgets its validation message");
            const auto field = game::BuildSectorEditorCameraFieldLayout(38, width, row, 8);
            Check(field.labelRect.y + field.labelRect.height < field.inputRect.y
                    && field.inputRect.width == width, "camera labels stack above fields at narrow pane sizes");
            const float deleteY = 38 + (std::size(game::SectorEditorCameraFields) + 1)
                    * game::SectorEditorCameraFieldHeight(row, 8);
            Check(normal >= deleteY + row + 8, "camera inspector fully exposes Delete with bottom padding");
        }
    }
    const float rowH = 46.0f;
    const float gap = 10.0f;
    const float collapsed = game::MeasureSectorEditorToolsContentHeight(
            rowH, gap, false);
    const float expanded = game::MeasureSectorEditorToolsContentHeight(
            rowH, gap, true);
    const float itemExpanded = game::MeasureSectorEditorToolsContentHeight(
            rowH, gap, false, true);
    Check(Near(expanded - collapsed, rowH + gap),
          "tools content height includes the conditional Trigger mode row");
    Check(Near(itemExpanded - collapsed, rowH + gap),
          "tools content height includes the conditional Item definition row");
    Check(Near(collapsed, 26.0f + 5.0f * (rowH + gap)
                  + 22.0f + 26.0f + 23.0f * (rowH + gap)
                  + 22.0f + 26.0f + gap + 2.0f * (rowH + gap)
                  + 22.0f + (rowH + gap) + 12.0f),
          "tools content height reaches the final Grid control with padding");
}

void TestLevelSettingsAppliesCompleteLightmapDraft()
{
    game::SectorTopologyMap map;
    map.lightmapSettings.qualityPreset =
            game::SectorLightmapBakeQualityPreset::High;
    const std::string originalHash = game::ComputeSectorLightmapSourceHash(map);

    game::SectorLightmapBakeSettings draft = map.lightmapSettings;
    draft.qualityPreset = game::SectorLightmapBakeQualityPreset::Draft;
    draft.ambientOcclusionRadius = game::SectorWorldToAuthoringDistance(2.0f);
    draft.ambientOcclusionStrength = 0.25f;
    draft.indirectBounceRadius = game::SectorWorldToAuthoringDistance(6.0f);
    draft.indirectBounceStrength = 0.45f;
    draft.objectProbeSpacingWorld = 5.0f;
    draft.objectProbeLowerHeightWorld = 0.8f;
    draft.objectProbeUpperHeightWorld = 1.8f;

    Check(game::ApplySectorLevelLightmapSettings(map, draft),
          "Level Settings applies changed AO, bounce, and probe fields");
    Check(map.lightmapSettings.qualityPreset
                  == game::SectorLightmapBakeQualityPreset::High,
          "Level Settings preserves the bake quality preset");
    Check(Near(map.lightmapSettings.ambientOcclusionStrength, 0.25f)
                  && Near(map.lightmapSettings.indirectBounceStrength, 0.45f)
                  && Near(map.lightmapSettings.objectProbeSpacingWorld, 5.0f),
          "Level Settings writes every displayed lightmap field");
    Check(game::ComputeSectorLightmapSourceHash(map) != originalHash,
          "Level Settings lightmap changes invalidate the lightmap source hash");

    game::SectorPreviewSettingsModalState modal;
    modal.draftLightmapSettings = map.lightmapSettings;
    game::ResetSectorPreviewSettingsModalLightingDefaults(modal);
    Check(modal.draftLightmapSettings.qualityPreset
                  == game::SectorLightmapBakeQualityPreset::High,
          "Lighting defaults preserve the hidden bake quality preset");
    Check(Near(modal.draftLightmapSettings.ambientOcclusionStrength, 0.55f)
                  && Near(modal.draftLightmapSettings.indirectBounceStrength, 0.20f),
          "Lighting defaults reset the moved lightmap controls");
}

void TestAuthoringFaceInspectorHeightIncludesAllSections()
{
    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float anchorSummaryHeight = 28.0f;
    game::SectorAuthoringFaceAnchor anchor;

    const float height =
            game::MeasureSectorEditorAuthoringFaceInspectorContentHeight(
                    anchor,
                    rowH,
                    gap,
                    anchorSummaryHeight);
    Check(Near(height, 1814.0f),
          "authoring face height includes crawlspace, liquid, roomtone, merge, ceiling sky, audio, materials, decals, and padding");

    anchor.roomtone.fadeMilliseconds = 500;
    const float overriddenFadeHeight =
            game::MeasureSectorEditorAuthoringFaceInspectorContentHeight(
                    anchor, rowH, gap, anchorSummaryHeight);
    Check(Near(overriddenFadeHeight - height, rowH + gap),
          "authoring face height includes an overridden roomtone fade row");
    anchor.roomtone.fadeMilliseconds =
            game::SectorRoomtoneSettings::UseMapFadeMilliseconds;

    anchor.floorDecal.materialId = "floor_decal";
    anchor.floorDecal.emissive = true;
    const float assignedDecalHeight =
            game::MeasureSectorEditorAuthoringFaceInspectorContentHeight(
                    anchor,
                    rowH,
                    gap,
                    anchorSummaryHeight);
    Check(Near(assignedDecalHeight - height, 232.0f),
          "authoring face height includes expanded emissive flat decal controls");
}

void TestPreviewSettingsModalResetPreservesSessionView()
{
    game::SectorPreviewSettingsModalState modal;
    modal.open = true;
    modal.activeTab = game::PreviewSettingsTab::Lighting;
    modal.generalScroll.offset.y = 11.0f;
    modal.skyScroll.offset.y = 22.0f;
    modal.lightingScroll.offset.y = 33.0f;
    modal.fogScroll.offset.y = 44.0f;
    modal.draftConfig.walkSpeed = 123.0f;
    modal.draftNpcToNpcCollisionEnabled = false;
    modal.errorMessage = "discard me";

    game::ResetSectorPreviewSettingsModalPreservingView(modal);

    Check(!modal.open, "preview settings reset closes modal");
    Check(modal.activeTab == game::PreviewSettingsTab::Lighting,
          "preview settings reset preserves active tab for the session");
    Check(Near(modal.generalScroll.offset.y, 11.0f)
                  && Near(modal.skyScroll.offset.y, 22.0f)
                  && Near(modal.lightingScroll.offset.y, 33.0f)
                  && Near(modal.fogScroll.offset.y, 44.0f),
          "preview settings reset preserves every remaining tab scroll offset");
    Check(modal.errorMessage.empty()
                  && !Near(modal.draftConfig.walkSpeed, 123.0f)
                  && modal.draftNpcToNpcCollisionEnabled,
          "preview settings reset discards transient drafts and errors");
}

void TestPreviewSettingsModalAppliesObjectProbeSettingsAndChangesHash()
{
    game::SectorTopologyMap map;
    const std::string originalHash = game::ComputeSectorLightmapSourceHash(map);

    game::SectorLightmapBakeSettings draft = map.lightmapSettings;
    draft.objectProbeSpacingWorld = 5.5f;
    draft.objectProbeLowerHeightWorld = 0.7f;
    draft.objectProbeUpperHeightWorld = 1.6f;

    const bool changed = game::ApplySectorPreviewObjectProbeSettings(map, draft);

    Check(changed, "preview settings modal apply reports changed object probe settings");
    Check(Near(map.lightmapSettings.objectProbeSpacingWorld, 5.5f),
          "preview settings modal apply writes object probe spacing");
    Check(Near(map.lightmapSettings.objectProbeLowerHeightWorld, 0.7f)
                  && Near(map.lightmapSettings.objectProbeUpperHeightWorld, 1.6f),
          "preview settings modal apply writes layered object probe heights");
    Check(game::ComputeSectorLightmapSourceHash(map) != originalHash,
          "object probe settings update changes lightmap source hash");
}

void TestPreviewSettingsModalResetsObjectProbeDefaults()
{
    game::SectorPreviewSettingsModalState modal;
    modal.draftLightmapSettings.objectProbeSpacingWorld = 9.0f;
    modal.draftLightmapSettings.objectProbeLowerHeightWorld = 3.0f;
    modal.draftLightmapSettings.objectProbeUpperHeightWorld = 0.2f;

    game::ResetSectorPreviewSettingsModalLightingDefaults(modal);

    Check(Near(modal.draftLightmapSettings.objectProbeSpacingWorld, 4.0f),
          "preview settings modal reset restores default object probe spacing");
    Check(Near(modal.draftLightmapSettings.objectProbeLowerHeightWorld, 0.6f)
                  && Near(modal.draftLightmapSettings.objectProbeUpperHeightWorld, 1.5f),
          "preview settings modal reset restores layered object probe height defaults");
}

void TestPreviewSettingsModalNormalizesLayeredProbeSettings()
{
    game::SectorLightmapBakeSettings settings;
    settings.objectProbeSpacingWorld = 0.0f;
    settings.objectProbeLowerHeightWorld = 20.0f;
    settings.objectProbeUpperHeightWorld = -3.0f;

    const game::SectorLightmapBakeSettings normalized =
            game::NormalizeSectorPreviewObjectProbeSettings(settings);
    Check(Near(normalized.objectProbeSpacingWorld, 0.25f),
          "preview settings clamps object probe spacing");
    Check(Near(normalized.objectProbeLowerHeightWorld, 0.0f)
                  && Near(normalized.objectProbeUpperHeightWorld, 16.0f),
          "preview settings clamps and orders layered object probe heights");
}

void TestPreviewSettingsFogTabLayout()
{
    const Rectangle modal{510.0f, 190.0f, 900.0f, 700.0f};
    const std::array<Rectangle, 4> tabs =
            game::BuildSectorPreviewSettingsTabLayout(modal, modal.y + 76.0f, 38.0f);
    for (size_t i = 0; i < tabs.size(); ++i) {
        Check(Contains(modal, tabs[i]), "preview settings tab fits inside expanded modal");
        for (size_t j = i + 1; j < tabs.size(); ++j) {
            Check(!Overlaps(tabs[i], tabs[j]), "preview settings tabs do not overlap");
        }
    }
    Check(tabs[3].x + tabs[3].width <= modal.x + modal.width - 30.0f,
          "fog tab preserves the modal right margin");
}

void TestWeaponEditorLayouts()
{
    const Rectangle viewport{0.0f, 0.0f, 1920.0f, 1080.0f};
    for (bool preview3D : {false, true}) {
        const game::SectorEditorWeaponEditorLayout layout =
                game::BuildSectorEditorWeaponEditorLayoutForViewport(
                        viewport.width, viewport.height, preview3D);
        Check(Contains(viewport, layout.panel),
              "weapon editor panel fits inside the viewport");
        Check(Contains(layout.panel, layout.listPane)
                      && Contains(layout.panel, layout.formBounds),
              "weapon editor list and form fit inside the panel");
        Check(!Overlaps(layout.listPane, layout.formBounds),
              "weapon editor list and form do not overlap");
        Check(!Overlaps(layout.addButton, layout.duplicateButton)
                      && !Overlaps(layout.saveButton, layout.cancelButton),
              "weapon editor action buttons do not overlap");
        Check(Contains(layout.panel, layout.validationMessage),
              "weapon editor validation message fits inside the panel");
        if (preview3D) {
            Check(Contains(layout.panel, layout.previewFireButton)
                          && Contains(layout.panel, layout.previewReloadButton)
                          && Contains(layout.panel, layout.holsterToggleButton),
                  "weapon preview actions fit inside the panel");
            Check(!Overlaps(layout.formBounds, layout.previewFireButton)
                          && !Overlaps(layout.formBounds, layout.previewReloadButton)
                          && !Overlaps(layout.formBounds, layout.holsterToggleButton),
                  "weapon preview actions stay outside the form scroll area");
            Check(!Overlaps(layout.previewFireButton, layout.previewReloadButton)
                          && !Overlaps(layout.previewReloadButton, layout.holsterToggleButton)
                          && !Overlaps(layout.holsterToggleButton, layout.saveButton)
                          && !Overlaps(layout.validationMessage, layout.previewFireButton)
                          && !Overlaps(layout.validationMessage, layout.previewReloadButton)
                          && !Overlaps(layout.validationMessage, layout.holsterToggleButton),
                  "weapon editor footer controls and validation message do not overlap");
        }
    }
}

void TestItemEditorLayouts()
{
    const game::SectorEditorItemEditorLayout layout =
            game::BuildSectorEditorItemEditorLayoutForViewport(1920.0f, 1080.0f);
    Check(layout.panel.width > 0.0f && layout.panel.height > 0.0f,
          "item editor panel has positive dimensions");
    Check(layout.listBounds.x + layout.listBounds.width
                    < layout.formBounds.x,
          "item editor keeps list and detail panes separate");
    Check(layout.saveButton.x + layout.saveButton.width
                    < layout.cancelButton.x + layout.cancelButton.width,
          "item editor footer actions are ordered");
    const game::SectorEditorItemEditorLayout compact =
            game::BuildSectorEditorItemEditorLayoutForViewport(1000.0f, 720.0f);
    Check(compact.panel.x >= 16.0f && compact.panel.y >= 16.0f,
          "item editor respects compact viewport margins");
}

void TestNpcEditorModalSplitPaneLayout()
{
    const game::SectorEditorNpcEditorModalLayout layout =
            game::BuildSectorEditorNpcEditorModalLayoutForViewport(1920.0f, 1080.0f);
    const Rectangle viewport{0.0f, 0.0f, 1920.0f, 1080.0f};
    Check(Contains(viewport, layout.modal),
          "NPC editor modal fits inside the editor viewport");
    Check(Contains(layout.modal, layout.listPane)
                  && Contains(layout.modal, layout.formBounds),
          "NPC list and form panes stay inside the modal");
    Check(!Overlaps(layout.listPane, layout.formBounds),
          "NPC list and form panes do not overlap");
    Check(!Overlaps(layout.addButton, layout.deleteButton)
                  && Contains(layout.listPane, layout.addButton)
                  && Contains(layout.listPane, layout.deleteButton),
          "NPC Add and Delete controls fit without overlap");
    Check(!Overlaps(layout.saveButton, layout.cancelButton)
                  && Contains(layout.modal, layout.saveButton)
                  && Contains(layout.modal, layout.cancelButton),
          "NPC Save and Cancel controls fit without overlap");
}

void TestSoundEditorSplitPaneLayout()
{
    const game::SectorEditorSoundEditorLayout layout =
            game::BuildSectorEditorSoundEditorLayoutForViewport(1920.0f, 1080.0f);
    const Rectangle viewport{0.0f, 0.0f, 1920.0f, 1080.0f};
    Check(Contains(viewport, layout.modal),
          "Sound Editor modal fits inside the editor viewport");
    Check(Contains(layout.modal, layout.listPane)
                  && Contains(layout.modal, layout.formBounds),
          "Sound Editor list and details panes stay inside the modal");
    Check(!Overlaps(layout.listPane, layout.formBounds),
          "Sound Editor list and details panes do not overlap");
    Check(!Overlaps(layout.addButton, layout.deleteButton)
                  && Contains(layout.listPane, layout.addButton)
                  && Contains(layout.listPane, layout.deleteButton),
          "Sound Editor Add and Remove controls fit without overlap");
    Check(!Overlaps(layout.saveButton, layout.cancelButton)
                  && Contains(layout.modal, layout.saveButton)
                  && Contains(layout.modal, layout.cancelButton),
          "Sound Editor Save and Cancel controls fit without overlap");
}

void TestPatrolEditorSplitPaneLayout()
{
    const game::SectorEditorPatrolEditorLayout layout =
            game::BuildSectorEditorPatrolEditorLayoutForViewport(
                    1920.0f, 1080.0f);
    const Rectangle viewport{0.0f, 0.0f, 1920.0f, 1080.0f};
    Check(Contains(viewport, layout.modal),
          "Patrol Editor modal fits inside the editor viewport");
    Check(Contains(layout.modal, layout.listPane)
                  && Contains(layout.modal, layout.formBounds)
                  && !Overlaps(layout.listPane, layout.formBounds),
          "Patrol Editor list and details panes fit without overlap");
    Check(!Overlaps(layout.addButton, layout.deleteButton)
                  && !Overlaps(layout.saveButton, layout.cancelButton),
          "Patrol Editor action buttons fit without overlap");

    const Rectangle card = game::BuildSectorEditorPatrolWaypointCardDrawRect(
            Rectangle{100.0f, 200.0f, 900.0f, 600.0f},
            Vector2{10.0f, 25.0f},
            30.0f,
            880.0f,
            174.0f);
    Check(Near(card.x, 90.0f) && Near(card.y, 205.0f),
          "Patrol waypoint card backgrounds use the scroll-area draw transform");

    const game::SectorEditorPatrolWaypointRowLayout row =
            game::BuildSectorEditorPatrolWaypointRowLayout(
                    980.0f, 40.0f, 88.0f);
    Check(!Overlaps(row.markerLabel, row.markerInput)
                  && !Overlaps(row.delayLabel, row.delayInput)
                  && !Overlaps(row.delayInput, row.gaitInput)
                  && !Overlaps(row.gaitInput, row.lookCheckbox)
                  && !Overlaps(row.lookCheckbox, row.arcLabel)
                  && !Overlaps(row.arcLabel, row.arcInput),
          "Patrol waypoint labels and compact inputs do not overlap");
    Check(row.delayInput.width == 110.0f
                  && row.gaitInput.width == 110.0f
                  && row.arcInput.width == 120.0f,
          "Patrol waypoint numeric and gait fields stay compact");
}

void TestPreviewSettingsModalFogDraftApplyAndReset()
{
    game::SectorTopologyMap map;
    map.fogSettings.enabled = true;
    map.fogSettings.density = 0.2f;
    map.fogSettings.color = Color{10, 20, 30, 80};

    game::SectorPreviewSettingsModalState modal;
    modal.draftFogSettings = game::NormalizeSectorTopologyFogSettings(map.fogSettings);
    Check(modal.draftFogSettings.enabled
                  && Near(modal.draftFogSettings.density, 0.2f)
                  && modal.draftFogSettings.color.a == 255,
          "preview settings modal copies normalized fog settings");

    const std::string lightmapHash = game::ComputeSectorLightmapSourceHash(map);
    modal.draftFogSettings.density = 0.35f;
    modal.draftFogSettings.referenceHeightWorld = -3.0f;
    Check(game::ApplySectorPreviewFogSettings(map, modal.draftFogSettings),
          "preview settings modal applies changed fog settings");
    Check(Near(map.fogSettings.density, 0.35f)
                  && Near(map.fogSettings.referenceHeightWorld, -3.0f),
          "preview settings modal writes normalized fog settings");
    Check(!game::ApplySectorPreviewFogSettings(map, modal.draftFogSettings),
          "preview settings modal reports unchanged fog settings");

    Check(game::ComputeSectorLightmapSourceHash(map) == lightmapHash,
          "preview fog settings do not change the lightmap source hash");

    game::ResetSectorPreviewSettingsModalFogDefaults(modal);
    const game::SectorTopologyFogSettings defaults = game::DefaultSectorTopologyFogSettings();
    Check(modal.draftFogSettings.enabled == defaults.enabled
                  && Near(modal.draftFogSettings.density, defaults.density)
                  && Near(modal.draftFogSettings.heightFalloff, defaults.heightFalloff),
          "preview settings modal resets fog defaults");
}

void TestPreviewNavigationTabLayout()
{
    Check(game::SectorEditorPreviewDebugTabs.size() == 10,
          "preview debug strip contains ten array-defined tabs");
    const Rectangle panel{32.0f, 32.0f, 700.0f, 520.0f};
    Rectangle previous{};
    for (size_t index = 0; index < game::SectorEditorPreviewDebugTabs.size(); ++index) {
        const Rectangle tab = game::BuildSectorEditorPreviewDebugTabRect(
                panel, 10.0f, 26.0f, 6.0f, 30.0f, 6.0f, index);
        Check(Contains(panel, tab), "preview debug tab fits inside its panel");
        if (index > 0) Check(!Overlaps(previous, tab), "preview debug tabs do not overlap");
        previous = tab;
    }
    Check(game::SectorEditorPreviewOverlayExpandedHeight(
                  game::PreviewDebugOverlayTab::Navigation) >= 520.0f,
          "preview interaction bounds include Navigation controls and diagnostics");

    const game::SectorEditorPreviewLightStartActionLayout lightActions =
            game::BuildSectorEditorPreviewLightStartActionLayout(
                    panel,
                    10.0f,
                    40.0f,
                    true,
                    true);
    Check(Contains(panel, lightActions.pilot)
                  && Contains(panel, lightActions.halo)
                  && Contains(panel, lightActions.shaft),
          "preview light placement actions fit inside the panel");
    Check(!Overlaps(lightActions.pilot, lightActions.halo)
                  && !Overlaps(lightActions.halo, lightActions.shaft)
                  && !Overlaps(lightActions.pilot, lightActions.shaft),
          "Pilot, Place Halo, and Place Shaft actions do not overlap");
    Check(lightActions.reservedWidth >= 326.0f,
          "preview status reserves the full light action strip width");
}

void TestLightProxyPlacementMath()
{
    Vector3 intersection{};
    Check(game::IntersectSectorEditorLightProxyPlacementPlane(
                  Ray{Vector3{0.0f, 0.0f, 0.0f}, Vector3{0.0f, 0.0f, 1.0f}},
                  Vector3{0.0f, 0.0f, 5.0f},
                  Vector3{0.0f, 0.0f, 1.0f},
                  intersection)
                  && Near(intersection.z, 5.0f),
          "halo placement ray intersects its camera-facing drag plane");
    Check(!game::IntersectSectorEditorLightProxyPlacementPlane(
                  Ray{Vector3{}, Vector3{1.0f, 0.0f, 0.0f}},
                  Vector3{0.0f, 0.0f, 5.0f},
                  Vector3{0.0f, 0.0f, 1.0f},
                  intersection),
          "halo placement rejects rays parallel to the drag plane");

    const Vector3 dragged = game::ApplySectorEditorLightProxyPlacementDrag(
            Vector3{1.0f, 2.0f, 3.0f},
            Vector3{4.0f, 5.0f, 6.0f},
            Vector3{6.0f, 8.0f, 10.0f},
            false);
    const Vector3 preciseDragged = game::ApplySectorEditorLightProxyPlacementDrag(
            Vector3{1.0f, 2.0f, 3.0f},
            Vector3{4.0f, 5.0f, 6.0f},
            Vector3{6.0f, 8.0f, 10.0f},
            true);
    Check(Near(dragged.x, 3.0f) && Near(dragged.y, 5.0f) && Near(dragged.z, 7.0f),
          "halo drag applies the full camera-plane delta");
    Check(Near(preciseDragged.x, 1.2f)
                  && Near(preciseDragged.y, 2.3f)
                  && Near(preciseDragged.z, 3.4f),
          "halo drag precision mode applies one tenth of the delta");

    const Vector3 depth = game::ApplySectorEditorLightProxyPlacementDepth(
            Vector3{0.0f, 0.0f, 10.0f},
            Vector3{},
            Vector3{0.0f, 0.0f, 1.0f},
            1.0f,
            false);
    const Vector3 preciseDepth = game::ApplySectorEditorLightProxyPlacementDepth(
            Vector3{0.0f, 0.0f, 10.0f},
            Vector3{},
            Vector3{0.0f, 0.0f, 1.0f},
            1.0f,
            true);
    Check(Near(depth.z, 9.8f), "positive halo placement wheel motion moves toward the camera");
    Check(Near(preciseDepth.z, 9.98f), "halo depth precision mode applies one tenth of the step");
}

} // namespace

void TestBaseboardLayout()
{
    for (float width : {180.0f, 240.0f, 320.0f}) {
        const auto off = game::BuildSectorEditorBaseboardLayout(0, width, 36, 8, false);
        const auto on = game::BuildSectorEditorBaseboardLayout(0, width, 36, 8, true);
        Check(Near(off.height, 44), "disabled baseboard reserves only checkbox space");
        for (int i = 0; i < 2; ++i) {
            Check(!Overlaps(on.labels[i], on.inputs[i]), "baseboard labels never overlap dimension fields");
            Check(on.inputs[i].x + on.inputs[i].width <= width, "baseboard field fits narrow pane");
        }
        const auto row = game::BuildSectorEditorInspectorTextureRowLayout(on.materialY, width, 8, 38, 72);
        Check(row.pickerButtonRect.y + row.pickerButtonRect.height <= on.height,
                "expanded baseboard scroll extent includes final picker button");
        Check(on.materialY >= on.inputs[1].y + on.inputs[1].height, "material row follows dimensions");
    }
}

int main()
{
    TestGameSettingsLayout();
    TestPreviewObjectAdjustmentLayout();
    TestMaterialBrowserFilterLayout();
    TestMaterialFormMacroLayout();
    TestWrappedDiagnosticHeight();
    TestBaseboardLayout();
    TestMainMenuShortcutMatching();
    TestKeyboardPanModifierPolicy();
    TestLightmapBakeSetupModalStateLifecycle();
    TestModelFilenameExtraction();
    TestAudioAssetPickerSession();
    TestAudioAssetPickerFiltering();
    TestTextureRowWithoutClear();
    TestTextureRowWithClear();
    TestCompactNumericRow();
    TestRightFloatNumericRow();
    TestRightIntNumericRow();
    TestRightNumericRowClamps();
    TestInspectorNumericWidthsMatchControlKinds();
    TestFogVolumeInspectorLayoutIncludesConditionalRows();
    TestTextureRowHeight();
    TestStackedOptionRow();
    TestRuntimeObjectInspectorHeightCountsBillboardRows();
    TestDoorInspectorHeightCountsConditionalRows();
    TestDoorTextureSettingsModalLayoutDoesNotOverlap();
    TestPreviewSettingsModalCopiesObjectProbeSettings();
    TestPreviewSettingsScrollableContentHeightsReachLastControls();
    TestMainMenuWorkspaceAndToolsLayouts();
    TestLevelSettingsAppliesCompleteLightmapDraft();
    TestAuthoringFaceInspectorHeightIncludesAllSections();
    TestPreviewSettingsModalResetPreservesSessionView();
    TestPreviewSettingsModalAppliesObjectProbeSettingsAndChangesHash();
    TestPreviewSettingsModalResetsObjectProbeDefaults();
    TestPreviewSettingsModalNormalizesLayeredProbeSettings();
    TestPreviewSettingsFogTabLayout();
    TestNpcEditorModalSplitPaneLayout();
    TestSoundEditorSplitPaneLayout();
    TestPatrolEditorSplitPaneLayout();
    TestWeaponEditorLayouts();
    TestItemEditorLayouts();
    TestPreviewSettingsModalFogDraftApplyAndReset();
    TestPreviewNavigationTabLayout();
    TestLightProxyPlacementMath();

    if (failures != 0) {
        std::cerr << failures << " SectorEditorUiLayoutTests failure(s)\n";
        return 1;
    }
    return 0;
}
