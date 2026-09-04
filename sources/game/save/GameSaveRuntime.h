#pragma once

#include "game/save/GameSaveData.h"

#include <string_view>

namespace engine {
class AssetManager;
class World;
}

namespace game {

struct SectorRuntimeObjectState;
struct SectorScriptHost;
struct SectorTopologyMap;
class SectorSceneRuntime;

const GameSaveLevelState* FindGameSaveLevelState(
        const std::vector<GameSaveLevelState>& levels,
        std::string_view levelId);

void UpsertGameSaveLevelState(
        std::vector<GameSaveLevelState>& levels,
        GameSaveLevelState state);

GameSaveLevelState CaptureGameSaveLevelState(
        const engine::World& world,
        const engine::AssetManager& assets,
        const SectorTopologyMap& map,
        const SectorRuntimeObjectState& runtimeObjects,
        const SectorScriptHost& scriptHost,
        std::string levelId);

void ApplyGameSaveLevelMapState(
        SectorTopologyMap& map,
        const GameSaveLevelState& state);

void ApplyGameSaveLevelRuntimeState(
        engine::World& world,
        engine::AssetManager& assets,
        SectorSceneRuntime& scene,
        const SectorTopologyMap& map,
        SectorScriptHost& scriptHost,
        const GameSaveLevelState& state);

} // namespace game
