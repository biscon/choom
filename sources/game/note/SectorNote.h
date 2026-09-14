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

constexpr float SectorNoteFadeSeconds = 0.35f;
enum class SectorNotePhase { Opening, Reading, Closing };
struct SectorNoteLine {
    size_t offset = 0;
    float y = 0;
    bool title = false;
};
struct SectorNoteRuntime {
    engine::AssetScopeHandle scope{};
    engine::TextureHandle paper{};
    std::array<engine::FontHandle, 8> fonts{};
    engine::ScriptTaskHandle owner{};
    engine::ScriptOperationHandle operation{};
    uint64_t token = 0, nextToken = 1;
    bool active = false, closeRequested = false, layoutDirty = true;
    SectorNotePhase phase = SectorNotePhase::Opening;
    float phaseSeconds = 0, opacity = 0, closingOpacity = 0;
    float scroll = 0, contentHeight = 0, maxScroll = 0;
    float bodySize = 0, titleSize = 0;
    size_t bodyFont = 0, titleFont = 0;
    unsigned int swallowedMouseButtons = 0;
    std::string title, body, wrappedText;
    std::vector<SectorNoteLine> lines;
    Rectangle viewport{}, panel{}, content{};
};

void LoadSectorNote(engine::AssetManager& assets, SectorNoteRuntime& runtime,
        const std::filesystem::path& assetRoot);
void UnloadSectorNote(engine::AssetManager& assets, SectorNoteRuntime& runtime);
// CPU layout helper also supports graphics-free tests using synthetic fonts.
void LayoutSectorNote(SectorNoteRuntime& runtime, Rectangle viewport,
        const Font& bodyFont, const Font& titleFont);
void UpdateSectorNote(engine::EngineContext& context, SectorScriptHost& host, float dt);
void DrawSectorNote(const engine::AssetManager& assets, SectorNoteRuntime& runtime,
        Rectangle viewport);
void CancelSectorNote(SectorScriptHost& host, const char* reason);
void RegisterSectorNoteBindings(lua_State* state);
} // namespace game
