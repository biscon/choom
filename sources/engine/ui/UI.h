#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"

#include <cstddef>
#include <cstdint>

namespace engine {

enum class UITextJustify {
    Left,
    Center,
    Right
};

struct UIConfig {
    Color textColor = RAYWHITE;
    Color mutedTextColor = Color{170, 178, 190, 255};
    Color panelColor = Color{20, 24, 32, 220};
    Color borderColor = Color{76, 86, 106, 255};
    Color disabledColor = Color{86, 92, 105, 255};
    Color widgetColor = Color{35, 43, 57, 255};
    Color widgetHoverColor = Color{48, 59, 78, 255};
    Color widgetActiveColor = Color{64, 83, 112, 255};
    Color accentColor = Color{95, 179, 128, 255};
    Color selectedToolColor = Color{72, 132, 102, 255};
    Color panelHeaderColor = Color{28, 34, 45, 235};
    Color invalidColor = Color{220, 88, 88, 255};
    Color placeholderColor = Color{210, 70, 210, 180};

    float fontSize = 28.0f;
    float textSpacing = 1.0f;
    float paddingX = 14.0f;
    float paddingY = 8.0f;
    float borderThickness = 2.0f;
    float cornerRadius = 0.12f;
    int cornerSegments = 8;
    float checkboxMarkPadding = 6.0f;
    float sliderTrackHeight = 6.0f;
    float sliderHandleWidth = 18.0f;
    float caretWidth = 2.0f;
    float caretBlinkInterval = 0.5f;
    float listItemHeight = 42.0f;
    float scrollbarSize = 14.0f;
    float scrollbarMinThumbSize = 24.0f;
    float mouseWheelScrollAmount = 48.0f;
    float panelHeaderHeight = 44.0f;
    float rowSpacing = 10.0f;
};

struct UIScrollState {
    Vector2 offset = {};
};

struct UIScrollAreaResult {
    Rectangle bounds = {};
    Rectangle viewport = {};
    Vector2 contentSize = {};
    bool scrollX = false;
    bool scrollY = false;
    bool drawFrame = true;
};

struct UIContext {
    struct OptionOverlay {
        bool active = false;
        uint32_t id = 0;
        Rectangle fieldBounds = {};
        Rectangle dropdownBounds = {};
        FontHandle font = NullFontHandle();
        const char* const* options = nullptr;
        size_t optionCount = 0;
        int* selectedIndex = nullptr;
    };

    uint32_t hotId = 0;
    uint32_t activeId = 0;
    uint32_t focusedId = 0;
    uint32_t openOptionId = 0;
    uint32_t activeScrollId = 0;
    int scrollDragAxis = 0;
    Vector2 scrollDragMouseStart = {};
    Vector2 scrollDragOffsetStart = {};
    size_t textCursorByteIndex = 0;
    double textCursorBlinkStartTime = 0.0;
    Vector2 mousePosition = {};
    bool mouseDown = false;
    bool inScrollArea = false;
    uint32_t scrollAreaId = 0;
    Rectangle scrollViewport = {};
    Vector2 scrollOffset = {};
    OptionOverlay optionOverlay;
};

struct UITextInputResult {
    bool changed = false;
    bool submitted = false;
    bool valid = false;
};

struct UIFloatInputState {
    char buffer[64] = {};
    bool editing = false;
    float editingSourceValue = 0.0f;
};

struct UIIntInputState {
    char buffer[32] = {};
    bool editing = false;
    int editingSourceValue = 0;
};

struct UINumericInputResult {
    bool changed = false;
    bool submitted = false;
    bool valid = true;
};

struct UIPanelResult {
    Rectangle contentRect = {};
};

struct UILabelFieldRowResult {
    Rectangle labelRect = {};
    Rectangle fieldRect = {};
};

void BeginUI(UIContext& ui, Input& input);
void EndUI(UIContext& ui, Input& input);
void EndUI(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets);

UIScrollAreaResult BeginScrollArea(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        const char* id,
        Rectangle bounds,
        Vector2 contentSize,
        UIScrollState& state,
        bool drawFrame = true);

void EndScrollArea(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        const UIScrollAreaResult& scrollArea,
        UIScrollState& state);

void Text(
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        UITextJustify justify = UITextJustify::Left,
        Color tint = BLANK);

void Text(
        const UIContext& ui,
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        UITextJustify justify = UITextJustify::Left,
        Color tint = BLANK);

UITextInputResult TextInput(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        char* buffer,
        size_t bufferCapacity,
        size_t minCharacters,
        size_t maxCharacters,
        UITextJustify justify = UITextJustify::Left);

bool Button(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        UITextJustify justify = UITextJustify::Center);

bool ToolButton(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        bool selected);

bool Checkbox(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        bool& checked,
        UITextJustify justify = UITextJustify::Left);

bool Slider(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        const char* id,
        Rectangle bounds,
        float minValue,
        float maxValue,
        float& value);

UINumericInputResult FloatInput(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        float& value,
        UIFloatInputState& state,
        float minValue,
        float maxValue,
        int decimalPlaces);

UINumericInputResult IntInput(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        int& value,
        UIIntInputState& state,
        int minValue,
        int maxValue,
        int step);

bool List(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* const* options,
        size_t optionCount,
        int& selectedIndex);

bool List(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* const* options,
        size_t optionCount,
        bool* selected);

bool Option(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* const* options,
        size_t optionCount,
        int& selectedIndex);

UIPanelResult BeginPanel(
        UIContext& ui,
        const UIConfig& config,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* title);

void EndPanel(
        UIContext& ui,
        const UIConfig& config,
        const UIPanelResult& panel);

void Separator(
        const UIConfig& config,
        Rectangle bounds);

UILabelFieldRowResult LabelFieldRow(
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        FontHandle font,
        const char* label,
        float labelWidth);

void Image(
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        TextureHandle texture,
        Color tint = WHITE);

void Image(
        const UIContext& ui,
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        TextureHandle texture,
        Color tint = WHITE);

} // namespace engine
