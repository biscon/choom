#pragma once

#include "engine/assets/AssetHandles.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace engine {

enum class DebugConsoleSeverity {
    Trace,
    Debug,
    Info,
    Success,
    Warning,
    Error,
    Fatal,
    Input,
    LuaResult
};

struct DebugConsoleLine {
    uint64_t sequence = 0;
    DebugConsoleSeverity severity = DebugConsoleSeverity::Info;
    std::string text;
};

struct DebugConsoleVisualLine {
    uint64_t sourceSequence = 0;
    int fragmentIndex = 0;
    DebugConsoleSeverity severity = DebugConsoleSeverity::Info;
    std::string prefix;
    std::string text;
};

struct DebugConsoleEditorVisualLine {
    int startCodepoint = 0;
    int endCodepoint = 0;
    std::string prefix;
    std::string text;
};

struct DebugConsoleEditor {
    std::vector<int> codepoints;
    int caret = 0;
    bool historyBrowsing = false;
    int historyIndex = -1;
    std::vector<int> historyDraft;
    int historyDraftCaret = 0;
    float caretBlinkMs = 0.0f;
    bool caretVisible = true;
    uint64_t revision = 1;
};

struct DebugConsoleStyle {
    float outerMargin = 16.0f;
    float topMargin = 64.0f;
    float heightFraction = 0.55f;
    float minHeight = 300.0f;
    float maxHeight = 720.0f;
    float contentPadding = 10.0f;
    float fontSize = 22.0f;
    float fontSpacing = 1.0f;
    float lineHeight = 25.0f;
    int maxVisibleInputLines = 3;
    int pageScrollLines = 20;
    float caretBlinkPeriodMs = 500.0f;
};

enum class DeferredDebugActionType {
    None,
    ReloadCurrentMap,
    SetGodMode,
    SetFreezeAi,
    SetDebugAi,
    QuitApplication
};

enum class DeferredDebugBooleanMode {
    Toggle,
    Enable,
    Disable
};

struct DeferredDebugAction {
    DeferredDebugActionType type = DeferredDebugActionType::None;
    std::string mapId;
    DeferredDebugBooleanMode booleanMode = DeferredDebugBooleanMode::Toggle;
};

struct DebugConsoleData {
    bool initialized = false;
    bool open = false;
    DebugConsoleEditor editor;
    std::vector<DebugConsoleLine> lines;
    std::vector<std::string> history;
    int scrollOffsetVisualLines = 0;
    bool followTail = true;
    size_t maxLogicalLines = 2000;
    size_t maxHistoryEntries = 200;
    size_t maxInputCodepoints = 4096;
    uint64_t nextSequence = 1;
    FontHandle font = NullFontHandle();
    DebugConsoleStyle style;
    bool layoutDirty = true;
    float cachedWrapWidth = 0.0f;
    std::vector<DebugConsoleVisualLine> visualLines;
    std::vector<DebugConsoleEditorVisualLine> editorVisualLines;
    uint64_t cachedEditorRevision = 0;
    float cachedEditorWrapWidth = 0.0f;
    int cachedCaretVisualLine = 0;
    float cachedCaretOffsetX = 0.0f;
    bool preserveScrollOnLayout = false;
    size_t anchorVisualLineCount = 0;
    DeferredDebugAction pendingAction;
    bool inputLimitWarningShown = false;
};

} // namespace engine
