#include "engine/debug/DebugConsole.h"

#include "engine/input/Input.h"
#include "engine/debug/DebugConsoleLogBridge.h"
#include "engine/scripting/ScriptConsole.h"
#include "engine/scripting/ScriptData.h"

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <limits>
#include <string>

namespace engine {
namespace {

constexpr int ReplacementCodepoint = 0xfffd;

bool IsUnicodeScalar(int codepoint)
{
    return codepoint >= 0 && codepoint <= 0x10ffff
            && !(codepoint >= 0xd800 && codepoint <= 0xdfff);
}

bool IsTextCodepoint(int codepoint)
{
    return IsUnicodeScalar(codepoint)
            && codepoint != 0x7f
            && !(codepoint >= 0 && codepoint < 0x20)
            && !(codepoint >= 0x80 && codepoint <= 0x9f);
}

void AppendUtf8(std::string& output, int codepoint)
{
    if (!IsUnicodeScalar(codepoint)) codepoint = ReplacementCodepoint;
    if (codepoint <= 0x7f) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

std::string EncodeUtf8(const std::vector<int>& codepoints, int begin, int end)
{
    std::string result;
    begin = std::clamp(begin, 0, static_cast<int>(codepoints.size()));
    end = std::clamp(end, begin, static_cast<int>(codepoints.size()));
    result.reserve(static_cast<size_t>(end - begin));
    for (int i = begin; i < end; ++i) AppendUtf8(result, codepoints[i]);
    return result;
}

void ResetCaretBlink(DebugConsoleEditor& editor)
{
    editor.caretBlinkMs = 0.0f;
    editor.caretVisible = true;
}

void ExitHistoryBrowse(DebugConsoleEditor& editor)
{
    editor.historyBrowsing = false;
    editor.historyIndex = -1;
    editor.historyDraft.clear();
    editor.historyDraftCaret = 0;
}

std::string_view TrimView(std::string_view value)
{
    while (!value.empty()
            && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty()
            && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

void SetEditorText(DebugConsoleEditor& editor, std::string_view text)
{
    editor.codepoints = DebugConsoleDecodeUtf8(text, false);
    editor.caret = static_cast<int>(editor.codepoints.size());
    ++editor.revision;
    ResetCaretBlink(editor);
}

void BrowseHistory(DebugConsoleData& console, int direction)
{
    DebugConsoleEditor& editor = console.editor;
    if (console.history.empty()) return;
    if (!editor.historyBrowsing) {
        if (direction > 0) return;
        editor.historyBrowsing = true;
        editor.historyDraft = editor.codepoints;
        editor.historyDraftCaret = editor.caret;
        editor.historyIndex = static_cast<int>(console.history.size()) - 1;
    } else {
        editor.historyIndex += direction;
        if (editor.historyIndex < 0) editor.historyIndex = 0;
        if (editor.historyIndex >= static_cast<int>(console.history.size())) {
            editor.codepoints = editor.historyDraft;
            editor.caret = editor.historyDraftCaret;
            ++editor.revision;
            ExitHistoryBrowse(editor);
            ResetCaretBlink(editor);
            return;
        }
    }
    SetEditorText(editor, console.history[static_cast<size_t>(editor.historyIndex)]);
    editor.historyBrowsing = true;
}

void ClearEditor(DebugConsoleEditor& editor)
{
    editor.codepoints.clear();
    editor.caret = 0;
    ++editor.revision;
    ExitHistoryBrowse(editor);
    ResetCaretBlink(editor);
}

void AddHistory(DebugConsoleData& console, const std::string& submitted)
{
    if (!console.history.empty() && console.history.back() == submitted) return;
    console.history.push_back(submitted);
    if (console.history.size() > console.maxHistoryEntries) {
        const size_t retain = console.maxHistoryEntries * 9 / 10;
        const size_t remove = console.history.size() - retain;
        console.history.erase(
                console.history.begin(),
                console.history.begin() + static_cast<std::ptrdiff_t>(remove));
    }
}

void AddError(DebugConsoleData& console, const std::string& error)
{
    DebugConsoleAddLine(console, error, DebugConsoleSeverity::Error);
}

using CommandHandler = void (*)(
        DebugConsoleData&,
        const std::vector<std::string>&,
        std::string_view);

void HandleClear(
        DebugConsoleData& console,
        const std::vector<std::string>& tokens,
        std::string_view currentMapId);
void HandleCopyLast(
        DebugConsoleData& console,
        const std::vector<std::string>& tokens,
        std::string_view currentMapId);
void HandleHelp(
        DebugConsoleData& console,
        const std::vector<std::string>& tokens,
        std::string_view currentMapId);
void HandleQuit(
        DebugConsoleData& console,
        const std::vector<std::string>& tokens,
        std::string_view currentMapId);
void HandleReload(
        DebugConsoleData& console,
        const std::vector<std::string>& tokens,
        std::string_view currentMapId);

struct CommandDefinition {
    const char* name;
    const char* usage;
    const char* summary;
    CommandHandler execute = nullptr;
};

constexpr CommandDefinition Commands[] = {
        {"clear", "/clear", "clear retained console output", HandleClear},
        {"copylast", "/copylast [lineCount]", "copy retained logical output",
                HandleCopyLast},
        {"help", "/help [command]", "list commands or describe one command",
                HandleHelp},
        {"quit", "/quit", "quit through normal application shutdown",
                HandleQuit},
        {"reload", "/reload", "restart the current map from disk",
                HandleReload}};

const CommandDefinition* FindCommand(std::string_view name)
{
    for (const CommandDefinition& command : Commands) {
        if (name.size() != std::char_traits<char>::length(command.name)) continue;
        bool same = true;
        for (size_t i = 0; i < name.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(name[i]))
                    != std::tolower(static_cast<unsigned char>(command.name[i]))) {
                same = false;
                break;
            }
        }
        if (same) return &command;
    }
    return nullptr;
}

bool ParsePositiveCount(std::string_view value, size_t& result)
{
    unsigned long long parsed = 0;
    const auto conversion = std::from_chars(
            value.data(), value.data() + value.size(), parsed);
    if (conversion.ec != std::errc{}
            || conversion.ptr != value.data() + value.size()
            || parsed == 0
            || parsed > std::numeric_limits<size_t>::max()) {
        return false;
    }
    result = static_cast<size_t>(parsed);
    return true;
}

void HandleHelp(
        DebugConsoleData& console,
        const std::vector<std::string>& tokens,
        std::string_view)
{
    if (tokens.size() > 2) {
        AddError(console, "usage: /help [command]");
        return;
    }
    if (tokens.size() == 2) {
        std::string requested = tokens[1];
        if (!requested.empty() && requested.front() == '/') requested.erase(0, 1);
        const CommandDefinition* found = FindCommand(requested);
        if (found == nullptr) {
            AddError(console, "unknown command: /" + requested);
        } else {
            DebugConsoleAddLine(console,
                    std::string{found->usage} + " - " + found->summary);
        }
        return;
    }
    for (const CommandDefinition& item : Commands) {
        DebugConsoleAddLine(console,
                std::string{item.usage} + " - " + item.summary);
    }
}

void HandleClear(
        DebugConsoleData& console,
        const std::vector<std::string>& tokens,
        std::string_view)
{
    if (tokens.size() != 1) {
        AddError(console, "usage: /clear");
        return;
    }
    DebugConsoleClear(console);
}

void HandleCopyLast(
        DebugConsoleData& console,
        const std::vector<std::string>& tokens,
        std::string_view)
{
    if (tokens.size() > 2) {
        AddError(console, "usage: /copylast [lineCount]");
        return;
    }
    size_t count = console.lines.size();
    if (tokens.size() == 2 && !ParsePositiveCount(tokens[1], count)) {
        AddError(console, "lineCount must be a positive integer");
        return;
    }
    count = std::min(count, console.lines.size());
    std::string copied;
    const size_t first = console.lines.size() - count;
    for (size_t i = first; i < console.lines.size(); ++i) {
        if (!copied.empty()) copied.push_back('\n');
        copied += console.lines[i].text;
    }
    SetClipboardText(copied.c_str());
    DebugConsoleAddLine(console,
            "copied " + std::to_string(count) + " console line(s)",
            DebugConsoleSeverity::Success);
}

void HandleReload(
        DebugConsoleData& console,
        const std::vector<std::string>& tokens,
        std::string_view currentMapId)
{
    if (tokens.size() != 1) {
        AddError(console, "usage: /reload");
        return;
    }
    if (currentMapId.empty()) {
        AddError(console, "reload unavailable: no active game map");
        return;
    }
    if (console.pendingAction.type != DeferredDebugActionType::None) {
        AddError(console, "another reload or quit action is already pending");
        return;
    }
    console.pendingAction = DeferredDebugAction{
            DeferredDebugActionType::ReloadCurrentMap,
            std::string{currentMapId}};
    DebugConsoleAddLine(console,
            "reload queued: " + std::string{currentMapId},
            DebugConsoleSeverity::Success);
}

void HandleQuit(
        DebugConsoleData& console,
        const std::vector<std::string>& tokens,
        std::string_view)
{
    if (tokens.size() != 1) {
        AddError(console, "usage: /quit");
        return;
    }
    if (console.pendingAction.type != DeferredDebugActionType::None) {
        AddError(console, "another reload or quit action is already pending");
        return;
    }
    console.pendingAction.type = DeferredDebugActionType::QuitApplication;
    DebugConsoleAddLine(console, "quit queued", DebugConsoleSeverity::Success);
}

void ExecuteCommand(
        DebugConsoleData& console,
        std::string_view submitted,
        std::string_view currentMapId)
{
    std::vector<std::string> tokens;
    std::string parseError;
    if (!DebugConsoleTokenizeCommand(submitted, tokens, parseError)) {
        AddError(console, "command parse error: " + parseError);
        return;
    }
    if (tokens.empty()) return;
    std::string name = tokens.front();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    const CommandDefinition* command = FindCommand(name);
    if (command == nullptr) {
        AddError(console, "unknown command: /" + name + " (try /help)");
        return;
    }
    command->execute(console, tokens, currentMapId);
}

void Submit(
        DebugConsoleData& console,
        ScriptRuntime* scripts,
        std::string_view currentMapId)
{
    const std::string submitted = DebugConsoleEditorText(console.editor);
    if (TrimView(submitted).empty()) return;
    DebugConsoleAddLine(console, submitted, DebugConsoleSeverity::Input);
    AddHistory(console, submitted);
    ClearEditor(console.editor);
    console.followTail = true;
    console.scrollOffsetVisualLines = 0;

    const std::string_view trimmed = TrimView(submitted);
    if (!trimmed.empty() && trimmed.front() == '/') {
        ExecuteCommand(console, trimmed, currentMapId);
        return;
    }
    if (scripts == nullptr) {
        AddError(console, "Lua unavailable: no map VM is active");
        return;
    }
    ScriptConsoleResult result = ScriptSystemExecuteConsole(*scripts, submitted);
    FlushPendingDebugConsoleLogs(console);
    if (!result.success) {
        AddError(console, result.error);
        return;
    }
    for (const std::string& value : result.values) {
        DebugConsoleAddLine(console, value, DebugConsoleSeverity::LuaResult);
    }
}

bool IsControlModifierDown()
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)
#if defined(__APPLE__)
            || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)
#endif
            ;
}

void ProcessEditingKey(
        DebugConsoleData& console,
        int key,
        bool repeated,
        ScriptRuntime* scripts,
        std::string_view currentMapId)
{
    DebugConsoleEditor& editor = console.editor;
    const bool control = IsControlModifierDown();
    if (!repeated && control && key == KEY_L) {
        DebugConsoleClear(console);
        return;
    }
    if (!repeated && control && key == KEY_V) {
        const char* clipboard = GetClipboardText();
        if (clipboard != nullptr) {
            const size_t available = console.editor.codepoints.size()
                            < console.maxInputCodepoints
                    ? console.maxInputCodepoints
                            - console.editor.codepoints.size()
                    : 0;
            DebugConsoleEditorInsert(
                    console,
                    DebugConsoleDecodeUtf8(clipboard, true, available));
        }
        return;
    }
    if (!repeated && (key == KEY_ENTER || key == KEY_KP_ENTER)) {
        Submit(console, scripts, currentMapId);
        return;
    }
    if (!repeated && key == KEY_ESCAPE) {
        console.open = false;
        ResetCaretBlink(editor);
        return;
    }
    if (key == KEY_LEFT) {
        editor.caret = std::max(0, editor.caret - 1);
    } else if (key == KEY_RIGHT) {
        editor.caret = std::min(
                static_cast<int>(editor.codepoints.size()), editor.caret + 1);
    } else if (key == KEY_HOME) {
        editor.caret = 0;
    } else if (key == KEY_END) {
        editor.caret = static_cast<int>(editor.codepoints.size());
    } else if (key == KEY_BACKSPACE && editor.caret > 0) {
        editor.codepoints.erase(editor.codepoints.begin() + editor.caret - 1);
        --editor.caret;
        ++editor.revision;
        ExitHistoryBrowse(editor);
    } else if (key == KEY_DELETE
            && editor.caret < static_cast<int>(editor.codepoints.size())) {
        editor.codepoints.erase(editor.codepoints.begin() + editor.caret);
        ++editor.revision;
        ExitHistoryBrowse(editor);
    } else if (key == KEY_UP) {
        BrowseHistory(console, -1);
        return;
    } else if (key == KEY_DOWN) {
        BrowseHistory(console, 1);
        return;
    } else if (key == KEY_PAGE_UP) {
        console.scrollOffsetVisualLines += console.style.pageScrollLines;
        console.followTail = false;
        return;
    } else if (key == KEY_PAGE_DOWN) {
        console.scrollOffsetVisualLines = std::max(
                0, console.scrollOffsetVisualLines - console.style.pageScrollLines);
        console.followTail = console.scrollOffsetVisualLines == 0;
        return;
    } else {
        return;
    }
    ++editor.revision;
    ResetCaretBlink(editor);
}

} // namespace

void DebugConsoleInitialize(DebugConsoleData& console, FontHandle font)
{
    console = DebugConsoleData{};
    console.font = font;
    console.lines.reserve(console.maxLogicalLines);
    console.history.reserve(console.maxHistoryEntries);
    console.editor.codepoints.reserve(console.maxInputCodepoints);
    console.editor.historyDraft.reserve(console.maxInputCodepoints);
    console.visualLines.reserve(console.maxLogicalLines * 2);
    console.editorVisualLines.reserve(8);
    console.initialized = true;
    DebugConsoleAddLine(
            console,
            "Debug console ready. Press F1 to close; /help lists commands.",
            DebugConsoleSeverity::Success);
}

void DebugConsoleShutdown(DebugConsoleData& console)
{
    console = DebugConsoleData{};
}

void DebugConsoleUpdate(
        DebugConsoleData& console,
        Input& input,
        ScriptRuntime* scripts,
        std::string_view currentMapId,
        bool available,
        float dtSeconds)
{
    if (!console.initialized) return;
    if (!available) {
        console.open = false;
        return;
    }

    const bool wasOpen = console.open;
    bool toggled = false;
    input.ForEachEvent(InputEventType::KeyPressed, true,
            [&console, &toggled](InputEvent& event) {
                if (event.key.key != KEY_F1) return;
                console.open = !console.open;
                toggled = true;
                ResetCaretBlink(console.editor);
                ConsumeEvent(event);
            });

    const bool captureThisFrame = wasOpen || console.open || toggled;
    if (!captureThisFrame) return;

    for (InputEvent& event : input.Events()) {
        if (event.handled) continue;
        if (console.open && !toggled) {
            if (event.type == InputEventType::KeyPressed) {
                ProcessEditingKey(
                        console, event.key.key, false, scripts, currentMapId);
            } else if (event.type == InputEventType::KeyRepeated) {
                ProcessEditingKey(
                        console, event.key.key, true, scripts, currentMapId);
            } else if (event.type == InputEventType::TextInput) {
                const int codepoint = static_cast<int>(event.text.codepoint);
                if (IsTextCodepoint(codepoint)) {
                    DebugConsoleEditorInsertCodepoint(console, codepoint);
                }
            } else if (event.type == InputEventType::MouseWheel) {
                const int amount = static_cast<int>(std::round(
                        std::fabs(event.wheel.value) * 3.0f));
                if (event.wheel.value > 0.0f) {
                    console.scrollOffsetVisualLines += amount;
                    console.followTail = false;
                } else {
                    console.scrollOffsetVisualLines = std::max(
                            0, console.scrollOffsetVisualLines - amount);
                    console.followTail = console.scrollOffsetVisualLines == 0;
                }
            }
        }
        ConsumeEvent(event);
    }

    if (console.open) {
        const float milliseconds = std::clamp(dtSeconds, 0.0f, 1.0f) * 1000.0f;
        console.editor.caretBlinkMs += milliseconds;
        while (console.editor.caretBlinkMs >= console.style.caretBlinkPeriodMs) {
            console.editor.caretBlinkMs -= console.style.caretBlinkPeriodMs;
            console.editor.caretVisible = !console.editor.caretVisible;
        }
    }
}

void DebugConsoleAddLine(
        DebugConsoleData& console,
        std::string_view text,
        DebugConsoleSeverity severity)
{
    if (!console.followTail && !console.layoutDirty) {
        console.preserveScrollOnLayout = true;
        console.anchorVisualLineCount = console.visualLines.size();
    }
    std::string line;
    const auto appendLine = [&]() {
        console.lines.push_back(DebugConsoleLine{
                console.nextSequence++, severity, std::move(line)});
        line.clear();
    };
    for (size_t i = 0; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
            appendLine();
        } else if (ch == '\n') {
            appendLine();
        } else if (ch == '\t') {
            line += "    ";
        } else if (ch >= 0x20 || ch >= 0x80) {
            line.push_back(static_cast<char>(ch));
        }
    }
    appendLine();

    if (console.lines.size() > console.maxLogicalLines) {
        const size_t retain = console.maxLogicalLines * 9 / 10;
        const size_t remove = console.lines.size() - retain;
        console.lines.erase(
                console.lines.begin(),
                console.lines.begin() + static_cast<std::ptrdiff_t>(remove));
    }
    if (console.followTail) console.scrollOffsetVisualLines = 0;
    console.layoutDirty = true;
}

void DebugConsoleClear(DebugConsoleData& console)
{
    console.lines.clear();
    console.visualLines.clear();
    console.scrollOffsetVisualLines = 0;
    console.followTail = true;
    console.layoutDirty = true;
}

bool DebugConsoleIsOpen(const DebugConsoleData& console)
{
    return console.initialized && console.open;
}

bool DebugConsoleCapturesGameplayInput(const DebugConsoleData& console)
{
    return DebugConsoleIsOpen(console);
}

DeferredDebugAction DebugConsoleTakeDeferredAction(DebugConsoleData& console)
{
    DeferredDebugAction result = std::move(console.pendingAction);
    console.pendingAction = DeferredDebugAction{};
    return result;
}

std::string DebugConsoleEditorText(const DebugConsoleEditor& editor)
{
    return EncodeUtf8(
            editor.codepoints, 0, static_cast<int>(editor.codepoints.size()));
}

std::vector<int> DebugConsoleDecodeUtf8(
        std::string_view text,
        bool sanitizePaste,
        size_t maxCodepoints)
{
    std::vector<int> result;
    result.reserve(std::min(text.size(), maxCodepoints));
    bool pendingSpace = false;
    for (size_t i = 0; i < text.size();) {
        if (result.size() >= maxCodepoints) break;
        const unsigned char first = static_cast<unsigned char>(text[i]);
        int codepoint = ReplacementCodepoint;
        size_t length = 1;
        if (first < 0x80) {
            codepoint = first;
        } else {
            int minimum = 0;
            if ((first & 0xe0) == 0xc0) {
                codepoint = first & 0x1f;
                length = 2;
                minimum = 0x80;
            } else if ((first & 0xf0) == 0xe0) {
                codepoint = first & 0x0f;
                length = 3;
                minimum = 0x800;
            } else if ((first & 0xf8) == 0xf0) {
                codepoint = first & 0x07;
                length = 4;
                minimum = 0x10000;
            }
            if (length > 1) {
                if (i + length > text.size()) {
                    codepoint = ReplacementCodepoint;
                    length = 1;
                } else {
                    bool valid = true;
                    for (size_t j = 1; j < length; ++j) {
                        const unsigned char continuation =
                                static_cast<unsigned char>(text[i + j]);
                        if ((continuation & 0xc0) != 0x80) {
                            valid = false;
                            break;
                        }
                        codepoint = (codepoint << 6) | (continuation & 0x3f);
                    }
                    if (!valid || codepoint < minimum || !IsUnicodeScalar(codepoint)) {
                        codepoint = ReplacementCodepoint;
                        length = 1;
                    }
                }
            }
        }
        i += length;
        if (sanitizePaste && (codepoint == '\r' || codepoint == '\n'
                    || codepoint == '\t')) {
            pendingSpace = true;
            continue;
        }
        if (sanitizePaste && !IsTextCodepoint(codepoint)) continue;
        if (pendingSpace) {
            if ((result.empty() || result.back() != ' ')
                    && result.size() < maxCodepoints) result.push_back(' ');
            pendingSpace = false;
        }
        if (result.size() < maxCodepoints) result.push_back(codepoint);
    }
    if (pendingSpace && result.size() < maxCodepoints
            && (result.empty() || result.back() != ' ')) {
        result.push_back(' ');
    }
    return result;
}

void DebugConsoleEditorInsert(
        DebugConsoleData& console,
        const std::vector<int>& codepoints)
{
    DebugConsoleEditor& editor = console.editor;
    const size_t available = editor.codepoints.size() < console.maxInputCodepoints
            ? console.maxInputCodepoints - editor.codepoints.size() : 0;
    const size_t insertCount = std::min(available, codepoints.size());
    editor.caret = std::clamp(
            editor.caret, 0, static_cast<int>(editor.codepoints.size()));
    editor.codepoints.insert(
            editor.codepoints.begin() + editor.caret,
            codepoints.begin(),
            codepoints.begin() + static_cast<std::ptrdiff_t>(insertCount));
    editor.caret += static_cast<int>(insertCount);
    ++editor.revision;
    ExitHistoryBrowse(editor);
    ResetCaretBlink(editor);
    if (insertCount < codepoints.size() && !console.inputLimitWarningShown) {
        console.inputLimitWarningShown = true;
        DebugConsoleAddLine(
                console,
                "console input was truncated at 4096 codepoints",
                DebugConsoleSeverity::Warning);
    }
}

void DebugConsoleEditorInsertCodepoint(
        DebugConsoleData& console,
        int codepoint)
{
    DebugConsoleEditor& editor = console.editor;
    if (editor.codepoints.size() >= console.maxInputCodepoints) {
        if (!console.inputLimitWarningShown) {
            console.inputLimitWarningShown = true;
            DebugConsoleAddLine(
                    console,
                    "console input was truncated at 4096 codepoints",
                    DebugConsoleSeverity::Warning);
        }
        return;
    }
    editor.caret = std::clamp(
            editor.caret, 0, static_cast<int>(editor.codepoints.size()));
    editor.codepoints.insert(editor.codepoints.begin() + editor.caret, codepoint);
    ++editor.caret;
    ++editor.revision;
    ExitHistoryBrowse(editor);
    ResetCaretBlink(editor);
}

bool DebugConsoleTokenizeCommand(
        std::string_view text,
        std::vector<std::string>& outTokens,
        std::string& outError)
{
    outTokens.clear();
    outError.clear();
    text = TrimView(text);
    if (!text.empty() && text.front() == '/') text.remove_prefix(1);
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size()
                && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
        if (i >= text.size()) break;
        std::string token;
        bool quoted = false;
        if (text[i] == '"') {
            quoted = true;
            ++i;
        }
        bool closed = !quoted;
        while (i < text.size()) {
            const char ch = text[i++];
            if (quoted && ch == '"') {
                closed = true;
                break;
            }
            if (!quoted && std::isspace(static_cast<unsigned char>(ch))) break;
            if (quoted && ch == '\\') {
                if (i >= text.size()) {
                    outError = "unfinished escape sequence";
                    return false;
                }
                const char escaped = text[i++];
                if (escaped == '\\' || escaped == '"') token.push_back(escaped);
                else if (escaped == 'n') token.push_back('\n');
                else if (escaped == 't') token.push_back('\t');
                else {
                    outError = std::string{"unsupported escape: \\"} + escaped;
                    return false;
                }
            } else {
                token.push_back(ch);
            }
        }
        if (!closed) {
            outError = "unterminated quoted argument";
            return false;
        }
        if (quoted && i < text.size()
                && !std::isspace(static_cast<unsigned char>(text[i]))) {
            outError = "quoted argument must be followed by whitespace";
            return false;
        }
        outTokens.push_back(std::move(token));
    }
    return true;
}

} // namespace engine
