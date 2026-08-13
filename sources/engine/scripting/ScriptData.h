#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct lua_State;

namespace engine {

struct EngineContext;
struct ScriptRuntime;

struct ScriptTaskHandle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};

struct ScriptOperationHandle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};

inline bool IsValid(ScriptTaskHandle handle)
{
    return handle.index != UINT32_MAX;
}

inline bool IsValid(ScriptOperationHandle handle)
{
    return handle.index != UINT32_MAX;
}

inline bool operator==(ScriptTaskHandle a, ScriptTaskHandle b)
{
    return a.index == b.index && a.generation == b.generation;
}

inline bool operator==(ScriptOperationHandle a, ScriptOperationHandle b)
{
    return a.index == b.index && a.generation == b.generation;
}

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
    Foreground,
    Background
};

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

enum class ScriptRuntimePhase {
    Empty,
    Loading,
    Active,
    ShuttingDown
};

enum class ScriptCallResult {
    Missing,
    Completed,
    Started,
    AlreadyRunning,
    ForegroundBusy,
    Error
};

using ScriptValue = std::variant<std::monostate, bool, int64_t, double, std::string>;

using ScriptOperationCancelFn = void (*)(
        EngineContext& engine,
        void* hostContext,
        uint64_t backendToken);

struct ScriptTask {
    bool occupied = false;
    uint32_t generation = 0;
    ScriptTaskState state = ScriptTaskState::Free;
    ScriptLaunchLane lane = ScriptLaunchLane::Background;
    std::string functionName;
    lua_State* thread = nullptr;
    int threadRegistryRef = -2;
    ScriptOperationHandle waitingOperation{};
    bool stopRequested = false;
    bool lifecycleInitTask = false;
    std::string lastError;
};

struct ScriptOperation {
    bool occupied = false;
    uint32_t generation = 0;
    ScriptOperationState state = ScriptOperationState::Free;
    ScriptOperationLaunchStyle launchStyle = ScriptOperationLaunchStyle::Blocking;
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

struct ScriptTimer {
    uint64_t token = 0;
    ScriptOperationHandle operation{};
    double remainingSeconds = 0.0;
};

struct PersistentScriptStore {
    std::unordered_map<std::string, bool> bools;
    std::unordered_map<std::string, int64_t> ints;
    std::unordered_map<std::string, std::string> strings;
};

struct LuaEngineContext {
    ScriptRuntime* scripts = nullptr;
    EngineContext* engine = nullptr;
    PersistentScriptStore* persistent = nullptr;
    void* hostContext = nullptr;
};

struct ScriptRuntime {
    lua_State* vm = nullptr;
    EngineContext* engine = nullptr;
    PersistentScriptStore* persistent = nullptr;
    void* hostContext = nullptr;
    LuaEngineContext luaContext;
    ScriptRuntimePhase phase = ScriptRuntimePhase::Empty;

    std::string mapId;
    std::string mapScriptPath;
    bool mapChunkPresent = false;
    bool initAttempted = false;
    bool initFinished = false;
    bool shutdownAttempted = false;
    bool loadingSave = false;

    std::vector<ScriptTask> tasks;
    std::vector<uint32_t> freeTaskSlots;
    std::vector<ScriptOperation> operations;
    std::vector<uint32_t> freeOperationSlots;
    std::vector<ScriptTimer> timers;
    uint64_t nextTimerToken = 1;

    std::vector<ScriptStartRequest> pendingStarts;
    std::vector<ScriptCompletionRecord> pendingCompletions;
    std::mutex completionInboxMutex;
    std::vector<ScriptCompletionRecord> completionInbox;

    std::unordered_map<lua_State*, ScriptTaskHandle> taskByThread;
    std::unordered_map<std::string, ScriptTaskHandle> taskByName;
    std::vector<ScriptTaskHandle> taskScratch;
    std::vector<ScriptStartRequest> startScratch;
    size_t activeStartScratchIndex = static_cast<size_t>(-1);
    std::vector<ScriptCompletionRecord> completionScratch;

    bool mapChangeRequested = false;
    std::string requestedMapId;
    std::string requestedSpawnId;
    bool mapAbortRequested = false;
    std::string mapAbortError;
};

struct ScriptCallOutcome {
    ScriptCallResult result = ScriptCallResult::Error;
    ScriptTaskHandle task{};
    std::vector<ScriptValue> immediateValues;
    std::string error;
};

struct ScriptTaskSnapshot {
    ScriptTaskHandle handle{};
    std::string functionName;
    ScriptLaunchLane lane = ScriptLaunchLane::Background;
    ScriptTaskState state = ScriptTaskState::Free;
    ScriptOperationHandle waitingOperation{};
    std::string operationLabel;
    bool stopRequested = false;
    std::string lastError;
};

struct ScriptOperationSnapshot {
    ScriptOperationHandle handle{};
    std::string debugLabel;
    ScriptOperationState state = ScriptOperationState::Free;
    ScriptOperationLaunchStyle launchStyle = ScriptOperationLaunchStyle::Blocking;
    ScriptTaskHandle ownerTask{};
    ScriptTaskHandle waiterTask{};
    uint32_t observerCount = 0;
    std::string error;
};

} // namespace engine
