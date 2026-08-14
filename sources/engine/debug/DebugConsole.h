#pragma once

#include "engine/debug/DebugConsoleData.h"

#include <string>
#include <string_view>
#include <vector>

namespace engine {

class AssetManager;
class Input;
struct ScriptRuntime;

void DebugConsoleInitialize(DebugConsoleData& console, FontHandle font);
void DebugConsoleShutdown(DebugConsoleData& console);

void DebugConsoleUpdate(
        DebugConsoleData& console,
        Input& input,
        ScriptRuntime* scripts,
        std::string_view currentMapId,
        bool available,
        float dtSeconds);

void DebugConsoleRender(
        DebugConsoleData& console,
        AssetManager& assets,
        int logicalWidth,
        int logicalHeight);

void DebugConsoleAddLine(
        DebugConsoleData& console,
        std::string_view text,
        DebugConsoleSeverity severity = DebugConsoleSeverity::Info);
void DebugConsoleClear(DebugConsoleData& console);

bool DebugConsoleIsOpen(const DebugConsoleData& console);
bool DebugConsoleCapturesGameplayInput(const DebugConsoleData& console);
DeferredDebugAction DebugConsoleTakeDeferredAction(DebugConsoleData& console);

std::string DebugConsoleEditorText(const DebugConsoleEditor& editor);
std::vector<int> DebugConsoleDecodeUtf8(
        std::string_view text,
        bool sanitizePaste,
        size_t maxCodepoints = static_cast<size_t>(-1));
void DebugConsoleEditorInsert(
        DebugConsoleData& console,
        const std::vector<int>& codepoints);
void DebugConsoleEditorInsertCodepoint(
        DebugConsoleData& console,
        int codepoint);
bool DebugConsoleTokenizeCommand(
        std::string_view text,
        std::vector<std::string>& outTokens,
        std::string& outError);

} // namespace engine
