#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/scripting/ScriptData.h"
#include <raylib.h>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

struct lua_State;
namespace engine { class AssetManager; struct EngineContext; }
namespace game {
struct SectorScriptHost;

// Digits use actions 0..9; the remaining actions are fixed across all skins.
constexpr int KeypadBackspace = 10;
constexpr int KeypadSubmit = 11;
struct SectorKeypadButton { int action = 0; Rectangle rect{}; };
struct SectorKeypadSkin {
    std::string id;
    bool ready = false;
    float aspectRatio = 1024.0f/1920.0f;
    engine::TextureHandle image{}, indicatorImage{};
    Rectangle display{}, indicator{};
    Vector3 indicatorColor{1, 0, 0};
    Color displayColor{176, 211, 162, 255};
    std::array<SectorKeypadButton, 12> buttons{};
    std::array<engine::SoundHandle, 3> sounds{};
};
struct SectorKeypadRuntime {
    engine::AssetScopeHandle scope{};
    std::vector<SectorKeypadSkin> skins;
    // Concrete loaded sizes; rendering selects one that fits without scaling.
    std::array<engine::FontHandle, 6> fonts{};
    engine::FontHandle helpFont{};
    engine::ScriptTaskHandle owner{};
    engine::ScriptOperationHandle lifetime{}, inputOperation{};
    uint64_t token = 0, nextToken = 1;
    size_t skinIndex = 0;
    int digits = 4, length = 0, pressedAction = -1, hoverAction = -1;
    std::array<char, 9> entry{};
    Vector3 indicatorColor{};
    float errorSeconds = 0, pressedSeconds = 0;
    bool active = false, cancelRequested = false;
    unsigned int swallowedMouseButtons = 0;
    Rectangle panel{}, viewport{};
};

void LoadSectorKeypads(engine::AssetManager& assets, SectorKeypadRuntime& runtime,
        const std::filesystem::path& assetRoot);
void PrepareSectorKeypadAssets(const engine::AssetManager& assets, SectorKeypadRuntime& runtime);
void UnloadSectorKeypads(engine::AssetManager& assets, SectorKeypadRuntime& runtime);
void LayoutSectorKeypad(SectorKeypadRuntime& runtime, Rectangle viewport);
Rectangle SectorKeypadRect(Rectangle panel, Rectangle normalized);
int PickSectorKeypad(const SectorKeypadRuntime& runtime, Vector2 point);
// Returns true only for an explicit submission of a complete entry.
bool ApplySectorKeypadAction(SectorKeypadRuntime& runtime, int action);
void UpdateSectorKeypad(engine::EngineContext& context, SectorScriptHost& host,
        Rectangle inputViewport, float dt);
void DrawSectorKeypad(const engine::AssetManager& assets, const SectorKeypadRuntime& runtime);
void CancelSectorKeypad(SectorScriptHost& host, const char* reason);
void RegisterSectorKeypadBindings(lua_State* state);
} // namespace game
