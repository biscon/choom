#include "game/SectorScriptBindings.h"

#include "engine/EngineContext.h"
#include "engine/scripting/ScriptSystem.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTriggers.h"

#include "lua.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace game {
namespace {

constexpr float DoorTargetEpsilon = 0.0001f;

bool IsValidMapId(const std::string& name)
{
    if (name.empty()) return false;
    for (const char character : name) {
        const bool letter = (character >= 'A' && character <= 'Z')
                || (character >= 'a' && character <= 'z');
        const bool digit = character >= '0' && character <= '9';
        if (!letter && !digit && character != '_' && character != '-') {
            return false;
        }
    }
    return true;
}

struct BeginDoorMoveResult {
    bool started = false;
    bool completedImmediately = false;
    uint64_t token = 0;
    std::string error;
};

SectorScriptHost& HostFromLua(lua_State* state)
{
    auto* host = static_cast<SectorScriptHost*>(
            engine::ScriptSystemHostContextFromLua(state));
    if (host == nullptr || host->runtimeObjects == nullptr || host->scripts == nullptr) {
        luaL_error(state, "sector script host is unavailable");
    }
    return *host;
}

engine::Entity FindPlacedObjectEntity(
        const SectorRuntimeObjectState& objects,
        const engine::World& world,
        int placedObjectId)
{
    const auto found = std::find_if(
            objects.placedObjectEntities.begin(),
            objects.placedObjectEntities.end(),
            [placedObjectId](const SectorPlacedRuntimeObjectEntity& entry) {
                return entry.placedObjectId == placedObjectId;
            });
    return found != objects.placedObjectEntities.end()
                    && world.IsAlive(found->entity)
            ? found->entity : engine::NullEntity();
}

SectorScriptDoorMove* FindDoorMove(SectorScriptHost& host, uint64_t token)
{
    const auto found = std::find_if(
            host.doorMoves.begin(),
            host.doorMoves.end(),
            [token](const SectorScriptDoorMove& move) {
                return move.active && move.token == token;
            });
    return found != host.doorMoves.end() ? &*found : nullptr;
}

bool DoorMoveAlreadyActive(const SectorScriptHost& host, int placedObjectId)
{
    return std::any_of(
            host.doorMoves.begin(),
            host.doorMoves.end(),
            [placedObjectId](const SectorScriptDoorMove& move) {
                return move.active && move.placedObjectId == placedObjectId;
            });
}

void RestoreDoorSpeed(
        engine::World& world,
        SectorScriptDoorMove& move)
{
    if (world.IsAlive(move.entity)
            && world.Has<SectorDoorMotion>(move.entity)) {
        world.Get<SectorDoorMotion>(move.entity).travelSpeed = move.savedTravelSpeed;
    }
}

void RefreshDoorSpatialCaches(
        engine::World& world,
        SectorRuntimeObjectState& objects)
{
    UpdateSectorDoorDerivedStateSystem(world);
    objects.dynamicDoorColliders.clear();
    CollectSectorDoorDynamicColliders(world, objects.dynamicDoorColliders);
    objects.dynamicPortalBlockers.clear();
    CollectSectorDoorDynamicPortalBlockers(world, objects.dynamicPortalBlockers);
    objects.doorSpatialStateChanged = true;
    objects.doorCollisionCacheInitialized = true;
}

void CancelDoorMove(
        engine::EngineContext& context,
        void* hostContext,
        uint64_t token)
{
    auto* host = static_cast<SectorScriptHost*>(hostContext);
    if (host == nullptr || host->runtimeObjects == nullptr) return;
    SectorScriptDoorMove* move = FindDoorMove(*host, token);
    if (move == nullptr) return;
    if (context.world.IsAlive(move->entity)
            && context.world.Has<SectorDoorMotion>(move->entity)) {
        SectorDoorMotion& motion = context.world.Get<SectorDoorMotion>(move->entity);
        motion.openFraction = std::clamp(motion.openFraction, 0.0f, 1.0f);
        motion.targetOpenFraction = motion.openFraction;
        motion.travelSpeed = move->savedTravelSpeed;
        RefreshDoorSpatialCaches(context.world, *host->runtimeObjects);
    }
    move->active = false;
}

BeginDoorMoveResult BeginDoorMove(
        engine::EngineContext& context,
        SectorScriptHost& host,
        int placedObjectId,
        float targetFraction,
        float durationMs)
{
    BeginDoorMoveResult result;
    if (DoorMoveAlreadyActive(host, placedObjectId)) {
        result.error = "door already has a scripted move: "
                + std::to_string(placedObjectId);
        return result;
    }
    const engine::Entity entity = FindPlacedObjectEntity(
            *host.runtimeObjects, context.world, placedObjectId);
    if (engine::IsNull(entity)
            || !context.world.Has<SectorDoor>(entity)
            || !context.world.Has<SectorDoorMotion>(entity)) {
        result.error = "door not found: " + std::to_string(placedObjectId);
        return result;
    }
    SectorDoor& door = context.world.Get<SectorDoor>(entity);
    SectorDoorMotion& motion = context.world.Get<SectorDoorMotion>(entity);
    if (!door.enabled) {
        result.error = "door is disabled: " + std::to_string(placedObjectId);
        return result;
    }
    const float current = std::isfinite(motion.openFraction)
            ? std::clamp(motion.openFraction, 0.0f, 1.0f) : 0.0f;
    if (std::fabs(current - targetFraction) <= DoorTargetEpsilon) {
        motion.openFraction = targetFraction;
        motion.targetOpenFraction = targetFraction;
        result.started = true;
        result.completedImmediately = true;
        return result;
    }
    if (durationMs == 0.0f) {
        motion.openFraction = targetFraction;
        motion.targetOpenFraction = targetFraction;
        RefreshDoorSpatialCaches(context.world, *host.runtimeObjects);
        result.started = true;
        result.completedImmediately = true;
        return result;
    }

    const float durationSeconds = durationMs / 1000.0f;
    const float distance = motion.travelAmount > 0.0f
                    && std::isfinite(motion.travelAmount)
            ? motion.travelAmount : 1.0f;
    const float scriptedSpeed = distance
            * std::fabs(targetFraction - current) / durationSeconds;
    if (!std::isfinite(scriptedSpeed) || scriptedSpeed <= 0.0f) {
        result.error = "could not derive a valid door speed";
        return result;
    }

    const uint64_t token = host.nextDoorMoveToken++;
    if (host.doorMoves.size() == host.doorMoves.capacity()) {
        std::fprintf(
                stderr,
                "[Lua WARNING] scripted door-move capacity exceeded; runtime allocation may occur\n");
    }
    host.doorMoves.push_back(SectorScriptDoorMove{
            token,
            placedObjectId,
            entity,
            {},
            targetFraction,
            motion.travelSpeed,
            true});
    motion.openFraction = current;
    motion.targetOpenFraction = targetFraction;
    motion.travelSpeed = scriptedSpeed;
    result.started = true;
    result.token = token;
    return result;
}

void BindDoorOperation(
        SectorScriptHost& host,
        uint64_t token,
        engine::ScriptOperationHandle operation)
{
    SectorScriptDoorMove* move = FindDoorMove(host, token);
    if (move != nullptr) move->operation = operation;
}

bool ParseDoorMove(
        lua_State* state,
        int& placedObjectId,
        float& targetFraction,
        float& durationMs)
{
    const lua_Integer rawId = luaL_checkinteger(state, 1);
    const lua_Number rawTarget = luaL_checknumber(state, 2);
    const lua_Number rawDuration = luaL_checknumber(state, 3);
    if (rawId <= 0 || rawId > std::numeric_limits<int>::max()) {
        luaL_argerror(state, 1, "door ID must be a positive integer");
    }
    if (!std::isfinite(static_cast<double>(rawTarget))
            || rawTarget < 0.0 || rawTarget > 1.0) {
        luaL_argerror(state, 2, "target fraction must be between 0 and 1");
    }
    if (!std::isfinite(static_cast<double>(rawDuration)) || rawDuration < 0.0) {
        luaL_argerror(state, 3, "duration must be finite and non-negative");
    }
    placedObjectId = static_cast<int>(rawId);
    targetFraction = static_cast<float>(rawTarget);
    durationMs = static_cast<float>(rawDuration);
    return true;
}

int LuaMoveDoor(lua_State* state)
{
    const int originalTop = lua_gettop(state);
    int placedObjectId = 0;
    float targetFraction = 0.0f;
    float durationMs = 0.0f;
    ParseDoorMove(state, placedObjectId, targetFraction, durationMs);
    engine::ScriptRuntime& scripts = engine::ScriptSystemRuntimeFromLua(state);
    const engine::ScriptTaskHandle task = engine::ScriptSystemCurrentTaskFromLua(state);
    engine::EngineContext& context = engine::ScriptSystemEngineFromLua(state);
    SectorScriptHost& host = HostFromLua(state);
    const BeginDoorMoveResult begin = BeginDoorMove(
            context, host, placedObjectId, targetFraction, durationMs);
    if (!begin.started) {
        lua_pushboolean(state, 0);
        lua_pushlstring(state, begin.error.data(), begin.error.size());
        return 2;
    }
    if (begin.completedImmediately) {
        lua_pushboolean(state, 1);
        return 1;
    }
    const engine::ScriptOperationHandle operation =
            engine::ScriptSystemCreateOperation(
                    scripts,
                    engine::ScriptOperationLaunchStyle::Blocking,
                    task,
                    "moveDoor:" + std::to_string(placedObjectId),
                    begin.token,
                    CancelDoorMove);
    BindDoorOperation(host, begin.token, operation);
    return engine::ScriptSystemYieldForOperation(
            state, operation, originalTop);
}

int LuaStartMoveDoor(lua_State* state)
{
    int placedObjectId = 0;
    float targetFraction = 0.0f;
    float durationMs = 0.0f;
    ParseDoorMove(state, placedObjectId, targetFraction, durationMs);
    engine::ScriptRuntime& scripts = engine::ScriptSystemRuntimeFromLua(state);
    if (scripts.phase != engine::ScriptRuntimePhase::Loading
            && scripts.phase != engine::ScriptRuntimePhase::Active) {
        lua_pushnil(state);
        lua_pushliteral(state, "script runtime is shutting down");
        return 2;
    }
    engine::EngineContext& context = engine::ScriptSystemEngineFromLua(state);
    SectorScriptHost& host = HostFromLua(state);
    const BeginDoorMoveResult begin = BeginDoorMove(
            context, host, placedObjectId, targetFraction, durationMs);
    if (!begin.started) {
        lua_pushnil(state);
        lua_pushlstring(state, begin.error.data(), begin.error.size());
        return 2;
    }
    const engine::ScriptOperationHandle operation =
            engine::ScriptSystemCreateOperation(
                    scripts,
                    engine::ScriptOperationLaunchStyle::Async,
                    engine::ScriptSystemTryCurrentTaskFromLua(state),
                    "moveDoor:" + std::to_string(placedObjectId),
                    begin.token,
                    CancelDoorMove);
    if (begin.completedImmediately) {
        engine::ScriptSystemCompleteOperation(scripts, operation);
    } else {
        BindDoorOperation(host, begin.token, operation);
    }
    engine::ScriptSystemPushOperationUserdata(state, operation);
    return 1;
}

int LuaChangeMap(lua_State* state)
{
    size_t mapLength = 0;
    const char* rawMap = luaL_checklstring(state, 1, &mapLength);
    const std::string mapId{rawMap, mapLength};
    std::string spawnId;
    if (!lua_isnoneornil(state, 2)) {
        size_t spawnLength = 0;
        const char* rawSpawn = luaL_checklstring(state, 2, &spawnLength);
        spawnId.assign(rawSpawn, spawnLength);
        if (spawnId.empty()) {
            lua_pushboolean(state, 0);
            lua_pushliteral(state, "spawn ID must not be empty");
            return 2;
        }
    }
    if (!IsValidMapId(mapId)) {
        lua_pushboolean(state, 0);
        lua_pushliteral(state, "invalid map ID");
        return 2;
    }
    engine::ScriptRuntime& runtime = engine::ScriptSystemRuntimeFromLua(state);
    if (runtime.phase != engine::ScriptRuntimePhase::Active
            && runtime.phase != engine::ScriptRuntimePhase::Loading) {
        lua_pushboolean(state, 0);
        lua_pushliteral(state, "script runtime is shutting down");
        return 2;
    }
    if (runtime.mapChangeRequested) {
        lua_pushboolean(state, 0);
        lua_pushliteral(state, "map change already requested");
        return 2;
    }
    runtime.mapChangeRequested = true;
    runtime.requestedMapId = mapId;
    runtime.requestedSpawnId = spawnId;
    lua_pushboolean(state, 1);
    return 1;
}

int LuaSetTriggerEnabled(lua_State* state, bool enabled)
{
    size_t length = 0;
    const char* rawId = luaL_checklstring(state, 1, &length);
    std::string error;
    if (!SetSectorScriptTriggerEnabled(HostFromLua(state), std::string{rawId, length}, enabled, error)) {
        lua_pushboolean(state, 0);
        lua_pushlstring(state, error.data(), error.size());
        return 2;
    }
    lua_pushboolean(state, 1);
    return 1;
}

int LuaEnableTrigger(lua_State* state)
{
    return LuaSetTriggerEnabled(state, true);
}

int LuaDisableTrigger(lua_State* state)
{
    return LuaSetTriggerEnabled(state, false);
}

void Register(lua_State* state, const char* name, lua_CFunction function)
{
    lua_pushcfunction(state, function);
    lua_setglobal(state, name);
}

} // namespace

void InitializeSectorScriptHost(
        SectorScriptHost& host,
        SectorRuntimeObjectState& runtimeObjects,
        SectorTopologyMap& map,
        engine::ScriptRuntime& scripts)
{
    host.runtimeObjects = &runtimeObjects;
    host.map = &map;
    host.scripts = &scripts;
    host.doorMoves.clear();
    host.doorMoves.reserve(64);
    host.triggers.clear();
    host.triggers.reserve(map.triggers.size());
    std::vector<size_t> triggerIndices(map.triggers.size());
    for (size_t i = 0; i < triggerIndices.size(); ++i) triggerIndices[i] = i;
    std::sort(triggerIndices.begin(), triggerIndices.end(), [&map](size_t left, size_t right) {
        return map.triggers[left].sourceAuthoringTriggerId < map.triggers[right].sourceAuthoringTriggerId;
    });
    for (size_t index : triggerIndices) {
        host.triggers.push_back(SectorScriptTriggerState{index, map.triggers[index].enabled});
    }
}

void ResetSectorScriptHost(SectorScriptHost& host)
{
    host.runtimeObjects = nullptr;
    host.map = nullptr;
    host.scripts = nullptr;
    host.doorMoves.clear();
    host.triggers.clear();
}

void RegisterSectorScriptBindings(lua_State* state)
{
    Register(state, "moveDoor", LuaMoveDoor);
    Register(state, "startMoveDoor", LuaStartMoveDoor);
    Register(state, "changeMap", LuaChangeMap);
    Register(state, "enableTrigger", LuaEnableTrigger);
    Register(state, "disableTrigger", LuaDisableTrigger);
}

void UpdateSectorScriptOperations(
        engine::EngineContext& context,
        SectorScriptHost& host)
{
    if (host.scripts == nullptr || host.runtimeObjects == nullptr) return;
    for (SectorScriptDoorMove& move : host.doorMoves) {
        if (!move.active || !engine::IsValid(move.operation)) continue;
        if (!context.world.IsAlive(move.entity)
                || !context.world.Has<SectorDoor>(move.entity)
                || !context.world.Has<SectorDoorMotion>(move.entity)) {
            engine::ScriptSystemFailOperation(
                    *host.scripts, move.operation, "door was removed");
            move.active = false;
            continue;
        }
        SectorDoor& door = context.world.Get<SectorDoor>(move.entity);
        SectorDoorMotion& motion = context.world.Get<SectorDoorMotion>(move.entity);
        if (!door.enabled) {
            RestoreDoorSpeed(context.world, move);
            engine::ScriptSystemFailOperation(
                    *host.scripts, move.operation, "door was disabled");
            move.active = false;
            continue;
        }
        if (std::fabs(motion.targetOpenFraction - move.targetFraction)
                > DoorTargetEpsilon) {
            RestoreDoorSpeed(context.world, move);
            engine::ScriptSystemFailOperation(
                    *host.scripts, move.operation, "door move was interrupted");
            move.active = false;
            continue;
        }
        if (std::fabs(motion.openFraction - move.targetFraction)
                <= DoorTargetEpsilon) {
            motion.openFraction = move.targetFraction;
            motion.targetOpenFraction = move.targetFraction;
            RestoreDoorSpeed(context.world, move);
            engine::ScriptSystemCompleteOperation(
                    *host.scripts, move.operation);
            move.active = false;
        }
    }
    host.doorMoves.erase(
            std::remove_if(
                    host.doorMoves.begin(),
                    host.doorMoves.end(),
                    [](const SectorScriptDoorMove& move) {
                        return !move.active;
                    }),
            host.doorMoves.end());
}

bool SetSectorScriptTriggerEnabled(
        SectorScriptHost& host,
        const std::string& triggerId,
        bool enabled,
        std::string& error)
{
    if (host.map == nullptr || triggerId.empty()) {
        error = triggerId.empty() ? "trigger ID must not be empty" : "trigger runtime is unavailable";
        return false;
    }
    for (SectorScriptTriggerState& state : host.triggers) {
        if (state.triggerIndex >= host.map->triggers.size()) continue;
        if (host.map->triggers[state.triggerIndex].id != triggerId) continue;
        state.enabled = enabled;
        if (!enabled) {
            state.pending = false;
            state.remainingDelayMilliseconds = 0.0f;
        }
        error.clear();
        return true;
    }
    error = "trigger not found: " + triggerId;
    return false;
}

void UpdateSectorScriptTriggers(
        SectorScriptHost& host,
        Vector2 playerPositionXZ,
        float dtSeconds)
{
    if (host.map == nullptr || host.scripts == nullptr) return;
    const float elapsedMs = std::isfinite(dtSeconds) && dtSeconds > 0.0f
            ? dtSeconds * 1000.0f : 0.0f;
    for (SectorScriptTriggerState& state : host.triggers) {
        if (state.triggerIndex >= host.map->triggers.size()) continue;
        const SectorCompiledTrigger& trigger = host.map->triggers[state.triggerIndex];
        const bool insideNow = SectorTriggerContainsWorldPoint(
                trigger.points, playerPositionXZ.x, playerPositionXZ.y);
        bool enteredThisFrame = false;
        if (state.enabled && !state.consumed && !state.pending && !state.inside && insideNow) {
            state.pending = true;
            state.remainingDelayMilliseconds = static_cast<float>(trigger.delayMilliseconds);
            enteredThisFrame = true;
        }
        state.inside = insideNow;
        if (!state.enabled || !state.pending) continue;
        if (!enteredThisFrame) state.remainingDelayMilliseconds -= elapsedMs;
        if (state.remainingDelayMilliseconds > 0.0f) continue;

        engine::ScriptCallOutcome outcome;
        if (trigger.script.empty()) {
            outcome.result = engine::ScriptCallResult::Missing;
        } else {
            outcome = engine::ScriptSystemCallForegroundHook(*host.scripts, trigger.script);
        }
        if (outcome.result == engine::ScriptCallResult::ForegroundBusy
                || outcome.result == engine::ScriptCallResult::AlreadyRunning) {
            continue;
        }
        state.pending = false;
        state.remainingDelayMilliseconds = 0.0f;
        if (!trigger.repeat) state.consumed = true;
        if (outcome.result == engine::ScriptCallResult::Missing) {
            std::fprintf(stderr,
                    "[Lua WARNING] trigger '%s' has no callable script function '%s'\n",
                    trigger.id.c_str(), trigger.script.c_str());
        } else if (outcome.result == engine::ScriptCallResult::Error) {
            std::fprintf(stderr,
                    "[Lua ERROR] trigger '%s' function '%s' failed: %s\n",
                    trigger.id.c_str(), trigger.script.c_str(), outcome.error.c_str());
        }
    }
}

} // namespace game
