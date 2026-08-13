#pragma once

#include "engine/ecs/Entity.h"
#include "engine/scripting/ScriptData.h"

#include <cstdint>
#include <string>
#include <vector>

struct lua_State;

namespace engine {
struct EngineContext;
}

namespace game {

struct SectorRuntimeObjectState;
struct SectorTopologyMap;

struct SectorScriptDoorMove {
    uint64_t token = 0;
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
    engine::ScriptOperationHandle operation{};
    float targetFraction = 0.0f;
    float savedTravelSpeed = 0.0f;
    bool active = false;
};

struct SectorScriptHost {
    SectorRuntimeObjectState* runtimeObjects = nullptr;
    SectorTopologyMap* map = nullptr;
    engine::ScriptRuntime* scripts = nullptr;
    std::vector<SectorScriptDoorMove> doorMoves;
    uint64_t nextDoorMoveToken = 1;
};

void InitializeSectorScriptHost(
        SectorScriptHost& host,
        SectorRuntimeObjectState& runtimeObjects,
        SectorTopologyMap& map,
        engine::ScriptRuntime& scripts);

void ResetSectorScriptHost(SectorScriptHost& host);

void RegisterSectorScriptBindings(lua_State* state);

void UpdateSectorScriptOperations(
        engine::EngineContext& context,
        SectorScriptHost& host);

} // namespace game
