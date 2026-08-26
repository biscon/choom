#pragma once

#include "engine/ecs/Entity.h"
#include "engine/scripting/ScriptData.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <raylib.h>
#include <string>
#include <vector>

struct lua_State;

namespace engine {
struct EngineContext;
}

namespace game {

struct SectorRuntimeObjectState;
class SectorNavigationWorld;
struct Health;
struct NpcNavigationRuntime;
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

struct SectorScriptTriggerState {
    size_t triggerIndex = 0;
    bool enabled = true;
    bool inside = false;
    bool pending = false;
    bool consumed = false;
    float remainingDelayMilliseconds = 0.0f;
};

struct SectorScriptNpcMove {
    uint64_t token = 0;
    uint64_t requestId = 0;
    std::string instanceId;
    engine::ScriptOperationHandle operation{};
    bool active = false;
};

struct SectorScriptNpcMoveDiagnostics {
    uint64_t requests = 0;
    uint64_t successes = 0;
    uint64_t failures = 0;
    uint64_t cancellations = 0;
    uint64_t capacityWarnings = 0;
    std::array<char, 64> lastInstanceId{};
    std::array<char, 192> lastOutcome{};
};

struct SectorScriptDoorPermission {
    engine::ScriptTaskHandle task{};
    engine::Entity entity = engine::NullEntity();
    float targetOpenFraction = 0.0f;
    bool active = false;
};

struct SectorScriptAudioApi {
    void* userData = nullptr;
    bool (*playMapSound)(void*, engine::EngineContext&, const std::string&,
            float, float, std::string&) = nullptr;
    bool (*playSoundEmitter)(void*, engine::EngineContext&, const std::string&,
            const float*, float, std::string&) = nullptr;
    bool (*stopSoundEmitter)(void*, engine::EngineContext&, const std::string&,
            std::string&) = nullptr;
};

struct SectorScriptHost {
    SectorRuntimeObjectState* runtimeObjects = nullptr;
    SectorNavigationWorld* navigation = nullptr;
    NpcNavigationRuntime* npcNavigation = nullptr;
    Health* playerHealth = nullptr;
    SectorTopologyMap* map = nullptr;
    SectorScriptAudioApi audio;
    engine::ScriptRuntime* scripts = nullptr;
    std::vector<SectorScriptDoorMove> doorMoves;
    std::vector<SectorScriptNpcMove> npcMoves;
    std::vector<SectorScriptTriggerState> triggers;
    SectorScriptNpcMoveDiagnostics npcMoveDiagnostics;
    SectorScriptDoorPermission doorPermission;
    uint64_t nextDoorMoveToken = 1;
    uint64_t nextNpcMoveToken = 1;
    bool dynamicLightsDirty = false;
};

void InitializeSectorScriptHost(
        SectorScriptHost& host,
        SectorRuntimeObjectState& runtimeObjects,
        SectorTopologyMap& map,
        engine::ScriptRuntime& scripts,
        SectorNavigationWorld* navigation = nullptr,
        NpcNavigationRuntime* npcNavigation = nullptr,
        SectorScriptAudioApi audio = {},
        Health* playerHealth = nullptr);

void ResetSectorScriptHost(SectorScriptHost& host);

void RegisterSectorScriptBindings(lua_State* state);

void UpdateSectorScriptOperations(
        engine::EngineContext& context,
        SectorScriptHost& host);

void UpdateSectorScriptTriggers(
        SectorScriptHost& host,
        Vector2 playerPositionXZ,
        float dtSeconds);

bool SetSectorScriptTriggerEnabled(
        SectorScriptHost& host,
        const std::string& triggerId,
        bool enabled,
        std::string& error);

bool RequestSectorScriptDoorUse(
        engine::EngineContext& context,
        SectorScriptHost& host,
        engine::Entity doorEntity);

void UpdateSectorScriptDoorPermission(
        engine::EngineContext& context,
        SectorScriptHost& host);

} // namespace game
