#pragma once

#include "engine/input/Input.h"
#include "engine/scripting/ScriptData.h"

#include <filesystem>
#include <string>
#include <vector>

namespace game {

struct SectorDialogueOption { std::string id; std::string text; };
struct SectorDialogueSet { std::string id; std::vector<SectorDialogueOption> options; };
struct SectorDialogueLine { size_t begin = 0; size_t end = 0; };
struct SectorDialogueRow {
    float top = 0.0f;
    float height = 0.0f;
    size_t firstLine = 0;
    size_t lineCount = 0;
};

// Session-owned assets and transient presentation. Indices refer to the immutable
// registry, never to ECS storage. Loading and menu transitions reserve scratch.
struct SectorDialogueRuntime {
    std::vector<SectorDialogueSet> sets;
    std::vector<size_t> visible;
    std::vector<SectorDialogueRow> rows;
    std::vector<SectorDialogueLine> lines;
    size_t setIndex = 0;
    size_t selected = 0;
    engine::ScriptOperationHandle operation{};
    uint64_t token = 0;
    uint64_t nextToken = 1;
    bool active = false;
    bool layoutReady = false;
    bool ensureSelectionVisible = false;
    float scroll = 0.0f;
    float contentHeight = 0.0f;
    float lineHeight = 0.0f;
    float numberWidth = 0.0f;
    Rectangle panel{};
    Rectangle layoutViewport{};
    unsigned int fontTexture = 0;
    int fontSize = 0;
    Vector2 previousMouse{};
    // Swallow release/click events belonging to an advance/select press.
    unsigned int swallowedButtons = 0;
};

void LoadSectorDialogue(SectorDialogueRuntime& runtime, const std::filesystem::path& directory);
void ResetSectorDialogueMenu(SectorDialogueRuntime& runtime);
bool BeginSectorDialogue(SectorDialogueRuntime& runtime, const std::string& setId,
        const std::vector<std::string>& hidden, std::string& error);
bool SelectSectorDialogue(SectorDialogueRuntime& runtime, engine::ScriptRuntime& scripts, size_t visibleIndex);
void LayoutSectorDialogue(SectorDialogueRuntime& runtime, const Font& font,
        int pixelSize, Rectangle viewport, float preferredTop);
void DrawSectorDialogue(const SectorDialogueRuntime& runtime, const Font& font);
void UpdateSectorDialogueInput(SectorDialogueRuntime& runtime, engine::ScriptRuntime& scripts,
        engine::Input& input, Rectangle inputViewport = {});
// Returns true once per input pass; the caller completes the current speech.
bool ConsumeSectorSpeechAdvance(SectorDialogueRuntime& runtime, engine::Input& input, bool speechActive);

} // namespace game
