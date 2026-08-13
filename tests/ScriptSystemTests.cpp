#include "engine/EngineContext.h"
#include "engine/scripting/ScriptPersistence.h"
#include "engine/scripting/ScriptConsole.h"
#include "engine/scripting/ScriptSystem.h"

#include "lua.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

struct TestFiles {
    std::filesystem::path root;
    std::filesystem::path mapPath;
    std::filesystem::path scriptPath;

    explicit TestFiles(const char* name)
    {
        const auto unique = std::chrono::steady_clock::now()
                .time_since_epoch().count();
        root = std::filesystem::temp_directory_path()
                / (std::string{"engine_lua_"} + name + "_"
                        + std::to_string(unique));
        const std::filesystem::path mapDirectory = root / "levels" / name;
        std::filesystem::create_directories(mapDirectory);
        std::filesystem::create_directories(root / "scripts");
        mapPath = mapDirectory / (std::string{name} + ".json");
        scriptPath = mapDirectory / (std::string{name} + ".lua");
    }

    ~TestFiles()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    void Write(const std::filesystem::path& path, const std::string& text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary);
        output << text;
        assert(output.good());
    }
};

bool CreateRuntime(
        engine::EngineContext& context,
        engine::ScriptRuntime& runtime,
        engine::PersistentScriptStore& persistent,
        const TestFiles& files,
        void* host = nullptr,
        engine::ScriptBindingRegisterFn bindings = nullptr,
        bool loadingSave = false)
{
    std::string error;
    const bool created = engine::ScriptSystemCreateForMap(
            context,
            runtime,
            persistent,
            files.mapPath.stem().string(),
            files.mapPath.string(),
            files.root.string(),
            host,
            bindings,
            loadingSave,
            error);
    if (!created) assert(!error.empty());
    return created;
}

void MissingAndBrokenScriptsFollowLifecyclePolicy()
{
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    TestFiles files("lifecycle");

    assert(CreateRuntime(context, runtime, persistent, files));
    assert(runtime.vm != nullptr);
    assert(runtime.initFinished);
    engine::ScriptSystemShutdownForMap(context, runtime);
    assert(runtime.vm == nullptr);

    files.Write(files.scriptPath, "delay(1)\n");
    assert(!CreateRuntime(context, runtime, persistent, files));
    assert(runtime.vm == nullptr);

    files.Write(files.scriptPath, "function init(\n");
    assert(!CreateRuntime(context, runtime, persistent, files));
    assert(runtime.vm == nullptr);
}

void YieldedInitPersistenceAndShutdownWork()
{
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    TestFiles files("yielded_init");
    files.Write(files.scriptPath, R"(
function init()
    setPersistentBool("loading_before", isLoadingSave())
    local ok = delay(0)
    assert(ok)
    setPersistentBool("loading_during_resume", isLoadingSave())
    setPersistentInt("runs", getPersistentInt("runs") + 1)
end
function shutdown()
    setPersistentBool("shutdown", true)
end
)");
    assert(CreateRuntime(context, runtime, persistent, files, nullptr, nullptr, true));
    assert(!runtime.initFinished);
    assert(runtime.loadingSave);
    const engine::ScriptConsoleResult loadingConsole =
            engine::ScriptSystemExecuteConsole(runtime, "1");
    assert(!loadingConsole.success);
    assert(loadingConsole.error.find("still loading") != std::string::npos);
    assert(persistent.bools.at("loading_before"));
    engine::ScriptSystemUpdate(context, runtime, 0.0f);
    assert(runtime.initFinished);
    assert(!runtime.loadingSave);
    assert(persistent.bools.at("loading_during_resume"));
    assert(persistent.ints.at("runs") == 1);
    engine::ScriptSystemShutdownForMap(context, runtime);
    engine::ScriptSystemShutdownForMap(context, runtime);
    assert(persistent.bools.at("shutdown"));
}

void BackgroundStartsAreDeferredAndForegroundIsSerialized()
{
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    TestFiles files("tasks");
    files.Write(files.scriptPath, R"(
function starter()
    local ok = startScript("worker")
    assert(ok)
    setPersistentInt("stage", 1)
end
function worker()
    setPersistentInt("stage", 2)
    delay(0)
    setPersistentInt("stage", 3)
end
function foregroundWait()
    delay(10)
end
function bareYield()
    coroutine.yield()
end
function firstQueued()
    local ok, reason = startScript("secondQueued")
    setPersistentBool("snapshot_duplicate_rejected", not ok)
    setPersistentString("snapshot_duplicate_reason", reason)
end
function secondQueued()
    setPersistentBool("second_ran", true)
end
)");
    assert(CreateRuntime(context, runtime, persistent, files));
    const engine::ScriptCallOutcome starter =
            engine::ScriptSystemCallForegroundHook(runtime, "starter");
    assert(starter.result == engine::ScriptCallResult::Completed);
    assert(persistent.ints.at("stage") == 1);
    assert(engine::ScriptSystemIsFunctionRunning(runtime, "worker"));
    engine::ScriptSystemUpdate(context, runtime, 0.0f);
    assert(persistent.ints.at("stage") == 2);
    engine::ScriptSystemUpdate(context, runtime, 0.0f);
    assert(persistent.ints.at("stage") == 3);

    const engine::ScriptCallOutcome waiting =
            engine::ScriptSystemCallForegroundHook(runtime, "foregroundWait");
    assert(waiting.result == engine::ScriptCallResult::Started);
    const engine::ScriptCallOutcome busy =
            engine::ScriptSystemCallForegroundHook(runtime, "starter");
    assert(busy.result == engine::ScriptCallResult::ForegroundBusy);
    std::string stopError;
    assert(engine::ScriptSystemStopFunction(
            context, runtime, "foregroundWait", stopError));
    engine::ScriptSystemUpdate(context, runtime, 0.0f);
    assert(!engine::ScriptSystemIsFunctionRunning(runtime, "foregroundWait"));
    assert(engine::ScriptSystemOperationSnapshot(runtime).empty());

    const engine::ScriptCallOutcome bare =
            engine::ScriptSystemCallForegroundHook(runtime, "bareYield");
    assert(bare.result == engine::ScriptCallResult::Error);
    assert(bare.error.find("bare coroutine.yield") != std::string::npos);

    std::string queueError;
    assert(engine::ScriptSystemQueueBackground(
            runtime, "firstQueued", queueError));
    assert(engine::ScriptSystemQueueBackground(
            runtime, "secondQueued", queueError));
    engine::ScriptSystemUpdate(context, runtime, 0.0f);
    assert(persistent.bools.at("snapshot_duplicate_rejected"));
    assert(persistent.bools.at("second_ran"));
    engine::ScriptSystemShutdownForMap(context, runtime);
}

struct FakeOperationHost {
    engine::ScriptRuntime* runtime = nullptr;
    engine::ScriptOperationHandle operation{};
    int cancelCount = 0;
};

void CancelFake(engine::EngineContext&, void* rawHost, uint64_t)
{
    ++static_cast<FakeOperationHost*>(rawHost)->cancelCount;
}

int LuaStartFake(lua_State* state)
{
    auto& runtime = engine::ScriptSystemRuntimeFromLua(state);
    auto* host = static_cast<FakeOperationHost*>(
            engine::ScriptSystemHostContextFromLua(state));
    host->operation = engine::ScriptSystemCreateOperation(
            runtime,
            engine::ScriptOperationLaunchStyle::Async,
            engine::ScriptSystemTryCurrentTaskFromLua(state),
            "fake",
            7,
            CancelFake);
    engine::ScriptSystemPushOperationUserdata(state, host->operation);
    return 1;
}

void RegisterFakeBindings(lua_State* state)
{
    lua_pushcfunction(state, LuaStartFake);
    lua_setglobal(state, "startFake");
}

void AsyncOperationsDeliverValuesAndCancelOnce()
{
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    FakeOperationHost host{&runtime};
    TestFiles files("operations");
    files.Write(files.scriptPath, R"(
function init()
    local operation = startFake()
    local ok, number, text = await(operation)
    setPersistentBool("ok", ok)
    setPersistentInt("number", number or -1)
    setPersistentString("text", text or "")
end
)");
    assert(CreateRuntime(
            context, runtime, persistent, files, &host, RegisterFakeBindings));
    assert(!runtime.initFinished);
    assert(engine::ScriptSystemCompleteOperation(
            runtime,
            host.operation,
            {int64_t{23}, std::string{"done"}}));
    assert(!engine::ScriptSystemCompleteOperation(runtime, host.operation));
    engine::ScriptSystemUpdate(context, runtime, 0.0f);
    assert(persistent.bools.at("ok"));
    assert(persistent.ints.at("number") == 23);
    assert(persistent.strings.at("text") == "done");
    const engine::ScriptOperationHandle staleOperation = host.operation;
    engine::ScriptSystemShutdownForMap(context, runtime);
    assert(!engine::ScriptSystemCompleteOperation(runtime, staleOperation));

    files.Write(files.scriptPath, R"(
function init()
    local operation = startFake()
    assert(cancelOperation(operation))
    local ok, reason = await(operation)
    setPersistentBool("cancel_ok", not ok)
    setPersistentString("cancel_reason", reason)
end
)");
    assert(CreateRuntime(
            context, runtime, persistent, files, &host, RegisterFakeBindings));
    assert(runtime.initFinished);
    assert(host.cancelCount == 1);
    assert(persistent.bools.at("cancel_ok"));
    assert(persistent.strings.at("cancel_reason") == "cancelled");
    assert(!engine::ScriptSystemCancelOperation(
            context, runtime, host.operation));
    assert(host.cancelCount == 1);
    engine::ScriptSystemShutdownForMap(context, runtime);
}

void ModulePathsAreDeterministic()
{
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    TestFiles files("modules");
    files.Write(files.root / "scripts" / "choice.lua", "return 'shared'\n");
    files.Write(files.scriptPath.parent_path() / "choice.lua", "return 'local'\n");
    files.Write(files.scriptPath, R"(
local choice = require("choice")
function init()
    setPersistentString("choice", choice)
end
)");
    assert(CreateRuntime(context, runtime, persistent, files));
    assert(persistent.strings.at("choice") == "local");
    lua_getglobal(runtime.vm, "package");
    lua_getfield(runtime.vm, -1, "cpath");
    assert(std::string{lua_tostring(runtime.vm, -1)}.empty());
    lua_settop(runtime.vm, 0);
    engine::ScriptSystemShutdownForMap(context, runtime);
}

void PersistentCodecIsTransactional()
{
    engine::PersistentScriptStore source;
    source.bools["flag"] = true;
    source.ints["count"] = 42;
    source.strings["name"] = "refinery";
    std::string json;
    std::string error;
    assert(engine::SavePersistentScriptStoreToJsonString(source, json, error));

    engine::PersistentScriptStore loaded;
    assert(engine::LoadPersistentScriptStoreFromJsonString(json, loaded, error));
    assert(loaded.bools.at("flag"));
    assert(loaded.ints.at("count") == 42);
    assert(loaded.strings.at("name") == "refinery");

    loaded.ints["sentinel"] = 9;
    assert(!engine::LoadPersistentScriptStoreFromJsonString(
            R"({"bools":{},"ints":{"bad":"value"},"strings":{}})",
            loaded,
            error));
    assert(loaded.ints.at("sentinel") == 9);
}

void ConsoleEvaluationIsSynchronousBoundedAndStackSafe()
{
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    TestFiles files("console");
    files.Write(files.scriptPath, R"(
function consoleWorker()
    setPersistentBool("console_worker", true)
end
)");
    assert(CreateRuntime(context, runtime, persistent, files));

    lua_pushinteger(runtime.vm, 91);
    const int baseTop = lua_gettop(runtime.vm);
    engine::ScriptConsoleResult expression =
            engine::ScriptSystemExecuteConsole(
                    runtime, "1 + 2, nil, true, 'text'");
    assert(expression.success);
    assert(expression.evaluatedExpression);
    assert(expression.values.size() == 4);
    assert(expression.values[0] == "[1] 3");
    assert(expression.values[1] == "[2] nil");
    assert(expression.values[2] == "[3] true");
    assert(expression.values[3] == "[4] \"text\"");
    assert(lua_gettop(runtime.vm) == baseTop);
    assert(lua_tointeger(runtime.vm, -1) == 91);

    engine::ScriptConsoleResult statement =
            engine::ScriptSystemExecuteConsole(runtime, "consoleValue = 17");
    assert(statement.success);
    assert(!statement.evaluatedExpression);
    assert(statement.values.empty());
    engine::ScriptConsoleResult read =
            engine::ScriptSystemExecuteConsole(runtime, "consoleValue");
    assert(read.success && read.values.size() == 1
            && read.values[0] == "[1] 17");

    engine::ScriptConsoleResult explicitReturn =
            engine::ScriptSystemExecuteConsole(runtime, "return 23, 'ok'");
    assert(explicitReturn.success);
    assert(!explicitReturn.evaluatedExpression);
    assert(explicitReturn.values.size() == 2);

    engine::ScriptConsoleResult runtimeError =
            engine::ScriptSystemExecuteConsole(runtime, "error('console boom')");
    assert(!runtimeError.success);
    assert(runtimeError.error.find("console boom") != std::string::npos);
    assert(runtimeError.error.find("stack traceback") != std::string::npos);
    assert(lua_gettop(runtime.vm) == baseTop);

    engine::ScriptConsoleResult blocking =
            engine::ScriptSystemExecuteConsole(runtime, "delay(1)");
    assert(!blocking.success);
    assert(blocking.error.find("blocking operation") != std::string::npos);

    engine::ScriptConsoleResult printed =
            engine::ScriptSystemExecuteConsole(runtime, "print('console print')");
    assert(printed.success);
    assert(printed.values.empty());

    std::string manyValues;
    for (int i = 0; i < 70; ++i) {
        if (!manyValues.empty()) manyValues += ',';
        manyValues += std::to_string(i);
    }
    engine::ScriptConsoleResult bounded =
            engine::ScriptSystemExecuteConsole(runtime, manyValues);
    assert(bounded.success);
    assert(bounded.values.size() == 65);
    assert(bounded.values.back().find("omitted") != std::string::npos);

    runtime.consoleExecuting = true;
    engine::ScriptConsoleResult reentrant =
            engine::ScriptSystemExecuteConsole(runtime, "1");
    assert(!reentrant.success);
    assert(reentrant.error.find("already executing") != std::string::npos);
    runtime.consoleExecuting = false;

    lua_settop(runtime.vm, 0);
    engine::ScriptSystemShutdownForMap(context, runtime);
    engine::ScriptConsoleResult unavailable =
            engine::ScriptSystemExecuteConsole(runtime, "1");
    assert(!unavailable.success);
    assert(unavailable.error.find("no map VM") != std::string::npos);
}

} // namespace

int main()
{
    MissingAndBrokenScriptsFollowLifecyclePolicy();
    YieldedInitPersistenceAndShutdownWork();
    BackgroundStartsAreDeferredAndForegroundIsSerialized();
    AsyncOperationsDeliverValuesAndCancelOnce();
    ModulePathsAreDeterministic();
    PersistentCodecIsTransactional();
    ConsoleEvaluationIsSynchronousBoundedAndStackSafe();
    return 0;
}
