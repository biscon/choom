#pragma once
#include "engine/assets/AssetHandles.h"
#include "engine/ecs/Entity.h"
#include "game/npc/NpcCollision.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorPaths.h"
#include <vector>
namespace engine
{
struct EngineContext;
class World;
} // namespace engine
namespace game
{
struct SectorRuntimeObjectState;
struct SectorTopologyMap;
struct SectorPropDrag
{
    SectorPropDragSettings settings;
    float distanceWorld = 0;
    float supportY = 0;
    engine::SoundHandle startSound = engine::NullSoundHandle();
    engine::SoundHandle movingSound = engine::NullSoundHandle();
    engine::SoundHandle endSound = engine::NullSoundHandle();
};
struct SectorPropDragSession
{
    engine::Entity entity = engine::NullEntity();
    Vector2 playerOffset = {};
    int pushDirection = 1;
    float velocityWorld = 0.0f;
    Vector2 lookOffset = {};
    Vector2 initialLookOffset = {};
    float lookCenterElapsed = 0.0f;
    bool lookInitialized = false;
    bool resetMouseLook = false;
    engine::SoundPlaybackHandle loop = engine::NullSoundPlaybackHandle();
};
bool BeginSectorPropDrag(engine::EngineContext &context, const SectorTopologyMap &map,
                         SectorPropDragSession &session, engine::Entity entity,
                         const SectorFpsControllerState &player);
void EndSectorPropDrag(engine::EngineContext &context, SectorPropDragSession &session,
                       bool playEnd = false);
bool UpdateSectorPropDrag(engine::EngineContext &context, const SectorTopologyMap &map,
                          SectorRuntimeObjectState &objects, const SectorCollisionWorld &collision,
                          const std::vector<NpcCollisionCylinder> &npcs,
                          SectorPropDragSession &session, SectorFpsControllerState &player,
                          const SectorFpsControllerConfig &config, SectorFpsControllerInput &input,
                          float dt);
// Owns the ordinary mouse-look update, then constrains it only during a grab.
void UpdateSectorPropDragMouseLook(engine::World &world, SectorPropDragSession &session,
                                   SectorFpsControllerState &player,
                                   const SectorFpsControllerConfig &config,
                                   const PlayerCameraApplicationSettings &settings,
                                   const SectorFpsControllerInput &input, float dt);
void RefreshSectorDragColliders(engine::EngineContext &context, SectorRuntimeObjectState &objects);
} // namespace game
