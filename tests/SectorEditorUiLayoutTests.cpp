#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/SectorEditorPreviewSettingsModal.h"

#include <cmath>
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

void TestDoorInspectorHeightCountsCoreRows()
{
    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float anchorStatusHeight = 44.0f;
    const float textureStatusHeight = 20.0f;
    const float height = game::SectorEditorDoorInspectorContentHeight(
            rowH,
            gap,
            anchorStatusHeight,
            textureStatusHeight);
    const float expected =
            38.0f
            + 34.0f
            + anchorStatusHeight + gap
            + (rowH + gap) * 4.0f
            + game::SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap
            + (rowH + gap) * 3.0f
            + (rowH + gap) * 4.0f
            + textureStatusHeight + gap
            + rowH + gap
            + rowH + gap;

    Check(Near(height, expected),
          "door inspector height includes anchor, core fields, motion, interaction controls, texture status, picker, and delete");
}

void TestPreviewSettingsModalCopiesObjectProbeSettings()
{
    game::SectorTopologyMap map;
    map.lightmapSettings.objectProbeSpacingWorld = 6.5f;
    map.lightmapSettings.objectProbeHeightWorld = 2.25f;

    game::SectorPreviewSettingsModalState modal;
    modal.draftLightmapSettings =
            game::NormalizeSectorPreviewObjectProbeSettings(map.lightmapSettings);

    Check(Near(modal.draftLightmapSettings.objectProbeSpacingWorld, 6.5f),
          "preview settings modal draft copies object probe spacing");
    Check(Near(modal.draftLightmapSettings.objectProbeHeightWorld, 2.25f),
          "preview settings modal draft copies object probe height");
}

void TestPreviewSettingsModalAppliesObjectProbeSettingsAndChangesHash()
{
    game::SectorTopologyMap map;
    const std::string originalHash = game::ComputeSectorLightmapSourceHash(map);

    game::SectorLightmapBakeSettings draft = map.lightmapSettings;
    draft.objectProbeSpacingWorld = 5.5f;
    draft.objectProbeHeightWorld = 1.6f;

    const bool changed = game::ApplySectorPreviewObjectProbeSettings(map, draft);

    Check(changed, "preview settings modal apply reports changed object probe settings");
    Check(Near(map.lightmapSettings.objectProbeSpacingWorld, 5.5f),
          "preview settings modal apply writes object probe spacing");
    Check(Near(map.lightmapSettings.objectProbeHeightWorld, 1.6f),
          "preview settings modal apply writes object probe height");
    Check(game::ComputeSectorLightmapSourceHash(map) != originalHash,
          "object probe settings update changes lightmap source hash");
}

void TestPreviewSettingsModalResetsObjectProbeDefaults()
{
    game::SectorPreviewSettingsModalState modal;
    modal.draftLightmapSettings.objectProbeSpacingWorld = 9.0f;
    modal.draftLightmapSettings.objectProbeHeightWorld = 3.0f;

    game::ResetSectorPreviewSettingsModalLightingDefaults(modal);

    Check(Near(modal.draftLightmapSettings.objectProbeSpacingWorld, 4.0f),
          "preview settings modal reset restores default object probe spacing");
    Check(Near(modal.draftLightmapSettings.objectProbeHeightWorld, 1.2f),
          "preview settings modal reset restores default object probe height");
}

} // namespace

int main()
{
    TestTextureRowWithoutClear();
    TestTextureRowWithClear();
    TestCompactNumericRow();
    TestRightFloatNumericRow();
    TestRightIntNumericRow();
    TestRightNumericRowClamps();
    TestTextureRowHeight();
    TestStackedOptionRow();
    TestRuntimeObjectInspectorHeightCountsBillboardRows();
    TestDoorInspectorHeightCountsCoreRows();
    TestPreviewSettingsModalCopiesObjectProbeSettings();
    TestPreviewSettingsModalAppliesObjectProbeSettingsAndChangesHash();
    TestPreviewSettingsModalResetsObjectProbeDefaults();

    if (failures != 0) {
        std::cerr << failures << " SectorEditorUiLayoutTests failure(s)\n";
        return 1;
    }
    return 0;
}
