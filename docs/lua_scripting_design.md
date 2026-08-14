# Level-Scoped Lua Scripting Design

## 1. Purpose

This document specifies a Lua scripting subsystem for a C++17/CMake 3D sector engine. It intentionally preserves the useful authoring model of the reference top-down engine:

- one optional Lua file per map;
- automatic map `init()` and `shutdown()` hooks;
- a fresh Lua VM for every map;
- Lua functions that can read like synchronous sequences while waiting on asynchronous C++ gameplay work;
- background scripts controlled with `startScript()` and `stopScript()`;
- reusable Lua modules loaded with `require`;
- both blocking and explicitly asynchronous engine commands.

The new implementation should not copy the reference engine's hard-coded `ScriptWaitType` switch. Instead, all long-running C++ work is represented by generalized, map-scoped operation handles. A blocking binding starts an operation and yields its Lua task; an asynchronous binding returns the same kind of operation handle without yielding. `await(handle)` can join an asynchronous operation later.

This is an implementation specification, not merely an architectural sketch. Names can be adapted to the target engine's existing conventions, but the ownership, lifecycle, and behavioral contracts should remain unchanged.

## 2. Goals and non-goals

### Goals

- Keep scripts easy to author. A blocking operation should look like an ordinary Lua function call.
- Make VM and script lifetime exactly match map lifetime.
- Allow multiple background tasks while serializing engine-driven foreground hooks.
- Generalize waits without teaching the scheduler about doors, elevators, dialogue, timers, animation, or other gameplay-specific systems.
- Make map changes and teardown safe even when scripts are suspended.
- Provide deterministic start, resume, stop, completion, and cleanup behavior.
- Make stale handles and late asynchronous completions harmless.
- Keep persistent game state outside Lua so VM recreation and save/load are straightforward.
- Produce useful errors and task/operation diagnostics.

### Non-goals

- Saving or restoring Lua coroutine stacks.
- Preserving Lua globals or `package.loaded` across maps.
- Running Lua from worker threads.
- Supporting untrusted or adversarial mods. The proposed standard-library configuration is not a security sandbox.
- Adding a second textual `include()` facility. Lua's standard `require` is the include/module system.
- Hot reloading suspended coroutines. A developer reload should recreate the map VM just like a map reload.

## 3. Required dependency and build integration

Target the vendored PUC Lua 5.5 source distribution and use the Lua C API directly through `lua.hpp`. Compile Lua as C, not C++, and link it into the engine.

A representative CMake setup is:

```cmake
set(LUA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua")

file(GLOB LUA_SOURCES CONFIGURE_DEPENDS
    "${LUA_SOURCE_DIR}/*.c"
)

# Standalone interpreter/compiler entry points must not be linked into the game.
list(FILTER LUA_SOURCES EXCLUDE REGEX "/lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/luac\\.c$")

add_library(lua55 STATIC ${LUA_SOURCES})
target_include_directories(lua55 PUBLIC "${LUA_SOURCE_DIR}")

target_link_libraries(game PRIVATE lua55)
```

If the vendored tree contains only library sources, the exclusions are harmless. Prefer an explicit source list if the target project avoids source globs.

Include Lua from C++ as:

```cpp
#include "lua.hpp"
```

Do not depend on APIs from Lua 5.4 or earlier in the core implementation. Lua 5.5 uses:

```cpp
int resultCount = 0;
const int status = lua_resume(thread, nullptr, argumentCount, &resultCount);
```

The second argument is `nullptr` because the host C++ loop, rather than another Lua coroutine, is resuming the task.

## 4. Script asset layout and naming

For a map file:

```text
assets/maps/refinery/refinery.map
```

the engine derives the optional map script path:

```text
assets/maps/refinery/refinery.lua
```

The rule is: same directory and same basename as the map, with a `.lua` extension. The map registry may store an explicit script override later, but v1 should use the derived path so every map has one obvious script.

Reusable modules live below:

```text
assets/scripts/
```

Examples:

```text
assets/scripts/effects/flicker.lua
assets/scripts/encounters/reinforcements/init.lua
```

Map files define simple global functions. The two reserved lifecycle names are:

```lua
function init()
    -- Optional. May call blocking functions and yield.
end

function shutdown()
    -- Optional. Must return synchronously and must not yield.
end
```

Other global functions are callable by map triggers or `startScript()`:

```lua
function alarmSequence()
    -- ...
end
```

Lua identifiers are case-sensitive. Use lower camel case for engine bindings and script functions.

## 5. High-level ownership

The game owns one `ScriptRuntime` embedded in, or directly owned by, its map runtime. `ScriptRuntime` owns:

- the map's `lua_State`;
- all Lua coroutine registry references;
- all script task slots;
- all script-visible operation slots;
- pending task-start requests;
- pending operation completion records;
- map script metadata and lifecycle flags;
- maps used to resolve Lua threads and names to stable task handles.

The campaign/session owns a separate `PersistentScriptStore`. It is not cleared when the VM closes and is serialized by the save system.

The following ownership relationship is mandatory:

```text
Game/Campaign
├── PersistentScriptStore       survives map changes; saved
└── MapRuntime
    ├── sector/world data
    └── ScriptRuntime           recreated for every map
        ├── lua_State
        ├── ScriptTask slots
        └── ScriptOperation slots
```

Lua never owns C++ world objects. Lua passes IDs or stable engine handles to bindings. Do not store pointers to objects inside containers in Lua userdata or suspended tasks.

## 6. Core C++ data model

The exact engine types may differ, but implement equivalent plain data structures.

### 6.1 Generational handles

Tasks and operations use slot index plus generation. A reused slot increments its generation, so a late completion or stale Lua userdata cannot affect a new object occupying the same index.

```cpp
struct ScriptTaskHandle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};

struct ScriptOperationHandle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};

inline bool IsValid(ScriptTaskHandle h) {
    return h.index != UINT32_MAX;
}

inline bool IsValid(ScriptOperationHandle h) {
    return h.index != UINT32_MAX;
}
```

Every slot stores its current generation and an occupied flag. Resolve a handle by validating all three conditions: index in range, slot occupied, and generation equal. Never expose a bare vector index as a durable handle.

### 6.2 Task states and launch lanes

```cpp
enum class ScriptTaskState {
    Free,
    Queued,
    Running,
    Waiting,
    StopRequested,
    Completed,
    Failed,
    Cancelled
};

enum class ScriptLaunchLane {
    Foreground, // engine-driven init/event/trigger hook
    Background  // startScript task
};
```

Only one foreground task may be active or queued at a time. Any number of distinct background functions may run concurrently. The function name must be unique across all queued and nonterminal tasks, regardless of lane.

### 6.3 Operation states and values

```cpp
enum class ScriptOperationState {
    Free,
    Pending,
    Succeeded,
    Failed,
    Cancelled
};

enum class ScriptOperationLaunchStyle {
    Blocking,
    Async
};

using ScriptValue = std::variant<
    std::monostate, // Lua nil
    bool,
    int64_t,
    double,
    std::string
>;
```

Operation result values must be self-contained values safe to retain until a future frame. Represent engine entity references as stable integer handles or string IDs. For vector-like results, return separate numeric values or extend `ScriptValue` with an explicit plain value type. Do not retain `lua_State*`, Lua stack indices, or pointers into gameplay containers in an operation result.

### 6.4 Task record

```cpp
struct ScriptTask {
    bool occupied = false;
    uint32_t generation = 0;
    ScriptTaskState state = ScriptTaskState::Free;
    ScriptLaunchLane lane = ScriptLaunchLane::Background;

    std::string functionName;
    lua_State* thread = nullptr;
    int threadRegistryRef = LUA_NOREF;

    ScriptOperationHandle waitingOperation{};
    bool stopRequested = false;
    bool lifecycleInitTask = false;

    std::string lastError;
};
```

The task's Lua thread is kept alive by `threadRegistryRef` on the main VM. `thread` is valid only until VM teardown. Erase thread lookup entries and unreference the thread before closing the VM.

### 6.5 Operation record

```cpp
using ScriptOperationCancelFn = void (*)(
    EngineContext& engine,
    uint64_t backendToken);

struct ScriptOperation {
    bool occupied = false;
    uint32_t generation = 0;
    ScriptOperationState state = ScriptOperationState::Free;
    ScriptOperationLaunchStyle launchStyle =
        ScriptOperationLaunchStyle::Blocking;

    std::string debugLabel;
    uint64_t backendToken = 0;
    ScriptOperationCancelFn cancelBackend = nullptr;

    ScriptTaskHandle ownerTask{};
    ScriptTaskHandle waiterTask{};

    std::vector<ScriptValue> values;
    std::string error;

    uint32_t luaObserverCount = 0;
    bool backendCancelSent = false;
};
```

The scheduler does not switch on an operation kind. Gameplay subsystems own and update their work, identified by `backendToken`, and notify the scripting system when it ends. `cancelBackend` is an optional non-capturing function pointer used to route cancellation without virtual classes or heap-allocated callbacks.

An operation can have at most one waiting task. This keeps result delivery and cancellation deterministic. A second concurrent `await()` returns `false, "operation already has a waiter"` immediately.

### 6.6 Runtime and persistent store

```cpp
enum class ScriptRuntimePhase {
    Empty,
    Loading,
    Active,
    ShuttingDown
};

struct ScriptStartRequest {
    std::string functionName;
    ScriptLaunchLane lane = ScriptLaunchLane::Background;
};

struct ScriptCompletionRecord {
    ScriptOperationHandle operation{};
    ScriptOperationState terminalState = ScriptOperationState::Failed;
    std::vector<ScriptValue> values;
    std::string error;
};

struct ScriptRuntime {
    lua_State* vm = nullptr;
    EngineContext* engine = nullptr;
    ScriptRuntimePhase phase = ScriptRuntimePhase::Empty;

    std::string mapId;
    std::string mapScriptPath;
    bool mapChunkPresent = false;
    bool initAttempted = false;
    bool initFinished = false;
    bool shutdownAttempted = false;

    std::vector<ScriptTask> tasks;
    std::vector<uint32_t> freeTaskSlots;
    std::vector<ScriptOperation> operations;
    std::vector<uint32_t> freeOperationSlots;

    std::vector<ScriptStartRequest> pendingStarts;
    std::vector<ScriptCompletionRecord> pendingCompletions;

    std::unordered_map<lua_State*, ScriptTaskHandle> taskByThread;
    std::unordered_map<std::string, ScriptTaskHandle> taskByName;

    bool mapChangeRequested = false;
    std::string requestedMapId;
    std::string requestedSpawnId;
};

struct PersistentScriptStore {
    std::unordered_map<std::string, bool> bools;
    std::unordered_map<std::string, int64_t> ints;
    std::unordered_map<std::string, std::string> strings;
};
```

If worker threads can complete operations, they must write to a separate thread-safe completion inbox. The main thread drains that inbox into `pendingCompletions` at the start of `ScriptSystemUpdate`. A simple mutex-protected vector is sufficient initially. Worker threads must never touch `lua_State`, task slots, operation slots, or map data.

Reserve sensible task and operation capacity when creating the VM. Slot pools prevent routine erases and keep allocation behavior predictable. Script scheduling normally is not a dominant hot path, but it should not churn containers unnecessarily every frame.

## 7. Lua registry context

Do not use a process-global `GameState*`. Store a small context in the Lua registry:

```cpp
struct LuaEngineContext {
    ScriptRuntime* scripts = nullptr;
    EngineContext* engine = nullptr;
    PersistentScriptStore* persistent = nullptr;
};
```

Allocate this context as part of the map runtime, so its address stays stable until `lua_close`. Register it under an address-unique key:

```cpp
static char kLuaEngineContextKey;

void SetLuaEngineContext(lua_State* L, LuaEngineContext* context) {
    lua_pushlightuserdata(L, &kLuaEngineContextKey);
    lua_pushlightuserdata(L, context);
    lua_settable(L, LUA_REGISTRYINDEX);
}

LuaEngineContext& GetLuaEngineContext(lua_State* L) {
    lua_pushlightuserdata(L, &kLuaEngineContextKey);
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* context = static_cast<LuaEngineContext*>(
        lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (context == nullptr) {
        luaL_error(L, "Lua engine context is unavailable");
    }
    return *context;
}
```

All coroutines share the registry with the main state, so the lookup works from bindings invoked on any managed thread.

Find the current task through `taskByThread[L]`, then validate the resulting generational handle. A binding that wants to yield must fail with `luaL_error` if it is not executing inside a managed task.

## 8. VM creation and module configuration

Create the VM only after the map's C++ world data and required resources are ready. This ensures map bindings and `init()` can query valid objects.

Creation order:

1. Clear the transient `ScriptRuntime` while preserving the external `PersistentScriptStore`.
2. Set phase to `Loading` and record map ID/path.
3. Call `luaL_newstate`; fail map activation if it returns null.
4. Call `luaL_openlibs`.
5. Install `LuaEngineContext` in the registry.
6. Register engine bindings and operation-handle metatable.
7. Configure deterministic module paths.
8. Reserve task, operation, start, completion, and result capacities.
9. Execute the optional map chunk.
10. Start optional `init()` as the foreground lifecycle task.
11. Set phase to `Active` once immediate setup succeeds.

### 8.1 `require` path

Replace `package.path`; do not append the host machine's default search path. Search map-local modules first and shared modules second:

```text
<map-directory>/?.lua
<map-directory>/?/init.lua
<asset-root>/scripts/?.lua
<asset-root>/scripts/?/init.lua
```

Join these with semicolons and assign the result to `package.path`. Set `package.cpath` to an empty string so scripts do not accidentally load native modules from a developer machine.

Use normalized forward slashes in paths passed to Lua, including on Windows. Chunk names should start with `@` followed by the normalized file path so tracebacks contain useful source locations.

Lua's standard `require` behavior is the include system:

- a module runs once per VM;
- its return value is cached in `package.loaded`;
- the cache naturally disappears when the map VM closes;
- modules should return a table and avoid adding globals.

Example module:

```lua
-- assets/scripts/sector/doors.lua
local Doors = {}

function Doors.openPair(a, b, durationMs)
    local first = startMoveDoor(a, 1.0, durationMs)
    local second = startMoveDoor(b, 1.0, durationMs)

    local ok, reason = await(first)
    if not ok then return false, reason end

    return await(second)
end

return Doors
```

Usage:

```lua
local Doors = require("sector.doors")
```

Do not implement `include(path)`, `dofile`, or manual repeated execution as a parallel module mechanism.

### 8.2 Trust model

`luaL_openlibs` exposes APIs such as `io`, `os`, `debug`, and dynamic loading facilities. Clearing `package.cpath` improves determinism but does not create a sandbox. This design assumes trusted game-authored scripts. If untrusted mod support is added later, it requires a separate threat model, restricted library registration, filesystem policy, resource limits, and likely a distinct VM configuration.

## 9. Loading and executing the map chunk

A missing map script is valid. Log it at debug/info level, leave `mapChunkPresent` false, and continue with an empty VM.

If the file exists:

1. Read it through the engine's asset/filesystem abstraction.
2. Compile with `luaL_loadbufferx(vm, bytes, size, chunkName, "t")`.
3. Execute with `lua_pcall(vm, 0, 0, 0)`.
4. Clear the main stack after success or after extracting an error.

Compilation or top-level execution failure is fatal to map activation. Produce a traceback containing map ID, path, phase, and Lua error. Tear down the partially created VM and map resources through the normal failure path.

Top-level code must not call a blocking binding. It runs on the main Lua state via `lua_pcall`, not inside a managed coroutine. Such a binding must raise a clear error such as:

```text
moveDoor() can only block inside init(), an event hook, or startScript() task
```

Immediate engine calls and `require` are allowed at top level, although declarations and module imports should normally be all that occurs there.

## 10. Lifecycle contract

### 10.1 Map activation and `init()`

After the map chunk succeeds, look up global `init`:

- If it is absent or nil, mark initialization finished and continue.
- If it exists but is not a function, treat this as a fatal script error.
- If it is a function, start it immediately as a managed foreground task named `init` with `lifecycleInitTask = true`.

`init()` may call blocking engine commands and yield across frames. Normal map simulation and input continue while it runs. Background tasks started by `init()` are queued and begin on the next script update. Engine-driven trigger hooks invoked while `init()` still owns the foreground lane receive `Busy`.

If `init()` returns immediately, mark it finished during map activation. If it yields, the map enters normal play with initialization in progress. If the suspended init task later errors, log the traceback and request a controlled transition to the engine's safe fallback (for example, unload to menu/error screen). Do not continue running a map whose initialization failed.

The distinction between immediate activation and yielded initialization should be visible in diagnostics.

### 10.2 Map shutdown

`shutdown()` is optional and synchronous. It is for immediate cleanup or committing persistent state, not for playing animations or waiting for gameplay.

Shutdown ordering is exact:

1. Stop accepting new hooks and set phase to `ShuttingDown`.
2. Freeze script/gameplay operation updates for this map.
3. If `shutdownAttempted` is false, set it true before calling Lua.
4. Call global `shutdown` once with `lua_pcall` while map C++ data and resources are still valid.
5. Log any shutdown error, but continue teardown.
6. Mark all tasks cancelled and detach all waiters.
7. Cancel every pending map-scoped operation and invoke each backend cancellation at most once.
8. Discard queued starts and queued/late completion records.
9. Unreference every Lua thread and operation userdata reference owned from C++.
10. Clear thread/name lookup maps.
11. Call `lua_close` and null the VM/context pointers.
12. Clear all transient script slots and set phase to `Empty`.
13. Only then unload map objects, sectors, resources, audio emitters, and other data that shutdown bindings might inspect.

Set `shutdownAttempted` before entering Lua so reentrant or repeated teardown cannot call it twice. Missing `shutdown` is valid. A non-function `shutdown` is logged as an error and cleanup proceeds.

Every yield-capable binding must require phase `Loading` or `Active` and a managed task owner. `Loading` is allowed specifically so the managed `init` task can yield. During shutdown it raises:

```text
blocking Lua operations are not allowed during shutdown()
```

Calling `lua_yieldk` from the non-yieldable shutdown `lua_pcall` would fail anyway, but the explicit phase check provides a useful diagnostic and avoids partially starting gameplay work.

### 10.3 All exit paths use the same teardown

The same shutdown function must be used for:

- normal map-to-map travel;
- returning to a menu;
- application exit while a map is active;
- loading a save that replaces the current map;
- developer map reload;
- rollback after a post-VM map activation failure.

It is safe to call teardown repeatedly; only the first call while a VM is present can run `shutdown()`.

### 10.4 Deferred map changes

A Lua binding must never unload the VM that is currently executing it. `changeMap(mapId, spawnId?)` validates and records a request, then returns immediately:

```cpp
runtime.mapChangeRequested = true;
runtime.requestedMapId = mapId;
runtime.requestedSpawnId = spawnId;
```

After the entire script update and current game update reach a safe orchestration point, consume the request:

1. move the strings into local C++ variables;
2. clear the request;
3. run old-map shutdown and close the old VM;
4. unload the old map;
5. load the requested map data/resources;
6. create the new VM and run its chunk/init.

If multiple requests arrive in the same frame, accept the first and return `false, "map change already requested"` for later requests. Never let a later background task silently override the destination.

## 11. Starting and running Lua tasks

### 11.1 Starting a managed function

The low-level start function accepts a global Lua function name, lane, and internal lifecycle flag and returns a structured result:

```cpp
enum class ScriptCallResult {
    Missing,
    Completed,
    Started,
    AlreadyRunning,
    ForegroundBusy,
    Error
};

struct ScriptCallOutcome {
    ScriptCallResult result = ScriptCallResult::Error;
    ScriptTaskHandle task{};
    std::vector<ScriptValue> immediateValues;
    std::string error;
};
```

Start algorithm:

1. Require a valid VM and `Loading` or `Active` phase.
2. Reject the name if it is already queued or present in `taskByName`.
3. For a foreground launch, reject if any foreground task is queued or nonterminal.
4. Fetch the global and return `Missing` if nil/not found; return `Error` if non-nil but not a function.
5. Allocate a task slot and construct its generational handle.
6. Create a Lua thread with `lua_newthread(vm)` and immediately anchor it using `luaL_ref(vm, LUA_REGISTRYINDEX)`.
7. Fetch the global function on the main state and move it to the thread with `lua_xmove`.
8. Fully register the task in `taskByThread` and `taskByName` before first resume. A binding invoked during the first resume must be able to find its owner.
9. Resume with zero arguments.
10. Handle the resume status without retaining a reference or pointer into a container across code that can allocate another slot.

Using stable slot handles is important: Lua code executed during `lua_resume` may call `startScript`, complete operations, or otherwise grow scheduler containers. Always resolve the task handle again after returning from Lua.

### 11.2 Resume results

For Lua 5.5:

```cpp
int resultCount = 0;
const int status = lua_resume(thread, nullptr, argumentCount, &resultCount);
```

Handle status as follows:

- `LUA_YIELD`: the binding must already have attached a pending operation to the task. Set task state to `Waiting`. A bare `coroutine.yield()` without a registered operation is unsupported; report an error and cancel the task.
- `LUA_OK`: collect any final return values needed by the C++ caller, mark `Completed`, release the thread reference, and remove name/thread lookup entries.
- Any other status: read the error, build a traceback with `luaL_traceback`, mark `Failed`, release the thread, and notify the lifecycle/error policy.

Engine event hooks should not use asynchronous final return values. If C++ requires a value immediately, register that callback as a synchronous query hook and reject yielding. This avoids pretending that a bool is available while its coroutine is suspended.

### 11.3 Foreground hooks

Engine-driven triggers and events use the foreground lane. Examples are `onEnterSector`, `onUseTerminal`, or authored trigger function names.

- A missing function returns `Missing` and is normally not an error.
- An available foreground lane starts the hook immediately, allowing C++ to distinguish immediate completion from a yield.
- If another foreground hook, including `init`, is active, return `ForegroundBusy` and log/handle it explicitly at the caller.
- Hooks are not automatically queued. The gameplay system decides whether a busy event should be ignored, retried, or retained in its own event queue.
- Background tasks continue while a foreground hook waits.

This matches the simple sequencing behavior of the reference engine without preventing ambient loops.

### 11.4 Background `startScript`

Lua API:

```lua
local ok, reason = startScript("alarmLoop")
```

`startScript` validates that the named global exists and is a function, and that the name is not queued/running. On success it queues a background `ScriptStartRequest`; it does not recursively resume another Lua task inside the current binding. The new task begins during the next `ScriptSystemUpdate`.

This is a core scheduler binding registered for every map VM by
`ScriptSystem.cpp`, before any game/sector-specific binding registration. It is
therefore intentionally not implemented in `SectorScriptBindings.cpp`.

Return values:

```text
true
false, "function not found: alarmLoop"
false, "script already queued or running: alarmLoop"
false, "script runtime is shutting down"
```

The duplicate check covers queued, running, and waiting tasks in both lanes. Terminal tasks are reclaimed before their names become startable again.

### 11.5 Stopping tasks

Lua API:

```lua
stopScript("alarmLoop")
stopAllScripts()
isScriptRunning("alarmLoop")
```

`stopScript(name)` marks the matching queued/active task for cancellation. It does not invoke Lua code and does not forcibly unwind the C stack. Cancellation is applied at the next safe scheduler cleanup point.

If the target is waiting on an operation:

- when the operation was created by that task through a blocking binding, cancel the backend operation and mark it cancelled;
- when the task used `await()` on an explicitly async operation, detach the waiter but allow the operation to continue;
- clear the task's waiting handle in either case.

If a task stops itself, the stop call returns, and Lua may run until the current call yields or the function returns. The scheduler then cancels it. This is cooperative cancellation; do not attempt unsafe preemption.

`stopAllScripts()` marks all foreground and background tasks, including its caller, but does not run `shutdown()`. Map teardown uses the stronger shutdown path. `isScriptRunning` returns true for queued, running, waiting, and stop-requested-but-not-yet-reclaimed tasks.

Like `startScript`, `stopScript`, `stopAllScripts`, and `isScriptRunning` are
global core bindings registered by `ScriptSystem.cpp` for every map VM.

Suggested return contract:

```text
stopScript: true, or false plus "script not running: <name>"
stopAllScripts: true
isScriptRunning: one boolean
```

## 12. Generalized operation and wait mechanism

### 12.1 Core idea

Every long-running C++ action that Lua can observe gets a `ScriptOperationHandle`. The script scheduler understands only these states:

```text
Pending -> Succeeded
Pending -> Failed
Pending -> Cancelled
```

Gameplay code determines when an operation completes. For example:

- the door subsystem completes when a motor reaches its target;
- the mover subsystem completes when an entity reaches a destination;
- the dialogue subsystem completes with a selected option;
- the timer subsystem completes after its deadline;
- the animation subsystem completes when a non-looping clip ends.

Adding a new waitable command requires a gameplay start function, an operation completion notification, and one or two Lua bindings. It does not require modifying the scheduler update switch.

### 12.2 Creating an operation

Use a common function:

```cpp
ScriptOperationHandle CreateScriptOperation(
    ScriptRuntime& runtime,
    ScriptOperationLaunchStyle launchStyle,
    ScriptTaskHandle ownerTask,
    std::string debugLabel,
    uint64_t backendToken,
    ScriptOperationCancelFn cancelFn);
```

The operation starts as `Pending`. For a blocking operation, set both `ownerTask` and `waiterTask` to the current task immediately. For an async operation, `ownerTask` may identify the creator for diagnostics, but leave `waiterTask` invalid until `await()`.

Gameplay startup may fail before an operation exists. Return `false, reason` immediately in that case. Gameplay startup may also finish synchronously. Create a terminal operation and either return its result immediately for a blocking call or return its handle for an async call.

### 12.3 Completing an operation

Expose main-thread functions:

```cpp
bool CompleteOperation(
    ScriptRuntime& runtime,
    ScriptOperationHandle handle,
    std::vector<ScriptValue> values = {});

bool FailOperation(
    ScriptRuntime& runtime,
    ScriptOperationHandle handle,
    std::string reason);

bool CancelOperation(
    EngineContext& engine,
    ScriptRuntime& runtime,
    ScriptOperationHandle handle,
    std::string reason = "cancelled");
```

All are idempotent with respect to terminal operations: the first valid transition wins; later completion attempts return false and may log at debug level. A stale generation, closed runtime, or out-of-range handle is ignored safely.

Completion stores self-contained result data and marks the waiter ready. Do not call `lua_resume` directly from a gameplay subsystem or completion function. Actual resume occurs in the scheduler's deterministic resume phase.

If completion originates on a worker thread, enqueue a `ScriptCompletionRecord`. Its generational handle makes a completion arriving after map teardown harmless.

### 12.4 Lua operation userdata

Async bindings return opaque full userdata:

```cpp
struct LuaScriptOperationHandle {
    ScriptOperationHandle handle{};
};
```

Give it a locked metatable, for example `Engine.ScriptOperation`, with:

- `__gc`: decrement `luaObserverCount` if the operation is still valid;
- `__tostring`: return a debug string such as `ScriptOperation(door:blast_a, pending)`;
- `__metatable`: a protected string so scripts cannot replace the metatable.

Garbage collection does not cancel gameplay. It only releases the Lua observation reference. Reclaim a terminal operation slot when it has no waiter and `luaObserverCount == 0`. Blocking operations normally have zero observers and can be reclaimed after their result is delivered. All slots are forcibly reclaimed on map teardown.

### 12.5 Result contract

Every blocking call and `await()` returns:

```text
true, [success values...]
false, reason
```

Examples:

```lua
local ok, reachedPosition = moveEntityTo("guard_1", x, y, z)
if not ok then
    log("move failed: " .. reachedPosition)
end

local ok, choice = await(dialogueOperation)
if not ok then
    log("dialogue cancelled: " .. choice)
end
```

Bad argument types, invalid binding invariants, and attempts to yield outside a managed task use `luaL_argerror`/`luaL_error`. Expected gameplay outcomes such as blocked movement, missing optional target, interruption, timeout, or explicit cancellation return `false, reason` rather than aborting the Lua task.

### 12.6 Continuation helper

A binding that yields must use `lua_yieldk`. Before yielding, record the stack depth occupied by the binding's original arguments in `lua_KContext`. When resumed, only values pushed above that base are returned to Lua.

```cpp
static int FinishOperationWait(
    lua_State* L,
    int status,
    lua_KContext context)
{
    (void)status;
    const int originalTop = static_cast<int>(context);
    return lua_gettop(L) - originalTop;
}
```

The binding pattern is:

```cpp
const int originalTop = lua_gettop(L);

// Parse arguments, start backend work, create operation, attach task.
// No result values are passed outward at the moment of yielding.
return lua_yieldk(
    L,
    0,
    static_cast<lua_KContext>(originalTop),
    FinishOperationWait);
```

Before resuming the task, push `true` plus success values, or `false` plus the error, onto the suspended thread and pass that count to `lua_resume`. The continuation returns only those newly supplied values rather than accidentally returning the original binding arguments.

Keep this logic in one shared helper and test it against Lua 5.5. Do not write a different continuation for every binding unless a binding genuinely needs custom result conversion.

### 12.7 Awaiting an async operation

`await(operation)` behaves as follows:

1. Validate userdata and its generational handle.
2. Resolve the current managed task; error if called at top level or during shutdown.
3. If the operation succeeded, immediately return `true, values...`.
4. If it failed, immediately return `false, reason`.
5. If it was cancelled, immediately return `false, reason-or-"cancelled"`.
6. If pending with another waiter, return `false, "operation already has a waiter"`.
7. Otherwise attach the current task as waiter, set the task to waiting, and yield with the shared continuation.

Awaiting does not change an async operation into a blocking-owned operation. Therefore stopping the waiter detaches it but does not cancel the async backend action.

`operationStatus(handle)` returns one of `"pending"`, `"succeeded"`, `"failed"`, `"cancelled"`, or `"stale"`, optionally followed by the terminal reason. It never yields.

`cancelOperation(handle)` sends backend cancellation once and moves the operation to `Cancelled`. If a task is awaiting it, that task resumes on the scheduler's next resume phase with `false, "cancelled"`.

## 13. Blocking and async binding example

The representative sector-engine action below moves a door to a normalized open fraction.

Lua surface:

```lua
moveDoor(doorId, openFraction, durationMs)
    -> true
    -> false, reason

startMoveDoor(doorId, openFraction, durationMs)
    -> operation
    -> nil, reason
```

Both bindings call one C++ starter:

```cpp
struct BeginDoorMoveResult {
    bool started = false;
    bool completedImmediately = false;
    uint64_t backendToken = 0;
    std::string error;
};

BeginDoorMoveResult BeginDoorMove(
    EngineContext& engine,
    DoorHandle door,
    float targetFraction,
    float durationMs);
```

Cancellation routes back to the door subsystem:

```cpp
static void CancelDoorMove(EngineContext& engine, uint64_t token) {
    DoorSystemCancelMove(engine.doors, DoorMoveHandle::FromBits(token));
}
```

Blocking binding outline:

```cpp
static int LuaMoveDoor(lua_State* L) {
    auto& context = GetLuaEngineContext(L);
    const char* doorId = luaL_checkstring(L, 1);
    const float fraction = static_cast<float>(luaL_checknumber(L, 2));
    const float durationMs = static_cast<float>(luaL_checknumber(L, 3));

    RequireYieldableManagedTask(L, *context.scripts);
    if (fraction < 0.0f || fraction > 1.0f || durationMs < 0.0f) {
        return luaL_error(L, "invalid door target or duration");
    }

    DoorHandle door = FindDoorById(*context.engine, doorId);
    if (!door.IsValid()) {
        lua_pushboolean(L, 0);
        lua_pushfstring(L, "door not found: %s", doorId);
        return 2;
    }

    BeginDoorMoveResult begin = BeginDoorMove(
        *context.engine, door, fraction, durationMs);
    if (!begin.started) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, begin.error.data(), begin.error.size());
        return 2;
    }

    if (begin.completedImmediately) {
        lua_pushboolean(L, 1);
        return 1;
    }

    const int originalTop = lua_gettop(L);
    ScriptTaskHandle task = GetCurrentTaskHandle(L, *context.scripts);
    ScriptOperationHandle operation = CreateScriptOperation(
        *context.scripts,
        ScriptOperationLaunchStyle::Blocking,
        task,
        std::string("moveDoor:") + doorId,
        begin.backendToken,
        CancelDoorMove);
    AttachBlockingWait(*context.scripts, task, operation);

    return lua_yieldk(L, 0, originalTop, FinishOperationWait);
}
```

Async binding outline:

```cpp
static int LuaStartMoveDoor(lua_State* L) {
    auto& context = GetLuaEngineContext(L);
    // Parse/validate and call BeginDoorMove exactly as above.

    ScriptTaskHandle creator = TryGetCurrentTaskHandle(
        L, *context.scripts); // invalid is allowed for async calls

    ScriptOperationHandle operation = CreateScriptOperation(
        *context.scripts,
        ScriptOperationLaunchStyle::Async,
        creator,
        std::string("moveDoor:") + doorId,
        begin.backendToken,
        CancelDoorMove);

    if (begin.completedImmediately) {
        CompleteOperation(*context.scripts, operation);
    }

    PushOperationUserdata(L, *context.scripts, operation);
    return 1;
}
```

The real implementation should factor shared parsing/start logic rather than duplicate it. The important property is that both calls create the same operation representation and use the same backend completion path.

When the door system finishes, it calls:

```cpp
CompleteOperation(scriptRuntime, scriptOperationHandle);
```

The door move record should retain the script operation handle or the engine should maintain a mapping from backend move token to script operation handle. That retained handle is a value, not a pointer. A completion after teardown simply fails generation/runtime validation.

### 13.1 NPC navigation operations

NPC movement uses the same operation and result contracts:

```lua
moveNpc(instanceId, x, z [, gait])
moveNpc(instanceId, levelMarkerId [, gait])
    -> true
    -> false, reason

startMoveNpc(instanceId, x, z [, gait])
startMoveNpc(instanceId, levelMarkerId [, gait])
    -> operation
    -> nil, reason
```

`instanceId` is the placed NPC instance ID. `x` and `z` are runtime world
coordinates; navigation projects the destination onto the appropriate floor.
The marker overload resolves an exact, case-sensitive compiled level-marker ID,
converts its authored position to runtime world units, and uses its X/Z as the
destination. Marker height and yaw are intentionally ignored; navigation keeps
the same floor projection and movement-facing behavior as the coordinate form.
The marker is resolved once when the request starts. `gait` defaults to
`"walk"` and also accepts `"run"`.

The blocking form resumes only after authoritative collision-constrained
locomotion reports physical arrival. Finding a Detour route does not complete
the operation. Async operations use `await`, `operationStatus`, and
`cancelOperation` exactly like door operations. Only one script-owned move may
be active for an NPC; overlapping script requests fail without replacing the
first operation. Script authority blocks future AI retargeting until the move
arrives, fails, or is cancelled.

Requests fail clearly for invalid IDs or gaits, non-finite/off-mesh targets,
unavailable or rebuilding navigation, partial/no paths, capacity exhaustion,
stalls, NPC removal, and map teardown. The navigation mesh must already be
ready when a request begins. Door traversal links, dynamic TileCache obstacles,
and Crowd avoidance are intentionally deferred to later navigation slices.

During gameplay, F8 toggles the read-only Nav diagnostics and cached world
path/agent overlay. This is useful with `startMoveNpc` commands issued from the
F1 debug console.

## 14. Built-in delay operation

Implement `delay(ms)` through the same operation mechanism, not as a special task wait enum.

Maintain a map-owned timer backend with timer records containing deadline and associated operation handle. A min-heap is useful if there are many timers; a vector scan is acceptable for a small number. Timers use the engine's monotonic game/script clock, not `os.clock`.

Contract:

```lua
delay(milliseconds) -> true
```

- Negative or non-finite durations are argument errors.
- `delay(0)` always creates a timer due on the next script update.
- It never resumes in the same scheduler pass in which it was created.
- Timer progression follows the engine's chosen pause policy. For v1, use scaled map simulation time so delays pause when map simulation is paused. If a real-time delay is needed later, expose a distinctly named command.

The scheduler also publishes read-only `FrameDelta` in seconds before resuming tasks, matching the reference engine's convenience global. Prefer a binding such as `getFrameDelta()` in later API cleanup, but the global is acceptable for compatible authoring.

## 15. Deterministic scheduler update

Run `ScriptSystemUpdate(runtime, dt)` once per active map frame on the main thread. In the outer game-loop orchestrator, advance gameplay subsystems that own operations before calling `ScriptSystemUpdate`, so completions produced this frame can wake scripts predictably. The script system itself advances only script-owned backends such as `delay` timers.

Exact ordering:

1. Return if VM is null or phase is not `Active`.
2. Publish `FrameDelta` in seconds.
3. Drain worker completion inbox into the main-thread completion list.
4. Advance the script-owned timer backend; gameplay completion records have already been reported by the earlier gameplay-system update.
5. Apply a snapshot of pending completion records to operation slots.
6. Move `pendingStarts` into a local scratch vector and start that snapshot. Starts created while executing these functions remain queued for the next frame.
7. Build a scratch list of task handles whose operations are terminal.
8. Resume each listed task at most once, in stable task-slot order.
9. Apply stop requests and task terminal cleanup.
10. Reclaim eligible terminal operations.
11. Return to the game orchestrator, which may then consume a deferred map-change request.

Do not iterate a container while Lua can invalidate its references. Snapshot handles, then resolve each handle immediately before acting. New starts/completions generated during resume are handled on the next update unless explicitly already part of the initial snapshots.

### 15.1 Resuming a completed wait

For each ready waiter:

1. Resolve task and operation handles again.
2. Skip stopped/stale tasks.
3. Detach waiter links before calling Lua.
4. Push `true` and each `ScriptValue` for success, or `false` and reason for failure/cancellation, on the task thread.
5. Clear `task.waitingOperation` and set task to `Running`.
6. Call `lua_resume(thread, nullptr, pushedValueCount, &resultCount)`.
7. Process yield/completion/error.
8. Reclaim the blocking operation after delivery if it has no observers.

Detaching before resume matters because resumed Lua can immediately start another wait or request its own cancellation.

### 15.2 Fairness and runaway scripts

Each task may be started or resumed at most once per update. This naturally prevents a chain of immediately terminal awaits from monopolizing a single scheduler pass if all starts/completions are snapshot-based.

Ordinary Lua code that never calls a yielding binding can still loop forever. For v1, treat scripts as trusted and document this limitation. In Debug builds, it is reasonable to add an optional instruction-count hook that aborts a task with a clear "instruction budget exceeded" error, but do not make a profiler/debug hook part of release semantics without measuring its cost.

## 16. Error handling and tracebacks

Centralize error extraction:

```cpp
std::string BuildLuaTraceback(lua_State* thread, const char* prefix) {
    const char* message = lua_tostring(thread, -1);
    luaL_traceback(
        thread,
        thread,
        message != nullptr ? message : "<non-string Lua error>",
        1);
    const char* traceback = lua_tostring(thread, -1);
    // Copy the string before popping/closing anything.
    return std::string(prefix) + ": " +
        (traceback != nullptr ? traceback : "<traceback unavailable>");
}
```

Every error log should identify:

- map ID and script path;
- lifecycle phase;
- function/task name;
- foreground/background lane;
- current operation label, if waiting;
- Lua traceback.

Policy:

- Missing optional map file or hook: not an error.
- Present global hook with wrong type: error.
- Map chunk compile/top-level error: fail map activation.
- Immediate `init` error: fail map activation and clean up.
- Yielded `init` error: request controlled map abort/fallback.
- Ordinary hook/background task error: terminate only that task and log.
- `shutdown` error: log and continue teardown.
- Expected gameplay failure: return `false, reason`; do not create a traceback.
- Internal stale completion: ignore safely, optionally debug-log.

Never leave error objects or ordinary results accumulating on the main Lua stack. Establish and assert stack balance around all C++-initiated Lua calls in Debug builds.

## 17. Persistent script state and save/load

Lua VM state is transient. Persistent narrative/gameplay values live in `PersistentScriptStore`, outside `ScriptRuntime`.

Expose simple global bindings:

```lua
setPersistentBool(key, value)
getPersistentBool(key, defaultValue?)

setPersistentInt(key, value)
getPersistentInt(key, defaultValue?)

setPersistentString(key, value)
getPersistentString(key, defaultValue?)
```

Recommended defaults when no explicit default is passed:

```text
bool   -> false
int    -> 0
string -> ""
```

Keys must be non-empty strings. Optionally prefix keys by campaign/mod namespace at a higher layer; do not silently prefix them by map, because that would prevent deliberate cross-map state.

Save files serialize the three maps as JSON objects or the target engine's equivalent. Save/load order is:

### Saving

1. Snapshot engine world state and `PersistentScriptStore`.
2. Do not serialize tasks, Lua globals, module cache, timers, or operations.

### Loading

1. Tear down the current map/VM through normal shutdown.
2. Deserialize `PersistentScriptStore` before loading the saved map.
3. Load the map's authored data and construct its baseline C++ runtime without creating the Lua VM yet.
4. Apply the saved map-runtime snapshot to those C++ objects.
5. Create the fresh VM, execute the map script, and run `init()` with both persistent and restored world state already visible.
6. Keep an engine-owned `isLoadingSave()` flag true through the initial `init()` call/resume so the script can avoid replaying one-shot introductions; clear it when init finishes successfully.

This ordering is part of the contract: saved world state is restored before `init()` observes the map. The restore code must not depend on Lua-created transient objects. If a future feature needs script-authored persistent entities, promote their definitions/state into engine-owned serializable data rather than trying to reconstruct them from an old coroutine. Do not attempt to resume the old coroutine.

## 18. Suggested public C++ interface

Keep gameplay orchestration dependent on a small public header. Internal slot and Lua details stay private.

```cpp
bool ScriptSystemCreateForMap(
    EngineContext& engine,
    ScriptRuntime& runtime,
    PersistentScriptStore& persistent,
    const std::string& mapId,
    const std::string& mapFilePath,
    const std::string& assetRoot);

void ScriptSystemUpdate(
    EngineContext& engine,
    ScriptRuntime& runtime,
    float dtSeconds);

void ScriptSystemShutdownForMap(
    EngineContext& engine,
    ScriptRuntime& runtime);

ScriptCallOutcome ScriptSystemCallForegroundHook(
    ScriptRuntime& runtime,
    const std::string& functionName);

bool ScriptSystemQueueBackground(
    ScriptRuntime& runtime,
    const std::string& functionName,
    std::string& outError);

bool ScriptSystemStopFunction(
    EngineContext& engine,
    ScriptRuntime& runtime,
    const std::string& functionName,
    std::string& outError);

void ScriptSystemStopAll(
    EngineContext& engine,
    ScriptRuntime& runtime);

bool ScriptSystemIsFunctionRunning(
    const ScriptRuntime& runtime,
    const std::string& functionName);

ScriptOperationHandle ScriptSystemCreateOperation(...);
bool ScriptSystemCompleteOperation(...);
bool ScriptSystemFailOperation(...);
bool ScriptSystemCancelOperation(...);
```

Register gameplay-specific Lua bindings in focused subsystem files where practical, while keeping argument/result helpers and scheduler bindings in the scripting subsystem. A useful split is:

```text
scripting/ScriptData.h
scripting/ScriptSystem.h/.cpp
scripting/ScriptOperations.h/.cpp
scripting/ScriptLuaApi.h/.cpp
scripting/ScriptPersistence.h/.cpp
gameplay/*ScriptBindings.cpp
```

This is a responsibility suggestion, not a required path inventory. Follow the target engine's existing organization.

## 19. Suggested Lua API baseline

Core scheduler/lifecycle API:

```text
delay(ms)
startScript(functionName)
stopScript(functionName)
stopAllScripts()
isScriptRunning(functionName)
await(operation)
operationStatus(operation)
cancelOperation(operation)
changeMap(mapId, spawnId?)
log(...)
```

Persistent state API:

```text
setPersistentBool / getPersistentBool
setPersistentInt / getPersistentInt
setPersistentString / getPersistentString
```

Gameplay functions should use paired naming when both forms are useful:

```text
moveDoor(...)        blocking
startMoveDoor(...)   async, returns operation

moveNpc(...)          blocking
startMoveNpc(...)     async, returns operation

playSequence(...)        blocking
startSequence(...)       async, returns operation
```

Do not create async variants for operations whose completion is irrelevant, such as setting a flag or toggling a light immediately. Immediate commands should simply return success/failure.

## 20. Complete map-script example

```lua
local Flicker = require("effects.flicker")

function init()
    log("initializing refinery")

    startScript("warningLightLoop")

    if not getPersistentBool("refinery.intro_seen") then
        setPlayerControlsEnabled(false)

        local ok, reason = moveDoor("entry_airlock", 1.0, 850)
        if not ok then
            log("entry airlock failed: " .. reason)
        end

        delay(300)
        showMessage("Pressure equalized")
        setPlayerControlsEnabled(true)
        setPersistentBool("refinery.intro_seen", true)
    end
end

function shutdown()
    -- Synchronous only. Background scripts and operations are cancelled
    -- automatically after this returns.
    setPersistentBool("refinery.visited", true)
    stopLoopingSound("refinery_alarm")
end

function warningLightLoop()
    while true do
        Flicker.step("warning_light")
        delay(math.random(60, 160))
    end
end

function onUseFreightLift()
    local lift = startMoveLift("freight_lift", "upper_deck", 2500)
    if lift == nil then
        return false
    end

    -- Other immediate script work can happen while the lift is moving.
    playSound("lift_motor")
    setObjective("reach_upper_deck")

    local ok, reason = await(lift)
    if not ok then
        log("lift failed: " .. reason)
        return false
    end

    return true
end

function onExitPortal()
    -- The actual transition happens only after this script update returns.
    return changeMap("maintenance_tunnels", "from_refinery")
end
```

## 21. Debugging and observability

Provide a read-only task/operation snapshot for a debug UI or console. Do not expose mutable internal pointers.

Task entries should include:

- generational handle;
- function name;
- foreground/background lane;
- state;
- current operation label and handle;
- whether stop is requested;
- last error.

Operation entries should include:

- generational handle;
- debug label;
- pending/terminal state;
- blocking/async launch style;
- owner and waiter task names/handles;
- observer count;
- failure/cancellation reason.

Useful log events are VM create/close, map chunk load, init/shutdown start/end, task start/stop/error, operation cancellation, stale completion, duplicate start, and foreground busy. Avoid logging every ordinary timer completion in release builds.

A Lua console, if added, should execute on the main VM only for immediate expressions/statements. It must clearly reject yield-capable commands because console chunks are not managed tasks. Starting a named background function from the console can go through the normal queue API.

## 22. Edge cases and required policies

- **No map script:** create the per-map VM anyway so bindings/console behavior stay consistent; skip hooks.
- **Script file appears but is empty:** valid.
- **`init` or `shutdown` is non-function:** configuration error.
- **Blocking call at top level/console/shutdown:** Lua error before starting backend work.
- **Bare `coroutine.yield`:** unsupported; fail the managed task because no operation can wake it.
- **Nested Lua-created coroutines:** not scheduler-managed. Modules may use them internally only if they resume them themselves and never invoke engine blocking bindings from them. Document this restriction to script authors.
- **Duplicate names:** rejected while queued or active, even if lanes differ.
- **Function redefinition after task start:** the running coroutine continues its existing function; future starts use the current global after the prior task terminates.
- **Operation completes before `await`:** terminal result is retained and returned immediately.
- **Operation handle collected while pending:** backend work continues; its terminal slot can be reclaimed once no waiter/observer remains.
- **Second waiter:** receives an immediate explicit failure.
- **Stop of blocking waiter:** cancels its blocking-owned operation.
- **Stop of async waiter:** detaches only; async operation continues.
- **Map shutdown:** cancels all pending operations regardless of launch style.
- **Backend cancellation callback completes synchronously:** first terminal transition wins; cancellation/completion functions remain idempotent.
- **Late completion after map reload:** generation/runtime validation rejects it.
- **Operation result contains invalid world handle:** binding/gameplay layer converts it to an expected failure before completion or on consumption; never dereference blindly.
- **Map change requested by a task that later errors:** the already accepted request remains valid unless the engine explicitly clears it as part of fatal-task policy. For ordinary task errors, keep it; for failed `init`, override with safe fallback teardown.
- **Shutdown calls `stopAllScripts`:** allowed but redundant; actual cancellation still occurs after shutdown returns.
- **Shutdown calls `startScript`, `await`, `delay`, or a blocking gameplay command:** error; shutdown cleanup continues.
- **Application exits without an active map:** no-op shutdown.

## 23. Verification plan

Add focused C++ tests around the scheduler using fake backend operations, plus one small integration map. If the target repository has no test framework, build a debug-only harness executable or deterministic in-engine test map.

### 23.1 VM and lifecycle

1. A map with no Lua file activates successfully.
2. An empty Lua file activates successfully.
3. Syntax and top-level runtime errors fail activation and close the VM.
4. Missing `init`/`shutdown` is valid.
5. Non-function lifecycle globals report errors.
6. Immediate `init` runs once.
7. Yielded `init` resumes and finishes while map simulation continues.
8. Immediate and delayed `init` errors trigger the correct rollback/fallback.
9. `shutdown` runs exactly once with map objects still queryable.
10. A shutdown error does not prevent task, operation, VM, and map cleanup.
11. A blocking call from shutdown reports a useful error and starts no backend work.
12. Map reload produces fresh globals, module cache, tasks, and operations.

### 23.2 Tasks

1. Immediate task completion releases its registry reference and name.
2. A yielded task resumes with the expected values.
3. Missing function, duplicate queued name, duplicate running name, and foreground busy have distinct results.
4. `startScript` called from Lua begins the new task next frame, not recursively.
5. Two different background tasks run concurrently.
6. Only one foreground hook runs while background tasks continue.
7. Stopping queued, running, waiting, and self tasks follows the documented policy.
8. `stopAllScripts` safely includes its caller.
9. A Lua runtime error terminates only the ordinary task and includes a traceback.
10. A bare `coroutine.yield()` is detected rather than hanging forever.

### 23.3 Operations

1. Blocking success returns `true` and all result values.
2. Expected failure and cancellation return `false, reason` without a Lua traceback.
3. A synchronously completed backend action does not yield unnecessarily.
4. Async start returns valid userdata.
5. Await before completion yields and later resumes.
6. Await after completion returns immediately.
7. A second waiter is rejected.
8. Explicit cancellation is sent to the backend once and wakes a waiter once.
9. Stopping a blocking owner cancels the operation.
10. Stopping an async waiter leaves its operation running.
11. Dropping async userdata does not cancel gameplay.
12. Terminal operations reclaim only after their waiter/observer references are gone.
13. Stale handles, duplicate completions, and late completions after teardown are harmless.
14. Result delivery does not accidentally return the binding's original arguments; specifically test the `lua_KContext` stack-base calculation.

### 23.4 Timing and scheduling

1. `delay(0)` resumes on the next frame exactly once.
2. Positive delays honor scaled map time and pause behavior.
3. Multiple due timers resume in stable slot order.
4. A resumed task that starts another immediately complete operation cannot resume twice in the same scheduler update.
5. Starts and completions generated during resume are deferred to the next snapshot/update.

### 23.5 Modules, persistence, and travel

1. Shared and map-local modules resolve in the documented order.
2. `require` caches once within a VM and reloads in the next map VM.
3. Native module search through `package.cpath` is disabled.
4. Persistent bool/int/string values survive map changes.
5. Persistent values serialize and restore before `init` observes them.
6. Lua globals, coroutine locals, timers, and operation handles do not survive save/load.
7. A Lua-requested map change occurs only after the current script/game update reaches the safe transition point.
8. Two same-frame map-change requests preserve the first destination.

### 23.6 Integration acceptance map

Create a minimal test map whose script:

- imports a shared module;
- starts an ambient background loop in `init`;
- performs one blocking delay/action;
- starts two async actions and awaits them in sequence;
- cancels another async action;
- exercises a busy foreground trigger;
- writes persistent state;
- requests travel to a second map;
- confirms `shutdown` exactly once;
- confirms the second map has a fresh VM and restored persistent values.

Run this in a Debug build and verify the debug task/operation list is empty after each map teardown. A normal Debug CMake build must also succeed with warnings enabled.

## 24. Implementation order

Implement in small, verifiable passes:

1. Vendor/build Lua 5.5 and create/close an empty per-map VM.
2. Derive and execute the optional map script; add tracebacks and strict load failures.
3. Register immediate bindings and persistent store access.
4. Add stable task slots, managed function start/resume, and synchronous lifecycle handling.
5. Add operation slots, result values, completion queue, shared continuation, and `delay`.
6. Add `startScript`, stop APIs, lanes, duplicate prevention, and debug snapshots.
7. Add async userdata, `await`, status, cancellation, and reclamation.
8. Integrate the first real sector-engine operation in blocking and async forms.
9. Add deferred map changes and exact shutdown ordering.
10. Integrate save/load persistence and the acceptance map.

At the end of each pass, compile and exercise the smallest relevant script. Do not wait until all gameplay bindings exist before testing cancellation, stale handles, and map teardown; those are the safety-critical parts of the design.

## 25. Final invariants

The implementation is complete only while all of these remain true:

- There is at most one Lua VM, and it belongs to the currently loaded map.
- Map data exists before VM creation and remains valid through `shutdown()`.
- No Lua API is called from a worker thread.
- No code closes a VM from inside a binding or coroutine resume.
- Every managed Lua thread has exactly one registry reference until terminal cleanup.
- Every durable task/operation reference is generational, never a raw container pointer.
- Every waiting task points to one valid pending operation, and every pending operation has at most one waiter.
- Gameplay systems complete operations; the scheduler alone resumes Lua tasks.
- A task resumes at most once per script update.
- Blocking and async variants share the same operation completion/cancellation path.
- Stopping a task cannot leave a blocking-owned backend action orphaned.
- Closing a map cancels all transient tasks/operations and invalidates every Lua handle.
- Persistent script state is engine-owned and serializable; coroutine/VM state is not.
- Missing optional scripts/hooks are valid, while present broken scripts fail loudly.
- `init()` may yield; `shutdown()` never may and is attempted exactly once.
