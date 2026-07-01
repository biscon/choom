# Immediate UI

The engine UI is a small immediate-mode layer under `engine/ui`. It is intended
for debug tools, prototypes, menus, and simple game UI. Widgets are plain
function calls and use caller-owned state.

The UI uses `engine::Input` for pointer, keyboard, and text events, and
`engine::AssetManager` for fonts and textures. Widgets do not own raylib
resources.

## Core Rules

- Call `engine::BeginUI()` before issuing widgets for a frame.
- Call `engine::EndUI()` after the last widget.
- Run UI after `Input::PollRaylib()` and before gameplay systems consume input.
- Pass stable widget IDs for interactive widgets.
- Store text input buffers and widget values in caller-owned state.
- Use `FontHandle` for text and `TextureHandle` for images.
- Missing or not-yet-ready assets render fallback output instead of crashing.

## Frame Order

The UI should consume events before game systems read unhandled input.

```cpp
context.input.BeginFrame();
context.input.PollRaylib(dt);

engine::BeginUI(ui, context.input);

if (engine::Button(
        ui,
        uiConfig,
        context.input,
        context.assets,
        "pause_button",
        Rectangle{40.0f, 40.0f, 180.0f, 48.0f},
        uiFont,
        "Pause")) {
    paused = !paused;
}

engine::EndUI(ui, uiConfig, context.input, context.assets);

// Gameplay systems should read only unhandled events after this point.
```

Use the `EndUI(ui, config, input, assets)` overload when the frame may contain
dropdowns. It draws deferred UI overlays after regular widgets so dropdowns can
appear above widgets below them.

The rectangles are in the same coordinate space as the current raylib mouse
position. In the sample `Main.cpp`, mouse scaling maps the window back into the
internal render target size, so UI rectangles use internal coordinates.

## Configuration

`UIConfig` stores colors and dimensions used by widgets.

```cpp
engine::UIConfig config;
config.accentColor = Color{95, 179, 128, 255};
config.fontSize = 28.0f;
config.paddingX = 14.0f;
config.paddingY = 8.0f;
```

Keep one `UIContext` for the UI surface that should share hover, active, and
keyboard-focus state.

```cpp
engine::UIContext ui;
engine::UIConfig config;
```

## Fonts

Text widgets take any font loaded through the asset manager.

```cpp
engine::FontHandle uiFont = assets.RequestFont(
        assets.GlobalScope(),
        "ui_regular_28",
        ASSETS_PATH "fonts/IBMPlexSans-Regular.ttf",
        28,
        engine::FontLoad_BilinearFilter
);
```

Draw static text with left, center, or right justification.

```cpp
engine::Text(
        config,
        assets,
        Rectangle{40.0f, 40.0f, 300.0f, 42.0f},
        uiFont,
        "Centered",
        engine::UITextJustify::Center
);
```

Pass `wordWrap = true` as the final argument to wrap static text within the
widget bounds. Word wrap defaults to `false`. Wrapped text is top-aligned within
the bounds; non-wrapped text keeps the existing vertical centering behavior.

## Text Input

`TextInput()` edits a caller-owned UTF-8 buffer. The buffer capacity is in
bytes. The minimum and maximum values are character counts.

```cpp
char playerName[32] = "Player";

engine::UITextInputResult result = engine::TextInput(
        ui,
        config,
        input,
        assets,
        "player_name",
        Rectangle{40.0f, 96.0f, 300.0f, 48.0f},
        uiFont,
        playerName,
        sizeof(playerName),
        3,
        20,
        engine::UITextJustify::Left
);

if (result.submitted && result.valid) {
    StartGame(playerName);
}
```

The current version is single-line only. It supports typing at the current
cursor, Space insertion, Left/Right arrow cursor movement, Home/End movement,
Backspace to remove the character left of the cursor, Delete to remove the
character right of the cursor, Enter to submit, Escape to blur, and clicking
outside to blur. The cursor blinks while the field is focused. `result.valid`
is true only when the current character count is within the supplied minimum
and maximum.

## Numeric Inputs

`FloatInput()` and `IntInput()` edit caller-owned values through caller-owned
edit state. Keep the state next to the value it edits.

```cpp
float floorHeight = 0.0f;
engine::UIFloatInputState floorHeightInput;

engine::UINumericInputResult result = engine::FloatInput(
        ui,
        config,
        input,
        assets,
        "floor_height",
        Rectangle{40.0f, 160.0f, 220.0f, 48.0f},
        uiFont,
        floorHeight,
        floorHeightInput,
        -64.0f,
        64.0f,
        2
);
```

When not focused, a float input displays the current value formatted with the
requested decimal places. Clicking the field starts a text edit from the
current value. Float fields accept normal decimal syntax such as `-1`, `1`,
`1.5`, `.5`, and `0.25`. Enter commits valid input, Escape cancels and restores
the value from before editing, and clicking outside commits valid input. Invalid
text is drawn with the invalid color; clicking outside while invalid cancels.
Committed values are clamped to the supplied range.

`IntInput()` combines a central integer text field with decrement and increment
buttons inside the supplied rectangle.

```cpp
int gridSize = 16;
engine::UIIntInputState gridSizeInput;

if (engine::IntInput(
        ui,
        config,
        input,
        assets,
        "grid_size",
        Rectangle{40.0f, 224.0f, 280.0f, 48.0f},
        uiFont,
        gridSize,
        gridSizeInput,
        1,
        128,
        1).changed) {
    RebuildGrid(gridSize);
}
```

The integer field accepts signed integer text only. The `-` and `+` buttons
step by the supplied amount and clamp to the supplied range.

## Buttons And Checkbox

Buttons return `true` on the frame they are clicked.

```cpp
if (engine::Button(
        ui,
        config,
        input,
        assets,
        "apply_button",
        Rectangle{40.0f, 160.0f, 180.0f, 48.0f},
        uiFont,
        "Apply")) {
    ApplySettings();
}
```

Use `ToolButton()` for toolbar buttons that need a selected state.

```cpp
enum class Tool {
    Select,
    Draw
};

Tool currentTool = Tool::Select;

if (engine::ToolButton(
        ui,
        config,
        input,
        assets,
        "tool_select",
        Rectangle{40.0f, 224.0f, 120.0f, 44.0f},
        uiFont,
        "Select",
        currentTool == Tool::Select)) {
    currentTool = Tool::Select;
}
```

Checkboxes toggle a caller-owned bool and return `true` when the value changes.

```cpp
bool fullscreen = false;

if (engine::Checkbox(
        ui,
        config,
        input,
        assets,
        "fullscreen",
        Rectangle{40.0f, 224.0f, 260.0f, 48.0f},
        uiFont,
        "Fullscreen",
        fullscreen)) {
    SetFullscreenEnabled(fullscreen);
}
```

## Slider

`Slider()` edits a caller-owned float, clamps it to the supplied range, and
returns `true` when it changes.

```cpp
float musicVolume = 0.75f;

if (engine::Slider(
        ui,
        config,
        input,
        "music_volume",
        Rectangle{40.0f, 288.0f, 300.0f, 48.0f},
        0.0f,
        1.0f,
        musicVolume)) {
    SetMusicVolume(musicVolume);
}
```

## Lists

`List()` can be used as a single-select list by passing an integer selected
index. It returns `true` when the selected row changes.

```cpp
const char* const classes[] = {
        "Scout",
        "Builder",
        "Medic",
        "Pilot"
};

int selectedClass = 0;

if (engine::List(
        ui,
        config,
        input,
        assets,
        "class_list",
        Rectangle{40.0f, 352.0f, 260.0f, config.listItemHeight * 4.0f},
        uiFont,
        classes,
        4,
        selectedClass)) {
    SetPlayerClass(selectedClass);
}
```

For multi-select lists, pass a bool array with one entry per option.

```cpp
bool enabledClasses[4] = {true, false, true, false};

engine::List(
        ui,
        config,
        input,
        assets,
        "enabled_classes",
        Rectangle{320.0f, 352.0f, 260.0f, config.listItemHeight * 4.0f},
        uiFont,
        classes,
        4,
        enabledClasses
);
```

## Options

`Option()` is a dropdown combo box. One option is always selected when the
option list is not empty. When open, the dropdown is rendered by `EndUI()` above
widgets below it.

```cpp
const char* const displayModes[] = {
        "Windowed",
        "Borderless",
        "Fullscreen"
};

int selectedDisplayMode = 0;

if (engine::Option(
        ui,
        config,
        input,
        assets,
        "display_mode",
        Rectangle{40.0f, 536.0f, 300.0f, 48.0f},
        uiFont,
        displayModes,
        3,
        selectedDisplayMode)) {
    ApplyDisplayMode(selectedDisplayMode);
}
```

## Scroll Areas

`BeginScrollArea()` and `EndScrollArea()` create a clipped panel for child
widgets. Store `UIScrollState` in caller-owned state and pass the full content
size each frame. Widgets issued between begin/end use local content coordinates.
Use the `Text(ui, ...)` and `Image(ui, ...)` overloads for those widgets inside
a scroll area.

```cpp
engine::UIScrollState inventoryScroll;

engine::UIScrollAreaResult scrollArea = engine::BeginScrollArea(
        ui,
        config,
        input,
        "inventory_scroll",
        Rectangle{40.0f, 600.0f, 420.0f, 220.0f},
        Vector2{640.0f, 520.0f},
        inventoryScroll
);

for (int i = 0; i < 10; ++i) {
    engine::Button(
            ui,
            config,
            input,
            assets,
            TextFormat("item_%d", i),
            Rectangle{16.0f, 16.0f + static_cast<float>(i) * 54.0f, 360.0f, 44.0f},
            uiFont,
            TextFormat("Inventory Item %d", i + 1)
    );
}

engine::EndScrollArea(ui, config, input, scrollArea, inventoryScroll);
```

Vertical and horizontal scrollbars appear only when the content is larger than
the available viewport. The mouse wheel scrolls vertically while the pointer is
over the scroll area. Scrollbar thumbs can be dragged with the left mouse
button, and clicking a scrollbar track pages by one viewport.

Pass `false` for the optional `drawFrame` argument when a scroll area is used
inside an already-framed panel and should only provide clipping/scrolling.

Nested scroll areas are not supported. `Option()` can be displayed as a closed
field inside a scroll area, but dropdown overlays from inside a clipped scroll
area are not opened in this version.

## Panels And Property Rows

`BeginPanel()` draws a static panel background, border, and optional title
header. It returns an inset content rectangle for child layout. Panels are not
movable, dockable, or resizable windows.

```cpp
engine::UIPanelResult panel = engine::BeginPanel(
        ui,
        config,
        assets,
        "sector_panel",
        Rectangle{40.0f, 40.0f, 420.0f, 260.0f},
        uiFont,
        "Sector"
);

engine::UILabelFieldRowResult floorRow = engine::LabelFieldRow(
        config,
        assets,
        Rectangle{
                panel.contentRect.x,
                panel.contentRect.y,
                panel.contentRect.width,
                48.0f
        },
        uiFont,
        "Floor",
        120.0f
);

engine::FloatInput(
        ui,
        config,
        input,
        assets,
        "sector_floor",
        floorRow.fieldRect,
        uiFont,
        floorHeight,
        floorHeightInput,
        -64.0f,
        64.0f,
        2
);

engine::EndPanel(ui, config, panel);
```

`LabelFieldRow()` is non-interactive. It draws the label in the left portion of
the row and returns both the label rectangle and the remaining field rectangle.
Use `Separator()` to draw a simple horizontal divider between groups.

## Images

Images use texture assets owned by `AssetManager`.

```cpp
engine::TextureHandle portrait = assets.RequestTexture(
        assets.GlobalScope(),
        "portrait",
        ASSETS_PATH "images/portrait.png",
        engine::TextureLoad_BilinearFilter
);

engine::Image(
        config,
        assets,
        Rectangle{40.0f, 352.0f, 128.0f, 128.0f},
        portrait
);
```

If the texture is missing, failed, or still loading, the widget draws a
placeholder outline.
