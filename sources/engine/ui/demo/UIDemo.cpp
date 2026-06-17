#include "engine/ui/demo/UIDemo.h"

#include <algorithm>
#include <cstdio>

namespace engine {

namespace {

static constexpr uint32_t FnvOffsetBasis = 2166136261u;
static constexpr uint32_t FnvPrime = 16777619u;

Rectangle Row(float x, float y, float width, float height)
{
    return Rectangle{x, y, width, height};
}

uint32_t DemoHashId(const char* id)
{
    uint32_t hash = FnvOffsetBasis;
    if (id == nullptr) {
        return hash;
    }

    while (*id != '\0') {
        hash ^= static_cast<uint8_t>(*id);
        hash *= FnvPrime;
        ++id;
    }

    return hash == 0 ? 1u : hash;
}

} // namespace

void DrawUIDemo(
        UIDemoState& state,
        Input& input,
        AssetManager& assets,
        const UIDemoAssets& demoAssets,
        Rectangle bounds)
{
    BeginUI(state.ui, input);

    if (!state.seededInvalidNumericDemo) {
        state.seededInvalidNumericDemo = true;
        state.textureScaleInput.editing = true;
        state.textureScaleInput.editingSourceValue = state.textureScale;
        std::snprintf(state.textureScaleInput.buffer, sizeof(state.textureScaleInput.buffer), "-");
        state.ui.focusedId = DemoHashId("demo_texture_scale");
        state.ui.textCursorByteIndex = 1;
        state.ui.textCursorBlinkStartTime = GetTime();
    }

    DrawRectangleRounded(bounds, 0.04f, 12, state.config.panelColor);
    DrawRectangleRoundedLinesEx(
            bounds,
            0.04f,
            12,
            state.config.borderThickness,
            state.config.borderColor
    );

    const float x = bounds.x + 28.0f;
    float y = bounds.y + 24.0f;
    const float width = std::min(bounds.width - 56.0f, 504.0f);
    const float columnGap = 18.0f;
    const float columnWidth = (width - columnGap) * 0.5f;
    const float rowHeight = 48.0f;
    const float gap = 14.0f;
    const char* const listOptions[] = {
            "Scout",
            "Builder",
            "Medic",
            "Pilot"
    };
    const char* const optionChoices[] = {
            "Windowed",
            "Borderless",
            "Fullscreen",
            "Presentation"
    };

    Text(
            state.config,
            assets,
            Row(x, y, width, 42.0f),
            demoAssets.font,
            "Immediate UI Kitchen Sink",
            UITextJustify::Center
    );
    y += 58.0f;

    Text(
            state.config,
            assets,
            Row(x, y, width, rowHeight),
            demoAssets.font,
            "Left justified text",
            UITextJustify::Left
    );
    y += rowHeight;

    Text(
            state.config,
            assets,
            Row(x, y, width, rowHeight),
            demoAssets.font,
            "Centered text",
            UITextJustify::Center,
            state.config.mutedTextColor
    );
    y += rowHeight;

    Text(
            state.config,
            assets,
            Row(x, y, width, rowHeight),
            demoAssets.font,
            "Right justified text",
            UITextJustify::Right
    );
    y += rowHeight + gap;

    TextInput(
            state.ui,
            state.config,
            input,
            assets,
            "demo_text_input",
            Row(x, y, width, rowHeight),
            demoAssets.font,
            state.textInput,
            sizeof(state.textInput),
            3,
            24,
            UITextJustify::Left
    );
    y += rowHeight + gap;

    if (Button(
            state.ui,
            state.config,
            input,
            assets,
            "demo_button",
            Row(x, y, width, rowHeight),
            demoAssets.font,
            "Button",
            UITextJustify::Center)) {
        ++state.buttonClicks;
    }
    y += rowHeight + 6.0f;

    char clickText[64];
    std::snprintf(clickText, sizeof(clickText), "Button clicks: %d", state.buttonClicks);
    Text(
            state.config,
            assets,
            Row(x, y, width, 34.0f),
            demoAssets.font,
            clickText,
            UITextJustify::Center,
            state.config.mutedTextColor
    );
    y += 42.0f;

    Checkbox(
            state.ui,
            state.config,
            input,
            assets,
            "demo_checkbox",
            Row(x, y, width, rowHeight),
            demoAssets.font,
            "Checkbox",
            state.checkbox
    );
    y += rowHeight + gap;

    Slider(
            state.ui,
            state.config,
            input,
            "demo_slider",
            Row(x, y, width, rowHeight),
            0.0f,
            1.0f,
            state.slider
    );
    y += rowHeight + 6.0f;

    char sliderText[64];
    std::snprintf(sliderText, sizeof(sliderText), "Slider: %.2f", state.slider);
    Text(
            state.config,
            assets,
            Row(x, y, width, 34.0f),
            demoAssets.font,
            sliderText,
            UITextJustify::Center,
            state.config.mutedTextColor
    );
    y += 48.0f;

    List(
            state.ui,
            state.config,
            input,
            assets,
            "demo_single_list",
            Row(x, y, columnWidth, state.config.listItemHeight * 4.0f),
            demoAssets.font,
            listOptions,
            4,
            state.listSelected
    );

    List(
            state.ui,
            state.config,
            input,
            assets,
            "demo_multi_list",
            Row(x + columnWidth + columnGap, y, columnWidth, state.config.listItemHeight * 4.0f),
            demoAssets.font,
            listOptions,
            4,
            state.listMultiSelected
    );
    y += state.config.listItemHeight * 4.0f + gap;

    Option(
            state.ui,
            state.config,
            input,
            assets,
            "demo_option",
            Row(x, y, width, rowHeight),
            demoAssets.font,
            optionChoices,
            4,
            state.optionSelected
    );
    y += rowHeight + gap;

    Text(
            state.config,
            assets,
            Row(x, y, width, rowHeight),
            demoAssets.font,
            "This text sits below the dropdown.",
            UITextJustify::Center,
            state.config.mutedTextColor
    );
    y += rowHeight + gap;

    const Rectangle imageBounds = Row(x + (width - 108.0f) * 0.5f, y, 108.0f, 108.0f);
    Image(state.config, assets, imageBounds, demoAssets.image);

    if (bounds.width >= 920.0f) {
        const float panelX = x + width + 56.0f;
        const float panelWidth = bounds.x + bounds.width - panelX - 28.0f;
        const Rectangle panelBounds{panelX, bounds.y + 82.0f, panelWidth, 458.0f};

        UIPanelResult panel = BeginPanel(
                state.ui,
                state.config,
                assets,
                "demo_editor_widgets_panel",
                panelBounds,
                demoAssets.font,
                "Editor Widgets"
        );

        float panelY = panel.contentRect.y;
        const float panelRowHeight = 46.0f;
        const float toolGap = 8.0f;
        const float toolWidth = (panel.contentRect.width - toolGap * 3.0f) * 0.25f;
        const char* const toolLabels[] = {"Select", "Move", "Draw", "Erase"};

        for (int i = 0; i < 4; ++i) {
            if (ToolButton(
                    state.ui,
                    state.config,
                    input,
                    assets,
                    TextFormat("demo_tool_%d", i),
                    Rectangle{
                            panel.contentRect.x + static_cast<float>(i) * (toolWidth + toolGap),
                            panelY,
                            toolWidth,
                            panelRowHeight
                    },
                    demoAssets.font,
                    toolLabels[i],
                    state.selectedTool == i)) {
                state.selectedTool = i;
            }
        }
        panelY += panelRowHeight + state.config.rowSpacing;

        Separator(
                state.config,
                Rectangle{panel.contentRect.x, panelY, panel.contentRect.width, 12.0f}
        );
        panelY += 20.0f;

        UILabelFieldRowResult floorRow = LabelFieldRow(
                state.config,
                assets,
                Rectangle{panel.contentRect.x, panelY, panel.contentRect.width, panelRowHeight},
                demoAssets.font,
                "Floor",
                132.0f
        );
        FloatInput(
                state.ui,
                state.config,
                input,
                assets,
                "demo_floor_height",
                floorRow.fieldRect,
                demoAssets.font,
                state.floorHeight,
                state.floorHeightInput,
                -64.0f,
                64.0f,
                2
        );
        panelY += panelRowHeight + state.config.rowSpacing;

        UILabelFieldRowResult ceilingRow = LabelFieldRow(
                state.config,
                assets,
                Rectangle{panel.contentRect.x, panelY, panel.contentRect.width, panelRowHeight},
                demoAssets.font,
                "Ceiling",
                132.0f
        );
        FloatInput(
                state.ui,
                state.config,
                input,
                assets,
                "demo_ceiling_height",
                ceilingRow.fieldRect,
                demoAssets.font,
                state.ceilingHeight,
                state.ceilingHeightInput,
                -64.0f,
                64.0f,
                2
        );
        panelY += panelRowHeight + state.config.rowSpacing;

        UILabelFieldRowResult scaleRow = LabelFieldRow(
                state.config,
                assets,
                Rectangle{panel.contentRect.x, panelY, panel.contentRect.width, panelRowHeight},
                demoAssets.font,
                "Scale",
                132.0f
        );
        FloatInput(
                state.ui,
                state.config,
                input,
                assets,
                "demo_texture_scale",
                scaleRow.fieldRect,
                demoAssets.font,
                state.textureScale,
                state.textureScaleInput,
                0.1f,
                8.0f,
                2
        );
        panelY += panelRowHeight + state.config.rowSpacing;

        UILabelFieldRowResult gridRow = LabelFieldRow(
                state.config,
                assets,
                Rectangle{panel.contentRect.x, panelY, panel.contentRect.width, panelRowHeight},
                demoAssets.font,
                "Grid",
                132.0f
        );
        IntInput(
                state.ui,
                state.config,
                input,
                assets,
                "demo_grid_size",
                gridRow.fieldRect,
                demoAssets.font,
                state.gridSize,
                state.gridSizeInput,
                1,
                128,
                1
        );
        panelY += panelRowHeight + state.config.rowSpacing;

        UILabelFieldRowResult subdivisionRow = LabelFieldRow(
                state.config,
                assets,
                Rectangle{panel.contentRect.x, panelY, panel.contentRect.width, panelRowHeight},
                demoAssets.font,
                "Subdiv",
                132.0f
        );
        IntInput(
                state.ui,
                state.config,
                input,
                assets,
                "demo_subdivision",
                subdivisionRow.fieldRect,
                demoAssets.font,
                state.subdivision,
                state.subdivisionInput,
                0,
                16,
                1
        );
        panelY += panelRowHeight + state.config.rowSpacing;

        char valuesText[160];
        std::snprintf(
                valuesText,
                sizeof(valuesText),
                "Tool %s  floor %.2f  ceiling %.2f  grid %d",
                toolLabels[state.selectedTool],
                state.floorHeight,
                state.ceilingHeight,
                state.gridSize
        );
        Text(
                state.config,
                assets,
                Rectangle{panel.contentRect.x, panelY, panel.contentRect.width, 38.0f},
                demoAssets.font,
                valuesText,
                UITextJustify::Left,
                state.config.mutedTextColor
        );

        EndPanel(state.ui, state.config, panel);

        const float scrollX = panelX;
        const float scrollY = panelBounds.y + panelBounds.height + 52.0f;
        const float scrollWidth = panelWidth;
        const Rectangle scrollBounds{scrollX, scrollY, scrollWidth, 390.0f};
        const Vector2 scrollContentSize{scrollWidth + 260.0f, 780.0f};

        Text(
                state.config,
                assets,
                Rectangle{scrollX, panelBounds.y + panelBounds.height + 10.0f, scrollWidth, 42.0f},
                demoAssets.font,
                "ScrollArea Demo",
                UITextJustify::Center
        );

        UIScrollAreaResult scrollArea = BeginScrollArea(
                state.ui,
                state.config,
                input,
                "demo_scroll_area",
                scrollBounds,
                scrollContentSize,
                state.scrollDemo
        );

        Text(
                state.ui,
                state.config,
                assets,
                Rectangle{16.0f, 14.0f, 520.0f, 42.0f},
                demoAssets.font,
                "Mouse wheel, drag thumbs, click rows.",
                UITextJustify::Left,
                state.config.mutedTextColor
        );

        for (int i = 0; i < 8; ++i) {
            char label[64];
            std::snprintf(label, sizeof(label), "Scrollable checkbox %d", i + 1);
            Checkbox(
                    state.ui,
                    state.config,
                    input,
                    assets,
                    TextFormat("demo_scroll_check_%d", i),
                    Rectangle{16.0f, 72.0f + static_cast<float>(i) * 58.0f, 420.0f, 46.0f},
                    demoAssets.font,
                    label,
                    state.scrollChecks[i]
            );
        }

        Slider(
                state.ui,
                state.config,
                input,
                "demo_scroll_slider",
                Rectangle{16.0f, 560.0f, 640.0f, 46.0f},
                0.0f,
                1.0f,
                state.scrollSlider
        );

        Text(
                state.ui,
                state.config,
                assets,
                Rectangle{16.0f, 628.0f, 720.0f, 42.0f},
                demoAssets.font,
                "This wide row forces the horizontal scrollbar to matter.",
                UITextJustify::Left
        );

        Button(
                state.ui,
                state.config,
                input,
                assets,
                "demo_scroll_button",
                Rectangle{16.0f, 724.0f, 520.0f, 48.0f},
                demoAssets.font,
                "Button Inside ScrollArea",
                UITextJustify::Center
        );

        EndScrollArea(state.ui, state.config, input, scrollArea, state.scrollDemo);
    }

    EndUI(state.ui, state.config, input, assets);
}

} // namespace engine
