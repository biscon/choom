#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"

#include <cstddef>

namespace engine {

struct UIDemoAssets {
    FontHandle font = NullFontHandle();
    TextureHandle image = NullTextureHandle();
};

struct UIDemoState {
    UIContext ui;
    UIConfig config;
    char textInput[64] = "Player";
    bool checkbox = true;
    float slider = 0.55f;
    int listSelected = 1;
    bool listMultiSelected[4] = {true, false, true, false};
    int optionSelected = 0;
    UIScrollState scrollDemo;
    bool scrollChecks[8] = {true, false, true, false, true, false, true, false};
    float scrollSlider = 0.35f;
    int buttonClicks = 0;
    float floorHeight = 0.0f;
    float ceilingHeight = 4.0f;
    float textureScale = 1.0f;
    int gridSize = 16;
    int subdivision = 3;
    UIFloatInputState floorHeightInput;
    UIFloatInputState ceilingHeightInput;
    UIFloatInputState textureScaleInput;
    UIIntInputState gridSizeInput;
    UIIntInputState subdivisionInput;
    int selectedTool = 0;
    bool seededInvalidNumericDemo = false;
};

void DrawUIDemo(
        UIDemoState& state,
        Input& input,
        AssetManager& assets,
        const UIDemoAssets& demoAssets,
        Rectangle bounds);

} // namespace engine
