# Lua Scripting API

This is the compact reference for globals added by the engine and sector game.
The VM also opens the standard Lua 5.5 libraries. Scripts are trusted game
content, not sandboxed code.

## Script files and lifecycle

A runtime level finishes its asset/runtime-object/navigation loading gate and
250 ms loading-screen fade before creating the map Lua runtime. It then loads
the `.lua` file beside its map JSON and calls `init()`. These global hooks are
optional:

```lua
function init()
    -- Runs after the level loading screen has faded. May block/yield.
end

function shutdown()
    -- Runs once before map teardown. Must not yield.
end
```

An authored trigger calls the global function named in its `script` field.
Other global functions can be launched with `startScript()`.

Use standard `require("module.name")` for modules. Lookup is map-local first,
then shared under `assets/scripts`; native modules are disabled:

```text
<map directory>/?.lua
<map directory>/?/init.lua
assets/scripts/?.lua
assets/scripts/?/init.lua
```

Blocking functions may only run inside `init()`, a trigger/event hook, or a
function launched by `startScript()`. Expected runtime failures use
`false, reason` (or `nil, reason` for async starters). Invalid argument types
raise normal Lua errors.

`FrameDelta` is a read-only global containing the current script update delta
in seconds:

```lua
elapsed = elapsed + FrameDelta
```

## Level audio

Map audio IDs come from the level's **Sound Editor**. Roomtones use entries
authored as `Music (streaming)`; the APIs below use buffered `Sound`
entries. Invalid IDs or assets that are not ready return `false, reason`.

### `playMapSound(soundId [, volume [, pitch]]) -> true | false, reason`

Plays a non-positional one-shot. Volume defaults to `1.0` and is limited to
`0.0..1.0`; pitch defaults to `1.0` and is limited to `0.01..4.0`.

```lua
playMapSound("light_switch_click", 0.8, 1.05)
```

### `playSoundEmitter(emitterId [, volume [, pitch]]) -> true | false, reason`

Plays the positional Sound Emitter with the stable string ID authored in the
editor. Emitters accept buffered Sound entries or independently streamed Music
entries; both use positional attenuation, stereo pan, and occlusion. Omitted
volume uses the emitter's authored volume. A looping emitter continues until
stopped; a non-looping emitter plays once. Calling this for an already-playing
loop updates its volume and pitch without restarting it.

```lua
playSoundEmitter("generator_motor")
playSoundEmitter("steam_vent", 0.65, 0.9)
```

### `stopSoundEmitter(emitterId) -> true | false, reason`

Stops the emitter if it is playing and prevents an authored looping emitter
from automatically restarting. Calling it again is harmless.

```lua
stopSoundEmitter("generator_motor")
```

## Tasks and timing

### `delay(milliseconds) -> true`

Suspends the current task for a finite, non-negative duration. `delay(0)`
resumes no earlier than the next script update.

```lua
delay(250)
```

### `startScript(functionName) -> true | false, reason`

Queues a named global function as a background task. It begins on the next
script update. A queued or running function cannot be started again.

```lua
local ok, reason = startScript("alarmLoop")
```

### `stopScript(functionName) -> true | false, reason`

Cooperatively cancels a queued or active named task at the next safe scheduler
point.

```lua
local stopped, reason = stopScript("alarmLoop")
```

### `stopAllScripts() -> true`

Stops all foreground and background tasks, including the caller. It does not
invoke `shutdown()`.

```lua
stopAllScripts()
```

### `isScriptRunning(functionName) -> boolean`

Returns true for queued, running, waiting, and stop-requested tasks that have
not yet been reclaimed.

```lua
if isScriptRunning("alarmLoop") then
    stopScript("alarmLoop")
end
```

### `isLoadingSave() -> boolean`

Reports whether the current map is being initialized from saved state.

```lua
if not isLoadingSave() then
    setPersistentBool("introSeen", true)
end
```

## Operations

Async gameplay starters return a `ScriptOperation`. Operations have the states
`pending`, `succeeded`, `failed`, `cancelled`, and `stale`.

### `await(operation) -> true [, values...] | false, reason`

Waits for an operation. Only one task may wait on a pending operation at once.

```lua
local movement = startMoveDoor(42, 1.0, 500)
local ok, reason = await(movement)
```

### `operationStatus(operation) -> state [, reason]`

Reads an operation without waiting. Failed and cancelled operations may also
return a reason.

```lua
local state, reason = operationStatus(movement)
```

### `cancelOperation(operation) -> true | false, reason`

Cancels a pending operation and its backend action. Completed or stale
operations cannot be cancelled.

```lua
local cancelled, reason = cancelOperation(movement)
```

## Persistent values

Persistent values survive map VM replacement and participate in save/load.
Keys must be non-empty strings. Bool, integer, and string values use separate
typed stores.

### `setPersistentBool(key, value)` / `getPersistentBool(key [, default]) -> boolean`

```lua
setPersistentBool("generatorOn", true)       -- no return value
local value = getPersistentBool("generatorOn", false)
```

The getter default is `false`.

### `setPersistentInt(key, value)` / `getPersistentInt(key [, default]) -> integer`

```lua
setPersistentInt("keycards", 2)              -- no return value
local count = getPersistentInt("keycards", 0)
```

The getter default is `0`.

### `setPersistentString(key, value)` / `getPersistentString(key [, default]) -> string`

```lua
setPersistentString("lastMap", "refinery")  -- no return value
local map = getPersistentString("lastMap", "")
```

The getter default is `""`.

## Logging

### `log(...)` / `print(...)`

Writes one informational log line. Arguments are separated by tabs. `print` is
an engine-provided alias with the same behavior.

```lua
log("door", 42, "opened")
print("health", 100)
```

## Doors

Doors have a stable, editor-visible string `instanceId`. The older positive
integer `placedObjectId` is still accepted by `moveDoor` and `startMoveDoor`
for compatibility.
`targetFraction` is from `0.0` (closed) to `1.0` (open), and `durationMs` is
finite and non-negative. A zero duration normally completes immediately.
Closing is deferred while NPC navigation holds the door open. A zero-duration
close is rejected in that state because it cannot safely bypass the physical
door sweep; a timed close waits until the final NPC hold is released.

### `moveDoor(doorId, targetFraction, durationMs) -> true | false, reason`

Blocks until the door reaches its target or the move fails.

```lua
local ok, reason = moveDoor("main_airlock", 1.0, 750)
```

### `startMoveDoor(doorId, targetFraction, durationMs) -> operation | nil, reason`

Starts the same move asynchronously.

```lua
local doorMove, reason = startMoveDoor("main_airlock", 0.0, 500)
```

Only one scripted move may control a door at a time.

### `openDoor(doorId)`, `closeDoor(doorId)`, `toggleDoor(doorId)`

Sets the normal authored door motion target and returns `true`, or
`false, reason`. These non-blocking script controls use the stable string ID
and intentionally bypass the player-use permission callbacks.

```lua
openDoor("main_airlock")
toggleDoor("service_hatch")
```

Manual doors participate in the E-key Use prompt. Their optional
`canOpenScript` and `canCloseScript` inspector callbacks gate only player use.
A callback may yield and must eventually return boolean `true` to allow the
requested open/close; `false`, no return value, a missing function, or an error
denies it. A blank callback preserves the default engine behavior.

## World item pickup callbacks

An authored item's optional `onTakeScript` field names a global Lua function
called with no arguments when the player presses E on that item. The engine
checks that the complete placement quantity fits before starting the callback.
The callback may yield and must eventually return boolean `true` to permit the
pickup. Boolean `false`, no boolean return value, a missing function, or a
script error leaves the item in the world. A blank callback permits pickup
immediately.

Capacity and item identity are checked again after a yielding callback. Pickup
remains atomic: the complete quantity is added or no inventory/world state is
changed. Foreground callbacks are serialized, and the same item cannot start a
second pickup while its callback is pending.

```lua
function canTakePistolAmmo()
    delay(100)
    return alarmDisabled
end
```

## Carried Object use callbacks

An Object placement may provide `onUseScript`. After that item is picked up,
its inventory Use action enters cursor-targeting mode. Left-clicking a visible,
ready static or dynamic prop calls the named global function with that prop's
stable string instance ID. Doors, NPCs, world items, and arbitrary world
surfaces are not Object-use targets.

The callback may yield. Its first return value must be boolean `true` to consume
exactly one carried Object entry. Boolean `false`, no boolean return value, a
missing function, or an error keeps the Object. While a yielding call is
pending, gameplay controls remain locked and the call cannot be cancelled or
started a second time. When the callback finishes, normal gameplay controls are
restored.

```lua
function useAccessCard(targetInstanceId)
    delay(100)
    if targetInstanceId == "security_console" then
        setPersistentBool("security_unlocked", true)
        return true
    end
    return false
end
```

This callback is separate from a dynamic prop's own no-argument
`onUseScript`, which continues to run from the centered E-key Use interaction.

## Dynamic props and animation

A dynamic prop becomes usable when its `onUseScript` inspector field names a
global Lua function. `useTitle` supplies the text in `Use <title>`, and
`useDistance` controls its reach. The E-key resolver chooses the eligible prop
or manual door closest to the center of the player's view. A `singleUse` prop
is consumed after its callback starts successfully. Callback return values are
ignored. Foreground callbacks are serialized, so a yielding callback cannot be
started again while it is still running.

Animation functions use the prop's stable string `instanceId`. Animation names
are the exact, case-sensitive clip names imported from its model. Omitting the
name keeps the currently selected clip.

### `playPropAnimation(propId [, animationName [, mode]]) -> true | false, reason`

Restarts and plays a clip. `mode` defaults to `"once"` and accepts `"once"`,
`"once_reverse"`, `"loop"`, or `"loop_reverse"`.

```lua
playPropAnimation("wall_switch", "switch|switchAction", "once_reverse")
playPropAnimation("ceiling_fan", "Ventilator", "loop")
```

### `pausePropAnimation(propId) -> true | false, reason`

Pauses at the current frame.

### `resumePropAnimation(propId) -> true | false, reason`

Continues the current clip with its current playback mode and direction.

### `stopPropAnimation(propId) -> true | false, reason`

Stops playback and returns the current clip to its initial frame.

### `setPropAnimationProgress(propId, progress [, animationName]) -> true | false, reason`

Selects an optional clip, seeks to normalized progress from `0.0` to `1.0`,
and leaves it paused. This also provides explicit initial/last-frame control.

```lua
setPropAnimationProgress("wall_switch", 0.0)
setPropAnimationProgress("wall_switch", 1.0, "switch|switchAction")
```

## 3D prop emission

Static 3D props and dynamic props share one stable `instanceId` namespace for
model presentation controls. Both inspectors expose the ID. New and legacy
static props receive a deterministic `prop_<objectId>` ID when needed.

### `setPropEmissiveScale(propId, scale) -> true | false, reason`

Multiplies every glTF emissive material in one prop instance. `0` turns model
emission off, `1` restores the model-authored value, fractional values dim it,
and values above `1` boost it. The scale must be a finite, non-negative float.
The model must already contain an emissive factor or texture; this function
does not infer an emissive mask from ordinary base color.

Prop and dynamic-light IDs use separate namespaces, so a lamp may deliberately
give both objects the same ID:

```lua
local function setHallLamp(enabled)
    setDynamicLightEnabled("hall_lamp", enabled)
    setPropEmissiveScale("hall_lamp", enabled and 1.0 or 0.0)
end
```

## Dynamic lights

Point, spot, and rectangular dynamic lights share one global stable-ID
namespace. Their `instanceId` is editable in the inspector. Mutations refresh
runtime dynamic lighting and atmosphere effects; disabling a light therefore
also removes its halo, shaft, and dust presentation.

### `setDynamicLightEnabled(lightId, enabled) -> true | false, reason`

### `setDynamicLightIntensity(lightId, intensity) -> true | false, reason`

Intensity must be finite and non-negative.

### `setDynamicLightColor(lightId, red, green, blue) -> true | false, reason`

Color channels are integer values from 0 through 255.

```lua
setDynamicLightEnabled("light_spot_12", false)
setDynamicLightIntensity("warning_light", 3.5)
setDynamicLightColor("warning_light", 255, 40, 20)
```

## Actor health

Health values are integers from `0` through the actor's current maximum.
Out-of-range values raise a Lua argument error.

### `setPlayerHealth(health) -> true | false, reason`

Sets the player's current health. A value of `0` depletes the player's health.

```lua
setPlayerHealth(75)
```

### `setNpcHealth(instanceId, health) -> true | false, reason`

Sets the current health of the placed NPC with the exact, case-sensitive
instance ID. Setting health to `0` kills the NPC and stops its navigation.
Dead NPCs cannot be revived by setting a positive value.

```lua
local ok, reason = setNpcHealth("guard_1", 25)
```

## NPC movement

`instanceId` is a placed NPC's unique instance ID. `gait` is optional and is
`"walk"` by default; `"run"` is also supported.

Coordinate destinations use runtime world X/Z:

```lua
moveNpc("guard_1", 12.0, 8.0, "run")
local movement = startMoveNpc("guard_1", 4.0, 8.0)
```

A level-marker ID can replace X/Z. IDs are exact and case-sensitive. The
marker is resolved once at request start; its authored position is converted
to world X/Z. Marker height and yaw are ignored.

```lua
moveNpc("guard_1", "patrol_end", "run")
local movement = startMoveNpc("guard_1", "guard_post")
```

### Blocking forms

```text
moveNpc(instanceId, x, z [, gait]) -> true | false, reason
moveNpc(instanceId, levelMarkerId [, gait]) -> true | false, reason
```

They resume only after collision-constrained locomotion physically arrives or
reports a terminal failure.

Loops must check the returned boolean and exit, yield, or back off after a
failure. A missing or removed NPC makes later requests fail immediately. As a
last-resort safeguard, each managed script start/resume has a budget of
1,000,000 Lua VM instructions; exceeding it terminates that task with an
`instruction budget exceeded` error instead of freezing the game thread.

### Async forms

```text
startMoveNpc(instanceId, x, z [, gait]) -> operation | nil, reason
startMoveNpc(instanceId, levelMarkerId [, gait]) -> operation | nil, reason
```

Only one script-owned move may control an NPC at a time. Use `await`,
`operationStatus`, or `cancelOperation` with the returned operation.

```lua
local movement, reason = startMoveNpc("guard_1", "patrol_end", "run")
if not movement then
    print("could not start guard move: " .. reason)
    return
end

local arrived, failure = await(movement)
if not arrived then
    print("guard move ended: " .. failure)
end
```

Navigation must be ready when the request starts. Rebuilds, map unload, NPC
deletion, unreachable destinations, capacity limits, and prolonged stalls end
the operation with a reason. Cancelling an operation also releases any door
hold owned by that move.

## Map travel

### `changeMap(mapId [, spawnId]) -> true | false, reason`

Requests a map change. Map IDs may contain letters, digits, `_`, and `-`.
`spawnId`, when provided, must be non-empty and identifies the destination
level marker. The first accepted request wins.

```lua
local ok, reason = changeMap("refinery", "west_entry")
```

## Triggers

Trigger IDs are exact authored trigger IDs.

### `enableTrigger(triggerId) -> true | false, reason`

```lua
local ok, reason = enableTrigger("reinforcements")
```

### `disableTrigger(triggerId) -> true | false, reason`

```lua
local ok, reason = disableTrigger("intro_once")
```
