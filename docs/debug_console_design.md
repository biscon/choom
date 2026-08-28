# Raylib Debug Console Design

## 1. Purpose

This document specifies an in-game developer console for a C++17 engine using Raylib and the same event-based input model as the reference engine. The console is a game-global debugging tool that:

- opens as an overlay above the current game or menu;
- captures keyboard/text input while open;
- provides editable input, history, scrolling, clipboard support, and bounded output;
- receives engine and Raylib log messages safely, including messages emitted by worker threads;
- executes Lua expressions or statements in the currently active map VM;
- supports a small portable slash-command set;
- survives map reloads even though the map-scoped Lua VM does not.

The initial slash commands are deliberately generic:

```text
/help
/clear
/copylast [lineCount]
/reload
/quit
```

Do not copy the reference engine's game-specific commands. Future sector-engine commands should register through the command table described below rather than growing a large `if` chain.

This design assumes the level-scoped Lua system described in `lua_scripting_design.md`: one Lua 5.5 VM per map, deferred map changes, synchronous console evaluation on the main Lua state, and a normal teardown/recreation path for map reload.

## 2. Goals and non-goals

### Goals

- Make the console usable without pausing development to add external tooling.
- Route all input through the existing handled-event system.
- Prevent console keystrokes, mouse clicks, and held movement controls from reaching gameplay.
- Make Lua evaluation useful for inspecting and changing the current map state.
- Keep console lifetime independent from map and Lua lifetime.
- Preserve output order and avoid calling UI or Lua code from worker threads.
- Keep memory bounded and frame-time behavior predictable.
- Render correctly into the engine's internal UI target and scale with its logical resolution.
- Make command dispatch easy to extend without coupling the console core to gameplay systems.

### Non-goals

- A full IDE, Lua debugger, source editor, or autocomplete system.
- Multiline editing in v1. Pasted newlines become spaces and Enter submits one logical line.
- Yielding Lua console chunks or awaiting script operations directly from the console.
- Persisting console history to disk in v1.
- Exposing the console in production builds by default.
- Implementing remote administration or networking.
- Replacing the engine's normal logging backend.

## 3. Lifetime and ownership

The debug console is owned by the game/application state, not the map runtime:

```text
GameState
├── InputData
├── DebugConsoleData       game-global
├── PersistentScriptStore
└── CurrentMap
    └── ScriptRuntime      destroyed and recreated on reload/map change
```

Consequences:

- output lines and command history survive map changes and `/reload`;
- the console can open in menus or when no map is loaded;
- Lua submission resolves the current VM at the moment Enter is pressed;
- operation userdata and coroutine state never live in the console;
- console shutdown occurs once during application shutdown, after its final pending logs are flushed.

The console owns its font resource or obtains a stable global font handle from the engine resource system. It never stores pointers into map data or the Lua runtime.

## 4. Suggested source organization

Use focused files or equivalent modules:

```text
debug/DebugConsoleData.h
debug/DebugConsole.h/.cpp
debug/DebugConsoleInput.cpp
debug/DebugConsoleRender.cpp
debug/DebugConsoleCommands.cpp
debug/DebugConsoleLogBridge.h/.cpp
scripting/ScriptConsole.cpp
```

Responsibilities:

- `DebugConsole`: initialization, shutdown, output retention, submission orchestration.
- `DebugConsoleInput`: event consumption and line editing.
- `DebugConsoleRender`: wrapping, layout, caret placement, and drawing.
- `DebugConsoleCommands`: portable command registry and handlers.
- `DebugConsoleLogBridge`: thread-safe capture of Raylib/engine logs.
- `ScriptConsole`: synchronous evaluation against the active Lua VM.

Gameplay-specific command implementations added later should live near the relevant subsystem and register a command during startup. The debug console core should not include door, AI, renderer, audio, or sector headers merely to dispatch commands.

## 5. Data model

### 5.1 Output severity and lines

Use semantic severity instead of storing Raylib colors as the primary data. Rendering maps severity to the current console theme.

```cpp
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
    std::string text; // UTF-8, one logical line; wrapping is render-time data
};
```

Store logical lines, not permanently wrapped visual fragments. The reference engine stores wrapped lines, which makes resize/font changes awkward and makes `/copylast` copy display wrapping rather than original messages. This design wraps during layout and caches the result until width/font/output changes.

Normalize embedded `\r`/`\n` in an added message by splitting it into separate logical lines. Preserve empty lines between newline separators. Remove other unsafe control characters except tab, which should expand to a fixed number of spaces.

### 5.2 Input editing state

Raylib's `GetCharPressed()` produces Unicode codepoints. Store the active input as codepoints so caret movement and deletion cannot split UTF-8 byte sequences:

```cpp
struct DebugConsoleEditor {
    std::vector<int> codepoints;
    int caret = 0; // codepoint index, range [0, codepoints.size()]

    bool historyBrowsing = false;
    int historyIndex = -1;
    std::vector<int> historyDraft;
    int historyDraftCaret = 0;

    float caretBlinkMs = 0.0f;
    bool caretVisible = true;
};
```

Convert to UTF-8 only when submitting, copying, storing history, or rendering. Validate pasted UTF-8 and replace malformed sequences with U+FFFD rather than retaining invalid byte strings.

### 5.3 Console runtime state

```cpp
struct DebugConsoleData {
    bool initialized = false;
    bool open = false;

    DebugConsoleEditor editor;
    std::vector<DebugConsoleLine> lines;
    std::vector<std::string> history; // submitted UTF-8 lines

    int scrollOffsetVisualLines = 0; // 0 means bottom/latest
    bool followTail = true;

    size_t maxLogicalLines = 2000;
    size_t maxHistoryEntries = 200;
    size_t maxInputCodepoints = 4096;

    uint64_t nextSequence = 1;

    Font font{};
    bool ownsFont = false;

    bool layoutDirty = true;
    float cachedWrapWidth = 0.0f;
    std::vector<DebugConsoleVisualLine> visualLines;
};
```

`DebugConsoleVisualLine` contains a source sequence/index, a UTF-8 fragment, severity, and any prefix metadata required for drawing. It is derived cache data and need not be serialized.

The exact vector types can follow the target engine. Reserve line/history/cache capacity during initialization. Removing old lines can be done in batches when exceeding the limit so repeated single-element front erases do not become unnecessarily expensive.

### 5.4 Theme and layout configuration

Keep layout values together:

```cpp
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
```

Compute the panel from the logical UI render-target width and height. Do not hard-code a particular display resolution. Rendering occurs in logical UI coordinates before presentation scaling.

## 6. Public interface

The console's public API should remain small:

```cpp
bool DebugConsoleInit(
    GameState& game,
    const std::string& fontPath);

void DebugConsoleShutdown(DebugConsoleData& console);

void DebugConsoleUpdate(
    GameState& game,
    float dtSeconds);

void DebugConsoleRender(
    const GameState& game,
    int logicalWidth,
    int logicalHeight);

void DebugConsoleAddLine(
    DebugConsoleData& console,
    std::string_view text,
    DebugConsoleSeverity severity = DebugConsoleSeverity::Info);

bool DebugConsoleIsOpen(const DebugConsoleData& console);

bool DebugConsoleCapturesGameplayInput(
    const DebugConsoleData& console);
```

Provide convenience logging helpers only if they reduce repetition:

```cpp
DebugConsoleInfo(console, text);
DebugConsoleError(console, text);
```

Do not make arbitrary systems depend on `GameState` merely to add a console line. Prefer passing `DebugConsoleData&`, using the central logger, or letting the log bridge mirror normal logs into the console.

## 7. Initialization and shutdown

### 7.1 Startup order

Recommended application startup:

1. Install the Raylib trace callback before `InitWindow` if startup logs should be captured.
2. Initialize Raylib/window/render targets.
3. Initialize the input system.
4. Initialize the console state and load its font.
5. Flush trace messages captured before console initialization.
6. Register portable slash commands.
7. Continue initializing maps, Lua, and gameplay systems.

The trace bridge owns its pending queue independently, so early logs are retained until the console exists.

Load a monospaced font with `LoadFontEx`, set bilinear filtering when appropriate, and fall back to `GetFontDefault()` if loading fails. Record `ownsFont` only for a successfully loaded font. Font failure must not disable the console.

Do not add the successful font-load message through `TraceLog` if the log bridge will mirror it; otherwise it can appear twice. Either emit it directly to the console or through the logger, not both.

### 7.2 Shutdown order

1. Flush pending trace messages to the console/log sink.
2. Run normal map/Lua shutdown.
3. Unload the console-owned font while the Raylib window/context still exists.
4. Clear console vectors and mark it uninitialized.
5. Close the window later in the application shutdown sequence.

The Raylib trace callback should remain valid until logging stops. If the engine replaces callbacks at shutdown, install a passthrough/default callback or guard the queue's static lifetime so late logs cannot access destroyed state.

## 8. Frame integration and ordering

The console must process input before gameplay consumers. Recommended frame order:

```cpp
UpdateInput(game.input);
FlushPendingConsoleLogs(game.debugConsole);

DebugConsoleUpdate(game, dt);
ProcessMenuInput(game);      // reads only unhandled events
ProcessGameplayInput(game);  // reads only unhandled events

UpdateGameplay(game, dt);
ScriptSystemUpdate(game, dt);

ProcessDeferredQuitOrReload(game);

RenderWorld(...);
RenderGameUi(...);
DebugConsoleRender(game, logicalWidth, logicalHeight); // last UI overlay
PresentFrame(...);
```

The console toggle key must be handled before gameplay sees it. Lua evaluation or slash commands may request `/reload` or `/quit`, but those actions are processed at the safe deferred-action point, never from inside input iteration or Lua evaluation.

The console remains responsive whether a map is active, a menu is visible, or gameplay simulation is paused.

## 9. Input capture contract

The existing input model provides events with a `handled` flag and filters such as:

```cpp
FilterEvents(input, true, InputEventType::KeyPressed)
ConsumeEvent(event)
```

The console follows these rules:

- When closed, it consumes only the non-repeated grave/backtick toggle press and the matching text-input event generated that frame.
- When open, it is modal to gameplay keyboard and mouse input.
- It consumes all unhandled `KeyPressed`, `KeyRepeated`, `KeyReleased`, and `TextInput` events after interpreting the ones it uses.
- It consumes mouse button/click/wheel events while open so firing, use actions, and UI clicks cannot pass through the overlay.
- Continuous/polled gameplay input such as `IsKeyDown`, mouse delta, or controller axes must additionally gate on `DebugConsoleCapturesGameplayInput()` because handled events cannot suppress direct polling.
- Opening the console clears current player movement/look intent immediately so the last held input does not persist.
- In a captured-mouse 3D game, opening the console releases the mouse/cursor through the engine's cursor ownership system; closing restores the previous gameplay capture mode.

If the shared input system does not yet emit mouse-wheel events, add them there or use Raylib wheel polling only inside the console update while open. Keep one owner for that polling so scroll input is not duplicated.

### 9.1 Toggle behavior

Use `KEY_GRAVE` by default. On a non-repeated press:

```cpp
console.open = !console.open;
ResetCaretBlink(console.editor);
suppressTextInputThisFrame = true;
ConsumeEvent(event);
```

Raylib may generate a text codepoint for the same physical grave key. Consume all text events in that frame when toggling so a backtick is not inserted into the editor.

Escape closes the console and is consumed. It does not clear the draft. Reopening restores the draft and caret.

### 9.2 Key bindings

While open:

```text
Enter             submit
Escape            close console
Backspace         delete previous codepoint
Delete            delete next codepoint
Left / Right      move caret one codepoint
Home / End        move to beginning/end
Up / Down         browse command history
PageUp/PageDown   scroll output by a page
Mouse wheel       scroll output
Ctrl+V            paste sanitized clipboard text
Ctrl+C            copy selected feature is omitted in v1; no selection exists
Ctrl+L            clear output, same as /clear
```

Only editing/navigation keys repeat. Ignore repeat events for Enter, Escape, toggle, and Ctrl shortcuts. The shared input system's initial delay and repeat interval remain authoritative.

Modifier checks can use `IsKeyDown(KEY_LEFT_CONTROL)` and `KEY_RIGHT_CONTROL`, or modifier fields added to input events later. On macOS, optionally treat Super/Command as the paste modifier.

### 9.3 Text input

Use `TextInput` events originating from `GetCharPressed`, not key names, to insert printable characters. Accept Unicode scalar values except:

- C0/C1 control characters;
- DEL;
- carriage return/newline/tab from ordinary typing;
- invalid Unicode or surrogate values.

Enforce `maxInputCodepoints`. If an insertion/paste would exceed the cap, insert the portion that fits and optionally add a warning line once. Do not allocate an unbounded paste.

### 9.4 Clipboard sanitation

`GetClipboardText()` may return null. Decode its UTF-8 and:

- replace `\r`, `\n`, and `\t` runs with one ASCII space;
- discard other control codepoints;
- replace malformed UTF-8 with U+FFFD;
- truncate at `maxInputCodepoints`;
- insert at the current caret.

Multiline paste does not execute multiple commands. This avoids accidental command injection from a clipboard and keeps the editor single-line logically even when visually wrapped.

## 10. Editor and history behavior

### 10.1 Editing

Insertion, deletion, caret movement, paste, and history recall reset the caret blink timer and make the caret visible. Clamp the caret after every state change.

Editing a recalled history entry exits history-browsing mode. The recalled text remains as the editable input.

### 10.2 History

On submission:

- do not store empty/whitespace-only lines;
- store both slash commands and Lua lines;
- do not append an entry identical to the immediately previous entry;
- cap history at `maxHistoryEntries`, dropping oldest entries in batches;
- reset history browsing after submission.

When Up is first pressed:

1. Save the current unfinished input and caret as the history draft.
2. Recall the newest history item.

Further Up presses move toward older entries and stop at the oldest. Down moves toward newer entries. Moving down past the newest restores the saved draft and its caret.

The history is game-global and remains intact across map reload. It is cleared only by application shutdown or an explicit future `/clearhistory` command, which is not required in v1.

### 10.3 Submission pipeline

Submission is deterministic:

1. Convert editor codepoints to UTF-8.
2. Trim only for the empty-line check and command parsing; retain the original submitted text for display/history.
3. Add an input line with `Input` severity and visual prefix `> `.
4. Add it to history under the rules above.
5. Clear editor and history-browse state before execution so errors/reentrant logs cannot corrupt the submitted buffer.
6. If the first non-whitespace character is `/`, dispatch a slash command.
7. Otherwise evaluate it as Lua in the current VM.
8. Add returned values as `LuaResult` or errors as `Error`.
9. Reset caret blink and follow the output tail.

Slash command recognition after leading whitespace is recommended. Lua code beginning with division is not meaningful at a fresh prompt, so reserving a leading slash is unambiguous.

## 11. Output retention, wrapping, and scrolling

### 11.1 Adding lines

`DebugConsoleAddLine` is a main-thread UI operation. It:

1. sanitizes/splits newlines into logical lines;
2. assigns monotonically increasing sequence numbers;
3. appends severity-tagged lines;
4. enforces `maxLogicalLines`;
5. marks the visual layout dirty;
6. preserves the user's scroll anchor if they are reading older output.

When at the bottom, new output keeps `scrollOffsetVisualLines == 0` and `followTail == true`. When scrolled up, new lines must not yank the viewport to the bottom. After rebuilding wrapped lines, preserve the top visible source sequence and wrapped-fragment index when possible. A simpler acceptable v1 behavior is to increase the visual scroll offset by exactly the number of new wrapped fragments.

### 11.2 Wrapping

Wrap both output and editor text by measuring with `MeasureTextEx`. Never split UTF-8 code units; iterate codepoints. Prefer wrapping at whitespace, with codepoint fallback for an individual token wider than the panel.

Output prefixes such as `> ` are layout metadata rather than stored inside the source text. Continuation fragments align with the start of content or use a small continuation indent.

Rebuild the visual cache when:

- logical output changes;
- console font/font size changes;
- panel width changes;
- style changes.

Do not re-measure all 2000 lines every frame when nothing changed.

### 11.3 Scrolling

`scrollOffsetVisualLines == 0` denotes the latest output. Positive values move upward. Clamp it to:

```cpp
max(0, visualLineCount - visibleOutputLineCount)
```

PageUp/PageDown adjust by `style.pageScrollLines` or the current visible count. Mouse wheel adjusts by a smaller amount such as three lines per notch. PageDown reaching zero re-enables tail following.

`/clear` and Ctrl+L clear logical and visual output and reset scrolling, but preserve history and the current editor draft.

## 12. Rendering

Render the console into the UI render target after all ordinary game/menu UI and immediately before presentation. It should not be affected by world camera transforms, fog, post-processing, or 3D depth.

### 12.1 Panel geometry

Given logical UI dimensions:

```cpp
const float panelX = style.outerMargin;
const float panelY = style.topMargin;
const float panelW = logicalWidth - style.outerMargin * 2.0f;
const float desiredH = logicalHeight * style.heightFraction;
const float panelH = std::clamp(
    desiredH,
    style.minHeight,
    std::min(style.maxHeight,
             logicalHeight - style.topMargin - style.outerMargin));
```

Divide the panel into:

- output area;
- a small gap/divider;
- input area sized for one to `maxVisibleInputLines` wrapped editor lines.

Use a dark translucent rounded background, visible border, slightly distinct input background, and a concise help/status strip above or below the panel.

### 12.2 Severity colors

Suggested defaults:

```text
Trace       gray
Debug       light gray
Info        sky blue
Success     green
Warning     yellow
Error       orange/red
Fatal       red
Input       white
LuaResult   cyan/sky blue
```

Keep colors in a theme so high-contrast or color-blind-friendly variants can be added later.

### 12.3 Editor layout and caret

Visually wrap the editor to at most three visible lines. If the full editor occupies more lines, choose a window that always contains the caret. The first visual line draws `> `; continuations have no prompt or use a fixed indent.

Each editor layout fragment records:

```cpp
struct DebugConsoleEditorVisualLine {
    int startCodepoint = 0;
    int endCodepoint = 0;
    std::string prefix;
    std::string utf8Text;
};
```

To place the caret, locate its fragment, encode the fragment prefix plus codepoints before the caret, and measure that UTF-8 string with the same font/size/spacing used to draw. Draw a 2-pixel vertical line for the caret.

The caret blinks every 500 ms while open. Clamp very large `dt` by toggling based on accumulated periods or reset after focus changes. Do not update blink while closed unless retaining phase is specifically desired.

### 12.4 Status/help strip

Display only portable controls, for example:

```text
` close  |  Enter submit  |  Up/Down history  |  PgUp/PgDn scroll  |  Ctrl+V paste  |  /help
```

Optionally show Lua context status at the right:

```text
Lua: map refinery
Lua: unavailable
Lua: shutting down
```

This prevents confusion when a Lua line is entered from a menu or during a deferred reload.

## 13. Slash-command architecture

The current game also uses the deferred boolean-command path for
`/god [on|off]`, `/invisible [on|off]`, `/freezeai [on|off]`, and
`/debugai [on|off]`. Bare forms toggle the active campaign-session value;
explicit forms are idempotent. Invisibility clears hostile NPC player alerts
and suppresses their visual and hearing detection while active. These commands
require an active game map and retain its ID in the deferred action so a map
change cancels a stale request safely.

### 13.1 Command registry

Avoid a monolithic command `if` chain. Register commands in a table:

```cpp
struct DebugConsoleCommandContext {
    GameState& game;
    DebugConsoleData& console;
};

using DebugConsoleCommandFn = bool (*)(
    DebugConsoleCommandContext& context,
    const std::vector<std::string>& args,
    std::string& outError);

struct DebugConsoleCommand {
    std::string name;       // without slash
    std::string usage;
    std::string summary;
    DebugConsoleCommandFn execute = nullptr;
};

struct DebugConsoleCommandRegistry {
    std::vector<DebugConsoleCommand> commands;
    std::unordered_map<std::string, size_t> indexByName;
};
```

Register during application initialization, reject duplicate names, and sort `/help` output by name. Command lookup is case-insensitive ASCII; arguments retain their original case.

Handlers return false only for usage/expected command failure and fill `outError`. They should add multi-line output directly only when appropriate. Internal errors go through the normal logger.

### 13.2 Tokenization

Support whitespace-delimited arguments plus double-quoted strings and basic escapes:

```text
/command one "two words" "quote: \""
```

Required escapes inside quotes are `\\`, `\"`, `\n`, and `\t`; newline/tab values can be decoded for future commands even though the editor itself is one line. Reject unterminated quotes with a clear parse error. The leading slash is removed before lookup.

The portable commands do not require quoted arguments, but a sound parser prevents every future command from inventing its own splitting rules.

### 13.3 Required commands

#### `/help`

```text
/help
/help <command>
```

Without an argument, list registered command usages and one-line summaries. With a command name, show its full usage/summary. Help is derived from the registry so it cannot drift from implementation.

#### `/clear`

```text
/clear
```

Clear console output and reset scroll to the bottom. Preserve history/editor. Extra arguments are an error.

#### `/copylast`

```text
/copylast
/copylast <lineCount>
```

Copy all retained logical output or the last positive number of logical lines to the clipboard. Copy original logical messages separated by `\n`, not render-time wrapped fragments or color metadata. Reject zero, negative, non-numeric, overflowed, or extra arguments. Add a success message after copying; that success line is not part of the copied buffer.

#### `/reload`

```text
/reload
```

Require an active map. Queue a `ReloadCurrentMap` application request and return immediately. Do not call map unload/load from the command handler. Reject a second request if a quit/map transition/reload is already pending.

At the safe deferred-action point, reload through the same path used by normal map teardown:

1. preserve the reload descriptor needed by the map system (current map ID/path and the engine's chosen restart spawn);
2. call map Lua `shutdown()` exactly once;
3. cancel script tasks/operations and close the old VM;
4. unload map data/resources;
5. reload the same map and create a fresh VM;
6. execute its script and start `init()`;
7. leave console lines, history, open state, and draft intact.

Default v1 policy is a clean map restart, not a save-state restore. Use the map's default spawn unless the sector engine already tracks a canonical entry descriptor for reload. The command prints `reload queued: <mapId>` before the transition, and the map system logs success/failure afterward.

#### `/quit`

```text
/quit
```

Queue an application quit request. Do not close the window or destroy Lua directly in the command handler. The main loop consumes it after updates and performs normal map/script, console, renderer, audio, and window shutdown. Extra arguments are an error.

## 14. Executing Lua in the current VM

### 14.1 Contract

Any submitted line not beginning with `/` is evaluated in the current map's main Lua state. This is intentionally synchronous and intended for inspection, immediate calls, assignments, function definitions, and scheduling background work.

Examples:

```lua
playerHealth()
getPersistentBool("refinery.visited")
setLightEnabled("hall_light", false)
x = 10
startScript("warningLightLoop")
```

Console chunks cannot yield. Therefore:

- immediate bindings work;
- async start functions that merely return an operation userdata work;
- `startScript()` works because it only queues a task;
- blocking bindings, `delay()`, and `await()` raise their normal "managed task required" error;
- the console never creates an anonymous scheduler task for submitted code.

This keeps evaluation simple and prevents hidden console coroutines from surviving map teardown. To run a blocking sequence, define/use a named map function and submit:

```lua
startScript("mySequence")
```

### 14.2 Availability and phase

Before evaluation, query the scripting system for the active console-evaluable VM. Return an error when:

- no map/VM is active;
- the map script runtime is loading in a phase where main-state evaluation is unsafe;
- shutdown or reload teardown has begun;
- Lua is already executing/reentrant on the same main state.

Suggested messages:

```text
Lua unavailable: no map VM is active
Lua unavailable: map VM is shutting down
Lua console is already executing
```

Evaluation runs only on the main thread during console input update.

### 14.3 Expression-first compilation

Try the input as an expression first by compiling:

```lua
return <submitted text>
```

If that compilation fails, restore the Lua stack and compile the original text as a statement chunk. Important distinction:

- expression compile failure is not shown if statement compilation succeeds;
- expression runtime failure is a real error and must not fall back to statement mode, because running a second interpretation could duplicate side effects;
- if both compilations fail, report the statement compile error, optionally followed by a concise expression-parse note in Debug builds.

Use text-only mode and a useful chunk name:

```cpp
luaL_loadbufferx(L, source.data(), source.size(), "=console", "t");
```

### 14.4 Stack discipline

Never assume the main Lua stack starts at zero, and never clear it globally with `lua_settop(L, 0)`. Save and restore its initial top:

```cpp
const int baseTop = lua_gettop(L);
// load/call/read results
lua_settop(L, baseTop);
```

Use an RAII guard so every return path restores `baseTop`. This prevents the console from destroying unrelated host-owned stack values if evaluation is later invoked from a broader C++ operation.

Call with `lua_pcall`, not `lua_resume`. A yield attempt returns an error because console chunks are non-yieldable.

### 14.5 Tracebacks

Install a traceback message handler below the compiled chunk before `lua_pcall`, or create an equivalent protected-call helper. Runtime output should include the Lua error plus stack traceback with chunk name `console` and referenced Lua source locations.

Do not route the same error through both `TraceLog` and `DebugConsoleAddLine` unless duplicate output is intentionally suppressed. The submission path should add the evaluation error once; logging it externally is optional.

### 14.6 Result formatting

Capture all values returned by the expression/statement using `LUA_MULTRET`. Format values without invoking arbitrary Lua metamethods:

```text
nil
true / false
integer decimal
floating-point with locale-independent formatting
quoted/escaped string
<table 0x...>
<function 0x...>
<thread 0x...>
<userdata Engine.ScriptOperation 0x...>
<lightuserdata 0x...>
```

Use `lua_type`, `lua_toboolean`, `lua_isinteger`, `lua_tointeger`, `lua_tonumber`, `lua_tolstring`, `lua_topointer`, and `luaL_typename`. Do not call global `tostring` or a `__tostring` metamethod during generic formatting; it may have side effects or raise another error.

Escape string control characters and cap each formatted value and the total output (for example, 4096 bytes per value and 16 KiB total). Join multiple returns with four spaces or emit indexed lines:

```text
[1] true
[2] "refinery"
[3] 42
```

Use the indexed multi-line form for clarity. A chunk with no return values adds no result line. An expression returning `nil` does add `[1] nil`.

Tables are not recursively dumped in v1. A future explicit helper such as `/inspect` or a Lua pretty-printer can do bounded traversal with cycle detection.

### 14.7 Evaluation result type

```cpp
struct ScriptConsoleResult {
    bool success = false;
    bool evaluatedExpression = false;
    std::vector<std::string> values;
    std::string error;
};

ScriptConsoleResult ScriptSystemExecuteConsole(
    ScriptRuntime& scripts,
    std::string_view submittedText);
```

The scripting subsystem owns this function because it knows VM phase, registry context, stack discipline, and Lua error conventions. The console only renders the result.

## 15. Lua `print` and logging integration

The map VM should register a console-aware `print` binding. It formats all arguments without throwing for ordinary primitive values, joins them with tabs or four spaces, writes through the engine logger with a `[LUA]` category, and lets the log bridge mirror the line into the console.

Avoid double insertion:

```text
Lua print -> engine logger/Raylib TraceLog -> pending log bridge -> console
```

or:

```text
Lua print -> console directly + external stdout sink directly
```

Choose one route. The first route is recommended because logs are retained outside the overlay and thread/category formatting remains centralized.

If the console evaluates `print("hello")`, the print line may appear during `lua_pcall`; after return, the evaluator produces no result line. The log queue is flushed either immediately after submission (main-thread queue only) or at the next normal frame flush. Preserve sequence ordering as closely as practical.

## 16. Raylib/engine log bridge

### 16.1 Reason for a queue

Raylib's trace callback and the engine logger may be invoked before console initialization or from worker threads. The callback must never mutate console vectors or call rendering functions. It writes a bounded pending record to a thread-safe queue; the main thread flushes records into `DebugConsoleData`.

```cpp
struct PendingConsoleLog {
    uint64_t sequence = 0;
    int raylibLevel = LOG_INFO;
    std::string category;
    std::string text;
};

struct DebugConsoleLogInbox {
    std::mutex mutex;
    std::vector<PendingConsoleLog> pending;
    uint64_t nextSequence = 1;
    uint64_t droppedCount = 0;
    size_t maxPending = 512;
};
```

Use one process-lifetime inbox whose storage outlives the callback. The callback:

1. formats `va_list` into a bounded local buffer with `vsnprintf`;
2. writes the message to stdout/stderr or forwards it to the engine's existing sink;
3. locks only long enough to append the record;
4. drops oldest or newest records under a documented overflow policy;
5. never calls `TraceLog` recursively.

Dropping newest records is cheaper under sustained overload and preserves the first diagnostic cause; dropping oldest keeps the latest state. Either is acceptable, but track `droppedCount` and emit one warning during flush:

```text
[WARNING] 37 console log messages were dropped
```

### 16.2 Main-thread flush

Swap the pending vector into reusable main-thread scratch storage under the mutex, then unlock before adding console lines. Sort by sequence only if multiple producer queues can reorder records; a single locked append queue is already ordered.

Map Raylib levels to console severity:

```text
LOG_TRACE   Trace
LOG_DEBUG   Debug
LOG_INFO    Info
LOG_WARNING Warning
LOG_ERROR   Error
LOG_FATAL   Fatal
```

Prefix lines with level/category once, for example:

```text
[INFO][Lua] initialized refinery
[ERROR][Renderer] shader compilation failed
```

Flush near the start of every frame, once after console initialization, and once before application shutdown. Never hold the inbox mutex while wrapping text, allocating UI cache entries, or rendering.

## 17. Deferred application actions

Commands should request state changes through a small application action queue or flags:

```cpp
enum class DeferredDebugActionType {
    None,
    ReloadCurrentMap,
    QuitApplication
};

struct DeferredDebugAction {
    DeferredDebugActionType type = DeferredDebugActionType::None;
};
```

Only one destructive/navigation action may be pending. `/reload` and `/quit` validate and queue; the main orchestrator consumes after console submission, Lua/script updates, and other unsafe iteration have ended.

Quit should take precedence if the engine receives an OS window-close request in the same frame. A pending reload must be discarded when quitting.

Never store pointers to the current map or VM in the deferred action. Store stable IDs/paths or let `ReloadCurrentMap` resolve the current map descriptor before teardown at the safe point.

## 18. Build and release policy

The console is a powerful developer surface because it executes arbitrary trusted Lua and can request application actions. Recommended policy:

- compile it in Debug and developer builds;
- disable the toggle and Lua evaluation in public Release builds unless an explicit developer flag is enabled;
- do not treat hiding the console key as a security boundary;
- never enable it for untrusted multiplayer clients as an authority mechanism;
- make the build flag obvious, such as `ENABLE_DEBUG_CONSOLE`.

If compiled out, logging must continue through its normal sinks and all console calls should either disappear behind compilation guards or become harmless no-ops without scattering conditionals throughout gameplay.

## 19. Edge cases and required behavior

- **No active map/VM:** slash commands still work; Lua submission reports unavailable.
- **VM changes during frame:** console evaluation completes first; reload is deferred.
- **Console open during reload:** console remains open, but Lua status reads unavailable during teardown/loading and active afterward.
- **Lua blocking call from console:** produces a managed-task/yield error, never a suspended console coroutine.
- **Expression compile fails, statement succeeds:** show no expression parse error.
- **Expression compiles but runtime fails:** show the runtime error once; do not retry as a statement.
- **Statement returns values explicitly:** display them if Lua permits/results are produced.
- **Lua returns many/large values:** enforce result count and byte caps, then append a truncation marker.
- **Lua result formatter encounters userdata:** show type/metatable identity without invoking metamethods.
- **Empty submission:** no output/history change.
- **Whitespace submission:** no output/history change.
- **Unknown slash command:** show `unknown command: /name` and suggest `/help`.
- **Malformed quoted command:** show parse error and execute nothing.
- **History editing:** exits browse mode without changing stored history.
- **Output arrives while scrolled up:** viewport remains anchored.
- **Output limit eviction while scrolled up:** clamp to the oldest retained content and never use a negative/out-of-range offset.
- **Toggle key emits text:** text is suppressed for that frame.
- **Console opens while movement keys are held:** clear movement/look intent; gameplay stays gated until close.
- **Clipboard unavailable/null:** no-op with an optional warning.
- **Invalid UTF-8 paste:** replacement codepoints, no broken caret boundaries.
- **Font load failure:** use Raylib default font and retain all functionality.
- **Very small logical resolution:** clamp panel and visible lines to positive dimensions; input remains visible.
- **Log queue overflow:** no unbounded growth; report dropped count.
- **Recursive logging from callback:** prohibited; callback writes directly to sinks and queue.
- **Application shutdown:** final log flush occurs before font/window destruction.

## 20. Verification plan

### 20.1 Input and editing

1. Grave opens/closes the console and never inserts a backtick.
2. Escape closes without discarding the draft.
3. Console receives input before gameplay and all used events are marked handled.
4. Gameplay polled movement/mouse look is suppressed while open.
5. Opening clears existing movement/look intent and releases captured mouse.
6. Insert, Backspace, Delete, Left, Right, Home, and End work at beginning, middle, and end.
7. Key repeat affects only permitted editing/navigation keys.
8. Unicode insertion, movement, Backspace, Delete, wrapping, and caret placement operate by codepoint.
9. Paste sanitation handles null, multiline, controls, malformed UTF-8, and size limits.
10. Ctrl+L clears output without clearing history or draft.

### 20.2 History and submission

1. Empty/whitespace-only submissions do nothing.
2. Consecutive duplicate submissions create one history entry.
3. Up saves and recalls over an unfinished draft.
4. Down past newest restores draft text and caret.
5. Editing a recalled line exits browsing without mutating stored history.
6. History cap drops oldest entries safely.
7. Slash commands and Lua lines share history.

### 20.3 Output and rendering

1. Logical lines split embedded newlines and sanitize controls.
2. Long words and ordinary sentences wrap without corrupting UTF-8.
3. Resizing/logical-resolution change invalidates and rebuilds layout.
4. Input grows to three visible wrapped lines and keeps caret visible beyond that.
5. Page and wheel scrolling clamp correctly.
6. New output follows tail at bottom and preserves position while scrolled up.
7. Eviction at the line cap preserves valid scroll state.
8. Console renders last over menus and 3D UI at the logical resolution.
9. Font failure uses the default font.

### 20.4 Commands

1. Registry rejects duplicate names and `/help` is generated from metadata.
2. Tokenizer handles whitespace, quotes, escapes, and malformed quotes.
3. `/clear` follows its exact retention policy.
4. `/copylast` copies logical unwrapped text, validates counts, and excludes its own confirmation.
5. `/reload` rejects no-map and already-pending states.
6. `/reload` queues rather than executing immediately, uses normal map/Lua shutdown, recreates a fresh VM, and preserves console state.
7. `/quit` queues normal application shutdown and supersedes reload.
8. Unknown commands produce one useful error.

### 20.5 Lua evaluation

1. No active VM reports a clear error.
2. Literal/arithmetic expressions display all return values.
3. Assignments and function definitions execute through statement fallback.
4. Expression compile failure followed by valid statement does not leak an error.
5. Expression runtime failure is not executed again as a statement.
6. Lua errors include a traceback and leave the main stack at its original top.
7. Strings, nil, bools, integers, floats, functions, tables, threads, light userdata, and operation userdata format safely.
8. Result caps and truncation work.
9. Immediate engine bindings and `startScript()` work.
10. Blocking commands, `delay`, and `await` fail clearly without leaving tasks/operations behind.
11. A console-triggered deferred map change never closes the currently executing VM inside `lua_pcall`.
12. Lua `print` appears once in console and normal logs.

### 20.6 Log bridge

1. Logs emitted before console initialization appear after first flush.
2. Main- and worker-thread logs reach the inbox without touching UI state.
3. Severity/category colors and prefixes are correct.
4. Queue overflow stays bounded and reports the dropped count once.
5. Flush swaps under lock and performs console allocations after unlocking.
6. Final logs flush before shutdown destroys the font/window.
7. Callback does not recurse or double-print.

## 21. Implementation order

Implement in verifiable passes:

1. Add console state, font lifetime, bounded logical output, and basic overlay rendering.
2. Integrate toggle/input capture before gameplay and implement Unicode editor operations.
3. Add wrapping, caret layout, scrolling, history, paste, and submission echo.
4. Add the command registry and `/help`, `/clear`, `/copylast`, `/reload`, `/quit`.
5. Add deferred reload/quit consumption in the main orchestrator.
6. Add synchronous expression-first Lua evaluation with stack guards, tracebacks, and bounded result formatting.
7. Route Lua `print` and Raylib/engine logs through the non-duplicating thread-safe log bridge.
8. Add debug/release gating and complete the verification matrix.

Compile and manually exercise each pass in a Debug build. Test `/reload` and Lua evaluation early, because VM lifetime and deferred teardown are the highest-risk integration points.

## 22. Final invariants

The implementation is complete only while all of these remain true:

- Console data, output, and history are game-global and survive map VM recreation.
- Lua evaluation always targets the current VM and never retains it afterward.
- Console Lua chunks execute synchronously and cannot yield.
- Console evaluation restores the Lua stack to its original top on every path.
- A failed expression is retried as a statement only after compile failure, never after runtime failure.
- `/reload` and `/quit` are deferred and never destroy state inside input dispatch or Lua execution.
- Console input is processed before gameplay, handled events are consumed, and direct gameplay polling is gated while open.
- Text editing and wrapping never split UTF-8 codepoints.
- Worker threads and log callbacks never mutate console UI state or call Lua.
- Log and console buffers have explicit upper bounds.
- Scrolling remains anchored when new output arrives above the tail.
- Output wrapping is derived from logical lines and current render width/font.
- Lua `print` and engine logs appear once, not through two competing insertion paths.
- Rendering occurs as the final logical-resolution UI overlay.
- Console shutdown unloads its font before Raylib/window teardown.
