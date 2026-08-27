#include "engine/debug/DebugConsole.h"
#include "engine/debug/DebugConsoleLogBridge.h"
#include "engine/input/Input.h"

#include <raylib.h>

#include <cassert>
#include <string>
#include <thread>
#include <vector>

namespace {

void SubmitConsoleCommand(
        engine::DebugConsoleData& console,
        std::string_view command,
        std::string_view mapId = "hub")
{
    engine::Input input;
    input.Initialize();
    for (char ch : command) {
        engine::InputEvent event{};
        event.type = engine::InputEventType::TextInput;
        event.text = engine::TextInputEvent{static_cast<uint32_t>(ch)};
        input.Events().push_back(event);
    }
    engine::InputEvent enter{};
    enter.type = engine::InputEventType::KeyPressed;
    enter.key = engine::KeyEvent{KEY_ENTER};
    input.Events().push_back(enter);
    engine::DebugConsoleUpdate(
            console, input, nullptr, mapId, true, 0.016f);
}

void EditorAndUtf8AreCodepointSafe()
{
    engine::DebugConsoleData console;
    engine::DebugConsoleInitialize(console, engine::NullFontHandle());
    const std::vector<int> decoded = engine::DebugConsoleDecodeUtf8(
            "h\xc3\xa9\xf0\x9f\x99\x82", false);
    assert(decoded.size() == 3);
    engine::DebugConsoleEditorInsert(console, decoded);
    assert(console.editor.caret == 3);
    assert(engine::DebugConsoleEditorText(console.editor)
            == "h\xc3\xa9\xf0\x9f\x99\x82");

    const std::vector<int> paste = engine::DebugConsoleDecodeUtf8(
            "one\r\ntwo\tthree\x01", true);
    engine::DebugConsoleData pasted;
    engine::DebugConsoleInitialize(pasted, engine::NullFontHandle());
    engine::DebugConsoleEditorInsert(pasted, paste);
    assert(engine::DebugConsoleEditorText(pasted.editor)
            == "one two three");

    console.maxInputCodepoints = 4;
    engine::DebugConsoleEditorInsert(console, std::vector<int>{'x', 'y'});
    assert(console.editor.codepoints.size() == 4);
    assert(console.inputLimitWarningShown);
}

void OutputIsLogicalBoundedAndSanitized()
{
    engine::DebugConsoleData console;
    engine::DebugConsoleInitialize(console, engine::NullFontHandle());
    engine::DebugConsoleClear(console);
    engine::DebugConsoleAddLine(
            console, "first\n\nsecond\tvalue\x01", engine::DebugConsoleSeverity::Info);
    assert(console.lines.size() == 3);
    assert(console.lines[0].text == "first");
    assert(console.lines[1].text.empty());
    assert(console.lines[2].text == "second    value");

    engine::DebugConsoleData bounded;
    engine::DebugConsoleInitialize(bounded, engine::NullFontHandle());
    engine::DebugConsoleClear(bounded);
    bounded.maxLogicalLines = 10;
    for (int i = 0; i < 11; ++i) {
        engine::DebugConsoleAddLine(bounded, std::to_string(i));
    }
    assert(bounded.lines.size() == 9);
    assert(bounded.lines.front().text == "2");
}

void CommandTokenizerHandlesQuotesAndErrors()
{
    std::vector<std::string> tokens;
    std::string error;
    assert(engine::DebugConsoleTokenizeCommand(
            R"(/command one "two words" "quote: \"")",
            tokens,
            error));
    assert(tokens.size() == 4);
    assert(tokens[0] == "command");
    assert(tokens[2] == "two words");
    assert(tokens[3] == "quote: \"");
    assert(!engine::DebugConsoleTokenizeCommand(
            R"(/command "unfinished)", tokens, error));
    assert(!error.empty());
}

void SubmissionQueuesDeferredActionsAndHistory()
{
    engine::DebugConsoleData console;
    engine::DebugConsoleInitialize(console, engine::NullFontHandle());
    console.open = true;
    engine::Input input;
    input.Initialize();
    const std::string command = "/reload";
    for (char ch : command) {
        engine::InputEvent event{};
        event.type = engine::InputEventType::TextInput;
        event.text = engine::TextInputEvent{static_cast<uint32_t>(ch)};
        input.Events().push_back(event);
    }
    engine::InputEvent enter{};
    enter.type = engine::InputEventType::KeyPressed;
    enter.key = engine::KeyEvent{KEY_ENTER};
    input.Events().push_back(enter);
    engine::DebugConsoleUpdate(
            console, input, nullptr, "hub", true, 0.016f);
    assert(console.history.size() == 1);
    assert(console.history.front() == command);
    const engine::DeferredDebugAction action =
            engine::DebugConsoleTakeDeferredAction(console);
    assert(action.type == engine::DeferredDebugActionType::ReloadCurrentMap);
    assert(action.mapId == "hub");
    assert(engine::DebugConsoleTakeDeferredAction(console).type
            == engine::DeferredDebugActionType::None);

    engine::Input toggleInput;
    toggleInput.Initialize();
    const std::string toggleCommand = "/god off";
    for (char ch : toggleCommand) {
        engine::InputEvent event{};
        event.type = engine::InputEventType::TextInput;
        event.text = engine::TextInputEvent{static_cast<uint32_t>(ch)};
        toggleInput.Events().push_back(event);
    }
    toggleInput.Events().push_back(enter);
    engine::DebugConsoleUpdate(
            console, toggleInput, nullptr, "hub", true, 0.016f);
    const engine::DeferredDebugAction toggle =
            engine::DebugConsoleTakeDeferredAction(console);
    assert(toggle.type == engine::DeferredDebugActionType::SetGodMode);
    assert(toggle.mapId == "hub");
    assert(toggle.booleanMode == engine::DeferredDebugBooleanMode::Disable);
}

void DebugAiCommandSupportsToggleModesAndHelp()
{
    engine::DebugConsoleData console;
    engine::DebugConsoleInitialize(console, engine::NullFontHandle());
    console.open = true;

    SubmitConsoleCommand(console, "/debugai");
    engine::DeferredDebugAction action =
            engine::DebugConsoleTakeDeferredAction(console);
    assert(action.type == engine::DeferredDebugActionType::SetDebugAi);
    assert(action.mapId == "hub");
    assert(action.booleanMode == engine::DeferredDebugBooleanMode::Toggle);

    SubmitConsoleCommand(console, "/debugai on");
    action = engine::DebugConsoleTakeDeferredAction(console);
    assert(action.type == engine::DeferredDebugActionType::SetDebugAi);
    assert(action.booleanMode == engine::DeferredDebugBooleanMode::Enable);

    SubmitConsoleCommand(console, "/debugai off");
    action = engine::DebugConsoleTakeDeferredAction(console);
    assert(action.type == engine::DeferredDebugActionType::SetDebugAi);
    assert(action.booleanMode == engine::DeferredDebugBooleanMode::Disable);

    SubmitConsoleCommand(console, "/debugai maybe");
    assert(engine::DebugConsoleTakeDeferredAction(console).type
            == engine::DeferredDebugActionType::None);
    assert(!console.lines.empty());
    assert(console.lines.back().text.find("usage: /debugai [on|off]")
            != std::string::npos);

    SubmitConsoleCommand(console, "/help debugai");
    assert(!console.lines.empty());
    assert(console.lines.back().text.find("/debugai [on|off]")
            != std::string::npos);
}

void TraceLogsFlushThroughTheThreadSafeInbox()
{
    engine::DebugConsoleData console;
    engine::DebugConsoleInitialize(console, engine::NullFontHandle());
    engine::DebugConsoleClear(console);
    engine::SetDebugConsoleLogCaptureEnabled(true);
    engine::InstallDebugConsoleTraceLogBridge();
    TraceLog(LOG_INFO, "console bridge main");
    std::thread worker([] {
        TraceLog(LOG_WARNING, "console bridge worker");
    });
    worker.join();
    engine::FlushPendingDebugConsoleLogs(console);
    assert(console.lines.size() == 2);
    assert(console.lines[0].text.find("console bridge main")
            != std::string::npos);
    assert(console.lines[1].text.find("console bridge worker")
            != std::string::npos);
    assert(console.lines[1].severity == engine::DebugConsoleSeverity::Warning);
    engine::SetDebugConsoleLogCaptureEnabled(false);
}

} // namespace

int main()
{
    EditorAndUtf8AreCodepointSafe();
    OutputIsLogicalBoundedAndSanitized();
    CommandTokenizerHandlesQuotesAndErrors();
    SubmissionQueuesDeferredActionsAndHistory();
    DebugAiCommandSupportsToggleModesAndHelp();
    TraceLogsFlushThroughTheThreadSafeInbox();
    return 0;
}
