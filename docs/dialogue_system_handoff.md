# Lua dialogue system handoff for the 3D FPS engine

This describes the implementation in the `adventure` engine, inspected on 2026-09-10. It is self-contained so it can be pasted into another coding-agent session without access to this repository. Source paths at the end are for reference. Recommendations for the FPS engine are explicitly separated from existing behavior.

## Implementation request for the receiving agent

Implement dialogue with the same Lua authoring experience described below, integrating it into the FPS engine's existing, more generalized scripting runtime. Preserve sequential, coroutine-backed speech calls, individually skippable lines, choice menus returning stable IDs, ordinary Lua branching and side effects, dynamic topic filtering, and nonblocking ambient speech. Adapt speakers, presentation, and input to the FPS engine. Use its existing task/wait infrastructure where appropriate.

The essential division of responsibility is: **Lua owns conversation flow; the engine presents speech and choices and resumes the waiting script when they complete.**

## 1. What writing a conversation feels like

Scripts look synchronous, but speech, movement, delays, and choices yield the current managed Lua coroutine. The game continues updating and rendering. Local variables, function calls, closures, loops, and branches survive the suspension.

For example, using this engine's API:

```lua
function Scene_use_actor_hotel_clerk()
    walkToHotspot("desk")
    face("back")
    sayActor("hotel_clerk", "Yes?")

    local choice = dialogue("hotel_clerk_intro")
    if choice == "need_room" then
        say("I need a room for the night.")
        sayActor("hotel_clerk", "No rooms. We are closed for business, sir.")
        setFlag("hotel_room_denied", true)
    elseif choice == "goodbye" then
        say("Never mind.")
    end

    return true
end
```

Each speech call finishes before the next statement runs. The player can click to finish a line early. Choosing a menu option returns its ID; the script explicitly decides whether to speak that option's text, say something else, show another menu, change game state, or finish.

There is no requirement to encode conversation flow in a separate dialogue graph. Choice assets contain labels and IDs; conditions, responses, loops, and consequences are Lua code.

## 2. Speech API

### Blocking speech

Here, “blocking” means suspending the calling coroutine, not blocking the engine thread.

| Function | Speaker / anchor |
| --- | --- |
| `say(text, ...)` | Currently controlled actor |
| `sayActor(actorId, text, ...)` | Named actor |
| `sayProp(propId, text, ...)` | Named scene prop, such as a radio |
| `sayAt(x, y, text, ...)` | Explicit 2D world position |

The optional suffix is either a duration in milliseconds, a named color, or a named color followed by a duration:

```lua
say("Hello.")
say("Read this a little longer.", 3000)
sayActor("hotel_clerk", "Yes?", "CYAN")
sayProp("radio", "Transmission received.", "GREEN", 2500)
sayAt(850, 420, "A voice in the distance.", 2000)
```

Colors are case-insensitive palette names. The engine also registers palette constants such as `CYAN`. Actor speech defaults to the actor definition's talk color; props and explicit positions default to white.

Current implementation quirk: `say()` accepts and validates the color argument but does not forward it; it always uses the controlled actor's talk color. `startSay()` has the same issue. The other speaker variants do forward overrides. This is an inconsistency to fix when porting, not a behavior to preserve.

Successful blocking speech returns `true` when resumed, whether timed out or skipped. Ordinary start failures return `false`; incorrect required argument types can raise Lua argument errors. Scripts generally use these calls as statements. The API does not report whether the player skipped a line.

### Nonblocking ambient speech

The matching functions are:

```lua
startSay(text, ...)
startSayActor(actorId, text, ...)
startSayProp(propId, text, ...)
startSayAt(x, y, text, ...)
```

They return a success boolean immediately and use a separate ambient speech collection. They do not register a speech wait. Thus an NPC can speak while moving or while another script continues:

```lua
startSayActor("hotel_clerk", "One moment.")
walkActorToHotspot("hotel_clerk", "desk")
```

Ambient lines are unskippable and do not themselves block normal world interactions. Up to six coexist; adding another removes the oldest. They are rendered smaller than primary speech. These functions return no speech handle, and there is no exposed operation to await or cancel one particular ambient line.

## 3. Timing and exactly what skipping does

Primary speech occupies one global `SpeechUiState` containing its anchor, speaker indices or world position, text, color, elapsed time, duration, skippability, and fade durations.

Without a nonnegative explicit duration, timing is:

```text
durationMs = clamp(750 + text.size() * 45, 1400, 7000)
```

`text.size()` is the C++ string's byte count, not a Unicode character count. A nonnegative override is used directly. Primary speech has 50 ms fade-in and fade-out inside that duration. Ambient speech adds those two fades to the computed or explicit base duration.

Every update advances the timer by `dt * 1000`. Expiration clears the primary speech state. For a skippable primary line, an unconsumed left or right mouse click also clears that state immediately and consumes the event. Skipping does not wait for a fade-out.

The scheduler sees that speech is no longer active and resumes the waiting coroutine. This uses the same completion path as timeout:

```text
say("First line") -> start primary speech -> yield
    timeout OR skip click -> clear speech -> scheduler resumes caller
say("Second line") -> start next speech -> yield
```

Consequences to preserve:

- A skip completes only the currently displayed line. It does not abort the conversation or fast-forward the whole script.
- Code after the line still executes, including inventory changes, flags, animations, or a nested menu.
- The consumed click does not become a world action or select the next menu in the normal sequential path.
- A later `delay(1000)` remains a separate timer wait; skipping speech does not skip it.
- Speech shows the full text with alpha fades. There is no typewriter reveal or “first click reveals, second click advances” phase.

All exposed blocking speech variants start skippable primary speech. Although C++ has a `skippable` field and parameter, there is no Lua per-line skippability parameter. Ambient speech is explicitly unskippable. Optional Lua control over skippability would be an extension.

## 4. Choice menus: data and return value

At startup the engine loads `.json` files directly inside `assets/dialogue/`. A file can contain several choice sets:

```json
{
  "choiceSets": [
    {
      "id": "guard_topics",
      "options": [
        { "id": "identity", "text": "Who are you?" },
        { "id": "clearance", "text": "I have authorization." },
        { "id": "goodbye", "text": "That's all." }
      ]
    }
  ]
}
```

Choice-set IDs are globally unique across loaded files; option IDs are unique within a set. The loader checks required IDs, nonempty labels, duplicate IDs, and nonempty option arrays. Lua addresses the set ID, not the filename. Option order follows the JSON array.

```lua
local choice = dialogue("guard_topics")
local filteredChoice = dialogue("guard_topics", { "identity", "clearance" })
```

`dialogue()` opens the menu and yields until a selection supplies an option ID string. A missing set, an already active menu, invalid optional-list input, or no visible options produces an immediate `nil` in the normal managed-call path. Handle `nil` without entering an endless retry loop.

The optional second argument is an array of IDs to hide, not a boolean map. Filtering is local to this invocation and does not mutate the loaded asset. Unknown hidden IDs have no effect. Hidden entries disappear; there is no disabled-option state. Selecting an option neither speaks its label automatically nor marks it as already used.

The current UI centers the visible choices near the bottom of the screen, highlights the hovered row, and accepts either left or right mouse click. A click outside the rows is consumed without selecting anything. There is no keyboard/controller selection or user cancellation implemented here. Leaving a conversation normally means selecting a scripted `goodbye` option.

Selection sets `resultReady` and stores `selectedOptionId`. The scheduler copies that result, clears the menu state, and resumes the waiting coroutine with the string. Clearing before resuming allows the script to open a nested menu immediately.

## 5. Repeated topics and dynamic filtering

The engine installs small Lua helpers under `Adv`:

| Helper | Behavior |
| --- | --- |
| `Adv.appendIf(list, condition, value)` | Appends if true; returns the list. |
| `Adv.hiddenOptions(map)` | Converts truthy `{ optionId = shouldHide }` entries into an ID array. |
| `Adv.runConversation(setId, handlers, hiddenOptions)` | Repeatedly shows a menu using the supplied hidden list and dispatches by ID. |
| `Adv.runConversationDynamic(setId, handlers, hiddenOptionsFn)` | Recomputes the hidden list before every menu display. |

A handler runs inside the same coroutine and can yield through speech, movement, delays, or nested `dialogue()` calls. Returning normally, including returning `nil`, redisplays the topic menu. Returning the string `"exit"` or `"break"` ends the helper and returns the selected choice ID. An unhandled choice also ends the helper and returns that ID. A `nil` result from `dialogue()` ends the helper with `nil`.

Representative example using the JSON above and existing API conventions (the `guard` actor and gameplay flag would need to exist):

```lua
function Scene_use_actor_guard()
    sayActor("guard", "State your business.")

    Adv.runConversationDynamic("guard_topics", {
        identity = function()
            say("Who are you?")
            sayActor("guard", "Station security.")
            setFlag("asked_guard_identity", true)
        end,

        clearance = function()
            say("I have authorization.")
            sayActor("guard", "You may pass.")
            setFlag("guard_granted_access", true)
            return "exit"
        end,

        goodbye = function()
            say("That's all.")
            return "exit"
        end
    }, function()
        return Adv.hiddenOptions({
            identity = flag("asked_guard_identity"),
            clearance = not flag("has_clearance")
        })
    end)

    return true
end
```

The identity topic disappears after its response. The clearance topic appears only when game state permits it. The goodbye option remains available. Filtering is evaluated between menu displays, not continuously while a menu is already open.

The dynamic helper is effectively this ordinary Lua function:

```lua
function runConversationDynamic(setId, handlers, hiddenOptionsFn)
    while true do
        local hidden = nil
        if hiddenOptionsFn ~= nil then
            hidden = hiddenOptionsFn()
        end

        local choice = dialogue(setId, hidden)
        if choice == nil then return nil end

        local handler = handlers and handlers[choice]
        if handler == nil then return choice end

        local result = handler(choice)
        if result == "exit" or result == "break" then
            return choice
        end
    end
end
```

Actual game scripts use this to hide previously discussed topics, unlock questions after discoveries in other scenes, and branch into a second choice set inside a response. They also stop an NPC's pacing script before talking and restart it afterward. Conversation logic can call any other exposed Lua gameplay function.

## 6. Coroutine/runtime integration

Scene hooks such as `Scene_use_actor_hotel_clerk` start as managed foreground coroutines. Only one foreground coroutine may be active at once; another foreground hook receives a busy result. `startScript(functionName)` queues a background coroutine, allowing ambient behavior to run concurrently. Queued starts are processed during scheduler updates.

Each managed coroutine stores a Lua thread, a registry reference keeping it alive, its function name, foreground/stop flags, a wait type, wait-specific data, and an optional string to pass back on resume.

The wait types are `None`, `WalkComplete`, `SpeechComplete`, `DelayMs`, and `DialogueChoice`. A coroutine is registered **before its first resume**, so a yielding API call made immediately on entry can find its owner.

Blocking speech follows this binding pattern:

1. Validate arguments and start the engine's speech UI.
2. Find the managed coroutine by its `lua_State*` and record `SpeechComplete`.
3. Call `lua_yieldk` with a continuation that eventually returns `true`.
4. On an update where primary speech is inactive, clear the wait and call `lua_resume`.
5. Continue executing Lua until it yields again, returns, or errors.

Choices use a continuation that returns the selected string. The scheduler pushes the stored ID onto the suspended thread and resumes with one argument. It consumes at most one dialogue result per update.

Finished or failed coroutines are marked for cleanup; their registry references are released. `stopScript()` requests coroutine termination. Yielding APIs need a managed coroutine: top-level file execution and ordinary console evaluation use `lua_pcall` and are not valid places to author a waiting conversation.

In the adventure update, inventory input runs first, then choice input, speech skip/timers, normal adventure actions when permitted, world movement/animation and other updates, and finally the script scheduler. A skip can therefore resume the script and start its next line that frame, after the skip-input stage has already run.

## 7. Input, presentation, and persistence

Primary speech or an active choice menu suppresses normal world action intake/processing in the adventure update. Choice menus also suppress inventory interaction. Primary speech alone does not fully suppress inventory input, which runs before skip handling and can consume a click first. Existing actor movement and other simulation updates continue while dialogue waits.

`disableControls()` / `enableControls()` are explicit gameplay input switches, separate from speech and choice completion. Scripts use them around staging sequences such as walking to an NPC. Skip and choice input are not gated by that switch. There is no automatic conversation camera or automatic talk animation; scripts can orchestrate exposed camera, movement, animation, audio, and effect calls.

Speech is rendered as wrapped, shadowed text above the speaker's screen rectangle or a world-position anchor, with screen-edge clamping and overlap adjustment. The player anchor resolves the currently controlled actor at render time. Actor and prop speech use stored indices. This is 2D presentation plumbing; the coroutine contract does not depend on it.

Conversation memory belongs to game state: `flag` / `setFlag`, `getInt` / `setInt`, `getString` / `setString`, inventories, and scene state. The save system serializes these script maps. It does not serialize suspended Lua stacks or the current dialogue position. Scene changes shut down the old scripting VM and clear speech/choice UI; save restoration also clears transient dialogue UI. Use persistent flags to reconstruct conversation progress across loads.

## 8. Limitations to improve in the generalized FPS runtime

These are observations about the source, not additional features already implemented:

- **Global primary speech has no owner.** Starting another primary line replaces the current line. Every `SpeechComplete` wait watches the same global active flag. Foreground serialization reduces collisions, but background scripts can still interfere. Use the FPS engine's operation handles or task IDs to associate completion with the correct caller, and define how competing primary requests are serialized or rejected.
- **Choice results have no explicit owner.** The first eligible waiting coroutine consumes the global result. Prefer an owned choice request with exactly one completion and a result delivered to its requester.
- **Cancellation is incomplete.** `CancelDialogueChoice()` only clears UI state. The scheduler waits for `resultReady`, so clearing a menu alone does not resume its waiter with `nil`. Stopping a coroutine does not itself clean up its UI. The older scripting guide's statement that `dialogue()` resumes when the menu closes is broader than the implementation. Define cancellation, script errors, scene unload, and speaker destruction explicitly in the FPS version.
- **Starting and registering a wait are not atomic.** Bindings start the UI before checking managed ownership. A call outside a managed coroutine can leave UI active and return failure. Validate the calling context first or roll back a failed registration.
- **Control restoration is manual.** An error after `disableControls()` can bypass `enableControls()`. Prefer scoped input ownership or guaranteed cleanup in the generalized runtime.
- **Dialogue is text-driven.** Speech state has no voice clip, localization key, lip sync, or audio-completion wait. Separate audio functions exist, but speech does not automatically synchronize or stop them on skip. Add those only if the target engine needs them.

For FPS presentation, keep the script contract while mapping actor/prop IDs to stable entity handles and position anchors to 3D coordinates. Use the target engine's subtitle UI or project a speaker anchor into screen space as appropriate. Explicitly choose which FPS actions remain available during primary dialogue: movement, look, fire, interact, and menu navigation need their own input policy. Route the advance/select action to dialogue and consume it before gameplay handles it.

Use configurable input bindings and keyboard/controller choice navigation as appropriate. An optional options table for duration, color, skippability, or voice can extend the API while retaining simple calls such as `sayActor("guard", "Halt.")`. These are adaptation suggestions; exact names and types should follow the FPS engine's established conventions.

## 9. Behavioral checks for the port

1. Two sequential speech calls display in order; the engine keeps updating while Lua waits.
2. Timeout and a single advance press both complete the current line exactly once and execute subsequent Lua side effects.
3. The advance input does not also skip the newly started line, select a newly opened menu, fire, or interact with the world.
4. A choice returns its stable ID, and a handler can immediately show a nested choice menu.
5. A handler can yield several times and then return to the outer topic menu with its locals intact.
6. Dynamic filtering reflects flags changed by the previous response; labels and asset data remain unchanged.
7. Missing/all-hidden choice sets terminate predictably instead of hanging or spinning.
8. Ambient speech returns immediately and coexists with primary speech without completing its wait.
9. Concurrent scripts cannot replace another caller's speech or steal its choice result under the target engine's chosen ownership policy.
10. Cancellation, errors, scene changes, and invalid speakers release waits/UI/input ownership; save/load reconstructs progress from persistent game state.

## Source map in the adventure repository

| File | What to inspect |
| --- | --- |
| `sources/scripting/ScriptSystemLuaApi.cpp` | Speech/ambient/choice bindings, optional argument parsing, Lua continuations. |
| `sources/scripting/ScriptSystem.cpp` | Managed coroutine start/resume, wait registration, scheduler, built-in `Adv` helpers. |
| `sources/scripting/ScriptData.h` | Coroutine records and wait types. |
| `sources/adventure/AdventureScriptCommands.cpp` | Speaker lookup, colors, primary versus ambient start operations. |
| `sources/adventure/AdventureInternal.cpp` | Duration formula and speech initialization. |
| `sources/adventure/AdventureData.h` | Speech state, choice assets, and choice UI state. |
| `sources/adventure/AdventureUpdate.cpp` | Skip event consumption, expiration, update order, and world-input gating. |
| `sources/adventure/Dialogue.cpp` | Choice filtering, selection/result state, and menu rendering. |
| `sources/adventure/DialogueChoiceAsset.cpp` | Choice JSON loading and validation. |
| `sources/render/UiRender.cpp` | Speech positioning, wrapping, and fades. |
| `sources/adventure/InventoryUi.cpp` | Inventory input priority and dialogue gating. |
| `sources/scene/SceneLoad.cpp`, `sources/save/SaveGame.cpp` | Scene lifecycle and persistence boundaries. |
| `assets/scenes/hotel_lobby/scene.lua` | Dynamic conversation, nested choices, quest flags, NPC pacing control. |
| `assets/scenes/store/scene.lua` | Topic filtering based on cross-scene discoveries. |
| `assets/dialogue/hotel_clerk.json` | Real choice-set asset. |

The existing `doc/adventure_scripting_guide_current.md` offers broader scripting documentation, but this handoff follows the inspected implementation where behavior differs.
