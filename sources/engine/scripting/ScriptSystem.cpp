#include "engine/scripting/ScriptSystem.h"

#include "engine/EngineContext.h"

#include "lua.hpp"
#include <raylib.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

namespace engine {
namespace {

constexpr size_t InitialTaskCapacity = 32;
constexpr size_t InitialOperationCapacity = 64;
constexpr const char* OperationMetatable = "Engine.ScriptOperation";
static char LuaEngineContextKey;

struct LuaScriptOperationHandle {
    ScriptOperationHandle handle{};
};

uint32_t NextGeneration(uint32_t generation)
{
    ++generation;
    return generation == 0 ? 1 : generation;
}

const char* PhaseName(ScriptRuntimePhase phase)
{
    switch (phase) {
        case ScriptRuntimePhase::Empty: return "empty";
        case ScriptRuntimePhase::Loading: return "loading";
        case ScriptRuntimePhase::Active: return "active";
        case ScriptRuntimePhase::ShuttingDown: return "shutting-down";
    }
    return "unknown";
}

const char* LaneName(ScriptLaunchLane lane)
{
    return lane == ScriptLaunchLane::Foreground ? "foreground" : "background";
}

const char* OperationStateName(ScriptOperationState state)
{
    switch (state) {
        case ScriptOperationState::Free: return "stale";
        case ScriptOperationState::Pending: return "pending";
        case ScriptOperationState::Succeeded: return "succeeded";
        case ScriptOperationState::Failed: return "failed";
        case ScriptOperationState::Cancelled: return "cancelled";
    }
    return "stale";
}

void Log(const ScriptRuntime& runtime, const char* level, const std::string& message)
{
    int traceLevel = LOG_INFO;
    if (level != nullptr && std::string{level} == "ERROR") {
        traceLevel = LOG_ERROR;
    } else if (level != nullptr && std::string{level} == "WARNING") {
        traceLevel = LOG_WARNING;
    } else if (level != nullptr && std::string{level} == "DEBUG") {
        traceLevel = LOG_DEBUG;
    }
    TraceLog(
            traceLevel,
            "[Lua %s] map='%s' script='%s' phase=%s: %s",
            level != nullptr ? level : "INFO",
            runtime.mapId.c_str(),
            runtime.mapScriptPath.c_str(),
            PhaseName(runtime.phase),
            message.c_str());
}

LuaEngineContext& GetLuaContext(lua_State* state)
{
    lua_pushlightuserdata(state, &LuaEngineContextKey);
    lua_gettable(state, LUA_REGISTRYINDEX);
    auto* context = static_cast<LuaEngineContext*>(lua_touserdata(state, -1));
    lua_pop(state, 1);
    if (context == nullptr || context->scripts == nullptr) {
        luaL_error(state, "Lua engine context is unavailable");
    }
    return *context;
}

void SetLuaContext(lua_State* state, LuaEngineContext* context)
{
    lua_pushlightuserdata(state, &LuaEngineContextKey);
    lua_pushlightuserdata(state, context);
    lua_settable(state, LUA_REGISTRYINDEX);
}

ScriptTask* ResolveTask(ScriptRuntime& runtime, ScriptTaskHandle handle)
{
    if (!IsValid(handle) || handle.index >= runtime.tasks.size()) return nullptr;
    ScriptTask& task = runtime.tasks[handle.index];
    return task.occupied && task.generation == handle.generation ? &task : nullptr;
}

const ScriptTask* ResolveTask(const ScriptRuntime& runtime, ScriptTaskHandle handle)
{
    if (!IsValid(handle) || handle.index >= runtime.tasks.size()) return nullptr;
    const ScriptTask& task = runtime.tasks[handle.index];
    return task.occupied && task.generation == handle.generation ? &task : nullptr;
}

ScriptOperation* ResolveOperation(
        ScriptRuntime& runtime,
        ScriptOperationHandle handle)
{
    if (!IsValid(handle) || handle.index >= runtime.operations.size()) return nullptr;
    ScriptOperation& operation = runtime.operations[handle.index];
    return operation.occupied && operation.generation == handle.generation
            ? &operation : nullptr;
}

const ScriptOperation* ResolveOperation(
        const ScriptRuntime& runtime,
        ScriptOperationHandle handle)
{
    if (!IsValid(handle) || handle.index >= runtime.operations.size()) return nullptr;
    const ScriptOperation& operation = runtime.operations[handle.index];
    return operation.occupied && operation.generation == handle.generation
            ? &operation : nullptr;
}

ScriptTaskHandle AllocateTask(ScriptRuntime& runtime)
{
    uint32_t index = 0;
    if (!runtime.freeTaskSlots.empty()) {
        index = runtime.freeTaskSlots.back();
        runtime.freeTaskSlots.pop_back();
    } else {
        if (runtime.tasks.size() == runtime.tasks.capacity()) {
            Log(runtime, "WARNING", "task capacity exceeded; scheduler allocation may occur");
        }
        assert(runtime.tasks.size() < std::numeric_limits<uint32_t>::max());
        index = static_cast<uint32_t>(runtime.tasks.size());
        runtime.tasks.emplace_back();
    }
    ScriptTask& task = runtime.tasks[index];
    const uint32_t generation = NextGeneration(task.generation);
    task.occupied = true;
    task.generation = generation;
    task.state = ScriptTaskState::Free;
    task.lane = ScriptLaunchLane::Background;
    task.functionName.clear();
    task.thread = nullptr;
    task.threadRegistryRef = LUA_NOREF;
    task.waitingOperation = {};
    task.stopRequested = false;
    task.lifecycleInitTask = false;
    task.lastError.clear();
    return ScriptTaskHandle{index, generation};
}

void FreeTask(ScriptRuntime& runtime, ScriptTaskHandle handle)
{
    ScriptTask* task = ResolveTask(runtime, handle);
    if (task == nullptr) return;
    if (task->thread != nullptr) runtime.taskByThread.erase(task->thread);
    const auto named = runtime.taskByName.find(task->functionName);
    if (named != runtime.taskByName.end() && named->second == handle) {
        runtime.taskByName.erase(named);
    }
    if (runtime.vm != nullptr && task->threadRegistryRef != LUA_NOREF) {
        luaL_unref(runtime.vm, LUA_REGISTRYINDEX, task->threadRegistryRef);
    }
    const uint32_t generation = task->generation;
    task->occupied = false;
    task->generation = generation;
    task->state = ScriptTaskState::Free;
    task->lane = ScriptLaunchLane::Background;
    task->functionName.clear();
    task->thread = nullptr;
    task->threadRegistryRef = LUA_NOREF;
    task->waitingOperation = {};
    task->stopRequested = false;
    task->lifecycleInitTask = false;
    task->lastError.clear();
    runtime.freeTaskSlots.push_back(handle.index);
}

ScriptOperationHandle AllocateOperation(ScriptRuntime& runtime)
{
    uint32_t index = 0;
    if (!runtime.freeOperationSlots.empty()) {
        index = runtime.freeOperationSlots.back();
        runtime.freeOperationSlots.pop_back();
    } else {
        if (runtime.operations.size() == runtime.operations.capacity()) {
            Log(runtime, "WARNING", "operation capacity exceeded; scheduler allocation may occur");
        }
        assert(runtime.operations.size() < std::numeric_limits<uint32_t>::max());
        index = static_cast<uint32_t>(runtime.operations.size());
        runtime.operations.emplace_back();
    }
    ScriptOperation& operation = runtime.operations[index];
    const uint32_t generation = NextGeneration(operation.generation);
    operation.occupied = true;
    operation.generation = generation;
    operation.state = ScriptOperationState::Free;
    operation.launchStyle = ScriptOperationLaunchStyle::Blocking;
    operation.debugLabel.clear();
    operation.backendToken = 0;
    operation.cancelBackend = nullptr;
    operation.ownerTask = {};
    operation.waiterTask = {};
    operation.values.clear();
    operation.error.clear();
    operation.luaObserverCount = 0;
    operation.backendCancelSent = false;
    operation.values.reserve(4);
    return ScriptOperationHandle{index, generation};
}

void FreeOperation(ScriptRuntime& runtime, ScriptOperationHandle handle)
{
    ScriptOperation* operation = ResolveOperation(runtime, handle);
    if (operation == nullptr) return;
    const uint32_t generation = operation->generation;
    operation->occupied = false;
    operation->generation = generation;
    operation->state = ScriptOperationState::Free;
    operation->launchStyle = ScriptOperationLaunchStyle::Blocking;
    operation->debugLabel.clear();
    operation->backendToken = 0;
    operation->cancelBackend = nullptr;
    operation->ownerTask = {};
    operation->waiterTask = {};
    operation->values.clear();
    operation->error.clear();
    operation->luaObserverCount = 0;
    operation->backendCancelSent = false;
    runtime.freeOperationSlots.push_back(handle.index);
}

std::string BuildLuaTraceback(
        lua_State* thread,
        const ScriptRuntime& runtime,
        const ScriptTask* task,
        const char* prefix)
{
    const char* message = lua_tostring(thread, -1);
    luaL_traceback(
            thread,
            thread,
            message != nullptr ? message : "<non-string Lua error>",
            1);
    const char* traceback = lua_tostring(thread, -1);
    std::ostringstream output;
    output << (prefix != nullptr ? prefix : "Lua error")
           << " [map=" << runtime.mapId
           << ", script=" << runtime.mapScriptPath
           << ", phase=" << PhaseName(runtime.phase);
    if (task != nullptr) {
        output << ", task=" << task->functionName
               << ", lane=" << LaneName(task->lane);
        const ScriptOperation* operation = ResolveOperation(
                runtime, task->waitingOperation);
        if (operation != nullptr) output << ", operation=" << operation->debugLabel;
    }
    output << "]: "
           << (traceback != nullptr ? traceback : "<traceback unavailable>");
    return output.str();
}

bool PushScriptValue(lua_State* state, const ScriptValue& value)
{
    if (std::holds_alternative<std::monostate>(value)) {
        lua_pushnil(state);
    } else if (const auto* boolean = std::get_if<bool>(&value)) {
        lua_pushboolean(state, *boolean ? 1 : 0);
    } else if (const auto* integer = std::get_if<int64_t>(&value)) {
        lua_pushinteger(state, static_cast<lua_Integer>(*integer));
    } else if (const auto* number = std::get_if<double>(&value)) {
        lua_pushnumber(state, static_cast<lua_Number>(*number));
    } else if (const auto* string = std::get_if<std::string>(&value)) {
        lua_pushlstring(state, string->data(), string->size());
    } else {
        return false;
    }
    return true;
}

bool ReadScriptValue(lua_State* state, int index, ScriptValue& output)
{
    switch (lua_type(state, index)) {
        case LUA_TNIL:
            output = std::monostate{};
            return true;
        case LUA_TBOOLEAN:
            output = lua_toboolean(state, index) != 0;
            return true;
        case LUA_TNUMBER:
            if (lua_isinteger(state, index)) {
                output = static_cast<int64_t>(lua_tointeger(state, index));
            } else {
                output = static_cast<double>(lua_tonumber(state, index));
            }
            return true;
        case LUA_TSTRING: {
            size_t length = 0;
            const char* value = lua_tolstring(state, index, &length);
            output = std::string{value != nullptr ? value : "", length};
            return true;
        }
        default:
            return false;
    }
}

bool HasQueuedName(const ScriptRuntime& runtime, const std::string& name)
{
    if (std::any_of(
            runtime.pendingStarts.begin(),
            runtime.pendingStarts.end(),
            [&name](const ScriptStartRequest& request) {
                return request.functionName == name;
            })) {
        return true;
    }
    if (runtime.activeStartScratchIndex == static_cast<size_t>(-1)) return false;
    for (size_t index = runtime.activeStartScratchIndex + 1;
            index < runtime.startScratch.size(); ++index) {
        if (runtime.startScratch[index].functionName == name) return true;
    }
    return false;
}

bool ForegroundBusy(const ScriptRuntime& runtime)
{
    for (const ScriptStartRequest& request : runtime.pendingStarts) {
        if (request.lane == ScriptLaunchLane::Foreground) return true;
    }
    for (const ScriptTask& task : runtime.tasks) {
        if (task.occupied && task.lane == ScriptLaunchLane::Foreground) return true;
    }
    return false;
}

void MarkInitTerminal(
        ScriptRuntime& runtime,
        bool succeeded,
        const std::string& error)
{
    runtime.initFinished = succeeded;
    runtime.loadingSave = false;
    if (!succeeded) {
        runtime.mapAbortRequested = true;
        runtime.mapAbortError = error.empty() ? "map init() failed" : error;
    }
}

int FinishOperationWait(lua_State* state, int status, lua_KContext context)
{
    (void)status;
    const int originalTop = static_cast<int>(context);
    return lua_gettop(state) - originalTop;
}

void ProcessResumeResult(
        ScriptRuntime& runtime,
        ScriptTaskHandle handle,
        int status,
        int resultCount,
        std::vector<ScriptValue>* immediateValues,
        std::string* outError)
{
    ScriptTask* task = ResolveTask(runtime, handle);
    if (task == nullptr) return;

    if (status == LUA_YIELD) {
        const ScriptOperation* operation = ResolveOperation(
                runtime, task->waitingOperation);
        if (operation != nullptr
                && operation->state == ScriptOperationState::Pending
                && operation->waiterTask == handle) {
            task->state = ScriptTaskState::Waiting;
            return;
        }
        lua_settop(task->thread, 0);
        task->state = ScriptTaskState::Failed;
        task->lastError = "unsupported bare coroutine.yield(): managed tasks must wait on an engine operation";
        if (task->lifecycleInitTask) {
            MarkInitTerminal(runtime, false, task->lastError);
        }
        if (outError != nullptr) *outError = task->lastError;
        Log(runtime, "ERROR", task->lastError);
        FreeTask(runtime, handle);
        return;
    }

    if (status == LUA_OK) {
        bool valuesValid = true;
        if (immediateValues != nullptr && resultCount > 0) {
            const int first = lua_gettop(task->thread) - resultCount + 1;
            immediateValues->reserve(static_cast<size_t>(resultCount));
            for (int index = first; index <= lua_gettop(task->thread); ++index) {
                ScriptValue value;
                if (!ReadScriptValue(task->thread, index, value)) {
                    valuesValid = false;
                    break;
                }
                immediateValues->push_back(std::move(value));
            }
        }
        lua_settop(task->thread, 0);
        if (!valuesValid) {
            task->state = ScriptTaskState::Failed;
            task->lastError = "Lua task returned a value unsupported by ScriptValue";
            if (outError != nullptr) *outError = task->lastError;
            if (task->lifecycleInitTask) MarkInitTerminal(runtime, false, task->lastError);
            Log(runtime, "ERROR", task->lastError);
        } else if (task->stopRequested) {
            task->state = ScriptTaskState::Cancelled;
            if (task->lifecycleInitTask) {
                MarkInitTerminal(runtime, false, "init() was cancelled");
            }
        } else {
            task->state = ScriptTaskState::Completed;
            if (task->lifecycleInitTask) MarkInitTerminal(runtime, true, {});
        }
        FreeTask(runtime, handle);
        return;
    }

    task->state = ScriptTaskState::Failed;
    task->lastError = BuildLuaTraceback(
            task->thread, runtime, task, "managed task failed");
    lua_settop(task->thread, 0);
    if (outError != nullptr) *outError = task->lastError;
    if (task->lifecycleInitTask) MarkInitTerminal(runtime, false, task->lastError);
    Log(runtime, "ERROR", task->lastError);
    FreeTask(runtime, handle);
}

ScriptCallOutcome StartManagedFunction(
        ScriptRuntime& runtime,
        const std::string& functionName,
        ScriptLaunchLane lane,
        bool lifecycleInitTask)
{
    ScriptCallOutcome outcome;
    if (runtime.vm == nullptr
            || (runtime.phase != ScriptRuntimePhase::Loading
                    && runtime.phase != ScriptRuntimePhase::Active)) {
        outcome.error = "script runtime is not active";
        return outcome;
    }
    if (runtime.taskByName.find(functionName) != runtime.taskByName.end()
            || HasQueuedName(runtime, functionName)) {
        outcome.result = ScriptCallResult::AlreadyRunning;
        outcome.error = "script already queued or running: " + functionName;
        return outcome;
    }
    if (lane == ScriptLaunchLane::Foreground && ForegroundBusy(runtime)) {
        outcome.result = ScriptCallResult::ForegroundBusy;
        outcome.error = "foreground script lane is busy";
        return outcome;
    }

    lua_getglobal(runtime.vm, functionName.c_str());
    if (lua_isnil(runtime.vm, -1)) {
        lua_pop(runtime.vm, 1);
        outcome.result = ScriptCallResult::Missing;
        return outcome;
    }
    if (!lua_isfunction(runtime.vm, -1)) {
        lua_pop(runtime.vm, 1);
        outcome.error = "global '" + functionName + "' is not a function";
        return outcome;
    }

    const ScriptTaskHandle handle = AllocateTask(runtime);
    ScriptTask* task = ResolveTask(runtime, handle);
    task->state = ScriptTaskState::Running;
    task->lane = lane;
    task->functionName = functionName;
    task->lifecycleInitTask = lifecycleInitTask;

    task->thread = lua_newthread(runtime.vm);
    task->threadRegistryRef = luaL_ref(runtime.vm, LUA_REGISTRYINDEX);
    lua_xmove(runtime.vm, task->thread, 1);
    runtime.taskByThread.emplace(task->thread, handle);
    runtime.taskByName.emplace(functionName, handle);

    lua_State* thread = task->thread;
    int resultCount = 0;
    const int status = lua_resume(thread, nullptr, 0, &resultCount);
    outcome.task = handle;
    ProcessResumeResult(
            runtime,
            handle,
            status,
            resultCount,
            &outcome.immediateValues,
            &outcome.error);
    if (status == LUA_YIELD && ResolveTask(runtime, handle) != nullptr) {
        outcome.result = ScriptCallResult::Started;
    } else if (status == LUA_OK && outcome.error.empty()) {
        outcome.result = ScriptCallResult::Completed;
    } else {
        outcome.result = ScriptCallResult::Error;
    }
    return outcome;
}

void DetachWaiter(ScriptRuntime& runtime, ScriptTaskHandle taskHandle)
{
    ScriptTask* task = ResolveTask(runtime, taskHandle);
    if (task == nullptr || !IsValid(task->waitingOperation)) return;
    ScriptOperation* operation = ResolveOperation(runtime, task->waitingOperation);
    if (operation != nullptr && operation->waiterTask == taskHandle) {
        operation->waiterTask = {};
    }
    task->waitingOperation = {};
}

void CancelTaskNow(
        EngineContext& engine,
        ScriptRuntime& runtime,
        ScriptTaskHandle handle)
{
    ScriptTask* task = ResolveTask(runtime, handle);
    if (task == nullptr) return;
    const ScriptOperationHandle waiting = task->waitingOperation;
    ScriptOperation* operation = ResolveOperation(runtime, waiting);
    if (operation != nullptr) {
        const bool ownsBlocking = operation->launchStyle
                        == ScriptOperationLaunchStyle::Blocking
                && operation->ownerTask == handle;
        if (ownsBlocking) {
            ScriptSystemCancelOperation(
                    engine, runtime, waiting, "owning script stopped");
            operation = ResolveOperation(runtime, waiting);
            if (operation != nullptr && operation->waiterTask == handle) {
                operation->waiterTask = {};
            }
        } else if (operation->waiterTask == handle) {
            operation->waiterTask = {};
        }
    }
    task = ResolveTask(runtime, handle);
    if (task != nullptr) {
        task->waitingOperation = {};
        task->state = ScriptTaskState::Cancelled;
        if (task->lifecycleInitTask) {
            MarkInitTerminal(runtime, false, "init() was cancelled");
        }
        FreeTask(runtime, handle);
    }
}

bool TransitionOperation(
        ScriptRuntime& runtime,
        ScriptOperationHandle handle,
        ScriptOperationState terminalState,
        std::vector<ScriptValue> values,
        std::string error)
{
    ScriptOperation* operation = ResolveOperation(runtime, handle);
    if (operation == nullptr || operation->state != ScriptOperationState::Pending) {
        return false;
    }
    operation->state = terminalState;
    operation->values = std::move(values);
    operation->error = std::move(error);
    return true;
}

int PushOperationResult(lua_State* state, const ScriptOperation& operation)
{
    if (operation.state == ScriptOperationState::Succeeded) {
        lua_pushboolean(state, 1);
        int count = 1;
        for (const ScriptValue& value : operation.values) {
            PushScriptValue(state, value);
            ++count;
        }
        return count;
    }
    lua_pushboolean(state, 0);
    const std::string reason = operation.error.empty()
            ? (operation.state == ScriptOperationState::Cancelled
                    ? "cancelled" : "operation failed")
            : operation.error;
    lua_pushlstring(state, reason.data(), reason.size());
    return 2;
}

LuaScriptOperationHandle* CheckOperationUserdata(lua_State* state, int index)
{
    return static_cast<LuaScriptOperationHandle*>(
            luaL_checkudata(state, index, OperationMetatable));
}

int LuaOperationGc(lua_State* state)
{
    auto* userdata = CheckOperationUserdata(state, 1);
    LuaEngineContext& context = GetLuaContext(state);
    ScriptOperation* operation = ResolveOperation(
            *context.scripts, userdata->handle);
    if (operation != nullptr && operation->luaObserverCount > 0) {
        --operation->luaObserverCount;
    }
    userdata->handle = {};
    return 0;
}

int LuaOperationToString(lua_State* state)
{
    auto* userdata = CheckOperationUserdata(state, 1);
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    const ScriptOperation* operation = ResolveOperation(runtime, userdata->handle);
    if (operation == nullptr) {
        lua_pushliteral(state, "ScriptOperation(stale)");
        return 1;
    }
    const std::string text = "ScriptOperation(" + operation->debugLabel + ", "
            + OperationStateName(operation->state) + ")";
    lua_pushlstring(state, text.data(), text.size());
    return 1;
}

int LuaAwait(lua_State* state)
{
    auto* userdata = CheckOperationUserdata(state, 1);
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    const ScriptTaskHandle task = ScriptSystemCurrentTaskFromLua(state);
    const int originalTop = lua_gettop(state);
    ScriptOperation* operation = ResolveOperation(runtime, userdata->handle);
    if (operation == nullptr) {
        lua_pushboolean(state, 0);
        lua_pushliteral(state, "stale operation");
        return 2;
    }
    if (operation->state != ScriptOperationState::Pending) {
        return PushOperationResult(state, *operation);
    }
    if (IsValid(operation->waiterTask) && !(operation->waiterTask == task)) {
        lua_pushboolean(state, 0);
        lua_pushliteral(state, "operation already has a waiter");
        return 2;
    }
    operation->waiterTask = task;
    return ScriptSystemYieldForOperation(
            state, userdata->handle, originalTop);
}

int LuaOperationStatus(lua_State* state)
{
    auto* userdata = CheckOperationUserdata(state, 1);
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    const ScriptOperation* operation = ResolveOperation(runtime, userdata->handle);
    if (operation == nullptr) {
        lua_pushliteral(state, "stale");
        return 1;
    }
    lua_pushstring(state, OperationStateName(operation->state));
    if ((operation->state == ScriptOperationState::Failed
                || operation->state == ScriptOperationState::Cancelled)
            && !operation->error.empty()) {
        lua_pushlstring(state, operation->error.data(), operation->error.size());
        return 2;
    }
    return 1;
}

int LuaCancelOperation(lua_State* state)
{
    auto* userdata = CheckOperationUserdata(state, 1);
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    EngineContext& engine = ScriptSystemEngineFromLua(state);
    ScriptOperation* operation = ResolveOperation(runtime, userdata->handle);
    if (operation == nullptr) {
        lua_pushboolean(state, 0);
        lua_pushliteral(state, "stale operation");
        return 2;
    }
    if (operation->state != ScriptOperationState::Pending) {
        lua_pushboolean(state, 0);
        lua_pushliteral(state, "operation is already complete");
        return 2;
    }
    ScriptSystemCancelOperation(engine, runtime, userdata->handle);
    lua_pushboolean(state, 1);
    return 1;
}

int LuaDelay(lua_State* state)
{
    const lua_Number milliseconds = luaL_checknumber(state, 1);
    if (!std::isfinite(static_cast<double>(milliseconds)) || milliseconds < 0.0) {
        return luaL_argerror(state, 1, "duration must be finite and non-negative");
    }
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    const ScriptTaskHandle task = ScriptSystemCurrentTaskFromLua(state);
    const int originalTop = lua_gettop(state);
    const uint64_t token = runtime.nextTimerToken++;
    const ScriptOperationHandle operation = ScriptSystemCreateOperation(
            runtime,
            ScriptOperationLaunchStyle::Blocking,
            task,
            "delay:" + std::to_string(static_cast<double>(milliseconds)) + "ms",
            token,
            nullptr);
    if (runtime.timers.size() == runtime.timers.capacity()) {
        Log(runtime, "WARNING", "timer capacity exceeded; scheduler allocation may occur");
    }
    runtime.timers.push_back(ScriptTimer{
            token,
            operation,
            static_cast<double>(milliseconds) / 1000.0});
    return ScriptSystemYieldForOperation(state, operation, originalTop);
}

int LuaStartScript(lua_State* state)
{
    const char* name = luaL_checkstring(state, 1);
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    std::string error;
    const bool queued = ScriptSystemQueueBackground(runtime, name, error);
    lua_pushboolean(state, queued ? 1 : 0);
    if (!queued) {
        lua_pushlstring(state, error.data(), error.size());
        return 2;
    }
    return 1;
}

int LuaStopScript(lua_State* state)
{
    const char* name = luaL_checkstring(state, 1);
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    EngineContext& engine = ScriptSystemEngineFromLua(state);
    std::string error;
    const bool stopped = ScriptSystemStopFunction(engine, runtime, name, error);
    lua_pushboolean(state, stopped ? 1 : 0);
    if (!stopped) {
        lua_pushlstring(state, error.data(), error.size());
        return 2;
    }
    return 1;
}

int LuaStopAllScripts(lua_State* state)
{
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    EngineContext& engine = ScriptSystemEngineFromLua(state);
    ScriptSystemStopAll(engine, runtime);
    lua_pushboolean(state, 1);
    return 1;
}

int LuaIsScriptRunning(lua_State* state)
{
    const char* name = luaL_checkstring(state, 1);
    lua_pushboolean(
            state,
            ScriptSystemIsFunctionRunning(
                    ScriptSystemRuntimeFromLua(state), name) ? 1 : 0);
    return 1;
}

std::string PersistentKey(lua_State* state)
{
    size_t length = 0;
    const char* key = luaL_checklstring(state, 1, &length);
    if (length == 0) luaL_argerror(state, 1, "key must not be empty");
    return std::string{key, length};
}

PersistentScriptStore& PersistentFromLua(lua_State* state)
{
    LuaEngineContext& context = GetLuaContext(state);
    if (context.persistent == nullptr) luaL_error(state, "persistent store is unavailable");
    return *context.persistent;
}

int LuaSetPersistentBool(lua_State* state)
{
    const std::string key = PersistentKey(state);
    luaL_checktype(state, 2, LUA_TBOOLEAN);
    PersistentFromLua(state).bools[key] = lua_toboolean(state, 2) != 0;
    return 0;
}

int LuaGetPersistentBool(lua_State* state)
{
    const std::string key = PersistentKey(state);
    const bool fallback = lua_gettop(state) >= 2
            ? (luaL_checktype(state, 2, LUA_TBOOLEAN), lua_toboolean(state, 2) != 0)
            : false;
    const auto& values = PersistentFromLua(state).bools;
    const auto found = values.find(key);
    lua_pushboolean(state, found != values.end() ? found->second : fallback);
    return 1;
}

int LuaSetPersistentInt(lua_State* state)
{
    const std::string key = PersistentKey(state);
    PersistentFromLua(state).ints[key] = static_cast<int64_t>(luaL_checkinteger(state, 2));
    return 0;
}

int LuaGetPersistentInt(lua_State* state)
{
    const std::string key = PersistentKey(state);
    const int64_t fallback = lua_gettop(state) >= 2
            ? static_cast<int64_t>(luaL_checkinteger(state, 2)) : 0;
    const auto& values = PersistentFromLua(state).ints;
    const auto found = values.find(key);
    lua_pushinteger(state, static_cast<lua_Integer>(
            found != values.end() ? found->second : fallback));
    return 1;
}

int LuaSetPersistentString(lua_State* state)
{
    const std::string key = PersistentKey(state);
    size_t length = 0;
    const char* value = luaL_checklstring(state, 2, &length);
    PersistentFromLua(state).strings[key] = std::string{value, length};
    return 0;
}

int LuaGetPersistentString(lua_State* state)
{
    const std::string key = PersistentKey(state);
    size_t fallbackLength = 0;
    const char* fallback = lua_gettop(state) >= 2
            ? luaL_checklstring(state, 2, &fallbackLength) : "";
    const auto& values = PersistentFromLua(state).strings;
    const auto found = values.find(key);
    if (found != values.end()) {
        lua_pushlstring(state, found->second.data(), found->second.size());
    } else {
        lua_pushlstring(state, fallback, fallbackLength);
    }
    return 1;
}

int LuaLog(lua_State* state)
{
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    std::string message;
    const int count = lua_gettop(state);
    for (int index = 1; index <= count; ++index) {
        if (index > 1) message.push_back('\t');
        switch (lua_type(state, index)) {
            case LUA_TNIL:
                message += "nil";
                break;
            case LUA_TBOOLEAN:
                message += lua_toboolean(state, index) ? "true" : "false";
                break;
            case LUA_TNUMBER: {
                std::ostringstream value;
                value.imbue(std::locale::classic());
                if (lua_isinteger(state, index)) {
                    value << lua_tointeger(state, index);
                } else {
                    value << lua_tonumber(state, index);
                }
                message += value.str();
                break;
            }
            case LUA_TSTRING: {
                size_t length = 0;
                const char* value = lua_tolstring(state, index, &length);
                if (value != nullptr) message.append(value, length);
                break;
            }
            default:
                message += '<';
                message += luaL_typename(state, index);
                message += '>';
                break;
        }
    }
    Log(runtime, "INFO", message);
    return 0;
}

int LuaIsLoadingSave(lua_State* state)
{
    lua_pushboolean(state, ScriptSystemRuntimeFromLua(state).loadingSave ? 1 : 0);
    return 1;
}

void RegisterFunction(lua_State* state, const char* name, lua_CFunction function)
{
    lua_pushcfunction(state, function);
    lua_setglobal(state, name);
}

void RegisterCoreBindings(lua_State* state)
{
    if (luaL_newmetatable(state, OperationMetatable)) {
        lua_pushcfunction(state, LuaOperationGc);
        lua_setfield(state, -2, "__gc");
        lua_pushcfunction(state, LuaOperationToString);
        lua_setfield(state, -2, "__tostring");
        lua_pushliteral(state, "protected ScriptOperation metatable");
        lua_setfield(state, -2, "__metatable");
        lua_pushliteral(state, "Engine.ScriptOperation");
        lua_setfield(state, -2, "__name");
    }
    lua_pop(state, 1);

    RegisterFunction(state, "delay", LuaDelay);
    RegisterFunction(state, "startScript", LuaStartScript);
    RegisterFunction(state, "stopScript", LuaStopScript);
    RegisterFunction(state, "stopAllScripts", LuaStopAllScripts);
    RegisterFunction(state, "isScriptRunning", LuaIsScriptRunning);
    RegisterFunction(state, "await", LuaAwait);
    RegisterFunction(state, "operationStatus", LuaOperationStatus);
    RegisterFunction(state, "cancelOperation", LuaCancelOperation);
    RegisterFunction(state, "setPersistentBool", LuaSetPersistentBool);
    RegisterFunction(state, "getPersistentBool", LuaGetPersistentBool);
    RegisterFunction(state, "setPersistentInt", LuaSetPersistentInt);
    RegisterFunction(state, "getPersistentInt", LuaGetPersistentInt);
    RegisterFunction(state, "setPersistentString", LuaSetPersistentString);
    RegisterFunction(state, "getPersistentString", LuaGetPersistentString);
    RegisterFunction(state, "log", LuaLog);
    RegisterFunction(state, "print", LuaLog);
    RegisterFunction(state, "isLoadingSave", LuaIsLoadingSave);
}

std::string NormalizeLuaPath(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

bool ReadFile(const std::string& path, std::string& bytes, std::string& error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "could not open Lua script '" + path + "'";
        return false;
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
        error = "could not read Lua script '" + path + "'";
        return false;
    }
    bytes = contents.str();
    return true;
}

void ConfigurePackagePaths(
        lua_State* state,
        const std::filesystem::path& mapDirectory,
        const std::filesystem::path& assetRoot)
{
    const std::string map = NormalizeLuaPath(mapDirectory);
    const std::string shared = NormalizeLuaPath(assetRoot / "scripts");
    const std::string path = map + "/?.lua;" + map + "/?/init.lua;"
            + shared + "/?.lua;" + shared + "/?/init.lua";
    lua_getglobal(state, "package");
    lua_pushlstring(state, path.data(), path.size());
    lua_setfield(state, -2, "path");
    lua_pushliteral(state, "");
    lua_setfield(state, -2, "cpath");
    lua_pop(state, 1);
}

void PrepareRuntimeStorage(ScriptRuntime& runtime)
{
    runtime.tasks.reserve(std::max(runtime.tasks.capacity(), InitialTaskCapacity));
    runtime.operations.reserve(std::max(runtime.operations.capacity(), InitialOperationCapacity));
    runtime.freeTaskSlots.reserve(InitialTaskCapacity);
    runtime.freeOperationSlots.reserve(InitialOperationCapacity);
    runtime.pendingStarts.reserve(InitialTaskCapacity);
    runtime.pendingCompletions.reserve(InitialOperationCapacity);
    runtime.completionInbox.reserve(InitialOperationCapacity);
    runtime.taskScratch.reserve(InitialTaskCapacity);
    runtime.startScratch.reserve(InitialTaskCapacity);
    runtime.completionScratch.reserve(InitialOperationCapacity);
    runtime.timers.reserve(InitialOperationCapacity);
    runtime.taskByThread.reserve(InitialTaskCapacity);
    runtime.taskByName.reserve(InitialTaskCapacity);
}

void ResetRuntimeForCreate(ScriptRuntime& runtime)
{
    assert(runtime.vm == nullptr);
    runtime.phase = ScriptRuntimePhase::Empty;
    runtime.engine = nullptr;
    runtime.persistent = nullptr;
    runtime.hostContext = nullptr;
    runtime.luaContext = LuaEngineContext{};
    runtime.mapId.clear();
    runtime.mapScriptPath.clear();
    runtime.mapChunkPresent = false;
    runtime.initAttempted = false;
    runtime.initFinished = false;
    runtime.shutdownAttempted = false;
    runtime.loadingSave = false;
    runtime.pendingStarts.clear();
    runtime.pendingCompletions.clear();
    {
        std::lock_guard<std::mutex> lock(runtime.completionInboxMutex);
        runtime.completionInbox.clear();
    }
    runtime.taskByThread.clear();
    runtime.taskByName.clear();
    runtime.taskScratch.clear();
    runtime.startScratch.clear();
    runtime.activeStartScratchIndex = static_cast<size_t>(-1);
    runtime.completionScratch.clear();
    runtime.timers.clear();
    runtime.mapChangeRequested = false;
    runtime.requestedMapId.clear();
    runtime.requestedSpawnId.clear();
    runtime.mapAbortRequested = false;
    runtime.mapAbortError.clear();
    runtime.consoleExecuting = false;
}

void ReclaimTerminalOperations(ScriptRuntime& runtime)
{
    for (uint32_t index = 0; index < runtime.operations.size(); ++index) {
        ScriptOperation& operation = runtime.operations[index];
        if (!operation.occupied
                || operation.state == ScriptOperationState::Pending
                || IsValid(operation.waiterTask)
                || operation.luaObserverCount != 0) {
            continue;
        }
        FreeOperation(runtime, ScriptOperationHandle{index, operation.generation});
    }
}

} // namespace

bool ScriptSystemCreateForMap(
        EngineContext& engine,
        ScriptRuntime& runtime,
        PersistentScriptStore& persistent,
        const std::string& mapId,
        const std::string& mapFilePath,
        const std::string& assetRoot,
        void* hostContext,
        ScriptBindingRegisterFn registerHostBindings,
        bool loadingSave,
        std::string& error)
{
    if (runtime.vm != nullptr) ScriptSystemShutdownForMap(engine, runtime);
    ResetRuntimeForCreate(runtime);
    PrepareRuntimeStorage(runtime);
    runtime.phase = ScriptRuntimePhase::Loading;
    runtime.engine = &engine;
    runtime.persistent = &persistent;
    runtime.hostContext = hostContext;
    runtime.mapId = mapId;
    std::filesystem::path scriptPath{mapFilePath};
    scriptPath.replace_extension(".lua");
    runtime.mapScriptPath = NormalizeLuaPath(scriptPath);
    runtime.loadingSave = loadingSave;

    runtime.vm = luaL_newstate();
    if (runtime.vm == nullptr) {
        error = "could not create Lua 5.5 state";
        runtime.phase = ScriptRuntimePhase::Empty;
        return false;
    }
    luaL_openlibs(runtime.vm);
    runtime.luaContext = LuaEngineContext{
            &runtime, &engine, &persistent, hostContext};
    SetLuaContext(runtime.vm, &runtime.luaContext);
    RegisterCoreBindings(runtime.vm);
    if (registerHostBindings != nullptr) registerHostBindings(runtime.vm);
    ConfigurePackagePaths(runtime.vm, scriptPath.parent_path(), assetRoot);
    lua_pushnumber(runtime.vm, 0.0);
    lua_setglobal(runtime.vm, "FrameDelta");

    std::error_code existsError;
    const bool scriptExists = std::filesystem::exists(scriptPath, existsError);
    if (existsError) {
        error = "could not inspect Lua script '" + runtime.mapScriptPath
                + "': " + existsError.message();
        ScriptSystemShutdownForMap(engine, runtime);
        return false;
    }
    if (scriptExists) {
        std::string bytes;
        if (!ReadFile(runtime.mapScriptPath, bytes, error)) {
            ScriptSystemShutdownForMap(engine, runtime);
            return false;
        }
        runtime.mapChunkPresent = true;
        const std::string chunkName = "@" + runtime.mapScriptPath;
        int status = luaL_loadbufferx(
                runtime.vm,
                bytes.data(),
                bytes.size(),
                chunkName.c_str(),
                "t");
        if (status == LUA_OK) status = lua_pcall(runtime.vm, 0, 0, 0);
        if (status != LUA_OK) {
            error = BuildLuaTraceback(
                    runtime.vm, runtime, nullptr, "map chunk failed");
            lua_settop(runtime.vm, 0);
            ScriptSystemShutdownForMap(engine, runtime);
            return false;
        }
        lua_settop(runtime.vm, 0);
    } else {
        Log(runtime, "INFO", "optional map script is absent");
    }

    runtime.initAttempted = true;
    ScriptCallOutcome init = StartManagedFunction(
            runtime, "init", ScriptLaunchLane::Foreground, true);
    if (init.result == ScriptCallResult::Missing) {
        runtime.initFinished = true;
        runtime.loadingSave = false;
    } else if (init.result == ScriptCallResult::Error) {
        error = init.error.empty() ? "init() failed" : init.error;
        runtime.mapAbortRequested = false;
        runtime.mapAbortError.clear();
        ScriptSystemShutdownForMap(engine, runtime);
        return false;
    }
    runtime.phase = ScriptRuntimePhase::Active;
    error.clear();
    Log(runtime, "INFO", runtime.initFinished
            ? "map VM created; init completed immediately"
            : "map VM created; init is suspended");
    return true;
}

void ScriptSystemUpdate(
        EngineContext& engine,
        ScriptRuntime& runtime,
        float dtSeconds)
{
    if (runtime.vm == nullptr || runtime.phase != ScriptRuntimePhase::Active) return;
    lua_pushnumber(runtime.vm, static_cast<lua_Number>(dtSeconds));
    lua_setglobal(runtime.vm, "FrameDelta");

    runtime.completionScratch.clear();
    {
        std::lock_guard<std::mutex> lock(runtime.completionInboxMutex);
        runtime.completionScratch.swap(runtime.completionInbox);
    }
    for (ScriptCompletionRecord& completion : runtime.completionScratch) {
        TransitionOperation(
                runtime,
                completion.operation,
                completion.terminalState,
                std::move(completion.values),
                std::move(completion.error));
    }

    const double delta = std::isfinite(dtSeconds) && dtSeconds > 0.0f
            ? static_cast<double>(dtSeconds) : 0.0;
    for (ScriptTimer& timer : runtime.timers) {
        ScriptOperation* operation = ResolveOperation(runtime, timer.operation);
        if (operation == nullptr || operation->state != ScriptOperationState::Pending) continue;
        timer.remainingSeconds -= delta;
        if (timer.remainingSeconds <= 0.0) {
            ScriptSystemCompleteOperation(runtime, timer.operation);
        }
    }
    runtime.timers.erase(
            std::remove_if(
                    runtime.timers.begin(),
                    runtime.timers.end(),
                    [&runtime](const ScriptTimer& timer) {
                        const ScriptOperation* operation = ResolveOperation(
                                runtime, timer.operation);
                        return operation == nullptr
                                || operation->state != ScriptOperationState::Pending;
                    }),
            runtime.timers.end());

    runtime.startScratch.clear();
    runtime.startScratch.swap(runtime.pendingStarts);
    for (size_t index = 0; index < runtime.startScratch.size(); ++index) {
        runtime.activeStartScratchIndex = index;
        const ScriptStartRequest& request = runtime.startScratch[index];
        if (request.functionName.empty()) continue;
        const ScriptCallOutcome outcome = StartManagedFunction(
                runtime, request.functionName, request.lane, false);
        if (outcome.result == ScriptCallResult::Error) {
            Log(runtime, "ERROR", outcome.error);
        }
    }
    runtime.activeStartScratchIndex = static_cast<size_t>(-1);
    runtime.startScratch.clear();

    runtime.taskScratch.clear();
    for (uint32_t index = 0; index < runtime.tasks.size(); ++index) {
        const ScriptTask& task = runtime.tasks[index];
        if (!task.occupied || task.stopRequested || !IsValid(task.waitingOperation)) continue;
        const ScriptOperation* operation = ResolveOperation(
                runtime, task.waitingOperation);
        if (operation != nullptr
                && operation->state != ScriptOperationState::Pending) {
            runtime.taskScratch.push_back(ScriptTaskHandle{index, task.generation});
        }
    }
    for (ScriptTaskHandle handle : runtime.taskScratch) {
        ScriptTask* task = ResolveTask(runtime, handle);
        if (task == nullptr || task->stopRequested) continue;
        const ScriptOperationHandle operationHandle = task->waitingOperation;
        ScriptOperation* operation = ResolveOperation(runtime, operationHandle);
        if (operation == nullptr || operation->state == ScriptOperationState::Pending) continue;

        if (operation->waiterTask == handle) operation->waiterTask = {};
        const int valueCount = PushOperationResult(task->thread, *operation);
        task->waitingOperation = {};
        task->state = ScriptTaskState::Running;
        lua_State* thread = task->thread;
        int resultCount = 0;
        const int status = lua_resume(thread, nullptr, valueCount, &resultCount);
        ProcessResumeResult(runtime, handle, status, resultCount, nullptr, nullptr);
    }

    runtime.taskScratch.clear();
    for (uint32_t index = 0; index < runtime.tasks.size(); ++index) {
        const ScriptTask& task = runtime.tasks[index];
        if (task.occupied && task.stopRequested) {
            runtime.taskScratch.push_back(ScriptTaskHandle{index, task.generation});
        }
    }
    for (ScriptTaskHandle handle : runtime.taskScratch) {
        CancelTaskNow(engine, runtime, handle);
    }
    ReclaimTerminalOperations(runtime);
}

void ScriptSystemShutdownForMap(
        EngineContext& engine,
        ScriptRuntime& runtime)
{
    if (runtime.vm == nullptr) {
        runtime.engine = nullptr;
        runtime.persistent = nullptr;
        runtime.hostContext = nullptr;
        runtime.luaContext = LuaEngineContext{};
        runtime.phase = ScriptRuntimePhase::Empty;
        return;
    }
    runtime.phase = ScriptRuntimePhase::ShuttingDown;
    if (!runtime.shutdownAttempted) {
        runtime.shutdownAttempted = true;
        lua_getglobal(runtime.vm, "shutdown");
        if (lua_isnil(runtime.vm, -1)) {
            lua_pop(runtime.vm, 1);
        } else if (!lua_isfunction(runtime.vm, -1)) {
            Log(runtime, "ERROR", "global 'shutdown' is not a function");
            lua_pop(runtime.vm, 1);
        } else if (lua_pcall(runtime.vm, 0, 0, 0) != LUA_OK) {
            const std::string shutdownError = BuildLuaTraceback(
                    runtime.vm, runtime, nullptr, "shutdown() failed");
            Log(runtime, "ERROR", shutdownError);
            lua_settop(runtime.vm, 0);
        }
    }

    runtime.pendingStarts.clear();
    runtime.pendingCompletions.clear();
    {
        std::lock_guard<std::mutex> lock(runtime.completionInboxMutex);
        runtime.completionInbox.clear();
    }
    for (uint32_t index = 0; index < runtime.operations.size(); ++index) {
        ScriptOperation& operation = runtime.operations[index];
        if (operation.occupied && operation.state == ScriptOperationState::Pending) {
            ScriptSystemCancelOperation(
                    engine,
                    runtime,
                    ScriptOperationHandle{index, operation.generation},
                    "map shutdown");
        }
    }
    for (uint32_t index = 0; index < runtime.tasks.size(); ++index) {
        ScriptTask& task = runtime.tasks[index];
        if (task.occupied) {
            DetachWaiter(runtime, ScriptTaskHandle{index, task.generation});
            FreeTask(runtime, ScriptTaskHandle{index, task.generation});
        }
    }
    runtime.taskByThread.clear();
    runtime.taskByName.clear();
    lua_close(runtime.vm);
    runtime.vm = nullptr;

    runtime.freeOperationSlots.clear();
    for (uint32_t index = 0; index < runtime.operations.size(); ++index) {
        ScriptOperation& operation = runtime.operations[index];
        const uint32_t generation = operation.generation;
        operation.occupied = false;
        operation.generation = generation;
        operation.state = ScriptOperationState::Free;
        operation.launchStyle = ScriptOperationLaunchStyle::Blocking;
        operation.debugLabel.clear();
        operation.backendToken = 0;
        operation.cancelBackend = nullptr;
        operation.ownerTask = {};
        operation.waiterTask = {};
        operation.values.clear();
        operation.error.clear();
        operation.luaObserverCount = 0;
        operation.backendCancelSent = false;
        runtime.freeOperationSlots.push_back(index);
    }
    runtime.timers.clear();
    runtime.engine = nullptr;
    runtime.persistent = nullptr;
    runtime.hostContext = nullptr;
    runtime.luaContext = LuaEngineContext{};
    runtime.mapChangeRequested = false;
    runtime.requestedMapId.clear();
    runtime.requestedSpawnId.clear();
    runtime.mapAbortRequested = false;
    runtime.mapAbortError.clear();
    runtime.consoleExecuting = false;
    runtime.phase = ScriptRuntimePhase::Empty;
    Log(runtime, "INFO", "map VM closed");
}

ScriptCallOutcome ScriptSystemCallForegroundHook(
        ScriptRuntime& runtime,
        const std::string& functionName)
{
    return StartManagedFunction(
            runtime, functionName, ScriptLaunchLane::Foreground, false);
}

bool ScriptSystemQueueBackground(
        ScriptRuntime& runtime,
        const std::string& functionName,
        std::string& outError)
{
    if (runtime.vm == nullptr
            || (runtime.phase != ScriptRuntimePhase::Loading
                    && runtime.phase != ScriptRuntimePhase::Active)) {
        outError = "script runtime is shutting down";
        return false;
    }
    if (functionName.empty()) {
        outError = "function name must not be empty";
        return false;
    }
    if (runtime.taskByName.find(functionName) != runtime.taskByName.end()
            || HasQueuedName(runtime, functionName)) {
        outError = "script already queued or running: " + functionName;
        return false;
    }
    lua_getglobal(runtime.vm, functionName.c_str());
    const bool found = lua_isfunction(runtime.vm, -1);
    const bool missing = lua_isnil(runtime.vm, -1);
    lua_pop(runtime.vm, 1);
    if (!found) {
        outError = missing ? "function not found: " + functionName
                           : "global is not a function: " + functionName;
        return false;
    }
    if (runtime.pendingStarts.size() == runtime.pendingStarts.capacity()) {
        Log(runtime, "WARNING", "pending-start capacity exceeded; scheduler allocation may occur");
    }
    runtime.pendingStarts.push_back(
            ScriptStartRequest{functionName, ScriptLaunchLane::Background});
    outError.clear();
    return true;
}

bool ScriptSystemStopFunction(
        EngineContext& engine,
        ScriptRuntime& runtime,
        const std::string& functionName,
        std::string& outError)
{
    (void)engine;
    auto queued = std::find_if(
            runtime.pendingStarts.begin(),
            runtime.pendingStarts.end(),
            [&functionName](const ScriptStartRequest& request) {
                return request.functionName == functionName;
            });
    if (queued != runtime.pendingStarts.end()) {
        runtime.pendingStarts.erase(queued);
        outError.clear();
        return true;
    }
    if (runtime.activeStartScratchIndex != static_cast<size_t>(-1)) {
        for (size_t index = runtime.activeStartScratchIndex + 1;
                index < runtime.startScratch.size(); ++index) {
            ScriptStartRequest& request = runtime.startScratch[index];
            if (request.functionName == functionName) {
                request.functionName.clear();
                outError.clear();
                return true;
            }
        }
    }
    const auto found = runtime.taskByName.find(functionName);
    if (found == runtime.taskByName.end()) {
        outError = "script not running: " + functionName;
        return false;
    }
    ScriptTask* task = ResolveTask(runtime, found->second);
    if (task == nullptr) {
        outError = "script not running: " + functionName;
        return false;
    }
    task->stopRequested = true;
    task->state = ScriptTaskState::StopRequested;
    outError.clear();
    return true;
}

void ScriptSystemStopAll(
        EngineContext& engine,
        ScriptRuntime& runtime)
{
    (void)engine;
    runtime.pendingStarts.clear();
    if (runtime.activeStartScratchIndex != static_cast<size_t>(-1)) {
        for (size_t index = runtime.activeStartScratchIndex + 1;
                index < runtime.startScratch.size(); ++index) {
            runtime.startScratch[index].functionName.clear();
        }
    }
    for (ScriptTask& task : runtime.tasks) {
        if (!task.occupied) continue;
        task.stopRequested = true;
        task.state = ScriptTaskState::StopRequested;
    }
}

bool ScriptSystemIsFunctionRunning(
        const ScriptRuntime& runtime,
        const std::string& functionName)
{
    return runtime.taskByName.find(functionName) != runtime.taskByName.end()
            || HasQueuedName(runtime, functionName);
}

ScriptOperationHandle ScriptSystemCreateOperation(
        ScriptRuntime& runtime,
        ScriptOperationLaunchStyle launchStyle,
        ScriptTaskHandle ownerTask,
        std::string debugLabel,
        uint64_t backendToken,
        ScriptOperationCancelFn cancelFn)
{
    if (runtime.vm == nullptr
            || (runtime.phase != ScriptRuntimePhase::Loading
                    && runtime.phase != ScriptRuntimePhase::Active)) {
        return {};
    }
    const ScriptOperationHandle handle = AllocateOperation(runtime);
    ScriptOperation* operation = ResolveOperation(runtime, handle);
    operation->state = ScriptOperationState::Pending;
    operation->launchStyle = launchStyle;
    operation->debugLabel = std::move(debugLabel);
    operation->backendToken = backendToken;
    operation->cancelBackend = cancelFn;
    operation->ownerTask = ownerTask;
    if (launchStyle == ScriptOperationLaunchStyle::Blocking) {
        operation->waiterTask = ownerTask;
    }
    return handle;
}

bool ScriptSystemCompleteOperation(
        ScriptRuntime& runtime,
        ScriptOperationHandle handle,
        std::vector<ScriptValue> values)
{
    return TransitionOperation(
            runtime,
            handle,
            ScriptOperationState::Succeeded,
            std::move(values),
            {});
}

bool ScriptSystemFailOperation(
        ScriptRuntime& runtime,
        ScriptOperationHandle handle,
        std::string reason)
{
    return TransitionOperation(
            runtime,
            handle,
            ScriptOperationState::Failed,
            {},
            std::move(reason));
}

bool ScriptSystemCancelOperation(
        EngineContext& engine,
        ScriptRuntime& runtime,
        ScriptOperationHandle handle,
        std::string reason)
{
    ScriptOperation* operation = ResolveOperation(runtime, handle);
    if (operation == nullptr || operation->state != ScriptOperationState::Pending) {
        return false;
    }
    operation->state = ScriptOperationState::Cancelled;
    operation->error = reason.empty() ? "cancelled" : std::move(reason);
    if (!operation->backendCancelSent) {
        operation->backendCancelSent = true;
        const ScriptOperationCancelFn cancel = operation->cancelBackend;
        const uint64_t token = operation->backendToken;
        if (cancel != nullptr) cancel(engine, runtime.hostContext, token);
    }
    return true;
}

void ScriptSystemEnqueueCompletion(
        ScriptRuntime& runtime,
        ScriptCompletionRecord completion)
{
    std::lock_guard<std::mutex> lock(runtime.completionInboxMutex);
    runtime.completionInbox.push_back(std::move(completion));
}

std::vector<ScriptTaskSnapshot> ScriptSystemTaskSnapshot(
        const ScriptRuntime& runtime)
{
    std::vector<ScriptTaskSnapshot> result;
    result.reserve(runtime.taskByName.size());
    for (uint32_t index = 0; index < runtime.tasks.size(); ++index) {
        const ScriptTask& task = runtime.tasks[index];
        if (!task.occupied) continue;
        ScriptTaskSnapshot snapshot;
        snapshot.handle = ScriptTaskHandle{index, task.generation};
        snapshot.functionName = task.functionName;
        snapshot.lane = task.lane;
        snapshot.state = task.state;
        snapshot.waitingOperation = task.waitingOperation;
        snapshot.stopRequested = task.stopRequested;
        snapshot.lastError = task.lastError;
        const ScriptOperation* operation = ResolveOperation(
                runtime, task.waitingOperation);
        if (operation != nullptr) snapshot.operationLabel = operation->debugLabel;
        result.push_back(std::move(snapshot));
    }
    return result;
}

std::vector<ScriptOperationSnapshot> ScriptSystemOperationSnapshot(
        const ScriptRuntime& runtime)
{
    std::vector<ScriptOperationSnapshot> result;
    for (uint32_t index = 0; index < runtime.operations.size(); ++index) {
        const ScriptOperation& operation = runtime.operations[index];
        if (!operation.occupied) continue;
        result.push_back(ScriptOperationSnapshot{
                ScriptOperationHandle{index, operation.generation},
                operation.debugLabel,
                operation.state,
                operation.launchStyle,
                operation.ownerTask,
                operation.waiterTask,
                operation.luaObserverCount,
                operation.error});
    }
    return result;
}

ScriptRuntime& ScriptSystemRuntimeFromLua(lua_State* state)
{
    return *GetLuaContext(state).scripts;
}

EngineContext& ScriptSystemEngineFromLua(lua_State* state)
{
    LuaEngineContext& context = GetLuaContext(state);
    if (context.engine == nullptr) luaL_error(state, "engine context is unavailable");
    return *context.engine;
}

void* ScriptSystemHostContextFromLua(lua_State* state)
{
    return GetLuaContext(state).hostContext;
}

ScriptTaskHandle ScriptSystemCurrentTaskFromLua(lua_State* state)
{
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    if (runtime.phase == ScriptRuntimePhase::ShuttingDown) {
        luaL_error(state, "blocking Lua operations are not allowed during shutdown()");
    }
    if (runtime.phase != ScriptRuntimePhase::Loading
            && runtime.phase != ScriptRuntimePhase::Active) {
        luaL_error(state, "blocking Lua operations require an active map runtime");
    }
    const auto found = runtime.taskByThread.find(state);
    if (found == runtime.taskByThread.end()
            || ResolveTask(runtime, found->second) == nullptr) {
        luaL_error(state, "blocking operation can only run inside init(), a foreground hook, or startScript() task");
    }
    return found->second;
}

ScriptTaskHandle ScriptSystemTryCurrentTaskFromLua(lua_State* state)
{
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    const auto found = runtime.taskByThread.find(state);
    return found != runtime.taskByThread.end()
                    && ResolveTask(runtime, found->second) != nullptr
            ? found->second : ScriptTaskHandle{};
}

int ScriptSystemYieldForOperation(
        lua_State* state,
        ScriptOperationHandle operationHandle,
        int originalStackTop)
{
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    const ScriptTaskHandle taskHandle = ScriptSystemCurrentTaskFromLua(state);
    ScriptTask* task = ResolveTask(runtime, taskHandle);
    ScriptOperation* operation = ResolveOperation(runtime, operationHandle);
    if (task == nullptr || operation == nullptr
            || operation->state != ScriptOperationState::Pending) {
        return luaL_error(state, "cannot wait on an invalid or completed operation");
    }
    if (IsValid(task->waitingOperation)
            && !(task->waitingOperation == operationHandle)) {
        return luaL_error(state, "task is already waiting on another operation");
    }
    if (IsValid(operation->waiterTask)
            && !(operation->waiterTask == taskHandle)) {
        return luaL_error(state, "operation already has a waiter");
    }
    operation->waiterTask = taskHandle;
    task->waitingOperation = operationHandle;
    task->state = ScriptTaskState::Waiting;
    return lua_yieldk(
            state,
            0,
            static_cast<lua_KContext>(originalStackTop),
            FinishOperationWait);
}

void ScriptSystemPushOperationUserdata(
        lua_State* state,
        ScriptOperationHandle operationHandle)
{
    ScriptRuntime& runtime = ScriptSystemRuntimeFromLua(state);
    ScriptOperation* operation = ResolveOperation(runtime, operationHandle);
    if (operation == nullptr) {
        lua_pushnil(state);
        return;
    }
    auto* userdata = static_cast<LuaScriptOperationHandle*>(
            lua_newuserdatauv(state, sizeof(LuaScriptOperationHandle), 0));
    userdata->handle = operationHandle;
    ++operation->luaObserverCount;
    luaL_getmetatable(state, OperationMetatable);
    lua_setmetatable(state, -2);
}

} // namespace engine
