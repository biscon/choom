#include "sector_editor/SectorEditorUiHelpers.h"

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

void TestTextureRowHeight()
{
    Check(Near(game::SectorEditorInspectorTextureRowHeight(), 60.0f),
          "texture row height accounts for action and value lines");
}

} // namespace

int main()
{
    TestTextureRowWithoutClear();
    TestTextureRowWithClear();
    TestCompactNumericRow();
    TestTextureRowHeight();

    if (failures != 0) {
        std::cerr << failures << " SectorEditorUiLayoutTests failure(s)\n";
        return 1;
    }
    return 0;
}
