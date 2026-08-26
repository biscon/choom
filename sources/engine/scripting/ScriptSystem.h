#pragma once

#include "engine/scripting/ScriptData.h"

#include <cstddef>
#include <string>
#include <vector>

struct lua_State;

namespace engine {

struct EngineContext;

using ScriptBindingRegisterFn = void (*)(lua_State* state);

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
        std::string& error);

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

// Starts a foreground hook whose eventual result can be collected after a
// yield. Immediate completions are still returned directly in ScriptCallOutcome.
ScriptCallOutcome ScriptSystemCallObservedForegroundHook(
        ScriptRuntime& runtime,
        const std::string& functionName);

ScriptCallOutcome ScriptSystemCallObservedForegroundHook(
        ScriptRuntime& runtime,
        const std::string& functionName,
        const ScriptValue* arguments,
        std::size_t argumentCount);

bool ScriptSystemTakeObservedCallOutcome(
        ScriptRuntime& runtime,
        ScriptTaskHandle task,
        ScriptObservedCallOutcome& outOutcome);

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

ScriptOperationHandle ScriptSystemCreateOperation(
        ScriptRuntime& runtime,
        ScriptOperationLaunchStyle launchStyle,
        ScriptTaskHandle ownerTask,
        std::string debugLabel,
        uint64_t backendToken,
        ScriptOperationCancelFn cancelFn);

bool ScriptSystemCompleteOperation(
        ScriptRuntime& runtime,
        ScriptOperationHandle handle,
        std::vector<ScriptValue> values = {});

bool ScriptSystemFailOperation(
        ScriptRuntime& runtime,
        ScriptOperationHandle handle,
        std::string reason);

bool ScriptSystemCancelOperation(
        EngineContext& engine,
        ScriptRuntime& runtime,
        ScriptOperationHandle handle,
        std::string reason = "cancelled");

void ScriptSystemEnqueueCompletion(
        ScriptRuntime& runtime,
        ScriptCompletionRecord completion);

std::vector<ScriptTaskSnapshot> ScriptSystemTaskSnapshot(
        const ScriptRuntime& runtime);

std::vector<ScriptOperationSnapshot> ScriptSystemOperationSnapshot(
        const ScriptRuntime& runtime);

// Host-binding helpers. They validate that the Lua state belongs to the active
// map runtime and never expose pointers to gameplay containers.
ScriptRuntime& ScriptSystemRuntimeFromLua(lua_State* state);
EngineContext& ScriptSystemEngineFromLua(lua_State* state);
void* ScriptSystemHostContextFromLua(lua_State* state);
ScriptTaskHandle ScriptSystemCurrentTaskFromLua(lua_State* state);
ScriptTaskHandle ScriptSystemTryCurrentTaskFromLua(lua_State* state);
int ScriptSystemYieldForOperation(
        lua_State* state,
        ScriptOperationHandle operation,
        int originalStackTop);
void ScriptSystemPushOperationUserdata(
        lua_State* state,
        ScriptOperationHandle operation);

} // namespace engine
