# Lua Scripting API

This is the compact reference for globals added by the engine and sector game.
The VM also opens the standard Lua 5.5 libraries. Scripts are trusted game
content, not sandboxed code.

## Script files and lifecycle

A map loads the `.lua` file beside its map JSON. These global hooks are
optional:

```lua
function init()
    -- Runs when the map loads. May use blocking/yielding functions.
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

`placedObjectId` is the positive integer ID of a placed door.
`targetFraction` is from `0.0` (closed) to `1.0` (open), and `durationMs` is
finite and non-negative. A zero duration normally completes immediately.
Closing is deferred while NPC navigation holds the door open. A zero-duration
close is rejected in that state because it cannot safely bypass the physical
door sweep; a timed close waits until the final NPC hold is released.

### `moveDoor(placedObjectId, targetFraction, durationMs) -> true | false, reason`

Blocks until the door reaches its target or the move fails.

```lua
local ok, reason = moveDoor(42, 1.0, 750)
```

### `startMoveDoor(placedObjectId, targetFraction, durationMs) -> operation | nil, reason`

Starts the same move asynchronously.

```lua
local doorMove, reason = startMoveDoor(42, 0.0, 500)
```

Only one scripted move may control a door at a time.

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

### Async forms

```text
startMoveNpc(instanceId, x, z [, gait]) -> operation | nil, reason
startMoveNpc(instanceId, levelMarkerId [, gait]) -> operation | nil, reason
```

Only one script-owned move may control an NPC at a time. Use `await`,
`operationStatus`, or `cancelOperation` with the returned operation.

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
